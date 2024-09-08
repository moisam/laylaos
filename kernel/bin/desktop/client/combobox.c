/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: combobox.c
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
 *  \file combobox.c
 *
 *  The implementation of a combobox widget.
 */

#include <stdlib.h>
#include <string.h>
#include "../include/client/combobox.h"
#include "../include/client/inputbox.h"
#include "../include/cursor.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"
#include "../include/keys.h"
#include "../include/kbd.h"

#include "inlines.c"

#define ARROW_WIDTH                     24
#define ARROW_HEIGHT                    24

#define COMBOBOX_MIN_WIDTH              40

#define GLOB                            __global_gui_data

#define TEMPLATE_BGCOLOR                0xCDCFD4FF
#define TEMPLATE_TEXTCOLOR              0x222226FF

#define _B          TEMPLATE_BGCOLOR,
#define _T          TEMPLATE_TEXTCOLOR,
#define _L          GLOBAL_LIGHT_SIDE_COLOR,
#define _D          GLOBAL_DARK_SIDE_COLOR,

static uint32_t arrow_down_img_template[] =
{
    _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D
    _B _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _T _T _T _T _T _T _T _T _T _T _T _T _T _T _B _B _B _D _D
    _B _L _B _B _B _T _T _T _T _T _T _T _T _T _T _T _T _T _T _B _B _B _D _D
    _B _L _B _B _B _B _T _T _T _T _T _T _T _T _T _T _T _T _B _B _B _B _D _D
    _B _L _B _B _B _B _B _T _T _T _T _T _T _T _T _T _T _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _T _T _T _T _T _T _T _T _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _T _T _T _T _T _T _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _T _T _T _T _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _T _T _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D
    _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D
};

#undef _T
#define _T          GLOBAL_DARK_SIDE_COLOR,

static uint32_t arrow_down_disabled_img_template[] =
{
    _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D
    _B _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _L _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _T _T _T _T _T _T _T _T _T _T _T _T _T _T _B _B _B _D _D
    _B _L _B _B _B _T _T _T _T _T _T _T _T _T _T _T _T _T _T _B _B _B _D _D
    _B _L _B _B _B _B _T _T _T _T _T _T _T _T _T _T _T _T _B _B _B _B _D _D
    _B _L _B _B _B _B _B _T _T _T _T _T _T _T _T _T _T _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _T _T _T _T _T _T _T _T _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _T _T _T _T _T _T _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _T _T _T _T _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _T _T _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D
    _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D
};

static uint32_t arrow_down_img[ARROW_WIDTH * ARROW_HEIGHT * 4];
static uint32_t arrow_down_disabled_img[ARROW_WIDTH * ARROW_HEIGHT * 4];

#undef _B
#undef _T
#undef _D
#undef _L

static struct bitmap32_t arrow_down =
{
    .width = ARROW_WIDTH, .height = ARROW_HEIGHT, .data = arrow_down_img,
};

static struct bitmap32_t arrow_down_disabled =
{
    .width = ARROW_WIDTH, .height = ARROW_HEIGHT, .data = arrow_down_disabled_img,
};


static void listentry_click_callback(struct listview_t *listv, int selindex);
static void list_frame_dispatch_event(struct event_t *ev);


struct combobox_t *combobox_new(struct gc_t *gc, struct window_t *parent,
                                int x, int y, int w, char *title)
{
    struct window_attribs_t attribs;
    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    struct combobox_t *combobox;
    Rect *rect;

    if(!(combobox = malloc(sizeof(struct combobox_t))))
    {
        return NULL;
    }

    memset(combobox, 0, sizeof(struct combobox_t));

    if(w < COMBOBOX_MIN_WIDTH)
    {
        w = COMBOBOX_MIN_WIDTH;
    }

    if(!(combobox->window.clip_rects = RectList_new()))
    {
        free(combobox);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y, x, y + INPUTBOX_HEIGHT - 1,  x + w - 1)))
    {
        RectList_free(combobox->window.clip_rects);
        free(combobox);
        return NULL;
    }

    RectList_add(combobox->window.clip_rects, rect);
    combobox->window.type = WINDOW_TYPE_COMBOBOX;
    combobox->window.x = x;
    combobox->window.y = y;
    combobox->window.w = w;
    combobox->window.h = INPUTBOX_HEIGHT;
    combobox->window.gc = gc;
    combobox->window.flags = WINDOW_NODECORATION | WINDOW_3D_WIDGET;
    combobox->window.visible = 1;
    combobox->window.bgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    combobox->window.fgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];
    
    if(title)
    {
        __window_set_title(&combobox->window, title, 0);
    }

    combobox->window.repaint = combobox_repaint;
    combobox->window.mousedown = combobox_mousedown;
    combobox->window.mouseover = combobox_mouseover;
    combobox->window.mouseup = combobox_mouseup;
    combobox->window.mouseexit = combobox_mouseexit;
    combobox->window.unfocus = combobox_unfocus;
    combobox->window.focus = combobox_focus;
    combobox->window.destroy = combobox_destroy;
    combobox->window.size_changed = widget_size_changed;
    combobox->window.theme_changed = combobox_theme_changed;

    window_insert_child(parent, (struct window_t *)combobox);

    // create the list frame
    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = 100;
    attribs.y = 100;
    attribs.w = w;
    attribs.h = 120;
    attribs.flags = 0;      // server will set the appropriate flags

    if(!(combobox->internal_frame = __window_create(&attribs, 
                                            WINDOW_TYPE_MENU_FRAME, 
                                            parent->winid)))
    {
        RectList_free(combobox->window.clip_rects);
        free(combobox);
        return NULL;
    }

    //combobox->internal_frame->repaint = draw_menu_to_canvas;
    combobox->internal_frame->event_handler = list_frame_dispatch_event;
    combobox->internal_frame->internal_data = combobox;
    gc_set_font(combobox->internal_frame->gc, font);
    combobox->internal_list = listview_new(combobox->internal_frame->gc,
                                           combobox->internal_frame,
                                           0, 0,
                                           combobox->internal_frame->w,
                                           combobox->internal_frame->h);

    if(!combobox->internal_list)
    {
        window_destroy(combobox->internal_frame);
        RectList_free(combobox->window.clip_rects);
        free(combobox);
        return NULL;
    }

    combobox->internal_list->entry_click_callback = listentry_click_callback;
    combobox->internal_list->entry_doubleclick_callback = listentry_click_callback;

    return combobox;
}


void combobox_destroy(struct window_t *combobox_window)
{
    struct combobox_t *combobox = (struct combobox_t *)combobox_window;

    if(combobox->internal_frame)
    {
        window_destroy_children(combobox->internal_frame);
        window_destroy(combobox->internal_frame);
        combobox->internal_frame = NULL;
        combobox->internal_list = NULL;
    }

    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(combobox_window);
}


void combobox_repaint(struct window_t *combobox_window, int is_active_child)
{
    struct combobox_t *combobox = (struct combobox_t *)combobox_window;
    int x = to_child_x(combobox_window, 0);
    int y = to_child_y(combobox_window, 0);
    int charh = char_height(combobox_window->gc->font, ' ');
    int disabled = (combobox->flags & COMBOBOX_FLAG_DISABLED);
    uint32_t bgcolor = disabled ?
                    GLOB.themecolor[THEME_COLOR_INPUTBOX_DISABLED_BGCOLOR] :
                    is_active_child ? 
                    GLOB.themecolor[THEME_COLOR_INPUTBOX_SELECT_BGCOLOR] :
                    GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    uint32_t fgcolor = disabled ?
                    GLOB.themecolor[THEME_COLOR_INPUTBOX_DISABLED_TEXTCOLOR] :
                    is_active_child ?
                    GLOB.themecolor[THEME_COLOR_INPUTBOX_SELECT_TEXTCOLOR] :
                    GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];

    // white background
    gc_fill_rect(combobox_window->gc,
                 x + 2, y + 2,
                 combobox_window->w - 4, combobox_window->h - 4,
                 bgcolor);

    // draw the title
    if(combobox_window->title)
    {
        combobox_window->gc->clipping.clip_rects = combobox_window->clip_rects;
        gc_draw_text(combobox_window->gc, combobox_window->title,
                     x + 4,
                     y + ((combobox_window->h - charh) / 2),
                     fgcolor, 0);
        combobox_window->gc->clipping.clip_rects = NULL;
    }

    // draw the arrow
    gc_blit_bitmap(combobox_window->gc,
                   disabled ? &arrow_down_disabled : &arrow_down,
                   x + combobox_window->w - ARROW_WIDTH - 2, y + 2,
                   0, 0, ARROW_WIDTH, ARROW_HEIGHT);

    // border last - to ensure no text overlaps it
    draw_inverted_3d_border(combobox_window->gc, x, y, 
                            combobox_window->w, combobox_window->h);
}


void combobox_append_text(struct window_t *combobox_window, char *addstr)
{
    widget_append_text(combobox_window, addstr);
    combobox_window->repaint(combobox_window, IS_ACTIVE_CHILD(combobox_window));
    child_invalidate(combobox_window);
}


void combobox_set_text(struct window_t *combobox_window, char *new_title)
{
    __window_set_title(combobox_window, new_title, 0);
    //child_repaint(textbox_window);
    combobox_window->repaint(combobox_window, IS_ACTIVE_CHILD(combobox_window));
    child_invalidate(combobox_window);
}


void combobox_mouseover(struct window_t *combobox_window, 
                        struct mouse_state_t *mstate)
{
}


static inline void list_frame_show(struct combobox_t *combobox)
{
    combobox->internal_list->last_click_time = 0;
    combobox->internal_list->last_down = NULL;
    combobox->internal_list->last_clicked = NULL;

    window_repaint(combobox->internal_frame);

    window_show(combobox->internal_frame);
    combobox->list_shown = 1;
    combobox->show_later = 0;
}


void combobox_mousedown(struct window_t *combobox_window, 
                        struct mouse_state_t *mstate)
{
    struct combobox_t *combobox = (struct combobox_t *)combobox_window;
    struct listview_t *listv = (struct listview_t *)combobox->internal_list;

    if(combobox->flags & COMBOBOX_FLAG_DISABLED)
    {
        return;
    }

    // toggle showing/hiding the list
    if(combobox->list_shown)
    {
        window_hide(combobox->internal_frame);
        combobox->list_shown = 0;
    }
    else
    {
        int y, h;

        // only show the list if there are entries
        if(listv->entry_count == 0)
        {
            return;
        }

        // if there is space on the screen, show the frame below the combobox,
        // otherwise show it above the combobox
        if(window_screen_y(combobox_window) + 
                combobox->internal_frame->h < GLOB.screen.h)
        {
            y = combobox_window->y + INPUTBOX_HEIGHT;
        }
        else
        {
            y = combobox_window->y - combobox->internal_frame->h;
        }

        // determine the list height, to a maximum of 8 entries
        if(listv->entry_count < 8)
        {
            h = listv->entry_count * LISTVIEW_LINE_HEIGHT;
        }
        else
        {
            h = 8 * LISTVIEW_LINE_HEIGHT;
        }

        if(combobox->internal_frame->h != h)
        {
            // resize the list
            listv->window.h = h;
            listview_size_changed((struct window_t *)listv);

            // and its parent window
            window_set_size(combobox->internal_frame,
                            combobox->window.x, y,
                            combobox->window.w, h);
            combobox->show_later = 1;
        }
        else
        {
            // frame coordinates are relative to the parent window, they are
            // automatically adjusted by the server
            window_set_pos(combobox->internal_frame, combobox_window->x, y);

            list_frame_show(combobox);
        }
    }
}


void combobox_mouseexit(struct window_t *combobox_window)
{
}


void combobox_mouseup(struct window_t *combobox_window, 
                      struct mouse_state_t *mstate)
{
}


void combobox_unfocus(struct window_t *combobox_window)
{
    struct combobox_t *combobox = (struct combobox_t *)combobox_window;

    // hide the list if we lose focus
    if(combobox->list_shown)
    {
        window_hide(combobox->internal_frame);
        combobox->list_shown = 0;
    }

    combobox_window->repaint(combobox_window, IS_ACTIVE_CHILD(combobox_window));
    child_invalidate(combobox_window);
}


void combobox_focus(struct window_t *combobox_window)
{
    combobox_window->repaint(combobox_window, IS_ACTIVE_CHILD(combobox_window));
    child_invalidate(combobox_window);
}


void combobox_add_item(struct combobox_t *combobox, int index, char *str)
{
    if(!combobox->internal_list)
    {
        return;
    }

    listview_add_item(combobox->internal_list, index, str);
}


void combobox_append_item(struct combobox_t *combobox, char *str)
{
    if(!combobox->internal_list)
    {
        return;
    }

    listview_append_item(combobox->internal_list, str);
}


void combobox_remove_item(struct combobox_t *combobox, int index)
{
    if(!combobox->internal_list)
    {
        return;
    }

    listview_remove_item(combobox->internal_list, index);
}


void combobox_set_selected_item(struct combobox_t *combobox, int index)
{
    struct listview_t *listv = (struct listview_t *)combobox->internal_list;

    if(!listv)
    {
        return;
    }

    // deselect any previous selection
    if(listv->cur_entry >= 0)
    {
        listv->cur_entry = -1;
        listv->entries[listv->cur_entry].selected = 0;
    }

    if(index >= 0 && index < listv->entry_count)
    {
        listv->cur_entry = index;
        listv->entries[listv->cur_entry].selected = 1;
    }
}


static void listentry_click_callback(struct listview_t *listv, int selindex)
{
    struct window_t *window = listv->window.parent;
    struct combobox_t *combobox = (struct combobox_t *)window->internal_data;

    window_hide(window);

    if(combobox)
    {
        struct window_t *combobox_window = (struct window_t *)combobox;

        // we might be called twice, e.g. when the user double clicks on an
        // entry, which triggers both the click and double click callbacks
        // (of course, the user has to be super fast to double click on our
        // listview that we hide right after the first click, but regardless).
        if(combobox->list_shown)
        {
            combobox->list_shown = 0;

            if(selindex >= 0 && selindex < listv->entry_count)
            {
                combobox_set_text(combobox_window, 
                                    listv->entries[selindex].text);
            }

            combobox_window->repaint(combobox_window, 
                                        IS_ACTIVE_CHILD(combobox_window));
            child_invalidate(combobox_window);

            if(combobox->entry_click_callback)
            {
                combobox->entry_click_callback(combobox, selindex);
            }
        }
    }
}


static void list_frame_dispatch_event(struct event_t *ev)
{
    struct window_t *window;
    struct combobox_t *combobox;
    
    if(!(window = win_for_winid(ev->dest)))
    {
        return;
    }

    switch(ev->type)
    {
        case EVENT_WINDOW_RESIZE_OFFER:
            window_resize(window, ev->win.x, ev->win.y,
                                  ev->win.w, ev->win.h);
            combobox = (struct combobox_t *)window->internal_data;

            if(combobox->show_later)
            {
                list_frame_show(combobox);
            }

            return;

        case EVENT_WINDOW_LOST_FOCUS:
        //case EVENT_WINDOW_LOWERED:
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            combobox = (struct combobox_t *)window->internal_data;

            // Hide the list if we lose focus.
            // If we lost focus to our parent, let the parent hide us.
            // If we lost focus to another window, hide ourself.
            if(get_input_focus() != combobox->window.parent->winid)
            {
                window_hide(window);
                combobox->list_shown = 0;
            }

            return;
        
        case EVENT_MOUSE:
            window_mouseover(window, ev->mouse.x, ev->mouse.y, 
                                     ev->mouse.buttons);
            return;
        
        case EVENT_MOUSE_EXIT:
            window_mouseexit(window, ev->mouse.buttons);
            return;

        case EVENT_KEY_PRESS:
            // handle ESC
            if(ev->key.code == KEYCODE_ESC && ev->key.modifiers == 0)
            {
                // hide the list
                combobox = (struct combobox_t *)window->internal_data;
                window_hide(window);
                combobox->list_shown = 0;
                return;
            }

            // see if a child widget wants to handle this key event before
            // processing global key events, e.g. menu accelerator keys
            if(window->active_child && window->active_child->keypress)
            {
                if(window->active_child->keypress(window->active_child,
                                                  ev->key.code, 
                                                  ev->key.modifiers))
                {
                    // child widget processed the event, we are done here
                    return;
                }
            }

            return;
    }
}


void combobox_disable(struct combobox_t *combobox)
{
    struct window_t *combobox_window = (struct window_t *)combobox;

    if(combobox->list_shown)
    {
        window_hide(combobox->internal_frame);
        combobox->list_shown = 0;
    }

    if(combobox->flags & COMBOBOX_FLAG_DISABLED)
    {
        return;
    }

    combobox->flags |= COMBOBOX_FLAG_DISABLED;
    combobox_repaint(combobox_window, IS_ACTIVE_CHILD(combobox_window));
    child_invalidate(combobox_window);
}


void combobox_enable(struct combobox_t *combobox)
{
    struct window_t *combobox_window = (struct window_t *)combobox;

    if(!(combobox->flags & COMBOBOX_FLAG_DISABLED))
    {
        return;
    }

    combobox->flags &= ~COMBOBOX_FLAG_DISABLED;
    combobox_repaint(combobox_window, IS_ACTIVE_CHILD(combobox_window));
    child_invalidate(combobox_window);
}


/*
 * Called on startup and when the system color theme changes.
 * Updates the global 'down arrow' bitmap.
 */
void combobox_theme_changed_global(void)
{
    int i, j, k;

    for(i = 0, k = 0; i < ARROW_HEIGHT; i++)
    {
        for(j = 0; j < ARROW_WIDTH; j++, k++)
        {
            if(arrow_down_img_template[k] == TEMPLATE_BGCOLOR)
            {
                arrow_down_img[k] = GLOB.themecolor[THEME_COLOR_SCROLLBAR_BGCOLOR];
            }
            else if(arrow_down_img_template[k] == TEMPLATE_TEXTCOLOR)
            {
                arrow_down_img[k] = GLOB.themecolor[THEME_COLOR_SCROLLBAR_TEXTCOLOR];
            }
            else
            {
                arrow_down_img[k] = arrow_down_img_template[k];
            }

            if(arrow_down_disabled_img_template[k] == TEMPLATE_BGCOLOR)
            {
                arrow_down_disabled_img[k] = GLOB.themecolor[THEME_COLOR_SCROLLBAR_BGCOLOR];
            }
            else if(arrow_down_disabled_img_template[k] == TEMPLATE_TEXTCOLOR)
            {
                arrow_down_disabled_img[k] = GLOB.themecolor[THEME_COLOR_SCROLLBAR_TEXTCOLOR];
            }
            else
            {
                arrow_down_disabled_img[k] = arrow_down_disabled_img_template[k];
            }
        }
    }
}


/*
 * Called when the system color theme changes.
 * Updates the widget's colors.
 */
void combobox_theme_changed(struct window_t *window)
{
    window->bgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    window->fgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];
}

