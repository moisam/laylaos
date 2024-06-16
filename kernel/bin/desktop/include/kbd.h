/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: kbd.h
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
 *  \file kbd.h
 *
 *  Declarations and struct definitions for working with the keyboard on
 *  the client side.
 */

#ifndef CLIENT_KEYBOARD_H
#define CLIENT_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GUI_SERVER

#include <stdint.h>
#include <kernel/mouse.h>

#include "list-struct.h"
#include "window-defs.h"
#include "client/window-struct.h"

int keyboard_grab(struct window_t *window);
void keyboard_ungrab(void);
winid_t get_input_focus(void);
char get_modifier_keys(void);
int get_keys_state(char *bitmap);

#endif      /* !GUI_SERVER */

#ifdef __cplusplus
}
#endif

#endif      /* CLIENT_KEYBOARD_H */
