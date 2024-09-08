/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: label.c
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
 *  \file label.c
 *
 *  The implementation of a label widget.
 */

#include <stdlib.h>
#include <string.h>
#include "../include/client/label.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"

#define GLOB                        __global_gui_data


struct label_t *label_new(struct gc_t *gc, struct window_t *parent,
                                           int x, int y, int w, int h,
                                           char *title)
{
    struct label_t *label;
    Rect *rect;

    if(!(label = malloc(sizeof(struct label_t))))
    {
        return NULL;
    }

    memset(label, 0, sizeof(struct label_t));

    if(!(label->window.clip_rects = RectList_new()))
    {
        free(label);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y, x, y + h - 1,  x + w - 1)))
    {
        RectList_free(label->window.clip_rects);
        free(label);
        return NULL;
    }

    RectList_add(label->window.clip_rects, rect);
    label->window.type = WINDOW_TYPE_LABEL;
    label->window.x = x;
    label->window.y = y;
    label->window.w = w;
    label->window.h = h;
    label->window.gc = gc;
    label->window.flags = WINDOW_NODECORATION;
    label->window.visible = 1;
    label->window.bgcolor = GLOB.themecolor[THEME_COLOR_WINDOW_BGCOLOR];
    label->window.fgcolor = GLOBAL_BLACK_COLOR;

    if(title)
    {
        char **lines;
        int i = 0, line_count = 1;
        char *s, *nl;
        
        // make a copy of the title as we will convert newlines to '\0'
        label->window.title = strdup(title);
        s = label->window.title;
        
        // find the number of lines
        while(*s && (nl = strchr(s, '\n')))
        {
            line_count++;
            s = nl + 1;
        }
        
        // create an array to store pointers to line heads
        if(!(lines = malloc((line_count + 1) * sizeof(char *))))
        {
            Rect_free(label->window.clip_rects->root);
            RectList_free(label->window.clip_rects);
            free(label);
            return NULL;
        }

        s = label->window.title;
        lines[i++] = s;
        
        while(*s && (nl = strchr(s, '\n')))
        {
            *nl = '\0';
            s = nl + 1;
            lines[i++] = s;
        }
        
        // NULL-terminate the array
        lines[i] = NULL;
        
        // and store it in our label struct
        label->internal_data = lines;
    }

    label->window.repaint = label_repaint;
    label->window.mousedown = label_mousedown;
    label->window.mouseover = label_mouseover;
    label->window.mouseup = label_mouseup;
    label->window.mouseexit = label_mouseexit;
    label->window.unfocus = label_unfocus;
    label->window.focus = label_focus;
    label->window.destroy = label_destroy;
    label->window.size_changed = widget_size_changed;
    label->window.theme_changed = label_theme_changed;

    //label->set_text = label_set_text;
    label->window.text_alignment = TEXT_ALIGN_TOP | TEXT_ALIGN_LEFT;

    window_insert_child(parent, (struct window_t *)label);

    return label;
}


void label_destroy(struct window_t *label_window)
{
    if(label_window->internal_data)
    {
        free(label_window->internal_data);
    }

    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(label_window);
}


void label_repaint(struct window_t *label_window, int is_active_child)
{
    int x, y, line_count, fontsz;
    char **lines;
    struct font_t *font = label_window->gc->font;
    int charh = char_height(font, ' ');
    struct label_t *label = (struct label_t *)label_window;

    // Fill the background
    gc_fill_rect(label_window->gc,
                 to_child_x(label_window, 0), to_child_y(label_window, 0),
                 label_window->w, label_window->h,
                 label_window->bgcolor);

    // Draw the title text
    if(!label->internal_data)
    {
        return;
    }

    lines = label->internal_data;
    line_count = 0;

    while(*lines)
    {
        line_count++;
        lines++;
    }

    // calculate the first line's y position
    if(label_window->text_alignment & TEXT_ALIGN_BOTTOM)
    {
        y = to_child_y(label_window,
                    (label_window->h - (line_count * charh)));
    }
    else if(label_window->text_alignment & TEXT_ALIGN_CENTERV)
    {
        y = to_child_y(label_window,
                    (label_window->h - (line_count * charh)) / 2);
    }
    else
    {
        y = to_child_y(label_window, 0);
    }

    struct clipping_t saved_clipping;
    struct clipping_t new_clipping = { label_window->clip_rects, 1 };
    gc_get_clipping(label_window->gc, &saved_clipping);
    gc_set_clipping(label_window->gc, &new_clipping);

    // ensure other drawing routines have not messed our preferred size
    lock_font(label_window->gc->font);
    fontsz = gc_get_fontsize(label_window->gc);
    gc_set_fontsize(label_window->gc, 16);

    lines = label->internal_data;

    while(*lines)
    {
        // calculate the line's x position
        if(label_window->text_alignment & TEXT_ALIGN_RIGHT)
        {
            x = to_child_x(label_window,
                            (label_window->w -
                                string_width(font, *lines)));
        }
        else if(label_window->text_alignment & TEXT_ALIGN_CENTERH)
        {
            x = to_child_x(label_window,
                            (label_window->w -
                                string_width(font, *lines)) / 2);
        }
        else
        {
            x = to_child_x(label_window, 0);
        }

        gc_draw_text(label_window->gc,
                             *lines,
                             x, y, label_window->fgcolor, 0);

        y += charh;
        lines++;
    }

    // restore the original font size
    gc_set_fontsize(label_window->gc, fontsz);
    unlock_font(label_window->gc->font);

    gc_set_clipping(label_window->gc, &saved_clipping);
}


void label_set_text(struct label_t *label, char *new_title)
{
    struct window_t *label_window = (struct window_t *)label;
    __window_set_title(label_window, new_title, 0);
    label_window->repaint(label_window, IS_ACTIVE_CHILD(label_window));
    child_invalidate(label_window);
}


void label_mouseover(struct window_t *label_window, 
                     struct mouse_state_t *mstate)
{
}


void label_mousedown(struct window_t *label_window, 
                     struct mouse_state_t *mstate)
{
}


void label_mouseexit(struct window_t *label_window)
{
}

void label_mouseup(struct window_t *label_window, 
                   struct mouse_state_t *mstate)
{
}

void label_unfocus(struct window_t *label_window)
{
}


void label_focus(struct window_t *label_window)
{
}


void label_set_text_alignment(struct label_t *label, int alignment)
{
    if(!label)
    {
        return;
    }

    //widget_set_text_alignment((struct window_t *)label, alignment);
    label->window.text_alignment = alignment;
}


void label_set_foreground(struct label_t *label, uint32_t color)
{
    if(!label)
    {
        return;
    }

    label->window.fgcolor = color;
}


void label_set_background(struct label_t *label, uint32_t color)
{
    if(!label)
    {
        return;
    }

    label->window.bgcolor = color;
}


/*
 * Called when the system color theme changes.
 * Updates the widget's colors.
 */
void label_theme_changed(struct window_t *window)
{
    window->bgcolor = GLOB.themecolor[THEME_COLOR_WINDOW_BGCOLOR];
}

