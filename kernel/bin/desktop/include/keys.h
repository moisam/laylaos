/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: keys.h
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
 *  \file keys.h
 *
 *  Declarations and macro definitions for working with keyboard keys
 *  on both client and server sides.
 */

#ifndef KEYS_H
#define KEYS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <kernel/keycodes.h>

// Modifier key masks
#define MODIFIER_MASK_CTRL              0x01
#define MODIFIER_MASK_SHIFT             0x02
#define MODIFIER_MASK_ALT               0x04
#define MODIFIER_MASK_CAPS              0x08
#define MODIFIER_MASK_NUM               0x10
#define MODIFIER_MASK_SCROLL            0x20

// Types of actions for key bindings. This tells the server what to do when
// a key with given modifiers is pressed
#define KEYBINDING_NOTHING              0x00
#define KEYBINDING_NOTIFY		        0x01
#define KEYBINDING_NOTIFY_ONCE	        0x02
#define KEYBINDING_FUNCTION		        0x03

// Key bindings can instruct the server to perform certain actions, e.g.
// reboot the machine. As the functions are defined on the server, the
// client can only give an index to tell the server what function to run
#define KEYBINDING_FUNCTION_REBOOT		0x01

static inline int is_printable_char(int key)
{
    if(key >= KEYCODE_A && key <= KEYCODE_ENTER)
    {
        return 1;
    }

    if(key >= KEYCODE_LBRACKET && key <= KEYCODE_SLASH)
    {
        return 1;
    }

    if(key >= KEYCODE_KP_DIV && key <= KEYCODE_KP_DOT)
    {
        return 1;
    }
    
    return 0;
}


#ifdef GUI_SERVER

/*
 * TODO: remove server declarations to a separate file.
 */

#include "../include/gc.h"

extern char modifiers;

/* int */ void server_process_key(struct gc_t *gc, char *key);
void server_key_bind(char key, char modifiers, int action, winid_t winid);
void server_key_unbind(char key, char modifiers, winid_t winid);
void key_state_bitmap(char *bitmap);

#else       /* !GUI_SERVER */

// Key names formatted as strings
extern char *long_key_names[];

void key_bind(char key, char modifiers, int action);
void key_unbind(char key, char modifiers);
int get_printable_char(char code, char modifiers);
char *get_long_key_name(int key);

#endif      /* GUI_SERVER */


#ifdef __cplusplus
}
#endif

#endif      /* KEYS_H */
