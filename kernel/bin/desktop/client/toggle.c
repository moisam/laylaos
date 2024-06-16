/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: toggle.c
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
 *  \file toggle.c
 *
 *  The implementation of a toggle widget.
 */

#include "../include/client/toggle.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"
#include "../include/keys.h"

#define GLOB                            __global_gui_data


struct toggle_t *toggle_new(struct gc_t *gc, struct window_t *parent,
                                             int x, int y)
{
    struct toggle_t *toggle;
    Rect *rect;

    if(!(toggle = malloc(sizeof(struct toggle_t))))
    {
        return NULL;
    }

    memset(toggle, 0, sizeof(struct toggle_t));

    if(!(toggle->window.clip_rects = RectList_new()))
    {
        free(toggle);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y, x, y + TOGGLE_HEIGHT - 1,  x + TOGGLE_WIDTH - 1)))
    {
        RectList_free(toggle->window.clip_rects);
        free(toggle);
        return NULL;
    }

    RectList_add(toggle->window.clip_rects, rect);
    toggle->window.type = WINDOW_TYPE_TOGGLE;
    toggle->window.x = x;
    toggle->window.y = y;
    toggle->window.w = TOGGLE_WIDTH;
    toggle->window.h = TOGGLE_HEIGHT;
    toggle->window.gc = gc;
    toggle->window.flags = WINDOW_NODECORATION;
    toggle->window.visible = 1;

    toggle->window.repaint = toggle_repaint;
    toggle->window.mousedown = toggle_mousedown;
    toggle->window.mouseover = toggle_mouseover;
    toggle->window.mouseup = toggle_mouseup;
    toggle->window.mouseexit = toggle_mouseexit;
    toggle->window.unfocus = toggle_unfocus;
    toggle->window.focus = toggle_focus;
    toggle->window.destroy = toggle_destroy;
    toggle->window.size_changed = widget_size_changed;
    toggle->window.keypress = toggle_keypress;

    toggle->toggled = 0;

    window_insert_child(parent, (struct window_t *)toggle);

    return toggle;
}


void toggle_destroy(struct window_t *toggle_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(toggle_window);
}


void toggle_repaint(struct window_t *toggle_window, int is_active_child)
{
    struct toggle_t *toggle = (struct toggle_t *)toggle_window;
    int x = to_child_x(toggle_window, 0);
    int y = to_child_y(toggle_window, 0);

    if(toggle->toggled)
    {
        gc_fill_rect(toggle_window->gc,
                     x, y,
                     toggle_window->w, toggle_window->h,
                     TOGGLE_BGCOLOR_ON);

        gc_fill_rect(toggle_window->gc,
                     x + 28, y + 2, 20, 20,
                     TOGGLE_BUTTON_COLOR);
    }
    else
    {
        gc_fill_rect(toggle_window->gc,
                     x, y,
                     toggle_window->w, toggle_window->h,
                     TOGGLE_BGCOLOR_OFF);

        gc_fill_rect(toggle_window->gc,
                     x + 2, y + 2, 20, 20,
                     TOGGLE_BUTTON_COLOR);
    }
}


void toggle_mouseover(struct window_t *toggle_window, 
                      struct mouse_state_t *mstate)
{
}


void toggle_mousedown(struct window_t *toggle_window, 
                      struct mouse_state_t *mstate)
{
    struct toggle_t *toggle = (struct toggle_t *)toggle_window;

    if(mstate->left_pressed)
    {
        toggle->toggled = !(toggle->toggled);
        toggle_window->repaint(toggle_window, IS_ACTIVE_CHILD(toggle_window));
        child_invalidate(toggle_window);

        //Fire the associated button click event if it exists
        if(toggle->toggle_change_callback)
        {
            toggle->toggle_change_callback(toggle->window.parent, toggle);
        }
    }
}


void toggle_mouseexit(struct window_t *toggle_window)
{
}

void toggle_mouseup(struct window_t *toggle_window, 
                    struct mouse_state_t *mstate)
{
}

void toggle_unfocus(struct window_t *toggle_window)
{
}

void toggle_focus(struct window_t *toggle_window)
{
}


int toggle_keypress(struct window_t *toggle_window, char code, char modifiers)
{
    struct toggle_t *toggle = (struct toggle_t *)toggle_window;

    switch(code)
    {
        case KEYCODE_ENTER:
        case KEYCODE_SPACE:
            toggle->toggled = !(toggle->toggled);
            toggle_window->repaint(toggle_window, 
                                   IS_ACTIVE_CHILD(toggle_window));
            child_invalidate(toggle_window);

            //Fire the associated button click event if it exists
            if(toggle->toggle_change_callback)
            {
                toggle->toggle_change_callback(toggle->window.parent, toggle);
            }
            
            return 1;
    }
    
    return 0;
}


void toggle_set_toggled(struct toggle_t *toggle, int toggled)
{
    toggle->toggled = !!toggled;
}

