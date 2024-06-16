/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: menu.h
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
 *  \file menu.h
 *
 *  Declarations and struct definitions for working with menus on the client
 *  side.
 */

#ifndef MENU_H
#define MENU_H

#ifndef GUI_SERVER

#include "font.h"

// standard menu height (depends on our fixed font height)
#define MENU_HEIGHT             24

#define MENU_TOP_PADDING        4

// types of menu items
#define TYPE_MENUITEM           1
#define TYPE_SUBMENU            2
#define TYPE_TOGGLE_MENUITEM    3
#define TYPE_CHECKED_MENUITEM   4

// types of menu icons
#define MENU_ICON_NONE          0
#define MENU_ICON_SYSTEM        1
#define MENU_ICON_FILE          2

// menu icon indices for type MENU_ICON_SYSTEM
#include "menu-icon.h"

struct menu_item_t
{
    int x, w;
    uint16_t id;
    uint8_t type;
    char *title;
    char accelerator;
    int highlighted;

#define MENU_ITEM_DISABLED              0x01
#define MENU_ITEM_TOGGLED               0x02
#define MENU_ITEM_CHECKED               0x04
    int flags;
    
    struct menu_shortcut_t *shortcut;

    struct menu_icon_t icon;
    struct menu_item_t *first_child;
    struct menu_item_t *next;
    struct window_t *owner, *frame;
    struct menu_item_t *next_displayed;
    void (*handler)(winid_t winid);
};

struct menu_shortcut_t
{
    char shortcut_key;      // shortcut key           (usually ASCII char)
    char shortcut_mod;      // shortcut modifier keys (CTRL, ...)
    struct menu_item_t *mi;
    struct menu_shortcut_t *next;
};

struct menu_item_t *menu_new_item(struct menu_item_t *parent, char *title);
struct menu_item_t *menu_new_toggle_item(struct menu_item_t *parent, char *title);
struct menu_item_t *menu_new_checked_item(struct menu_item_t *parent, 
                                          char *title);
struct menu_item_t *menu_new_icon_item(struct menu_item_t *parent, char *title,
                                       char *icon_filename, uint8_t icon_index);

struct menu_item_t *mainmenu_new_item(struct window_t *window, char *title);

struct menu_item_t *menu_new_submenu(struct menu_item_t *parent, char *title);
struct menu_item_t *menu_new_icon_submenu(struct menu_item_t *parent, char *title,
                                          char *icon_filename, uint8_t icon_index);

void finalize_menus(struct window_t *window);
void draw_mainmenu(struct window_t *window);

void menu_item_set_enabled(struct menu_item_t *mi, int enabled);
void menu_item_set_toggled(struct menu_item_t *mi, int toggled);
void menu_item_set_checked(struct menu_item_t *mi, int checked);
void menu_item_set_shortcut(struct window_t *window, struct menu_item_t *mi,
                            char key, char modifiers);

#endif      /* GUI_SERVER */

#endif      /* MENU_H */
