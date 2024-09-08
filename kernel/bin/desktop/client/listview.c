/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: listview.c
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
 *  \file listview.c
 *
 *  The implementation of a listview widget.
 */

#include <string.h>
#include "../include/gui.h"
#include "../include/client/listview.h"
#include "../include/client/scrollbar.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"
#include "../include/keys.h"
#include "../include/kbd.h"
#include "../include/mouse.h"

#include "inlines.c"

#define GLOB                __global_gui_data

#define ENTRY_INDEX(entry)  (entry - &listv->entries[0])

void listview_vscroll_callback(struct window_t *parent, struct scrollbar_t *sbar);


static inline void reset_backbuf_clipping(struct listview_t *list)
{
    Rect *rect;

    // account for the border
    rect = list->backbuf_gc.clipping.clip_rects->root;
    rect->top = 2;
    rect->left = 2;
    rect->bottom = list->backbuf_gc.h - 3;
    rect->right = list->backbuf_gc.w - 3;
}


struct listview_t *listview_new(struct gc_t *gc, struct window_t *parent,
                                int x, int y, int w, int h)
{
    struct listview_t *list;
    Rect *rect;

    if(!(list = malloc(sizeof(struct listview_t))))
    {
        return NULL;
    }

    memset(list, 0, sizeof(struct listview_t));


    if(gc_alloc_backbuf(gc, &list->backbuf_gc, w, h) < 0)
    {
        free(list);
        return NULL;
    }

    gc_set_font(&list->backbuf_gc, GLOB.sysfont.data ? &GLOB.sysfont :
                                                       &GLOB.mono);

    // All subsequent drawing on the listview's canvas will be clipped to a
    // 1-pixel border. If we draw the border later (e.g. in listview_repaint())
    // we will fail, as the border will be clipped and will not be drawn.
    // A workaround would be to temporarily unclip the clipping and draw the
    // border, but this is complicated and messy. Instead, we draw the border
    // here, once and for all, and we never need to worry about it again.
    draw_inverted_3d_border(&list->backbuf_gc, 0, 0, w, h);

    reset_backbuf_clipping(list);

    if(!(list->window.clip_rects = RectList_new()))
    {
        free(list->backbuf_gc.buffer);
        free(list);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y + 1, x + 1, y + h - 2,  x + w - 2)))
    {
        RectList_free(list->window.clip_rects);
        free(list->backbuf_gc.buffer);
        free(list);
        return NULL;
    }

    RectList_add(list->window.clip_rects, rect);
    list->window.type = WINDOW_TYPE_LISTVIEW;
    list->window.x = x;
    list->window.y = y;
    list->window.w = w;
    list->window.h = h;
    list->window.gc = gc;
    list->window.flags = WINDOW_NODECORATION | WINDOW_3D_WIDGET;
    list->window.visible = 1;
    list->window.bgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    list->window.fgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];

    list->cur_entry = -1;

    list->window.repaint = listview_repaint;
    list->window.mousedown = listview_mousedown;
    list->window.mouseover = listview_mouseover;
    list->window.mouseup = listview_mouseup;
    list->window.mouseexit = listview_mouseexit;
    list->window.unfocus = listview_unfocus;
    list->window.focus = listview_focus;
    list->window.destroy = listview_destroy;
    list->window.keypress = listview_keypress;
    list->window.keyrelease = listview_keyrelease;
    list->window.size_changed = listview_size_changed;
    list->window.theme_changed = listview_theme_changed;

    if(!(list->vscroll = scrollbar_new(&list->backbuf_gc,
                                           (struct window_t *)list, 1)))
    {
        RectList_free(list->window.clip_rects);
        free(list->backbuf_gc.buffer);
        free(list);
        return NULL;
    }

    scrollbar_disable(list->vscroll);
    list->vscroll->value_change_callback = listview_vscroll_callback;
    //scrollbar_disable(selector->hscroll);

    window_insert_child(parent, (struct window_t *)list);

    return list;
}


void listview_destroy(struct window_t *listview_window)
{
    struct listview_t *listv = (struct listview_t *)listview_window;

    if(listv->entries)
    {
        listview_free_list(listv->entries, listv->entry_count);
        listv->entries = NULL;
        listv->entry_count = 0;
    }

    widget_destroy(listview_window);
}


static inline void may_draw_vscroll(struct listview_t *listv)
{
    if(!(listv->vscroll->flags & SCROLLBAR_FLAG_DISABLED))
    {
        listv->vscroll->window.repaint((struct window_t *)listv->vscroll, 0);
    }
}


static inline void paint_entry(struct window_t *listview_window,
                               struct listview_entry_t *entry,
                               int y, int w, int charh)
{
    struct listview_t *listv = (struct listview_t *)listview_window;

    gc_fill_rect(&listv->backbuf_gc, 2, y,
                 w, LISTVIEW_LINE_HEIGHT,
                 entry->selected ? 
                        GLOB.themecolor[THEME_COLOR_INPUTBOX_SELECT_BGCOLOR] :
                        listview_window->bgcolor);

    gc_draw_text(&listv->backbuf_gc, entry->text,
                 4, y + ((LISTVIEW_LINE_HEIGHT - charh) / 2),
                 entry->selected ? 
                        GLOB.themecolor[THEME_COLOR_INPUTBOX_SELECT_TEXTCOLOR] :
                        listview_window->fgcolor, 0);
}


static inline int usable_width(struct listview_t *listv)
{
    // make sure not to paint over the right side vertical scrollbar!
    return listv->window.w - (listv->vscroll->window.visible ? 20 : 4);
}


void listview_repaint(struct window_t *listview_window, int is_active_child)
{
    struct listview_entry_t *entry;
    struct listview_t *listv = (struct listview_t *)listview_window;
    int y = -listv->scrolly;
    int yend = listview_window->h;
    int charh = char_height(listview_window->gc->font, ' ');
    int w = usable_width(listv);

    //White background
    gc_fill_rect(&listv->backbuf_gc,
                 2, 2,
                 listview_window->w - 4, listview_window->h - 4,
                 listview_window->bgcolor);

    if(!listv->entries)
    {
        may_draw_vscroll(listv);
        return;
    }

    for(entry = &listv->entries[0];
        entry < &listv->entries[listv->entry_count];
        entry++)
    {
        if((y + LISTVIEW_LINE_HEIGHT) > 0)
        {
            paint_entry(listview_window, entry, y, w, charh);
        }
        
        y += LISTVIEW_LINE_HEIGHT;
        
        if(y >= yend)
        {
            break;
        }
    }

    may_draw_vscroll(listv);

    gc_blit(listview_window->gc, &listv->backbuf_gc,
            listview_window->x, listview_window->y);
}


void listview_mouseover(struct window_t *listview_window, 
                        struct mouse_state_t *mstate)
{
    int scrolly = 0;
    int old_scrolly;
    struct listview_t *listv = (struct listview_t *)listview_window;

    if(!listv->entries)
    {
        return;
    }
    
    if(mstate->buttons & MOUSE_VSCROLL_DOWN)
    {
        scrolly += LISTVIEW_LINE_HEIGHT;
    }

    if(mstate->buttons & MOUSE_VSCROLL_UP)
    {
        scrolly -= LISTVIEW_LINE_HEIGHT;
    }

    if(scrolly == 0)
    {
        return;
    }
    
    old_scrolly = listv->scrolly;
    listv->scrolly += scrolly;
    
    if((listv->vh - listv->scrolly) < listview_window->h)
    {
        listv->scrolly = listv->vh - listview_window->h;
    }
    
    if(listv->scrolly < 0)
    {
        listv->scrolly = 0;
    }
    
    if(old_scrolly == listv->scrolly)
    {
        return;
    }
    
    scrollbar_set_val(listv->vscroll, listv->scrolly);
    listview_repaint(listview_window, IS_ACTIVE_CHILD(listview_window));
    child_invalidate(listview_window);
}


static inline void repaint_visible_entries(struct window_t *listview_window,
                                           int w, int charh)
{
    struct listview_t *listv = (struct listview_t *)listview_window;
    int y = -listv->scrolly;
    int yend = listview_window->h;
    int i;

    for(i = 0; i < listv->entry_count; i++)
    {
        if((y + LISTVIEW_LINE_HEIGHT) > 0)
        {
            paint_entry(listview_window, &listv->entries[i], y, w, charh);
        }

        y += LISTVIEW_LINE_HEIGHT;

        if(y >= yend)
        {
            break;
        }
    }
}


static inline void unselect_all_except_cur(struct listview_t *listv)
{
    int i;

    for(i = 0; i < listv->entry_count; i++)
    {
        if(i != listv->cur_entry)
        {
            listv->entries[i].selected = 0;
        }
    }
}


void listview_mousedown(struct window_t *listview_window, 
                        struct mouse_state_t *mstate)
{
    struct listview_entry_t *entry;
    struct listview_t *listv = (struct listview_t *)listview_window;
    int y = -listv->scrolly;
    int yend = listview_window->h;
    int mousex = mstate->x;
    int mousey = mstate->y;
    int found = 0, scrolly = listv->scrolly;
    int ctrl_down = (listv->modifiers & MODIFIER_MASK_CTRL);
    int shift_down = (listv->modifiers & MODIFIER_MASK_SHIFT);
    int charh = char_height(listview_window->gc->font, ' ');
    int old_cur_entry = listv->cur_entry;
    int w = usable_width(listv);

    if(!listv->entries)
    {
        return;
    }

    if(!mstate->left_pressed)
    {
        return;
    }
    
    // find the entry in which the mouse was pressed
    for(entry = &listv->entries[0];
        entry < &listv->entries[listv->entry_count];
        entry++)
    {
        if(mousex >= 2 && mousex <= w + 2 &&
           mousey >= y && mousey < y + LISTVIEW_LINE_HEIGHT)
        {
            listv->last_down = entry;
            listv->cur_entry = entry - &listv->entries[0];
            entry->selected = !entry->selected;

            // scroll to selected item if needed
            if(y < 0)
            {
                // entry is partially above our upper margin
                scrolly += y;
            }
            else if(y + LISTVIEW_LINE_HEIGHT > yend)
            {
                // entry is partially below our lower margin
                scrolly += (y + LISTVIEW_LINE_HEIGHT - yend);
            }
            else
            {
                // entry is within our viewable margins
                paint_entry(listview_window, entry, y, w, charh);
            }

            found = 1;
            break;
        }

        y += LISTVIEW_LINE_HEIGHT;

        if(y >= yend)
        {
            break;
        }
    }
    
    if(!found)
    {
        listv->last_click_time = 0;
        listv->last_down = NULL;
        listv->last_clicked = NULL;
        listv->cur_entry = -1;
    }
    else
    {
        if(listv->flags & LISTVIEW_FLAG_MULTISELECT)
        {
            // multiselect using SHIFT + mouse click
            if(shift_down)
            {
                if(old_cur_entry >= 0 && old_cur_entry != listv->cur_entry)
                {
                    int a, b, i;

                    if(old_cur_entry > listv->cur_entry)
                    {
                        a = listv->cur_entry;
                        b = old_cur_entry;
                    }
                    else
                    {
                        a = old_cur_entry;
                        b = listv->cur_entry;
                    }

                    // select everything between the last selected item and
                    // the newly selected item
                    for(i = a; i <= b; i++)
                    {
                        listv->entries[i].selected = 1;
                    }

                    // and force repaint of the visible items
                    repaint_visible_entries(listview_window, w, charh);
                }
            }
            // multiselect using CTRL + mouse click is handled implicitly
            // in the loop above -- here we need to handle the case where 
            // neither SHIFT nor CTRL were pressed, where we need to deselect
            // everything apart from the currently selected item
            else if(!ctrl_down)
            {
                // deselect everything (but keep the currently selected
                // item) and force repaint of the visible items
                listv->entries[listv->cur_entry].selected = 1;
                unselect_all_except_cur(listv);
                repaint_visible_entries(listview_window, w, charh);
            }
        }
        else
        {
            listv->entries[listv->cur_entry].selected = 1;
            unselect_all_except_cur(listv);
            repaint_visible_entries(listview_window, w, charh);
        }
    }

    if(scrolly != listv->scrolly)
    {
        listv->scrolly = scrolly;
        scrollbar_set_val(listv->vscroll, listv->scrolly);
        listview_repaint(listview_window, IS_ACTIVE_CHILD(listview_window));
    }
    else
    {
        gc_blit(listview_window->gc, &listv->backbuf_gc,
                listview_window->x, listview_window->y);
    }

    child_invalidate(listview_window);
}


void listview_mouseup(struct window_t *listview_window, 
                      struct mouse_state_t *mstate)
{
    struct listview_entry_t *entry;
    struct listview_t *listv = (struct listview_t *)listview_window;
    int y = -listv->scrolly;
    int yend = listview_window->h;
    int w1 = usable_width(listv) + 2;
    int mousex = mstate->x;
    int mousey = mstate->y;
    unsigned long long click_time;
    int found = 0;

    if(!listv->entries)
    {
        return;
    }

    if(!mstate->left_released)
    {
        return;
    }
    
    click_time = time_in_millis();
    
    // find the entry in which the mouse was released
    for(entry = &listv->entries[0];
        entry < &listv->entries[listv->entry_count];
        entry++)
    {
        if(mousex >= 2 && mousex < w1 &&
           mousey >= y && mousey < y + LISTVIEW_LINE_HEIGHT)
        {
            found = 1;

            // is this a click?
            if(listv->last_down == entry)
            {
                // is this a double-click?
                if(listv->last_clicked == entry &&
                   (click_time - listv->last_click_time) < DOUBLE_CLICK_THRESHOLD)
                {
                    if(listv->entry_doubleclick_callback)
                    {
                        listv->entry_doubleclick_callback(listv, ENTRY_INDEX(entry));
                    }

                    listv->last_click_time = 0;
                    listv->last_down = NULL;
                    listv->last_clicked = NULL;
                    break;
                }

                listv->last_click_time = click_time;
                listv->last_clicked = entry;

                if(listv->entry_click_callback)
                {
                    listv->entry_click_callback(listv, ENTRY_INDEX(entry));
                }
            }
            else
            {
                listv->last_click_time = 0;
                listv->last_down = NULL;
                listv->last_clicked = NULL;
            }

            break;
        }

        y += LISTVIEW_LINE_HEIGHT;
        
        if(y >= yend)
        {
            break;
        }
    }

    if(!found)
    {
        if(listv->entry_click_callback)
        {
            listv->entry_click_callback(listv, -1);
        }
    }
}


void listview_mouseexit(struct window_t *listview_window)
{
}


void listview_unfocus(struct window_t *listview_window)
{
}


void listview_focus(struct window_t *listview_window)
{
    struct listview_t *listv = (struct listview_t *)listview_window;

    // update our view of the keyboard's modifier keys (used in multi-selection)
    listv->modifiers = get_modifier_keys();
}


static inline void unselect_all(struct listview_t *listv)
{
    struct listview_entry_t *entry;

    for(entry = &listv->entries[0];
        entry < &listv->entries[listv->entry_count];
        entry++)
    {
        entry->selected = 0;
    }
}


static void scroll_to_cur(struct window_t *listview_window)
{
    struct listview_t *listv = (struct listview_t *)listview_window;
    struct listview_entry_t *entry;
    struct listview_entry_t *cur_entry = &listv->entries[listv->cur_entry];
    int y = 0;

    if(!listv->entries || listv->cur_entry < 0)
    {
        return;
    }
    
    // find the entry in which the mouse was pressed
    for(entry = &listv->entries[0];
        entry < &listv->entries[listv->entry_count];
        entry++)
    {
        if(cur_entry == entry)
        {
            if(y < listv->scrolly)
            {
                listv->scrolly = y;
                break;
            }

            if(y + LISTVIEW_LINE_HEIGHT >= listv->scrolly + listview_window->h)
            {
                listv->scrolly = (y + LISTVIEW_LINE_HEIGHT) - listview_window->h;
                break;
            }
        }

        y += LISTVIEW_LINE_HEIGHT;
    }
}


static inline int entries_per_page(struct listview_t *listv)
{
    int i = listv->window.h / LISTVIEW_LINE_HEIGHT;

    return i ? i : 1;
}


#define SCROLL_AND_REPAINT(listview_window) \
    scroll_to_cur(listview_window); \
    scrollbar_set_val(listv->vscroll, listv->scrolly); \
    listview_repaint(listview_window, IS_ACTIVE_CHILD(listview_window)); \
    child_invalidate(listview_window);

#define SCROLL_REPAINT_AND_NOTIFTY(lv, lvw) \
    SCROLL_AND_REPAINT(lvw);    \
    if((lv)->selection_change_callback) \
        (lv)->selection_change_callback(lv);


int listview_keypress(struct window_t *listview_window, 
                      char key, char modifiers)
{
    struct listview_t *listv = (struct listview_t *)listview_window;
    int multiselect = (listv->flags & LISTVIEW_FLAG_MULTISELECT);

    // Don't handle ALT-key combinations, as these are usually menu shortcuts.
    if(modifiers & MODIFIER_MASK_ALT)
    {
        return 0;
    }

    switch(key)
    {
        case KEYCODE_LCTRL:
        case KEYCODE_RCTRL:
            listv->modifiers |= MODIFIER_MASK_CTRL;
            return 1;

        case KEYCODE_LSHIFT:
        case KEYCODE_RSHIFT:
            listv->modifiers |= MODIFIER_MASK_SHIFT;
            return 1;

        case KEYCODE_LALT:
        case KEYCODE_RALT:
            listv->modifiers |= MODIFIER_MASK_ALT;
            return 1;

        case KEYCODE_HOME:
            if(listv->cur_entry <= 0 || !listv->entries)
            {
                return 1;
            }
            
            // multi-selection if SHIFT is pressed
            if(multiselect && (listv->modifiers & MODIFIER_MASK_SHIFT))
            {
                while(listv->cur_entry > 0)
                {
                    listv->entries[--listv->cur_entry].selected = 1;
                }
            }
            // single-selection if CTRL is not pressed
            else if(!(listv->modifiers & MODIFIER_MASK_CTRL))
            {
                unselect_all(listv);
                listv->cur_entry = 0;
                listv->entries[listv->cur_entry].selected = 1;
            }

            SCROLL_REPAINT_AND_NOTIFTY(listv, listview_window);

            return 1;

        case KEYCODE_END:
            if(!listv->entries)
            {
                return 1;
            }
            
            // multi-selection if SHIFT is pressed
            if(multiselect && (listv->modifiers & MODIFIER_MASK_SHIFT))
            {
                if(listv->cur_entry < 0)
                {
                    listv->cur_entry = 0;
                    listv->entries[0].selected = 1;
                }

                while(listv->cur_entry < listv->entry_count)
                {
                    listv->entries[++listv->cur_entry].selected = 1;
                }

                //selector->cur_entry = selector->entry_count - 1;
            }
            // single-selection if CTRL is not pressed
            else if(!(listv->modifiers & MODIFIER_MASK_CTRL))
            {
                unselect_all(listv);
                listv->cur_entry = listv->entry_count - 1;
                listv->entries[listv->cur_entry].selected = 1;
            }

            SCROLL_REPAINT_AND_NOTIFTY(listv, listview_window);

            return 1;

        case KEYCODE_PGUP:
            if(listv->cur_entry <= 0 || !listv->entries)
            {
                return 1;
            }

            // multi-selection if SHIFT is pressed
            if(multiselect && (listv->modifiers & MODIFIER_MASK_SHIFT))
            {
                int last = listv->cur_entry - (entries_per_page(listv) - 1);

                if(last < 0)
                {
                     last = 0;
                }

                while(listv->cur_entry >= last)
                {
                    listv->entries[--listv->cur_entry].selected = 1;
                }
            }
            // single-selection if CTRL is not pressed
            else if(!(listv->modifiers & MODIFIER_MASK_CTRL))
            {
                unselect_all(listv);

                listv->cur_entry -= entries_per_page(listv) - 1;

                if(listv->cur_entry < 0)
                {
                    listv->cur_entry = 0;
                }

                listv->entries[listv->cur_entry].selected = 1;
            }

            SCROLL_REPAINT_AND_NOTIFTY(listv, listview_window);

            return 1;

        case KEYCODE_PGDN:
            if(!listv->entries)
            {
                return 1;
            }
            
            if(listv->cur_entry == -1)
            {
                listv->cur_entry = 0;
                listv->entries[listv->cur_entry].selected = 1;
            }
            else
            {
                if(listv->cur_entry >= listv->entry_count - 1)
                {
                    return 1;
                }

                // multi-selection if SHIFT is pressed
                if(multiselect && (listv->modifiers & MODIFIER_MASK_SHIFT))
                {
                    int last = listv->cur_entry + entries_per_page(listv);

                    if(last > listv->entry_count)
                    {
                        last = listv->entry_count;
                    }

                    while(listv->cur_entry < last)
                    {
                        listv->entries[++listv->cur_entry].selected = 1;
                    }
                }
                else
                {
                    // single-selection if CTRL is not pressed
                    if(!(listv->modifiers & MODIFIER_MASK_CTRL))
                    {
                        unselect_all(listv);

                        listv->cur_entry += entries_per_page(listv) - 1;

                        if(listv->cur_entry >= listv->entry_count)
                        {
                            listv->cur_entry = listv->entry_count - 1;
                        }

                        listv->entries[listv->cur_entry].selected = 1;
                    }
                }
            }

            SCROLL_REPAINT_AND_NOTIFTY(listv, listview_window);

            return 1;

        case KEYCODE_LEFT:
        case KEYCODE_UP:
            if(listv->cur_entry <= 0 || !listv->entries)
            {
                return 1;
            }

            // multi-selection if SHIFT is pressed
            if(multiselect && (listv->modifiers & MODIFIER_MASK_SHIFT))
            {
                listv->entries[--listv->cur_entry].selected = 1;
            }
            else
            {
                listv->cur_entry--;

                // single-selection if CTRL is not pressed
                if(!(listv->modifiers & MODIFIER_MASK_CTRL))
                {
                    unselect_all(listv);
                    listv->entries[listv->cur_entry].selected = 1;
                }
            }

            SCROLL_REPAINT_AND_NOTIFTY(listv, listview_window);

            return 1;

        case KEYCODE_RIGHT:
        case KEYCODE_DOWN:
            if(!listv->entries)
            {
                return 1;
            }

            if(listv->cur_entry == -1)
            {
                listv->cur_entry = 0;
                listv->entries[listv->cur_entry].selected = 1;
            }
            else
            {
                if(listv->cur_entry >= listv->entry_count - 1)
                {
                    return 1;
                }

                listv->cur_entry++;

                // multi-selection if SHIFT is pressed
                if(multiselect && (listv->modifiers & MODIFIER_MASK_SHIFT))
                {
                    listv->entries[listv->cur_entry].selected = 1;
                }
                else
                {
                    // single-selection if CTRL is not pressed
                    if(!(listv->modifiers & MODIFIER_MASK_CTRL))
                    {
                        unselect_all(listv);
                        listv->entries[listv->cur_entry].selected = 1;
                    }
                }
            }

            SCROLL_REPAINT_AND_NOTIFTY(listv, listview_window);

            return 1;

        case KEYCODE_SPACE:
            if(listv->cur_entry <= 0 || !listv->entries)
            {
                return 1;
            }

            // toggle multi-selection if CTRL is pressed
            if(multiselect && (listv->modifiers & MODIFIER_MASK_CTRL))
            {
                listv->entries[listv->cur_entry].selected =
                          !listv->entries[listv->cur_entry].selected;

                SCROLL_REPAINT_AND_NOTIFTY(listv, listview_window);
            }

            return 1;

        case KEYCODE_ENTER:
            // we handle ENTER with no modifier keys
            if(listv->modifiers)
            {
                return 0;
            }

            if(listv->cur_entry >= 0 &&
               listv->entry_doubleclick_callback &&
               listview_get_selected(listv, NULL) == 1)
            {
                listv->entry_doubleclick_callback(listv,
                              ENTRY_INDEX(&listv->entries[listv->cur_entry]));
            }
            
            return 1;
    }

    return 0;
}


int listview_keyrelease(struct window_t *listview_window, 
                        char key, char modifiers)
{
    struct listview_t *listv = (struct listview_t *)listview_window;

    switch(key)
    {
        case KEYCODE_LCTRL:
        case KEYCODE_RCTRL:
            listv->modifiers &= ~MODIFIER_MASK_CTRL;
            return 1;

        case KEYCODE_LSHIFT:
        case KEYCODE_RSHIFT:
            listv->modifiers &= ~MODIFIER_MASK_SHIFT;
            return 1;

        case KEYCODE_LALT:
        case KEYCODE_RALT:
            listv->modifiers &= ~MODIFIER_MASK_ALT;
            return 1;
    }

    return 0;
}


void listview_size_changed(struct window_t *window)
{
    struct listview_t *listv = (struct listview_t *)window;

    if(listv->backbuf_gc.w != window->w ||
       listv->backbuf_gc.h != window->h)
    {
        if(gc_realloc_backbuf(window->gc, &listv->backbuf_gc,
                              window->w, window->h) < 0)
        {
            /*
             * NOTE: this should be a fatal error.
             */
            return;
        }

        draw_inverted_3d_border(&listv->backbuf_gc, 0, 0, window->w, window->h);
        reset_backbuf_clipping(listv);
    }

    widget_size_changed(window);
}


void listview_vscroll_callback(struct window_t *parent, struct scrollbar_t *sbar)
{
    struct listview_t *listv = (struct listview_t *)parent;

    if(sbar->val != listv->scrolly)
    {
        listv->scrolly = sbar->val;
        listview_repaint(parent, IS_ACTIVE_CHILD(parent));
        child_invalidate(parent);
    }
}


static inline void may_need_vscroll(struct listview_t *listv)
{
    scrollbar_parent_size_changed((struct window_t *)listv,
                                      (struct window_t *)listv->vscroll);

    // we may need a vertical scrollbar
    if(listv->vh > listv->window.h)
    {
        scrollbar_set_max(listv->vscroll, listv->vh - listv->window.h);
        scrollbar_set_val(listv->vscroll, listv->scrolly);
        scrollbar_set_step(listv->vscroll, LISTVIEW_LINE_HEIGHT);
        scrollbar_enable(listv->vscroll);
        listv->vscroll->window.visible = 1;
    }
    else
    {
        scrollbar_disable(listv->vscroll);
        listv->vscroll->window.visible = 0;
    }
}


static inline void reset_vh(struct listview_t *listv)
{
    listv->vh = LISTVIEW_LINE_HEIGHT * listv->entry_count;
    listv->scrolly = 0;

    may_need_vscroll(listv);

    listv->last_click_time = 0;
    listv->last_down = NULL;
    listv->last_clicked = NULL;
}


void listview_add_item(struct listview_t *listv, int index, char *str)
{
    struct listview_entry_t *entries;
    int i;

    if(index < 0)
    {
        index = 0;
    }

    if(index > listv->entry_count)
    {
        index = listv->entry_count;
    }

    // make sure there is enough room
    if(listv->entry_count >= listv->entry_len)
    {
        entries = realloc(listv->entries,
                            (listv->entry_len + 16) * 
                                sizeof(struct listview_entry_t));

        if(!entries)
        {
            return;
        }

        listv->entry_len += 16;
        listv->entries = entries;
    }

    // make room for the new entry
    for(i = listv->entry_count; i > index; i--)
    {
        listv->entries[i].text = listv->entries[i - 1].text;
        listv->entries[i].selected = listv->entries[i - 1].selected;
    }

    listv->entries[i].text = strdup(str);
    listv->entries[i].selected = 0;
    listv->entry_count++;

    if(listv->cur_entry == index)
    {
        listv->cur_entry = -1;
    }

    reset_vh(listv);
}


void listview_append_item(struct listview_t *listv, char *str)
{
    struct listview_entry_t *entries;

    if(listv->entry_count < listv->entry_len)
    {
        listv->entries[listv->entry_count].text = strdup(str);
        listv->entries[listv->entry_count].selected = 0;
        listv->entry_count++;
    }
    else
    {
        entries = realloc(listv->entries,
                            (listv->entry_len + 16) * 
                                sizeof(struct listview_entry_t));

        if(entries)
        {
            entries[listv->entry_count].text = strdup(str);
            entries[listv->entry_count].selected = 0;
            listv->entry_count++;
            listv->entry_len += 16;
            listv->entries = entries;
        }
    }

    reset_vh(listv);
}


void listview_remove_item(struct listview_t *listv, int index)
{
    int i;

    if(index < 0 || index >= listv->entry_count)
    {
        return;
    }

    for(i = index; i < listv->entry_count - 1; i++)
    {
        listv->entries[i].text = listv->entries[i + 1].text;
        listv->entries[i].selected = listv->entries[i + 1].selected;
    }

    if(listv->entries[i].text)
    {
        free(listv->entries[i].text);
        listv->entries[i].text = NULL;
    }

    listv->entries[i].selected = 0;
    listv->entry_count--;

    if(listv->cur_entry == index)
    {
        listv->cur_entry = -1;
    }

    reset_vh(listv);
}


void listview_remove_all(struct listview_t *listv)
{
    int i;

    for(i = 0; i < listv->entry_count; i++)
    {
        if(listv->entries[i].text)
        {
            free(listv->entries[i].text);
            listv->entries[i].text = NULL;
        }

        listv->entries[i].selected = 0;
    }

    listv->entry_count = 0;
    listv->cur_entry = -1;
    reset_vh(listv);
}


void listview_clear_selection(struct listview_t *listv)
{
    int i;

    for(i = 0; i < listv->entry_count; i++)
    {
        listv->entries[i].selected = 0;
    }

    listv->cur_entry = -1;
}


void listview_free_list(struct listview_entry_t *entries, int entry_count)
{
    int i;
    
    for(i = 0; i < entry_count; i++)
    {
        if(entries[i].text)
        {
            free(entries[i].text);
        }
    }
    
    free(entries);
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
 * The returned list should be freed by calling listview_free_list().
 */
int listview_get_selected(struct listview_t *listv,
                          struct listview_entry_t **res)
{
    struct listview_entry_t *entry, *newentries;
    int i, count = 0;

    for(entry = &listv->entries[0];
        entry < &listv->entries[listv->entry_count];
        entry++)
    {
        if(entry->selected)
        {
            count++;
        }
    }
    
    if(count == 0)
    {
        return 0;
    }
    
    // only want the count
    if(res == NULL)
    {
        return count;
    }
    
    if(!(newentries = malloc(count * sizeof(struct listview_entry_t))))
    {
        return -1;
    }
    
    i = 0;

    for(entry = &listv->entries[0];
        entry < &listv->entries[listv->entry_count];
        entry++)
    {
        if(entry->selected)
        {
            A_memset(&newentries[i], 0, sizeof(struct listview_entry_t));
            newentries[i].text = strdup(entry->text);
            newentries[i].selected = 1;
            newentries[i].index = ENTRY_INDEX(entry);
            i++;
        }
    }
    
    *res = newentries;
    return count;
}


// If set is non-zero, multiple selections are allowed. Zero disables that.
void listview_set_multiselect(struct listview_t *listv, int enable)
{
    if(enable)
    {
        listv->flags |= LISTVIEW_FLAG_MULTISELECT;
    }
    else
    {
        listv->flags &= ~LISTVIEW_FLAG_MULTISELECT;
    }
}


/*
 * Called when the system color theme changes.
 * Updates the widget's colors.
 */
void listview_theme_changed(struct window_t *window)
{
    window->bgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    window->fgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];
}

