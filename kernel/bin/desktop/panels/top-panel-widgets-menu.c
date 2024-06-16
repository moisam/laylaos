/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: top-panel-widgets-menu.c
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
 *  \file top-panel-widgets-menu.c
 *
 *  Functions to display, paint and hide menu frames displayed by top
 *  panel widgets.
 */

#include "../include/panels/widget.h"
#include "../include/client/window.h"

#include "../client/inlines.c"

#define GLOB                        __global_gui_data


void widget_menu_show(struct widget_t *widget)
{
    int x, y;
    
    if(!widget || !widget->menu)
    {
        return;
    }
    
    // center the menu below the widget
    x = widget->win.x + (widget->win.w - widget->menu->w);
    y = widget_height;
    
    // ensure the menu doesn't fall off the right edge of the screen
    if((x + widget->menu->w) > main_window->w)
    {
        x = main_window->w - widget->menu->w;
    }
    
    // or the left edge!
    if(x < 0)
    {
        x = 0;
    }
    
    window_set_pos(widget->menu, x, y);
    window_show(widget->menu);
}


void widget_menu_hide(struct widget_t *widget)
{
    if(!widget || !widget->menu)
    {
        return;
    }
    
    if(!(widget->menu->flags & WINDOW_HIDDEN))
    {
        window_hide(widget->menu);
    }
}


void widget_menu_fill_background(struct window_t *frame)
{
    gc_fill_rect(frame->gc, 0, 0, frame->w, frame->h, TOPPANEL_BGCOLOR);
}


void widget_menu_fill_rect(struct window_t *frame, int x, int y, 
                           int w, int h, uint32_t color)
{
    gc_fill_rect(frame->gc, x, y, w, h, color);
}


void widget_menu_draw_text(struct window_t *frame, char *buf,
                           int x, int y, uint32_t color)
{
    gc_draw_text(frame->gc, buf, x, y, color, 0);
}


void widget_menu_repaint_dummy(struct window_t *frame, int unused)
{
    (void)unused;
    
    widget_menu_fill_background(frame);
}


struct window_t *widget_menu_create(int w, int h)
{
    struct window_t *frame;
    struct window_attribs_t attribs;

    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = w;
    attribs.h = h;
    //attribs.flags = 0;      // server will set the appropriate flags
    attribs.flags = WINDOW_NODECORATION | WINDOW_SKIPTASKBAR;

    if(!(frame = window_create(&attribs)))
    //if(!(frame = __window_create(&attribs, WINDOW_TYPE_MENU_FRAME, main_window->winid)))
    {
        return NULL;
    }

    frame->repaint = widget_menu_repaint_dummy;
    //frame->owner_winid = main_window->winid;

    gc_set_font(frame->gc, GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono);
    
    return frame;
}


void widget_menu_may_hide(winid_t winid)
{
    struct widget_t *widget;
    ListNode *current_node;

    for(current_node = main_window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        widget = (struct widget_t *)current_node->payload;

        if(widget->menu && widget->menu->winid == winid)
        {
            widget_menu_hide(widget);
            focused_widget = NULL;
            break;
        }
    }
}


void widget_menu_invalidate(struct window_t *frame)
{
    window_invalidate(frame);
}


uint32_t widget_menu_bg_color(void)
{
    return widget_colors[0].bg;
}


uint32_t widget_menu_fg_color(void)
{
    return widget_colors[0].text;
}

uint32_t widget_menu_hi_color(void)
{
    return TOPPANEL_HICOLOR;
}

