/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: widget.h
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
 *  \file widget.h
 *
 *  Declarations and struct definitions for the desktop's top panel widgets.
 *  Examples of these widgets include the clock widget and the applications
 *  menu.
 */

#ifndef WIDGET_H
#define WIDGET_H

#include <kernel/mouse.h>
#include "../list.h"
#include "../gui.h"
#include "../gc.h"
#include "../resources.h"
#include "../event.h"
#include "../window-defs.h"
#include "../client/window-attrib-struct.h"
#include "../client/window-struct.h"
#include "top-panel.h"

#define DEFAULT_WIDGET_HEIGHT           TOPPANEL_HEIGHT

#define WIDGET_STATE_NORMAL             0
#define WIDGET_STATE_MOUSEOVER          1
#define WIDGET_STATE_DOWN               2

/**********************************************
 * Struct definitions
 **********************************************/

struct widget_t
{
    struct window_t win;
    struct gc_t gc;

#define WIDGET_FLAG_INITIALIZED                     0x01
#define WIDGET_FLAG_HIDDEN                          0x02
#define WIDGET_FLAG_DRAWN                           0x04
#define WIDGET_FLAG_FLOAT_LEFT                      0x08
    int flags;

    uint8_t state;
    
    struct window_t *menu;

    int (*periodic)(struct widget_t *);
    void (*button_click_callback)(struct widget_t *, int, int);
};

struct widget_color_t
{
    uint32_t bg, text;
};

extern struct widget_color_t widget_colors[];
extern int widget_height;
extern struct widget_t *focused_widget;

/**********************************************
 * Internal library functions
 **********************************************/

void widgets_init(void);
struct widget_t *widget_create(void);
int widgets_redraw(void);
void widgets_periodic(void);
void widget_menu_may_hide(winid_t);
void widgets_show_apps(void);

/**********************************************
 * Public functions for widget perusal
 **********************************************/

int widget_char_height(void);
int widget_char_width(void);
int widget_char_ascender(void);
int widget_string_width(char *str);
int widget_is_monospace_font(void);
uint32_t widget_bg_color(struct widget_t *widget);
uint32_t widget_fg_color(struct widget_t *widget);
uint32_t widget_hi_color(struct widget_t *widget);
void widget_draw_text(struct widget_t *widget, char *buf, 
                        int x, int y, uint32_t color);
void widget_fill_background(struct widget_t *widget);
void widget_fill_rect(struct widget_t *widget, int x, int y, 
                        int w, int h, uint32_t color);
resid_t widget_image_load(char *filename, struct bitmap32_t *bitmap);
void widget_fill_bitmap(struct widget_t *widget,
                        int x, int y, int w, int h, struct bitmap32_t *bitmap);
int widget_child_x(struct window_t *window, int x);
int widget_child_y(struct window_t *window, int y);

void widget_get_app_categories(char ***categories, int *count);
void widget_get_app_entries(struct app_entry_t **res);
void widget_run_command(char *cmd);

void widget_unfocus(struct window_t *widget_win);

// top-panel-widgets-menu.c
void widget_menu_show(struct widget_t *widget);
void widget_menu_hide(struct widget_t *widget);
void widget_menu_fill_background(struct window_t *frame);
void widget_menu_fill_rect(struct window_t *frame, int x, int y, 
                            int w, int h, uint32_t color);
void widget_menu_draw_text(struct window_t *frame, char *buf, 
                            int x, int y, uint32_t color);
struct window_t *widget_menu_create(int w, int h);
void widget_menu_invalidate(struct window_t *frame);
uint32_t widget_menu_bg_color(void);
uint32_t widget_menu_fg_color(void);
uint32_t widget_menu_hi_color(void);

#endif      /* WIDGET_H */
