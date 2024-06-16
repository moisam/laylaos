/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: listview.h
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
 *  \file listview.h
 *
 *  Declarations and struct definitions for the listview widget.
 */

#ifndef LISTVIEW_H
#define LISTVIEW_H

#include "../client/window.h"
#include "../mouse-state-struct.h"
#include "../font.h"

// standard inputbox height (depends on our fixed font height)
//#define LISTVIEW_LINE_HEIGHT        (__global_gui_data.mono.charh + 8)
#define LISTVIEW_LINE_HEIGHT        24

struct listview_entry_t
{
    char *text;
    int index;
    int selected:1;
};

struct listview_t
{
    struct window_t window;
    struct gc_t backbuf_gc;
    struct scrollbar_t *vscroll;

    int entry_count, entry_len;
    struct listview_entry_t *entries;
    struct listview_entry_t *last_down, *last_clicked;

    int cur_entry;      // current highlighted entry
    unsigned long long last_click_time;     // used to handle double-clicks
    int scrolly;    // current y scroll offset
    int vh;         // virtual height
    char modifiers; // state of midifier keys, used in multi-selection

    // callback functions (optional)
    void (*entry_doubleclick_callback)(struct listview_t *, int);
    void (*entry_click_callback)(struct listview_t *, int);
    void (*selection_change_callback)(struct listview_t *);

#define LISTVIEW_FLAG_MULTISELECT   0x01
    int flags;
};

struct listview_t *listview_new(struct gc_t *gc, struct window_t *w,
                                int x, int y, int width, int height);
void listview_destroy(struct window_t *listview_window);

void listview_mouseover(struct window_t *listview_window, 
                        struct mouse_state_t *mstate);
void listview_mousedown(struct window_t *listview_window, 
                        struct mouse_state_t *mstate);
void listview_mouseup(struct window_t *listview_window, 
                        struct mouse_state_t *mstate);
void listview_mouseexit(struct window_t *listview_window);
void listview_unfocus(struct window_t *listview_window);
void listview_focus(struct window_t *listview_window);
void listview_repaint(struct window_t *listview_window, int is_active_child);
void listview_size_changed(struct window_t *listview_window);

void listview_add_item(struct listview_t *listview, int index, char *str);
void listview_append_item(struct listview_t *listview, char *str);
void listview_remove_item(struct listview_t *listview, int index);
void listview_remove_all(struct listview_t *listview);
void listview_clear_selection(struct listview_t *listview);
void listview_set_multiselect(struct listview_t *listview, int);

int listview_keypress(struct window_t *listview_window, 
                        char key, char modifiers);
int listview_keyrelease(struct window_t *listview_window, 
                        char key, char modifiers);

int listview_get_selected(struct listview_t *listv,
                          struct listview_entry_t **res);
void listview_free_list(struct listview_entry_t *entries, int entry_count);

#endif //LISTVIEW_H
