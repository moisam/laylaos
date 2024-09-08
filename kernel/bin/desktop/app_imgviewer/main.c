/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
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
 *  A simple image viewer program.
 */

#include <errno.h>
#include <sys/stat.h>
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/keys.h"
#include "../include/menu.h"
#include "../include/resources.h"
#include "../include/client/dialog.h"
#include "defs.h"

#define APP_TITLE           "Image viewer"
#define WIN_MIN_WIDTH       300
#define WIN_MIN_HEIGHT      300

#define ZOOM_STEP           20.0f
#define ZOOM_MAX            600.0f
#define ZOOM_MIN            25.0f


struct window_t *main_window;
struct gc_t backbuf_gc;
struct bitmap32_t loaded_bitmap;
char *loaded_path;
struct stat loaded_path_stat;

struct menu_item_t *saveas_mi, *properties_mi;
struct menu_item_t *zoomin_mi, *zoomout_mi, *fitwin_mi, *origzoom_mi;

// current zoom
float zoom = 100.0f;

// possible zooms
float zoom_sizes[] =
{
    25.0f,
    50.0f,
    100.0f,
    200.0f,
    400.0f,
    600.0f,
};

// pointers to the zoom menu items for quick access
// these will be filled in create_main_menu() below
struct menu_item_t *zoom_mi[] =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

size_t zoom_count = sizeof(zoom_sizes) / sizeof(zoom_sizes[0]);

// forward declarations
void menu_view_zoomin_handler(winid_t winid);
void menu_view_zoomout_handler(winid_t winid);


void repaint(struct window_t *window, int is_active_child)
{
    int renderw = loaded_bitmap.width * (zoom / 100.0f);
    int renderh = loaded_bitmap.height * (zoom / 100.0f);
    int posx = ((int)backbuf_gc.w - renderw) / 2;
    int posy = ((int)backbuf_gc.h - renderh) / 2;

    if(!backbuf_gc.buffer)
    {
        return;
    }

    gc_fill_rect(&backbuf_gc, 0, 0,
                 backbuf_gc.w, backbuf_gc.h, GLOBAL_BLACK_COLOR);

    /*
    // paint black on the left and right if needed
    if(posx > 0)
    {
        gc_fill_rect(main_window->gc, 0, 0,
                     posx, main_window->h, GLOBAL_BLACK_COLOR);

        gc_fill_rect(main_window->gc, main_window->w - posx, 0,
                     posx, main_window->h, GLOBAL_BLACK_COLOR);
    }

    // paint black on the top and bottom if needed
    if(posy > 0)
    {
        gc_fill_rect(main_window->gc, 0, 0,
                     main_window->w, posy, GLOBAL_BLACK_COLOR);

        gc_fill_rect(main_window->gc, 0, main_window->h - posy,
                     main_window->w, posy, GLOBAL_BLACK_COLOR);
    }
    */

    // now paint the image, scaled if needed
    if(loaded_bitmap.data)
    {
        gc_stretch_bitmap(&backbuf_gc, &loaded_bitmap,
                          posx, posy,
                          renderw, renderh,
                          0, 0,
                          loaded_bitmap.width, loaded_bitmap.height);
    }

    gc_blit(main_window->gc, &backbuf_gc, 0, MENU_HEIGHT);
}


// Callback for when the window size changes
void size_changed(struct window_t *window)
{
    if(backbuf_gc.w != window->w ||
       backbuf_gc.h != window->h - MENU_HEIGHT)
    {
        if(gc_realloc_backbuf(window->gc, &backbuf_gc,
                              window->w, window->h - MENU_HEIGHT) < 0)
        {
            /*
             * NOTE: this should be a fatal error.
             */
            return;
        }
    }

    window_repaint(main_window);
}


void load_file(char *filename)
{
    char *ext;
    unsigned int neww, newh;
    size_t i;
    struct bitmap32_t new_bitmap;

    if(!filename || !*filename)
    {
        return;
    }

    if(stat(filename, &loaded_path_stat) == -1)
    {
        char buf[strlen(filename) + 128];

        sprintf(buf, "Failed to open %s: %s", filename, strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
        return;
    }

    ext = file_extension(filename);
    
    if(strcasecmp(ext, ".png") == 0)
    {
        if(!png_load(filename, &new_bitmap))
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            return;
        }
    }
    else if(strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".jpg") == 0)
    {
        if(!jpeg_load(filename, &new_bitmap))
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            return;
        }
    }
    else if(strcasecmp(ext, ".ico") == 0)
    {
        struct bitmap32_array_t *imga;
        int i, j, w, h;

        if(!(imga = ico_load(filename)))
        {
            return;
        }
        
        // find the highest resolution
        w = 0;
        h = 0;
        j = 0;

        for(i = 0; i < imga->count; i++)
        {
            if(imga->bitmaps[i].width > w || imga->bitmaps[i].height > h)
            {
                j = i;
                w = imga->bitmaps[i].width;
                h = imga->bitmaps[i].height;
            }
        }

        new_bitmap.width = imga->bitmaps[j].width;
        new_bitmap.height = imga->bitmaps[j].height;
        new_bitmap.data = imga->bitmaps[j].data;
        imga->bitmaps[j].data = NULL;
        bitmap32_array_free(imga);
    }
    else
    {
        char buf[strlen(filename) + 64];

        sprintf(buf, "Failed to open %s: Unsupported format", filename);
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
        return;
    }

    // free the old bitmap
    if(loaded_bitmap.data)
    {
        free(loaded_bitmap.data);
    }

    loaded_bitmap.width = new_bitmap.width;
    loaded_bitmap.height = new_bitmap.height;
    loaded_bitmap.data = new_bitmap.data;

    if(loaded_path)
    {
        free(loaded_path);
    }

    loaded_path = strdup(filename);

    // set the new window title
    char buf[strlen(filename) + 16];

    sprintf(buf, "%s - %s", APP_TITLE, filename);
    window_set_title(main_window, buf);

    // reset the zoom menu items
    zoom = 100.0f;
    menu_item_set_enabled(zoomin_mi, 1);
    menu_item_set_enabled(zoomout_mi, 1);
    menu_item_set_enabled(fitwin_mi, 1);
    menu_item_set_enabled(origzoom_mi, 1);

    for(i = 0; i < zoom_count; i++)
    {
        menu_item_set_enabled(zoom_mi[i], 1);
        menu_item_set_checked(zoom_mi[i], 0);
    }

    menu_item_set_checked(zoom_mi[2], 1);

    // enable the File -> Properties menu item
    menu_item_set_enabled(properties_mi, 1);

    // resize the window if needed
    if(loaded_bitmap.width > 600)
    {
        zoom = (600 / loaded_bitmap.width) * 100.0f;

        // find the nearest zoom
        for(i = 0; i < zoom_count; i++)
        {
            if(zoom < zoom_sizes[i])
            {
                zoom = zoom_sizes[i];
                menu_item_set_checked(zoom_mi[2], 0);
                menu_item_set_checked(zoom_mi[i], 1);
                break;
            }
        }

        neww = loaded_bitmap.width * (zoom / 100.0f);
        newh = loaded_bitmap.height * (zoom / 100.0f);
    }
    else
    {
        neww = (loaded_bitmap.width < WIN_MIN_WIDTH) ? WIN_MIN_WIDTH :
                                                       loaded_bitmap.width;
        newh = (loaded_bitmap.height < WIN_MIN_HEIGHT) ? WIN_MIN_HEIGHT :
                                                         loaded_bitmap.height;
    }

    newh += MENU_HEIGHT;

    if(neww != main_window->w || newh != main_window->h)
    {
        // we will repaint when the window is resized
        window_set_size(main_window, main_window->x, main_window->y,
                        neww, newh);
        return;
    }

    // no resize, paint now
    window_repaint(main_window);
}


static inline void uncheck_current_zoom(void)
{
    size_t i;

    for(i = 0; i < zoom_count; i++)
    {
        if(zoom_sizes[i] == zoom)
        {
            menu_item_set_checked(zoom_mi[i], 0);
            return;
        }
    }
}


static inline void check_current_zoom(void)
{
    size_t i;

    for(i = 0; i < zoom_count; i++)
    {
        if(zoom_sizes[i] == zoom)
        {
            menu_item_set_checked(zoom_mi[i], 1);
            return;
        }
    }
}


void process_mouseover(struct window_t *window, struct mouse_state_t *mstate)
{
    if(mstate->buttons & MOUSE_VSCROLL_DOWN)
    {
        menu_view_zoomout_handler(0);
    }

    if(mstate->buttons & MOUSE_VSCROLL_UP)
    {
        menu_view_zoomin_handler(0);
    }
}

/*
void process_mousedown(struct window_t *window, struct mouse_state_t *mstate)
{
}


void process_mouseup(struct window_t *window, struct mouse_state_t *mstate)
{
}
*/


void menu_file_open_handler(winid_t winid)
{
    struct opensave_dialog_t *dialog;
    struct opensave_file_t *files;
    int count;

    if(!(dialog = open_dialog_create(winid)))
    {
        /*
         * TODO: show an error message here.
         */
        return;
    }

    dialog->multiselect = 0;
    dialog->filetype_filter = "All formats|*.ico;*.jpg;*.jpeg;*.png|"
                              "ICO images|*.ico|"
                              "JPEG images|*.jpg;*.jpeg|"
                              "PNG images|*.png";

    if(open_dialog_show(dialog) == DIALOG_RESULT_OK)
    {
        if((count = open_dialog_get_selected(dialog, &files)) > 0)
        {
            load_file(files[0].path);
            open_dialog_free_list(files, count);
        }
    }

    open_dialog_destroy(dialog);
}


void menu_file_saveas_handler(winid_t winid)
{
}


void menu_file_properties_handler(winid_t winid)
{
    show_properties_dialog();
}


void menu_file_close_handler(winid_t winid)
{
    window_destroy(main_window);
    gui_exit(EXIT_SUCCESS);
}


void menu_view_zoomin_handler(winid_t winid)
{
    float newzoom = zoom + ZOOM_STEP;

    if(newzoom >= ZOOM_MAX)
    {
        newzoom = ZOOM_MAX;
    }

    if(newzoom == zoom)
    {
        return;
    }

    uncheck_current_zoom();
    zoom = newzoom;
    check_current_zoom();

    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_zoomout_handler(winid_t winid)
{
    float newzoom = zoom - ZOOM_STEP;

    if(newzoom <= ZOOM_MIN)
    {
        newzoom = ZOOM_MIN;
    }

    if(newzoom == zoom)
    {
        return;
    }

    uncheck_current_zoom();
    zoom = newzoom;
    check_current_zoom();

    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_fitwin_handler(winid_t winid)
{
    float zw = (float)backbuf_gc.w / (float)loaded_bitmap.width;
    float zh = (float)backbuf_gc.h / (float)loaded_bitmap.height;
    float newzoom;

    // take the smaller
    newzoom = ((zw < zh) ? zw : zh) * 100.0f;

    uncheck_current_zoom();
    zoom = newzoom;
    check_current_zoom();

    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_origzoom_handler(winid_t winid)
{
    // same code as zoom 100%
    uncheck_current_zoom();
    zoom = zoom_sizes[2];
    menu_item_set_checked(zoom_mi[2], 1);
    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_zoom600_handler(winid_t winid)
{
    uncheck_current_zoom();
    zoom = zoom_sizes[5];
    menu_item_set_checked(zoom_mi[5], 1);
    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_zoom400_handler(winid_t winid)
{
    uncheck_current_zoom();
    zoom = zoom_sizes[4];
    menu_item_set_checked(zoom_mi[4], 1);
    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_zoom200_handler(winid_t winid)
{
    uncheck_current_zoom();
    zoom = zoom_sizes[3];
    menu_item_set_checked(zoom_mi[3], 1);
    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_zoom100_handler(winid_t winid)
{
    uncheck_current_zoom();
    zoom = zoom_sizes[2];
    menu_item_set_checked(zoom_mi[2], 1);
    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_zoom50_handler(winid_t winid)
{
    uncheck_current_zoom();
    zoom = zoom_sizes[1];
    menu_item_set_checked(zoom_mi[1], 1);
    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_view_zoom25_handler(winid_t winid)
{
    uncheck_current_zoom();
    zoom = zoom_sizes[0];
    menu_item_set_checked(zoom_mi[0], 1);
    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_help_shortcuts_handler(winid_t winid)
{
    show_shortcuts_dialog();
}


void menu_help_about_handler(winid_t winid)
{
    show_about_dialog();
}


void create_main_menu(void)
{
    struct menu_item_t *file_menu = mainmenu_new_item(main_window, "&File");
    struct menu_item_t *view_menu = mainmenu_new_item(main_window, "&View");
    struct menu_item_t *help_menu = mainmenu_new_item(main_window, "&Help");
    
    struct menu_item_t *mi;

    /*
     * Create the File menu.
     */
    mi = menu_new_item(file_menu, "&Open");
    mi->handler = menu_file_open_handler;
    // assign the shortcut: CTRL + O
    menu_item_set_shortcut(main_window, mi, KEYCODE_O, MODIFIER_MASK_CTRL);

    saveas_mi = menu_new_item(file_menu, "&Save as ...");
    saveas_mi->handler = menu_file_saveas_handler;
    menu_item_set_enabled(saveas_mi, 0);
    // assign the shortcut: CTRL + S
    menu_item_set_shortcut(main_window, saveas_mi, 
                            KEYCODE_S, MODIFIER_MASK_CTRL);

    properties_mi = menu_new_item(file_menu, "&Properties");
    properties_mi->handler = menu_file_properties_handler;
    menu_item_set_enabled(properties_mi, 0);
    // assign the shortcut: ALT + Enter
    menu_item_set_shortcut(main_window, properties_mi, 
                            KEYCODE_ENTER, MODIFIER_MASK_ALT);

    mi = menu_new_item(file_menu, "E&xit");
    mi->handler = menu_file_close_handler;
    // assign the shortcut: CTRL + Q
    menu_item_set_shortcut(main_window, mi, KEYCODE_Q, MODIFIER_MASK_CTRL);

    /*
     * Create the View menu.
     */
    zoomin_mi = menu_new_item(view_menu, "Zoom &in");
    zoomin_mi->handler = menu_view_zoomin_handler;
    menu_item_set_enabled(zoomin_mi, 0);
    // assign the shortcut: CTRL + '+' (we actually use '=' so the user won't 
    //                                  have to press SHIFT as well)
    menu_item_set_shortcut(main_window, zoomin_mi, 
                            KEYCODE_EQUAL, MODIFIER_MASK_CTRL);

    zoomout_mi = menu_new_item(view_menu, "Zoom &out");
    zoomout_mi->handler = menu_view_zoomout_handler;
    menu_item_set_enabled(zoomout_mi, 0);
    // assign the shortcut: CTRL + '-'
    menu_item_set_shortcut(main_window, zoomout_mi, 
                            KEYCODE_MINUS, MODIFIER_MASK_CTRL);

    menu_new_item(view_menu, "-");

    fitwin_mi = menu_new_item(view_menu, "Fit to window");
    fitwin_mi->handler = menu_view_fitwin_handler;
    menu_item_set_enabled(fitwin_mi, 0);

    origzoom_mi = menu_new_item(view_menu, "Original size");
    origzoom_mi->handler = menu_view_origzoom_handler;
    menu_item_set_enabled(origzoom_mi, 0);

    menu_new_item(view_menu, "-");

    zoom_mi[5] = menu_new_checked_item(view_menu, "Zoom &600%");
    zoom_mi[5]->handler = menu_view_zoom600_handler;
    menu_item_set_enabled(zoom_mi[5], 0);

    zoom_mi[4] = menu_new_checked_item(view_menu, "Zoom &400%");
    zoom_mi[4]->handler = menu_view_zoom400_handler;
    menu_item_set_enabled(zoom_mi[4], 0);

    zoom_mi[3] = menu_new_checked_item(view_menu, "Zoom &200%");
    zoom_mi[3]->handler = menu_view_zoom200_handler;
    menu_item_set_enabled(zoom_mi[3], 0);

    zoom_mi[2] = menu_new_checked_item(view_menu, "Zoom &100%");
    zoom_mi[2]->handler = menu_view_zoom100_handler;
    menu_item_set_enabled(zoom_mi[2], 0);

    zoom_mi[1] = menu_new_checked_item(view_menu, "Zoom &50%");
    zoom_mi[1]->handler = menu_view_zoom50_handler;
    menu_item_set_enabled(zoom_mi[1], 0);

    zoom_mi[0] = menu_new_checked_item(view_menu, "Zoom 25%");
    zoom_mi[0]->handler = menu_view_zoom25_handler;
    menu_item_set_enabled(zoom_mi[0], 0);

    /*
     * Create the Help menu.
     */
    mi = menu_new_item(help_menu, "&Keyboard shortcuts");
    mi->handler = menu_help_shortcuts_handler;
    // assign the shortcut: CTRL + F1
    menu_item_set_shortcut(main_window, mi, KEYCODE_F1, MODIFIER_MASK_CTRL);

    mi = menu_new_item(help_menu, "&About");
    mi->handler = menu_help_about_handler;

    finalize_menus(main_window);
}


int main(int argc, char **argv)
{
    struct event_t *ev = NULL;
    struct window_attribs_t attribs;

    gui_init(argc, argv);

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = WIN_MIN_WIDTH;
    attribs.h = WIN_MIN_HEIGHT + MENU_HEIGHT;
    attribs.flags = WINDOW_HASMENU;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n",
                argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    create_main_menu();

    if(gc_alloc_backbuf(main_window->gc, &backbuf_gc,
                        WIN_MIN_WIDTH, WIN_MIN_HEIGHT) < 0)
    {
        fprintf(stderr, "%s: failed to create back buffer: %s\n",
                argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    window_set_title(main_window, APP_TITLE);
    window_set_icon(main_window, "image2.ico");
    window_set_min_size(main_window, WIN_MIN_WIDTH, WIN_MIN_HEIGHT);

    main_window->repaint = repaint;
    main_window->size_changed = size_changed;
    //main_window->mousedown = process_mousedown;
    //main_window->mouseup = process_mouseup;
    main_window->mouseover = process_mouseover;

    window_repaint(main_window);
    window_show(main_window);

    if(argc > 1)
    {
        load_file(argv[1]);
    }

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
                window_destroy(main_window);

                gui_exit(EXIT_SUCCESS);
                break;

            default:
                break;
        }

        free(ev);
    }
}

