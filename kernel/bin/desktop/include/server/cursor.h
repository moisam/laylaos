/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: cursor.h
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
 *  \file cursor.h
 *
 *  Declarations and struct definitions for working with the cursor on
 *  the server side.
 *
 *  The functions declared in this file are NOT intended for client
 *  application use.
 */

#ifndef SERVER_CURSOR_H
#define SERVER_CURSOR_H

#ifndef GUI_SERVER
#error cursor.h should not be included in client applications
#endif

#include "../cursor.h"

#define CURSOR_COUNT        64
#define SYS_CURSOR_COUNT    11

//Information for drawing a pretty mouse
#define MOUSE_WIDTH         16
#define MOUSE_HEIGHT        24
#define MOUSE_BUFSZ         (MOUSE_WIDTH * MOUSE_HEIGHT)

struct cursor_t
{
    uint32_t *data;
    int w, h;
    int hotx, hoty;

#define CURSOR_FLAG_MALLOCED    0x01
    uint32_t flags;
};

extern struct cursor_t cursor[/* CURSOR_COUNT */];
extern uint32_t transparent_color;
extern volatile curid_t old_cursor;
extern volatile curid_t cur_cursor;


static inline void change_cursor(curid_t new_cursor)
{
    old_cursor = cur_cursor;
    cur_cursor = new_cursor;
}


void prep_mouse_cursor(struct gc_t *gc);
curid_t server_cursor_load(struct gc_t *gc, int w, int h, 
                            int hotx, int hoty, uint32_t *data);
void server_cursor_free(curid_t curid);

#endif      /* CURSOR_H */
