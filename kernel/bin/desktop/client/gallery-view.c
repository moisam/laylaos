/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: gallery-view.c
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
 *  \file gallery-view.c
 *
 *  The implementation of a gallery view widget. It uses the file selector 
 *  widget internally to do the heavy work of painting, scrolling, selecting
 *  items, etc.
 */

#include "../include/client/gallery-view.h"
#include "../desktop/desktop_entry_lines.c"
#include "../include/menu.h"

#include "file-selector-inlines.c"


#define SELECTOR(w)             (struct window_t *)((w)->selector)

#define GLOB                    __global_gui_data


/*
 * Callback functions for our internal file selector to let us know of
 * selection changes.
 */

static void doubleclick_callback(struct file_selector_t *selector,
                                 struct file_entry_t *entry)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)selector->window.parent;

    if(gallery && gallery->entry_doubleclick_callback)
    {
        gallery->entry_doubleclick_callback(gallery, entry);
    }
}


static void selection_change_callback(struct file_selector_t *selector)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)selector->window.parent;

    if(gallery && gallery->selection_change_callback)
    {
        gallery->selection_change_callback(gallery);
    }
}


static void click_callback(struct file_selector_t *selector, 
                           struct file_entry_t *entry)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)selector->window.parent;

    if(gallery && gallery->entry_click_callback)
    {
        gallery->entry_click_callback(gallery, entry);
    }
}


struct gallery_view_t *gallery_view_new(struct gc_t *gc, 
                                        struct window_t *parent,
                                        int x, int y, int w, int h)
{
    struct gallery_view_t *gallery;

    if(!(gallery = malloc(sizeof(struct gallery_view_t))))
    {
        return NULL;
    }

    memset(gallery, 0, sizeof(struct gallery_view_t));

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    gallery->window.type = WINDOW_TYPE_GALLERY_VIEW;
    gallery->window.x = x;
    gallery->window.y = y;
    gallery->window.w = w;
    gallery->window.h = h;
    gallery->window.gc = gc;
    gallery->window.visible = 1;

    gallery->window.repaint = gallery_view_repaint;
    gallery->window.mousedown = gallery_view_mousedown;
    gallery->window.mouseover = gallery_view_mouseover;
    gallery->window.mouseup = gallery_view_mouseup;
    gallery->window.mouseexit = gallery_view_mouseexit;
    gallery->window.unfocus = gallery_view_unfocus;
    gallery->window.focus = gallery_view_focus;
    gallery->window.destroy = gallery_view_destroy;
    gallery->window.keypress = gallery_view_keypress;
    gallery->window.keyrelease = gallery_view_keyrelease;
    gallery->window.size_changed = gallery_view_size_changed;

    if(!(gallery->selector = 
            file_selector_new(gc, (struct window_t *)gallery, x, y, w, h, NULL)))
    {
        free(gallery);
        return NULL;
    }

    gallery->selector->entry_doubleclick_callback = doubleclick_callback;
    gallery->selector->entry_click_callback = click_callback;
    gallery->selector->selection_change_callback = selection_change_callback;

    window_insert_child(parent, (struct window_t *)gallery);

    return gallery;
}

void gallery_view_destroy(struct window_t *gallery_window)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;

    if(gallery->selector)
    {
        file_selector_destroy(SELECTOR(gallery));
        gallery->selector = NULL;
    }

    widget_destroy((struct window_t *)gallery_window);
}

void gallery_view_repaint(struct window_t *gallery_window, 
                          int is_active_child)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    file_selector_repaint(SELECTOR(gallery), is_active_child);
}

void gallery_view_mouseover(struct window_t *gallery_window, 
                            struct mouse_state_t *mstate)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    file_selector_mouseover(SELECTOR(gallery), mstate);
}

void gallery_view_mousedown(struct window_t *gallery_window, 
                            struct mouse_state_t *mstate)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    file_selector_mousedown(SELECTOR(gallery), mstate);
}

void gallery_view_mouseexit(struct window_t *gallery_window)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    file_selector_mouseexit(SELECTOR(gallery));
}

void gallery_view_mouseup(struct window_t *gallery_window, 
                          struct mouse_state_t *mstate)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    file_selector_mouseup(SELECTOR(gallery), mstate);
}

void gallery_view_unfocus(struct window_t *gallery_window)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    file_selector_unfocus(SELECTOR(gallery));
}


void gallery_view_focus(struct window_t *gallery_window)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    file_selector_focus(SELECTOR(gallery));
}

/*
 * Get the list of selected items. If res is NULL, the count is returned
 * (useful in querying the number of items without getting the actual items).
 *
 * Returns:
 *    0 if no items are selected
 *    -1 if an error occurred
 *    otherwise the count of selected items
 *
 * The returned list should be freed by calling gallery_view_free_list().
 */
int gallery_view_get_selected(struct gallery_view_t *gallery, 
                              struct file_entry_t **res)
{
    return file_selector_get_selected(gallery->selector, res);
}

void gallery_view_free_list(struct file_entry_t *entries, int entry_count)
{
    file_selector_free_list(entries, entry_count);
}

void gallery_view_select_all(struct gallery_view_t *gallery)
{
    file_selector_select_all(gallery->selector);
}

void gallery_view_unselect_all(struct gallery_view_t *gallery)
{
    file_selector_unselect_all(gallery->selector);
}

int gallery_view_keypress(struct window_t *gallery_window, 
                           char code, char modifiers)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    return file_selector_keypress(SELECTOR(gallery), code, modifiers);
}

int gallery_view_keyrelease(struct window_t *gallery_window, 
                             char code, char modifiers)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)gallery_window;
    return file_selector_keyrelease(SELECTOR(gallery), code, modifiers);
}

void gallery_view_size_changed(struct window_t *window)
{
    struct gallery_view_t *gallery = (struct gallery_view_t *)window;
    gallery->selector->window.x = window->x;
    gallery->selector->window.y = window->y;
    gallery->selector->window.w = window->w;
    gallery->selector->window.h = window->h;
    file_selector_size_changed(SELECTOR(gallery));
}

void gallery_view_add(struct gallery_view_t *gallery, char *title, 
                      struct bitmap32_t *bitmap)
{
    struct file_entry_t *entry;
    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;

    if(!gallery->selector)
    {
        return;
    }

    if(gallery->selector->entries == NULL)
    {
        if(!(gallery->selector->entries = 
                        malloc(32 * sizeof(struct file_entry_t))))
        {
            return;
        }

        gallery->entries_malloced = 32;
    }
    else if(gallery->selector->entry_count >= gallery->entries_malloced)
    {
        struct file_entry_t *new_entries;

        if(!(new_entries = realloc(gallery->selector->entries, 
                                    (gallery->entries_malloced << 1) * 
                                            sizeof(struct file_entry_t))))
        {
            return;
        }

        gallery->selector->entries = new_entries;
        gallery->entries_malloced <<= 1;
    }

    entry = &gallery->selector->entries[gallery->selector->entry_count++];

    A_memset(entry, 0, sizeof(struct file_entry_t));
    entry->name = strdup(title);
    entry->icon = bitmap;

    if(entry->name)
    {
        split_two_lines(font, entry->name,
                              entry->name_line_start,
                              entry->name_line_end,
                              entry->name_line_pixels,
                              ICONVIEW_ENTRYWIDTH - 8);
    }

    // find the longest entry width so we do not have to go through this
    // again when painting and processing the mouse
    int pixels, longest_pixels = 0;

    for(entry = &gallery->selector->entries[0]; 
        entry < &gallery->selector->entries[gallery->selector->entry_count]; 
        entry++)
    {
        // we have to get the actual pixel width, as even if two entries
        // have the same number of characters, their pixel width might differ
        pixels = string_width(font, entry->name);

        if(pixels > longest_pixels)
        {
            longest_pixels = pixels;
        }
    }

    // add margins to ease calculations during painting
    gallery->selector->longest_entry_width = 
                    longest_pixels + LISTVIEW_ICONWIDTH + 4 + 4;

    file_selector_reset_scrolls(gallery->selector);
    file_selector_reset_click_count(gallery->selector);
}

