/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: messagebox.c
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
 *  \file messagebox.c
 *
 *  The implementation of general dialog boxes and input dialogs.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "../include/event.h"
#include "../include/keys.h"
#include "../include/client/button.h"
#include "../include/client/label.h"
#include "../include/client/dialog.h"
#include "../include/client/inputbox.h"

#include "inlines.c"


#define GLOB                        __global_gui_data

#define MAX_BUTTONS                 8
#define DIALOG_PADDING              16
#define BUTTON_PADDING              16
#define MIN_DIALOG_WIDTH            200
#define MIN_DIALOG_HEIGHT           60
#define MIN_BUTTON_WIDTH            60
#define MIN_BUTTON_HEIGHT           30


struct bounds_t
{
    int x, y;
    int w, h;
};


struct
{
    char *captions[4];
    int types[4];
    int count;
} standard_buttons[] =
{
    { { NULL, }, { 0, }, 0, },
    { {"Yes", "No", }, { DIALOG_BUTTON_DEFAULT, DIALOG_BUTTON_CANCEL, }, 2 },
    { {"Yes", "No", "Cancel", }, 
            { DIALOG_BUTTON_DEFAULT, 0, DIALOG_BUTTON_CANCEL, }, 3 },
    { {"Ok", }, { DIALOG_BUTTON_DEFAULT, }, 1 },
    { {"Ok", "Cancel", }, 
            { DIALOG_BUTTON_DEFAULT, DIALOG_BUTTON_CANCEL, }, 2 },
    { {"Retry", "Cancel", }, 
            { DIALOG_BUTTON_DEFAULT, DIALOG_BUTTON_CANCEL, }, 2 },
    { {"Accept", "Decline", }, 
            { DIALOG_BUTTON_DEFAULT, DIALOG_BUTTON_CANCEL, }, 2 },
};


void messagebox_dispatch_event(struct event_t *ev)
{
    struct window_t *window;
    
    if(!(window = win_for_winid(ev->dest)))
    {
        return;
    }
    
    switch(ev->type)
    {
        case EVENT_WINDOW_POS_CHANGED:
            window->x = ev->win.x;
            window->y = ev->win.y;
            return;
        
        case EVENT_WINDOW_RESIZE_OFFER:
            window_resize(window, ev->win.x, ev->win.y,
                                  ev->win.w, ev->win.h);
            return;
        
        case EVENT_MOUSE:
            window_mouseover(window, ev->mouse.x, ev->mouse.y, 
                                     ev->mouse.buttons);
            return;
        
        case EVENT_MOUSE_EXIT:
            window_mouseexit(window, ev->mouse.buttons);
            return;

        case EVENT_WINDOW_CLOSING:
        {
            /*
             * TODO:
             */
            struct dialog_status_t *status = window->internal_data;

            status->selected_button = -1;
            status->close_dialog = 1;
            return;
        }

        case EVENT_KEY_PRESS:
            // handle TABs
            if(ev->key.code == KEYCODE_TAB && ev->key.modifiers == 0)
            {
                widget_next_tabstop(window);
                return;
            }

            // handle ESC
            if(ev->key.code == KEYCODE_ESC && ev->key.modifiers == 0)
            {
                struct dialog_status_t *status = window->internal_data;

                status->selected_button = -1;
                status->close_dialog = 1;
                return;
            }

            // see if a child widget wants to handle this key event before
            // processing global key events, e.g. menu accelerator keys
            if(window->active_child && window->active_child->keypress)
            {
                if(window->active_child->keypress(window->active_child,
                                                  ev->key.code, 
                                                  ev->key.modifiers))
                {
                    // child widget processed the event, we are done here
                    return;
                }
            }

            return;
    }
}


void dialog_button_handler(struct button_t *button, int x, int y)
{
    struct window_t *dialog_window = (struct window_t *)button->window.parent;
    struct dialog_status_t *status = dialog_window->internal_data;
    
    status->selected_button = (int)(intptr_t)button->internal_data;
    status->close_dialog = 1;

    // if we are not running on the same thread as the dialog box, send a
    // SIGCONT signal to the dialog box's thread
    if(status->dialog_thread && !pthread_equal(pthread_self(), status->dialog_thread))
    {
        pthread_kill(status->dialog_thread, SIGCONT);
    }
}


struct dialog_button_t *init_buttons(struct dialog_button_t *inbuttons, 
                                     int *button_count)
{
    struct dialog_button_t *outbuttons;
    int i, j;
    
    if(inbuttons == DIALOG_YES_NO        ||
       inbuttons == DIALOG_YES_NO_CANCEL ||
       inbuttons == DIALOG_OK            ||
       inbuttons == DIALOG_OK_CANCEL     ||
       inbuttons == DIALOG_RETRY_CANCEL  ||
       inbuttons == DIALOG_ACCEPT_DECLINE)
    {
        j = (int)(intptr_t)inbuttons;
        
        if(!(outbuttons = malloc(standard_buttons[j].count * 
                                    sizeof(struct dialog_button_t))))
        {
            return NULL;
        }
        
        for(i = 0; i < standard_buttons[j].count; i++)
        {
            /*
            if(!(outbuttons[i].caption = strdup(standard_buttons[j].captions[i])))
            {
                free(outbuttons);
                return NULL;
            }
            */
            outbuttons[i].caption = standard_buttons[j].captions[i];
            
            outbuttons[i].type = standard_buttons[j].types[i];
        }
        
        *button_count = standard_buttons[j].count;
        return outbuttons;
    }
    
    return inbuttons;
}


int dialog_res_button(struct dialog_button_t *inbuttons, 
                      int button_count, int button)
{
    if(inbuttons == DIALOG_YES_NO)
    {
        switch(button)
        {
            case 0: return DIALOG_RESULT_YES;
            case -1:
            case 1: return DIALOG_RESULT_NO;
        }
    }
    else if(inbuttons == DIALOG_YES_NO_CANCEL)
    {
        switch(button)
        {
            case 0: return DIALOG_RESULT_YES;
            case 1: return DIALOG_RESULT_NO;
            case -1:
            case 2: return DIALOG_RESULT_CANCEL;
        }
    }
    else if(inbuttons == DIALOG_OK)
    {
        switch(button)
        {
            case -1:
            case 0: return DIALOG_RESULT_OK;
        }
    }
    else if(inbuttons == DIALOG_OK_CANCEL)
    {
        switch(button)
        {
            case 0: return DIALOG_RESULT_OK;
            case -1:
            case 1: return DIALOG_RESULT_CANCEL;
        }
    }
    else if(inbuttons == DIALOG_RETRY_CANCEL)
    {
        switch(button)
        {
            case 0: return DIALOG_RESULT_RETRY;
            case -1:
            case 1: return DIALOG_RESULT_CANCEL;
        }
    }
    else if(inbuttons == DIALOG_ACCEPT_DECLINE)
    {
        switch(button)
        {
            case 0: return DIALOG_RESULT_ACCEPT;
            case -1:
            case 1: return DIALOG_RESULT_DECLINE;
        }
    }
    
    // if the user pressed ESC, find the button that is assigned as the
    // cancel button and return it
    if(button == -1)
    {
        int i;
        
        for(i = 0; i < button_count; i++)
        {
            if(inbuttons[i].type == DIALOG_BUTTON_CANCEL)
            {
                button = i;
                break;
            }
        }
    }
    
    return button;
}


int messagebox_prepare(winid_t owner, char *title, char *message,
                    struct dialog_button_t *buttons, int button_count,
                    int add_inputbox,
                    struct dialog_status_t *status, 
                    struct window_t **dialog_window)
{
    struct dialog_button_t *dialog_buttons;
    int dialog_button_count = button_count;
    int lines = 1;
    int longest_line_chars = 0;
    int line_chars = 0;
    char *s;
    int i, j;

    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    int charh = char_height(font, ' ');
    
    // ensure we have a message
    if(!message || !*message)
    {
        __set_errno(EINVAL);
        return -1;
    }
    
    // get button captions and count
    if(!(dialog_buttons = init_buttons(buttons, &dialog_button_count)))
    {
        __set_errno(ENOMEM);
        return -1;
    }
    
    // ensure we have a valid button count
    if(dialog_button_count < 1 || dialog_button_count > MAX_BUTTONS)
    {
        __set_errno(EINVAL);
        return -1;
    }
    
    // get message line count and the longest line length
    for(s = message; *s; s++)
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

    // check the last (or the only) line length
    if(line_chars > longest_line_chars)
    {
        longest_line_chars = line_chars;
    }
    
    // prepare to calculate button and text dimensions
    struct bounds_t bounds[dialog_button_count + 1];
    
    // get total buttons width
    for(i = 0, j = 0; i < dialog_button_count; i++)
    {
        // add 2 for left and right padding inside the button
        size_t sz = (strlen(dialog_buttons[i].caption) + 2) * GLOB.mono.charw;
        
        if(sz < MIN_BUTTON_WIDTH)
        {
            bounds[i].w = MIN_BUTTON_WIDTH;
        }
        else
        {
            bounds[i].w = sz;
        }
        
        j += bounds[i].w + BUTTON_PADDING;
    }
    
    // calculate initial message dimensions
    bounds[dialog_button_count].y = DIALOG_PADDING;
    bounds[dialog_button_count].h = (lines * charh);
    bounds[dialog_button_count].x = DIALOG_PADDING;
    bounds[dialog_button_count].w = (longest_line_chars * GLOB.mono.charw);
    
    if(bounds[dialog_button_count].h < MIN_DIALOG_HEIGHT)
    {
        bounds[dialog_button_count].h = MIN_DIALOG_HEIGHT;
    }

    if(bounds[dialog_button_count].w < MIN_DIALOG_WIDTH)
    {
        bounds[dialog_button_count].w = MIN_DIALOG_WIDTH;
    }

    if(bounds[dialog_button_count].w < j)
    {
        bounds[dialog_button_count].w = j;
    }
    
    j = (DIALOG_PADDING * 2) + bounds[dialog_button_count].h;
    
    if(add_inputbox)
    {
        j += INPUTBOX_HEIGHT;
    }
    
    // now finalize button dimensions
    for(i = dialog_button_count - 1; i >= 0; i--)
    {
        bounds[i].y = j;
        bounds[i].h = MIN_BUTTON_HEIGHT;
        
        if(i == dialog_button_count - 1)
        {
            bounds[i].x = DIALOG_PADDING + 
                            bounds[dialog_button_count].w - bounds[i].w;
        }
        else
        {
            bounds[i].x = bounds[i + 1].x - BUTTON_PADDING - bounds[i].w;
        }
    }
    
    
    struct window_attribs_t attribs;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = (DIALOG_PADDING * 2) + bounds[dialog_button_count].w;
    attribs.h = (DIALOG_PADDING * 3) + bounds[dialog_button_count].h + 
                                                        MIN_BUTTON_HEIGHT;
    attribs.flags = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | WINDOW_SKIPTASKBAR;

    if(add_inputbox)
    {
        attribs.h += INPUTBOX_HEIGHT;
    }
    
    if(!(*dialog_window = __window_create(&attribs, WINDOW_TYPE_DIALOG, 
                                                    owner)))
    {
        if(dialog_buttons != buttons)
        {
            free(dialog_buttons);
        }
        
        return -1;
    }
    
    (*dialog_window)->event_handler = messagebox_dispatch_event;
    status->selected_button = -1;
    status->close_dialog = 0;
    status->dialog_thread = pthread_self();
    (*dialog_window)->internal_data = status;
    //dialog_window->internal_data = (void *)-1;
    window_set_title(*dialog_window, title);
    
    for(i = 0; i < dialog_button_count; i++)
    {
        struct button_t *button = button_new((*dialog_window)->gc,
                                             *dialog_window,
                                             bounds[i].x, bounds[i].y,
                                             bounds[i].w, bounds[i].h,
                                             dialog_buttons[i].caption);
        button->button_click_callback = dialog_button_handler;
        button->internal_data = (void *)(intptr_t)i;
    }

    label_new((*dialog_window)->gc, *dialog_window, bounds[i].x, bounds[i].y,
              bounds[i].w, bounds[i].h, message);

    if(add_inputbox)
    {
        inputbox_new((*dialog_window)->gc, *dialog_window,
                     bounds[i].x, bounds[i].y + bounds[i].h,
                     bounds[i].w, NULL);
    }
    
    window_repaint(*dialog_window);

    //window_show(dialog_window);
    simple_request(REQUEST_DIALOG_SHOW, GLOB.server_winid, 
                                        (*dialog_window)->winid);
    (*dialog_window)->flags &= ~WINDOW_HIDDEN;

    return 0;
}


int messagebox_show(winid_t owner, char *title, char *message,
                    struct dialog_button_t *buttons, int button_count)
{
    struct window_t *dialog_window;
    struct dialog_status_t status;
    int i;
    
    if((i = messagebox_prepare(owner, title, message,
                               buttons, button_count, 0,
                               &status, &dialog_window)) != 0)
    {
        return i;
    }

    i = 0;
    
    while(1)
    {
        struct event_t *ev = NULL;

        if((ev = next_event_for_seqid(NULL /* dialog_window */, 0, 1)))
        {
            //messagebox_dispatch_event(ev);
            event_dispatch(ev);
            free(ev);
        }

        if(status.close_dialog)
        {
            i = status.selected_button;
            break;
        }
    }
    
    i = dialog_res_button(buttons, button_count, i);
    
    /*
    simple_request(REQUEST_DIALOG_HIDE, GLOB.server_winid, dialog_window->winid);
    dialog_window->flags |= WINDOW_HIDDEN;
    */

    window_destroy_children(dialog_window);
    window_destroy(dialog_window);
    
    return i;
}


char *inputbox_show(winid_t owner, char *title, char *message)
{
    struct window_t *dialog_window;
    struct dialog_status_t status;
    int i;
    char *res;
    
    if((i = messagebox_prepare(owner, title, message,
                               DIALOG_OK_CANCEL, 2, 1,
                               &status, &dialog_window)) != 0)
    {
        return NULL;
    }

    i = 0;
    
    while(1)
    {
        struct event_t *ev = NULL;

        if((ev = next_event_for_seqid(NULL /* dialog_window */, 0, 1)))
        {
            //messagebox_dispatch_event(ev);
            event_dispatch(ev);
            free(ev);
        }
        
        if(status.close_dialog)
        {
            i = status.selected_button;
            break;
        }
    }
    
    i = dialog_res_button(DIALOG_OK_CANCEL, 2, i);
    res = NULL;
    
    if(i == DIALOG_RESULT_OK)
    {
        struct window_t *current_child;
        ListNode *current_node;

        for(current_node = dialog_window->children->root_node;
            current_node != NULL;
            current_node = current_node->next)
        {
            current_child = (struct window_t *)current_node->payload;

            if(current_child->type == WINDOW_TYPE_INPUTBOX)
            {
                res = current_child->title ? strdup(current_child->title) : 
                                             NULL;
                break;
            }
        }
    }
    
    /*
    simple_request(REQUEST_DIALOG_HIDE, GLOB.server_winid, dialog_window->winid);
    dialog_window->flags |= WINDOW_HIDDEN;
    */

    window_destroy_children(dialog_window);
    window_destroy(dialog_window);
    
    return res;
}

