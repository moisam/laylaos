/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: textbox.c
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
 *  \file textbox.c
 *
 *  The implementation of a textbox widget.
 */

#include <stdlib.h>
#include <string.h>
#include "../include/client/textbox.h"
#include "../include/cursor.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"

#include "inlines.c"

#define GLOB        __global_gui_data


struct textbox_t *textbox_new(struct gc_t *gc, struct window_t *parent,
                                               int x, int y, int w, int h,
                                               char *title)
{
    struct textbox_t *text_box;
    Rect *rect;

    if(!(text_box = malloc(sizeof(struct textbox_t))))
    {
        return NULL;
    }

    memset(text_box, 0, sizeof(struct textbox_t));

    if(!(text_box->window.clip_rects = RectList_new()))
    {
        free(text_box);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y, x, y + h - 1,  x + w - 1)))
    {
        RectList_free(text_box->window.clip_rects);
        free(text_box);
        return NULL;
    }

    RectList_add(text_box->window.clip_rects, rect);
    text_box->window.type = WINDOW_TYPE_TEXTBOX;
    text_box->window.x = x;
    text_box->window.y = y;
    text_box->window.w = w;
    text_box->window.h = h;
    text_box->window.gc = gc;
    text_box->window.flags = WINDOW_NODECORATION | WINDOW_3D_WIDGET;
    text_box->window.visible = 1;
    text_box->window.bgcolor = GLOB.themecolor[THEME_COLOR_TEXTBOX_BGCOLOR];
    text_box->window.fgcolor = GLOB.themecolor[THEME_COLOR_TEXTBOX_TEXTCOLOR];
    
    if(title)
    {
        __window_set_title(&text_box->window, title, 0);
    }

    //text_box->window.paint_function = textbox_paint;
    text_box->window.repaint = textbox_repaint;
    text_box->window.mousedown = textbox_mousedown;
    text_box->window.mouseover = textbox_mouseover;
    text_box->window.mouseup = textbox_mouseup;
    text_box->window.mouseexit = textbox_mouseexit;
    text_box->window.unfocus = textbox_unfocus;
    text_box->window.focus = textbox_focus;
    text_box->window.destroy = textbox_destroy;
    text_box->window.size_changed = widget_size_changed;
    text_box->window.theme_changed = textbox_theme_changed;

    window_insert_child(parent, (struct window_t *)text_box);

    return text_box;
}


void textbox_destroy(struct window_t *textbox_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(textbox_window);
}


void textbox_repaint(struct window_t *textbox_window, int is_active_child)
{
    int title_len;
    int x = to_child_x(textbox_window, 0);
    int y = to_child_y(textbox_window, 0);
    int charh = char_height(textbox_window->gc->font, ' ');

    //White background
    gc_fill_rect(textbox_window->gc,
                 x + 1, y + 1,
                 textbox_window->w - 2, textbox_window->h - 2,
                 textbox_window->bgcolor /* 0xFFFFFFFF */);

    //Draw the title centered within the button
    if(textbox_window->title)
    {
        title_len = string_width(textbox_window->gc->font, textbox_window->title);

        textbox_window->gc->clipping.clip_rects = textbox_window->clip_rects;
        gc_draw_text(textbox_window->gc, textbox_window->title,
                              x + ((textbox_window->w - title_len) / 2),
                              y + ((textbox_window->h - charh) / 2),
                              textbox_window->fgcolor /* 0x000000FF */, 0);
        textbox_window->gc->clipping.clip_rects = NULL;
    }

    // border last - to ensure no text overlaps it
    draw_inverted_3d_border(textbox_window->gc, x, y, 
                            textbox_window->w, textbox_window->h);
}


void textbox_append_text(struct window_t *textbox_window, char *addstr)
{
    widget_append_text(textbox_window, addstr);
    textbox_window->repaint(textbox_window, IS_ACTIVE_CHILD(textbox_window));
    child_invalidate(textbox_window);
}


void textbox_set_text(struct window_t *textbox_window, char *new_title)
{
    __window_set_title(textbox_window, new_title, 0);
    //child_repaint(textbox_window);
    textbox_window->repaint(textbox_window, IS_ACTIVE_CHILD(textbox_window));
    child_invalidate(textbox_window);
}


void textbox_mouseover(struct window_t *textbox_window, 
                       struct mouse_state_t *mstate)
{
}


void textbox_mousedown(struct window_t *textbox_window, 
                       struct mouse_state_t *mstate)
{
}


void textbox_mouseexit(struct window_t *textbox_window)
{
}

void textbox_mouseup(struct window_t *textbox_window, 
                     struct mouse_state_t *mstate)
{
}

void textbox_unfocus(struct window_t *textbox_window)
{
}


void textbox_focus(struct window_t *textbox_window)
{
}


/*
 * Called when the system color theme changes.
 * Updates the widget's colors.
 */
void textbox_theme_changed(struct window_t *window)
{
    window->bgcolor = GLOB.themecolor[THEME_COLOR_TEXTBOX_BGCOLOR];
    window->fgcolor = GLOB.themecolor[THEME_COLOR_TEXTBOX_TEXTCOLOR];
}

