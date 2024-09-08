/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: show_display.c
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
 *  \file show_display.c
 *
 *  Part of the system settings program. Shows a window with display settings.
 */

#include "../include/gui.h"
#include "../include/bitmap.h"
#include "../include/resources.h"
#include "../include/client/dialog.h"
#include "../include/client/label.h"
#include "defs.h"

#define ICONWIDTH               64
#define GLOB                    __global_gui_data

struct bitmap32_t monitor_bitmap = { 0, 0, 0 };


static void label_icon_repaint(struct window_t *label_window, int is_active_child)
{
    if(!monitor_bitmap.data)
    {
        return;
    }

    gc_blit_bitmap(label_window->gc, &monitor_bitmap,
                   to_child_x(label_window, 0), to_child_y(label_window, 0),
                   0, 0,
                   monitor_bitmap.width, monitor_bitmap.height);
}


winid_t show_window_display(void)
{
    struct window_t *window;
    struct label_t *l0;
    struct window_attribs_t attribs;
    char str[1024];
    unsigned int iconw = ICONWIDTH;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 200;
    attribs.h = 210;
    attribs.flags = WINDOW_NORESIZE;

    if(!(window = window_create(&attribs)))
    {
        char buf[128];

        sprintf(buf, "Failed to create window: %s", strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
        return 0;
    }

    // load the monitor icon
    monitor_bitmap.width = iconw;
    monitor_bitmap.height = iconw;

    if(!monitor_bitmap.data)
    {
        sysicon_load("device-computer", &monitor_bitmap);
    }

    // the loaded bitmap might be of a different size
    iconw = monitor_bitmap.width;

    // Create a label for the monitor icon. We will override its
    // repaint function so we can draw our icon
    l0 = label_new(window->gc, window, 
                   (attribs.w - iconw) / 2, 10, 
                   iconw, iconw, NULL);
    l0->window.repaint = label_icon_repaint;

    // prep the first string and set the label
    sprintf(str, "Screen width:\n"
                 "Screen height:\n"
                 "Bytes per pixel:\n"
                 "RGB mode:");
    label_new(window->gc, window, 20, 120, 100, 120, str);

    // prep the second string and set the label
    sprintf(str, "%d\n"
                 "%d\n"
                 "%d\n"
                 "%s",
                 GLOB.screen.w, GLOB.screen.h,
                 GLOB.screen.pixel_width,
                 GLOB.screen.rgb_mode ? "Yes" : "No");
    label_new(window->gc, window, 130, 120, 70, 120, str);

    window_set_title(window, "Display settings");
    window_set_icon(window, "settings.ico");

    window_repaint(window);
    window_show(window);

    return window->winid;
}

