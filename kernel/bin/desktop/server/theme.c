/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: theme.c
 *    This file is part of LaylaOS.
 *
 *    LaylaOS is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    LaylaOS is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LaylaOS.  If not, see <http://www.gnu.org/licenses/>.
 */    

/**
 *  \file theme.c
 *
 *  Functions to work with color themes on the server side.
 */

#define GUI_SERVER
#include "../include/gui.h"
#include "../include/theme.h"
#include "../include/server/server.h"
#include "../include/server/event.h"

#include "inlines.c"

#define GLOB                            __global_gui_data

void server_init_theme(void)
{
    int i;

    // set the builtin color theme, then try to read the theme colors file
    for(i = 0; i < THEME_COLOR_LAST; i++)
    {
        GLOB.themecolor[i] = builtin_color_theme[i];
    }
}


void send_theme_data(winid_t dest, uint32_t seqid, int fd)
{
    size_t datasz = THEME_COLOR_LAST * sizeof(uint32_t);
    size_t bufsz = sizeof(struct event_res_t) + datasz;
    char cbuf[bufsz];
    struct event_res_t *evbuf = (struct event_res_t *)cbuf;

    A_memset((void *)evbuf, 0, bufsz);
    A_memcpy((void *)evbuf->data, GLOB.themecolor, datasz);

    evbuf->type = EVENT_COLOR_THEME_DATA;
    evbuf->seqid = seqid;
    evbuf->datasz = datasz;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = dest;
    evbuf->valid_reply = 1;
    evbuf->palette.color_count = THEME_COLOR_LAST;
    direct_write(fd, (void *)evbuf, bufsz);
}

void broadcast_new_theme(void)
{
    struct server_window_t *window;
    ListNode *current_node;

    if(!root_window)
    {
        return;
    }

    // No windows displayed. Just inform root
    if(!root_window->children)
    {
        if(root_window->clientfd)
        {
            send_theme_data(root_window->winid, 0, root_window->clientfd->fd);
        }

        return;
    }

    // Loop through the windows and inform everybody
    for(current_node = root_window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        window = (struct server_window_t *)current_node->payload;

        if(window->clientfd)
        {
            send_theme_data(window->winid, 0, window->clientfd->fd);
        }
    }

    if(root_window->clientfd)
    {
        send_theme_data(root_window->winid, 0, root_window->clientfd->fd);
    }

    // The windows should each draw their content, but the window decorations
    // are handled by the server. This is why we need to do an update.
    // Do a dirty update for the desktop, which will, in turn, do a 
    // dirty update for all affected child windows
    reinit_window_controlbox();
    server_window_paint(gc, root_window, NULL, 
                                FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);
    draw_mouse_cursor(0);
    invalidate_screen_rect(desktop_bounds.top, desktop_bounds.left, 
                                   desktop_bounds.bottom, desktop_bounds.right);
}

