/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: mouse.c
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
 *  \file mouse.c
 *
 *  Functions to work with the mouse.
 */

#include "../include/event.h"
#include "../include/mouse.h"
#include "inlines.c"

#define GLOB                        __global_gui_data


// Returns: 1 on successful mouse grab, 0 on error
int mouse_grab(struct window_t *window, int confine)
{
    struct event_t *ev;
    uint32_t seqid;
    uint32_t type;
    
    if(!window)
    {
        return 0;
    }

    seqid = simple_request(confine ? REQUEST_GRAB_AND_CONFINE_MOUSE :
                                     REQUEST_GRAB_MOUSE,
                           GLOB.server_winid, window->winid);

    if(!(ev = get_server_reply(seqid)))
    {
        return 0;
    }

    type = ev->type;
    free(ev);

    return !(type == EVENT_ERROR);
}


void mouse_ungrab(void)
{
    simple_request(REQUEST_UNGRAB_MOUSE, 
                        GLOB.server_winid, TO_WINID(GLOB.mypid, 0));
}

