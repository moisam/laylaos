/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: opensave-dialog.c
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
 *  \file opensave-dialog.c
 *
 *  The implementation of Open and Save dialog boxes.
 */

#include <string.h>
#include <sys/stat.h>
#include "../include/client/label.h"
#include "../include/client/dialog.h"
#include "../include/client/combobox.h"
#include "../include/keys.h"
#include "../include/cursor.h"

#include "inlines.c"

#include "../app_files/history.c"

#define LOCATION_BAR_HEIGHT         (INPUTBOX_HEIGHT + 8)
#define BUTTON_WIDTH                80
#define BUTTON_HEIGHT               30

#define GLOB                        __global_gui_data

#define ISSPACE(c)                  (c == ' ' || c == '\t' || c == '\n'|| c == '\r')

extern void messagebox_dispatch_event(struct event_t *ev);

// To indicate if a dialog box is currently shown
static volatile int dialog_shown = 0;

#define __          0x00000000,
#define _T          GLOBAL_BLACK_COLOR,

static uint32_t __iconview_button_img[] =
{
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
};

static uint32_t __listview_button_img[] =
{
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ _T _T _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ _T _T _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ _T _T _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
};

static uint32_t __compactview_button_img[] =
{
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ _T _T _T _T _T __ __ _T _T _T _T _T __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
    __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
};

#undef __
#undef _T

static struct bitmap32_t iconview_button_img =
{
    .width = 24, .height = 24, .data = __iconview_button_img,
};

static struct bitmap32_t listview_button_img =
{
    .width = 24, .height = 24, .data = __listview_button_img,
};

static struct bitmap32_t compactview_button_img =
{
    .width = 24, .height = 24, .data = __compactview_button_img,
};


/********************************
 * Internal functions
 ********************************/

static struct opensave_dialog_t *__dialog_create(winid_t owner, int type)
{
    struct opensave_dialog_t *dialog;
    
    if(!(dialog = malloc(sizeof(struct opensave_dialog_t))))
    {
        return NULL;
    }
    
    A_memset(dialog, 0, sizeof(struct opensave_dialog_t));
    dialog->ownerid = owner;
    dialog->type = type;

    // multiselect if this is an Open dialog box
    dialog->multiselect = (type == DIALOGBOX_OPEN);

    hist_current = -1;
    hist_last = -1;
    
    return dialog;
}


static void dialog_button_handler(struct button_t *button, int x, int y)
{
    struct window_t *dialog_window = (struct window_t *)button->window.parent;
    struct dialog_status_t *status = dialog_window->internal_data;
    
    status->selected_button = (int)(intptr_t)button->internal_data;
    status->close_dialog = 1;
    //dialog_window->internal_data = button->internal_data;
}


static inline char *type_to_title(int type)
{
    switch(type)
    {
        case DIALOGBOX_SAVE: return "Save";
        case DIALOGBOX_SAVEAS: return "Save as..";
        default: return "Open";
    }
}


static void adjust_back_forward_buttons(struct window_t *dialog_window)
{
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;
    int hist_cur = get_history_current();

    // if we are at the beginning of the history list, disable the 
    // Go -> Back button
    if(hist_cur <= 0)
    {
        imgbutton_disable(internal->imgbutton_back);
    }
    else
    {
        imgbutton_enable(internal->imgbutton_back);
    }

    // if we are at the end of the history list, disable the 
    // Go -> Forward button
    if(hist_cur >= get_history_last())
    {
        imgbutton_disable(internal->imgbutton_forward);
    }
    else
    {
        imgbutton_enable(internal->imgbutton_forward);
    }
}


static int reload_path(struct window_t *dialog_window, char *newdir)
{
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;
    int res;

    cursor_show(dialog_window, CURSOR_WAITING);
    res = file_selector_set_path(internal->selector, newdir);
    cursor_show(dialog_window, CURSOR_NORMAL);

    if(res == 0)
    {
        if(internal->curdir)
        {
            free(internal->curdir);
        }
        
        internal->curdir = newdir;
        inputbox_set_text((struct window_t *)internal->location_bar, internal->curdir);
        inputbox_set_text((struct window_t *)internal->filename_inputbox, "");
        window_repaint(dialog_window);
        window_invalidate(dialog_window);

        // if current directory is not root, enable the Go to Parent button
        if(internal->curdir[0] == '/' && internal->curdir[1] == '\0')
        {
            imgbutton_disable(internal->imgbutton_up);
        }
        else
        {
            imgbutton_enable(internal->imgbutton_up);
        }

        return 0;
    }
    else
    {
        return -1;
    }
}


static void imgbutton_back_handler(struct imgbutton_t *button, int x, int y)
{
    struct window_t *dialog_window = (struct window_t *)button->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;
    char *newdir;
    
    if(get_history_current() <= 0)
    {
        adjust_back_forward_buttons(button->window.parent);
        return;
    }
    
    newdir = strdup(history_back());

    if(reload_path(button->window.parent, newdir) != 0)
    {
        free(newdir);
    }
    
    adjust_back_forward_buttons(button->window.parent);

    // give focus back to the file selector so the user can interact with it
    window_set_focus_child(button->window.parent, 
                           (struct window_t *)internal->selector);
}


static void imgbutton_forward_handler(struct imgbutton_t *button, int x, int y)
{
    struct window_t *dialog_window = button->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;
    char *newdir;
    
    if(get_history_current() >= get_history_last())
    {
        adjust_back_forward_buttons(dialog_window);
        return;
    }

    newdir = strdup(history_forward());

    if(reload_path(dialog_window, newdir) != 0)
    {
        free(newdir);
    }
    
    adjust_back_forward_buttons(dialog_window);

    // give focus back to the file selector so the user can interact with it
    window_set_focus_child(dialog_window, (struct window_t *)internal->selector);
}


static void imgbutton_up_handler(struct imgbutton_t *button, int x, int y)
{
    struct window_t *dialog_window = button->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;

    if(!internal->curdir || !*internal->curdir)
    {
        return;
    }

    char *slash = internal->curdir + strlen(internal->curdir) - 1;
    char *newdir;
    
    while(slash > internal->curdir && *slash != '/')
    {
        slash--;
    }
    
    if(slash < internal->curdir || *slash != '/')
    {
        return;
    }

    if(slash == internal->curdir)
    {
        newdir = strdup("/");
    }
    else
    {
        *slash = '\0';
        newdir = strdup(internal->curdir);
        *slash = '/';
    }
    
    if(reload_path(dialog_window, newdir) != 0)
    {
        free(newdir);
    }
    else
    {
        history_push(newdir);
        adjust_back_forward_buttons(dialog_window);
    }

    // give focus back to the file selector so the user can interact with it
    window_set_focus_child(dialog_window, (struct window_t *)internal->selector);
}


static void imgbutton_iconview_handler(struct imgbutton_t *button)
{
    struct window_t *dialog_window = button->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;

    // make the push state "sticky"
    if(!internal->imgbutton_iconview->push_state)
    {
        imgbutton_set_push_state(internal->imgbutton_iconview, 1);
    }

    imgbutton_set_push_state(internal->imgbutton_listview, 0);
    imgbutton_set_push_state(internal->imgbutton_compactview, 0);

    file_selector_set_viewmode(internal->selector, FILE_SELECTOR_ICON_VIEW);
    window_repaint(dialog_window);
    window_invalidate(dialog_window);
}


static void imgbutton_listview_handler(struct imgbutton_t *button)
{
    struct window_t *dialog_window = button->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;

    // make the push state "sticky"
    if(!internal->imgbutton_listview->push_state)
    {
        imgbutton_set_push_state(internal->imgbutton_listview, 1);
    }

    imgbutton_set_push_state(internal->imgbutton_compactview, 0);
    imgbutton_set_push_state(internal->imgbutton_iconview, 0);

    file_selector_set_viewmode(internal->selector, FILE_SELECTOR_LIST_VIEW);
    window_repaint(dialog_window);
    window_invalidate(dialog_window);
}


static void imgbutton_compactview_handler(struct imgbutton_t *button)
{
    struct window_t *dialog_window = button->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;

    // make the push state "sticky"
    if(!internal->imgbutton_compactview->push_state)
    {
        imgbutton_set_push_state(internal->imgbutton_compactview, 1);
    }

    imgbutton_set_push_state(internal->imgbutton_listview, 0);
    imgbutton_set_push_state(internal->imgbutton_iconview, 0);

    file_selector_set_viewmode(internal->selector, FILE_SELECTOR_COMPACT_VIEW);
    window_repaint(dialog_window);
    window_invalidate(dialog_window);
}


static int locationbar_keypress(struct window_t *inputbox_window, 
                                char code, char modifiers)
{
    // call the standard function to do all the hardwork
    int res = inputbox_keypress(inputbox_window, code, modifiers);
    
    // if the user pressed Enter, navigate to the new path
    if(code == KEYCODE_ENTER)
    {
        struct window_t *dialog_window = inputbox_window->parent;
        struct __opensave_internal_state_t *internal = dialog_window->internal_data;
        char *newdir = realpath(inputbox_window->title, NULL);

        if(!newdir)
        {
            return 1;
        }

        if(reload_path(inputbox_window->parent, newdir) != 0)
        {
            free(newdir);
        }
        else
        {
            history_push(newdir);
            adjust_back_forward_buttons(dialog_window);

            // give focus back to the file selector so the user can interact with it
            window_set_focus_child(dialog_window, 
                                    (struct window_t *)internal->selector);
        }
    }
    
    return res;
}


static int filename_inputbox_keypress(struct window_t *inputbox_window, 
                                      char code, char modifiers)
{
    // call the standard function to do all the hardwork
    int res = inputbox_keypress(inputbox_window, code, modifiers);
    
    // if the user pressed Enter, navigate to the new path
    if(code == KEYCODE_ENTER)
    {
        struct window_t *dialog_window = (struct window_t *)inputbox_window->parent;
        struct __opensave_internal_state_t *internal = dialog_window->internal_data;

        // emulate a click on the Open/Save button
        internal->status.selected_button = (int)(intptr_t)0;
        internal->status.close_dialog = 1;
    }

    return res;
}


static void fileentry_doubleclick_callback(struct file_selector_t *selector,
                                           struct file_entry_t *entry)
{
    struct window_t *dialog_window = (struct window_t *)selector->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;

    if(!S_ISDIR(entry->mode) || !internal->curdir)
    {
        // emulate a click on the Open/Save button
        internal->status.selected_button = (int)(intptr_t)0;
        internal->status.close_dialog = 1;

        return;
    }
    
    size_t curdir_len = strlen(internal->curdir);
    size_t len = curdir_len + strlen(entry->name);
    char *newdir;
    
    if(!(newdir = malloc(len + 2)))
    {
        return;
    }
    
    if(internal->curdir[curdir_len - 1] == '/')
    {
        sprintf(newdir, "%s%s", internal->curdir, entry->name);
    }
    else
    {
        sprintf(newdir, "%s/%s", internal->curdir, entry->name);
    }

    if(reload_path(dialog_window, newdir) != 0)
    {
        free(newdir);
    }
    else
    {
        history_push(newdir);
        adjust_back_forward_buttons(dialog_window);
    }
}


static void fileentry_selection_change_callback(struct file_selector_t *selector)
{
    struct window_t *dialog_window = (struct window_t *)selector->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;
    struct window_t *inputbox = (struct window_t *)internal->filename_inputbox;
    struct file_entry_t *entries, *entry;
    int count, file_count = 0;
    size_t buflen = 0;
    char *buf, *p;

    if((count = file_selector_get_selected(selector, &entries)) <= 0)
    {
        return;
    }

    for(entry = &entries[0]; entry < &entries[count]; entry++)
    {
        // count files, not directories
        if(!S_ISDIR(entry->mode))
        {
            // add 3 to account for possible quotes and space
            buflen += strlen(entry->name) + 3;
            file_count++;
        }
    }

    if(file_count == 0)
    {
        file_selector_free_list(entries, count);
        return;
    }
    
    if(!(buf = malloc(buflen + 1)))
    {
        file_selector_free_list(entries, count);
        return;
    }

    if(file_count == 1)
    {
        // find the file
        for(entry = &entries[0]; entry < &entries[count]; entry++)
        {
            if(!S_ISDIR(entry->mode))
            {
                sprintf(buf, "%s", entry->name);
                break;
            }
        }
    }
    else
    {
        p = buf;

        for(entry = &entries[0]; entry < &entries[count]; entry++)
        {
            if(!S_ISDIR(entry->mode))
            {
                sprintf(p, "\"%s\" ", entry->name);
                p += strlen(p);
            }
        }
    }

    file_selector_free_list(entries, count);
    inputbox_set_text(inputbox, buf);
    free(buf);

    inputbox_repaint(inputbox, IS_ACTIVE_CHILD(inputbox));
    child_invalidate(inputbox);
}


void fileentry_click_callback(struct file_selector_t *selector, 
                              struct file_entry_t *entry)
{
    fileentry_selection_change_callback(selector);
}


static int is_valid_filename(char *name)
{
    char *p;
    int quotes = 0;

    // make sure it is not an empty name
    if(!name || !*name)
    {
        return 0;
    }

    // and does not begin with a space
    if(ISSPACE(*name))
    {
        return 0;
    }

    // and has no invalid characters
    for(p = name; *p; p++)
    {
        if(/* ISSPACE(*p) || */ *p == '^' || *p == '<' || *p == '>' ||
           *p == '|' || *p == '/' || *p == '\\' || *p == ':' || *p == '*')
        {
            return 0;
        }
    }

    // and if we have multiple filenames separated by quotes, make sure we
    // have an even number of quotes
    for(p = name; *p; p++)
    {
        if(*p == '"')
        {
            quotes++;
        }
    }

    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(quotes % 2)
    {
        return 0;
    }

    return 1;
}


static int get_filters(struct opensave_dialog_t *dialog, char ***res)
{
    char *str = dialog->filetype_filter;
    char *p, *pipe, **list;
    int i, count = 0;

    if(str == NULL)
    {
        return 0;
    }

    p = str;

    // count the filters first
    while(*p)
    {
        // filters should be formatted as: "name1|type1|name2|type2|..."
        if(!(pipe = strchr(p, '|')))
        {
            // malformed string, bail out
            break;
        }

        p = pipe + 1;
        count++;

        if(!(pipe = strchr(p, '|')))
        {
            // last filter
            break;
        }

        p = pipe + 1;
    }

    if(count == 0)
    {
        return 0;
    }

    if(!(list = calloc(count * sizeof(char **) * 2, 1)))
    {
        return 0;
    }

    p = str;
    i = 0;

    // now get the filter list
    while(*p)
    {
        pipe = strchr(p, '|');

        // copy the name
        if((list[i] = malloc(pipe - p + 1)))
        {
            strncpy(list[i], p, pipe - p);
            list[i][pipe - p] = '\0';
        }

        p = pipe + 1;

        // copy the filter pattern
        if(!(pipe = strchr(p, '|')))
        {
            // last filter
            if((list[i + 1] = malloc(strlen(p) + 1)))
            {
                strcpy(list[i + 1], p);
            }

            break;
        }
        else
        {
            if((list[i + 1] = malloc(pipe - p + 1)))
            {
                strncpy(list[i + 1], p, pipe - p);
                list[i + 1][pipe - p] = '\0';
            }
        }

        p = pipe + 1;
        i += 2;
    }

    *res = list;

    return count;
}


static void add_filters_to_selector(struct file_selector_t *selector,
                                    char *filter)
{
    char *semicolon, *p;
    char buf[64];

    file_selector_clear_filters(selector);

    if(!(semicolon = strchr(filter, ';')))
    {
        // only one filter
        file_selector_add_filter(selector, filter);
    }
    else
    {
        // multiple filters separated by semicolons
        p = filter;

        while(*p)
        {
            // make sure it is a filter of reasonable length
            if(semicolon - p < sizeof(buf))
            {
                strncpy(buf, p, semicolon - p);
                buf[semicolon - p] = '\0';
                file_selector_add_filter(selector, buf);
            }

            p = semicolon + 1;

            if(!(semicolon = strchr(p, ';')))
            {
                // last filter
                file_selector_add_filter(selector, p);
                break;
            }
        }
    }
}


static void add_filters_to_combobox(struct combobox_t *combobox,
                                    int filter_count, char **filter_list)
{
    int i;

    if(!filter_count)
    {
        return;
    }

    for(i = 0; i < filter_count; i++)
    {
        combobox_append_item(combobox, filter_list[i * 2]);
    }

    combobox_set_text((struct window_t *)combobox, filter_list[0]);
    combobox_set_selected_item(combobox, 0);
}


static void free_filter_list(int filter_count, char **filter_list)
{
    int i;

    for(i = 0; i < filter_count * 2; i++)
    {
        free(filter_list[i]);
        filter_list[i] = NULL;
    }

    free(filter_list);
}


static void combobox_entry_click_callback(struct combobox_t *combobox,
                                          int selindex)
{
    struct window_t *dialog_window = (struct window_t *)combobox->window.parent;
    struct __opensave_internal_state_t *internal = dialog_window->internal_data;

    add_filters_to_selector(internal->selector, 
                            internal->filter_list[selindex * 2 + 1]);
    file_selector_reload_entries(internal->selector);
    window_repaint(dialog_window);
    window_invalidate(dialog_window);
}


static int __dialog_show(struct opensave_dialog_t *dialog)
{
    int i, y;
    char *home = getenv("HOME");

    struct window_attribs_t attribs;
    struct button_t *button;
    struct __opensave_internal_state_t *internal = &dialog->internal;

    // ensure only one dialog is shown at a time
    if(dialog_shown)
    {
        return DIALOG_RESULT_CANCEL;
    }

    // create the window
    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 450;
    attribs.h = 300;
    attribs.flags = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | 
                    WINDOW_SKIPTASKBAR | WINDOW_NOICON;

    if(!(dialog->window = __window_create(&attribs, WINDOW_TYPE_DIALOG, 
                                                    dialog->ownerid)))
    {
        return 0;
    }

    dialog_shown = 1;
    dialog->window->event_handler = messagebox_dispatch_event;
    internal->status.close_dialog = 0;
    dialog->window->internal_data = internal;

    window_set_title(dialog->window, type_to_title(dialog->type));

    // create the user interface

    // the "Go Back" button (left arrow)
    internal->imgbutton_back = imgbutton_new(dialog->window->gc, dialog->window,
                                           2, 4, 28, 28);
    imgbutton_set_sysicon(internal->imgbutton_back, "sign-left");
    imgbutton_set_bordered(internal->imgbutton_back, 0);
    imgbutton_disable(internal->imgbutton_back);
    internal->imgbutton_back->button_click_callback = imgbutton_back_handler;

    // the "Go Forward" button (right arrow)
    internal->imgbutton_forward = imgbutton_new(dialog->window->gc, 
                                                dialog->window,
                                                30, 4, 28, 28);
    imgbutton_set_sysicon(internal->imgbutton_forward, "sign-right");
    imgbutton_set_bordered(internal->imgbutton_forward, 0);
    imgbutton_disable(internal->imgbutton_forward);
    internal->imgbutton_forward->button_click_callback = imgbutton_forward_handler;

    // the "Go to Parent" button (up arrow)
    internal->imgbutton_up = imgbutton_new(dialog->window->gc, dialog->window,
                                           62, 4, 28, 28);
    imgbutton_set_sysicon(internal->imgbutton_up, "sign-up");
    imgbutton_set_bordered(internal->imgbutton_up, 0);
    internal->imgbutton_up->button_click_callback = imgbutton_up_handler;

    // the "Icon View" button (custom bitmap)
    internal->imgbutton_iconview = push_imgbutton_new(dialog->window->gc, 
                                            dialog->window,
                                            dialog->window->w - 94, 4, 28, 28);
    imgbutton_set_image(internal->imgbutton_iconview, &iconview_button_img);
    internal->imgbutton_iconview->push_state_change_callback = 
                                            imgbutton_iconview_handler;

    // the "List View" button (custom bitmap)
    internal->imgbutton_listview = push_imgbutton_new(dialog->window->gc, 
                                            dialog->window,
                                            dialog->window->w - 66, 4, 28, 28);
    imgbutton_set_image(internal->imgbutton_listview, &listview_button_img);
    internal->imgbutton_listview->push_state_change_callback = 
                                            imgbutton_listview_handler;

    // the "Compact View" button (custom bitmap)
    internal->imgbutton_compactview = push_imgbutton_new(dialog->window->gc, 
                                               dialog->window,
                                               dialog->window->w - 38, 4,
                                               28, 28);
    imgbutton_set_image(internal->imgbutton_compactview, &compactview_button_img);
    internal->imgbutton_compactview->push_state_change_callback = 
                                        imgbutton_compactview_handler;
    imgbutton_set_push_state(internal->imgbutton_compactview, 1);

    // labels
    label_new(dialog->window->gc, dialog->window, 94, 9, 70, 20, "Location:");

    y = dialog->window->h - (BUTTON_HEIGHT + 10);

    // the "Cancel" button
    button = button_new(dialog->window->gc, dialog->window,
                        dialog->window->w - BUTTON_WIDTH - 10, y,
                        BUTTON_WIDTH, BUTTON_HEIGHT,
                        "Cancel");
    button->button_click_callback = dialog_button_handler;
    button->internal_data = (void *)(intptr_t)1;

    label_new(dialog->window->gc, dialog->window,
              4, y + 10,
              90, 20, "Files of type:");

    internal->filter_combobox = combobox_new(dialog->window->gc, 
                                             dialog->window,
                                             100, y + 5, 220, NULL);
    internal->filter_combobox->entry_click_callback =
                    combobox_entry_click_callback;

    y = dialog->window->h - ((BUTTON_HEIGHT * 2) + 15);

    // the "Open" or "Save" button
    button = button_new(dialog->window->gc, dialog->window,
                        dialog->window->w - BUTTON_WIDTH - 10, y,
                        BUTTON_WIDTH, BUTTON_HEIGHT,
                        type_to_title(dialog->type));
    button->button_click_callback = dialog_button_handler;
    button->internal_data = (void *)(intptr_t)0;

    // the "File name:" label and input box
    label_new(dialog->window->gc, dialog->window,
              4, y + 10,
              90, 20, "File name:");

    internal->filename_inputbox = inputbox_new(dialog->window->gc, dialog->window,
                                     100, y + 5, 220, NULL);
    internal->filename_inputbox->window.keypress = filename_inputbox_keypress;

    // the location bar at the top
    internal->location_bar = inputbox_new(dialog->window->gc, dialog->window,
                                          158, 4,
                                          dialog->window->w - 158 - 100, NULL);
    internal->location_bar->window.keypress = locationbar_keypress;

    if(dialog->multiselect && dialog->type != DIALOGBOX_OPEN)
    {
        // multiselect only works for Open dialog boxes
        dialog->multiselect = 0;
    }

    // the file selector (main body of the dialog)
    // it is added last so it has focus
    internal->selector = file_selector_new(dialog->window->gc, dialog->window,
                                           0, LOCATION_BAR_HEIGHT,
                                           dialog->window->w,
                                           y - LOCATION_BAR_HEIGHT - 10,
                                           NULL);

    //dialog->selector = selector;
    internal->selector->entry_click_callback = fileentry_click_callback;
    internal->selector->selection_change_callback = fileentry_selection_change_callback;
    internal->selector->entry_doubleclick_callback = fileentry_doubleclick_callback;
    file_selector_set_viewmode(internal->selector, FILE_SELECTOR_COMPACT_VIEW);

    if(dialog->multiselect)
    {
        internal->selector->flags |= FILE_SELECTOR_FLAG_MULTISELECT;
    }
    else
    {
        internal->selector->flags &= ~FILE_SELECTOR_FLAG_MULTISELECT;
    }

    // set current path
    if(dialog->path)
    {
        home = dialog->path;
    }

    if(!home || !*home)
    {
        home = "/";
    }

    // get the optional filters
    if(internal->filter_list)
    {
        free_filter_list(internal->filter_count, internal->filter_list);
        internal->filter_list = NULL;
        internal->filter_count = 0;
    }

    internal->filter_count = get_filters(dialog, &internal->filter_list);
    add_filters_to_combobox(internal->filter_combobox,
                            internal->filter_count, internal->filter_list);
    add_filters_to_selector(internal->selector, internal->filter_list[1]);
    
    internal->curdir = strdup(home);
    inputbox_set_text((struct window_t *)internal->location_bar, internal->curdir);
    file_selector_set_path(internal->selector, home);
    history_push(home);

    // if current directory is not root, enable the Go -> Parent menu item
    if(internal->curdir[0] == '/' && internal->curdir[1] == '\0')
    {
        imgbutton_disable(internal->imgbutton_up);
    }
    else
    {
        imgbutton_enable(internal->imgbutton_up);
    }

    // now paint and show the dialog box
    window_repaint(dialog->window);

    simple_request(REQUEST_DIALOG_SHOW, GLOB.server_winid, 
                                        dialog->window->winid);
    dialog->window->flags &= ~WINDOW_HIDDEN;

    while(1)
    {
        struct window_t *combobox_window = 
                    internal->filter_combobox->internal_frame;
        struct event_t *ev = NULL;
        
        if(!pending_events_timeout(1))
        {
            continue;
        }

        // get events for the main dialog window
        if((ev = next_event_for_seqid(dialog->window, 0, 0)))
        {
            messagebox_dispatch_event(ev);
            free(ev);
        }

        // get events for the combobox's list frame in case it is shown
        if((ev = next_event_for_seqid(combobox_window, 0, 0)))
        {
            combobox_window->event_handler(ev);
            free(ev);
        }
        
        if(internal->status.close_dialog)
        {
            i = internal->status.selected_button;

            // make sure we have a reasonable filename(s), unless the user
            // cancelled
            if(i == 0 &&
               !is_valid_filename(internal->filename_inputbox->window.title))
            {
                internal->status.close_dialog = 0;
                continue;
            }

            break;
        }
    }

    simple_request(REQUEST_DIALOG_HIDE, GLOB.server_winid, dialog->window->winid);
    dialog->window->flags |= WINDOW_HIDDEN;

    dialog_shown = 0;

    return (i == 0) ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
}


static void __dialog_destroy(struct opensave_dialog_t *dialog)
{
    int i;

    if(!dialog || !dialog->window)
    {
        return;
    }
    
    window_destroy_children(dialog->window);
    window_destroy(dialog->window);

    if(dialog->internal.curdir)
    {
        free(dialog->internal.curdir);
        dialog->internal.curdir = NULL;
    }

    if(dialog->internal.filter_list)
    {
        free_filter_list(dialog->internal.filter_count,
                         dialog->internal.filter_list);
        dialog->internal.filter_list = NULL;
        dialog->internal.filter_count = 0;
    }

    free(dialog);

    for(i = 0; i < HISTORY_COUNT; i++)
    {
        if(history[i])
        {
            free(history[i]);
            history[i] = NULL;
        }
    }
}


/*
 * Get the list of selected items. If res is NULL, the count is returned
 * (useful in querying the number of items without getting the actual items).
 *
 * Returns:
 *    0 if no items are selected
 *    -1 if an error occurred
 *    otherwise the count of selected items
 *
 * The returned list should be freed by calling __dialog_free_list().
 */
int __dialog_get_selected(struct opensave_dialog_t *dialog, 
                          struct opensave_file_t **res)
{
    struct opensave_file_t *newentries;
    int i, count = 0, quote;
    char *dir = dialog->internal.curdir;
    size_t dirlen = dir ? strlen(dir) : 0;
    char *p, *str = dialog->internal.filename_inputbox->window.title;
    char *q1, *q2;
    int multi = !!strchr(str, '"');

    if(!dirlen)
    {
        return -1;
    }

    // get the count
    if(multi)
    {
        for(quote = 0, p = str; *p; p++)
        {
            if(*p == '"')
            {
                quote = !quote;

                if(!quote)
                {
                    count++;
                }
            }
        }

        // unmatched quote
        if(quote)
        {
            return -1;
        }
    }
    else
    {
        count = 1;
    }

    // only want the count
    if(res == NULL)
    {
        return count;
    }
    
    if(!(newentries = malloc(count * sizeof(struct opensave_file_t))))
    {
        return -1;
    }

    A_memset(newentries, 0, count * sizeof(struct opensave_file_t));

    if(count == 1)
    {
        if((newentries[0].path = malloc(dirlen + strlen(str) + 2)))
        {
            sprintf(newentries[0].path, "%s/%s", dir, str);
        }
    }
    else
    {
        p = str;
        i = 0;

        while(*p)
        {
            size_t qlen;

            q1 = strchr(p, '"');

            // no more quotes, we are done
            if(!q1)
            {
                break;
            }

            q2 = strchr(q1 + 1, '"');

            // unmatched quote, bail out
            if(!q2)
            {
                free(newentries);
                return -1;
            }

            qlen = q2 - q1;

            if((newentries[i].path = malloc(dirlen + qlen + 2)))
            {
                strcpy(newentries[i].path, dir);
                newentries[i].path[dirlen] = '/';
                strncpy(newentries[i].path + dirlen + 1, q1 + 1, qlen - 1);
                newentries[i].path[dirlen + qlen] = '\0';
            }

            p = q2 + 1;
            i++;
        }
    }
    
    *res = newentries;
    return count;
}


void __dialog_free_list(struct opensave_file_t *entries, int entry_count)
{
    int i;
    
    for(i = 0; i < entry_count; i++)
    {
        if(entries[i].path)
        {
            free(entries[i].path);
        }
    }
    
    free(entries);
}


/********************************
 * Global functions
 ********************************/

struct opensave_dialog_t *open_dialog_create(winid_t owner)
{
    return __dialog_create(owner, DIALOGBOX_OPEN);
}

struct opensave_dialog_t *save_dialog_create(winid_t owner)
{
    return __dialog_create(owner, DIALOGBOX_SAVE);
}

struct opensave_dialog_t *saveas_dialog_create(winid_t owner)
{
    return __dialog_create(owner, DIALOGBOX_SAVEAS);
}

int open_dialog_show(struct opensave_dialog_t *dialog)
{
    return __dialog_show(dialog);
}

int save_dialog_show(struct opensave_dialog_t *dialog)
{
    return __dialog_show(dialog);
}

int saveas_dialog_show(struct opensave_dialog_t *dialog)
{
    return __dialog_show(dialog);
}

void open_dialog_destroy(struct opensave_dialog_t *dialog)
{
    __dialog_destroy(dialog);
}

void save_dialog_destroy(struct opensave_dialog_t *dialog)
{
    __dialog_destroy(dialog);
}

void saveas_dialog_destroy(struct opensave_dialog_t *dialog)
{
    __dialog_destroy(dialog);
}

int open_dialog_get_selected(struct opensave_dialog_t *dialog, 
                             struct opensave_file_t **res)
{
    return __dialog_get_selected(dialog, res);
}

int save_dialog_get_selected(struct opensave_dialog_t *dialog, 
                             struct opensave_file_t **res)
{
    return __dialog_get_selected(dialog, res);
}

int saveas_dialog_get_selected(struct opensave_dialog_t *dialog, 
                               struct opensave_file_t **res)
{
    return __dialog_get_selected(dialog, res);
}

void open_dialog_free_list(struct opensave_file_t *entries, int entry_count)
{
    __dialog_free_list(entries, entry_count);
}

void save_dialog_free_list(struct opensave_file_t *entries, int entry_count)
{
    __dialog_free_list(entries, entry_count);
}

void saveas_dialog_free_list(struct opensave_file_t *entries, int entry_count)
{
    __dialog_free_list(entries, entry_count);
}

