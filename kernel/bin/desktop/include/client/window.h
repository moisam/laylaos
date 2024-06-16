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
 *  Declarations and struct definitions for client windows.
 *  These are the windows all programs (except the server) deal with.
 *  The server has its own implementation of windows, defined in the header
 *  file include/server/window.h.
 */

#ifndef CLIENT_WINDOW_H
#define CLIENT_WINDOW_H

#include <sys/types.h>
#include <unistd.h>
#include <kernel/mouse.h>
#include "../theme.h"
#include "../gc.h"
#include "../window-defs.h"
#include "../event.h"
#include "../directrw.h"

#include "../mouse-state-struct.h"
#include "window-attrib-struct.h"
#include "window-struct.h"

#include "../event.h"
#include "../gui-global.h"


#define IS_ACTIVE_CHILD(widget)   ((widget) == (widget)->parent->active_child)


#ifdef __cplusplus
extern "C" {
#endif

/**********************************************
 * Inlined helper functions
 **********************************************/

static inline int to_child_x(struct window_t *window, int x)
{
    return window->x + x;
}

static inline int to_child_y(struct window_t *window, int y)
{
    return window->y + y;
}

static inline void window_invalidate_rect(struct window_t *window,
                                          int top, int left,
                                          int bottom, int right)
{
    volatile struct event_t ev;

    ev.type = REQUEST_WINDOW_INVALIDATE;
    ev.rect.left = left;
    ev.rect.top = top;
    ev.rect.right = right;
    ev.rect.bottom = bottom;
    ev.src = window->winid;
    ev.dest = __global_gui_data.server_winid;
    //write(__global_gui_data.serverfd, (void *)&ev, sizeof(struct event_t));
    direct_write(__global_gui_data.serverfd, (void *)&ev, sizeof(struct event_t));
}

static inline void child_invalidate(struct window_t *child)
{
    struct window_t *parent = child->parent;

    while(parent->parent)
    {
        parent = parent->parent;
    }

    window_invalidate_rect(parent, child->y, child->x,
                                   child->y + child->h - 1,
                                   child->x + child->w - 1);
}

static inline void window_invalidate(struct window_t *window)
{
    window_invalidate_rect(window, 0, 0, window->h - 1, window->w - 1);
}


/**********************************************
 * Public functions
 **********************************************/

// client-window.c
struct window_t *__window_create(struct window_attribs_t *attribs, 
                                 int8_t type, winid_t owner);
struct window_t *window_create(struct window_attribs_t *attribs);
void window_destroy(struct window_t *window);
void window_destroy_children(struct window_t *window);
void window_destroy_all(void);
struct window_t *win_for_winid(winid_t winid);
void window_show(struct window_t *window);
void window_hide(struct window_t *window);
void window_raise(struct window_t *window);
void window_maximize(struct window_t *window);
void window_minimize(struct window_t *window);
void window_restore(struct window_t *window);
void window_set_title(struct window_t *window, char *new_title);
void __window_set_title(struct window_t *window, char *new_title, 
                        int notify_parent);
void window_set_icon(struct window_t *window, char *name);
void window_load_icon(struct window_t *window, unsigned int w, unsigned int h,
                      uint32_t *data);
void window_repaint(struct window_t *window);
void window_insert_child(struct window_t *window, struct window_t *child);
void window_set_min_size(struct window_t *window, uint16_t w, uint16_t h);
void window_resize(struct window_t *window, int16_t x, int16_t y,
                                            uint16_t w, uint16_t h);
void window_set_pos(struct window_t *window, int x, int y);
void window_set_size(struct window_t *window, int x, int y, 
                     uint16_t w, uint16_t h);
void window_set_bordered(struct window_t *window, int bordered);
void window_set_resizable(struct window_t *window, int resizable);
void window_set_ontop(struct window_t *window, int ontop);
void window_set_focus_child(struct window_t *window, struct window_t *child);
void window_enter_fullscreen(struct window_t *window);
void window_exit_fullscreen(struct window_t *window);
int window_grab_keyboard(struct window_t *window);
int window_ungrab_keyboard(struct window_t *window);
void window_destroy_canvas(struct window_t *window);
int window_new_canvas(struct window_t *window);
void window_resize_layout(struct window_t *window);

// client-window-mouse.c
void window_mouseover(struct window_t *window, int x, int y, 
                      mouse_buttons_t buttons);
void window_mouseexit(struct window_t *window, mouse_buttons_t buttons);

// widget.c
void widget_destroy(struct window_t *widget);
int widget_append_text(struct window_t *widget, char *addstr);
int widget_add_text_at(struct window_t *widget, size_t where, char *c);
void widget_set_size_hints(struct window_t *widget, 
                           struct window_t *relativeto,
                           int hint, int x, int y, int w, int h);
void widget_size_changed(struct window_t *widget);
void widget_set_tabindex(struct window_t *parent, struct window_t *widget);
void widget_next_tabstop(struct window_t *window);
void widget_set_text_alignment(struct window_t *widget, int alignment);

#ifdef __cplusplus
}
#endif

#endif      /* CLIENT_WINDOW_H */
