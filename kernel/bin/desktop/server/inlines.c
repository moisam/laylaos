/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: inlines.c
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
 *  \file inlines.c
 *
 *  Common inlined functions used by the server.
 */

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

// get the absolute on-screen x-coordinate of this window
INLINE int server_window_screen_x(struct server_window_t *window)
{
    int x = 0;

    while(window)
    {
        x += window->x;
        window = window->parent;
    }
    
    return x;
}


// get the absolute on-screen y-coordinate of this window
INLINE int server_window_screen_y(struct server_window_t *window)
{
    int y = 0;

    while(window)
    {
        y += window->y;
        window = window->parent;
    }
    
    return y;
}


INLINE void grab_mouse(struct server_window_t *win, int confine)
{
    grabbed_mouse_window = win;

    if(confine)
    {
        mouse_bounds.top = win->client_y;
        mouse_bounds.left = win->client_x;
        mouse_bounds.bottom = win->client_y + win->client_h - 1;
        mouse_bounds.right = win->client_x + win->client_w - 1;
        mouse_is_confined = confine;
    }
}


INLINE void ungrab_mouse(void)
{
    grabbed_mouse_window = NULL;
    mouse_is_confined = 0;
    mouse_bounds.left = 0;
    mouse_bounds.top = 0;
    mouse_bounds.right = gc->w - 1;
    mouse_bounds.bottom = gc->h - 1;
}


INLINE void server_window_set_size(struct server_window_t *window,
                                   int16_t x, int16_t y,
                                   uint16_t w, uint16_t h)
{
    window->x = x;
    window->y = y;
    window->w = w;
    window->h = h;
    window->client_x = x;
    window->client_y = y;
    window->client_w = w;
    window->client_h = h;

    if(!(window->flags & WINDOW_NODECORATION))
    {
        window->w += (2 * WINDOW_BORDERWIDTH);
        window->h += WINDOW_TITLEHEIGHT + WINDOW_BORDERWIDTH;
        window->client_x += WINDOW_BORDERWIDTH;
        window->client_y += WINDOW_TITLEHEIGHT;
    }
    
    window->xw1 = window->x + window->w - 1;
    window->yh1 = window->y + window->h - 1;

    window->client_xw1 = window->client_x + window->client_w - 1;
    window->client_yh1 = window->client_y + window->client_h - 1;

    if(grabbed_mouse_window == window)
    {
        // update mouse bounds
        grab_mouse(window, mouse_is_confined);
    }
}


INLINE void reset_controlbox_state(struct gc_t *gc, 
                                   struct server_window_t *window)
{
    int state = window->controlbox_state;

    window->controlbox_state &=
                    ~(CLOSEBUTTON_OVER|MAXIMIZEBUTTON_OVER|MINIMIZEBUTTON_OVER);

    if(state != window->controlbox_state)
    {
        server_window_draw_controlbox(gc, window,
                                          server_window_screen_x(window),
                                          server_window_screen_y(window),
                                          CONTROLBOX_FLAG_CLIP | 
                                          CONTROLBOX_FLAG_INVALIDATE);
    }
}


/*
 * Let the active window (and its active child) know the mouse has left
 * their coordinates (so they can redraw themselves, for example).
 */
INLINE void mouse_exit(struct gc_t *gc, struct server_window_t *window,
                                        int mouse_x, int mouse_y,
                                        mouse_buttons_t mouse_buttons)
{

go:

    if(!(window->flags & (WINDOW_NODECORATION|WINDOW_NOCONTROLBOX)))
    {
        reset_controlbox_state(gc, window);
    }

    send_mouse_exit_event(window, mouse_x - window->x,
                                  mouse_y - window->y, mouse_buttons);

    if(window->mouseover_child)
    {
        struct server_window_t *tmp = window->mouseover_child;
        window->mouseover_child = NULL;
        window = tmp;
        goto go;
    }
}




#include <limits.h>

#ifndef SCREEN_RECTS_NOT_EXTERNS
extern Rect rtmp[];
extern int count;
extern struct gc_t *gc;
extern mutex_t update_lock;
#endif

INLINE void do_screen_update(void)
{
    Rect rr = { INT_MAX, INT_MAX, 0, 0 };

    if(count == 0)
    {
        return;
    }

    for(int x = 0; x < count; x++)
    {
        if(rtmp[x].top < rr.top)
        {
            rr.top = rtmp[x].top;
        }

        if(rtmp[x].left < rr.left)
        {
            rr.left = rtmp[x].left;
        }

        if(rtmp[x].bottom > rr.bottom)
        {
            rr.bottom = rtmp[x].bottom;
        }

        if(rtmp[x].right > rr.right)
        {
            rr.right = rtmp[x].right;
        }
    }
    
    ioctl(__global_gui_data.fbfd, FB_INVALIDATE_AREA, &rr);
    count = 0;
}

INLINE void invalidate_screen_rect(int top, int left, int bottom, int right)
{
    mutex_lock(&update_lock);

    if(count >= 64)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        do_screen_update();
    }


    for(int y = 0; y < count; y++)
    {
        if(rtmp[y].top <= top &&
           rtmp[y].left <= left &&
           rtmp[y].bottom >= bottom &&
           rtmp[y].right >= right)
        {
            mutex_unlock(&update_lock);
            return;
        }
    }
    
    rtmp[count].top = top;
    rtmp[count].left = left;
    rtmp[count].bottom = bottom;
    rtmp[count].right = right;
    count++;
    mutex_unlock(&update_lock);
}


INLINE void invalidate_window(struct server_window_t *win)
{
    invalidate_screen_rect(win->y, win->x, 
                           win->y + win->h - 1, win->x + win->w - 1);
}

