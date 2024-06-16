/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: statusbar.c
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
 *  \file statusbar.c
 *
 *  The implementation of a statusbar widget.
 */

#include <stdlib.h>
#include <string.h>
#include "../include/client/statusbar.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"


struct statusbar_t *statusbar_new(struct gc_t *gc, struct window_t *parent)
{
    struct statusbar_t *sbar;
    Rect *rect;
    int w, y;

    if(!(sbar = malloc(sizeof(struct statusbar_t))))
    {
        return NULL;
    }

    memset(sbar, 0, sizeof(struct statusbar_t));

    if(!(sbar->window.clip_rects = RectList_new()))
    {
        free(sbar);
        return NULL;
    }
    
    y = parent->h - STATUSBAR_HEIGHT;
    w = parent->w;

    if(!(rect = Rect_new(y, 0, y + STATUSBAR_HEIGHT - 1,  w - 1)))
    {
        RectList_free(sbar->window.clip_rects);
        free(sbar);
        return NULL;
    }

    RectList_add(sbar->window.clip_rects, rect);
    sbar->window.type = WINDOW_TYPE_STATUSBAR;
    sbar->window.x = 0;
    sbar->window.y = y;
    sbar->window.w = w;
    sbar->window.h = STATUSBAR_HEIGHT;
    sbar->window.gc = gc;
    sbar->window.flags = WINDOW_NODECORATION;
    sbar->window.visible = 1;
    sbar->window.bgcolor = STATUSBAR_BGCOLOR;
    sbar->window.fgcolor = STATUSBAR_TEXTCOLOR;
    
    sbar->window.repaint = statusbar_repaint;
    sbar->window.mousedown = statusbar_mousedown;
    sbar->window.mouseover = statusbar_mouseover;
    sbar->window.mouseup = statusbar_mouseup;
    sbar->window.mouseexit = statusbar_mouseexit;
    sbar->window.unfocus = statusbar_unfocus;
    sbar->window.focus = statusbar_focus;
    sbar->window.destroy = statusbar_destroy;
    sbar->window.size_changed = statusbar_size_changed;

    window_insert_child(parent, (struct window_t *)sbar);

    return sbar;
}


void statusbar_destroy(struct window_t *statusbar_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(statusbar_window);
}


void statusbar_repaint(struct window_t *statusbar_window, int is_active_child)
{
    int x = to_child_x(statusbar_window, 0);
    int y = to_child_y(statusbar_window, 0);

    // background
    gc_fill_rect(statusbar_window->gc, x, y,
                 statusbar_window->w - 1, statusbar_window->h - 1,
                 statusbar_window->bgcolor);

    // border
    gc_horizontal_line(statusbar_window->gc, x, y,
                            statusbar_window->w, GLOBAL_DARK_SIDE_COLOR);
    gc_vertical_line(statusbar_window->gc, x, y,
                            statusbar_window->h, GLOBAL_DARK_SIDE_COLOR);
    gc_horizontal_line(statusbar_window->gc, x, y + statusbar_window->h - 1,
                            statusbar_window->w, GLOBAL_LIGHT_SIDE_COLOR);
    gc_vertical_line(statusbar_window->gc, x + statusbar_window->w - 1, y,
                            statusbar_window->h, GLOBAL_LIGHT_SIDE_COLOR);

    //Draw the text
    if(statusbar_window->title)
    {
        statusbar_window->gc->clipping.clip_rects = statusbar_window->clip_rects;
        gc_draw_text(statusbar_window->gc,
                             statusbar_window->title, 4,
                             to_child_y(statusbar_window, 4),
                             statusbar_window->fgcolor, 0);
        statusbar_window->gc->clipping.clip_rects = NULL;
    }
}


void statusbar_append_text(struct window_t *statusbar_window, char *addstr)
{
    widget_append_text(statusbar_window, addstr);
    statusbar_window->repaint(statusbar_window, IS_ACTIVE_CHILD(statusbar_window));
    child_invalidate(statusbar_window);
}


void statusbar_set_text(struct window_t *statusbar_window, char *new_title)
{
    __window_set_title(statusbar_window, new_title, 0);
    statusbar_window->repaint(statusbar_window, IS_ACTIVE_CHILD(statusbar_window));
    child_invalidate(statusbar_window);
}


void statusbar_mouseover(struct window_t *statusbar_window, 
                         struct mouse_state_t *mstate)
{
}


void statusbar_mousedown(struct window_t *statusbar_window, 
                         struct mouse_state_t *mstate)
{
}


void statusbar_mouseexit(struct window_t *statusbar_window)
{
}

void statusbar_mouseup(struct window_t *statusbar_window, 
                       struct mouse_state_t *mstate)
{
}

void statusbar_unfocus(struct window_t *statusbar_window)
{
}


void statusbar_focus(struct window_t *statusbar_window)
{
}


void statusbar_size_changed(struct window_t *statusbar_window)
{
    statusbar_window->w = statusbar_window->parent->w;
    widget_size_changed(statusbar_window);
}

