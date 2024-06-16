/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: scrollbar.h
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
 *  \file scrollbar.h
 *
 *  Declarations and struct definitions for the scrollbar widget.
 */

#ifndef SCROLLBAR_H
#define SCROLLBAR_H

#include "../client/window.h"
#include "../mouse-state-struct.h"
#include "../font.h"

// standard scrollbar height & width
#define VSCROLLBAR_WIDTH            16
#define HSCROLLBAR_HEIGHT           16

struct scrollbar_t
{
    struct window_t window;
    int min, max, val;  // min, max and current value
    int step;           // step used to increment or decrement current value
    int thumbh, thumbw; // thumb (slider) height & width
    int thumbdelta;     // offset from where the mouse was pressed inside the 
                        // thumb (slider), used when dragging the thumb
    int scrolling;      // set when the left mouse button is pressed on the
                        // thumb (slider)

#define SCROLLBAR_FLAG_DISABLED     0x01
    int flags;

    // callback functions (optional)
    void (*value_change_callback)(struct window_t *, struct scrollbar_t *);
};

struct scrollbar_t *scrollbar_new(struct gc_t *, struct window_t *, int);
void scrollbar_destroy(struct window_t *scrollbar_window);

void scrollbar_mouseover(struct window_t *scrollbar_window, 
                            struct mouse_state_t *mstate);
void scrollbar_mousedown(struct window_t *scrollbar_window, 
                            struct mouse_state_t *mstate);
void scrollbar_mouseup(struct window_t *scrollbar_window, 
                            struct mouse_state_t *mstate);
void scrollbar_mouseexit(struct window_t *scrollbar_window);
void scrollbar_unfocus(struct window_t *scrollbar_window);
void scrollbar_focus(struct window_t *scrollbar_window);
void scrollbar_repaint(struct window_t *scrollbar_window, int is_active_child);
int scrollbar_keypress(struct window_t *scrollbar_window, 
                            char code, char modifiers);
int scrollbar_keyrelease(struct window_t *scrollbar_window, 
                            char code, char modifiers);

void scrollbar_parent_size_changed(struct window_t *parent,
                                   struct window_t *scrollbar_window);

void scrollbar_disable(struct scrollbar_t *scrollbar);
void scrollbar_enable(struct scrollbar_t *scrollbar);
void scrollbar_set_step(struct scrollbar_t *sbar, int step);
void scrollbar_set_max(struct scrollbar_t *sbar, int max);
void scrollbar_set_min(struct scrollbar_t *sbar, int min);
void scrollbar_set_val(struct scrollbar_t *sbar, int val);

#endif //SCROLLBAR_H
