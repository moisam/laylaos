/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: main.c
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
 *  \file main.c
 *
 *  The system settings program.
 */

#include "../include/gui.h"
#include "../include/bitmap.h"
#include "../include/resources.h"
#include "../include/client/gallery-view.h"
#include "../include/client/dialog.h"
#include "defs.h"

#define ICONWIDTH               64
#define GLOB                    __global_gui_data

// indexes to the bitmaps array below
enum
{
    INDEX_BACKGROUND = 0,
    //INDEX_DATE_AND_TIME,
    //INDEX_DISKS,
    INDEX_DISPLAY,
    //INDEX_NETWORK,
    //INDEX_SOUND,
    INDEX_SYSTEM_INFO,
    INDEX_THEME,
    INDEX_LAST,
};

// sysicon name and display title for each settings item
struct
{
    char *sysicon_name, *display_title;
    winid_t winid;      // if a settings sub-window is shown for this item
    winid_t (*func)();  // function to display a new sub-window for this item
    struct bitmap32_t bitmap;
} settings_items[] =
{
    { "flower",          "Background",    0, show_window_background, { 0, } },
    //{ "calendar-clock",  "Date and Time", 0, NULL, { 0, } },
    //{ "device-drive",    "Disks",         0, NULL, { 0, } },
    { "device-computer", "Display",       0, show_window_display, { 0, } },
    //{ "cloud",           "Network",       0, NULL, { 0, } },
    //{ "bullhorn",        "Sound",         0, NULL, { 0, } },
    { "cog",             "System Info",   0, show_window_sysinfo, { 0, } },
    { "puzzle",          "Theme",         0, show_window_theme, { 0, } },
};

struct window_t *main_window;


static void galleryentry_doubleclick_callback(struct gallery_view_t *gallery,
                                              struct file_entry_t *entry)
{
    int i;

    for(i = 0; i < INDEX_LAST; i++)
    {
        // find the function to display the requested settings item
        if(strcmp(settings_items[i].display_title, entry->name) == 0)
        {
            // if the settings window is already displayed, bring it forward
            if(settings_items[i].winid != 0)
            {
                window_raise(win_for_winid(settings_items[i].winid));
                break;
            }

            // otherwise display a new window
            if(settings_items[i].func)
            {
                settings_items[i].winid = settings_items[i].func();
            }

            break;
        }
    }
}


int main(int argc, char **argv)
{
    int i;
    struct event_t *ev = NULL;
    struct window_attribs_t attribs;
    struct gallery_view_t *gallery;

    gui_init(argc, argv);

    for(i = 0; i < INDEX_LAST; i++)
    {
        settings_items[i].bitmap.width = ICONWIDTH;
        settings_items[i].bitmap.height = ICONWIDTH;

        if(!sysicon_load(settings_items[i].sysicon_name, &settings_items[i].bitmap))
        {
            fprintf(stderr, "%s: failed to load system icon: %s\n", 
                            argv[0], strerror(errno));
            gui_exit(EXIT_FAILURE);
        }
    }

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 420;
    attribs.h = 300;
    attribs.flags = 0;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n", 
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    // add settings items to the gallery view
    gallery = gallery_view_new(main_window->gc, main_window,
                               0, 0, main_window->w, main_window->h);

    for(i = 0; i < INDEX_LAST; i++)
    {
        gallery_view_add(gallery, settings_items[i].display_title,
                                  &settings_items[i].bitmap);
    }

    gallery->entry_doubleclick_callback = galleryentry_doubleclick_callback;
    widget_set_size_hints((struct window_t *)gallery, NULL, 
                            RESIZE_FILLW|RESIZE_FILLH, 0, 0, 0, 0);

    window_set_title(main_window, "System settings");
    window_repaint(main_window);
    window_set_icon(main_window, "settings.ico");
    window_show(main_window);

    get_desktop_bg();

    while(1)
    {
        if(!(ev = next_event()))
        {
            continue;
        }
        
        if(event_dispatch(ev))
        {
            free(ev);
            continue;
        }
    
        switch(ev->type)
        {
            case EVENT_WINDOW_CLOSING:
                if(ev->dest == main_window->winid)
                {
                    window_destroy_all();
                    gui_exit(EXIT_SUCCESS);
                }
                else
                {
                    struct window_t *win = win_for_winid(ev->dest);

                    window_hide(win);
                    window_destroy(win);

                    for(i = 0; i < INDEX_LAST; i++)
                    {
                        if(settings_items[i].winid == ev->dest)
                        {
                            settings_items[i].winid = 0;
                            break;
                        }
                    }
                }

                break;

            case EVENT_KEY_PRESS:
                break;

            default:
                break;
        }

        free(ev);
    }
}

