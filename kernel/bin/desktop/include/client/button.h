/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: button.h
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
 *  \file button.h
 *
 *  Declarations and struct definitions for the button widget.
 */

#ifndef BUTTON_H
#define BUTTON_H

#include "../client/window.h"
#include "../client/button-states.h"


struct button_t
{
    struct window_t window;
    uint8_t state;
    struct button_color_t colors[BUTTON_COLOR_ARRAY_LENGTH];
    void *internal_data;
    int flags;

    // callback functions (optional)
    void (*button_click_callback)(struct button_t *, int, int);
};


struct button_t *button_new(struct gc_t *gc, struct window_t *window,
                            int x, int y, int w, int h, char *);
void button_destroy(struct window_t *button);

void button_mouseover(struct window_t *button_window, 
                        struct mouse_state_t *mstate);
void button_mouseup(struct window_t *button_window, 
                        struct mouse_state_t *mstate);
void button_mousedown(struct window_t *button_window, 
                        struct mouse_state_t *mstate);
void button_mouseexit(struct window_t *button_window);
void button_repaint(struct window_t *button_window, int is_active_child);
void button_unfocus(struct window_t *button_window);
void button_focus(struct window_t *button_window);
//void button_size_changed(struct window_t *window);
int button_keypress(struct window_t *button_window, char code, char modifiers);

void button_set_title(struct button_t *button, char *new_title);
void button_set_bordered(struct button_t *button, int bordered);

void button_disable(struct button_t *button);
void button_enable(struct button_t *button);

#endif //BUTTON_H
