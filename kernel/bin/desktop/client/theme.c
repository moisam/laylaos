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
 *  Functions to get and change the system color theme.
 */

#include <errno.h>
#include "../include/gui.h"
#include "../include/screen.h"
#include "../include/directrw.h"

#define GLOB        __global_gui_data


/*
 * Set the color theme from an event packet. Parameter _evbuf MUST BE a pointer
 * to a struct event_res_t.
 */
void set_color_theme(void *_evbuf)
{
    int j;
    struct event_res_t *evbuf = _evbuf;

    j = (THEME_COLOR_LAST > evbuf->palette.color_count) ?
                    evbuf->palette.color_count : THEME_COLOR_LAST;
    j *= sizeof(uint32_t);

    A_memcpy(GLOB.themecolor, evbuf->data, j);

    // let the widgets initialize their global bitmaps
    combobox_theme_changed_global();
    scrollbar_theme_changed_global();
    spinner_theme_changed_global();
}


/*
 * Get the system color theme from the server and stores it in the global
 * theme color array.
 */
int get_color_theme(void)
{
    struct event_t ev, *ev2;
    uint32_t seqid = __next_seqid();

    ev.type = REQUEST_COLOR_THEME_GET;
    ev.seqid = seqid;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;

    direct_write(GLOB.serverfd, &ev, sizeof(struct event_t));

    if(!(ev2 = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return 0;
    }

    set_color_theme((struct event_res_t *)ev2);
    free(ev2);

    return 1;
}


/*
 * Sends the current color theme (in the global theme color array) to the 
 * server so it can be broadcast to all apps.
 */
void send_color_theme_to_server(void)
{
    uint32_t seqid = __next_seqid();
    size_t datasz = THEME_COLOR_LAST * sizeof(uint32_t);
    size_t bufsz = sizeof(struct event_res_t) + datasz;
    char cbuf[bufsz];
    struct event_res_t *evbuf = (struct event_res_t *)cbuf;

    A_memset((void *)evbuf, 0, bufsz);
    A_memcpy((void *)evbuf->data, GLOB.themecolor, datasz);

    evbuf->type = REQUEST_COLOR_THEME_SET;
    evbuf->seqid = seqid;
    evbuf->datasz = datasz;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = GLOB.server_winid;
    evbuf->palette.color_count = THEME_COLOR_LAST;

    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);
}

