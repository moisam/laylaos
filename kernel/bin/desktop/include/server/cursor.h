/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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

#include <sys/time.h>
#include "../cursor.h"

#define CURSOR_COUNT        4096
#define SYS_CURSOR_COUNT    35

#define MOUSE_WIDTH         16
#define MOUSE_HEIGHT        24
#define MOUSE_BUFSZ         (MOUSE_WIDTH * MOUSE_HEIGHT)


extern struct cursor_t *cursor[/* CURSOR_COUNT */];
extern volatile curid_t old_cursor;
extern volatile curid_t cur_cursor;
extern volatile int cur_timer_set;


static inline void change_cursor(curid_t new_cursor)
{
    old_cursor = cur_cursor;
    cur_cursor = new_cursor;

    if(cursor[cur_cursor])
    {
        cursor[cur_cursor]->curframe = 0;

        if(cursor[cur_cursor]->count > 1)
        {
            struct itimerval timer;
            timer.it_interval.tv_sec = 0;
            timer.it_interval.tv_usec = cursor[cur_cursor]->bitmaps[0].delay * 1000;
            timer.it_value.tv_sec = 0;
            timer.it_value.tv_usec = cursor[cur_cursor]->bitmaps[0].delay * 1000;

            setitimer(ITIMER_REAL, &timer, NULL);
            cur_timer_set = 1;
        }
        else if(cur_timer_set)
        {
            struct itimerval timer;
            timer.it_interval.tv_sec = 0;
            timer.it_interval.tv_usec = 0;
            timer.it_value.tv_sec = 0;
            timer.it_value.tv_usec = 0;

            setitimer(ITIMER_REAL, &timer, NULL);
            cur_timer_set = 0;
        }
    }
}


// defined in cursor.c
void prep_mouse_cursor(struct gc_t *gc);
curid_t server_cursor_load(struct gc_t *gc, int w, int h, 
                            int hotx, int hoty, uint32_t *data);
void server_cursor_free(curid_t curid);

// defined in cursor-x11.c
int prep_mouse_cursor_x11(int pixelsz);
void server_cursor_change_syscursor(struct event_res_t *evres);

#endif      /* CURSOR_H */
