/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: help.c
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
 *  \file help.c
 *
 *  Functions to show help and keyboard shortcuts dialog boxes.
 */

#include "../include/gui.h"
#include "../include/client/dialog.h"

#define GLOB                        __global_gui_data

extern struct window_t *main_window;

static char *shortcuts[] =
{
    "Ctrl + F1",
    "Ctrl + N",
    "Ctrl + Q",
    NULL,
};

static char *descriptions[] =
{
    "Show shortcuts",
    "New game",
    "Quit",
    NULL,
};

char *app_name = "Minesweeper";
char *app_ver = "1.0.0";
char *app_about = "The classic game of minesweeper";
char *app_copyright = "Copyright (c) 2024 Mohammed Isam";


void show_shortcuts_dialog(void)
{
    struct shortcuts_dialog_t *dialog;
    
    if(!(dialog = shortcuts_dialog_create(main_window->winid,
                                          shortcuts, descriptions)))
    {
        return;
    }
    
    shortcuts_dialog_set_title(dialog, "Keyboard shortcuts");
    shortcuts_dialog_show(dialog);
    shortcuts_dialog_destroy(dialog);
}


void credits_callback(struct button_t *button, int x, int y)
{
}


void license_callback(struct button_t *button, int x, int y)
{
}


void help_callback(struct button_t *button, int x, int y)
{
}


void show_about_dialog(void)
{
    struct about_dialog_t *dialog;
    
    if(!(dialog = aboutbox_create(main_window->winid)))
    {
        return;
    }
    
    aboutbox_set_name(dialog, app_name);
    aboutbox_set_version(dialog, app_ver);
    aboutbox_set_about(dialog, app_about);
    aboutbox_set_copyright(dialog, app_copyright);
    
    aboutbox_credits_callback(dialog, credits_callback);
    aboutbox_license_callback(dialog, license_callback);
    aboutbox_help_callback(dialog, help_callback);
    
    aboutbox_show(dialog);
    aboutbox_destroy(dialog);
}

