/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: top-panel-widgets.c
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
 *  \file top-panel-widgets.c
 *
 *  Common functions for use by top panel widgets.
 */

#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/list.h>
#include <sys/ioctl.h>
#include "../include/list.h"
#include "../include/panels/widget.h"
#include "../include/client/window.h"
#include "../include/font.h"
#include "../desktop/run_command.c"


#define GLOB                        __global_gui_data

struct app_entry_t *first_entry = NULL;
struct app_entry_t *last_entry = NULL;

#include "../desktop/desktop_entry_lister.c"

int widget_height = DEFAULT_WIDGET_HEIGHT;

struct widget_t *focused_widget = NULL;

struct widget_color_t widget_colors[] =
{
    // { background, text } for normal, mouse-over and pressed states
    { TOPPANEL_BGCOLOR, TOPPANEL_FGCOLOR },
    { TOPPANEL_MOUSEOVER_BGCOLOR, TOPPANEL_MOUSEOVER_FGCOLOR },
    { TOPPANEL_DOWN_BGCOLOR, TOPPANEL_DOWN_FGCOLOR }
};

struct widget_name_t
{
    char *name;
    char *initfunc_name;
};

struct widget_name_t widget_names[] =
{
    { "widget_clock", "widget_init_clock" },
    { "widget_volume", "widget_init_volume" },
    { "widget_apps", "widget_init_apps" },
    { NULL, NULL },
};


void widgets_init(void)
{
    struct widget_name_t *widget;
    void *lib;
    int (*init)(void);
    char *p, path[64];

    // load default application categories and the applications list (needed
    // by the Applications widget)
    if(!(p = malloc(PATH_MAX)))
    {
        // yelp - at least get the categories
        get_app_categories(NULL);
    }
    else
    {
        ftree(DEFAULT_DESKTOP_PATH, p);
        free(p);
    }

    // load all widgets
    for(widget = widget_names; widget->name != NULL; widget++)
    {
        sprintf(path, "/bin/widgets/%s.so", widget->name);

        if((lib = dlopen(path, 0)))
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            if((init = dlsym(lib, widget->initfunc_name)))
            {
                init();
            }
        }
    }
}


int widget_char_height(void)
{
    return char_height(backbuf_gc.font, ' ');
}


int widget_char_ascender(void)
{
    return char_ascender(backbuf_gc.font, ' ');
}


int widget_char_width(void)
{
    return char_width(backbuf_gc.font, ' ');
}


int widget_string_width(char *str)
{
    return string_width(backbuf_gc.font, str);
}


int widget_is_monospace_font(void)
{
    return !(backbuf_gc.font->flags & FONT_FLAG_TRUE_TYPE);
}


uint32_t widget_bg_color(struct widget_t *widget)
{
    return widget_colors[widget->state].bg;
}


uint32_t widget_fg_color(struct widget_t *widget)
{
    return widget_colors[widget->state].text;
}

uint32_t widget_hi_color(struct widget_t *widget)
{
    return TOPPANEL_HICOLOR;
}


void widget_copy_buf(struct widget_t *widget)
{
    unsigned int i = widget->win.x * backbuf_gc.pixel_width;
    unsigned int count = widget->win.w * backbuf_gc.pixel_width;
    uint8_t *src = backbuf_gc.buffer + i;
    uint8_t *dest = main_window->canvas + i;
    int y;

    for(y = 0; y < widget_height; y++)
    {
        A_memcpy(dest, src, count);
        src += backbuf_gc.pitch;
        dest += backbuf_gc.pitch;
    }
}


void widgets_layout(void)
{
    struct window_t *tmp;
    int16_t curx = main_window->w;  // for normal, right-aligned widgets
    int16_t lcurx = 0;              // for left-aligned widgets
    ListNode *current_node;

    for(current_node = main_window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        tmp = (struct window_t *)current_node->payload;
        
        if(!tmp->visible)
        {
            continue;
        }
        
        if(((struct widget_t *)tmp)->flags & WIDGET_FLAG_FLOAT_LEFT)
        {
            tmp->x = lcurx;
            tmp->y = 0;
            tmp->h = widget_height;

            tmp->clip_rects->root->top = 0;
            tmp->clip_rects->root->left = lcurx;
            tmp->clip_rects->root->bottom = widget_height - 1;
            tmp->clip_rects->root->right = lcurx + tmp->w - 1;

            lcurx += tmp->w;
        }
        else
        {
            curx -= tmp->w;
            tmp->x = curx;
            tmp->y = 0;
            tmp->h = widget_height;

            tmp->clip_rects->root->top = 0;
            tmp->clip_rects->root->left = curx;
            tmp->clip_rects->root->bottom = widget_height - 1;
            tmp->clip_rects->root->right = curx + tmp->w - 1;
        }
    }
}


int widgets_redraw(void)
{
    struct window_t *widget;
    int res = 0;
    ListNode *current_node;

    widgets_layout();

    for(current_node = main_window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        widget = (struct window_t *)current_node->payload;

        if(!widget->visible)
        {
            continue;
        }

        if(widget->x < 0)
        {
            continue;
        }

        widget->repaint(widget, (widget == main_window->active_child));
        widget->flags |= WIDGET_FLAG_DRAWN;
    }

    res = 1;
    
    return res;
}


void widgets_periodic(void)
{
    struct widget_t *widget;
    ListNode *current_node;

    for(current_node = main_window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        widget = (struct widget_t *)current_node->payload;

        if(widget->win.visible)
        {
            if(widget->periodic(widget))
            {
                widget_copy_buf(widget);
                child_invalidate((struct window_t *)widget);
            }
        }
    }
}


int widget_periodic_dummy(struct widget_t *widget_win)
{
    (void)widget_win;

    return 0;
}


void widget_repaint_dummy(struct window_t *widget_win, int is_active_child)
{
    (void)widget_win;
    (void)is_active_child;
}


/*
 * Handle mouse over events over top panel widgets.
 */
void widget_mouseover(struct window_t *widget_win, 
                      struct mouse_state_t *mstate)
{
    struct widget_t *widget = (struct widget_t *)widget_win;

    widget->state = (mstate->buttons & MOUSE_LBUTTON_DOWN) ?
                                WIDGET_STATE_DOWN : WIDGET_STATE_MOUSEOVER;

    widget_win->repaint(widget_win, 
                        (widget_win == widget_win->parent->active_child));
    widget_copy_buf(widget);
    child_invalidate(widget_win);
}


/*
 * Handle mouse down events over top panel widgets.
 */
void widget_mousedown(struct window_t *widget_win, 
                      struct mouse_state_t *mstate)
{
    struct widget_t *widget = (struct widget_t *)widget_win;

    widget->state = WIDGET_STATE_DOWN;

    widget_win->repaint(widget_win, 
                        (widget_win == widget_win->parent->active_child));
    widget_copy_buf(widget);
    child_invalidate(widget_win);
}


/*
 * Handle mouse exit events over top panel widgets.
 */
void widget_mouseexit(struct window_t *widget_win)
{
    struct widget_t *widget = (struct widget_t *)widget_win;

    widget->state = WIDGET_STATE_NORMAL;

    widget_win->repaint(widget_win, 
                        (widget_win == widget_win->parent->active_child));
    widget_copy_buf(widget);
    child_invalidate(widget_win);
}


/*
 * Handle mouse up events over top panel widgets.
 */
void widget_mouseup(struct window_t *widget_win, struct mouse_state_t *mstate)
{
    struct widget_t *widget = (struct widget_t *)widget_win;
    
    // if we clicked on a widget while another had its menu open, close the 
    // other menu before showing the new menu
    if(focused_widget && focused_widget != widget && focused_widget->menu)
    {
        widget_menu_hide(focused_widget);
    }

    focused_widget = widget;

    widget->state = WIDGET_STATE_MOUSEOVER;

    widget_win->repaint(widget_win, 
                        (widget_win == widget_win->parent->active_child));
    widget_copy_buf(widget);
    child_invalidate(widget_win);

    //Fire the associated button click event if it exists
    if(widget->button_click_callback)
    {
        widget->button_click_callback(widget, mstate->x, mstate->y);
    }
}


/*
 * Handle focus and unfocus events for top panel widgets.
 */
void widget_unfocus(struct window_t *widget_win)
{
    struct widget_t *widget = (struct widget_t *)widget_win;

    widget->state = WIDGET_STATE_NORMAL;

    widget_win->repaint(widget_win, 
                        (widget_win == widget_win->parent->active_child));
    widget_copy_buf(widget);
    child_invalidate(widget_win);

    if(focused_widget == widget)
    {
        focused_widget = NULL;
    }
}

void widget_focus(struct window_t *widget_win)
{
}


/*
 * Create a new widget.
 */
struct widget_t *widget_create(void)
{
    struct widget_t *widget;
    Rect *rect;

    widget = malloc(sizeof(struct widget_t));
    
    if(!widget)
    {
        return NULL;
    }

    A_memset(widget, 0, sizeof(struct widget_t));

    if(!(widget->win.clip_rects = RectList_new()))
    {
        free(widget);
        return NULL;
    }

    if(!(rect = Rect_new(0, 0, 0,  0)))
    {
        RectList_free(widget->win.clip_rects);
        free(widget);
        return NULL;
    }

    RectList_add(widget->win.clip_rects, rect);
    widget->win.type = WINDOW_TYPE_BUTTON;
    widget->win.flags = WINDOW_NODECORATION;
    widget->win.visible = 1;
    widget->state = WIDGET_STATE_NORMAL;

    widget->win.repaint = widget_repaint_dummy;
    widget->win.mousedown = widget_mousedown;
    widget->win.mouseover = widget_mouseover;
    widget->win.mouseup = widget_mouseup;
    widget->win.mouseexit = widget_mouseexit;
    widget->win.unfocus = widget_unfocus;
    widget->win.focus = widget_focus;

    widget->periodic = widget_periodic_dummy;
    widget->button_click_callback = NULL;

    window_insert_child(main_window, (struct window_t *)widget);

    return widget;
}


/********************************************************************
 * The following functions are wrappers to enable widgets to
 * call our defined functions without worrying about them not being
 * dynamically loaded by the linker.
 ********************************************************************/

resid_t widget_image_load(char *filename, struct bitmap32_t *bitmap)
{
    return image_load(filename, bitmap);
}


void widget_draw_text(struct widget_t *widget, char *buf, 
                      int x, int y, uint32_t color)
{
    x += widget->win.x;
    y += widget->win.y;
    backbuf_gc.clipping.clip_rects = widget->win.clip_rects;
    gc_draw_text(&backbuf_gc, buf, x, y, color, 0);
    backbuf_gc.clipping.clip_rects = NULL;
}


void widget_fill_rect(struct widget_t *widget, int x, int y, 
                      int w, int h, uint32_t color)
{
    x += widget->win.x;
    y += widget->win.y;
    backbuf_gc.clipping.clip_rects = widget->win.clip_rects;
    gc_fill_rect(&backbuf_gc, x, y, w, h, color);
    backbuf_gc.clipping.clip_rects = NULL;
}


void widget_fill_background(struct widget_t *widget)
{
    uint32_t col = widget_colors[widget->state].bg;

    gc_fill_rect(&backbuf_gc,
                         widget->win.x, widget->win.y,
                         widget->win.w, widget->win.h, col);
}


void widget_fill_bitmap(struct widget_t *widget,
                        int x, int y, int w, int h, struct bitmap32_t *bitmap)
{
    x += widget->win.x;
    y += widget->win.y;
    backbuf_gc.clipping.clip_rects = widget->win.clip_rects;

    gc_blit_bitmap_highlighted(&backbuf_gc, bitmap, x, y, 0, 0, w, h, 0);

    backbuf_gc.clipping.clip_rects = NULL;
}


int widget_child_x(struct window_t *window, int x)
{
    return to_child_x(window, x);
}


int widget_child_y(struct window_t *window, int y)
{
    return to_child_y(window, y);
}


void widget_get_app_categories(char ***categories, int *count)
{
    *categories = app_categories;
    *count = app_category_count;
}


void widget_get_app_entries(struct app_entry_t **res)
{
    *res = first_entry;
}


void widget_run_command(char *cmd)
{
    run_command(cmd);
}


void widgets_show_apps(void)
{
    struct widget_t *widget;
    ListNode *current_node;

    for(current_node = main_window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        widget = (struct widget_t *)current_node->payload;

        if(widget->win.visible && 
           widget->win.title && !strcmp(widget->win.title, "Applications"))
        {
            if(widget->button_click_callback)
            {
                // emulate a mouse click event
                widget->button_click_callback(widget, 1, 1);
            }

            break;
        }
    }
}

