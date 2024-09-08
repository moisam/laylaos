/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: scrollbar.c
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
 *  \file scrollbar.c
 *
 *  The implementation of a scrollbar widget.
 */

#include <stdlib.h>
#include <string.h>
#include "../include/client/scrollbar.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"

#define GLOB                            __global_gui_data

#define TEMPLATE_BGCOLOR                0xCDCFD4FF
#define TEMPLATE_TEXTCOLOR              0x222226FF

#define _B                              TEMPLATE_BGCOLOR
#define _T                              TEMPLATE_TEXTCOLOR

static uint32_t arrow_up_img_template[] =
{
    _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _T, _T, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _T, _T, _T, _T, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _T, _T, _T, _T, _T, _T, _T, _T, _B, _B, _B, _T,
    _T, _B, _B, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _B, _B, _T,
    _T, _B, _B, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T,
};

static uint32_t arrow_down_img_template[] =
{
    _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _B, _B, _T,
    _T, _B, _B, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _B, _B, _T,
    _T, _B, _B, _B, _T, _T, _T, _T, _T, _T, _T, _T, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _T, _T, _T, _T, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _T, _T, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T,
};

static uint32_t arrow_left_img_template[] =
{
    _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _T, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _T, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T,
};

static uint32_t arrow_right_img_template[] =
{
    _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _T, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _T, _T, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _T, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _T, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _T, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _T, _T, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _B, _T,
    _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T, _T,
};

static uint32_t arrow_up_img[16 * 16 * 4];
static uint32_t arrow_down_img[16 * 16 * 4];
static uint32_t arrow_left_img[16 * 16 * 4];
static uint32_t arrow_right_img[16 * 16 * 4];

#undef _B
#undef _T

static struct bitmap32_t arrow_up =
{
    .width = 16, .height = 16, .data = arrow_up_img,
};

static struct bitmap32_t arrow_down =
{
    .width = 16, .height = 16, .data = arrow_down_img,
};

static struct bitmap32_t arrow_left =
{
    .width = 16, .height = 16, .data = arrow_left_img,
};

static struct bitmap32_t arrow_right =
{
    .width = 16, .height = 16, .data = arrow_right_img,
};


static inline void calc_scrollbar_dimensions(struct window_t *parent,
                                             struct window_t *scrollbar_window)
{
    int one = (parent->flags & WINDOW_3D_WIDGET) ? 2 : 1;
    int two = (parent->flags & WINDOW_3D_WIDGET) ? 4 : 2;

    if(scrollbar_window->type == WINDOW_TYPE_VSCROLL)
    {
        scrollbar_window->x = parent->w - VSCROLLBAR_WIDTH - one;
        scrollbar_window->y = one;
        scrollbar_window->w = VSCROLLBAR_WIDTH;
        scrollbar_window->h = parent->h - two;
    }
    else
    {
        scrollbar_window->x = one;
        scrollbar_window->y = parent->h - HSCROLLBAR_HEIGHT - one;
        scrollbar_window->w = parent->w - two;
        scrollbar_window->h = HSCROLLBAR_HEIGHT;
    }
}


struct scrollbar_t *scrollbar_new(struct gc_t *gc, struct window_t *parent,
                                  int is_vertical)
{
    struct scrollbar_t *sbar;
    Rect *rect;

    if(!(sbar = malloc(sizeof(struct scrollbar_t))))
    {
        return NULL;
    }

    memset(sbar, 0, sizeof(struct scrollbar_t));

    if(!parent->children && !(parent->children = List_new()))
    {
        free(sbar);
        return NULL;
    }

    if(!(sbar->window.clip_rects = RectList_new()))
    {
        free(sbar);
        return NULL;
    }
    
    sbar->window.type = is_vertical ? WINDOW_TYPE_VSCROLL : WINDOW_TYPE_HSCROLL;
    calc_scrollbar_dimensions(parent, &sbar->window);

    if(is_vertical)
    {
        sbar->thumbw = sbar->window.w;
        sbar->thumbh = 32;
    }
    else
    {
        sbar->thumbw = 32;
        sbar->thumbh = sbar->window.h;
    }

    if(!(rect = Rect_new(sbar->window.y, sbar->window.x,
                         sbar->window.y + sbar->window.h - 1,
                         sbar->window.x + sbar->window.w - 1)))
    {
        RectList_free(sbar->window.clip_rects);
        free(sbar);
        return NULL;
    }

    RectList_add(sbar->window.clip_rects, rect);

    sbar->step = 2;     // set default step, user can change it later

    sbar->window.gc = gc;
    sbar->window.flags = WINDOW_NODECORATION;
    sbar->window.visible = 1;
    sbar->window.bgcolor = GLOB.themecolor[THEME_COLOR_SCROLLBAR_BGCOLOR];
    sbar->window.fgcolor = GLOB.themecolor[THEME_COLOR_SCROLLBAR_TEXTCOLOR];
    
    sbar->window.repaint = scrollbar_repaint;
    sbar->window.mousedown = scrollbar_mousedown;
    sbar->window.mouseover = scrollbar_mouseover;
    sbar->window.mouseup = scrollbar_mouseup;
    sbar->window.mouseexit = scrollbar_mouseexit;
    sbar->window.unfocus = scrollbar_unfocus;
    sbar->window.focus = scrollbar_focus;
    sbar->window.destroy = scrollbar_destroy;
    sbar->window.keypress = scrollbar_keypress;
    sbar->window.keyrelease = scrollbar_keyrelease;
    sbar->window.size_changed = widget_size_changed;
    sbar->window.theme_changed = scrollbar_theme_changed;

    window_insert_child(parent, (struct window_t *)sbar);

    return sbar;
}


void scrollbar_destroy(struct window_t *scrollbar_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(scrollbar_window);
}


static inline int vscroll_usable_pixels(struct scrollbar_t *sbar)
{
    return sbar->window.h - 16 - 16 - sbar->thumbh;
}


static inline int vscroll_y_to_val(struct scrollbar_t *sbar, int y)
{
    int valrange = sbar->max - sbar->min;
    int pixrange = vscroll_usable_pixels(sbar);

    return (y - 16) * valrange / pixrange;
}


static inline int hscroll_usable_pixels(struct scrollbar_t *sbar)
{
    return sbar->window.w - 16 - 16 - sbar->thumbw;
}


static inline int hscroll_x_to_val(struct scrollbar_t *sbar, int x)
{
    int valrange = sbar->max - sbar->min;
    int pixrange = hscroll_usable_pixels(sbar);

    return (x - 16) * valrange / pixrange;
}


static inline int thumb_offset(struct scrollbar_t *sbar)
{
    int valrange = sbar->max - sbar->min;
    int pixrange;

    if(sbar->window.type == WINDOW_TYPE_VSCROLL)
    {
        pixrange = vscroll_usable_pixels(sbar);

        return 16 + (valrange ? (sbar->val * pixrange / valrange) : 0);
    }
    else
    {
        pixrange = hscroll_usable_pixels(sbar);

        return 16 + (valrange ? (sbar->val * pixrange / valrange) : 0);
    }
}


void scrollbar_repaint(struct window_t *scrollbar_window, int is_active_child)
{
    struct scrollbar_t *sbar = (struct scrollbar_t *)scrollbar_window;
    int tx = to_child_x(scrollbar_window, 0);
    int ty = to_child_y(scrollbar_window, 0);

    gc_fill_rect(scrollbar_window->gc,
                 tx, ty,
                 scrollbar_window->w, scrollbar_window->h,
                 scrollbar_window->bgcolor);

    if(scrollbar_window->type == WINDOW_TYPE_VSCROLL)
    {
        // vertical scrollbar - draw up and down arrows
        gc_blit_bitmap(scrollbar_window->gc, &arrow_up,
                       tx, ty,
                       0, 0, 16, 16);

        gc_blit_bitmap(scrollbar_window->gc, &arrow_down,
                       tx,
                       ty + scrollbar_window->h - 16,
                       0, 0, 16, 16);

        // calculate thumb coordinates and size
        ty += thumb_offset(sbar);

        gc_draw_rect(scrollbar_window->gc, tx, ty, sbar->thumbw, sbar->thumbh,
                     scrollbar_window->fgcolor);
    }
    else
    {
        // horizontal scrollbar - draw left and right arrows
        gc_blit_bitmap(scrollbar_window->gc, &arrow_left,
                       tx, ty,
                       0, 0, 16, 16);

        gc_blit_bitmap(scrollbar_window->gc, &arrow_right,
                       tx + scrollbar_window->w - 16,
                       ty,
                       0, 0, 16, 16);

        // calculate thumb coordinates and size
        tx += thumb_offset(sbar);

        gc_draw_rect(scrollbar_window->gc, tx, ty, sbar->thumbw, sbar->thumbh,
                     scrollbar_window->fgcolor);
    }
}


static void set_slider_val(struct scrollbar_t *sbar, int newval)
{
    struct window_t *scrollbar_window = (struct window_t *)sbar;

    if(newval < sbar->min)
    {
        newval = sbar->min;
    }
    else if(newval > sbar->max)
    {
        newval = sbar->max;
    }

    if(newval != sbar->val)
    {
        sbar->val = newval;

        // Let our parent widget repaint us
        if(sbar->value_change_callback)
        {
            sbar->value_change_callback(scrollbar_window->parent, sbar);
        }
    }
}


void scrollbar_mouseover(struct window_t *scrollbar_window, 
                         struct mouse_state_t *mstate)
{
    struct scrollbar_t *sbar = (struct scrollbar_t *)scrollbar_window;

    if(scrollbar_window->type == WINDOW_TYPE_VSCROLL)
    {
        // Vertical scrollbar
        if(sbar->scrolling)
        {
            // Mouse is dragging the thumb (slider)
            set_slider_val(sbar, vscroll_y_to_val(sbar, mstate->y - 
                                                        sbar->thumbdelta));
        }
        else
        {
            // Normal mouse movement. Check for scrolling via the scrollwheel
            if(mstate->buttons & MOUSE_VSCROLL_DOWN)
            {
                set_slider_val(sbar, sbar->val + (sbar->step * 8));
            }

            if(mstate->buttons & MOUSE_VSCROLL_UP)
            {
                set_slider_val(sbar, sbar->val - (sbar->step * 8));
            }
        }
    }
    else
    {
        // Horizontal scrollbar
        if(sbar->scrolling)
        {
            // Mouse is dragging the thumb (slider)
            set_slider_val(sbar, hscroll_x_to_val(sbar, mstate->x - 
                                                        sbar->thumbdelta));
        }
        else
        {
            // Normal mouse movement. Check for scrolling via the scrollwheel
            if((mstate->buttons & MOUSE_VSCROLL_DOWN) ||
               (mstate->buttons & MOUSE_HSCROLL_RIGHT))
            {
                set_slider_val(sbar, sbar->val + (sbar->step * 8));
            }

            if((mstate->buttons & MOUSE_VSCROLL_UP) ||
               (mstate->buttons & MOUSE_HSCROLL_LEFT))
            {
                set_slider_val(sbar, sbar->val - (sbar->step * 8));
            }
        }
    }
}


void scrollbar_mousedown(struct window_t *scrollbar_window, 
                         struct mouse_state_t *mstate)
{
    struct scrollbar_t *sbar = (struct scrollbar_t *)scrollbar_window;
    int toff;

    if(!mstate->left_pressed)
    {
        return;
    }

    if(scrollbar_window->type == WINDOW_TYPE_VSCROLL)
    {
        // Vertical scrollbar
        if(mstate->y < 16)
        {
            // Mouse down within the up arrow
            set_slider_val(sbar, sbar->val - sbar->step);
        }
        else if(mstate->y >= scrollbar_window->h - 16)
        {
            // Mouse down within the down arrow
            set_slider_val(sbar, sbar->val + sbar->step);
        }
        else
        {
            // Mouse down within the main slider area. Check if it happened
            // inside the thumb (slider)
            toff = thumb_offset(sbar);

            if(mstate->y >= toff && mstate->y < toff + sbar->thumbh)
            {
                sbar->scrolling = 1;
                sbar->thumbdelta = mstate->y - toff;
            }
            else
            {
                sbar->scrolling = 0;
                set_slider_val(sbar, vscroll_y_to_val(sbar, mstate->y));
            }
        }
    }
    else
    {
        // Horizontal scrollbar
        if(mstate->x < 16)
        {
            // Mouse down within the left arrow
            set_slider_val(sbar, sbar->val - sbar->step);
        }
        else if(mstate->x >= scrollbar_window->w - 16)
        {
            // Mouse down within the right arrow
            set_slider_val(sbar, sbar->val + sbar->step);
        }
        else
        {
            // Mouse down within the main slider area. Check if it happened
            // inside the thumb (slider)
            toff = thumb_offset(sbar);

            if(mstate->x >= toff && mstate->x < toff + sbar->thumbw)
            {
                sbar->scrolling = 1;
                sbar->thumbdelta = mstate->x - toff;
            }
            else
            {
                sbar->scrolling = 0;
                set_slider_val(sbar, hscroll_x_to_val(sbar, mstate->x));
            }
        }
    }
}


void scrollbar_mouseexit(struct window_t *scrollbar_window)
{
}

void scrollbar_mouseup(struct window_t *scrollbar_window, 
                       struct mouse_state_t *mstate)
{
    struct scrollbar_t *sbar = (struct scrollbar_t *)scrollbar_window;

    if(!mstate->left_released)
    {
        return;
    }

    sbar->scrolling = 0;
}

void scrollbar_unfocus(struct window_t *scrollbar_window)
{
    if(scrollbar_window->parent->unfocus)
    {
        // pass the event to our parent
        scrollbar_window->parent->unfocus(scrollbar_window->parent);
    }
}


void scrollbar_focus(struct window_t *scrollbar_window)
{
    if(scrollbar_window->parent->focus)
    {
        // pass the event to our parent
        scrollbar_window->parent->focus(scrollbar_window->parent);
    }
}


int scrollbar_keypress(struct window_t *scrollbar_window, 
                       char code, char modifiers)
{
    if(scrollbar_window->parent->keypress)
    {
        // pass the event to our parent
        return scrollbar_window->parent->keypress(scrollbar_window->parent,
                                                  code, modifiers);
    }

    return 0;
}


int scrollbar_keyrelease(struct window_t *scrollbar_window, 
                         char code, char modifiers)
{
    if(scrollbar_window->parent->keyrelease)
    {
        // pass the event to our parent
        return scrollbar_window->parent->keyrelease(scrollbar_window->parent,
                                                    code, modifiers);
    }

    return 0;
}


void scrollbar_disable(struct scrollbar_t *scrollbar)
{
    //struct window_t *scrollbar_window = (struct window_t *)scrollbar;

    if(scrollbar->flags & SCROLLBAR_FLAG_DISABLED)
    {
        return;
    }

    scrollbar->flags |= SCROLLBAR_FLAG_DISABLED;
    //scrollbar_window->repaint(scrollbar_window, IS_ACTIVE_CHILD(scrollbar_window));
    //child_invalidate(scrollbar_window);
}


void scrollbar_enable(struct scrollbar_t *scrollbar)
{
    //struct window_t *scrollbar_window = (struct window_t *)scrollbar;

    if(!(scrollbar->flags & SCROLLBAR_FLAG_DISABLED))
    {
        return;
    }

    scrollbar->flags &= ~SCROLLBAR_FLAG_DISABLED;
    //scrollbar_window->repaint(scrollbar_window, IS_ACTIVE_CHILD(scrollbar_window));
    //child_invalidate(scrollbar_window);
}


void scrollbar_parent_size_changed(struct window_t *parent,
                                   struct window_t *scrollbar_window)
{
    calc_scrollbar_dimensions(parent, scrollbar_window);

    widget_size_changed(scrollbar_window);
}


void scrollbar_set_step(struct scrollbar_t *sbar, int step)
{
    if(step > 0 /* && step < sbar->max */)
    {
        sbar->step = step;
    }
}


void scrollbar_set_max(struct scrollbar_t *sbar, int max)
{
    sbar->max = max;
}


void scrollbar_set_min(struct scrollbar_t *sbar, int min)
{
    sbar->min = min;
}


void scrollbar_set_val(struct scrollbar_t *sbar, int val)
{
    if(val >= sbar->min && val <= sbar->max)
    {
        sbar->val = val;
    }
}


static inline
void color_from_template(uint32_t *array, uint32_t *template, int index)
{
    if(template[index] == TEMPLATE_BGCOLOR)
    {
        array[index] = GLOB.themecolor[THEME_COLOR_SCROLLBAR_BGCOLOR];
    }
    else if(template[index] == TEMPLATE_TEXTCOLOR)
    {
        array[index] = GLOB.themecolor[THEME_COLOR_SCROLLBAR_TEXTCOLOR];
    }
    else
    {
        array[index] = template[index];
    }
}


/*
 * Called on startup and when the system color theme changes.
 * Updates the global arrow bitmaps.
 */
void scrollbar_theme_changed_global(void)
{
    int i, j, k;

    for(i = 0, k = 0; i < 16; i++)
    {
        for(j = 0; j < 16; j++, k++)
        {
            color_from_template(arrow_up_img, arrow_up_img_template, k);
            color_from_template(arrow_down_img, arrow_down_img_template, k);
            color_from_template(arrow_left_img, arrow_left_img_template, k);
            color_from_template(arrow_right_img, arrow_right_img_template, k);
        }
    }
}


/*
 * Called when the system color theme changes.
 * Updates the widget's colors.
 */
void scrollbar_theme_changed(struct window_t *window)
{
    window->bgcolor = GLOB.themecolor[THEME_COLOR_SCROLLBAR_BGCOLOR];
    window->fgcolor = GLOB.themecolor[THEME_COLOR_SCROLLBAR_TEXTCOLOR];
}

