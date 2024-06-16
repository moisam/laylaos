/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: server-window-mouse.c
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
 *  \file server-window-mouse.c
 *
 *  Functions to handle mouse events on server windows.
 *
 *  This code is based on the "Windowing Systems by Example" blog series.
 *  The source code it released under the MIT license and can be found at:
 *  https://github.com/JMarlin/wsbe.
 */

#define GUI_SERVER
#include <pthread.h>
#include "../include/gc.h"
#include "../include/server/window.h"
#include "../include/server/cursor.h"
#include "../include/server/event.h"
#include "../include/server/server.h"

#include "inlines.c"

int root_mouse_x, root_mouse_y;
struct mouse_state_t root_button_state;

#define MY_X        server_window_screen_x(window)
#define MY_Y        server_window_screen_y(window)


static void do_child_mouse_event(struct server_window_t *window, 
                                 struct mouse_state_t *mstate)
{
    server_window_mouseover(gc, window, mstate);

    if(!(window->flags & (WINDOW_NODECORATION | WINDOW_NORESIZE)) &&
        (window->state == WINDOW_STATE_NORMAL))
    {
        if(mstate->y < WINDOW_BORDERWIDTH)
        {
            if(mstate->x < WINDOW_BORDERWIDTH)
            {
                change_cursor(CURSOR_NWSE);
            }
            else if(mstate->x >= (window->w - WINDOW_BORDERWIDTH))
            {
                change_cursor(CURSOR_NESW);
            }
            else
            {
                change_cursor(CURSOR_NS);
            }
        }
        else if(mstate->y >= (window->h - WINDOW_BORDERWIDTH))
        {
            if(mstate->x < WINDOW_BORDERWIDTH)
            {
                change_cursor(CURSOR_NESW);
            }
            else if(mstate->x >= (window->w - WINDOW_BORDERWIDTH))
            {
                change_cursor(CURSOR_NWSE);
            }
            else
            {
                change_cursor(CURSOR_NS);
            }
        }
        else if(mstate->x < WINDOW_BORDERWIDTH)
        {
            change_cursor(CURSOR_WE);
        }
        else if(mstate->x >= (window->w - WINDOW_BORDERWIDTH))
        {
            change_cursor(CURSOR_WE);
        }
        else
        {
            change_cursor(window->cursor_id);
        }
    }
    else
    {
        change_cursor(window->cursor_id);
    }
}


#define RESIZE_DRAG                 1
#define RESIZE_NORTH                2
#define RESIZE_SOUTH                3
#define RESIZE_EAST                 4
#define RESIZE_WEST                 5
#define RESIZE_NORTH_WEST           6
#define RESIZE_NORTH_EAST           7
#define RESIZE_SOUTH_WEST           8
#define RESIZE_SOUTH_EAST           9


#define SET_DRAG_CHILD(type)                    \
{                                               \
    window->drag_off_x = mstate->x - child->x;  \
    window->drag_off_y = mstate->y - child->y;  \
    window->drag_child = child;                 \
    window->drag_type = type;                   \
    window->tracked_child = NULL;               \
    found = 1;                                  \
    break;                                      \
}


// Interface between windowing system and mouse device
void server_window_process_mouse(struct gc_t *gc, 
                                 struct server_window_t *window,
                                 struct mouse_state_t *mstate)
{
    int found = 0;
    struct server_window_t *child;
    struct server_window_t *old_mouseover_child = window->mouseover_child;
    ListNode *current_node;

    window->mouseover_child = NULL;

    if(window->drag_child)
    {
        goto skip;
    }
    
    // If we had a button depressed, then we need to see if the mouse was
    // over any of the child windows
    // We go front-to-back in terms of the window stack for free occlusion
    current_node = window->children->last_node;

    for( ; current_node != NULL; current_node = current_node->prev)
    {
        child = (struct server_window_t *)current_node->payload;

        // Don't check hidden windows
        if((child->flags & WINDOW_HIDDEN))
        {
            continue;
        }


        // If mouse isn't window bounds, we can't possibly be interacting with it 
        if(!(mstate->x >= child->x && mstate->x <= child->xw1 &&
             mstate->y >= child->y && mstate->y <= child->yh1))
        {
            continue;
        }
        
        // Now we'll check to see if we're dragging a titlebar
        if(mstate->left_pressed)
        {
            // See if the mouse position lies within the bounds of the current
            // window's 31 px tall titlebar
            // We check the decoration flag since we can't drag a window 
            // without a titlebar
            if(!(child->flags & WINDOW_NODECORATION) &&
                (child->state != WINDOW_STATE_MAXIMIZED))
            {
                int ymin = child->y + WINDOW_BORDERWIDTH;
                int xmin = child->client_x;
                int ymax = child->client_yh1 + 1;
                int xmax = child->client_xw1 + 1;

                if(mstate->y < ymin)
                {
                    if(mstate->x < xmin)
                    {
                        SET_DRAG_CHILD(RESIZE_NORTH_WEST);
                    }
                    else if(mstate->x >= xmax)
                    {
                        SET_DRAG_CHILD(RESIZE_NORTH_EAST);
                    }
                    else
                    {
                        SET_DRAG_CHILD(RESIZE_NORTH);
                    }
                }
                else if(mstate->y < (child->y + WINDOW_TITLEHEIGHT - 
                                                    WINDOW_BORDERWIDTH))
                {
                    if((child->flags & WINDOW_NOCONTROLBOX) ||
                       (mstate->x < (xmax - (CONTROL_BUTTON_LENGTH * 3))))
                    {
                        SET_DRAG_CHILD(RESIZE_DRAG);
                    }
                }
                else if(mstate->y >= ymax)
                {
                    if(mstate->x < xmin)
                    {
                        SET_DRAG_CHILD(RESIZE_SOUTH_WEST);
                    }
                    else if(mstate->x >= xmax)
                    {
                        SET_DRAG_CHILD(RESIZE_SOUTH_EAST);
                    }
                    else
                    {
                        SET_DRAG_CHILD(RESIZE_SOUTH);
                    }
                }
                else if(mstate->x < xmin)
                {
                    SET_DRAG_CHILD(RESIZE_WEST);
                }
                else if(mstate->x >= xmax)
                {
                    SET_DRAG_CHILD(RESIZE_EAST);
                }
            }

            server_window_raise(gc, child, 1);

            window->tracked_child = child;
        }
        
        window->mouseover_child = child;
            
        if(window->mouseover_child != old_mouseover_child)
        {
            send_mouse_enter_event(window->mouseover_child,
                                   mstate->x - child->x, mstate->y - child->y,
                                   mstate->buttons);
        }

        // Found a target, so forward the mouse event to that window and 
        // quit looking
        found = 1;

        struct mouse_state_t mstate2;
        mstate2.buttons = mstate->buttons;
        mstate2.left_pressed = mstate->left_pressed;
        mstate2.left_released = mstate->left_released;
        mstate2.x = mstate->x - child->x;
        mstate2.y = mstate->y - child->y;

        do_child_mouse_event(child, &mstate2);

        break;
    }

skip:

    // Moving this outside of the mouse-in-child detection since it doesn't 
    // really have anything to do with it
    if(!(mstate->buttons & MOUSE_LBUTTON_DOWN))
    {
        window->drag_child = (struct server_window_t *)0;
    }
    
    // Update drag window to match the mouse if we have an active drag window
    if(window->drag_child)
    {
        found = 1;

        if(window->drag_type == RESIZE_DRAG)
        {
            server_window_move(gc, window->drag_child,
                                   mstate->x - window->drag_off_x,
                                   mstate->y - window->drag_off_y);
        }
        else if(!(window->drag_child->flags & WINDOW_NORESIZE))
        {
            if(window->drag_type == RESIZE_NORTH)
            {
                int diffy = mstate->y - window->drag_off_y - window->drag_child->y;
                server_window_resize(gc, window->drag_child, 0, diffy, 0, -diffy, 1);
            }
            else if(window->drag_type == RESIZE_SOUTH)
            {
                int diffy = mstate->y - window->drag_child->y - window->drag_child->h;
                server_window_resize(gc, window->drag_child, 0, 0, 0, diffy, 1);
            }
            else if(window->drag_type == RESIZE_WEST)
            {
                int diffx = mstate->x - window->drag_off_x - window->drag_child->x;
                server_window_resize(gc, window->drag_child, diffx, 0, -diffx, 0, 1);
            }
            else if(window->drag_type == RESIZE_EAST)
            {
                int diffx = mstate->x - window->drag_child->x - window->drag_child->w;
                server_window_resize(gc, window->drag_child, 0, 0, diffx, 0, 1);
            }
            else if(window->drag_type == RESIZE_NORTH_EAST)
            {
                int diffy = mstate->y - window->drag_off_y - window->drag_child->y;
                int diffx = mstate->x - window->drag_child->x - window->drag_child->w;
                server_window_resize(gc, window->drag_child, 0, diffy, diffx, -diffy, 1);
            }
            else if(window->drag_type == RESIZE_NORTH_WEST)
            {
                int diffy = mstate->y - window->drag_off_y - window->drag_child->y;
                int diffx = mstate->x - window->drag_off_x - window->drag_child->x;
                server_window_resize(gc, window->drag_child, diffx, diffy, -diffx, -diffy, 1);
            }
            else if(window->drag_type == RESIZE_SOUTH_EAST)
            {
                int diffy = mstate->y - window->drag_child->y - window->drag_child->h;
                int diffx = mstate->x - window->drag_child->x - window->drag_child->w;
                server_window_resize(gc, window->drag_child, 0, 0, diffx, diffy, 1);
            }
            else if(window->drag_type == RESIZE_SOUTH_WEST)
            {
                int diffy = mstate->y - window->drag_child->y - window->drag_child->h;
                int diffx = mstate->x - window->drag_off_x - window->drag_child->x;
                server_window_resize(gc, window->drag_child, diffx, 0, -diffx, diffy, 1);
            }
        }
    }
    
    if(!found)
    {
        do_child_mouse_event(window, mstate);

        /*
         * This is done to fix a glitch where mouse events get erroneously sent
         * to a newly created child window, instead of going to the parent (desktop).
         * This happens when you press the left button, for example, and before
         * you release the button, the new window is shown, and this function
         * matches the mouse to the child window and sends a mouse-down event to it.
         */
        if(mstate->left_pressed)
        {
            window->tracked_child = window;
        }
    }
}


#define CLOSEBTN_STATE(which)                               \
{                                                           \
    window->controlbox_state |= CLOSEBUTTON_##which;        \
    window->controlbox_state &=                             \
        ~(MAXIMIZEBUTTON_##which | MINIMIZEBUTTON_##which); \
}

#define MINBTN_STATE(which)                             \
{                                                       \
    window->controlbox_state |= MINIMIZEBUTTON_##which; \
    window->controlbox_state &=                         \
        ~(CLOSEBUTTON_##which | MAXIMIZEBUTTON_##which);\
}

#define MAXBTN_STATE(which)                             \
{                                                       \
    window->controlbox_state |= MAXIMIZEBUTTON_##which; \
    window->controlbox_state &=                         \
        ~(CLOSEBUTTON_##which | MINIMIZEBUTTON_##which);\
}

#define CLEAR_STATE(which)                              \
{                                                       \
    window->controlbox_state &=                         \
        ~(CLOSEBUTTON_##which | MAXIMIZEBUTTON_##which |\
          MINIMIZEBUTTON_##which);                      \
}


void server_window_mouseover(struct gc_t *gc, struct server_window_t *window,
                                              struct mouse_state_t *mstate)
{
    int x = mstate->x;
    int y = mstate->y;

    if(!(window->flags & (WINDOW_NODECORATION | WINDOW_NOCONTROLBOX)) &&
       !window->tracking_mouse)
    {
        if(y >= 0 && y < WINDOW_TITLEHEIGHT - WINDOW_BORDERWIDTH)
        {
            int bx = window->w - WINDOW_BORDERWIDTH - CONTROL_BUTTON_LENGTH;

            if(mstate->left_pressed)
            {
                if(x >= bx)
                {
                    CLOSEBTN_STATE(DOWN);
                }
                else if(x >= (bx - CONTROL_BUTTON_LENGTH))
                {
                    if((window->flags & WINDOW_NORESIZE))
                    {
                        // window not maximizable
                        CLEAR_STATE(DOWN);
                    }
                    else
                    {
                        MAXBTN_STATE(DOWN);
                    }
                }
                else if(x >= (bx - CONTROL_BUTTON_LENGTH2))
                {
                    if((window->flags & WINDOW_NOMINIMIZE))
                    {
                        // window not minimizable
                        CLEAR_STATE(DOWN);
                    }
                    else
                    {
                        MINBTN_STATE(DOWN);
                    }
                }
                else
                {
                    CLEAR_STATE(DOWN);
                }

                // Since the button has visibly changed state, we need to 
                // invalidate the area that needs updating
                server_window_draw_controlbox(gc, window, MY_X, MY_Y,
                                                  CONTROLBOX_FLAG_CLIP | 
                                                  CONTROLBOX_FLAG_INVALIDATE);
                return;
            }

            if(mstate->left_released)
            {
                int state = window->controlbox_state;

                window->controlbox_state = 0;

                if(x >= bx)
                {
                    // close button
                    if(state & CLOSEBUTTON_DOWN)
                    {
                        server_window_close(gc, window);
                        return;
                    }
                }
                else if(x >= (bx - CONTROL_BUTTON_LENGTH))
                {
                    // max button
                    if(state & MAXIMIZEBUTTON_DOWN)
                    {
                        server_window_toggle_maximize(gc, window);
                        return;
                    }
                }
                else if(x >= (bx - CONTROL_BUTTON_LENGTH2))
                {
                    // min button
                    if(state & MINIMIZEBUTTON_DOWN)
                    {
                        server_window_toggle_minimize(gc, window);
                        return;
                    }
                }
            }

            if(x >= bx)
            {
                CLOSEBTN_STATE(OVER);
            }
            else if(x >= (bx - CONTROL_BUTTON_LENGTH))
            {
                MAXBTN_STATE(OVER);
            }
            else if(x >= (bx - CONTROL_BUTTON_LENGTH2))
            {
                MINBTN_STATE(OVER);
            }
            else
            {
                CLEAR_STATE(OVER);
            }

            // Since the button has visibly changed state, we need to 
            // invalidate the area that needs updating
            server_window_draw_controlbox(gc, window, MY_X, MY_Y,
                                              CONTROLBOX_FLAG_CLIP | 
                                              CONTROLBOX_FLAG_INVALIDATE);
            return;
        }
    }

    if(mstate->left_pressed || mstate->left_released)
    {
        window->tracking_mouse = !!(mstate->left_pressed);
    }
            
    if((window->flags & WINDOW_NODECORATION))
    {
        send_mouse_event(window, x, y, mstate->buttons);
    }
    else
    {
        reset_controlbox_state(gc, window);
        send_mouse_event(window, x - WINDOW_BORDERWIDTH, 
                         y - WINDOW_TITLEHEIGHT, mstate->buttons);
    }
}

#undef MY_X
#undef MY_Y

