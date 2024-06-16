/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: server-window-controlbox.c
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
 *  \file server-window-controlbox.c
 *
 *  Functions to draw window control boxes, and handle window minimization,
 *  maximization and fullscreen requests.
 */

#define GUI_SERVER
#include <pthread.h>
#include "../include/gui.h"
#include "../include/gc.h"
#include "../include/server/server.h"
#include "../include/server/window.h"
#include "../include/server/event.h"
#include "../include/server/rects.h"
#include "../include/memops.h"

#include "inlines.c"


#define GLOB        __global_gui_data


uint32_t *bclose_pixels = NULL;
uint32_t *bmin_pixels = NULL;
uint32_t *bmax_pixels = NULL;

uint32_t *bclose_over_pixels = NULL;
uint32_t *bmin_over_pixels = NULL;
uint32_t *bmax_over_pixels = NULL;

uint32_t *bclose_disabled_pixels = NULL;
uint32_t *bmin_disabled_pixels = NULL;
uint32_t *bmax_disabled_pixels = NULL;

// defined in main.c
extern Rect desktop_bounds;
extern pid_t mypid;


void prep_window_controlbox(void)
{
    int x, y;
    static int pixels = CONTROL_BUTTON_LENGTH;
    size_t sz = 4 * pixels * pixels;

    // alloc some memory for the button "bitmaps"
    bclose_pixels = (uint32_t *)malloc(sz);
    bmax_pixels = (uint32_t *)malloc(sz);
    bmin_pixels = (uint32_t *)malloc(sz);
    bclose_over_pixels = (uint32_t *)malloc(sz);
    bmax_over_pixels = (uint32_t *)malloc(sz);
    bmin_over_pixels = (uint32_t *)malloc(sz);
    bclose_disabled_pixels = (uint32_t *)malloc(sz);
    bmax_disabled_pixels = (uint32_t *)malloc(sz);
    bmin_disabled_pixels = (uint32_t *)malloc(sz);

    if(!bclose_pixels || !bmax_pixels || !bmin_pixels ||
       !bclose_over_pixels || !bmax_over_pixels || !bmin_over_pixels ||
       !bclose_disabled_pixels || !bmax_disabled_pixels || !bmin_disabled_pixels)
    {
        printf("gui: failed to alloc memory for controlbox buttons!");
        abort();
        //return;
    }

    // fill in the backgrounds
    for(y = 0; y < pixels; y++)
    {
        uint32_t where = y * pixels;

        memset32(bclose_pixels + where, CLOSEBUTTON_BGCOLOR, pixels);
        memset32(bmax_pixels + where, MAXIMIZEBUTTON_BGCOLOR, pixels);
        memset32(bmin_pixels + where, MINIMIZEBUTTON_BGCOLOR, pixels);

        memset32(bclose_over_pixels + where, CLOSEBUTTON_MOUSEOVER_BGCOLOR, pixels);
        memset32(bmax_over_pixels + where, MAXIMIZEBUTTON_MOUSEOVER_BGCOLOR, pixels);
        memset32(bmin_over_pixels + where, MINIMIZEBUTTON_MOUSEOVER_BGCOLOR, pixels);

        memset32(bclose_disabled_pixels + where, CLOSEBUTTON_BGCOLOR, pixels);
        memset32(bmax_disabled_pixels + where, MAXIMIZEBUTTON_BGCOLOR, pixels);
        memset32(bmin_disabled_pixels + where, MINIMIZEBUTTON_BGCOLOR, pixels);
    }
    
    // fill the 'X' in the close button
    y = ((pixels / 2) - 4) * pixels;
    bclose_pixels[y + 8] = CLOSEBUTTON_TEXTCOLOR;
    bclose_pixels[y + 16] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 8] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 9] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 15] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 16] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 8] = CLOSEBUTTON_TEXTCOLOR_DISABLED;
    bclose_disabled_pixels[y + 16] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    y += pixels;
    bclose_pixels[y + 9] = CLOSEBUTTON_TEXTCOLOR;
    bclose_pixels[y + 15] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 9] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 10] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 14] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 15] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 9] = CLOSEBUTTON_TEXTCOLOR_DISABLED;
    bclose_disabled_pixels[y + 15] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    y += pixels;
    bclose_pixels[y + 10] = CLOSEBUTTON_TEXTCOLOR;
    bclose_pixels[y + 14] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 10] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 11] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 13] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 14] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 10] = CLOSEBUTTON_TEXTCOLOR_DISABLED;
    bclose_disabled_pixels[y + 14] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    y += pixels;
    bclose_pixels[y + 11] = CLOSEBUTTON_TEXTCOLOR;
    bclose_pixels[y + 13] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 11] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 12] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 13] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 11] = CLOSEBUTTON_TEXTCOLOR_DISABLED;
    bclose_disabled_pixels[y + 13] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    y += pixels;
    bclose_pixels[y + 12] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 12] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 12] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    y += pixels;
    bclose_pixels[y + 11] = CLOSEBUTTON_TEXTCOLOR;
    bclose_pixels[y + 13] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 11] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 12] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 13] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 11] = CLOSEBUTTON_TEXTCOLOR_DISABLED;
    bclose_disabled_pixels[y + 13] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    y += pixels;
    bclose_pixels[y + 10] = CLOSEBUTTON_TEXTCOLOR;
    bclose_pixels[y + 14] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 10] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 11] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 13] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 14] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 10] = CLOSEBUTTON_TEXTCOLOR_DISABLED;
    bclose_disabled_pixels[y + 14] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    y += pixels;
    bclose_pixels[y + 9] = CLOSEBUTTON_TEXTCOLOR;
    bclose_pixels[y + 15] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 9] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 10] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 14] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 15] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 9] = CLOSEBUTTON_TEXTCOLOR_DISABLED;
    bclose_disabled_pixels[y + 15] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    y += pixels;
    bclose_pixels[y + 8] = CLOSEBUTTON_TEXTCOLOR;
    bclose_pixels[y + 16] = CLOSEBUTTON_TEXTCOLOR;
    bclose_over_pixels[y + 8] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 9] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 15] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_over_pixels[y + 16] = CLOSEBUTTON_MOUSEOVER_TEXTCOLOR;
    bclose_disabled_pixels[y + 8] = CLOSEBUTTON_TEXTCOLOR_DISABLED;
    bclose_disabled_pixels[y + 16] = CLOSEBUTTON_TEXTCOLOR_DISABLED;

    // fill the box in the maximize button
    y = ((pixels / 2) - 4) * pixels;
    memset32(bmax_pixels + y + 8, MAXIMIZEBUTTON_TEXTCOLOR, 9);
    memset32(bmax_over_pixels + y + 8, MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR, 9);
    memset32(bmax_over_pixels + y + 9, MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR, 9);
    memset32(bmax_disabled_pixels + y + 8, MAXIMIZEBUTTON_TEXTCOLOR_DISABLED, 9);
    
    for(x = 1; x < 8; x++)
    {
        bmax_pixels[y + (x * pixels) + 8] = MAXIMIZEBUTTON_TEXTCOLOR;
        bmax_pixels[y + (x * pixels) + 16] = MAXIMIZEBUTTON_TEXTCOLOR;
        bmax_over_pixels[y + (x * pixels) + 8] = MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR;
        bmax_over_pixels[y + (x * pixels) + 9] = MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR;
        bmax_over_pixels[y + (x * pixels) + 16] = MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR;
        bmax_over_pixels[y + (x * pixels) + 17] = MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR;
        bmax_disabled_pixels[y + (x * pixels) + 8] = MAXIMIZEBUTTON_TEXTCOLOR_DISABLED;
        bmax_disabled_pixels[y + (x * pixels) + 16] = MAXIMIZEBUTTON_TEXTCOLOR_DISABLED;
    }

    y += (8 * pixels);
    memset32(bmax_pixels + y + 8, MAXIMIZEBUTTON_TEXTCOLOR, 9);
    memset32(bmax_over_pixels + y + 8, MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR, 9);
    memset32(bmax_over_pixels + y + 9, MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR, 9);
    memset32(bmax_disabled_pixels + y + 8, MAXIMIZEBUTTON_TEXTCOLOR_DISABLED, 9);

    // fill the '_' in the minimize button
    y = (((pixels / 2) - 4) + 8) * pixels;
    memset32(bmin_pixels + y + 8, MINIMIZEBUTTON_TEXTCOLOR, 8);
    memset32(bmin_over_pixels + y + 7, MINIMIZEBUTTON_MOUSEOVER_TEXTCOLOR, 8);
    memset32(bmin_over_pixels + y + 8, MINIMIZEBUTTON_MOUSEOVER_TEXTCOLOR, 8);
    memset32(bmin_disabled_pixels + y + 8, MINIMIZEBUTTON_TEXTCOLOR_DISABLED, 8);
}


static inline
void server_window_invalidate_controlbox(int wscreen_x, int wscreen_y,
                                         uint16_t winw)
{
    invalidate_screen_rect(wscreen_y,
                           wscreen_x + winw - WINDOW_BORDERWIDTH -
                                              CONTROL_BUTTON_LENGTH3,
                           wscreen_y + WINDOW_TITLEHEIGHT - 1,
                           wscreen_x + winw - 1);
}


#define pixels          CONTROL_BUTTON_LENGTH

void server_window_draw_controlbox(struct gc_t *gc,
                                   struct server_window_t *window,
                                   int wscreen_x, int wscreen_y, int flags)
{
    int x, y;
    struct bitmap32_t bitmap = { .width = pixels, .height = pixels };
    struct clipping_t saved_clipping;

    if(flags & CONTROLBOX_FLAG_CLIP)
    {
        server_window_apply_bound_clipping(window, 0, 0, &window->clipping);
    }

    gc_get_clipping(gc, &saved_clipping);
    gc_set_clipping(gc, &window->clipping);

    bitmap.data = (window->controlbox_state & CLOSEBUTTON_OVER) ?
                        bclose_over_pixels : bclose_pixels;
    x = wscreen_x + window->w - WINDOW_BORDERWIDTH - pixels;
    y = wscreen_y + WINDOW_BORDERWIDTH;
    gc_blit_bitmap_highlighted(gc, &bitmap, x, y, 0, 0, pixels, pixels, 0);

    bitmap.data = (window->flags & WINDOW_NORESIZE) ? bmax_disabled_pixels :
                     (window->controlbox_state & MAXIMIZEBUTTON_OVER) ?
                        bmax_over_pixels : bmax_pixels;
    x -= pixels;
    gc_blit_bitmap_highlighted(gc, &bitmap, x, y, 0, 0, pixels, pixels, 0);

    bitmap.data = (window->flags & WINDOW_NOMINIMIZE) ? bmin_disabled_pixels :
                     (window->controlbox_state & MINIMIZEBUTTON_OVER) ?
                        bmin_over_pixels : bmin_pixels;
    x -= pixels;
    gc_blit_bitmap_highlighted(gc, &bitmap, x, y, 0, 0, pixels, pixels, 0);

    gc_set_clipping(gc, &saved_clipping);
    
    if(flags & CONTROLBOX_FLAG_CLIP)
    {
        clear_clip_rects(&window->clipping);
    }
    
    if(flags & CONTROLBOX_FLAG_INVALIDATE)
    {
        server_window_invalidate_controlbox(wscreen_x, wscreen_y, window->w);
    }
}

#undef pixels


void server_window_toggle_maximize(struct gc_t *gc, 
                                   struct server_window_t *window)
{
    if(window->state == WINDOW_STATE_MAXIMIZED)
    {
        // Return to normal state
        window->state = WINDOW_STATE_NORMAL;
        window->flags = window->saved.flags;
        server_window_resize_absolute(gc, window,
                                        window->saved.x, window->saved.y,
                                        window->saved.w, window->saved.h, 1);
    }
    else
    {
        // Maximize the window
        // Take into account the window border and title if the window has
        // decorations on
        int neww = desktop_bounds.right - desktop_bounds.left -
                        ((window->flags & WINDOW_NODECORATION) ? 0 :
                            (2 * WINDOW_BORDERWIDTH));
        int newh = desktop_bounds.bottom - desktop_bounds.top -
                        ((window->flags & WINDOW_NODECORATION) ? 0 :
                            (WINDOW_TITLEHEIGHT + WINDOW_BORDERWIDTH));

        window->saved.x = window->x;
        window->saved.y = window->y;
        window->saved.w = window->client_w;
        window->saved.h = window->client_h;
        window->saved.flags = window->flags;
        window->state = WINDOW_STATE_MAXIMIZED;
        server_window_resize_absolute(gc, window,
                                  desktop_bounds.left, desktop_bounds.top,
                                  neww, newh, 1);
    }
}


void server_window_toggle_fullscreen(struct gc_t *gc, 
                                     struct server_window_t *window)
{
    if(window->state == WINDOW_STATE_FULLSCREEN)
    {
        // Return to normal state
        window->state = WINDOW_STATE_NORMAL;
        window->flags = window->saved.flags;
        server_window_resize_absolute(gc, window,
                                        window->saved.x, window->saved.y,
                                        window->saved.w, window->saved.h, 1);
    }
    else
    {
        // Enter fullscreen mode
        window->saved.x = window->x;
        window->saved.y = window->y;
        window->saved.w = window->client_w;
        window->saved.h = window->client_h;
        window->saved.flags = window->flags;
        window->state = WINDOW_STATE_FULLSCREEN;
        window->flags |= (WINDOW_NODECORATION | 
                          WINDOW_NOCONTROLBOX | 
                          WINDOW_ALWAYSONTOP);
        server_window_resize_absolute(gc, window, 0, 0,
                                        GLOB.screen.w, GLOB.screen.h, 1);
    }
}


struct server_window_t *next_active_sibling(volatile ListNode *current_node)
{
    volatile struct server_window_t *sibling;

    for( ; current_node != NULL; current_node = current_node->next)
    {
        sibling = (struct server_window_t *)current_node->payload;

        if(/* sibling->type == WINDOW_TYPE_WINDOW && */
           !(sibling->flags & (WINDOW_NORAISE | WINDOW_HIDDEN | WINDOW_NOFOCUS)) &&
           sibling->state != WINDOW_STATE_MINIMIZED)
        {
            return (struct server_window_t *)sibling;
        }
    }
    
    return NULL;
}


struct server_window_t *prev_active_sibling(volatile ListNode *current_node)
{
    volatile struct server_window_t *sibling;

    for( ; current_node != NULL; current_node = current_node->prev)
    {
        sibling = (struct server_window_t *)current_node->payload;

        if(/* sibling->type == WINDOW_TYPE_WINDOW && */
           !(sibling->flags & (WINDOW_NORAISE | WINDOW_HIDDEN | WINDOW_NOFOCUS)) &&
           sibling->state != WINDOW_STATE_MINIMIZED)
        {
            return (struct server_window_t *)sibling;
        }
    }
    
    return NULL;
}


void server_window_toggle_minimize(struct gc_t *gc, struct server_window_t *window)
{
    struct server_window_t *sibling;
    volatile ListNode *current_node;

    if(window->state == WINDOW_STATE_MINIMIZED)
    {
        // Return to normal state
        window->state = window->saved.state;
        window->flags &= ~WINDOW_HIDDEN;
        server_window_raise(gc, window, 1);
        notify_win_shown(window);
    }
    else
    {
        // Minimize the window
        window->saved.state = window->state;
        window->state = WINDOW_STATE_MINIMIZED;
        window->flags |= WINDOW_HIDDEN;
        server_window_hide(gc, window);
        notify_win_hidden(window);
        
        if(window->parent->active_child != window)
        {
            cancel_active_child(window->parent, window);
            return;
        }

        // now we have to find the next eligible active window and bring it
        // to the top
        for(current_node = window->parent->children->root_node;
            current_node != NULL;
            current_node = current_node->next)
        {
            if(window == (struct server_window_t *)current_node->payload)
            {
                // found the current window, now check the higher, then the
                // lower, sibling and bring one of them to the top
                if((sibling = next_active_sibling(current_node->next)))
                {
                    server_window_raise(gc, sibling, 1);
                    break;
                }

                if((sibling = prev_active_sibling(current_node->prev)))
                {
                    server_window_raise(gc, sibling, 1);
                    break;
                }

                // this is the sole window on the screen
                break;
            }
        }

        cancel_active_child(window->parent, window);
    }
}


void server_window_close(struct gc_t *gc, struct server_window_t *window)
{
    // ask the application to close
    notify_simple_event(window->clientfd->fd, EVENT_WINDOW_CLOSING, 
                        window->winid, GLOB.mypid, 0);
}

