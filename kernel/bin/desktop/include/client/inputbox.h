/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: inputbox.h
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
 *  \file inputbox.h
 *
 *  Declarations and struct definitions for the inputbox widget.
 */

#ifndef INPUTBOX_H
#define INPUTBOX_H

#include "../client/window.h"
#include "../mouse-state-struct.h"
#include "../font.h"

// standard inputbox height (depends on our fixed font height)
//#define INPUTBOX_HEIGHT             (__global_gui_data.mono.charh + 12)
#define INPUTBOX_HEIGHT             28

struct inputbox_t
{
    struct window_t window;
    struct gc_t backbuf_gc;
    int caretx, carety;     // caret position, pixel-based
    int careth;             // caret height in pixels

    int scrollx;    // current x scroll offset
    int vw;         // virtual width

    int selecting;
    size_t select_start, select_end;
    curid_t global_curid;

#define INPUTBOX_FLAG_CARET_SHOWN           0x01
    int flags;
};

struct inputbox_t *inputbox_new(struct gc_t *gc, struct window_t *w,
                                int x, int y, int width, char *text);
void inputbox_destroy(struct window_t *inputbox_window);

void inputbox_mouseover(struct window_t *inputbox_window, 
                        struct mouse_state_t *mstate);
void inputbox_mousedown(struct window_t *inputbox_window, 
                        struct mouse_state_t *mstate);
void inputbox_mouseup(struct window_t *inputbox_window, 
                        struct mouse_state_t *mstate);
void inputbox_mouseexit(struct window_t *inputbox_window);
void inputbox_unfocus(struct window_t *inputbox_window);
void inputbox_focus(struct window_t *inputbox_window);
void inputbox_repaint(struct window_t *inputbox_window, int is_active_child);
void inputbox_size_changed(struct window_t *inputbox_window);

void inputbox_append_text(struct window_t *inputbox_window, char *addstr);
void inputbox_set_text(struct window_t *inputbox_window, char *addstr);
int inputbox_keypress(struct window_t *inputbox_window, 
                        char key, char modifiers);
int inputbox_keyrelease(struct window_t *inputbox_window, 
                        char key, char modifiers);

#endif //INPUTBOX_H
