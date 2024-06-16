/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: group-border.c
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
 *  \file group-border.c
 *
 *  The implementation of a group border widget.
 */

#include <string.h>
#include "../include/client/group-border.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"

struct group_border_t *group_border_new(struct gc_t *gc,
                                        struct window_t *parent,
                                        int x, int y, int w, int h,
                                        char *title)
{
    struct group_border_t *gb;
    Rect *rect;

    if(!(gb = malloc(sizeof(struct group_border_t))))
    {
        return NULL;
    }

    memset(gb, 0, sizeof(struct group_border_t));
    
    if(!(gb->window.clip_rects = RectList_new()))
    {
        free(gb);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y, x, y + h - 1,  x + w - 1)))
    {
        RectList_free(gb->window.clip_rects);
        free(gb);
        return NULL;
    }

    RectList_add(gb->window.clip_rects, rect);
    gb->window.type = WINDOW_TYPE_GROUP_BORDER;
    gb->window.x = x;
    gb->window.y = y;
    gb->window.w = w;
    gb->window.h = h;
    gb->window.gc = gc;
    gb->window.flags = WINDOW_NODECORATION;
    gb->window.visible = 1;
    gb->window.bgcolor = GROUP_BORDER_BGCOLOR;
    gb->window.fgcolor = GROUP_BORDER_TEXTCOLOR;
    
    if(title)
    {
        __window_set_title(&gb->window, title, 0);
    }

    gb->window.repaint = group_border_repaint;
    gb->window.mousedown = group_border_mousedown;
    gb->window.mouseover = group_border_mouseover;
    gb->window.mouseup = group_border_mouseup;
    gb->window.mouseexit = group_border_mouseexit;
    gb->window.unfocus = group_border_unfocus;
    gb->window.focus = group_border_focus;
    gb->window.destroy = group_border_destroy;
    gb->window.size_changed = widget_size_changed;

    window_insert_child(parent, (struct window_t *)gb);

    return gb;
}


void group_border_destroy(struct window_t *gb_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(gb_window);
}


void group_border_repaint(struct window_t *gb_window, int is_active_child)
{
    int title_len;
    int x = to_child_x(gb_window, 0);
    int y = to_child_y(gb_window, 0);
    int w = gb_window->w;
    int h = gb_window->h;
    int charh = char_height(gb_window->gc->font, ' ');
    int halfh = charh / 2;

    // default background
    gc_fill_rect(gb_window->gc,
                 x, y,
                 gb_window->w - 1, gb_window->h - 1,
                 gb_window->bgcolor);

    // "3D" border
    
    // top
    gc_horizontal_line(gb_window->gc, x, y + halfh, w, GLOBAL_BLACK_COLOR);
    gc_horizontal_line(gb_window->gc, x, y + halfh + 1, w, GLOBAL_WHITE_COLOR);

    // left
    gc_vertical_line(gb_window->gc, x, y + halfh, h - halfh, GLOBAL_BLACK_COLOR);
    gc_vertical_line(gb_window->gc, x + 1, y + halfh + 1, h - halfh - 1, 
                                                        GLOBAL_WHITE_COLOR);

    // bottom
    gc_horizontal_line(gb_window->gc, x, y + h - 2, w, GLOBAL_BLACK_COLOR);
    gc_horizontal_line(gb_window->gc, x, y + h - 1, w, GLOBAL_WHITE_COLOR);

    // right
    gc_vertical_line(gb_window->gc, x + w - 2, y + halfh, h - halfh - 1, 
                                                        GLOBAL_BLACK_COLOR);
    gc_vertical_line(gb_window->gc, x + w - 1, y + halfh, h - halfh, 
                                                        GLOBAL_WHITE_COLOR);


    if(gb_window->title)
    {
        // Get the title length
        title_len = string_width(gb_window->gc->font, 
                                 gb_window->title);

        // Default background
        gc_fill_rect(gb_window->gc,
                     x + 6, y,
                     title_len + 4 - 1, charh,
                     gb_window->bgcolor);

        // Draw the title
        struct clipping_t saved_clipping;
        struct clipping_t new_clipping = { gb_window->clip_rects, 1 };
        gc_get_clipping(gb_window->gc, &saved_clipping);
        gc_set_clipping(gb_window->gc, &new_clipping);

        gc_draw_text(gb_window->gc, gb_window->title,
                                x + 8,
                                y,
                                gb_window->fgcolor, 0);
        gc_set_clipping(gb_window->gc, &saved_clipping);
    }
}


void group_border_mouseover(struct window_t *gb_window, 
                            struct mouse_state_t *mstate)
{
}


void group_border_mousedown(struct window_t *gb_window, 
                            struct mouse_state_t *mstate)
{
}


void group_border_mouseexit(struct window_t *gb_window)
{
}


void group_border_mouseup(struct window_t *gb_window, 
                          struct mouse_state_t *mstate)
{
}


void group_border_unfocus(struct window_t *gb_window)
{
}


void group_border_focus(struct window_t *gb_window)
{
}


void group_border_set_title(struct group_border_t *gb, char *new_title)
{
    __window_set_title(&gb->window, new_title, 0);
}

