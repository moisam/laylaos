/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: options.c
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
 *  \file options.c
 *
 *  Functions to show the game options dialog.
 */

#include "../include/event.h"
#include "../include/resources.h"
#include "../include/menu.h"
#include "../include/client/button.h"
#include "../include/client/label.h"
#include "../include/client/dialog.h"
#include "../include/client/spinner.h"
#include "../include/client/toggle.h"

#include "../client/inlines.c"

#include "defs.h"

#define GLOB                        __global_gui_data

#define __STR(s)                    #s
#define STR(s)                      __STR(s)


// defined in main.c
extern int cols;
extern int rows;
extern int mines;
extern int count_time;
extern struct window_t *main_window;

int new_rows, new_cols, new_mines;
int neww, newh;

extern void messagebox_dispatch_event(struct event_t *ev);


static void dialog_button_handler(struct button_t *button, int x, int y)
{
    struct window_t *dialog_window = (struct window_t *)button->window.parent;
    struct dialog_status_t *status = dialog_window->internal_data;
    
    status->close_dialog = 1;
}


static void rows_change_callback(struct window_t *window,
                                 struct spinner_t *spinner)
{
    new_rows = spinner->val;
}


static void cols_change_callback(struct window_t *window,
                                 struct spinner_t *spinner)
{
    new_cols = spinner->val;
}


static void mines_change_callback(struct window_t *window,
                                 struct spinner_t *spinner)
{
    new_mines = spinner->val;
}


static void toggle_change_callback(struct window_t *window,
                                   struct toggle_t *toggle)
{
    count_time = toggle->toggled;
}


void show_options_dialog(void)
{
    struct window_attribs_t attribs;
    struct window_t *dialog_window;
    struct button_t *button;
    struct label_t *label;
    struct spinner_t *spinner;
    struct toggle_t *toggle;
    struct dialog_status_t status;
    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    int charh = char_height(font, ' ');

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 220;
    attribs.h = 170;
    attribs.flags = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | WINDOW_SKIPTASKBAR;

    if(!(dialog_window = __window_create(&attribs, WINDOW_TYPE_DIALOG, 
                                                    main_window->winid)))
    {
        return;
    }

    dialog_window->event_handler = messagebox_dispatch_event;
    status.close_dialog = 0;
    dialog_window->internal_data = &status;
    window_set_title(dialog_window, "Options");

    // create the labels
    label = label_new(dialog_window->gc, dialog_window, 10, 14,
                      100, charh, "Rows (" STR(MIN_ROWS) "-"
                                  STR(MAX_ROWS) "):");
    label_set_text_alignment(label, TEXT_ALIGN_LEFT);

    label = label_new(dialog_window->gc, dialog_window, 10, 44,
                      120, charh, "Columns (" STR(MIN_COLS) "-"
                                  STR(MAX_COLS) "):");
    label_set_text_alignment(label, TEXT_ALIGN_LEFT);

    label = label_new(dialog_window->gc, dialog_window, 10, 74,
                      120, charh, "Mines (" STR(MIN_MINES) "-"
                                  STR(MAX_MINES) "):");
    label_set_text_alignment(label, TEXT_ALIGN_LEFT);

    label = label_new(dialog_window->gc, dialog_window, 10, 104,
                      100, charh, "Count time?");
    label_set_text_alignment(label, TEXT_ALIGN_LEFT);

    // create the spinners
    spinner = spinner_new(dialog_window->gc, dialog_window,
                          160, 10, 50);
    spinner_set_max(spinner, MAX_ROWS);
    spinner_set_min(spinner, MIN_ROWS);
    spinner_set_val(spinner, rows);
    spinner->value_change_callback = rows_change_callback;

    spinner = spinner_new(dialog_window->gc, dialog_window,
                          160, 40, 50);
    spinner_set_max(spinner, MAX_COLS);
    spinner_set_min(spinner, MIN_COLS);
    spinner_set_val(spinner, cols);
    spinner->value_change_callback = cols_change_callback;

    spinner = spinner_new(dialog_window->gc, dialog_window,
                          160, 70, 50);
    spinner_set_max(spinner, MAX_MINES);
    spinner_set_min(spinner, MIN_MINES);
    spinner_set_val(spinner, mines);
    spinner->value_change_callback = mines_change_callback;

    // create the toggle
    toggle = toggle_new(dialog_window->gc, dialog_window,
                        160, 100);
    toggle_set_toggled(toggle, !!count_time);
    toggle->toggle_change_callback = toggle_change_callback;

    // create the button
    button = button_new(dialog_window->gc, dialog_window,
                        dialog_window->w - 75, 
                        dialog_window->h - 35, 68, 30, "Close");
    button->button_click_callback = dialog_button_handler;

    // now paint and show the dialog box
    window_repaint(dialog_window);
    simple_request(REQUEST_DIALOG_SHOW, GLOB.server_winid, 
                                        dialog_window->winid);
    dialog_window->flags &= ~WINDOW_HIDDEN;

    new_rows = rows;
    new_cols = cols;
    new_mines = mines;

    while(1)
    {
        struct event_t *ev = NULL;
        
        if((ev = next_event_for_seqid(dialog_window, 0, 1)))
        {
            messagebox_dispatch_event(ev);
            free(ev);
        }
        
        if(status.close_dialog)
        {
            break;
        }
    }

    window_destroy_children(dialog_window);
    window_destroy(dialog_window);

    // validate and apply new values
    if(new_rows < MIN_ROWS)
    {
        new_rows = MIN_ROWS;
    }
    else if(new_rows > MAX_ROWS)
    {
        new_rows = MAX_ROWS;
    }

    if(new_cols < MIN_COLS)
    {
        new_cols = MIN_COLS;
    }
    else if(new_cols > MAX_COLS)
    {
        new_cols = MAX_COLS;
    }

    if(new_mines < MIN_MINES)
    {
        new_mines = MIN_MINES;
    }
    else if(new_mines > MAX_MINES)
    {
        new_mines = MAX_MINES;
    }

    rows = new_rows;
    cols = new_cols;
    mines = new_mines;

    // don't let mines cover more than half of the board
    if(mines > ((rows * cols) / 2))
    {
        mines = (rows * cols) / 2;
    }

    neww = (CELL_SIZE * cols) + (LEFT_BORDER * 2);
    newh = (CELL_SIZE * rows) + TOP_BORDER + BOTTOM_BORDER;

    if(neww != main_window->w || newh != main_window->h)
    {
        window_set_size(main_window, main_window->x, main_window->y,
                                     neww, newh);
    }
}

