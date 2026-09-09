/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: dragndrop.h
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
 *  \file dragndrop.h
 *
 *  Declarations and struct definitions for handling drag and drop operations.
 */

#ifndef DRAGNDROP_H
#define DRAGNDROP_H

#include <kernel/mouse.h>       // typedef mouse_buttons_t

// Window ID typedef
#ifndef __WINID_TYPE_DEFINED__
#define __WINID_TYPE_DEFINED__
typedef uint64_t winid_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DRAG_RESPONSE_REJECT            0x00
#define DRAG_RESPONSE_ACCEPT_COPY       0x01
#define DRAG_RESPONSE_ACCEPT_MOVE       0x02

void drag_start(char **mimetypes, int count);
void drag_move(int mousex, int mousey, mouse_buttons_t b, char keymods);
void drag_drop(int mousex, int mousey, mouse_buttons_t b, char keymods, const char *pathname);
void drag_cancel(void);
void drag_response(winid_t winid, int response);
void drag_request_payload(winid_t dest, winid_t src, char *mimetype);

#ifdef __cplusplus
}
#endif

#endif      /* DRAGNDROP_H */
