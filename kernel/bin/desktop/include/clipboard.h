/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: clipboard.h
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
 *  \file clipboard.h
 *
 *  Declarations of clipboard manipulation functions.
 */

#ifndef GUI_CLIPBOARD_H
#define GUI_CLIPBOARD_H

#define CLIPBOARD_FORMAT_TEXT           0x01
#define CLIPBOARD_FORMAT_COUNT          1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GUI_SERVER

/*
 * TODO: remove server declarations to a separate file.
 */

#include "event.h"

size_t server_clipboard_set(struct event_res_t *evres);
void *server_clipboard_get(int format, size_t *datasz);
size_t server_clipboard_query_size(int format);

#else       /* !GUI_SERVER */

size_t clipboard_has_data(int format);
void *clipboard_get_data(int format);
int clipboard_set_data(int format, void *data, size_t datasz);

#endif      /* !GUI_SERVER */

#ifdef __cplusplus
}
#endif

#endif      /* GUI_CLIPBOARD_H */
