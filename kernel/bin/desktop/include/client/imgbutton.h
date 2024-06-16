/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: imgbutton.h
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
 *  \file imgbutton.h
 *
 *  Declarations and struct definitions for the image button widget.
 */

#ifndef IMGBUTTON_H
#define IMGBUTTON_H

#include "../client/window.h"
#include "../client/button-states.h"


struct imgbutton_t
{
    struct window_t window;
    uint8_t state;
    uint8_t push_state;     // only used for pushbuttons
    struct button_color_t colors[BUTTON_COLOR_ARRAY_LENGTH];
    struct bitmap32_t bitmap;       // "normal" bitmap
    struct bitmap32_t grey_bitmap;  // bitmap for the disabled state
    int flags;      // see button-states.h

    // callback functions (optional)
    void (*button_click_callback)(struct imgbutton_t *, int, int);
    void (*push_state_change_callback)(struct imgbutton_t *);
};


struct imgbutton_t *imgbutton_new(struct gc_t *gc, struct window_t *window,
                                  int x, int y, int w, int h);

struct imgbutton_t *push_imgbutton_new(struct gc_t *gc, 
                                       struct window_t *parent,
                                       int x, int y, int w, int h);

void imgbutton_destroy(struct window_t *button);

void imgbutton_mouseover(struct window_t *button_window, 
                            struct mouse_state_t *mstate);
void imgbutton_mouseup(struct window_t *button_window, 
                            struct mouse_state_t *mstate);
void imgbutton_mousedown(struct window_t *button_window, 
                            struct mouse_state_t *mstate);
void imgbutton_mouseexit(struct window_t *button_window);
void imgbutton_repaint(struct window_t *button_window, int is_active_child);
void imgbutton_unfocus(struct window_t *button_window);
void imgbutton_focus(struct window_t *button_window);
int imgbutton_keypress(struct window_t *button_window, 
                        char code, char modifiers);

void imgbutton_set_image(struct imgbutton_t *button, 
                            struct bitmap32_t *bitmap);
void imgbutton_set_sysicon(struct imgbutton_t *button, char *name);
void imgbutton_set_bordered(struct imgbutton_t *button, int bordered);

void imgbutton_set_push_state(struct imgbutton_t *button, int pushed);

void imgbutton_disable(struct imgbutton_t *button);
void imgbutton_enable(struct imgbutton_t *button);

#endif //IMGBUTTON_H
