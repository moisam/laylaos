/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
 * 
 *    file: screen.h
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
 *  \file screen.h
 *
 *  Functions to work with the screen structure on the client side.
 */

#ifndef GUI_SCREEN_H
#define GUI_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#define SCREEN_COLOR_FORMAT_UNKNOWN      0
#define SCREEN_COLOR_FORMAT_RGBA         1
#define SCREEN_COLOR_FORMAT_BGRA         2
#define SCREEN_COLOR_FORMAT_ARGB         3
#define SCREEN_COLOR_FORMAT_ABGR         4
#define SCREEN_COLOR_FORMAT_RGB          5
#define SCREEN_COLOR_FORMAT_BGR          6

#include "screen-struct.h"
#include "rect-struct.h"

int get_screen_info(struct screen_t *screen);
int get_screen_palette(struct screen_t *screen);
int get_screen_color_format(struct screen_t *screen);
int get_desktop_bounds(Rect *r);

#ifdef __cplusplus
}
#endif

#endif      /* GUI_SCREEN_H */
