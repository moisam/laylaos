/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: gallery-view.h
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
 *  \file gallery-view.h
 *
 *  Declarations and struct definitions for the gallery view widget.
 */

#ifndef GALLERY_VIEW_H
#define GALLERY_VIEW_H

#include "window.h"
#include "file-selector.h"
#include "../bitmap.h"

struct gallery_view_t
{
    struct window_t window;
    struct gc_t backbuf_gc;
    struct file_selector_t *selector;
    int entries_malloced;   // file selector does not store this info

    // callback functions (optional)
    void (*entry_doubleclick_callback)(struct gallery_view_t *, 
                                        struct file_entry_t *);
    void (*entry_click_callback)(struct gallery_view_t *, 
                                        struct file_entry_t *);
    void (*selection_change_callback)(struct gallery_view_t *);
};

struct gallery_view_t *gallery_view_new(struct gc_t *gc,
                                        struct window_t *parent,
                                        int x, int y, 
                                        int width, int height);
void gallery_view_destroy(struct window_t *galley_window);

void gallery_view_mouseover(struct window_t *galley_window, 
                                struct mouse_state_t *mstate);
void gallery_view_mousedown(struct window_t *galley_window, 
                                struct mouse_state_t *mstate);
void gallery_view_mouseup(struct window_t *galley_window, 
                                struct mouse_state_t *mstate);
void gallery_view_mouseexit(struct window_t *galley_window);
void gallery_view_unfocus(struct window_t *galley_window);
void gallery_view_focus(struct window_t *galley_window);
void gallery_view_repaint(struct window_t *galley_window, 
                                int is_active_child);
void gallery_view_size_changed(struct window_t *window);

int gallery_view_keypress(struct window_t *galley_window, 
                                char code, char modifiers);
int gallery_view_keyrelease(struct window_t *galley_window, 
                                char key, char modifiers);

void gallery_view_free_list(struct file_entry_t *entries, int entry_count);
int gallery_view_get_selected(struct gallery_view_t *gallery, 
                                struct file_entry_t **res);
void gallery_view_select_all(struct gallery_view_t *gallery);
void gallery_view_unselect_all(struct gallery_view_t *gallery);

void gallery_view_add(struct gallery_view_t *gallery, char *title, 
                      struct bitmap32_t *bitmap);

#endif //GALLERY_VIEW_H
