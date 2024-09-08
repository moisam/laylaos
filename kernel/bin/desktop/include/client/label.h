/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: label.h
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
 *  \file label.h
 *
 *  Declarations and struct definitions for the label widget.
 */

#ifndef LABEL_H
#define LABEL_H

#include "../client/window.h"
#include "../mouse-state-struct.h"

struct label_t
{
    struct window_t window;
    void *internal_data;
};

struct label_t *label_new(struct gc_t *gc, struct window_t *w,
                          int x, int y, int width, int height, char *text);
void label_destroy(struct window_t *label_window);

void label_mouseover(struct window_t *label_window, 
                        struct mouse_state_t *mstate);
void label_mousedown(struct window_t *label_window, 
                        struct mouse_state_t *mstate);
void label_mouseup(struct window_t *label_window, 
                        struct mouse_state_t *mstate);
void label_mouseexit(struct window_t *label_window);
void label_unfocus(struct window_t *label_window);
void label_focus(struct window_t *label_window);
void label_repaint(struct window_t *label_window, int is_active_child);

void label_set_text(struct label_t *label_window, char *addstr);
void label_set_text_alignment(struct label_t *label, int alignment);

void label_set_foreground(struct label_t *label, uint32_t color);
void label_set_background(struct label_t *label, uint32_t color);

void label_theme_changed(struct window_t *window);

#endif      /* LABEL_H */
