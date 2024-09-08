/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: menu.c
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
 *  \file menu.c
 *
 *  Functions to create, display, distory and interact with window menus.
 */

#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#define KBDUS_DEFINE_KEYCODES
#include <kernel/kbdus.h>
#include "../include/gui.h"
#include "../include/list.h"
#include "../include/event.h"
#include "../include/menu.h"
#include "../include/keys.h"
#include "../include/kbd.h"
#include "inlines.c"


#define GLOB        __global_gui_data


struct bitmap32_t menu_icons = { 0, };
int menu_icon_count = 0;

static uint16_t next_id = 0;

#define is_divider(mi)      ((mi)->title[0] == '-' && (mi)->title[1] == '\0')
#define is_disabled(mi)     ((mi)->flags & MENU_ITEM_DISABLED)


int load_menu_icons(void)
{
    static int loaded = 0;
    
    if(loaded)
    {
        return 1;
    }
    
    if(!png_load(MENU_ICONS_FILE_PATH, &menu_icons))
    {
        return 0;
    }
    
    menu_icon_count = menu_icons.width / MENU_ICON_WIDTH;
    loaded = 1;
    
    return 1;
}


struct menu_item_t *alloc_menu_item(struct menu_item_t *parent,
                                    char *title, uint8_t type,
                                    uint8_t icon_type,
                                    char *icon_filename,
                                    uint8_t icon_index)
{
    struct menu_item_t *m, *m2;

    if(!title || !*title)
    {
        __set_errno(EINVAL);
        return NULL;
    }
    
    if(++next_id == 0)
    {
        // rounding up to zero (our field is 8 bits, supporting upto 
        // 255 menu items)
        __set_errno(ENOBUFS);
        return NULL;
    }

    if(!(m = malloc(sizeof(struct menu_item_t))))
    {
        __set_errno(ENOMEM);
        return NULL;
    }

    memset(m, 0, sizeof(struct menu_item_t));
    m->type = type;
    m->id = next_id;

    if(!(m->title = strdup(title)))
    {
        free(m);
        __set_errno(ENOMEM);
        return NULL;
    }

    m->icon.type = icon_type;
    m->icon.index = icon_index;
    m->icon.filename = icon_filename ? strdup(icon_filename) : NULL;

    if(parent)
    {
        if(!parent->first_child)
        {
            parent->first_child = m;
        }
        else
        {
            m2 = parent->first_child;
            
            while(m2->next)
            {
                m2 = m2->next;
            }
            
            m2->next = m;
        }
        
        m->owner = parent->owner;
    }
    
    return m;
}


/*
 * Create toggle-able menu items with icons.
 */
struct menu_item_t *menu_new_toggle_item(struct menu_item_t *parent, 
                                         char *title)
{
    uint8_t icon_type = MENU_ICON_SYSTEM;
    uint8_t icon_index = MENU_SYSTEM_TOGGLE_OFF;

    // try to load system icons (if not loaded yet)
    // we need them for the toggled/untoggled icons
    if(!load_menu_icons())
    {
        // fail back to menus without icons
        icon_type = MENU_ICON_NONE;
        icon_index = 0;
    }

    return alloc_menu_item(parent, title, TYPE_TOGGLE_MENUITEM,
                           icon_type, NULL, icon_index);
}


void menu_item_set_toggled(struct menu_item_t *mi, int toggled)
{
    if(!mi || mi->type != TYPE_TOGGLE_MENUITEM)
    {
        return;
    }
    
    if(toggled)
    {
        mi->flags |= MENU_ITEM_TOGGLED;
        mi->icon.index = MENU_SYSTEM_TOGGLE_ON;
    }
    else
    {
        mi->flags &= ~MENU_ITEM_TOGGLED;
        mi->icon.index = MENU_SYSTEM_TOGGLE_OFF;
    }
}


/*
 * Create check-able menu items with icons.
 */
struct menu_item_t *menu_new_checked_item(struct menu_item_t *parent, 
                                          char *title)
{
    // try to load system icons (if not loaded yet)
    // we need them for the checkbox icon
    load_menu_icons();

    return alloc_menu_item(parent, title, TYPE_CHECKED_MENUITEM,
                           MENU_ICON_NONE, NULL, 0);
}


void menu_item_set_checked(struct menu_item_t *mi, int checked)
{
    if(!mi || mi->type != TYPE_CHECKED_MENUITEM)
    {
        return;
    }
    
    if(checked)
    {
        mi->flags |= MENU_ITEM_CHECKED;
        mi->icon.index = MENU_SYSTEM_CHECKBOX_CHECKED;
        mi->icon.type = MENU_ICON_SYSTEM;
    }
    else
    {
        mi->flags &= ~MENU_ITEM_CHECKED;
        mi->icon.index = 0;
        mi->icon.type = MENU_ICON_NONE;
    }
}


void menu_item_set_enabled(struct menu_item_t *mi, int enabled)
{
    if(enabled)
    {
        mi->flags &= ~MENU_ITEM_DISABLED;
    }
    else
    {
        mi->flags |= MENU_ITEM_DISABLED;
    }
}


/*
 * Create menu items without icons.
 */
struct menu_item_t *menu_new_submenu(struct menu_item_t *parent, char *title)
{
    return alloc_menu_item(parent, title, TYPE_SUBMENU, 
                           MENU_ICON_NONE, NULL, 0);
}


struct menu_item_t *menu_new_item(struct menu_item_t *parent, char *title)
{
    return alloc_menu_item(parent, title, TYPE_MENUITEM, 
                           MENU_ICON_NONE, NULL, 0);
}


struct menu_item_t *mainmenu_new_item(struct window_t *window, char *title)
{
    struct menu_item_t *mmi, *tmp;
    
    if(!(mmi = alloc_menu_item(NULL, title, TYPE_MENUITEM, 
                               MENU_ICON_NONE, NULL, 0)))
    {
        return NULL;
    }

    if(!window->main_menu)
    {
        window->main_menu = mmi;
    }
    else
    {
        tmp = window->main_menu;

        while(tmp->next)
        {
            tmp = tmp->next;
        }

        tmp->next = mmi;
    }
    
    mmi->owner = window;

    return mmi;
}


/*
 * Create menu items with icons. If 'icon_filename' is NULL, 'icon_index' is
 * used as a system icon index. Otherwise, 'icon_filename' is used as the
 * external icon filename.
 *
 * NOTE: Right now, we only support icons by index.
 * TODO: Support loading icons from external files.
 */
struct menu_item_t *menu_new_icon_submenu(struct menu_item_t *parent, 
                                          char *title, char *icon_filename,
                                          uint8_t icon_index)
{
    uint8_t icon_type = icon_filename ? MENU_ICON_FILE : MENU_ICON_SYSTEM;
    
    if(icon_type != MENU_ICON_SYSTEM)
    {
        __set_errno(EINVAL);
        return NULL;
    }
    
    // try to load system icons (if not loaded yet)
    if(!load_menu_icons())
    {
        // fail back to menus without icons
        icon_type = MENU_ICON_NONE;
        icon_index = 0;
    }

    return alloc_menu_item(parent, title, TYPE_SUBMENU, 
                           icon_type, icon_filename, icon_index);
}


struct menu_item_t *menu_new_icon_item(struct menu_item_t *parent, 
                                       char *title,
                                       char *icon_filename, uint8_t icon_index)
{
    uint8_t icon_type = icon_filename ? MENU_ICON_FILE : MENU_ICON_SYSTEM;

    if(icon_type != MENU_ICON_SYSTEM)
    {
        __set_errno(EINVAL);
        return NULL;
    }

    // try to load system icons (if not loaded yet)
    if(!load_menu_icons())
    {
        // fail back to menus without icons
        icon_type = MENU_ICON_NONE;
        icon_index = 0;
    }

    return alloc_menu_item(parent, title, TYPE_MENUITEM, 
                           icon_type, icon_filename, icon_index);
}


void menu_item_set_shortcut(struct window_t *window, struct menu_item_t *mi,
                            char key, char modifiers)
{
    struct menu_shortcut_t *ms;
    
    if(!window || !mi || !key)
    {
        return;
    }
    
    if(!(ms = malloc(sizeof(struct menu_shortcut_t))))
    {
        return;
    }

    ms->shortcut_key = key;
    ms->shortcut_mod = modifiers;
    ms->mi = mi;
    
    if(window->menu_shortcuts)
    {
        ms->next = window->menu_shortcuts;
    }
    else
    {
        ms->next = NULL;
    }

    window->menu_shortcuts = ms;
    mi->shortcut = ms;
}


void set_accelerator(struct menu_item_t *mi)
{
    char *s;

    // get the accelerator key (if any)
    for(s = mi->title; *s; s++)
    {
        if(s[0] == '&' && s[1] != '\0')
        {
            mi->accelerator = s[1];
            
            // ensure the accelerator is lowercase so that we can compare it to
            // key presses in menu_handle_accelerator() below
            if(isupper((int)(mi->accelerator) & 0xff))
            {
                mi->accelerator = tolower(mi->accelerator);
            }

            break;
        }
    }
    
    if(!mi->accelerator)
    {
        mi->w += GLOB.mono.charw;
    }
}

void finalize_submenu(struct menu_item_t *first_child, uint8_t menu_id)
{
    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    struct menu_item_t *mi;
    int x = 0;
    
    mi = first_child;

    while(mi)
    {
        mi->w = string_width(font, mi->title) + 8;
        set_accelerator(mi);

        if(menu_id != 0)    // submenus
        {
            mi->x = 0;
            mi->w += MENU_TEXT_LEFT_MARGIN;
        }
        else                // main menu
        {
            mi->x = x;
            x = mi->x + mi->w;
        }

        mi = mi->next;
    }

    // now process submenus
    for(mi = first_child; mi != NULL; mi = mi->next)
    {
        if(!mi->first_child)
        {
            continue;
        }
        
        finalize_submenu(mi->first_child, mi->id);
    }
}


void finalize_menus(struct window_t *window)
{
    if(!window->main_menu)
    {
        return;
    }
    
    finalize_submenu(window->main_menu, 0);
}


struct menu_item_t *get_menu_item_by_accelerator(struct menu_item_t *first,
                                                 char accelerator)
{
    struct menu_item_t *mi;

    for(mi = first->first_child; mi != NULL; mi = mi->next)
    {
        if(mi->accelerator == accelerator)
        {
            return mi;
        }
    }
    
    return NULL;
}


void draw_mainmenu(struct window_t *window)
{
    struct menu_item_t *mi;
    int x = 0;

    gc_fill_rect(window->gc, 0, 0, window->w, MENU_HEIGHT, MENU_BGCOLOR);

    if(!window->main_menu)
    {
        return;
    }

    for(mi = window->main_menu; mi != NULL; mi = mi->next)
    {
        gc_draw_text(window->gc,
                             mi->title, x + 4, 2,
                             MENU_TEXTCOLOR, mi->accelerator);
        x += mi->w;
    }
}


static char *long_modifier_name(char modifiers)
{
    modifiers &= (MODIFIER_MASK_CTRL|MODIFIER_MASK_SHIFT|MODIFIER_MASK_ALT);
    
    if(modifiers == (MODIFIER_MASK_CTRL|MODIFIER_MASK_SHIFT|MODIFIER_MASK_ALT))
    {
        return "Ctrl+Alt+Shift";
    }

    if(modifiers == (MODIFIER_MASK_CTRL|MODIFIER_MASK_SHIFT))
    {
        return "Ctrl+Shift";
    }

    if(modifiers == (MODIFIER_MASK_CTRL|MODIFIER_MASK_ALT))
    {
        return "Ctrl+Alt";
    }

    if(modifiers == (MODIFIER_MASK_SHIFT|MODIFIER_MASK_ALT))
    {
        return "Alt+Shift";
    }

    if(modifiers == (MODIFIER_MASK_CTRL))
    {
        return "Ctrl";
    }

    if(modifiers == (MODIFIER_MASK_ALT))
    {
        return "Alt";
    }

    if(modifiers == (MODIFIER_MASK_SHIFT))
    {
        return "Shift";
    }
    
    // should not reach here
    return "";
}


void draw_menuitem_to_canvas(struct gc_t *gc, struct menu_item_t *mi,
                             int y, int w, int highlighted)
{
    // draw a divider
    if(is_divider(mi))
    {
        gc_fill_rect(gc, 1, y + 1, w - 2, MENU_HEIGHT - 2, MENU_BGCOLOR);
        gc_horizontal_line(gc, 4, y + (MENU_HEIGHT / 2),
                               w - 8, 
                               GLOB.themecolor[THEME_COLOR_WINDOW_BORDERCOLOR]);
    }
    else
    {
        int disabled = is_disabled(mi);
        
        // draw the background
        gc_fill_rect(gc, 1, y + 1, w - 2, MENU_HEIGHT - 2,
                     disabled ? MENU_BGCOLOR :
                                highlighted ? MENU_MOUSEOVER_BGCOLOR :
                                              MENU_BGCOLOR);

        // draw the icon (optional)
        if(menu_icon_count != 0 &&
           mi->icon.type == MENU_ICON_SYSTEM &&
           mi->icon.index < menu_icon_count)
        {
            gc_blit_bitmap(gc, &menu_icons, MENU_LEFT_MARGIN, y + 2,
                               mi->icon.index * MENU_ICON_WIDTH, 0,
                               MENU_ICON_WIDTH, MENU_ICON_HEIGHT);
        }

        // draw the menu item's title
        gc_draw_text(gc, mi->title, MENU_TEXT_LEFT_MARGIN, y + 2,
                                 disabled ? MENU_DISABLED_TEXTCOLOR :
                                    highlighted ? MENU_MOUSEOVER_TEXTCOLOR : 
                                                  MENU_TEXTCOLOR,
                                 mi->accelerator);

        // draw the shortcut (optional)
        if(mi->shortcut && mi->shortcut->shortcut_key)
        {
            char buf[32];
            int tw;
            
            if(mi->shortcut->shortcut_mod)
            {
                sprintf(buf, "%s+%s",
                             long_modifier_name(mi->shortcut->shortcut_mod),
                             get_long_key_name(mi->shortcut->shortcut_key));
            }
            else
            {
                sprintf(buf, "%s",
                             get_long_key_name(mi->shortcut->shortcut_key));
            }
            
            tw = string_width(gc->font, buf);

            gc_draw_text(gc, buf,
                             w - tw - MENU_RIGHT_MARGIN, y + 2,
                             disabled ? MENU_DISABLED_TEXTCOLOR :
                                    highlighted ? MENU_MOUSEOVER_TEXTCOLOR : 
                                                  MENU_TEXTCOLOR,
                             0);
        }
    }
}


void draw_menu_to_canvas(struct window_t *frame, int unused)
{
    (void)unused;

    struct menu_item_t *mi;
    int y = MENU_TOP_PADDING;
    int fontsz;
    
    gc_fill_rect(frame->gc, 0, 0, frame->w, frame->h, MENU_BGCOLOR);
    gc_draw_rect(frame->gc, 0, 0, frame->w, frame->h, 
                    GLOB.themecolor[THEME_COLOR_WINDOW_BORDERCOLOR]);

    if(!frame->menu_item)
    {
        return;
    }

    // ensure other drawing routines have not messed our preferred size
    lock_font(frame->gc->font);
    fontsz = gc_get_fontsize(frame->gc);
    gc_set_fontsize(frame->gc, 16);

    for(mi = frame->menu_item->first_child; mi != NULL; mi = mi->next)
    {
        mi->highlighted = 0;
        draw_menuitem_to_canvas(frame->gc, mi, y, frame->w, 0);
        y += MENU_HEIGHT;
    }

    // restore the original font size
    gc_set_fontsize(frame->gc, fontsz);
    unlock_font(frame->gc->font);
}


static inline int shortcut_name_length(struct font_t *font,
                                       struct menu_shortcut_t *shortcut)
{
    int len;
    
    if(!shortcut)
    {
        return 0;
    }
    
    len = string_width(font, get_long_key_name(shortcut->shortcut_key));
    
    if(shortcut->shortcut_mod)
    {
        len += string_width(font, "XXXXXX");
    }
    
    len += 16;
    
    return len;
}


int create_menu_frame(struct window_t *window, struct menu_item_t *menu)
{
    struct menu_item_t *mi;
    struct window_t *frame;
    struct window_attribs_t attribs;
    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    int max_w = 0, max_h = (MENU_TOP_PADDING * 2), w;
    
    if(!menu->first_child)
    {
        return 0;
    }
    
    // calculate w & h
    for(mi = menu->first_child; mi != NULL; mi = mi->next)
    {
        if((w = string_width(font, mi->title) +
                      shortcut_name_length(font, mi->shortcut) +
                           MENU_TEXT_LEFT_MARGIN + MENU_RIGHT_MARGIN) > max_w)
        {
            max_w = w;
        }

        max_h += MENU_HEIGHT;
    }

    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = menu->x;
    attribs.y = MENU_HEIGHT;
    attribs.w = max_w;
    attribs.h = max_h;
    attribs.flags = 0;      // server will set the appropriate flags

    if(!(frame = __window_create(&attribs, WINDOW_TYPE_MENU_FRAME, 
                                 window->winid)))
    {
        return 0;
    }

    frame->repaint = draw_menu_to_canvas;
    frame->owner_winid = window->winid;
    frame->menu_item = menu;
    menu->frame = frame;

    gc_set_font(frame->gc, font);
    
    return 1;
}


static void inline hide_menu(struct menu_item_t *menu)
{
    if(menu->next_displayed)
    {
        hide_menu(menu->next_displayed);
        menu->next_displayed = NULL;
    }

    simple_request(REQUEST_MENU_FRAME_HIDE, GLOB.server_winid, 
                   menu->frame->winid);
    menu->frame->flags |= WINDOW_HIDDEN;
}


static void inline window_hide_menu(struct window_t *window)
{
    if(window->displayed_menu)
    {
        hide_menu(window->displayed_menu);
        window->displayed_menu = NULL;
        window->flags &= ~WINDOW_SHOWMENU;
    }
}


void show_menu_internal(struct window_t *window, struct menu_item_t *mi)
{
    if(window->displayed_menu)
    {
        if(window->displayed_menu == mi)
        {
            return;
        }

        window_hide_menu(window);
    }

    if(!mi->frame && !create_menu_frame(window, mi))
    {
        window->flags &= ~WINDOW_SHOWMENU;
        return;
    }

    mi->next_displayed = NULL;
    window->displayed_menu = mi;
    window->flags |= WINDOW_SHOWMENU;

    draw_menu_to_canvas(mi->frame, 0);
    //window_show(mi->frame);
    window_set_pos(mi->frame, mi->x, MENU_HEIGHT);
    simple_request(REQUEST_MENU_FRAME_SHOW, GLOB.server_winid, 
                   mi->frame->winid);
    mi->frame->flags &= ~WINDOW_HIDDEN;
}


void mainmenu_mouseover(struct window_t *window, int x, int y,
                                                 mouse_buttons_t buttons)
{
    int lbutton_down = buttons & MOUSE_LBUTTON_DOWN;
    int last_lbutton_down = window->last_button_state & MOUSE_LBUTTON_DOWN;
    struct menu_item_t *mi;

    // Update the stored mouse button state to match the current state 
    window->last_button_state = buttons;

    for(mi = window->main_menu; mi != NULL; mi = mi->next)
    {
        if(!(x >= mi->x && x < (mi->x + mi->w)))
        {
            continue;
        }
        
        if(lbutton_down && !last_lbutton_down)
        {
            if(window->flags & WINDOW_SHOWMENU)
            {
                window->flags &= ~WINDOW_SHOWMENU;
            }
            else
            {
                window->flags |= WINDOW_SHOWMENU;
            }
        }
        
        if(window->flags & WINDOW_SHOWMENU)
        {
            show_menu_internal(window, mi);
        }
        else
        {
            window_hide_menu(window);
        }

        break;
    }
}


void menuframe_mouseover(struct window_t *frame, int mouse_x, int mouse_y,
                                                 mouse_buttons_t buttons)
{
    struct menu_item_t *mi;
    struct gc_t tmp_gc;
    int y = MENU_TOP_PADDING, fontsz;

    int lbutton_down = buttons & MOUSE_LBUTTON_DOWN;
    int last_lbutton_down = frame->last_button_state & MOUSE_LBUTTON_DOWN;
    uint16_t selected_item = (uint16_t)0;

    // Update the stored mouse button state to match the current state 
    frame->last_button_state = buttons;
    
    if(!frame->menu_item || !frame->menu_item->first_child)
    {
        return;
    }
    
    if(mouse_x < 0 || mouse_x > frame->w ||
       mouse_y < MENU_TOP_PADDING || mouse_y > frame->h - MENU_TOP_PADDING)
    {
        return;
    }
    
    
    tmp_gc = *frame->gc;
    
    if(frame->internal_data != NULL ||
        (frame->internal_data = malloc(frame->w * MENU_HEIGHT * 
                                            frame->gc->pixel_width)))
    {
        tmp_gc.clipping.clip_rects = NULL;
        tmp_gc.clipping.clipping_on = 0;
        tmp_gc.w = frame->w; 
        tmp_gc.h = MENU_HEIGHT;
        tmp_gc.buffer = frame->internal_data;
        tmp_gc.buffer_size = frame->w * MENU_HEIGHT * frame->gc->pixel_width;
        tmp_gc.pitch = frame->w * frame->gc->pixel_width;

        // draw temporary left border
        gc_vertical_line(&tmp_gc, 0, 0, MENU_HEIGHT, 
                            GLOB.themecolor[THEME_COLOR_WINDOW_BORDERCOLOR]);

        // draw temporary right border
        gc_vertical_line(&tmp_gc, frame->w - 1, 0, MENU_HEIGHT, 
                            GLOB.themecolor[THEME_COLOR_WINDOW_BORDERCOLOR]);
    }


    // ensure other drawing routines have not messed our preferred size
    lock_font(frame->gc->font);
    fontsz = gc_get_fontsize(frame->gc);
    gc_set_fontsize(frame->gc, 16);


    for(mi = frame->menu_item->first_child; mi != NULL; mi = mi->next)
    {
        if(/* mouse_x > 0 && mouse_x < frame->w && */
           mouse_y > y && mouse_y < y + MENU_HEIGHT)
        {
            // dividers and disabled menu items cannot be selected
            if(!is_divider(mi) && !is_disabled(mi))
            {
                selected_item = mi->id;
            }

            mi->highlighted = 1;
            draw_menuitem_to_canvas(&tmp_gc, mi, 0, frame->w, 1);
            A_memcpy(frame->gc->buffer + ((y + 1) * frame->gc->pitch),
                     tmp_gc.buffer + tmp_gc.pitch,
                     tmp_gc.buffer_size - (2 * tmp_gc.pitch));
        }
        else if(mi->highlighted)
        {
            mi->highlighted = 0;
            draw_menuitem_to_canvas(&tmp_gc, mi, 0, frame->w, 0);
            A_memcpy(frame->gc->buffer + ((y + 1) * frame->gc->pitch),
                     tmp_gc.buffer + tmp_gc.pitch,
                     tmp_gc.buffer_size - (2 * tmp_gc.pitch));
        }

        y += MENU_HEIGHT;
    }


    // restore the original font size
    gc_set_fontsize(frame->gc, fontsz);
    unlock_font(frame->gc->font);


    window_invalidate(frame);


    if(!lbutton_down && last_lbutton_down && selected_item)
    {
        struct window_t *window;
        
        // inform the owner window of the selected menu item
        hide_menu(frame->menu_item);
        
        if((window = win_for_winid(frame->owner_winid)))
        {
            window->flags &= ~WINDOW_SHOWMENU;
            window->displayed_menu = NULL;
        }
        
        send_menu_event(frame->owner_winid, frame->winid,
                        frame->menu_item->id, selected_item);
    }
}


struct menu_item_t *next_menu_item(struct menu_item_t *first_item, 
                                   struct menu_item_t *mi)
{
    struct menu_item_t *tmp;

    // is this the only item on the menu?
    if(!first_item || !first_item->next)
    {
        return mi;
    }

    for(tmp = first_item; tmp != NULL; tmp = tmp->next)
    {
        if(tmp == mi)
        {
            if(tmp->next == NULL)              // last item, return the first
            {
                return first_item;
            }

            return tmp->next;
        }
    }
    
    return NULL;
}


struct menu_item_t *prev_menu_item(struct menu_item_t *first_item, 
                                   struct menu_item_t *mi)
{
    struct menu_item_t *tmp;

    // is this the only item on the menu?
    if(!first_item || !first_item->next)
    {
        return mi;
    }

    if(mi == first_item)                    // first item, return the last
    {
        tmp = first_item;
        
        while(tmp->next)
        {
            tmp = tmp->next;
        }
        
        return tmp;
    }

    for(tmp = first_item; tmp != NULL; tmp = tmp->next)
    {
        if(!tmp->next)
        {
            break;
        }

        if(tmp->next == mi)
        {
            return tmp;
        }
    }
    
    return NULL;
}


// when changing the highlighted menu item, use the current mouse buttons so
// we don't trigger a false 'click' event
#define HIGHLIGHT(y)                                                \
{                                                                   \
    menuframe_mouseover(frame, 1, y, frame->last_button_state);     \
}


void do_select_menu(struct window_t *window, struct window_t *frame,
                                             struct menu_item_t *mi)
{
    // inform the owner window of the selected menu item
    window_hide_menu(window);
    /*
    hide_menu(frame->menu_item);
    window->flags &= ~WINDOW_SHOWMENU;
    window->displayed_menu = NULL;
    */
    send_menu_event(frame->owner_winid, frame->winid,
                    frame->menu_item->id, mi->id);
}


int menuframe_handle_accelerator(struct window_t *frame, 
                                 char modifiers, char key)
{
    struct window_t *window;
    struct menu_item_t *mi;
    char accelerator;
    int y = MENU_TOP_PADDING;

    // CTRL-key is definitely not an accelerator key
    if(modifiers & MODIFIER_MASK_CTRL)
    {
        return 0;
    }
    
    // if the focused window is a menu, accelerator keys are not pressed
    // along with ALT
    if(modifiers & MODIFIER_MASK_ALT)
    {
        return 0;
    }

    if(!frame->menu_item || !frame->menu_item->first_child)
    {
        return 0;
    }

    if(!(window = win_for_winid(frame->owner_winid)))
    {
        return 0;
    }

    // accelerator key should be ESC (to close the menu) ...
    if(key == KEYCODE_ESC)
    {
        window_hide_menu(window);
        return 1;
    }

    // ... or the Right arrow key (to show the next menu) ...
    if(key == KEYCODE_RIGHT)
    {
        if((mi = next_menu_item(window->main_menu,
                                    window->displayed_menu)) !=
                                            window->displayed_menu)
        {
            show_menu_internal(window, mi);
        }

        return 1;
    }

    // ... or the Left arrow key (to show the previous menu) ...
    if(key == KEYCODE_LEFT)
    {
        if((mi = prev_menu_item(window->main_menu,
                                    window->displayed_menu)) !=
                                        window->displayed_menu)
        {
            show_menu_internal(window, mi);
        }

        return 1;
    }

    // ... or the Up arrow key (to highlight one item up) ...
    if(key == KEYCODE_UP)
    {
        for(mi = frame->menu_item->first_child; mi != NULL; mi = mi->next)
        {
            if(mi->highlighted)
            {
                if(y == MENU_TOP_PADDING)
                {
                    // top item is selected, select the bottom item in the menu
                    HIGHLIGHT(frame->h - MENU_TOP_PADDING - 1);
                }
                else
                {
                    // some item is selected, select the item above it
                    HIGHLIGHT(y - 1);
                }

                return 1;
            }

            y += MENU_HEIGHT;
        }

        // nothing is selected, so select the bottom item in the menu
        HIGHLIGHT(frame->h - MENU_TOP_PADDING - 1);
        return 1;
    }

    // ... or the Down arrow key (to highlight one item down) ...
    if(key == KEYCODE_DOWN)
    {
        for(mi = frame->menu_item->first_child; mi != NULL; mi = mi->next)
        {
            if(mi->highlighted)
            {
                if(y + MENU_HEIGHT < frame->h - MENU_TOP_PADDING)
                {
                    // some item is selected, select the item below it
                    HIGHLIGHT(y + MENU_HEIGHT + 1);
                }
                else
                {
                    // bottom item is selected, select the top item in the menu
                    HIGHLIGHT(MENU_TOP_PADDING + 1);
                }

                return 1;
            }

            y += MENU_HEIGHT;
        }

        // nothing is selected, so select the top item in the menu
        HIGHLIGHT(MENU_TOP_PADDING + 1);
        return 1;
    }

    // ... or the Enter key (to select the highlighted item) ...
    if(key == KEYCODE_ENTER)
    {
        for(mi = frame->menu_item->first_child; mi != NULL; mi = mi->next)
        {
            if(mi->highlighted)
            {
                // dividers and disabled menu items cannot be selected
                if(!is_divider(mi) && !is_disabled(mi))
                {
                    // inform the owner window of the selected menu item
                    do_select_menu(window, frame, mi);
                }

                return 1;
            }
        }

        // nothing is selected, so select the top item in the menu
        HIGHLIGHT(MENU_TOP_PADDING + 1);
        return 1;
    }

    // ... or a printable ASCII key
    if(!is_printable_char(key) || !(accelerator = keycodes[((int)key & 0xff)]))
    {
        return 0;
    }

    if(!(mi = get_menu_item_by_accelerator(frame->menu_item, accelerator)))
    {
        return 0;
    }

    // inform the owner window of the selected menu item
    do_select_menu(window, frame, mi);
    return 1;
}


int mainmenu_handle_accelerator(struct window_t *window, 
                                char modifiers, char key)
{
    struct menu_item_t *mi;
    char accelerator;

    // if the focused window is a normal window, accelerator keys are pressed
    // along with ALT, and the window must actually have a menu!
    if(!(modifiers & MODIFIER_MASK_ALT))
    {
        return 0;
    }

    if(!window->main_menu || !window->main_menu->first_child)
    {
        return 0;
    }

    // accelerator key should be a printable ASCII key
    if(!is_printable_char(key) || !(accelerator = keycodes[((int)key & 0xff)]))
    {
        return 0;
    }

    /*
    if(!(mi = get_menu_item_by_accelerator(window->main_menu, accelerator)))
    {
        return 0;
    }
    */

    for(mi = window->main_menu; mi != NULL; mi = mi->next)
    {
        if(mi->accelerator == accelerator)
        {
            break;
        }
    }

    if(!mi)
    {
        return 0;
    }

    if(!(window->flags & WINDOW_SHOWMENU))
    {
        window->flags |= WINDOW_SHOWMENU;
        show_menu_internal(window, mi);
    }

    return 1;
}


void hide_menu2(struct menu_item_t *mi)
{
    while(mi)
    {
        struct menu_item_t *next = mi->next_displayed;

        mi->next_displayed = NULL;
        mi->frame->flags |= WINDOW_HIDDEN;
        mi = next;
    }
}


struct menu_item_t *get_menu_item(struct menu_item_t *first_item,
                                  uint16_t menu_id, uint16_t item_id)
{
    struct menu_item_t *mi, *mi2;

    for(mi = first_item; mi != NULL; mi = mi->next)
    {
        if(mi->id == menu_id)
        {
            for(mi2 = mi->first_child; mi2 != NULL; mi2 = mi2->next)
            {
                if(mi2->id == item_id)
                {
                    return mi2;
                }
            }
            
            // menu item not found under this menu
            return NULL;
        }
        
        if(mi->first_child)
        {
            if((mi2 = get_menu_item(mi->first_child, menu_id, item_id)))
            {
                return mi2;
            }
        }
    }
    
    return NULL;
}


/*
 * Events of relevance to menus:
 *   EVENT_MENU_HIDDEN      => Server tells us it hid the menu (e.g. if the 
 *                             user clicked on another window)
 *   EVENT_MOUSE            => Mouse moved or clicked on a menu item or an
 *                             open menu
 *   EVENT_KEY_PRESS        => We use this to handle accelerator keys
 *   EVENT_MENU_SELECTED    => A menu item was selected by mouse or keyboard
 */


int maybe_mainmenu_event(struct window_t *window, struct event_t *ev)
{
    //struct window_t *frame;
    struct menu_item_t *mi;

    switch(ev->type)
    {
        case EVENT_MOUSE:
            if(ev->mouse.y >= MENU_HEIGHT)
            {
                // if clicked anywhere in the window ...
                if((ev->mouse.buttons & MOUSE_LBUTTON_DOWN) &&
                   !(window->last_button_state & MOUSE_LBUTTON_DOWN))
                {
                    // ... close any open menus
                    window_hide_menu(window);
                }
                
                window_mouseover(window, ev->mouse.x, ev->mouse.y, 
                                         ev->mouse.buttons);
                return 1;
            }
            
            mainmenu_mouseover(window, ev->mouse.x, ev->mouse.y, 
                                       ev->mouse.buttons);
            return 1;

        case EVENT_KEY_PRESS:
            return mainmenu_handle_accelerator(window, ev->key.modifiers, 
                                                       ev->key.code);

        /*
        case EVENT_MENU_FRAME_HIDDEN:
            if((frame = win_for_winid(ev->src)))
            {
                if(window->displayed_menu == frame->menu_item)
                {
                    window->displayed_menu = NULL;
                    window->flags &= ~WINDOW_SHOWMENU;
                }
                
                hide_menu2(frame->menu_item);
            }
            
            return 1;
        */
        
        case EVENT_MENU_SELECTED:
            if(!window->main_menu)
            {
                return 0;
            }
            
            if((mi = get_menu_item(window->main_menu,
                                   ev->menu.menu_id, ev->menu.entry_id)))
            {
                if(mi->handler)
                {
                    mi->handler(window->winid);
                }
            }
            
            return 1;

        default:
            return 0;
    }
}


int maybe_menuframe_event(struct window_t *frame, struct event_t *ev)
{
    struct window_t *window;

    switch(ev->type)
    {
        case EVENT_MOUSE:
            menuframe_mouseover(frame, ev->mouse.x, ev->mouse.y, 
                                       ev->mouse.buttons);
            return 1;

        case EVENT_KEY_PRESS:
            return menuframe_handle_accelerator(frame, ev->key.modifiers, 
                                                       ev->key.code);

        case EVENT_WINDOW_HIDDEN:
            if((window = win_for_winid(frame->owner_winid)))
            {
                // If we lost focus to our parent, let the parent hide us.
                // If we lost focus to another window, hide ourself.
                if(get_input_focus() != window->winid)
                {
                    if(window->displayed_menu == frame->menu_item)
                    {
                        window->displayed_menu = NULL;
                        window->flags &= ~WINDOW_SHOWMENU;
                    }
                
                    hide_menu2(frame->menu_item);
                }
            }
            
            return 1;

        case EVENT_WINDOW_SHOWN:
            if((window = win_for_winid(frame->owner_winid)))
            {
                if(window->displayed_menu != frame->menu_item)
                {
                    hide_menu2(window->displayed_menu);
                }

                window->displayed_menu = frame->menu_item;
                window->flags |= WINDOW_SHOWMENU;
            }
            
            return 1;

        //case EVENT_WINDOW_LOWERED:
        case EVENT_WINDOW_LOST_FOCUS:
            if((window = win_for_winid(frame->owner_winid)))
            {
                // If we lost focus to our parent, let the parent hide us.
                // If we lost focus to another window, hide ourself.
                if(get_input_focus() != window->winid)
                {
                    window_hide_menu(window);
                }
            }
            
            return 1;

        //case EVENT_WINDOW_RAISED:
        case EVENT_MOUSE_ENTER:
        case EVENT_MOUSE_EXIT:
        case EVENT_KEY_RELEASE:
            return 1;

        default:
            return 0;
    }
}


int event_dispatch(struct event_t *ev)
{
    struct window_t *window;
    
    if(!(window = win_for_winid(ev->dest)))
    {
        return 0;
    }
    
    // dialog boxes have their own event handlers
    if(window->event_handler)
    {
        window->event_handler(ev);
        
        // assume they handled the event
        return 1;
    }
    
    // possible main menu events
    if(window->type == WINDOW_TYPE_WINDOW)
    {
        switch(ev->type)
        {
            case EVENT_WINDOW_RAISED:
            case EVENT_WINDOW_LOWERED:
            case EVENT_WINDOW_HIDDEN:
            case EVENT_WINDOW_SHOWN:
            //case EVENT_WINDOW_LOST_FOCUS:
            //case EVENT_WINDOW_GAINED_FOCUS:
            case EVENT_WINDOW_CLOSING:
                // might need to hide menus
                window_hide_menu(window);
                break;

            case EVENT_COLOR_THEME_DATA:
                // set the new color theme
                set_color_theme(ev);

                // inform all the child widgets of this window
                struct window_t *current_child;
                ListNode *current_node;

                if(window->children)
                {
                    for(current_node = window->children->root_node;
                        current_node != NULL;
                        current_node = current_node->next)
                    {
                        current_child = (struct window_t *)current_node->payload;

                        if(current_child->theme_changed)
                        {
                            current_child->theme_changed((struct window_t *)current_child);
                        }
                    }
                }

                // repaint the window itself
                window_repaint(window);
                window_invalidate(window);
                return 1;

            case EVENT_WINDOW_POS_CHANGED:
                // might need to hide menus
                window_hide_menu(window);
                window->x = ev->win.x;
                window->y = ev->win.y;
                return 1;

            case EVENT_WINDOW_RESIZE_OFFER:
                // might need to hide menus
                window_hide_menu(window);
                window_resize(window, ev->win.x, ev->win.y,
                                      ev->win.w, ev->win.h);
                return 1;

            case EVENT_MOUSE_EXIT:
                window_mouseexit(window, ev->mouse.buttons);
                return 1;

            case EVENT_KEY_PRESS:
                // see if a child widget wants to handle this key event before
                // processing global key events, e.g. menu accelerator keys
                if(window->active_child && window->active_child->keypress)
                {
                    if(window->active_child->keypress(window->active_child,
                                                      ev->key.code, 
                                                      ev->key.modifiers))
                    {
                        // child widget processed the event, we are done here
                        return 1;
                    }
                }
            
                // handle menu shortcuts, e.g. CTRL + S
                if(window->menu_shortcuts)
                {
                    struct menu_shortcut_t *ms = window->menu_shortcuts;
                
                    while(ms)
                    {
                        if(ms->shortcut_key == ev->key.code &&
                           ms->shortcut_mod == ev->key.modifiers)
                        {
                            if(!(ms->mi->flags & MENU_ITEM_DISABLED) &&
                               ms->mi && ms->mi->handler)
                            {
                                ms->mi->handler(window->winid);
                            }
                        
                            return 1;
                        }
                    
                        ms = ms->next;
                    }
                }
            
                // handle TABs
                if(ev->key.code == KEYCODE_TAB && ev->key.modifiers == 0)
                {
                    widget_next_tabstop(window);
                    return 1;
                }
            
                break;

            case EVENT_KEY_RELEASE:
                // same as above but for key release events
                if(window->active_child && window->active_child->keyrelease)
                {
                    if(window->active_child->keyrelease(window->active_child,
                                                        ev->key.code, 
                                                        ev->key.modifiers))
                    {
                        // child widget processed the event, we are done here
                        return 1;
                    }
                }

                break;
        }

        if(window->main_menu)
        {
            return maybe_mainmenu_event(window, ev);
        }

        // if there is no main menu, we will only handle mouse events
        // so that our widgets (buttons, ...) will work as the user expects
        // regardless of what the main application does
        if(ev->type == EVENT_MOUSE)
        {
            window_mouseover(window, ev->mouse.x, ev->mouse.y, 
                                     ev->mouse.buttons);
            return 1;
        }
        else
        {
            return 0;
        }
    }
    
    // menu frame events
    if(window->type == WINDOW_TYPE_MENU_FRAME)
    {
        return maybe_menuframe_event(window, ev);
    }

    return 0;
}

