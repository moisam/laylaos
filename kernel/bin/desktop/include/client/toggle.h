/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: toggle.h
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
 *  \file toggle.h
 *
 *  Declarations and struct definitions for the toggle widget.
 */

#ifndef TOGGLE_H
#define TOGGLE_H

#include "../client/window.h"

#define TOGGLE_WIDTH                50
#define TOGGLE_HEIGHT               24

struct toggle_t
{
    struct window_t window;
    int toggled;

    // callback functions (optional)
    void (*toggle_change_callback)(struct window_t *, struct toggle_t *);
};


struct toggle_t *toggle_new(struct gc_t *gc, struct window_t *window,
                            int x, int y);
void toggle_destroy(struct window_t *button);

void toggle_mouseover(struct window_t *toggle_window, 
                        struct mouse_state_t *mstate);
void toggle_mouseup(struct window_t *toggle_window, 
                        struct mouse_state_t *mstate);
void toggle_mousedown(struct window_t *toggle_window, 
                        struct mouse_state_t *mstate);
void toggle_mouseexit(struct window_t *toggle_window);
void toggle_repaint(struct window_t *toggle_window, int is_active_child);
void toggle_unfocus(struct window_t *toggle_window);
void toggle_focus(struct window_t *toggle_window);
int toggle_keypress(struct window_t *toggle_window, char code, char modifiers);

void toggle_set_toggled(struct toggle_t *toggle, int toggled);

#endif //TOGGLE_H
