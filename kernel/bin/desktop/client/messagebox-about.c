/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: messagebox-about.c
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
 *  \file messagebox-about.c
 *
 *  The implementation of an "About this software" dialog box.
 */

#include "../include/event.h"
#include "../include/resources.h"
#include "../include/client/button.h"
#include "../include/client/label.h"
#include "../include/client/dialog.h"

#include "inlines.c"

#define GLOB                        __global_gui_data

#define BUTTON_WIDTH                68
#define BUTTON_HEIGHT               30
#define ICON_HEIGHT                 64
#define LABEL_PADDING               8
#define MIN_DIALOG_WIDTH            (BUTTON_WIDTH * 5)


extern void messagebox_dispatch_event(struct event_t *ev);


/*
 * An About dialog box looks something like:
 *
 *     +-------------------------------------------------------+
 *     |                   About box title                     |
 *     +-------------------------------------------------------+
 *     |                       +------+                        |
 *     |                       | Icon |                        |
 *     |                       +------+                        |
 *     |                                                       |
 *     |                  Application's name                   |
 *     |                 Application's version                 |
 *     |                Application's about text               |
 *     |                                                       |
 *     |             Application's copyright notice            |
 *     |                                                       |
 *     | +---------+  +---------+  +--------+      +---------+ |
 *     | | Credits |  | License |  |  Help  |      |  Close  | |
 *     | +---------+  +---------+  +--------+      +---------+ |
 *     +-------------------------------------------------------+
 */

struct about_dialog_t *aboutbox_create(winid_t owner)
{
    struct about_dialog_t *dialog;
    
    if(!(dialog = malloc(sizeof(struct about_dialog_t))))
    {
        return NULL;
    }
    
    A_memset(dialog, 0, sizeof(struct about_dialog_t));
    dialog->ownerid = owner;

    dialog->app_icon_resid = window_icon_get(owner, &dialog->app_icon);
    
    return dialog;
}


static void dialog_button_handler(struct button_t *button, int x, int y)
{
    struct window_t *dialog_window = (struct window_t *)button->window.parent;
    struct dialog_status_t *status = dialog_window->internal_data;
    
    status->close_dialog = 1;
}


int aboutbox_show(struct about_dialog_t *dialog)
{
    int lines = 1;
    int longest_line_chars = 0;
    int line_chars = 0;
    int x, y, w;
    char *s;

    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    int charh = char_height(font, ' ');

    // get the about message line count and the longest line length
    if(dialog->str.about)
    {
        for(s = dialog->str.about; *s; s++)
        {
            if(*s == '\n')
            {
                if(line_chars > longest_line_chars)
                {
                    longest_line_chars = line_chars;
                }

                line_chars = 0;
                lines++;
            }
            else
            {
                line_chars++;
            }
        }
    }
    
    if(line_chars == 0)
    {
        line_chars = 64;
    }

    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    // check the last (or the only) line length
    if(line_chars > longest_line_chars)
    {
        longest_line_chars = line_chars;
    }

    struct window_attribs_t attribs;
    struct button_t *button;
    struct label_t *label;
    struct dialog_status_t status;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = (LABEL_PADDING * 2) + (longest_line_chars * GLOB.mono.charw);
    attribs.h = (LABEL_PADDING * 9) + (lines * charh) + BUTTON_HEIGHT;
    // add some room for the icon and the app's name, version and copyright text
    attribs.h += 64 + (charh * 3);
    attribs.flags = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | WINDOW_SKIPTASKBAR;

    if(attribs.w < MIN_DIALOG_WIDTH)
    {
        attribs.w = MIN_DIALOG_WIDTH;
    }

    if(!(dialog->window = __window_create(&attribs, WINDOW_TYPE_DIALOG, 
                                                    dialog->ownerid)))
    {
        return 0;
    }

    dialog->window->event_handler = messagebox_dispatch_event;
    status.close_dialog = 0;
    dialog->window->internal_data = &status;
    window_set_title(dialog->window, dialog->title ? dialog->title : "About");

    // create a label for the application's name
    x = LABEL_PADDING;
    y = ICON_HEIGHT + (LABEL_PADDING * 2);
    w = dialog->window->w - (LABEL_PADDING * 2);
    label = label_new(dialog->window->gc, dialog->window, x, y,
                      w, charh,
                      dialog->str.name ? dialog->str.name : "(NULL)");
    label_set_text_alignment(label, TEXT_ALIGN_CENTERH);

    // create a label for the application's version
    y += charh + LABEL_PADDING;
    label = label_new(dialog->window->gc, dialog->window, x, y,
                      w, charh,
                      dialog->str.ver ? dialog->str.ver : "1.0.0");
    label_set_text_alignment(label, TEXT_ALIGN_CENTERH);

    // create a label for the application's about text
    y += charh + LABEL_PADDING;
    label = label_new(dialog->window->gc, dialog->window, x, y,
                      w, (lines * charh),
                      dialog->str.about ? dialog->str.about : "(NULL)");
    label_set_text_alignment(label, TEXT_ALIGN_CENTERH);

    // create a label for the application's copyright notice
    y += (lines * charh) + (LABEL_PADDING * 2);
    label = label_new(dialog->window->gc, dialog->window, x, y,
                      w, charh,
                      dialog->str.copyright ? dialog->str.copyright : 
                                              "Copyright (c)");
    label_set_text_alignment(label, TEXT_ALIGN_CENTERH);

    // create the buttons
    y += charh + (LABEL_PADDING * 2);
    
    if(dialog->callbacks.credits)
    {
        button = button_new(dialog->window->gc, dialog->window,
                            x, y, BUTTON_WIDTH, BUTTON_HEIGHT, "Credits");
        button->button_click_callback = dialog->callbacks.credits;
        x += BUTTON_WIDTH + 8;
    }

    if(dialog->callbacks.license)
    {
        button = button_new(dialog->window->gc, dialog->window,
                            x, y, BUTTON_WIDTH, BUTTON_HEIGHT, "License");
        button->button_click_callback = dialog->callbacks.license;
        x += BUTTON_WIDTH + 8;
    }

    if(dialog->callbacks.help)
    {
        button = button_new(dialog->window->gc, dialog->window,
                            x, y, BUTTON_WIDTH, BUTTON_HEIGHT, "Help");
        button->button_click_callback = dialog->callbacks.help;
    }

    x = dialog->window->w - BUTTON_WIDTH - LABEL_PADDING;
    button = button_new(dialog->window->gc, dialog->window,
                        x, y, BUTTON_WIDTH, BUTTON_HEIGHT, "Close");
    button->button_click_callback = dialog_button_handler;

    // now paint and show the dialog box
    window_repaint(dialog->window);
    
    // paint the icon (if we have it)
    if(dialog->app_icon_resid != INVALID_RESID)
    {
        gc_blit_bitmap_highlighted(dialog->window->gc, 
                                   &dialog->app_icon,
                                   (dialog->window->w - ICON_HEIGHT) / 2,
                                   LABEL_PADDING,
                                   0, 0, ICON_HEIGHT, ICON_HEIGHT, 0);
    }

    //window_show(dialog_window);
    simple_request(REQUEST_DIALOG_SHOW, GLOB.server_winid, 
                                        dialog->window->winid);
    dialog->window->flags &= ~WINDOW_HIDDEN;

    while(1)
    {
        struct event_t *ev = NULL;
        
        /*
        if((ev = next_event()))
        {
            if(win_for_winid(ev->dest) == dialog_window)
            {
                messagebox_dispatch_event(ev);
            }
        }
        */
        if((ev = next_event_for_seqid(/* NULL */ dialog->window, 0, 0)))
        {
            //if(win_for_winid(ev->dest) == dialog->window)
            {
                messagebox_dispatch_event(ev);
            }

            free(ev);
        }
        
        if(status.close_dialog)
        {
            break;
        }
    }
    
    return 1;
}


void aboutbox_destroy(struct about_dialog_t *dialog)
{
    if(!dialog || !dialog->window)
    {
        return;
    }
    
    window_destroy_children(dialog->window);
    window_destroy(dialog->window);
    free(dialog);
}


void aboutbox_set_title(struct about_dialog_t *dialog, char *title)
{
    dialog->title = title;
}


void aboutbox_set_name(struct about_dialog_t *dialog, char *app_name)
{
    dialog->str.name = app_name;
}


void aboutbox_set_version(struct about_dialog_t *dialog, char *app_ver)
{
    dialog->str.ver = app_ver;
}


void aboutbox_set_about(struct about_dialog_t *dialog, char *app_about)
{
    dialog->str.about = app_about;
}


void aboutbox_set_copyright(struct about_dialog_t *dialog, char *app_copyright)
{
    dialog->str.copyright = app_copyright;
}


void aboutbox_credits_callback(struct about_dialog_t *dialog,
                               void (*fn)(struct button_t *, int, int))
{
    dialog->callbacks.credits = fn;
}


void aboutbox_license_callback(struct about_dialog_t *dialog,
                               void (*fn)(struct button_t *, int, int))
{
    dialog->callbacks.license = fn;
}


void aboutbox_help_callback(struct about_dialog_t *dialog,
                            void (*fn)(struct button_t *, int, int))
{
    dialog->callbacks.help = fn;
}

