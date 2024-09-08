/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: spinner.h
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
 *  \file spinner.h
 *
 *  Declarations and struct definitions for the spinner widget.
 */

#ifndef SPINNER_H
#define SPINNER_H

#include "../client/window.h"
#include "../mouse-state-struct.h"
#include "../font.h"

struct spinner_t
{
    struct window_t window;
    struct gc_t backbuf_gc;
    int min, max, val;      // min, max and current value
    int caretx, carety;     // caret position, pixel-based
    int careth;             // caret height in pixels

    int scrollx;    // current x scroll offset
    int vw;         // virtual width

    int selecting;
    size_t select_start, select_end;
    curid_t global_curid;

#define INPUTBOX_FLAG_CARET_SHOWN           0x01
    int flags;

    // callback functions (optional)
    void (*value_change_callback)(struct window_t *, struct spinner_t *);
};

struct spinner_t *spinner_new(struct gc_t *gc, struct window_t *parent,
                              int x, int y, int width);
void spinner_destroy(struct window_t *spinner_window);

void spinner_mouseover(struct window_t *spinner_window, 
                        struct mouse_state_t *mstate);
void spinner_mousedown(struct window_t *spinner_window, 
                        struct mouse_state_t *mstate);
void spinner_mouseup(struct window_t *spinner_window, 
                        struct mouse_state_t *mstate);
void spinner_mouseexit(struct window_t *spinner_window);
void spinner_unfocus(struct window_t *spinner_window);
void spinner_focus(struct window_t *spinner_window);
void spinner_repaint(struct window_t *spinner_window, int is_active_child);
void spinner_size_changed(struct window_t *spinner_window);

int spinner_keypress(struct window_t *spinner_window, 
                        char key, char modifiers);
int spinner_keyrelease(struct window_t *spinner_window, 
                        char key, char modifiers);

void spinner_set_max(struct spinner_t *spinner, int max);
void spinner_set_min(struct spinner_t *spinner, int min);
void spinner_set_val(struct spinner_t *spinner, int val);

void spinner_theme_changed(struct window_t *window);

#endif //SPINNER_H
