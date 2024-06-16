/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: inlines.c
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
 *  \file inlines.c
 *
 *  Common inlined functions used by client-side windows and widgets.
 */

#include <errno.h>
#include <unistd.h>
#include "../include/gui-global.h"
#include "../include/directrw.h"
#include "../include/gc.h"
#include "../include/theme.h"


static inline int send_menu_event(winid_t dest, winid_t src,
                                  uint16_t menu_id, uint16_t entry_id)
{
    struct event_t ev;

    ev.type = EVENT_MENU_SELECTED;
    ev.seqid = __next_seqid();
    ev.src = src;
    ev.dest = dest;
    ev.valid_reply = 1;
    ev.menu.entry_id = entry_id;
    ev.menu.menu_id = menu_id;

    if(direct_write(__global_gui_data.serverfd, (void *)&ev, 
                                        sizeof(struct event_t)) < 0)
    {
        int err = errno;

        if(err == ENOTCONN || err == ECONNREFUSED || err == EINVAL)
        {
            return 0;
        }
    }
    
    return 1;
}


static inline uint32_t simple_request(int event, winid_t dest, winid_t src)
{
    struct event_t ev;
    uint32_t seqid = __next_seqid();

    ev.seqid = seqid;
    ev.type = event;
    ev.src = src;
    ev.dest = dest;
    ev.valid_reply = 1;
    direct_write(__global_gui_data.serverfd, (void *)&ev, 
                                        sizeof(struct event_t));

    return seqid;
}


// get the absolute on-screen x-coordinate of this window
static inline int window_screen_x(struct window_t *window)
{
    int x = 0;

    while(window)
    {
        x += window->x;
        window = window->parent;
    }
    
    return x;
}


// get the absolute on-screen y-coordinate of this window
static inline int window_screen_y(struct window_t *window)
{
    int y = 0;

    while(window)
    {
        y += window->y;
        window = window->parent;
    }
    
    return y;
}


/*
 * Draw inverted 3d border for textboxes, inputboxes, listviews, etc.
 */
static inline void draw_inverted_3d_border(struct gc_t *gc, 
                                           int x, int y, int w, int h)
{
    // top
    gc_horizontal_line(gc, x, y, w, GLOBAL_DARK_SIDE_COLOR);
    gc_horizontal_line(gc, x, y + 1, w, GLOBAL_DARK_SIDE_COLOR);
    // left
    gc_vertical_line(gc, x, y, h, GLOBAL_DARK_SIDE_COLOR);
    gc_vertical_line(gc, x + 1, y,  h, GLOBAL_DARK_SIDE_COLOR);
    // bottom
    gc_horizontal_line(gc, x, y + h - 1, w, GLOBAL_LIGHT_SIDE_COLOR);
    gc_horizontal_line(gc, x + 1, y + h - 2, w - 1, WINDOW_BGCOLOR);
    // right
    gc_vertical_line(gc, x + w - 1, y, h, GLOBAL_LIGHT_SIDE_COLOR);
    gc_vertical_line(gc, x + w - 2, y + 1, h - 2, WINDOW_BGCOLOR);
}


/*
 * Draw 3d border for buttons and image buttons.
 */
static inline void draw_3d_border(struct gc_t *gc, 
                                  int x, int y, int w, int h,
                                  int with_black_border)
{
    // with extra black border (for buttons under focus)
    if(with_black_border)
    {
        // black border
        gc_draw_rect(gc, x, y, w, h, GLOBAL_BLACK_COLOR);

        // top
        gc_horizontal_line(gc, x + 1, y + 1, w - 2, GLOBAL_LIGHT_SIDE_COLOR);
        gc_horizontal_line(gc, x + 1, y + 2, w - 2, GLOBAL_LIGHT_SIDE_COLOR);
        // left
        gc_vertical_line(gc, x + 1, y + 1, h - 2, GLOBAL_LIGHT_SIDE_COLOR);
        gc_vertical_line(gc, x + 2, y + 1,  h - 2, GLOBAL_LIGHT_SIDE_COLOR);
        // bottom
        gc_horizontal_line(gc, x + 1, y + h - 2, w - 2, GLOBAL_DARK_SIDE_COLOR);
        gc_horizontal_line(gc, x + 2, y + h - 3, w - 3, GLOBAL_DARK_SIDE_COLOR);
        // right
        gc_vertical_line(gc, x + w - 2, y + 1, h - 2, GLOBAL_DARK_SIDE_COLOR);
        gc_vertical_line(gc, x + w - 3, y + 2, h - 3, GLOBAL_DARK_SIDE_COLOR);
    }
    else
    {
        // top
        gc_horizontal_line(gc, x, y, w, GLOBAL_LIGHT_SIDE_COLOR);
        gc_horizontal_line(gc, x, y + 1, w, GLOBAL_LIGHT_SIDE_COLOR);
        // left
        gc_vertical_line(gc, x, y, h, GLOBAL_LIGHT_SIDE_COLOR);
        gc_vertical_line(gc, x + 1, y,  h, GLOBAL_LIGHT_SIDE_COLOR);
        // bottom
        gc_horizontal_line(gc, x, y + h - 1, w, GLOBAL_DARK_SIDE_COLOR);
        gc_horizontal_line(gc, x + 1, y + h - 2, w - 1, GLOBAL_DARK_SIDE_COLOR);
        // right
        gc_vertical_line(gc, x + w - 1, y, h, GLOBAL_DARK_SIDE_COLOR);
        gc_vertical_line(gc, x + w - 2, y + 1, h - 1, GLOBAL_DARK_SIDE_COLOR);
    }
}

