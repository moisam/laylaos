/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: client-window-mouse.c
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
 *  \file client-window-mouse.c
 *
 *  Functions to handle mouse events sent to client windows.
 */

#include "../include/client/window.h"

// Get mouse x position relative to child widget, by subtracting the x offset
// of all parents except the root window. This is to ensure the child widget
// gets an x value relative to its own coordinates
static inline int rel_child_x(struct window_t *child, int x)
{
    struct window_t *parent = child->parent;
    int parentx = 0;

    while(parent->parent)
    {
        parentx += parent->x;
        parent = parent->parent;
    }

    return x - child->x - parentx;
}


// Same as above but for y
static inline int rel_child_y(struct window_t *child, int y)
{
    struct window_t *parent = child->parent;
    int parenty = 0;

    while(parent->parent)
    {
        parenty += parent->y;
        parent = parent->parent;
    }

    return y - child->y - parenty;
}


static inline void window_mousetrack(struct window_t *child, 
                                     struct mouse_state_t *mstate)
{
    mstate->x = rel_child_x(child, mstate->x);
    mstate->y = rel_child_y(child, mstate->y);

    if(mstate->left_pressed || mstate->right_pressed)
    {
        child->mousedown(child, mstate);
    }
    else if(mstate->left_released || mstate->right_released)
    {
        child->mouseup(child, mstate);
    }
    else
    {
        child->mouseover(child, mstate);
    }
}


static struct window_t *mouse_within_child(struct window_t *parent, 
                                           int x, int y)
{
    /* volatile */ struct window_t *child, *child2;
    /* volatile */ ListNode *current_node;

    for(current_node = parent->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        child = (struct window_t *)current_node->payload;

        // If mouse isn't within window bounds, we can't possibly be
        // interacting with it 
        if(!(x >= child->x && x < (child->x + child->w) &&
             y >= child->y && y < (child->y + child->h)))
        {
            continue;
        }

        if(!child->visible)
        {
            continue;
        }

        if(!child->children)
        {
            return child;
        }

        child2 = mouse_within_child(child, x - child->x, y - child->y);

        return child2 ? child2 : child;
    }

    return NULL;
}


void window_mouseover(struct window_t *window, int x, int y, 
                                               mouse_buttons_t buttons)
{
    /* volatile */ struct window_t *child = NULL;
    struct window_t *old_mouseover_child = window->mouseover_child;
    struct mouse_state_t mstate;

    int lbutton_down = buttons & MOUSE_LBUTTON_DOWN;
    int rbutton_down = buttons & MOUSE_RBUTTON_DOWN;
    int last_lbutton_down = window->last_button_state & MOUSE_LBUTTON_DOWN;
    int last_rbutton_down = window->last_button_state & MOUSE_RBUTTON_DOWN;

    mstate.buttons = buttons;
    mstate.left_pressed = (lbutton_down && !last_lbutton_down);
    mstate.left_released = (!lbutton_down && last_lbutton_down);
    mstate.right_pressed = (rbutton_down && !last_rbutton_down);
    mstate.right_released = (!rbutton_down && last_rbutton_down);
    mstate.x = x;
    mstate.y = y;
    
    if(!window->children)
    {
        goto out;
    }

    if(window->tracked_child)
    {
        window_mousetrack(window->tracked_child, &mstate);

        if(mstate.left_released || mstate.right_released)
        {
            window->tracked_child = NULL;
        }

        // Update the stored mouse button state to match the current state 
        window->last_button_state = buttons;

        return;
    }

    if((child = mouse_within_child(window, x, y)))
    {
        window->mouseover_child = child;

        if(mstate.left_pressed || mstate.right_pressed)
        {
            struct window_t *old_active_child = window->active_child;

            window->active_child = child;
            window->mousedown_child = child;

            if(old_active_child)
            {
                if(old_active_child != child)
                {
                    old_active_child->unfocus(old_active_child);
                    child->focus(child);
                }
            }
            else
            {
                child->focus(child);
            }

            window->tracked_child = child;

            //child->focus(child);
            window_mousetrack(child, &mstate);
            goto out;
        }
        
        if(child->mouseover)
        {
            //mstate.x -= child->x;
            //mstate.y -= child->y;
            mstate.x = rel_child_x(child, mstate.x);
            mstate.y = rel_child_y(child, mstate.y);

            child->mouseover(child, &mstate);
        }

        if(old_mouseover_child && old_mouseover_child != child &&
           old_mouseover_child->mouseexit)
        {
            old_mouseover_child->mouseexit(old_mouseover_child);
        }
    }

out:

    // the mouse is not overlaying any child (i.e. it is overlaying the 
    // window itself)
    if(child == NULL)
    {
        if(old_mouseover_child && old_mouseover_child->mouseexit)
        {
            old_mouseover_child->mouseexit(old_mouseover_child);
            window->mouseover_child = NULL;
        }

        // The mouse event happened in the window itself. See if it is interested
        // in these sorts of things
        if((mstate.left_pressed || mstate.right_pressed) && window->mousedown)
        {
            window->mousedown(window, &mstate);
        }
        else if((mstate.left_released || mstate.right_released) && window->mouseup)
        {
            window->mouseup(window, &mstate);
        }
        else if(window->mouseover)
        {
            window->mouseover(window, &mstate);
        }
    }

    // Update the stored mouse button state to match the current state 
    window->last_button_state = buttons;
}


void window_mouseexit(struct window_t *window, mouse_buttons_t buttons)
{
    int lbutton_down = buttons & MOUSE_LBUTTON_DOWN;

    if(window->mouseover_child && window->mouseover_child->mouseexit)
    {
        window->mouseover_child->mouseexit(window->mouseover_child);
    }
    
    window->mouseover_child = NULL;


    if(window->tracked_child && !lbutton_down)
    {
        window->tracked_child = NULL;
    }

    // Update the stored mouse button state to match the current state 
    window->last_button_state = buttons;
}

