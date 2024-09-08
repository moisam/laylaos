/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: window-struct.h
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
 *  \file window-struct.h
 *
 *  Definition of the client window structure.
 */

#ifndef CLIENT_WINDOW_STRUCT_DEFINED
#define CLIENT_WINDOW_STRUCT_DEFINED    1

struct window_t
{
    struct window_t *parent;

    int16_t x, y;
    uint16_t w, h;
    uint32_t flags;
    winid_t winid;
    winid_t owner_winid;        // only used for popups & menus
    int8_t type;
    int8_t visible;
    char *title;
    size_t title_alloced;       // size of memory alloc'd for title
    size_t title_len;           // title length in chars
    int text_alignment;
    
    uint32_t fgcolor, bgcolor;

    int shmid;
    uint8_t *canvas;
    uint32_t canvas_size;
    uint32_t canvas_pitch;
    
    int tab_index;

    void *internal_data;

    struct gc_t *gc;
    RectList *clip_rects;

    mouse_buttons_t last_button_state;

    void (*mousedown)(struct window_t *, struct mouse_state_t *);
    void (*mouseover)(struct window_t *, struct mouse_state_t *);
    void (*mouseup)(struct window_t *, struct mouse_state_t *);
    void (*mouseenter)(struct window_t *, struct mouse_state_t *);
    void (*mouseexit)(struct window_t *);
    void (*focus)(struct window_t *);
    void (*unfocus)(struct window_t *);
    void (*repaint)(struct window_t *, int);
    void (*destroy)(struct window_t *);
    void (*event_handler)(struct event_t *);
    int (*keypress)(struct window_t *, char, char);
    int (*keyrelease)(struct window_t *, char, char);

    struct window_t *active_child;
    struct window_t *tracked_child;
    //struct window_t *drag_child;
    //struct window_t *focused_child;
    struct window_t *mouseover_child;
    struct window_t *mousedown_child;
    List *children;
    
    union
    {
        struct menu_item_t *main_menu;      // used by the parent window to 
                                            // reference its menu frames
        struct menu_item_t *menu_item;      // used by the menu frame itself
    };

    struct menu_item_t *displayed_menu;
    
    struct menu_shortcut_t *menu_shortcuts;
    
    struct statusbar_t *statusbar;
    
    // for relative positioning and sizing of widgets (not main windows)
    struct window_t *relative_to;
    int16_t relative_x, relative_y;
    uint16_t relative_w, relative_h;

    // for the possible values, see 'Widget alignment types' in window-defs.h
    int resize_hints;

    void (*size_changed)(struct window_t *);
    void (*theme_changed)(struct window_t *);
};

#endif      /* CLIENT_WINDOW_STRUCT_DEFINED */
