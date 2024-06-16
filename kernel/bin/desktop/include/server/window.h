/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: window.h
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
 *  \file window.h
 *
 *  Declarations and struct definitions for server windows.
 *  These are the the server representation of windows, which are eventually
 *  drawn on the screen. Client programs has their own implementation of
 *  windows, defined in the header file include/client/window.h.
 *
 *  The functions declared in this file are NOT intended for client
 *  application use.
 */

#ifndef WINDOW_SERVER_H
#define WINDOW_SERVER_H

#include <sys/types.h>
#include <kernel/mouse.h>
#include "../window-defs.h"
#include "window-struct.h"
#include "../theme.h"
#include "../gc.h"
#include "../event.h"
#include "../menu.h"
#include "../mouse-state-struct.h"

#include <sys/ioctl.h>
#include <gui/fb.h>
#include "../gui-global.h"

// Flags to server_window_paint()
#define FLAG_PAINT_CHILDREN         0x01
#define FLAG_PAINT_BORDER           0x02

// forward declaration to avoid cyclic dependencies
struct gc_t;

/**********************************************
 * Public functions
 **********************************************/

// main.c
void server_window_dead(struct server_window_t *window);
void cancel_active_child(struct server_window_t *parent, 
                         struct server_window_t *win);
uint8_t *create_canvas(uint32_t canvas_size, int *__shmid);
struct server_window_t *server_window_by_winid(winid_t winid);
void draw_mouse_cursor(int invalidate);
void server_window_may_hide(struct server_window_t *win);

// server-window.c
void server_window_draw_border(struct gc_t *gc, 
                                struct server_window_t *window);

void server_window_apply_bound_clipping(struct server_window_t *window,
                                        int in_recursion, 
                                        RectList *dirty_regions, 
                                        struct clipping_t *clipping);

void server_window_update_title(struct gc_t *gc, 
                                struct server_window_t *window);
void server_window_invalidate(struct gc_t *gc, struct server_window_t *window,
                                int top, int left, int bottom, int right);
void server_window_paint(struct gc_t *gc, struct server_window_t *window,
                                RectList *dirty_regions, int flags);

void server_window_get_windows_above(struct server_window_t *parent, 
                                     struct server_window_t *child, 
                                     List *clip_windows);
void server_window_get_windows_below(struct server_window_t *parent, 
                                     struct server_window_t *child, 
                                     List *clip_windows);

void server_window_raise(struct gc_t *gc, struct server_window_t *window, 
                            uint8_t do_draw);
void server_window_move(struct gc_t *gc, struct server_window_t *window, 
                            int new_x, int new_y);
void server_window_resize_finalize(struct gc_t *gc, 
                                    struct server_window_t *window);
void server_window_resize_accept(struct gc_t *gc, 
                                    struct server_window_t *window,
                                    int new_x, int new_y,
                                    int new_w, int new_h, uint32_t seqid);
void server_window_resize(struct gc_t *gc, struct server_window_t *window,
                                    int dx, int dy, int dw, int dh, 
                                    uint32_t seqid);
void server_window_resize_hidden(struct gc_t *gc, 
                                    struct server_window_t *window,
                                    int dx, int dy, int dw, int dh);
void server_window_resize_absolute(struct gc_t *gc,
                                    struct server_window_t *window,
                                    int x, int y, int w, int h, 
                                    uint32_t seqid);

void server_window_hide(struct gc_t *gc, struct server_window_t *window);
void server_window_insert_child(struct server_window_t *window, 
                                struct server_window_t *child);
void server_window_remove_child(struct server_window_t *window, 
                                struct server_window_t *child);

void server_window_set_title(struct gc_t *gc, struct server_window_t *window,
                                char *new_title, size_t new_len);
void server_window_create_canvas(struct gc_t *gc, 
                                struct server_window_t *window);


// server-window-controlbox.c
void prep_window_controlbox(void);
void server_window_draw_controlbox(struct gc_t *gc, 
                                    struct server_window_t *window,
                                    int wscreen_x, int wscreen_y, int flags);

void server_window_toggle_minimize(struct gc_t *gc, 
                                    struct server_window_t *window);
void server_window_toggle_maximize(struct gc_t *gc, 
                                    struct server_window_t *window);
void server_window_toggle_fullscreen(struct gc_t *gc, 
                                    struct server_window_t *window);

void server_window_close(struct gc_t *gc, struct server_window_t *window);

// server-window-mouse.c
void server_window_process_mouse(struct gc_t *gc, 
                                    struct server_window_t *window,
                                    struct mouse_state_t *mstate);
void server_window_mouseover(struct gc_t *gc, struct server_window_t *window,
                                    struct mouse_state_t *mstate);

#endif      /* WINDOW_SERVER_H */
