/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: radio-button.h
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
 *  \file radio-button.h
 *
 *  Declarations and struct definitions for the radio button widget.
 */

#ifndef RADIOBUTTON_H
#define RADIOBUTTON_H

#include "../client/window.h"
#include "../client/label.h"
#include "../client/button-states.h"


struct radiobutton_t
{
    struct window_t window;
    int state;
    int selected;
    char *group;
    struct label_t *label;

    // callback functions (optional)
    void (*button_click_callback)(struct radiobutton_t *, int, int);
};


struct radiobutton_t *radiobutton_new(struct gc_t *gc, struct window_t *window,
                                      int x, int y, int w, int h, char *title);
void radiobutton_destroy(struct window_t *button);

void radiobutton_mouseover(struct window_t *button_window, 
                           struct mouse_state_t *mstate);
void radiobutton_mouseup(struct window_t *button_window, 
                           struct mouse_state_t *mstate);
void radiobutton_mousedown(struct window_t *button_window, 
                           struct mouse_state_t *mstate);
void radiobutton_mouseexit(struct window_t *button_window);
void radiobutton_repaint(struct window_t *button_window, int is_active_child);
void radiobutton_unfocus(struct window_t *button_window);
void radiobutton_focus(struct window_t *button_window);
int radiobutton_keypress(struct window_t *button_window, char code, char modifiers);

void radiobutton_set_title(struct radiobutton_t *button, char *new_title);

void radiobutton_disable(struct radiobutton_t *button);
void radiobutton_enable(struct radiobutton_t *button);
void radiobutton_set_selected(struct radiobutton_t *button);

void radiobutton_theme_changed(struct window_t *window);

#endif //RADIOBUTTON_H
