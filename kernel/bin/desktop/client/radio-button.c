/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: radio-button.c
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
 *  \file radio-button.c
 *
 *  The implementation of a radio button widget.
 */

#include "../include/client/radio-button.h"
#include "../include/menu.h"
#include "../include/keys.h"

#define GLOB                            __global_gui_data

#define RADIO_WIDTH                     12
#define RADIO_HEIGHT                    12

#define _B                              0xCDCFD4FF,
#define _G                              0xBABDC4FF,
#define _L                              GLOBAL_LIGHT_SIDE_COLOR,
#define _D                              GLOBAL_DARK_SIDE_COLOR,
#define _K                              GLOBAL_BLACK_COLOR,
#define __                              0x00000000,

static uint32_t radiobutton_clear_img[] =
{
    __ __ __ __ _D _D _D _D __ __ __ __
    __ __ _D _D _K _K _K _K _D _D __ __
    __ _D _K _K _L _L _L _L _K _K _L __
    __ _D _K _L _L _L _L _L _L _B _L __
    _D _K _L _L _L _L _L _L _L _L _B _L
    _D _K _L _L _L _L _L _L _L _L _B _L
    _D _K _L _L _L _L _L _L _L _L _B _L
    _D _K _L _L _L _L _L _L _L _L _B _L
    __ _D _K _L _L _L _L _L _L _B _L __
    __ _D _B _B _L _L _L _L _B _B _L __
    __ __ _L _L _B _B _B _B _L _L __ __
    __ __ __ __ _L _L _L _L __ __ __ __
};

static uint32_t radiobutton_selected_img[] =
{
    __ __ __ __ _D _D _D _D __ __ __ __
    __ __ _D _D _K _K _K _K _D _D __ __
    __ _D _K _K _L _L _L _L _K _K _L __
    __ _D _K _L _L _L _L _L _L _B _L __
    _D _K _L _L _L _K _K _L _L _L _B _L
    _D _K _L _L _K _K _K _K _L _L _B _L
    _D _K _L _L _K _K _K _K _L _L _B _L
    _D _K _L _L _L _K _K _L _L _L _B _L
    __ _D _K _L _L _L _L _L _L _B _L __
    __ _D _B _B _L _L _L _L _B _B _L __
    __ __ _L _L _B _B _B _B _L _L __ __
    __ __ __ __ _L _L _L _L __ __ __ __
};

static uint32_t radiobutton_disabled_img[] =
{
    __ __ __ __ _D _D _D _D __ __ __ __
    __ __ _D _D _K _K _K _K _D _D __ __
    __ _D _K _K _L _L _L _L _K _K _L __
    __ _D _K _L _L _L _L _L _L _B _L __
    _D _K _L _L _L _G _G _L _L _L _B _L
    _D _K _L _L _G _G _G _G _L _L _B _L
    _D _K _L _L _G _G _G _G _L _L _B _L
    _D _K _L _L _L _G _G _L _L _L _B _L
    __ _D _K _L _L _L _L _L _L _B _L __
    __ _D _B _B _L _L _L _L _B _B _L __
    __ __ _L _L _B _B _B _B _L _L __ __
    __ __ __ __ _L _L _L _L __ __ __ __
};

#define RADIO_BITMAP(name)  \
    static struct bitmap32_t name = \
    {   \
        .width = RADIO_WIDTH, .height = RADIO_HEIGHT, .data = name##_img,   \
    }

RADIO_BITMAP(radiobutton_clear);
RADIO_BITMAP(radiobutton_selected);
RADIO_BITMAP(radiobutton_disabled);

#undef _B
#undef _G
#undef _L
#undef _D
#undef _K
#undef __


struct radiobutton_t *radiobutton_new(struct gc_t *gc, 
                                      struct window_t *parent,
                                      int x, int y, int w, int h, char *title)
{
    struct radiobutton_t *button;

    if(!(button = malloc(sizeof(struct radiobutton_t))))
    {
        return NULL;
    }

    memset(button, 0, sizeof(struct radiobutton_t));

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    button->window.type = WINDOW_TYPE_RADIOBUTTON;
    button->window.x = x;
    button->window.y = y;
    button->window.w = w;
    button->window.h = h;
    button->window.gc = gc;
    button->window.visible = 1;

    button->window.repaint = radiobutton_repaint;
    button->window.mousedown = radiobutton_mousedown;
    button->window.mouseover = radiobutton_mouseover;
    button->window.mouseup = radiobutton_mouseup;
    button->window.mouseexit = radiobutton_mouseexit;
    button->window.unfocus = radiobutton_unfocus;
    button->window.focus = radiobutton_focus;
    button->window.destroy = radiobutton_destroy;
    button->window.keypress = radiobutton_keypress;
    //button->window.keyrelease = radiobutton_keyrelease;
    button->window.size_changed = widget_size_changed;
    //button->window.theme_changed = radiobutton_theme_changed;

    if(!(button->label = 
            label_new(gc, (struct window_t *)button, 
                      x + RADIO_WIDTH + 4, y, 
                      w - (RADIO_WIDTH + 4), h, title)))
    {
        free(button);
        return NULL;
    }

    button->label->window.mousedown = radiobutton_mousedown;

    window_insert_child(parent, (struct window_t *)button);

    return button;
}


void radiobutton_destroy(struct window_t *button_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(button_window);
}


void radiobutton_repaint(struct window_t *button_window, int is_active_child)
{
    struct radiobutton_t *button = (struct radiobutton_t *)button_window;
    int x = to_child_x(button_window, 0) + 2;
    int y = to_child_y(button_window, 0) + 4; // ((button_window->h - RADIO_HEIGHT) / 2);

    if(button->label)
    {
        struct window_t *label_window = (struct window_t *)button->label;
        label_repaint(label_window, IS_ACTIVE_CHILD(label_window));
    }

    if(button->state == BUTTON_STATE_DISABLED)
    {
        // draw the greyscale image
        gc_blit_bitmap(button_window->gc, &radiobutton_disabled,
                       x, y, 0, 0, RADIO_WIDTH, RADIO_HEIGHT);
    }
    else
    {
        if(button->selected)
        {
            gc_blit_bitmap(button_window->gc, &radiobutton_selected,
                           x, y, 0, 0, RADIO_WIDTH, RADIO_HEIGHT);
        }
        else
        {
            gc_blit_bitmap(button_window->gc, &radiobutton_clear,
                           x, y, 0, 0, RADIO_WIDTH, RADIO_HEIGHT);
        }
    }
}


void radiobutton_set_title(struct radiobutton_t *button, char *new_title)
{
    if(button->label)
    {
        struct window_t *label_window = (struct window_t *)button->label;
        __window_set_title(label_window, new_title, 0);
        label_window->repaint(label_window, IS_ACTIVE_CHILD(label_window));
        child_invalidate(label_window);
    }
}


void radiobutton_mouseover(struct window_t *button_window, 
                           struct mouse_state_t *mstate)
{
}


void radiobutton_mousedown(struct window_t *button_window, 
                           struct mouse_state_t *mstate)
{
    struct radiobutton_t *button;

    // check if the mouse event came from our label
    if(button_window->type == WINDOW_TYPE_LABEL)
    {
        button_window = button_window->parent;
    }

    button = (struct radiobutton_t *)button_window;

    if(button->state == BUTTON_STATE_DISABLED)
    {
        return;
    }

    radiobutton_set_selected(button);
}


void radiobutton_mouseexit(struct window_t *button_window)
{
}

void radiobutton_mouseup(struct window_t *button_window, 
                         struct mouse_state_t *mstate)
{
}

void radiobutton_unfocus(struct window_t *button_window)
{
}


void radiobutton_focus(struct window_t *button_window)
{
}


int radiobutton_keypress(struct window_t *button_window, char code, char modifiers)
{
    struct radiobutton_t *button;

    // check if the mouse event came from our label
    if(button_window->type == WINDOW_TYPE_LABEL)
    {
        button_window = button_window->parent;
    }

    button = (struct radiobutton_t *)button_window;

    if(button->state == BUTTON_STATE_DISABLED)
    {
        return 0;
    }

    switch(code)
    {
        case KEYCODE_ENTER:
        case KEYCODE_SPACE:
            button->state = BUTTON_STATE_MOUSEOVER;
            radiobutton_set_selected(button);
            return 1;
    }
    
    return 0;
}


void radiobutton_disable(struct radiobutton_t *button)
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


void radiobutton_enable(struct radiobutton_t *button)
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


void radiobutton_set_selected(struct radiobutton_t *button)
{
    struct window_t *button_window = (struct window_t *)button;
    struct radiobutton_t *button2;
    struct window_t *current_child;
    ListNode *current_node;

    // check if we are already selected
    if(button->selected)
    {
        return;
    }

    // find the selected radio button in this group
    if(button_window->parent && button_window->parent->children)
    {
        for(current_node = button_window->parent->children->root_node;
            current_node != NULL;
            current_node = current_node->next)
        {
            current_child = (struct window_t *)current_node->payload;

            if(current_child->type == WINDOW_TYPE_RADIOBUTTON)
            {
                button2 = (struct radiobutton_t *)current_child;

                if(button2->group == button->group && button2 != button &&
                   button2->selected)
                {
                    button2->selected = 0;
                    radiobutton_repaint(current_child, 0);
                    child_invalidate(current_child);
                    break;
                }
            }
        }
    }

    button->selected = 1;
    radiobutton_repaint(button_window, 1);
    child_invalidate(button_window);

    // Fire the associated button click callback if it exists
    if(button->button_click_callback)
    {
        button->button_click_callback(button, 0, 0);
    }
}

