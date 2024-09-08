/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: file-selector.h
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
 *  \file file-selector.h
 *
 *  Declarations and struct definitions for the file selector widget.
 */

#ifndef FILE_SELECTOR_H
#define FILE_SELECTOR_H

#include "window.h"
#include "scrollbar.h"
#include "listview.h"

#define FILE_SELECTOR_ICON_VIEW             0
#define FILE_SELECTOR_LIST_VIEW             1
#define FILE_SELECTOR_COMPACT_VIEW          2

#define LISTVIEW_ENTRYHEIGHT                LISTVIEW_LINE_HEIGHT
#define LISTVIEW_LEFT_MARGIN                4
#define LISTVIEW_ICONWIDTH                  20

#define ICONVIEW_ENTRYWIDTH                 128
#define ICONVIEW_ENTRYHEIGHT                112
#define ICONVIEW_LEFT_MARGIN                32
#define ICONVIEW_ICONWIDTH                  64

// File selector is multiselect by default.
// Unset the FILE_SELECTOR_FLAG_MULTISELECT flag to make it single-select.
#define FILE_SELECTOR_FLAG_MULTISELECT      1

// Max number of filters supported
#define FILE_SELECTOR_MAX_FILTERS           16


struct file_entry_t
{
    char *name;
    mode_t mode;
    time_t mtime, atime, ctime;
    off_t file_size;
    int highlighted;
    struct bitmap32_t *icon;

    // for internal use
    size_t name_line_start[2];  // start index of the two lines
    size_t name_line_end[2];    // end index of the two lines
    size_t name_line_pixels[2]; // width in pixels
};

struct file_selector_t
{
    struct window_t window;
    struct gc_t backbuf_gc;
    struct scrollbar_t *vscroll, *hscroll;

    int entry_count;
    struct file_entry_t *entries;
    struct file_entry_t *last_down, *last_clicked;

    int cur_entry;      // current highlighted entry
    int selection_box_entry;    // entry with a surrounding black selection box
    unsigned long long last_click_time;     // used to handle double-clicks
    int scrolly;    // current y scroll offset (for vertical scrolling)
    int scrollx;    // current x scroll offset (for horizontal scrolling)
    int vh;         // virtual height, used in icon and list view modes
    int vw;         // virtual width, used in compact mode
    int longest_entry_width;    // longest entry width in pixels, used in
                                // compact mode
    char modifiers; // state of midifier keys, used in multi-selection

    // view mode - see the FILE_SELECTOR_* defines above
    int viewmode;

    int flags;

    char *filters[FILE_SELECTOR_MAX_FILTERS];

    // callback functions (optional)
    void (*entry_doubleclick_callback)(struct file_selector_t *, 
                                        struct file_entry_t *);
    void (*entry_click_callback)(struct file_selector_t *, 
                                        struct file_entry_t *);
    void (*selection_change_callback)(struct file_selector_t *);
};


/****************************************
 * Function prototypes
 ****************************************/

struct file_selector_t *file_selector_new(struct gc_t *, struct window_t *,
                                          int x, int y, 
                                          int width, int height, char *);
void file_selector_destroy(struct window_t *selector_window);

void file_selector_mouseover(struct window_t *selector_window, 
                                struct mouse_state_t *mstate);
void file_selector_mousedown(struct window_t *selector_window, 
                                struct mouse_state_t *mstate);
void file_selector_mouseup(struct window_t *selector_window, 
                                struct mouse_state_t *mstate);
void file_selector_mouseexit(struct window_t *selector_window);
void file_selector_unfocus(struct window_t *selector_window);
void file_selector_focus(struct window_t *selector_window);
void file_selector_repaint(struct window_t *selector_window, 
                                int is_active_child);
void file_selector_size_changed(struct window_t *window);

int file_selector_set_path(struct file_selector_t *selector, char *new_path);
char *file_selector_get_path(struct file_selector_t *selector);
int file_selector_keypress(struct window_t *selector_window, 
                                char code, char modifiers);
int file_selector_keyrelease(struct window_t *selector_window, 
                                char key, char modifiers);

void file_selector_free_list(struct file_entry_t *entries, int entry_count);
int file_selector_get_selected(struct file_selector_t *selector, 
                                struct file_entry_t **res);
void file_selector_select_all(struct file_selector_t *selector);
void file_selector_unselect_all(struct file_selector_t *selector);
void file_selector_set_viewmode(struct file_selector_t *selector, int mode);

void file_selector_clear_filters(struct file_selector_t *selector);
void file_selector_add_filter(struct file_selector_t *selector, char *filter);
void file_selector_reload_entries(struct file_selector_t *selector);

#endif //FILE_SELECTOR_H
