/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: show_theme.c
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
 *  \file show_theme.c
 *
 *  Part of the system settings program. Shows a window with theme settings.
 */

#include "../include/gui.h"
#include "../include/client/dialog.h"
#include "../include/client/textbox.h"
#include "defs.h"

#define GLOB        __global_gui_data

struct
{
    // theme name
    char *name;

    // windows
    uint32_t themecolor[64];

} themes[] =
{
    {
      "Default",
      {
          0xCDCFD4FF, 0x3B4047FF, 0x3B4047FF,   // windows
          0xCDCFD4FF, 0x535E64FF, 0x2E3238FF,
          0x2E3238FF,
          0xCDCFD4FF, 0x222226FF, 0x222226FF,   // buttons
          0xB4B4B8FF, 0x222226FF, 0x222226FF,
          0xB4B4B8FF, 0x222226FF, 0x222226FF,
          0xE0DFE3FF, 0x222226FF, 0x222226FF,
          0xCDCFD4FF, 0xBABDC4FF, 0x222226FF,
          0xCDCFD4FF, 0x222226FF,               // status bars
          0xCDCFD4FF, 0x222226FF,               // scroll bars
          0xFFFFFFFF, 0x000000FF,               // textboxes
          0xFFFFFFFF, 0x000000FF,               // inputboxes
          0x16A085FF, 0xFFFFFFFF,
          0xCDCFD4FF, 0xBABDC4FF,
          0x16A085FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
      },
    },
    {
      "Blue",
      {
          0xCDCFD4FF, 0x3366CCFF, 0x8F8F91FF,   // windows
          0xCDCFD4FF, 0x535E64FF, 0x2856B2FF,
          0x7B7B7CFF,
          0xCDCFD4FF, 0x222226FF, 0x222226FF,   // buttons
          0xB4B4B8FF, 0x222226FF, 0x222226FF,
          0xB4B4B8FF, 0x222226FF, 0x222226FF,
          0xE0DFE3FF, 0x222226FF, 0x222226FF,
          0xCDCFD4FF, 0xBABDC4FF, 0x222226FF,
          0xCDCFD4FF, 0x222226FF,               // status bars
          0xCDCFD4FF, 0x222226FF,               // scroll bars
          0xFFFFFFFF, 0x000000FF,               // textboxes
          0xFFFFFFFF, 0x000000FF,               // inputboxes
          0x337CC4FF, 0xFFFFFFFF,
          0xCDCFD4FF, 0xBABDC4FF,
          0x337CC4FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
      },
    },
    {
      "Orange",
      {
          0xCDCFD4FF, 0xFF8000FF, 0x004C00FF,   // windows
          0xCDCFD4FF, 0x535E64FF, 0xDC7105FF,
          0x013601FF,
          0xCDCFD4FF, 0x222226FF, 0x222226FF,   // buttons
          0xB4B4B8FF, 0x222226FF, 0x222226FF,
          0xB4B4B8FF, 0x222226FF, 0x222226FF,
          0xE0DFE3FF, 0x222226FF, 0x222226FF,
          0xCDCFD4FF, 0xBABDC4FF, 0x222226FF,
          0xCDCFD4FF, 0x222226FF,               // status bars
          0xCDCFD4FF, 0x222226FF,               // scroll bars
          0xFFFFFFFF, 0x000000FF,               // textboxes
          0xFFFFFFFF, 0x000000FF,               // inputboxes
          0xFF9933FF, 0xFFFFFFFF,
          0xCDCFD4FF, 0xBABDC4FF,
          0xFF9933FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
      },
    },
};

size_t theme_count = sizeof(themes) / sizeof(themes[0]);


/*
 * Draw the right side of the window, which showcases the individual colors
 * in the theme.
 */
static void draw_right_side(struct window_t *window, int is_active_child)
{
    int x = 220, y = 20, w = 200, h = 240;

    gc_fill_rect(window->gc, 0, 0, window->w, window->h, window->bgcolor);

    // Draw an example window

    // Fill in the titlebar background
    gc_fill_rect(window->gc, x, y,
                     w, WINDOW_TITLEHEIGHT,
                     GLOB.themecolor[THEME_COLOR_WINDOW_TITLECOLOR]);

    // Draw the window title
    gc_draw_text(window->gc, "Sample window",
                     x + 10, y + 6, 
                     GLOB.themecolor[THEME_COLOR_WINDOW_TEXTCOLOR], 0);

    // Draw the controlbox
    //draw_controlbox(window->gc, window, x, y, 0);

    // Draw the outer border
    gc_fill_rect(window->gc, x, y, w,
                     WINDOW_BORDERWIDTH, 
                     GLOB.themecolor[THEME_COLOR_WINDOW_BORDERCOLOR]);
    gc_fill_rect(window->gc, x, y + h - WINDOW_BORDERWIDTH,
                     w, WINDOW_BORDERWIDTH, 
                     GLOB.themecolor[THEME_COLOR_WINDOW_BORDERCOLOR]);

    gc_fill_rect(window->gc, x, y, WINDOW_BORDERWIDTH,
                     h, 
                     GLOB.themecolor[THEME_COLOR_WINDOW_BORDERCOLOR]);
    gc_fill_rect(window->gc, x + w - WINDOW_BORDERWIDTH,
                     y, WINDOW_BORDERWIDTH, h, 
                     GLOB.themecolor[THEME_COLOR_WINDOW_BORDERCOLOR]);
}


void listentry_click_callback(struct listview_t *listv, int selindex)
{
    //struct window_t *window = (struct window_t *)listv->window.parent;

    if(selindex < 0 || selindex >= theme_count)
    {
        return;
    }

    A_memcpy(GLOB.themecolor, themes[selindex].themecolor, 
                                THEME_COLOR_LAST * sizeof(uint32_t));

    /*
    window_repaint(window);
    draw_right_side(window);
    window_invalidate(window);
    */

    // tell the server about the new theme so it can be broadcast to everybody
    send_color_theme_to_server();
}


winid_t show_window_theme(void)
{
    int i;
    struct window_t *window;
    struct listview_t *list;
    struct window_attribs_t attribs;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 440;
    attribs.h = 320;
    attribs.flags = WINDOW_NORESIZE;

    if(!(window = window_create(&attribs)))
    {
        char buf[128];

        sprintf(buf, "Failed to create window: %s", strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
        return 0;
    }

    list = listview_new(window->gc, window, 20, 20, 180, 280);
    list->entry_click_callback = listentry_click_callback;

    for(i = 0; i < theme_count; i++)
    {
        listview_append_item(list, themes[i].name);
    }

    list->cur_entry = 0;
    list->entries[list->cur_entry].selected = 1;

    inputbox_new(window->gc, window, 240, 70, 160, "Inputbox");
    textbox_new(window->gc, window, 240, 110, 160, 30, "Textbox");
    button_new(window->gc, window, 280, 150, 70, 30, "Button");

    window_set_title(window, "System theme");
    window_set_icon(window, "settings.ico");
    window->repaint = draw_right_side;

    window_repaint(window);
    //draw_right_side(window);
    window_show(window);

    return window->winid;
}

