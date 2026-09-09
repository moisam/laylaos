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
 *  the client side.
 */

#ifndef CURSOR_H
#define CURSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gc.h"

#include "cursor-struct.h"
#include "cursor-struct-alloc.h"

// Cursor types
#define CURSOR_NONE             0
#define CURSOR_NORMAL           1
#define CURSOR_WE               2
#define CURSOR_NS               3
#define CURSOR_NWSE             4
#define CURSOR_NESW             5
#define CURSOR_FLEUR            6
#define CURSOR_CROSSHAIR        7
#define CURSOR_WAITING          8
#define CURSOR_IBEAM            9
#define CURSOR_OPEN_HAND        10
#define CURSOR_X                11

#define CURSOR_E                12
#define CURSOR_N                13
#define CURSOR_NE               14
#define CURSOR_NW               15
#define CURSOR_S                16
#define CURSOR_SE               17
#define CURSOR_SW               18
#define CURSOR_W                19
#define CURSOR_UP               20
#define CURSOR_CLOSED_HAND      21
#define CURSOR_POINTING_HAND    22
#define CURSOR_HELP             23
#define CURSOR_FORBIDDEN        24
#define CURSOR_ARROW_WAITING    25
#define CURSOR_VIBEAM           26
#define CURSOR_COLRESIZE        27
#define CURSOR_ROWRESIZE        28
#define CURSOR_CELL             29
#define CURSOR_CONTEXT_MENU     30
#define CURSOR_ZOOMIN           31
#define CURSOR_ZOOMOUT          32
#define CURSOR_DND_LINK         33
#define CURSOR_DND_COPY         34
#define CURSOR_DND_MOVE         35
#define CURSOR_DND_NODROP       CURSOR_X


struct cursor_t *x11_cursor_load_sz(char *file_name, int reqsz);
struct cursor_t *x11_cursor_load(char *file_name);


#ifndef GUI_SERVER

#include "mouse-state-struct.h"
#include "window-defs.h"
#include "client/window-struct.h"

curid_t cursor_load(int w, int h, int hotx, int hoty, uint32_t *data);
void cursor_change_syscursor(curid_t curid, int pixelsz, const char *path);
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
