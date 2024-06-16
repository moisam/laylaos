/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: imgbutton.c
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
 *  \file imgbutton.c
 *
 *  The implementation of an image button widget.
 */

#include <string.h>
#include <stdlib.h>
#include "../include/client/imgbutton.h"
#include "../include/client/button.h"
#include "../include/resources.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"
#include "../include/keys.h"

#include "inlines.c"

#define GLOB                    __global_gui_data

#define ICONWIDTH               64

// defined in button.c
extern struct button_color_t default_colors[BUTTON_COLOR_ARRAY_LENGTH];


struct imgbutton_t *imgbutton_new(struct gc_t *gc, struct window_t *parent,
                                                   int x, int y, int w, int h)
{
    struct imgbutton_t *button;
    Rect *rect;

    if(!(button = malloc(sizeof(struct imgbutton_t))))
    {
        return NULL;
    }

    memset(button, 0, sizeof(struct imgbutton_t));
    
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
    
    button->window.repaint = imgbutton_repaint;
    button->window.mousedown = imgbutton_mousedown;
    button->window.mouseover = imgbutton_mouseover;
    button->window.mouseup = imgbutton_mouseup;
    button->window.mouseexit = imgbutton_mouseexit;
    button->window.unfocus = imgbutton_unfocus;
    button->window.focus = imgbutton_focus;
    button->window.destroy = imgbutton_destroy;
    button->window.size_changed = widget_size_changed;
    button->window.keypress = imgbutton_keypress;

    button->state = BUTTON_STATE_NORMAL;
    button->flags |= BUTTON_FLAG_BORDERED;
    
    memcpy(button->colors, default_colors, sizeof(default_colors));

    window_insert_child(parent, (struct window_t *)button);

    return button;
}


static void free_bitmaps(struct imgbutton_t *button)
{
    // only free the bitmap if it was malloc'd
    if(button->flags & BUTTON_FLAG_BITMAP_MALLOCED)
    {
        if(button->bitmap.data)
        {
            free(button->bitmap.data);
            button->bitmap.data = NULL;
        }
    }

    // grey bitmap is always malloc'd by us
    if(button->grey_bitmap.data)
    {
        free(button->grey_bitmap.data);
        button->grey_bitmap.data = NULL;
    }
}


void imgbutton_destroy(struct window_t *button_window)
{
    free_bitmaps((struct imgbutton_t *)button_window);

    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(button_window);
}


void imgbutton_repaint(struct window_t *button_window, int is_active_child)
{
    struct imgbutton_t *button = (struct imgbutton_t *)button_window;
    uint32_t border_color, bg_color;
    int x = to_child_x(button_window, 0);
    int y = to_child_y(button_window, 0);

    bg_color = button->colors[button->state].bg;
    border_color = button->colors[button->state].border;

    gc_fill_rect(button_window->gc,
                 x + 1, y + 1,
                 button_window->w - 1, button_window->h - 1,
                 bg_color);

    struct clipping_t saved_clipping;
    struct clipping_t new_clipping = { button_window->clip_rects, 1 };
    gc_get_clipping(button_window->gc, &saved_clipping);
    gc_set_clipping(button_window->gc, &new_clipping);

    if(button->state == BUTTON_STATE_DISABLED && button->grey_bitmap.data)
    {
        // draw the greyscale image
        gc_stretch_bitmap(button_window->gc, &button->grey_bitmap,
                          x + 2, y + 2,
                          button_window->w - 4, button_window->h - 4,
                          0, 0,
                          button->grey_bitmap.width,
                          button->grey_bitmap.height);
    }
    else
    {
        // draw the normal image
        if(button->bitmap.data)
        {
            if(button->push_state)
            {
                // for pushbuttons, draw the image slightly depressed to the
                // bottom and right
                gc_stretch_bitmap(button_window->gc, &button->bitmap,
                                  x + 3, y + 3,
                                  button_window->w - 4, button_window->h - 4,
                                  0, 0,
                                  button->bitmap.width, button->bitmap.height);
            }
            else
            {
                gc_stretch_bitmap(button_window->gc, &button->bitmap,
                                  x + 2, y + 2,
                                  button_window->w - 4, button_window->h - 4,
                                  0, 0,
                                  button->bitmap.width, button->bitmap.height);
            }
        }
    }

    gc_set_clipping(button_window->gc, &saved_clipping);

    // draw the border last to avoid the image overlapping it
    if(button->flags & BUTTON_FLAG_BORDERED)
    {
        if(button->flags & BUTTON_FLAG_FLATBORDER)
        {
            gc_draw_rect(button_window->gc,
                         x, y,
                         button_window->w, button_window->h,
                         border_color);
        }
        else
        {
            if(button->push_state)
            {
                // for pushbuttons, draw the inverted 3d border if pushed
                if(is_active_child)
                {
                    // black border
                    gc_draw_rect(button_window->gc, x, y,
                                 button_window->w, button_window->h,
                                 GLOBAL_BLACK_COLOR);
                    draw_inverted_3d_border(button_window->gc, x + 1, y + 1,
                                            button_window->w - 2,
                                            button_window->h - 2);
                }
                else
                {
                    draw_inverted_3d_border(button_window->gc, x, y,
                                            button_window->w, button_window->h);
                }
            }
            else
            {
                draw_3d_border(button_window->gc, x, y,
                               button_window->w, button_window->h,
                               is_active_child);
            }
        }
    }
}


void imgbutton_mouseover(struct window_t *button_window, 
                         struct mouse_state_t *mstate)
{
    struct imgbutton_t *button = (struct imgbutton_t *)button_window;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        button->state = (mstate->buttons & MOUSE_LBUTTON_DOWN) ?
                                    BUTTON_STATE_DOWN : BUTTON_STATE_MOUSEOVER;

        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}


void imgbutton_mousedown(struct window_t *button_window, 
                         struct mouse_state_t *mstate)
{
    struct imgbutton_t *button = (struct imgbutton_t *)button_window;

    if(mstate->left_pressed && button->state != BUTTON_STATE_DISABLED)
    {
        button->state = BUTTON_STATE_DOWN;
        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}


void imgbutton_mouseexit(struct window_t *button_window)
{
    struct imgbutton_t *button = (struct imgbutton_t *)button_window;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        if(button_window->type == WINDOW_TYPE_PUSHBUTTON)
        {
            button->state = button->push_state ? BUTTON_STATE_PUSHED :
                                                 BUTTON_STATE_NORMAL;
        }
        else
        {
            button->state = BUTTON_STATE_NORMAL;
        }

        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}

void imgbutton_mouseup(struct window_t *button_window, 
                       struct mouse_state_t *mstate)
{
    struct imgbutton_t *button = (struct imgbutton_t *)button_window;

    if(mstate->left_released && button->state != BUTTON_STATE_DISABLED)
    {
        if(button_window->type == WINDOW_TYPE_PUSHBUTTON)
        {
            button->push_state = !button->push_state;
            button->state = button->push_state ? BUTTON_STATE_PUSHED :
                                                 BUTTON_STATE_MOUSEOVER;
        }
        else
        {
            button->state = BUTTON_STATE_MOUSEOVER;
        }

        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);

        //Fire the associated button click callback if it exists
        if(button->button_click_callback)
        {
            button->button_click_callback(button, mstate->x, mstate->y);
        }

        //Fire the associated push state change callback if it exists
        if(button->push_state_change_callback)
        {
            button->push_state_change_callback(button);
        }
    }
}

void imgbutton_unfocus(struct window_t *button_window)
{
    struct imgbutton_t *button = (struct imgbutton_t *)button_window;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        button->state = BUTTON_STATE_NORMAL;
        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}

void imgbutton_focus(struct window_t *button_window)
{
    struct imgbutton_t *button = (struct imgbutton_t *)button_window;

    if(button->state != BUTTON_STATE_DISABLED)
    {
        button->state = BUTTON_STATE_NORMAL;
        button_window->repaint(button_window, IS_ACTIVE_CHILD(button_window));
        child_invalidate(button_window);
    }
}


int imgbutton_keypress(struct window_t *button_window, 
                       char code, char modifiers)
{
    struct imgbutton_t *button = (struct imgbutton_t *)button_window;

    if(button->state == BUTTON_STATE_DISABLED)
    {
        return 0;
    }

    switch(code)
    {
        case KEYCODE_ENTER:
        case KEYCODE_SPACE:
            if(button_window->type == WINDOW_TYPE_PUSHBUTTON)
            {
                button->push_state = !button->push_state;
                button->state = button->push_state ? BUTTON_STATE_PUSHED :
                                                     BUTTON_STATE_MOUSEOVER;
            }
            else
            {
                button->state = BUTTON_STATE_MOUSEOVER;
            }

            button_window->repaint(button_window, 
                                   IS_ACTIVE_CHILD(button_window));
            child_invalidate(button_window);

            //Fire the associated button click event if it exists
            if(button->button_click_callback)
            {
                button->button_click_callback(button, 0, 0);
            }

            //Fire the associated push state change callback if it exists
            if(button->push_state_change_callback)
            {
                button->push_state_change_callback(button);
            }
            
            return 1;
    }
    
    return 0;
}


void imgbutton_set_sysicon(struct imgbutton_t *button, char *name)
{
    free_bitmaps(button);

    button->bitmap.width = ICONWIDTH;
    button->bitmap.height = ICONWIDTH;

    if(!(sysicon_load(name, &button->bitmap)))
    {
        button->bitmap.width = 0;
        button->bitmap.height = 0;
        button->bitmap.data = NULL;
    }
    else
    {
        struct bitmap32_t *grey;

        if((grey = image_to_greyscale(&button->bitmap)))
        {
            button->grey_bitmap.width = grey->width;
            button->grey_bitmap.height = grey->height;
            button->grey_bitmap.data = grey->data;
            // free the struct but keep the data
            free(grey);
        }

        button->flags |= BUTTON_FLAG_BITMAP_MALLOCED;
    }
}


void imgbutton_set_image(struct imgbutton_t *button, struct bitmap32_t *bitmap)
{
    free_bitmaps(button);

    if(!bitmap)
    {
        button->bitmap.width = 0;
        button->bitmap.height = 0;
        button->bitmap.data = NULL;
    }
    else
    {
        struct bitmap32_t *grey;

        button->bitmap.width = bitmap->width;
        button->bitmap.height = bitmap->height;
        button->bitmap.data = bitmap->data;

        if((grey = image_to_greyscale(&button->bitmap)))
        {
            button->grey_bitmap.width = grey->width;
            button->grey_bitmap.height = grey->height;
            button->grey_bitmap.data = grey->data;
            // free the struct but keep the data
            free(grey);
        }

        button->flags &= ~BUTTON_FLAG_BITMAP_MALLOCED;
    }
}


void imgbutton_set_bordered(struct imgbutton_t *button, int bordered)
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


void imgbutton_disable(struct imgbutton_t *button)
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


void imgbutton_enable(struct imgbutton_t *button)
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

