/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: top-panel.c
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
 *  \file top-panel.c
 *
 *  The desktop top panel. This is shown at the top of the screen
 *  and shows a list of widgets, e.g. the clock and the applications menu.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <kernel/clock.h>
#include "../include/client/window.h"
#include "../include/client/paths.h"
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/font.h"
#include "../include/keys.h"
#include "../include/panels/top-panel.h"
#include "../include/panels/widget.h"


#define GLOB                        __global_gui_data


// our top panel window
struct window_t *main_window;

struct gc_t backbuf_gc;


void repaint_toppanel(struct window_t *window, int is_active_child)
{
    gc_fill_rect(&backbuf_gc, 0, 0,
                 main_window->w, main_window->h, TOPPANEL_BGCOLOR);

    widgets_redraw();
    A_memcpy(main_window->canvas, backbuf_gc.buffer, backbuf_gc.buffer_size);
    window_invalidate_rect(main_window, 0, 0,
                           main_window->y + main_window->h - 1,
                           main_window->x + main_window->w - 1);
}


int main(int argc, char **argv)
{
    struct event_t *ev;
    struct window_attribs_t attribs;
    time_t last_sec = 0;

    gui_init(argc, argv);

    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = GLOB.screen.w;
    attribs.h = TOPPANEL_HEIGHT;
    attribs.flags = WINDOW_NODECORATION | WINDOW_NORAISE | 
                    WINDOW_NOFOCUS | WINDOW_ALWAYSONTOP;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n",
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    main_window->repaint = repaint_toppanel;

    backbuf_gc.w = main_window->w;
    backbuf_gc.h = main_window->h;
    backbuf_gc.pixel_width = main_window->gc->pixel_width;
    backbuf_gc.pitch = backbuf_gc.w * backbuf_gc.pixel_width;
    backbuf_gc.buffer_size = backbuf_gc.pitch * backbuf_gc.h;
    backbuf_gc.buffer = malloc(backbuf_gc.buffer_size);
    backbuf_gc.screen = main_window->gc->screen;
    backbuf_gc.clipping.clip_rects = NULL;
    backbuf_gc.clipping.clipping_on = 0;
    mutex_init(&backbuf_gc.lock);

    gc_set_font(&backbuf_gc, GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono);
    
    widgets_init();

    repaint_toppanel(main_window, 0);
    
    window_show(main_window);

    while(1)
    {
        fd_set rdfs;
        int i;
        struct timeval tv, now;

        FD_ZERO(&rdfs);
        FD_SET(GLOB.serverfd, &rdfs);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        i = select(GLOB.serverfd + 1, &rdfs, NULL, NULL, &tv);

        if(i > 0)
        {
            if(FD_ISSET(GLOB.serverfd, &rdfs))
            {
                __get_event(GLOB.serverfd, GLOB.evbuf_internal, GLOB.evbufsz, 0);
                ev = (struct event_t *)GLOB.evbuf_internal;

                if(!event_dispatch(ev))
                {
                    switch(ev->type)
                    {
                        case EVENT_WINDOW_LOWERED:
                            widget_menu_may_hide(GLOB.evbuf_internal->dest);
                            break;

                        case EVENT_KEY_PRESS:
                            // if the Apps key was pressed, show the 
                            // Applications menu
                            if(ev->key.code == KEYCODE_APPS)
                            {
                                widgets_show_apps();
                            }
                            // if the Calculator key was pressed, run the 
                            // Calculator app
                            else if(ev->key.code == KEYCODE_CALC)
                            {
                                widget_run_command(CALCULATOR_EXE);
                            }

                            break;

                        default:
                            break;
                    }
                }
            }
        }
        
        gettimeofday(&now, NULL);
        
        if(now.tv_sec != last_sec)
        {
            last_sec = now.tv_sec;
            widgets_periodic();
        }
    }
}

