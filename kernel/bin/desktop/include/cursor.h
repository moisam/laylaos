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
 *  the client side.
 */

#ifndef CURSOR_H
#define CURSOR_H

#include <kernel/mouse.h>       // typedef mouse_buttons_t
#include "gc.h"

#include "cursor-struct.h"

// Curosr types
#define CURSOR_NONE         0
#define CURSOR_NORMAL       1
#define CURSOR_WE           2
#define CURSOR_NS           3
#define CURSOR_NWSE         4
#define CURSOR_NESW         5
#define CURSOR_CROSS        6
#define CURSOR_CROSSHAIR    7
#define CURSOR_WAITING      8
#define CURSOR_IBEAM        9
#define CURSOR_HAND         10
#define CURSOR_X            11


#ifdef __cplusplus
extern "C" {
#endif

#ifndef GUI_SERVER

#include "client/window-struct.h"

curid_t cursor_load(int w, int h, int hotx, int hoty, uint32_t *data);
void cursor_free(curid_t curid);
void cursor_show(struct window_t *win, curid_t curid);
void cursor_hide(struct window_t *win);
void cursor_set_pos(int x, int y);
void cursor_get_info(struct cursor_info_t *curinfo);

#endif


#ifdef __cplusplus
}
#endif

#endif      /* CURSOR_H */
