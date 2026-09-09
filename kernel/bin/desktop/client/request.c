/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
 * 
 *    file: request.c
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
 *  \file request.c
 *
 *  Miscellaneous client requests not fitting anywhere else.
 */

#include "../include/gui.h"
#include "../include/event.h"
#include "../include/directrw.h"
#include "inlines.c"

#define GLOB        __global_gui_data

uint32_t __seqid = 0;


void set_desktop_bounds(int top, int left, int bottom, int right)
{
    struct event_t ev;

    //A_memset((void *)&ev, 0, sizeof(struct event_t));
    ev.type = REQUEST_SET_DESKTOP_BOUNDS;
    ev.seqid = __next_seqid();
    ev.rect.top = top;
    ev.rect.left = left;
    ev.rect.bottom = bottom;
    ev.rect.right = right;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    //write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}

