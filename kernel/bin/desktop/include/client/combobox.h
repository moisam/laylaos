/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: combobox.h
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
 *  \file combobox.h
 *
 *  Declarations and struct definitions for the combobox widget.
 */

#ifndef COMBOBOX_H
#define COMBOBOX_H

#include "../client/window.h"
#include "../client/listview.h"
#include "../mouse-state-struct.h"

struct combobox_t
{
    struct window_t window;

    struct window_t *internal_frame;
    struct listview_t *internal_list;
    int list_shown;
    int show_later;

    // callback functions (optional)
    void (*entry_click_callback)(struct combobox_t *, int);
};

struct combobox_t *combobox_new(struct gc_t *gc, struct window_t *parent,
                                int x, int y, int width, char *title);
void combobox_destroy(struct window_t *combobox_window);

void combobox_mouseover(struct window_t *combobox_window, 
                        struct mouse_state_t *mstate);
void combobox_mousedown(struct window_t *combobox_window, 
                        struct mouse_state_t *mstate);
void combobox_mouseup(struct window_t *combobox_window, 
                        struct mouse_state_t *mstate);
void combobox_mouseexit(struct window_t *combobox_window);
void combobox_unfocus(struct window_t *combobox_window);
void combobox_focus(struct window_t *combobox_window);
void combobox_repaint(struct window_t *combobox_window, int is_active_child);

void combobox_append_text(struct window_t *combobox_window, char *addstr);
void combobox_set_text(struct window_t *combobox_window, char *addstr);

void combobox_add_item(struct combobox_t *combobox, int index, char *str);
void combobox_append_item(struct combobox_t *combobox, char *str);
void combobox_remove_item(struct combobox_t *combobox, int index);
void combobox_set_selected_item(struct combobox_t *combobox, int index);

#endif //COMBOBOX_H
