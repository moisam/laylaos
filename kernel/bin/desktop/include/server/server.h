/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: server.h
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
 *  \file server.h
 *
 *  Header file for the server application.
 *
 *  The functions declared in this file are NOT intended for client
 *  application use.
 */

#ifndef GUI_SERVER_H
#define GUI_SERVER_H

// main.c
extern struct server_window_t *grabbed_mouse_window;
extern struct server_window_t *grabbed_keyboard_window;
extern struct server_window_t *root_window;

extern int mouse_is_confined;
extern Rect mouse_bounds;
extern struct gc_t *gc;

void may_change_mouse_cursor(struct server_window_t *win);

// server-login.c
void server_login(char *myname);

#endif      /* GUI_SERVER_H */
