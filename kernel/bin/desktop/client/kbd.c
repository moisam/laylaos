/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: kbd.c
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
 *  \file kbd.c
 *
 *  Functions to query keyboard state and grab/ungrab keyboard input.
 */

#include "../include/event.h"
#include "../include/kbd.h"
#include "inlines.c"

#define GLOB                        __global_gui_data


// Returns: 1 on successful mouse grab, 0 on error
int keyboard_grab(struct window_t *window)
{
    struct event_t *ev;
    uint32_t seqid;
    uint32_t type;

    seqid = simple_request(REQUEST_GRAB_KEYBOARD, 
                                GLOB.server_winid, window->winid);

    if(!(ev = get_server_reply(seqid)))
    {
        return 0;
    }

    type = ev->type;
    free(ev);

    return !(type == EVENT_ERROR);
}


void keyboard_ungrab(void)
{
    simple_request(REQUEST_UNGRAB_KEYBOARD, 
                        GLOB.server_winid, TO_WINID(GLOB.mypid, 0));
}


winid_t get_input_focus(void)
{
    struct event_t *ev;
    uint32_t seqid;
    winid_t winid;

    seqid = simple_request(REQUEST_GET_INPUT_FOCUS, 
                                GLOB.server_winid, TO_WINID(GLOB.mypid, 0));

    if(!(ev = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev->type == EVENT_ERROR)
    {
        free(ev);
        return 0;
    }
    
    winid = ev->winattr.winid;
    free(ev);

    return winid;
}


char get_modifier_keys(void)
{
    struct event_t *ev;
    uint32_t seqid;
    char mod;

    seqid = simple_request(REQUEST_GET_MODIFIER_KEYS, 
                                GLOB.server_winid, TO_WINID(GLOB.mypid, 0));

    if(!(ev = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev->type == EVENT_ERROR)
    {
        free(ev);
        return 0;
    }
    
    mod = ev->key.modifiers;
    free(ev);

    return mod;
}


int get_keys_state(char *bitmap)
{
    struct event_t *ev;
    uint32_t seqid;

    if(!bitmap)
    {
        return 0;
    }

    seqid = simple_request(REQUEST_GET_KEYS_STATE, 
                                GLOB.server_winid, TO_WINID(GLOB.mypid, 0));

    if(!(ev = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev->type == EVENT_ERROR)
    {
        free(ev);
        return 0;
    }
    
    A_memcpy(bitmap, &ev->keybmp.bits, 32);
    free(ev);
    
    return 1;
}

