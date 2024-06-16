/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: button.c
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
 *  \file button.c
 *
 *  The implementation of a button widget.
 */

#include <string.h>
#include <stdlib.h>
#include "../include/client/button.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"
#include "../include/keys.h"

#define GLOB        __global_gui_data

struct button_color_t default_colors[BUTTON_COLOR_ARRAY_LENGTH] =
{
    // { background, text, border } for normal, mouse-over and pressed states
    { BUTTON_BGCOLOR, BUTTON_TEXTCOLOR, BUTTON_BORDERCOLOR },
    { BUTTON_MOUSEOVER_BGCOLOR, BUTTON_MOUSEOVER_TEXTCOLOR, 
                                BUTTON_MOUSEOVER_BORDERCOLOR },
    { BUTTON_DOWN_BGCOLOR, BUTTON_DOWN_TEXTCOLOR, BUTTON_DOWN_BORDERCOLOR },
    { BUTTON_PUSH_BGCOLOR, BUTTON_PUSH_TEXTCOLOR, BUTTON_PUSH_BORDERCOLOR },
    { BUTTON_DISABLED_BGCOLOR, BUTTON_DISABLED_TEXTCOLOR, 
                               BUTTON_DISABLED_BORDERCOLOR },
};


struct button_t *button_new(struct gc_t *gc, struct window_t *parent,
                                             int x, int y, int w, int h,
                                             char *title)
{
    struct button_t *button;
    Rect *rect;

    if(!(button = malloc(sizeof(struct button_t))))
    {
        return NULL;
    }

    memset(button, 0, sizeof(struct button_t));
    
    if(!(button->window.clip_rects = RectList_new()))
    {
        free(button);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y, x, y + h - 1,  x + w - 1)))
    {
        RectList_free(button->window.clip_rects);
        free(button);
        return NULL;
    }

    RectList_add(button->window.clip_rects, rect);
    button->window.type = WINDOW_TYPE_BUTTON;
    button->window.x = x;
    button->window.y = y;
    button->window.w = w;
    button->window.h = h;
    button->window.gc = gc;
    button->window.flags = WINDOW_NODECORATION;
    button->window.visible = 1;
    button->internal_data = 0;
    
    if(title)
    {
        __window_set_title(&button->window, title, 0);
    }

    button->window.repaint = button_repaint;
    button->window.mousedown = button_mousedown;
    button->window.mouseover = button_mouseover;
    button->window.mouseup = button_mouseup;
    button->window.mouseexit = button_mouseexit;
    button->window.unfocus = button_unfocus;
    button->window.focus = button_focus;
    button->window.destroy = button_destroy;
    button->window.size_changed = widget_size_changed;
    button->window.keypress = button_keypress;

    //button->button_click_callback = NULL;

    button->state = BUTTON_STATE_NORMAL;
    button->flags |= BUTTON_FLAG_BORDERED;
    
    memcpy(button->colors, default_colors, sizeof(default_colors));

    window_insert_child(parent, (struct window_t *)button);

    return button;
}


void button_destroy(struct window_t *button_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(button_window);
}


void button_repaint(struct window_t *button_window, int is_active_child)
{
    int title_len;
    struct button_t *button = (struct button_t *)button_window;
    uint32_t border_color, bg_color, text_color;
    int x = to_child_x(button_window, 0);
    int y = to_child_y(button_window, 0);

    bg_color = button->colors[button->state].bg;
    text_color = button->colors[button->state].text;
    border_color = button->colors[button->state].border;

    gc_fill_rect(button_window->gc,
                 x + 1, y + 1,
                 button_window->w - 1, button_window->h - 1,
                 bg_color);

    if(button->flags & BUTTON_FLAG_BORDERED)
    {
        gc_draw_rect(button_window->gc,
                     x, y,
                     button_window->w, button_window->h,
                     border_color);
    }

    if(is_active_child && button->state != BUTTON_STATE_DISABLED)
    {
        gc_draw_rect(button_window->gc,
                     x + 3, y + 3,
                     button_window->w - 6, button_window->h - 6,
                     WINDOW_TITLECOLOR);

        gc_draw_rect(button_window->gc,
                     x + 4, y + 4,
                     button_window->w - 8, button_window->h - 8,
                     WINDOW_TITLECOLOR);    
    }

    if(button_window->title)
    {
        int charh = char_height(button_window->gc->font, ' ');

        // Get the title length
        title_len = string_width(button_window->gc->font, 
                                 button_window->title);

        // Draw the title centered within the button
        struct clipping_t saved_clipping;
        struct clipping_t new_clipping = { button_window->clip_rects, 1 };
        gc_get_clipping(button_window->gc, &saved_clipping);
        gc_set_clipping(button_window->gc, &new_clipping);

        gc_draw_text(button_window->gc, button_window->title,
                                x + (button_window->w / 2) - (title_len / 2),
                                y + (button_window->h / 2) - (charh / 2),
                                text_color, 0);

        gc_set_clipping(button_window->gc, &saved_clipping);
    }
}


void button_mouseover(struct window_t *button_window, 
                      struct mouse_state_t *mstate)
{
    struct button_t *button = (struct button_t *)button_window;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        button->state = (mstate->buttons & MOUSE_LBUTTON_DOWN) ?
                                    BUTTON_STATE_DOWN : BUTTON_STATE_MOUSEOVER;

        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}


void button_mousedown(struct window_t *button_window, 
                      struct mouse_state_t *mstate)
{
    struct button_t *button = (struct button_t *)button_window;

    if(mstate->left_pressed && button->state != BUTTON_STATE_DISABLED)
    {
        button->state = BUTTON_STATE_DOWN;

        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}


void button_mouseexit(struct window_t *button_window)
{
    struct button_t *button = (struct button_t *)button_window;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        button->state = BUTTON_STATE_NORMAL;

        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}

void button_mouseup(struct window_t *button_window, 
                    struct mouse_state_t *mstate)
{
    struct button_t *button = (struct button_t *)button_window;

    if(mstate->left_released && button->state != BUTTON_STATE_DISABLED)
    {
        button->state = BUTTON_STATE_MOUSEOVER;

        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);

        // Fire the associated button click callback if it exists
        if(button->button_click_callback)
        {
            button->button_click_callback(button, mstate->x, mstate->y);
        }
    }
}

void button_unfocus(struct window_t *button_window)
{
    struct button_t *button = (struct button_t *)button_window;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        button->state = BUTTON_STATE_NORMAL;
        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}

void button_focus(struct window_t *button_window)
{
    struct button_t *button = (struct button_t *)button_window;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        button->state = BUTTON_STATE_NORMAL;
        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}


int button_keypress(struct window_t *button_window, char code, char modifiers)
{
    struct button_t *button = (struct button_t *)button_window;

    if(button->state == BUTTON_STATE_DISABLED)
    {
        return 0;
    }

    switch(code)
    {
        case KEYCODE_ENTER:
        case KEYCODE_SPACE:
            button->state = BUTTON_STATE_MOUSEOVER;
            button_window->repaint(button_window, 
                                   IS_ACTIVE_CHILD(button_window));
            child_invalidate(button_window);

            // Fire the associated button click callback if it exists
            if(button->button_click_callback)
            {
                button->button_click_callback(button, 0, 0);
            }
            
            return 1;
    }
    
    return 0;
}


void button_set_title(struct button_t *button, char *new_title)
{
    __window_set_title(&button->window, new_title, 0);
}


void button_set_bordered(struct button_t *button, int bordered)
{
    if(bordered)
    {
        button->flags |= BUTTON_FLAG_BORDERED;
    }
    else
    {
        button->flags &= ~BUTTON_FLAG_BORDERED;
    }
}


void button_disable(struct button_t *button)
{
    struct window_t *button_window = (struct window_t *)button;

    if(button->state == BUTTON_STATE_DISABLED)
    {
        return;
    }

    button->state = BUTTON_STATE_DISABLED;
    button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
    child_invalidate(button_window);
}


void button_enable(struct button_t *button)
{
    struct window_t *button_window = (struct window_t *)button;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        return;
    }

    button->state = BUTTON_STATE_NORMAL;
    button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
    child_invalidate(button_window);
}

