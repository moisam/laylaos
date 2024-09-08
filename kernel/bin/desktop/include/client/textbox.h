/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: textbox.h
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
 *  \file textbox.h
 *
 *  Declarations and struct definitions for the textbox widget.
 */

#ifndef TEXTBOX_H
#define TEXTBOX_H

#include "../client/window.h"
#include "../mouse-state-struct.h"

struct textbox_t
{
    struct window_t window;
};

struct textbox_t *textbox_new(struct gc_t *gc, struct window_t *w,
                              int x, int y, int width, int height, char *t);
void textbox_destroy(struct window_t *textbox_window);

void textbox_mouseover(struct window_t *textbox_window, 
                        struct mouse_state_t *mstate);
void textbox_mousedown(struct window_t *textbox_window, 
                        struct mouse_state_t *mstate);
void textbox_mouseup(struct window_t *textbox_window, 
                        struct mouse_state_t *mstate);
void textbox_mouseexit(struct window_t *textbox_window);
void textbox_unfocus(struct window_t *textbox_window);
void textbox_focus(struct window_t *textbox_window);
void textbox_repaint(struct window_t *textbox_window, int is_active_child);

void textbox_append_text(struct window_t *textbox_window, char *addstr);
void textbox_set_text(struct window_t *textbox_window, char *addstr);

void textbox_theme_changed(struct window_t *window);

#endif //TEXTBOX_H
