/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: desktop.h
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
 *  \file desktop.h
 *
 *  Header file for the desktop application.
 */

#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H

#include <stdint.h>
#include "../include/window-defs.h"

// we need to store windows' IDs and flags and a few other params
struct winent_t
{
    winid_t winid;
    uint32_t flags;
    char *title;
    struct bitmap32_t *icon;
    struct winent_t *next;
};

// desktop.c
extern struct winent_t *winentries;

void redraw_desktop_background(int x, int y, int w, int h);

// desktop_alt_tab.c
extern struct window_t *alttab_win;
void desktop_init_alttab(void);
void desktop_prep_alttab(void);
void desktop_cancel_alttab(void);
void desktop_draw_alttab(void);
void desktop_finish_alttab(void);

#endif      /* GUI_DESKTOP_H */
