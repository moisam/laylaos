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
#include "../include/rect.h"

// private desktop requests
#define REQUEST_GET_DESKTOP_BACKGROUND      (REQUEST_APPLICATION_PRIVATE + 0)
#define REQUEST_SET_DESKTOP_BACKGROUND      (REQUEST_APPLICATION_PRIVATE + 1)

// private desktop events
#define EVENT_DESKTOP_BACKGROUND_INFO       (EVENT_APPLICATION_PRIVATE + 0)

// background image aspect values
#define DESKTOP_BACKGROUND_CENTERED         0
#define DESKTOP_BACKGROUND_TILES            1
#define DESKTOP_BACKGROUND_SCALED           2
#define DESKTOP_BACKGROUND_STRETCHED        3
#define DESKTOP_BACKGROUND_ZOOMED           4

#define DESKTOP_BACKGROUND_FIRST_ASPECT     0
#define DESKTOP_BACKGROUND_LAST_ASPECT      4


// we need to store windows' IDs and flags and a few other params
struct winent_t
{
    winid_t winid;
    uint32_t flags;
    char *title;
    struct bitmap32_t *icon;
    struct winent_t *next;
};


/*
 * Event struct used to send/request desktop background info.
 */
struct event_desktop_bg_t
{
    // the first 4 fields should be the same in all event struct types
    uint32_t type;
    uint32_t seqid;

    // window id of event's source and destination
    winid_t src, dest;

    // non-zero if this is a valid server reply, 0 if server error happened
    int valid_reply;

    // non-zero if the reply contains an image path
    int bg_is_image;

    // if image path is given, specify how to display the image
    int bg_image_aspect;

    // When requesting a resource to be loaded, data contains the resource's
    // pathname. When returning a loaded resource to the client, data contains
    // the actual data (image pixels, font data, etc).
    size_t datasz;
    char data[];
};


// desktop.c
extern struct winent_t *winentries;
extern struct window_t *desktop_window;
extern Rect desktop_bounds;

// desktop_background.c
extern int background_is_image;
extern uint32_t background_color;
extern char *background_image_path;
extern int background_image_aspect;

void load_desktop_background(void);
void draw_desktop_background(void);
void redraw_desktop_background(int x, int y, int w, int h);

// desktop_alt_tab.c
extern struct window_t *alttab_win;

void desktop_init_alttab(void);
void desktop_prep_alttab(void);
void desktop_cancel_alttab(void);
void desktop_draw_alttab(void);
void desktop_finish_alttab(void);

#endif      /* GUI_DESKTOP_H */
