/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: messagebox-shortcuts.c
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
 *  \file messagebox-shortcuts.c
 *
 *  The implementation of a "Keyboard shortcuts" dialog box.
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
 * A Keyboard Shortcuts dialog box looks something like:
 *
 *     +----------------------------------------------------+
 *     |                   Dialog box title                 |
 *     +----------------------------------------------------+
 *     |                                                    |
 *     | Shortcut #1       What does it do                  |
 *     | Shortcut #2       Same                             |
 *     | ...                                                |
 *     | Shortcut #n       Same                             |
 *     |                                                    |
 *     |                                        +---------+ |
 *     |                                        |  Close  | |
 *     |                                        +---------+ |
 *     +----------------------------------------------------+
 */

struct shortcuts_dialog_t *shortcuts_dialog_create(winid_t owner,
                                                   char **shortcuts,
                                                   char **descriptions)
{
    struct shortcuts_dialog_t *dialog;
    int count1 = 0, count2 = 0;
    char **p;

    // ensure we got valid arrays with at least one item each
    if(!shortcuts || !*shortcuts || !descriptions || !*descriptions)
    {
        return NULL;
    }

    // ensure arrays are of the same length
    for(p = shortcuts; *p; p++)
    {
        count1++;
    }

    for(p = descriptions; *p; p++)
    {
        count2++;
    }

    if(count1 != count2)
    {
        return NULL;
    }

    if(!(dialog = malloc(sizeof(struct shortcuts_dialog_t))))
    {
        return NULL;
    }
    
    A_memset(dialog, 0, sizeof(struct shortcuts_dialog_t));
    dialog->ownerid = owner;
    dialog->str.shortcuts = shortcuts;
    dialog->str.descriptions = descriptions;
    
    return dialog;
}


static void dialog_button_handler(struct button_t *button, int x, int y)
{
    struct window_t *dialog_window = (struct window_t *)button->window.parent;
    struct dialog_status_t *status = dialog_window->internal_data;
    
    status->close_dialog = 1;
}


int shortcuts_dialog_show(struct shortcuts_dialog_t *dialog)
{
    int lines = 1;
    int longest_shortcutw = 0, longest_descriptionw = 0;
    size_t sz;
    char *shortcuts = NULL, *descriptions = NULL;
    int linew = 0;
    int x, y, w;
    char **s, *p;

    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    int charh = char_height(font, ' ');

    // get the shortcut line count and the longest line width
    for(sz = 0, s = dialog->str.shortcuts; *s; s++, lines++)
    {
        if((linew = string_width(font, *s)) > longest_shortcutw)
        {
            longest_shortcutw = linew;
        }

        sz += strlen(*s) + 1;
    }

    if(!(shortcuts = malloc(sz + 1)))
    {
        /*
         * TODO: show an error message perhaps?
         */
        return 0;
    }

    // now amalgamate them into one big string
    for(p = shortcuts, s = dialog->str.shortcuts; *s; s++)
    {
        sprintf(p, "%s\n", *s);
        p += strlen(p);
    }

    // get the description line count and the longest line width
    for(sz = 0, s = dialog->str.descriptions; *s; s++)
    {
        if((linew = string_width(font, *s)) > longest_descriptionw)
        {
            longest_descriptionw = linew;
        }

        sz += strlen(*s) + 1;
    }

    if(!(descriptions = malloc(sz + 1)))
    {
        /*
         * TODO: show an error message perhaps?
         */
        return 0;
    }

    // now amalgamate them into one big string
    for(p = descriptions, s = dialog->str.descriptions; *s; s++)
    {
        sprintf(p, "%s\n", *s);
        p += strlen(p);
    }
    
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    struct window_attribs_t attribs;
    struct button_t *button;
    struct label_t *label;
    struct dialog_status_t status;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = (LABEL_PADDING * 6) + longest_shortcutw + longest_descriptionw;
    attribs.h = (LABEL_PADDING * 4) + (lines * charh) + BUTTON_HEIGHT;
    attribs.flags = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | WINDOW_SKIPTASKBAR;

    if(attribs.w < MIN_DIALOG_WIDTH)
    {
        attribs.w = MIN_DIALOG_WIDTH;
    }

    if(!(dialog->window = __window_create(&attribs, WINDOW_TYPE_DIALOG, 
                                                    dialog->ownerid)))
    {
        free(shortcuts);
        free(descriptions);
        return 0;
    }

    dialog->window->event_handler = messagebox_dispatch_event;
    status.close_dialog = 0;
    dialog->window->internal_data = &status;
    window_set_title(dialog->window, dialog->title ? dialog->title :
                                                     "Keyboard Shortcuts");

    // create a label for the shortcuts (left side column)
    x = LABEL_PADDING * 2;
    y = LABEL_PADDING * 2;
    w = longest_shortcutw;
    label = label_new(dialog->window->gc, dialog->window, x, y,
                      w, (lines * charh), shortcuts);
    label_set_text_alignment(label, TEXT_ALIGN_LEFT);

    // create a label for the descriptions (right side column)
    x += (LABEL_PADDING * 2) + longest_shortcutw;
    w = longest_descriptionw;
    label = label_new(dialog->window->gc, dialog->window, x, y,
                      w, (lines * charh), descriptions);
    label_set_text_alignment(label, TEXT_ALIGN_LEFT);

    // create the button
    y += (lines * charh) + LABEL_PADDING;
    x = dialog->window->w - BUTTON_WIDTH - LABEL_PADDING;
    button = button_new(dialog->window->gc, dialog->window,
                        x, y, BUTTON_WIDTH, BUTTON_HEIGHT, "Close");
    button->button_click_callback = dialog_button_handler;

    // now paint and show the dialog box
    window_repaint(dialog->window);
    simple_request(REQUEST_DIALOG_SHOW, GLOB.server_winid, 
                                        dialog->window->winid);
    dialog->window->flags &= ~WINDOW_HIDDEN;

    while(1)
    {
        struct event_t *ev = NULL;
        
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
    
    free(shortcuts);
    free(descriptions);

    return 1;
}


void shortcuts_dialog_destroy(struct shortcuts_dialog_t *dialog)
{
    if(!dialog || !dialog->window)
    {
        return;
    }
    
    window_destroy_children(dialog->window);
    window_destroy(dialog->window);
    free(dialog);
}


void shortcuts_dialog_set_title(struct shortcuts_dialog_t *dialog, char *title)
{
    dialog->title = title;
}

