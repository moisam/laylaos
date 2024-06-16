/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: server-window.c
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
 *  \file server-window.c
 *
 *  Functions to create, destroy, and work with server-side windows.
 *  These are the the server representation of windows, which are eventually
 *  drawn on the screen. Client programs has their own implementation of
 *  windows, defined in the header file include/client/window.h.
 *
 *  This code is based on the "Windowing Systems by Example" blog series.
 *  The source code it released under the MIT license and can be found at:
 *  https://github.com/JMarlin/wsbe.
 */

#define GUI_SERVER
#include <string.h>
#include <pthread.h>
#include <sys/shm.h>
#include "../include/gc.h"
#include "../include/resources.h"
#include "../include/server/server.h"
#include "../include/server/window.h"
#include "../include/server/event.h"
#include "../include/server/rects.h"

#include "font-array-bold.h"

#include "inlines.c"

// defined in main.c
extern Rect desktop_bounds;


void server_window_draw_border(struct gc_t *gc, struct server_window_t *window)
{
    uint16_t client_w = window->client_w;
    int screen_x = window->x;
    int screen_y = window->y;
    int i, iw = 0;
    int ttop = 10;

    struct clipping_t saved_clipping;
    gc_get_clipping(gc, &saved_clipping);
    gc_set_clipping(gc, &window->clipping);

    //Draw a 3px border line under the titlebar
    gc_fill_rect(gc, screen_x + WINDOW_BORDERWIDTH,
                     screen_y + WINDOW_TITLEHEIGHT - WINDOW_BORDERWIDTH,
                     window->client_w,
                     WINDOW_BORDERWIDTH, WINDOW_BORDERCOLOR);

    if(!(window->flags & WINDOW_NOCONTROLBOX))
    {
        client_w -= (3 * CONTROL_BUTTON_LENGTH);
    }
    
    if(!(window->flags & WINDOW_NOICON))
    {
        iw = WINDOW_ICONWIDTH;
    }

    // Fill in the titlebar background
    gc_fill_rect(gc, screen_x + WINDOW_BORDERWIDTH,
                     screen_y + WINDOW_BORDERWIDTH,
                     client_w,
                     WINDOW_TITLEHEIGHT - (2 * WINDOW_BORDERWIDTH),
                     window->parent->active_child == window ? 
                                                   WINDOW_TITLECOLOR :
                                                   WINDOW_TITLECOLOR_INACTIVE);

    // Draw the window title
    if(GLOB.sysfont_bold.data)
    {
        gc->font = &GLOB.sysfont_bold;
        ttop = 6;
    }

    gc_draw_text(gc, window->title,
                     screen_x + 10 + iw, screen_y + ttop,
                     window->parent->active_child == window ? 
                                                 WINDOW_TEXTCOLOR :
                                                 WINDOW_TEXTCOLOR_INACTIVE, 0);

    gc->font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;

    if(!(window->flags & WINDOW_NOCONTROLBOX))
    {
        // Draw the controlbox
        server_window_draw_controlbox(gc, window, screen_x, screen_y, 0);
    }

    if(!(window->flags & WINDOW_NOICON) && window->icon)
    {
        i = (WINDOW_TITLEHEIGHT - WINDOW_ICONWIDTH) / 2;

        if(window->icon->type == RESOURCE_IMAGE)
        {
            struct bitmap32_t *bmp = (struct bitmap32_t *)window->icon->data;
            gc_stretch_bitmap(gc, bmp,
                                  screen_x + WINDOW_BORDERWIDTH + 4, screen_y + i,
                                  WINDOW_ICONWIDTH, WINDOW_ICONWIDTH,
                                  0, 0,
                                  bmp->width, bmp->height);
        }
        else if(window->icon->type == RESOURCE_IMAGE_ARRAY)
        {
            gc_blit_icon(gc, (struct bitmap32_array_t *)window->icon->data,
                             screen_x + WINDOW_BORDERWIDTH + 4, screen_y + i,
                             0, 0,
                             WINDOW_ICONWIDTH, WINDOW_ICONWIDTH);
        }
    }

    // Long window titles can spill into the right border, so we draw the
    // border last
    gc_fill_rect(gc, screen_x, screen_y, window->w,
                     WINDOW_BORDERWIDTH, WINDOW_BORDERCOLOR);
    gc_fill_rect(gc, screen_x, window->yh1 + 1 - WINDOW_BORDERWIDTH,
                     window->w, WINDOW_BORDERWIDTH, WINDOW_BORDERCOLOR);

    gc_fill_rect(gc, screen_x, screen_y, WINDOW_BORDERWIDTH,
                     window->h, WINDOW_BORDERCOLOR);
    gc_fill_rect(gc, window->xw1 + 1 - WINDOW_BORDERWIDTH,
                     screen_y, WINDOW_BORDERWIDTH, window->h, 
                     WINDOW_BORDERCOLOR);

    gc_set_clipping(gc, &saved_clipping);
}


// Apply clipping for window bounds without subtracting child window rects
void server_window_apply_bound_clipping(struct server_window_t *window,
                                        int in_recursion, 
                                        RectList *dirty_regions,
                                        struct clipping_t *clipping)
{
    Rect *temp_rect, *current_dirty_rect, *clone_dirty_rect;
    List clip_windows;
    struct server_window_t *clipping_window;

    if((!(window->flags & WINDOW_NODECORATION)) && in_recursion)
    {
        temp_rect = Rect_new_unlocked(window->client_y,
                                      window->client_x,
                                      window->client_yh1,
                                      window->client_xw1);
    }
    else
    {
        temp_rect = Rect_new_unlocked(window->y, window->x, 
                                      window->yh1, window->xw1);
    }

    // If there's no parent (meaning we're at the top of the window tree)
    // then we just add our rectangle and exit.
    // If we were passed a dirty region list, we first clone those dirty
    // rects into the clipping region and then intersect the top-level
    // window bounds against it so that we're limited to the dirty region
    // from the outset
    if(!window->parent)
    {
        if(dirty_regions)
        {
            // Clone the dirty regions and put them into the clipping list
            for(current_dirty_rect = dirty_regions->root;
                current_dirty_rect != NULL;
                current_dirty_rect = current_dirty_rect->next)
            {
                // Clone
                clone_dirty_rect = 
                            Rect_new_unlocked(current_dirty_rect->top,
                                              current_dirty_rect->left,
                                              current_dirty_rect->bottom,
                                              current_dirty_rect->right);
                
                // Add
                add_clip_rect_unlocked(clipping, clone_dirty_rect);
            }

            // Finally, intersect this top level window against them
            intersect_clip_rect_unlocked(clipping, temp_rect);
        }
        else
        {
            add_clip_rect_unlocked(clipping, temp_rect);
        }

        return;
    }

    // Otherwise, we first reduce our clipping area to the visibility area 
    // of our parent
    server_window_apply_bound_clipping(window->parent, 1, 
                                       dirty_regions, clipping);

    // And finally, we subtract the rectangles of any siblings that are 
    // occluding us 
    server_window_get_windows_above(window->parent, window, &clip_windows);

    // Now that we've reduced our clipping area to our parent's clipping area, 
    // we can intersect our own bounds rectangle to get our main visible area
    intersect_clip_rect_unlocked(clipping, temp_rect);

    ListNode *current_node = clip_windows.root_node;

    while(current_node)
    {
        ListNode *next_node = current_node->next;

        clipping_window = (struct server_window_t *)current_node->payload;

        Rect tmp;
        tmp.top = clipping_window->y;
        tmp.left = clipping_window->x;
        tmp.bottom = clipping_window->yh1;
        tmp.right = clipping_window->xw1;
        subtract_clip_rect_unlocked(clipping, &tmp);

        Listnode_free_unlocked(current_node); 
        current_node = next_node;
    }
}


void server_window_update_title(struct gc_t *gc, 
                                struct server_window_t *window)
{
    if((window->flags & WINDOW_NODECORATION))
    {
        return;
    }

    // Start by limiting painting to the window's visible area
    server_window_apply_bound_clipping(window, 0, 0, &window->clipping);

    // Draw border
    server_window_draw_border(gc, window);

    clear_clip_rects(&window->clipping);

    invalidate_screen_rect(window->y, window->x,
                           window->client_y - 1, window->xw1);
}


// Request a repaint of a certain region of a window
void server_window_invalidate(struct gc_t *gc,
                              struct server_window_t *window,
                              int top, int left, int bottom, int right)
{
    RectList dirty_regions;
    Rect dirty_rect;

    dirty_regions.root = &dirty_rect;
    dirty_regions.last = &dirty_rect;

    dirty_rect.top = top + window->client_y;
    dirty_rect.left = left + window->client_x;
    dirty_rect.bottom = bottom + window->client_y;
    dirty_rect.right = right + window->client_x;
    dirty_rect.next = NULL;

    server_window_paint(gc, window, &dirty_regions, 0);
}


// Another override-redirect function
void server_window_paint(struct gc_t *gc, struct server_window_t *window,
                         RectList *dirty_regions, int flags)
{
    struct server_window_t *current_child;
    Rect *temp_rect;
    ListNode *current_node;

    // Start by limiting painting to the window's visible area
    server_window_apply_bound_clipping(window, 0, dirty_regions, 
                                       &window->clipping);

    // If we have window decorations turned on, draw them and then further
    // limit the clipping area to the inner drawable area of the window 
    if(!(window->flags & WINDOW_NODECORATION))
    {
        if(flags & FLAG_PAINT_BORDER)
        {
            server_window_draw_border(gc, window);
        }

        temp_rect = Rect_new_unlocked(window->client_y,
                                      window->client_x,
                                      window->client_yh1,
                                      window->client_xw1);

        intersect_clip_rect_unlocked(&window->clipping, temp_rect);
    }

    // Then subtract the screen rectangles of any children 
    // NOTE: We don't do this in window_apply_bound_clipping because, due to 
    // its recursive nature, it would cause the screen rectangles of all of 
    // our parent's children to be subtracted from the clipping area -- which
    // would eliminate this window.
    if(window->children)
    {
        for(current_node = window->children->root_node;
            current_node != NULL;
            current_node = current_node->next)
        {
            Rect tmp;
            
            current_child = (struct server_window_t *)current_node->payload;

            if((current_child->flags & WINDOW_HIDDEN))
            {
                continue;
            }

            tmp.top = current_child->y;
            tmp.left = current_child->x;
            tmp.bottom = current_child->yh1;
            tmp.right = current_child->xw1;
            subtract_clip_rect_unlocked(&window->clipping, &tmp);
        }
    }

    // Finally, with all the clipping set up, we can set the context's 0,0 
    // to the top-left corner of the window's drawable area, and call the
    // window's final paint function
    gc_copy_window(gc, window);

    clear_clip_rects(&window->clipping);

    // Even though we're no longer having all mouse events cause a redraw 
    // from the desktop down, we still need to call paint on our children in
    // the case that we were called with a dirty region list since each 
    // window needs to be responsible for recursively checking if its
    // children were dirtied 
    if(!(flags & FLAG_PAINT_CHILDREN) || !window->children)
    {
        return;
    }

    for(current_node = window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        current_child = (struct server_window_t *)current_node->payload;

        if((current_child->flags & WINDOW_HIDDEN))
        {
            continue;
        }

        if(dirty_regions)
        {
            // Check to see if the child is affected by any of the
            // dirty region rectangles
            for(temp_rect = dirty_regions->root;
                temp_rect != NULL;
                temp_rect = temp_rect->next)
            {
                if(temp_rect->left <= current_child->xw1 &&
                   temp_rect->right >= current_child->x &&
                   temp_rect->top <= current_child->yh1 &&
                   temp_rect->bottom >= current_child->y)
                {
                    break;
                }
            }

            // Skip drawing this child if no intersection was found
            if(temp_rect == NULL)
            {
                continue;
            }
        }
        
        // Otherwise, recursively request the child to redraw its dirty areas
        server_window_paint(gc, current_child, dirty_regions, flags);
    }
}


// Used to get a list of windows overlapping the passed window
void server_window_get_windows_above(struct server_window_t *parent,
                                     struct server_window_t *child,
                                     List *clip_windows)
{
    struct server_window_t *current_window;
    ListNode *current_node;

    clip_windows->count = 0;
    clip_windows->root_node = NULL;
    clip_windows->last_node = NULL;

    // We just need to get a list of all items in the
    // child list at higher indexes than the passed window
    // We start by finding the passed child in the list
    for(current_node = parent->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        if(child == (struct server_window_t *)current_node->payload)
        {
            break;
        }
    }

    // Now we just need to add the remaining items in the list
    // to the output (IF they overlap, of course)
    // NOTE: As a bonus, this will also automatically fall through
    // if the window wasn't found
    for(current_node = current_node->next;
        current_node != NULL;
        current_node = current_node->next)
    {
        current_window = (struct server_window_t *)current_node->payload;

        if((current_window->flags & WINDOW_HIDDEN))
        {
            continue;
        }

        // Our good old rectangle intersection logic
        if(current_window->x <= child->xw1 &&
		   current_window->xw1 >= child->x &&
		   current_window->y <= child->yh1 &&
		   current_window->yh1 >= child->y)
		{
		    // Insert the overlapping window
            List_add_unlocked(clip_windows, current_window);
        }
    }
}


// Used to get a list of windows which the passed window overlaps
// Same exact thing as get_windows_above, but goes backwards through
// the list.
void server_window_get_windows_below(struct server_window_t *parent,
                                     struct server_window_t *child,
                                     List *clip_windows)
{
    struct server_window_t *current_window;
    ListNode *current_node;

    clip_windows->count = 0;
    clip_windows->root_node = NULL;
    clip_windows->last_node = NULL;

    // We just need to get a list of all items in the
    // child list at higher indexes than the passed window
    // We start by finding the passed child in the list
    current_node = parent->children->last_node;

    for( ; current_node != NULL; current_node = current_node->prev)
    {
        if(child == (struct server_window_t *)current_node->payload)
        {
            break;
        }
    }

    // Now we just need to add the remaining items in the list
    // to the output (IF they overlap, of course)
    // NOTE: As a bonus, this will also automatically fall through
    // if the window wasn't found
    if(current_node)
    {
        current_node = current_node->prev;
    }

    for( ; current_node != NULL; current_node = current_node->prev)
    {
        current_window = (struct server_window_t *)current_node->payload;

        if((current_window->flags & WINDOW_HIDDEN))
        {
            continue;
        }

        // Our good old rectangle intersection logic
        if(current_window->x <= child->xw1 &&
		   current_window->xw1 >= child->x &&
		   current_window->y <= child->yh1 &&
		   current_window->yh1 >= child->y)
		{
		    // Insert the overlapping window
            List_add_unlocked(clip_windows, current_window);
        }
    }
}


void add_child_on_top(struct server_window_t *window, ListNode *new_node)
{
    ListNode *current_node = window->children->last_node;
    struct server_window_t *tmp;

    for( ; current_node != NULL; current_node = current_node->prev)
    {
        tmp = (struct server_window_t *)current_node->payload;

        if(!(tmp->flags & WINDOW_ALWAYSONTOP))
        {
            break;
        }
    }

    if(current_node)
    {
        new_node->next = current_node->next;
        current_node->next = new_node;
        new_node->prev = current_node;
        
        if(new_node->next)
        {
            new_node->next->prev = new_node;
        }
        
        if(current_node == window->children->last_node)
        {
            window->children->last_node = new_node;
        }
    }
    else
    {
        new_node->next = window->children->root_node;
        
        if(window->children->root_node)
        {
            window->children->root_node->prev = new_node;
        }
        
        window->children->root_node = new_node;
        
        if(window->children->last_node == NULL)
        {
            window->children->last_node = new_node;
        }
    }
}


void server_window_raise(struct gc_t *gc, struct server_window_t *window, 
                         uint8_t do_draw)
{
    struct server_window_t *parent = window->parent, *last_active;
    ListNode *current_node;

    if(!(window->flags & WINDOW_NOFOCUS) && parent)
    {
        if(parent->focused_child && parent->focused_child != window)
        {
            notify_win_lost_focus(parent->focused_child);
            notify_win_gained_focus(window);
        }
        
        parent->focused_child = window;
    }

    if((window->flags & WINDOW_NORAISE) || !parent || !parent->children)
    {
        goto may_draw;
    }

    if(parent->active_child == window && 
       window->state != WINDOW_STATE_FULLSCREEN)
    {
        goto may_draw;
    }
    
    last_active = parent->active_child;

    // Find the child in the list
    for(current_node = parent->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        if(window == (struct server_window_t *)current_node->payload)
        {
            // Re-point neighbors to each other 
            if(current_node->prev)
            {
                current_node->prev->next = current_node->next;
            }

            if(current_node->next)
            {
                current_node->next->prev = current_node->prev;
            }

            // If the item was the root item, we need to make
            // the node following it the new root
            if(current_node == parent->children->root_node)
            {
                parent->children->root_node = current_node->next;
            }

            if(current_node == parent->children->last_node)
            {
                parent->children->last_node = current_node->prev;
            }

            current_node->prev = NULL;
            current_node->next = NULL;

            // If there aren't any items in the list yet, assign the
            // new item to the root node
            if(!parent->children->root_node)
            {
                parent->children->root_node = current_node;
                parent->children->last_node = current_node;
            }
            else
            {
                if((window->flags & WINDOW_ALWAYSONTOP))
                {
                    // Otherwise, we'll find the last node and add our 
                    // new node after it
                    parent->children->last_node->next = current_node;
                    current_node->prev = parent->children->last_node;
                    parent->children->last_node = current_node;
                }
                else
                {
                    add_child_on_top(parent, current_node);
                }
            }

            break;
        }
    }

    if(window->type == WINDOW_TYPE_WINDOW || 
       window->type == WINDOW_TYPE_DIALOG)
    {
        // Make it active 
        parent->active_child = window;

        if(last_active && last_active != window)
        {
            if(!(last_active->flags & WINDOW_HIDDEN))
            {
                // exit fullscreen mode if needed
                if(last_active->state == WINDOW_STATE_FULLSCREEN)
                {
                    server_window_toggle_fullscreen(gc, last_active);
                }

                // and update the title
                server_window_update_title(gc, last_active);

                notify_win_lowered(last_active);
            }

            notify_win_raised(window);
        }
    }


may_draw:

    if(do_draw)
    {
        server_window_paint(gc, window, (RectList *)0, 
                            FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);
        invalidate_screen_rect(window->y, window->x, window->yh1, window->xw1);
    }

    if(window->displayed_dialog)
    {
        server_window_raise(gc, window->displayed_dialog, 1 /* do_draw */);
    }
}


// We're wrapping this guy so that we can handle any needed redraw
void server_window_move(struct gc_t *gc, struct server_window_t *window, 
                        int new_x, int new_y)
{
    int old_x = window->x;
    int old_y = window->y;
    Rect new_window_rect;

    RectList *replacement_list, *dirty_list;
    List dirty_windows;

    if(new_y < desktop_bounds.top)
    {
        return;
    }

    // To make life a little bit easier, we'll make the not-unreasonable 
    // rule that if a window is moved, it must become the top-most window
    server_window_raise(gc, window, 0); // Raise it, but don't repaint it yet

    // We'll hijack our dirty rect collection from our existing clipping 
    // operations. So, first we'll get the visible regions of the original
    // window position
    server_window_apply_bound_clipping(window, 0, 0, &window->clipping);

    // Temporarily update the window position
    server_window_set_size(window, new_x, new_y, 
                           window->client_w, window->client_h);

    // Calculate the new bounds
    new_window_rect.top = window->y;
    new_window_rect.left = window->x;
    new_window_rect.bottom = new_window_rect.top + window->h - 1;
    new_window_rect.right = new_window_rect.left + window->w - 1;

    // Reset the window position
    server_window_set_size(window, old_x, old_y, 
                           window->client_w, window->client_h);

    // Now, we'll get the *actual* dirty area by subtracting the new location 
    // of the window 
    subtract_clip_rect(&window->clipping, &new_window_rect);

    // Now that the context clipping tools made the list of dirty rects for us,
    // we can go ahead and steal the list it made for our own purposes
    if(!(replacement_list = RectList_new()))
    {
        clear_clip_rects(&window->clipping);
        return;
    }

    dirty_list = window->clipping.clip_rects;
    window->clipping.clip_rects = replacement_list;
    window->clipping.clipping_on = 0;

    // Now, let's get all of the siblings that we overlap before the move
    server_window_get_windows_below(window->parent, window, &dirty_windows);

    server_window_set_size(window, new_x, new_y, 
                           window->client_w, window->client_h);

    // And we'll repaint all of them using the dirty rects
    // (removing them from the list as we go for convenience)
    ListNode *current_node = dirty_windows.root_node;

    while(current_node)
    {
        server_window_paint(gc, (struct server_window_t *)
                                  current_node->payload, dirty_list,
                                      FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);
        current_node = current_node->next;
    }

    // The one thing that might still be dirty is the parent we're inside of
    server_window_paint(gc, window->parent, dirty_list, 0);

    // We're done with the lists, so we can dump them
    Rect *r = dirty_list->root;

    while(r)
    {
        invalidate_screen_rect(r->top, r->left, r->bottom, r->right);
        r = r->next;
    }

    while(dirty_windows.root_node)
    {
        current_node = dirty_windows.root_node;
        dirty_windows.root_node = current_node->next;
        Listnode_free_unlocked(current_node);
    }

    while(dirty_list->root)
    {
        r = dirty_list->root;
        dirty_list->root = r->next;
        Rect_free_unlocked(r);
    }

    RectList_free_unlocked(dirty_list);

    // With the dirtied siblings redrawn, we can do the final update of 
    // the window location and paint it at that new position
    server_window_paint(gc, window, (RectList *)0, 
                        FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);

    draw_mouse_cursor(0);
    invalidate_screen_rect(window->y, window->x, window->yh1, window->xw1);
    send_pos_changed_event(window);
}


void server_window_create_canvas(struct gc_t *gc, 
                                 struct server_window_t *window)
{
    int new_shmid;
    uint8_t *old_canvas = window->canvas;
    uint8_t *new_canvas;
    uint32_t new_canvas_size;
    uint32_t new_canvas_pitch;

    window->canvas = NULL;
    new_canvas_size = window->client_w * window->client_h * gc->pixel_width;

    if((new_canvas = create_canvas(new_canvas_size, &new_shmid)))
    {
        new_canvas_pitch = window->client_w * gc->pixel_width;
        
        if(window->shmid)
        {
            shmctl(window->shmid, IPC_RMID, NULL);
            window->shmid = 0;
        }

        if(old_canvas)
        {
            shmdt(old_canvas);
        }
        
        window->canvas_alloced_size = new_canvas_size;
        window->canvas_size = new_canvas_size;
        window->canvas = new_canvas;
        window->canvas_pitch = new_canvas_pitch;
        window->shmid = new_shmid;
    }
    else
    {
        window->canvas = old_canvas;
    }
}


#define RAISE_AND_RETURN(err)                       \
    if(seqid)                                       \
        send_err_event(window->clientfd->fd,        \
                       window->winid,               \
                       err, EINVAL, seqid);         \
    server_window_raise(gc, window, 1);             \
    return;


void server_window_resize(struct gc_t *gc, struct server_window_t *window,
                                           int dx, int dy, int dw, int dh,
                                           uint32_t seqid)
{
    if((dw && (window->w + dw) < window->minw) ||
       (dh && (window->h + dh) < window->minh))
    {
        server_window_raise(gc, window, 1);
        return;
    }

    if(dy && (window->y + dy) < 0 /* desktop_bounds.top */)
    {
        server_window_raise(gc, window, 1);
        return;
    }
    
    if(window->flags & WINDOW_NORESIZE)
    {
        server_window_raise(gc, window, 1);
        return;
    }

    if(dw == 0 && dh == 0)
    {
        server_window_raise(gc, window, 1);
        return;
    }
    
    // To make life a little bit easier, we'll make the not-unreasonable 
    // rule that if a window is moved, it must become the top-most window
    server_window_raise(gc, window, 0); // Raise it, but don't repaint it yet
    
    if(window->pending_resize)
    {
        window->pending_x = window->x + dx;
        window->pending_y = window->y + dy;
        window->pending_w = window->client_w + dw;
        window->pending_h = window->client_h + dh;
        return;
    }

    window->pending_x = 0;
    window->pending_y = 0;
    window->pending_w = 0;
    window->pending_h = 0;
    window->pending_resize = 1;

    send_resize_offer(window, window->x + dx, window->y + dy,
                              window->client_w + dw,
                              window->client_h + dh, seqid);
}


void server_window_resize_absolute(struct gc_t *gc,
                                   struct server_window_t *window,
                                   int x, int y, int w, int h, uint32_t seqid)
{
    /*
    if(window->flags & WINDOW_NORESIZE)
    {
        RAISE_AND_RETURN(EVENT_WINDOW_RESIZE_OFFER);
    }
    */

    if(w < window->minw)
    {
        w = window->minw;
    }

    if(h < window->minh)
    {
        h = window->minh;
    }

    // To make life a little bit easier, we'll make the not-unreasonable 
    // rule that if a window is moved, it must become the top-most window
    server_window_raise(gc, window, 0); // Raise it, but don't repaint it yet
    
    if(window->pending_resize)
    {
        window->pending_x = x;
        window->pending_y = y;
        window->pending_w = w;
        window->pending_h = h;
        return;
    }

    window->pending_x = 0;
    window->pending_y = 0;
    window->pending_w = 0;
    window->pending_h = 0;
    window->pending_resize = 1;

    send_resize_offer(window, x, y, w, h, seqid);
}


#define CANCEL_RESIZE(window)   \
    window->pending_x = 0;      \
    window->pending_y = 0;      \
    window->pending_w = 0;      \
    window->pending_h = 0;      \
    window->pending_resize = 0;


void server_window_resize_accept(struct gc_t *gc, 
                                 struct server_window_t *window,
                                 int new_x, int new_y,
                                 int new_w, int new_h, uint32_t seqid)
{
    if((new_w < window->minw) ||
       (new_h < window->minh))
    {
        CANCEL_RESIZE(window);
        RAISE_AND_RETURN(EVENT_WINDOW_RESIZE_CONFIRM);
    }

    if(new_y < 0 /* desktop_bounds.top */)
    {
        CANCEL_RESIZE(window);
        RAISE_AND_RETURN(EVENT_WINDOW_RESIZE_CONFIRM);
    }
    
    /*
    if(window->flags & WINDOW_NORESIZE)
    {
        RAISE_AND_RETURN(EVENT_WINDOW_RESIZE_CONFIRM);
    }
    */
    
    if(window->resize.canvas)
    {
        CANCEL_RESIZE(window);
        RAISE_AND_RETURN(EVENT_WINDOW_RESIZE_CONFIRM);
    }
    
    window->resize.x = new_x;
    window->resize.y = new_y;
    window->resize.w = new_w;
    window->resize.h = new_h;

    int new_shmid;
    uint8_t *new_canvas;
    uint32_t new_canvas_pitch = new_w * gc->pixel_width;
    uint32_t new_canvas_size = new_w * new_h * gc->pixel_width;

    if(new_canvas_size > window->canvas_alloced_size)
    {
        if((new_canvas = create_canvas(new_canvas_size, &new_shmid)))
        {
            window->resize.canvas_alloced_size = new_canvas_size;
            window->resize.canvas_size = new_canvas_size;
            window->resize.canvas = new_canvas;
            window->resize.canvas_pitch = new_canvas_pitch;
            window->resize.shmid = new_shmid;
        }
    }
    else
    {
        window->resize.canvas_alloced_size = window->canvas_alloced_size;
        window->resize.canvas_size = new_canvas_size;
        window->resize.canvas = window->canvas;
        window->resize.canvas_pitch = new_canvas_pitch;
        window->resize.shmid = window->shmid;
    }

    send_resize_confirmation(window, seqid);
}


void server_window_resize_finalize(struct gc_t *gc, 
                                   struct server_window_t *window)
{
    int old_x = window->x;
    int old_y = window->y;
    int old_w = window->client_w;
    int old_h = window->client_h;
    Rect new_window_rect;
    RectList *replacement_list, *dirty_list;
    List dirty_windows;
    
    if(!window->resize.canvas)
    {
        return;
    }

    // We'll hijack our dirty rect collection from our existing clipping 
    // operations. So, first we'll get the visible regions of the original
    // window position
    server_window_apply_bound_clipping(window, 0, NULL, &window->clipping);

    // Temporarily update the window position
    server_window_set_size(window, window->resize.x, window->resize.y,
                                   window->resize.w, window->resize.h);

    // Calculate the new bounds
    new_window_rect.top = window->y;
    new_window_rect.left = window->x;
    new_window_rect.bottom = new_window_rect.top + window->h - 1;
    new_window_rect.right = new_window_rect.left + window->w - 1;

    // Reset the window position
    server_window_set_size(window, old_x, old_y, old_w, old_h);

    // Now, we'll get the *actual* dirty area by subtracting the new location 
    // of the window 
    subtract_clip_rect(&window->clipping, &new_window_rect);

    // Now that the context clipping tools made the list of dirty rects for us,
    // we can go ahead and steal the list it made for our own purposes
    if(!(replacement_list = RectList_new()))
    {
        clear_clip_rects(&window->clipping);
        return;
    }

    dirty_list = window->clipping.clip_rects;
    window->clipping.clip_rects = replacement_list;
    window->clipping.clipping_on = 0;

    // Now, let's get all of the siblings that we overlap before the move
    server_window_get_windows_below(window->parent, window, &dirty_windows);

    server_window_set_size(window, window->resize.x, window->resize.y,
                                   window->resize.w, window->resize.h);

    ListNode *current_node = dirty_windows.root_node;

    while(current_node)
    {
        server_window_paint(gc, (struct server_window_t *)
                                  current_node->payload, dirty_list,
                                      FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);
        current_node = current_node->next;
    }

    // The one thing that might still be dirty is the parent we're inside of
    server_window_paint(gc, window->parent, dirty_list, 0);

    Rect *r = dirty_list->root;

    while(r)
    {
        invalidate_screen_rect(r->top, r->left, r->bottom, r->right);
        r = r->next;
    }

    while(dirty_windows.root_node)
    {
        current_node = dirty_windows.root_node;
        dirty_windows.root_node = current_node->next;
        Listnode_free_unlocked(current_node); 
    }

    while(dirty_list->root)
    {
        r = dirty_list->root;
        dirty_list->root = r->next;
        Rect_free_unlocked(r);
    }

    RectList_free_unlocked(dirty_list);

    if(window->shmid && window->shmid != window->resize.shmid)
    {
        shmctl(window->shmid, IPC_RMID, NULL);
        //window->shmid = 0;
    }

    if(window->canvas && window->canvas != window->resize.canvas)
    {
        shmdt(window->canvas);
        //window->canvas = NULL;
    }

    window->canvas_size = window->resize.canvas_size;
    window->canvas_alloced_size = window->resize.canvas_alloced_size;
    window->canvas = window->resize.canvas;
    window->canvas_pitch = window->resize.canvas_pitch;
    window->shmid = window->resize.shmid;

    window->resize.canvas_alloced_size = 0;
    window->resize.canvas_size = 0;
    window->resize.canvas = 0;
    window->resize.canvas_pitch = 0;
    window->resize.shmid = 0;
}


void server_window_hide(struct gc_t *gc, struct server_window_t *window)
{
    RectList *replacement_list, *dirty_list;
    List dirty_windows;
    
    // We'll hijack our dirty rect collection from our existing clipping 
    // operations. So, first we'll get the visible regions of the original
    // window position
    server_window_apply_bound_clipping(window, 0, 0, &window->clipping);

    // Now that the context clipping tools made the list of dirty rects for us,
    // we can go ahead and steal the list it made for our own purposes
    if(!(replacement_list = RectList_new()))
    {
        clear_clip_rects(&window->clipping);
        return;
    }

    dirty_list = window->clipping.clip_rects;
    window->clipping.clip_rects = replacement_list;
    window->clipping.clipping_on = 0;

    // Now, let's get all of the siblings that we overlap before the move
    server_window_get_windows_below(window->parent, window, &dirty_windows);

    // And we'll repaint all of them using the dirty rects
    // (removing them from the list as we go for convenience)
    ListNode *current_node = dirty_windows.root_node;

    while(current_node)
    {
        struct server_window_t *w = (struct server_window_t *)
                                            current_node->payload;

        server_window_paint(gc, w, dirty_list, 
                            FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);
        invalidate_screen_rect(w->y, w->x, w->yh1, w->xw1);
        current_node = current_node->next;
    }

    // The one thing that might still be dirty is the parent we're inside of
    server_window_paint(gc, window->parent, dirty_list, 0);

    // We're done with the lists, so we can dump them
    Rect *r = dirty_list->root;

    while(r)
    {
        invalidate_screen_rect(r->top, r->left, r->bottom, r->right);
        r = r->next;
    }

    while(dirty_windows.root_node)
    {
        current_node = dirty_windows.root_node;
        dirty_windows.root_node = current_node->next;
        Listnode_free_unlocked(current_node); 
    }

    while(dirty_list->root)
    {
        r = dirty_list->root;
        dirty_list->root = r->next;
        Rect_free_unlocked(r);
    }

    RectList_free_unlocked(dirty_list);

    struct server_window_t *owner;

    if(window->owner_winid && 
       (owner = server_window_by_winid(window->owner_winid)))
    {
        if(owner->displayed_dialog == window)
        {
            owner->displayed_dialog = NULL;
        }
    }
}


// Quick wrapper for shoving a new entry into the child list
void server_window_insert_child(struct server_window_t *window, 
                                struct server_window_t *child)
{
    child->parent = window;
    
    /*
     * If the new window wants to be on top, just chuck it at the end of the
     * children list. Otherwise, we have to add it below any windows that want
     * to stay on top (e.g. desktop panels and the ALT-TAB window).
     */
    if((child->flags & WINDOW_ALWAYSONTOP))
    {
        List_add(window->children, child);
    }
    else
    {
        ListNode *new_node;

        if(!(new_node = ListNode_new(child)))
        {
            return;
        }
        
        add_child_on_top(window, new_node);
        window->children->count++;
    }
}


void server_window_remove_child(struct server_window_t *window, 
                                struct server_window_t *child)
{
    ListNode *current_node;

    for(current_node = window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        if(child == (struct server_window_t *)current_node->payload)
        {
            // Re-point neighbors to each other 
            if(current_node->prev)
            {
                current_node->prev->next = current_node->next;
            }

            if(current_node->next)
            {
                current_node->next->prev = current_node->prev;
            }

            // If the item was the root item, we need to make
            // the node following it the new root
            if(current_node == window->children->root_node)
            {
                window->children->root_node = current_node->next;
            }

            if(current_node == window->children->last_node)
            {
                window->children->last_node = current_node->prev;
            }

            // Make sure the count of items is up-to-date
            window->children->count--; 

            // Now that we've clipped the node out of the list, we must 
            // free its memory
            Listnode_free((ListNode *)current_node); 

            break;
        }
    }

    cancel_active_child(window, child);
}


// Assign a string to the title of the window
void server_window_set_title(struct gc_t *gc, struct server_window_t *window,
                                              char *new_title, size_t new_len)
{
    char *title_copy;
    
    if(!new_title || !new_len)
    {
        title_copy = NULL;
    }
    else
    {
        if(!(title_copy = (char *)malloc((new_len + 1) * sizeof(char))))
        {
            return;
        }
        
        A_memcpy(title_copy, new_title, new_len);
        title_copy[new_len] = '\0';
    }
    
    // Make sure to free any preexisting title 
    if(window->title)
    {
        free(window->title);
    }

    window->title = title_copy;

    if((window->flags & WINDOW_HIDDEN))
    {
        return;
    }

    // Make sure the change is reflected on-screen
    if(window->flags & WINDOW_NODECORATION)
    {
        server_window_invalidate(gc, window, 0, 0, 
                                     window->h - 1, window->w - 1);
    }
    else
    {
        server_window_update_title(gc, window);
    }
}

