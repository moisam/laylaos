/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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


#define GLOB                        __global_gui_data
#define pixels                      CONTROL_BUTTON_LENGTH

/*
 * Each controlbox button has 4 states:
 *    - normal
 *    - mouse over
 *    - disabled
 *    - disabled and mouse over
 *
 * These 4 states has similar states for when the window is not focused.
 * The state defines what background and text color are used to draw the button.
 */
#define TOTAL_BUTTON_STATES         8
#define STATE_NORMAL                0
#define STATE_HOVER                 1
#define STATE_DISABLED              2
#define STATE_DISABLED_HOVER        3

uint32_t *bclose_pixels[TOTAL_BUTTON_STATES];
uint32_t *bmin_pixels[TOTAL_BUTTON_STATES];
uint32_t *bmax_pixels[TOTAL_BUTTON_STATES];

// defined in main.c
extern Rect desktop_bounds;
extern pid_t mypid;

#define __      0,
#define _X      0xff,

static uint32_t bclose_mask[pixels * pixels] =
{
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ _X _X __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ _X _X __ __ __ _X _X __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ _X _X __ _X _X __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ _X _X _X __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ _X __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ _X _X _X __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ _X _X __ _X _X __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ _X _X __ __ __ _X _X __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ _X _X __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
};

static uint32_t bmax_mask[pixels * pixels] =
{
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X _X _X _X _X _X _X _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X _X _X _X _X _X _X _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ __ _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ __ _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ __ _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ __ _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ __ _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ __ _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X __ __ __ __ __ __ _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X _X _X _X _X _X _X _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X _X _X _X _X _X _X _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
};

static uint32_t bmin_mask[pixels * pixels] =
{
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X _X _X _X _X _X _X _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ _X _X _X _X _X _X _X _X _X _X __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ 
};

#undef __
#undef _X


static void alloc_controlbox_bitmaps(void)
{
    size_t sz = 4 * pixels * pixels;
    int i;

    // alloc some memory for the button "bitmaps"
    for(i = 0; i < TOTAL_BUTTON_STATES; i++)
    {
        bclose_pixels[i] = malloc(sz);
        bmax_pixels[i] = malloc(sz);
        bmin_pixels[i] = malloc(sz);

        if(!bclose_pixels[i] || !bmax_pixels[i] || !bmin_pixels[i])
        {
            printf("gui: failed to alloc memory for controlbox buttons!");
            abort();
        }
    }
}


void reinit_window_controlbox(void)
{
    int x;
    int sz = pixels * pixels;
    int bottomline = (pixels - 1) * pixels;
    uint32_t text = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_TEXT];
    uint32_t textdis = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_TEXT];
    uint32_t textdishi = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_TEXT_SHADOW];
    uint32_t intext = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_INACTIVE_TEXT];
    uint32_t intextdis = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_INACTIVE_TEXT];
    uint32_t intextdishi = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_INACTIVE_TEXT_SHADOW];

    // fill in the backgrounds
#define FILL_ALL_BACKGROUNDS(state, color)      \
    memset32(bclose_pixels[state], color, sz);  \
    memset32(bmax_pixels[state], color, sz);    \
    memset32(bmin_pixels[state], color, sz);

    FILL_ALL_BACKGROUNDS(0, GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_BGCOLOR]);
    FILL_ALL_BACKGROUNDS(1, GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_BGCOLOR_HOVER]);
    FILL_ALL_BACKGROUNDS(2, GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_BGCOLOR]);
    FILL_ALL_BACKGROUNDS(3, GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_BGCOLOR]);
    FILL_ALL_BACKGROUNDS(4, GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_INACTIVE_BGCOLOR]);
    FILL_ALL_BACKGROUNDS(5, GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_INACTIVE_BGCOLOR_HOVER]);
    FILL_ALL_BACKGROUNDS(6, GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_INACTIVE_BGCOLOR]);
    FILL_ALL_BACKGROUNDS(7, GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_INACTIVE_BGCOLOR]);

#undef FILL_ALL_BACKGROUNDS

    // for the disabled button state, fill the shadow icon before the normal one
    for(x = 0; x < bottomline; x++)
    {
        if(bclose_mask[x])
        {
            bclose_pixels[2][x + pixels + 1] = textdishi;
            bclose_pixels[3][x + pixels + 1] = textdishi;
            bclose_pixels[6][x + pixels + 1] = intextdishi;
            bclose_pixels[7][x + pixels + 1] = intextdishi;
        }

        if(bmax_mask[x])
        {
            bmax_pixels[2][x + pixels + 1] = textdishi;
            bmax_pixels[3][x + pixels + 1] = textdishi;
            bmax_pixels[6][x + pixels + 1] = intextdishi;
            bmax_pixels[7][x + pixels + 1] = intextdishi;
        }

        if(bmin_mask[x])
        {
            bmin_pixels[2][x + pixels + 1] = textdishi;
            bmin_pixels[3][x + pixels + 1] = textdishi;
            bmin_pixels[6][x + pixels + 1] = intextdishi;
            bmin_pixels[7][x + pixels + 1] = intextdishi;
        }
    }

    // now draw the buttons
    for(x = 0; x < pixels * pixels; x++)
    {
        if(bclose_mask[x])
        {
            bclose_pixels[0][x] = text;
            bclose_pixels[1][x] = text;
            bclose_pixels[2][x] = textdis;
            bclose_pixels[3][x] = textdis;
            bclose_pixels[4][x] = intext;
            bclose_pixels[5][x] = intext;
            bclose_pixels[6][x] = intextdis;
            bclose_pixels[7][x] = intextdis;
        }

        if(bmax_mask[x])
        {
            bmax_pixels[0][x] = text;
            bmax_pixels[1][x] = text;
            bmax_pixels[2][x] = textdis;
            bmax_pixels[3][x] = textdis;
            bmax_pixels[4][x] = intext;
            bmax_pixels[5][x] = intext;
            bmax_pixels[6][x] = intextdis;
            bmax_pixels[7][x] = intextdis;
        }

        if(bmin_mask[x])
        {
            bmin_pixels[0][x] = text;
            bmin_pixels[1][x] = text;
            bmin_pixels[2][x] = textdis;
            bmin_pixels[3][x] = textdis;
            bmin_pixels[4][x] = intext;
            bmin_pixels[5][x] = intext;
            bmin_pixels[6][x] = intextdis;
            bmin_pixels[7][x] = intextdis;
        }
    }

    // draw the borders
    uint32_t bhi = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_BORDER_HI];
    uint32_t bhovhi = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_BORDER_HI_HOVER];
    uint32_t blo = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_BORDER_LO];
    uint32_t bhovlo = GLOB.themecolor[THEME_COLOR_WINDOW_CONTROLBOX_BORDER_LO_HOVER];
    int i, j;

#define PUT_PIXEL(state, x, color)      \
        bclose_pixels[state][x] = color;\
        bmax_pixels[state][x] = color;  \
        bmin_pixels[state][x] = color;  \

    // top
    for(x = 0; x < pixels; x++)
    {
        PUT_PIXEL(0, x, bhi);
        PUT_PIXEL(1, x, bhovhi);
        PUT_PIXEL(4, x, bhi);
        PUT_PIXEL(5, x, bhovhi);
    }

    // bottom
    for(x = 0; x < pixels; x++)
    {
        i = bottomline + x;

        PUT_PIXEL(0, i, blo);
        PUT_PIXEL(1, i, bhovlo);
        PUT_PIXEL(4, i, blo);
        PUT_PIXEL(5, i, bhovlo);
    }

    // sides
    for(x = 0; x < pixels; x++)
    {
        i = (x * pixels);
        j = (x * pixels) + pixels - 1;

        PUT_PIXEL(0, i, bhi);
        PUT_PIXEL(0, j, blo);
        PUT_PIXEL(1, i, bhovhi);
        PUT_PIXEL(1, j, bhovlo);
        PUT_PIXEL(4, i, bhi);
        PUT_PIXEL(4, j, blo);
        PUT_PIXEL(5, i, bhovhi);
        PUT_PIXEL(5, j, bhovlo);
    }
}


void prep_window_controlbox(void)
{
    alloc_controlbox_bitmaps();
    reinit_window_controlbox();
}


static inline
void server_window_invalidate_controlbox(int wscreen_x, int wscreen_y,
                                         uint16_t winw)
{
    invalidate_screen_rect(wscreen_y,
                           wscreen_x + winw - CONTROL_BUTTON_LENGTH3 - 5,
                           wscreen_y + WINDOW_TITLEHEIGHT - 1,
                           wscreen_x + winw - 1);
}


void server_window_draw_controlbox(struct gc_t *gc,
                                   struct server_window_t *window,
                                   int wscreen_x, int wscreen_y, int flags)
{
    int x, y;
    struct bitmap32_t bitmap = { .width = pixels, .height = pixels };
    struct clipping_t saved_clipping;
    int active = (window->parent->active_child == window);
    int index;

    if(flags & CONTROLBOX_FLAG_CLIP)
    {
        server_window_apply_bound_clipping(window, NULL, NULL, &window->clipping, 0);
    }

    gc_get_clipping(gc, &saved_clipping);
    gc_set_clipping(gc, &window->clipping);

    // draw the close button
    index = (window->controlbox_state & CLOSEBUTTON_OVER) ? STATE_HOVER : STATE_NORMAL;
    index += active ? 0 : 4;

    bitmap.data = bclose_pixels[index];
    x = wscreen_x + window->w - 5 - pixels;
    y = wscreen_y + 5;

    gc_blit_bitmap_highlighted(gc, &bitmap, x, y, 0, 0, pixels, pixels, 0);

    // draw the maximize button
    index = (window->flags & WINDOW_NORESIZE) ? STATE_DISABLED : STATE_NORMAL;
    index += (window->controlbox_state & MAXIMIZEBUTTON_OVER) ? 1 : 0;
    index += active ? 0 : 4;

    bitmap.data = bmax_pixels[index];
    x -= pixels;
    gc_blit_bitmap_highlighted(gc, &bitmap, x, y, 0, 0, pixels, pixels, 0);

    // draw the minimize button
    index = (window->flags & WINDOW_NOMINIMIZE) ? STATE_DISABLED : STATE_NORMAL;
    index += (window->controlbox_state & MINIMIZEBUTTON_OVER) ? 1 : 0;
    index += active ? 0 : 4;

    bitmap.data = bmin_pixels[index];
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


static inline void save_window_state(struct server_window_t *window)
{
    window->saved.x = window->x;
    window->saved.y = window->y;
    window->saved.w = window->client_w;
    window->saved.h = window->client_h;
    window->saved.flags = window->flags;
}

void server_window_maximize(struct gc_t *gc,
                            struct server_window_t *window,
                            uint32_t seqid)
{
    if(window->state == WINDOW_STATE_MAXIMIZED)
    {
        return;
    }

    // Maximize the window
    // Take into account the window border and title if the window has
    // decorations on
    int neww = desktop_bounds.right - desktop_bounds.left -
                        ((window->flags & WINDOW_NODECORATION) ? 0 :
                            (2 * WINDOW_BORDERWIDTH));
    int newh = desktop_bounds.bottom - desktop_bounds.top -
                        ((window->flags & WINDOW_NODECORATION) ? 0 :
                            (WINDOW_TITLEHEIGHT + WINDOW_BORDERWIDTH));

    save_window_state(window);
    window->state = WINDOW_STATE_MAXIMIZED;
    server_window_resize_absolute(gc, window,
                                  desktop_bounds.left, desktop_bounds.top,
                                  neww, newh, seqid);
}


void server_window_fullscreen(struct gc_t *gc,
                              struct server_window_t *window,
                              uint32_t seqid)
{
    if(window->state == WINDOW_STATE_FULLSCREEN)
    {
        return;
    }

    // Enter fullscreen mode
    save_window_state(window);
    window->state = WINDOW_STATE_FULLSCREEN;
    window->flags |= (WINDOW_NODECORATION | 
                      WINDOW_NOCONTROLBOX | 
                      WINDOW_ALWAYSONTOP);
    server_window_resize_absolute(gc, window, 0, 0,
                                      GLOB.screen.w, GLOB.screen.h, seqid);
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


void server_window_minimize(struct gc_t *gc, struct server_window_t *window)
{
    struct server_window_t *sibling;
    volatile ListNode *current_node;

    if(window->flags & WINDOW_HIDDEN)
    {
        return;
    }

    // Minimize the window
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


void server_window_restore(struct gc_t *gc, struct server_window_t *window, uint32_t seqid)
{
    if(window->flags & WINDOW_HIDDEN)
    {
        // If hidden, show the window
        window->flags &= ~WINDOW_HIDDEN;
        server_window_raise(gc, window, 1);
        notify_win_shown(window);
        return;
    }
    else if(window->state != WINDOW_STATE_NORMAL)
    {
        // If maximized or fullscreen, return to normal state
        window->state = WINDOW_STATE_NORMAL;
        window->flags = window->saved.flags;
        server_window_resize_absolute(gc, window,
                                          window->saved.x, window->saved.y,
                                          window->saved.w, window->saved.h, seqid);
    }
}


void server_window_close(struct gc_t *gc, struct server_window_t *window)
{
    // ask the application to close
    notify_simple_event(window->clientfd->fd, EVENT_WINDOW_CLOSING, 
                        window->winid, GLOB.mypid, 0);
}

