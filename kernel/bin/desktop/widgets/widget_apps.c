/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: widget_apps.c
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
 *  \file widget_apps.c
 *
 *  The applications menu widget. This is shown in the top panel and allows
 *  the user to launch an application.
 */

#include <string.h>
#include "../include/panels/widget.h"

// define a custom flag for us to use
#define APPLICATION_FLAG_SELECTED       0x0100

#define MAX_CATEGORIES                  64

struct widget_t *apps_widget;
static char **app_categories;
static int app_category_count;
static struct app_entry_t *category_first_entry[MAX_CATEGORIES];
static struct app_entry_t *category_last_entry[MAX_CATEGORIES];
static int category_selected = 0;
static int mouse_down = 0;


/*
 * Repaint the widget itself.
 */
static void widget_repaint_apps(struct window_t *widget_win, 
                                int is_active_child)
{
    struct widget_t *widget = (struct widget_t *)widget_win;
    size_t len;
    static char *title = "Applications";
    int x, y;

    widget_fill_background(widget);

    len = widget_string_width(title);

    x = (widget_win->w / 2) - (len / 2);
    y = (widget_win->h / 2) - (widget_char_height() / 2);

    if(x < 0)
    {
        x = 0;
    }

    // Draw the title centered within the widget
    widget_draw_text(widget, title, x, y, widget_fg_color(widget));
}


#define TOP_FRAME_PADDING   (10)
#define LEFT_ITEM_PADDING   (6)
#define TOP_ITEM_PADDING    (8)
#define LEFT_COLUMN_X       (2)
#define LEFT_COLUMN_W       (16 * 8 /* charw */)
#define LEFT_COLUMN_H       (charh + 16)
#define RIGHT_COLUMN_X      (LEFT_COLUMN_W + 2)
#define RIGHT_COLUMN_W      (frame->w - RIGHT_COLUMN_X - 2)
#define RIGHT_COLUMN_H      (charh + 16)


static inline void draw_app_category(struct window_t *frame, int i, int y,
                                     int charh,
                                     uint32_t textcolor, uint32_t bgcolor,
                                     uint32_t hicolor)
{
    if(category_selected == i)
    {
        // draw a border with 4 pixels of padding around the text,
        // assuming the longest category name to be < 16 chars
        widget_menu_fill_rect(frame, LEFT_COLUMN_X, y - TOP_ITEM_PADDING,
                                     LEFT_COLUMN_W, LEFT_COLUMN_H, hicolor);
    }
    else
    {
        widget_menu_fill_rect(frame, LEFT_COLUMN_X, y - TOP_ITEM_PADDING,
                                     LEFT_COLUMN_W, LEFT_COLUMN_H, bgcolor);
    }

    // pad text: 6 pixels from frame left, 6 pixels from frame top
    widget_menu_draw_text(frame, app_categories[i], 
                          LEFT_COLUMN_X + LEFT_ITEM_PADDING, y, textcolor);
}


static inline void draw_app_entry(struct window_t *frame,
                                  struct app_entry_t *entry,
                                  int y, int charh,
                                  uint32_t textcolor, uint32_t bgcolor,
                                  uint32_t hicolor)
{
    if(entry->flags & APPLICATION_FLAG_SELECTED)
    {
        // draw a border with 4 pixels of padding around the text,
        // assuming the longest category name to be < 16 chars
        widget_menu_fill_rect(frame, RIGHT_COLUMN_X, y - TOP_ITEM_PADDING, 
                                     RIGHT_COLUMN_W, RIGHT_COLUMN_H, hicolor);
    }
    else
    {
        widget_menu_fill_rect(frame, RIGHT_COLUMN_X, y - TOP_ITEM_PADDING, 
                                     RIGHT_COLUMN_W, RIGHT_COLUMN_H, bgcolor);
    }

    // pad text: 6 pixels from frame left, 6 pixels from frame top
    widget_menu_draw_text(frame, entry->name, 
                          RIGHT_COLUMN_X + LEFT_ITEM_PADDING, y, textcolor);
}


static void paint_category_apps(struct window_t *frame,
                                  int i, int charh,
                                  uint32_t textcolor, uint32_t bgcolor,
                                  uint32_t hicolor)
{
	struct app_entry_t *entry;
    int y = TOP_FRAME_PADDING;

    widget_menu_fill_rect(frame, RIGHT_COLUMN_X, y, 
                                 RIGHT_COLUMN_W, frame->h - y, bgcolor);

    for(entry = category_first_entry[i]; entry != NULL; entry = entry->next)
    {
        draw_app_entry(frame, entry, y, charh,
                              textcolor, bgcolor, hicolor);
        y += RIGHT_COLUMN_H;
    }
}


/*
 * Repaint the widget's menu frame.
 */
static void widget_menu_repaint_applist(struct window_t *frame, int unused)
{
    (void)unused;

    int y = TOP_FRAME_PADDING;
	int i;
	int charh = widget_char_height();
	uint32_t textcolor = widget_menu_fg_color();
	uint32_t hicolor = widget_menu_hi_color();
	uint32_t bgcolor = widget_menu_bg_color();
    
    for(i = 0; i < app_category_count; i++)
    {
        draw_app_category(frame, i, y, charh, textcolor, bgcolor, hicolor);

        // draw app names on the right column
        if(category_selected == i)
        {
            paint_category_apps(frame, i, charh, textcolor, bgcolor, hicolor);
        }

        y += LEFT_COLUMN_H;
    }
}


static void applist_mouseover(struct window_t *frame,
                              struct mouse_state_t *mstate)
{
    int i, j, k;
    int y = TOP_FRAME_PADDING;
	int charh = widget_char_height();
	uint32_t textcolor = widget_menu_fg_color();
	uint32_t hicolor = widget_menu_hi_color();
	uint32_t bgcolor = widget_menu_bg_color();

    // find it the mouse is in the left or right column
    if(mstate->x < RIGHT_COLUMN_X)
    {
        // left column

        // find the index of the menu item under the mouse
        j = (mstate->y - (TOP_FRAME_PADDING - TOP_ITEM_PADDING)) / LEFT_COLUMN_H;

        if(j < app_category_count)
        {
            // de-select any previous selection
            for(k = j, i = 0; i < app_category_count; i++)
            {
                if(category_selected == i && i != j)
                {
                    k = i;
                    category_selected = -1;
                    draw_app_category(frame, i, y, charh, 
                                      textcolor, bgcolor, hicolor);
                }
    
                y += LEFT_COLUMN_H;
            }

            // select the new category if it is valid
            y = TOP_FRAME_PADDING + (j * LEFT_COLUMN_H);
            category_selected = j;
            draw_app_category(frame, j, y, charh, textcolor, bgcolor, hicolor);

            // and redraw the right side if we changed category
            if(k != j)
            {
                paint_category_apps(frame, j, charh, textcolor, bgcolor, hicolor);
            }
        }
    }
    else if(mstate->x < frame->w)
    {
        // right column
    	struct app_entry_t *entry, *old_sel = NULL;

        // find the selected category from the left column
        i = category_selected;

        // find the index of the menu item under the mouse
        j = (mstate->y - (TOP_FRAME_PADDING - TOP_ITEM_PADDING)) / RIGHT_COLUMN_H;
        k = 0;

        // find the old selection
        for(entry = category_first_entry[i]; entry != NULL; entry = entry->next)
        {
            if(entry->flags & APPLICATION_FLAG_SELECTED)
            {
                old_sel = entry;
                break;
            }

            k++;
        }

        // find the new selection
        for(entry = category_first_entry[i]; entry != NULL; entry = entry->next)
        {
            if(j == 0)
            {
                // this is the one - select it
                entry->flags |= APPLICATION_FLAG_SELECTED;
                draw_app_entry(frame, entry, y, charh,
                               textcolor, bgcolor, hicolor);

                // deselect the old selection, if any
                if(old_sel && old_sel != entry)
                {
                    y = TOP_FRAME_PADDING + (k * RIGHT_COLUMN_H);
                    old_sel->flags &= ~APPLICATION_FLAG_SELECTED;
                    draw_app_entry(frame, old_sel, y, charh,
                                   textcolor, bgcolor, hicolor);
                }

                break;
            }

            j--;
            y += RIGHT_COLUMN_H;
        }
    }

    widget_menu_invalidate(frame);
}


static void applist_mousedown(struct window_t *frame,
                              struct mouse_state_t *mstate)
{
    // don't care if mouse is pressed anywhere but the right column
    if(mstate->x < RIGHT_COLUMN_X || mstate->x >= frame->w)
    {
        return;
    }

    mouse_down = 1;
}


static void applist_mouseup(struct window_t *frame,
                            struct mouse_state_t *mstate)
{
    struct app_entry_t *entry, *sel = NULL;
    int i, j;
	int charh = widget_char_height();

    // don't care if mouse is released anywhere but the right column
    if(mstate->x < RIGHT_COLUMN_X || mstate->x >= frame->w)
    {
        return;
    }

    // find the selected category from the left column
    i = category_selected;

    // find the index of the menu item under the mouse
    j = (mstate->y - (TOP_FRAME_PADDING - TOP_ITEM_PADDING)) / RIGHT_COLUMN_H;

    for(entry = category_first_entry[i]; entry != NULL; entry = entry->next)
    {
        if(j == 0)
        {
            // this is the one - run the application
            sel = entry;
            break;
        }
        else
        {
            // we are still not there
            j--;
        }
    }

    mouse_down = 0;

    // only hide the menu if we have a selection
    if(sel)
    {
        widget_menu_hide(apps_widget);
        widget_unfocus((struct window_t *)apps_widget);
        widget_run_command(sel->command);
    }
}


/*
 * Handle mouse up events on the widget itself.
 * This toggles showing/hiding the widget's menu frame.
 */
static void widget_mouseup_apps(struct widget_t *widget, 
                                int mouse_x, int mouse_y)
{
    // toggle showing/hiding the panel menu
    if(widget->menu)
    {
        if((widget->menu->flags & WINDOW_HIDDEN))
        {
            widget_menu_show(widget);
        }
        else
        {
            widget_menu_hide(widget);
        }
    }
    else
    {
        struct window_t *menu;
        int charh = widget_char_height();
        int w = (60 * 8 /* charw */);
        int h = (20 * charh);
        
        if((menu = widget_menu_create(w, h)))
        {
            category_selected = 0;
            menu->repaint = widget_menu_repaint_applist;
            menu->mouseover = applist_mouseover;
            menu->mousedown = applist_mousedown;
            menu->mouseup = applist_mouseup;
            widget->menu = menu;
            widget_menu_fill_background(menu);
            widget_menu_repaint_applist(menu, 0);
            widget_menu_show(widget);
        }
    }
}


int widget_init_apps(void)
{
	struct app_entry_t *entry, *next;
    struct app_entry_t *global_first_entry = NULL;
    int i;

    if(!(apps_widget = widget_create()))
    {
        return 0;
    }

    apps_widget->win.w = 160;
    apps_widget->win.repaint = widget_repaint_apps;
    apps_widget->win.title = "Applications";
    apps_widget->button_click_callback = widget_mouseup_apps;
    apps_widget->flags |= (WIDGET_FLAG_INITIALIZED | WIDGET_FLAG_FLOAT_LEFT);

    widget_get_app_categories(&app_categories, &app_category_count);
    widget_get_app_entries(&global_first_entry);

    // sort entries into their different categories
    for(i = 0; i < app_category_count; i++)
    {
        for(entry = global_first_entry; entry != NULL; entry = next)
        {
            next = entry->next;

            if(entry->category != i)
            {
                continue;
            }

            if(entry == global_first_entry)
            {
                global_first_entry = next;

                if(next)
                {
                    next->prev = NULL;
                }
            }
            else
            {
                entry->prev->next = next;

                if(next)
                {
                    next->prev = entry->prev;
                }
            }

            entry->prev = NULL;
            entry->next = NULL;

            if(category_first_entry[i] == NULL)
            {
                category_first_entry[i] = entry;
                category_last_entry[i] = entry;
            }
            else
            {
                category_last_entry[i]->next = entry;
                entry->prev = category_last_entry[i];
                category_last_entry[i] = entry;
            }
        }
    }

    return 1;
}

