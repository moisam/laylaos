/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gui.h
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
 *  \file gui.h
 *
 *  General declarations and macros not fitting anywhere else.
 */

#ifndef GUI_H
#define GUI_H

#include <sys/types.h>
#include "client/window.h"
#include "screen.h"

#include "gui-global.h"

#ifdef __cplusplus
extern "C" {
#endif

void gui_init(int argc, char **argv);
void gui_init_no_fonts(int argc, char **argv);
void gui_exit(int exit_code);


/*
 * Default GUI file paths.
 */

#define MENU_ICONS_FILE_PATH            "/usr/share/gui/menu_icons_16.png"

#define DEFAULT_DESKTOP_PATH            "/usr/share/gui/desktop"
#define DEFAULT_ICON_PATH               "/usr/share/gui/icons"
#define DEFAULT_EXE_ICON_PATH           DEFAULT_ICON_PATH "/executable.ico"
#define DEFAULT_APP_CATEGORIES_PATH     DEFAULT_DESKTOP_PATH "/categories"
#define DEFAULT_FONT_PATH               "/usr/share/fonts"


/*
 * Struct to represent an application entry. This is used by the desktop to
 * keep information about what applications to show on the desktop, as well as
 * by the top-panel Applications widget.
 */
struct app_entry_t
{
    int x, y, w, h;
    int basex, basey;
    int click_count;
    time_t click_ticks;
    int mouse_bdx, mouse_bdy;
    int mouse_dx, mouse_dy;

    char *name;
    char *command;
    char *iconpath;
    char *icon;

    // desktop breaks down name into lines for quick access
    size_t name_line_start[2];  // start index of the two lines
    size_t name_line_end[2];    // end index of the two lines
    size_t name_line_pixels[2]; // width in pixels

    int category;

#define APPLICATION_FLAG_SHOW_ON_DESKTOP    0x01
    int flags;

    struct bitmap32_t icon_bitmap;
    struct app_entry_t *next, *prev;
};


/*
 * Helper function to get time in milliseconds.
 */
#include <time.h>

static inline uint64_t time_in_millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ms = ((uint64_t)ts.tv_sec * 1000) + 
                  ((uint64_t)ts.tv_nsec / 1000000);
    return ms;
}

#ifdef __cplusplus
}
#endif

#endif      /* GUI_H */
