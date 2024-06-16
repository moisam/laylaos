/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: keys.c
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
 *  \file keys.c
 *
 *  Functions to work with keys and their bindings on the client side.
 */

//#include <kernel/keycodes.h>
#include <kernel/kbdus.h>
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/keys.h"
#include "../include/directrw.h"

#define GLOB        __global_gui_data


void key_bind(char key, char modifiers, int action)
{
    struct event_t ev;

    ev.type = REQUEST_BIND_KEY;
    ev.seqid = __next_seqid();
    ev.keybind.key = key;
    ev.keybind.modifiers = modifiers;
    ev.keybind.action = action;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
    //write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


void key_unbind(char key, char modifiers)
{
    struct event_t ev;

    ev.type = REQUEST_UNBIND_KEY;
    ev.seqid = __next_seqid();
    ev.keybind.key = key;
    ev.keybind.modifiers = modifiers;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
    //write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


int get_printable_char(char code, char modifiers)
{
    int key;
    
    if(!is_printable_char(code))
    {
        return 0;
    }

    key = (int)code & 0xff;

    if(modifiers & MODIFIER_MASK_CTRL)
    {
        return ctrl_char(key);
    }
    
    if(modifiers & MODIFIER_MASK_CAPS)
    {
        if(is_caps_char(key))
        {
            return (modifiers & MODIFIER_MASK_SHIFT) ?
                            keycodes[key] : shift_keycodes[key];
        }
    }

    if(modifiers & MODIFIER_MASK_SHIFT)
    {
        return shift_keycodes[key];
    }

    return keycodes[key];
}

