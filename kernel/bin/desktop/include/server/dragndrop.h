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
 *  Declarations and struct definitions for handling drag and drop operations
 *  the server side.
 *
 *  The functions declared in this file are NOT intended for client
 *  application use.
 */

#ifndef SERVER_DRAGNDROP_H
#define SERVER_DRAGNDROP_H

#ifndef GUI_SERVER
#error dragndrop.h should not be included in client applications
#endif

extern struct server_window_t *dnd_current_target;
extern struct server_window_t *dnd_current_source;

void server_drag_start(struct event_t *ev);
void server_drag_move(struct event_t *ev);
void server_drag_cancel(void);
void server_drag_drop(struct event_t *ev);
void server_drag_response(struct event_t *ev);

#endif      /* SERVER_DRAGNDROP_H */
