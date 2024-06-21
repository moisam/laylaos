/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: mouse.h
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
 *  \file mouse.h
 *
 *  Declarations and struct definitions for working with the mouse on
 *  the client side.
 */

#ifndef CLIENT_MOUSE_H
#define CLIENT_MOUSE_H

#define WHEEL_DELTA                 120
#define DOUBLE_CLICK_THRESHOLD      500     /* millisecs */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GUI_SERVER

#include <stdint.h>
#include <kernel/mouse.h>

#include "list-struct.h"
#include "window-defs.h"
#include "mouse-state-struct.h"
#include "event.h"
#include "client/window-struct.h"

int mouse_grab(struct window_t *window, int confine);
void mouse_ungrab(void);

#endif      /* !GUI_SERVER */

#ifdef __cplusplus
}
#endif

#endif      /* CLIENT_MOUSE_H */
