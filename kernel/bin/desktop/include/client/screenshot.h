/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: screenshot.h
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
 *  \file screenshot.h
 *
 *  Functions to work with screenshots on the client side.
 */

#ifndef GUI_SCREENSHOT_H
#define GUI_SCREENSHOT_H

#ifdef __cplusplus
extern "C" {
#endif

uint32_t *screenshot_get(winid_t winid, int x, int y, int w, int h);
void screenshot_free(uint32_t *screenshot);

#ifdef __cplusplus
}
#endif

#endif      /* GUI_SCREENSHOT_H */
