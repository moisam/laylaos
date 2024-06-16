/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: desktop_entries.c
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
 *  \file desktop_entries.c
 *
 *  The desktop entry implementation. This code loads desktop entries and
 *  processes mouse actions, e.g. load a program when the user double clicks
 *  its desktop entry.
 */

#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "../include/gui.h"
#include "../include/resources.h"
#include "../include/client/window.h"
#include "../include/font.h"
#include "../include/panels/top-panel.h"
#include "../include/panels/bottom-panel.h"
#include "../include/mouse.h"
#include "desktop.h"
#include "desktop_entries.h"

#ifndef MAX
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#endif

#define ICONWIDTH               64

#define LEFT_MARGIN             16
#define RIGHT_MARGIN            16
#define ENTRYWIDTH              (ICONWIDTH + LEFT_MARGIN + RIGHT_MARGIN)

#define HIGHLIGHT_COLOR         0xFFFBCCAA
#define BG_COLOR                0x16A085FF

#define GLOB                    __global_gui_data


struct app_entry_t *first_entry = NULL;
struct app_entry_t *last_entry = NULL;
struct app_entry_t *selected_entry = NULL;

#include "desktop_entry_lister.c"
#include "desktop_entry_lines.c"

// defined in desktop.c
extern struct window_t *desktop_window;


void invalidate_entry_rect(struct app_entry_t *desktop_entry)
{
    window_invalidate_rect(desktop_window,
                           desktop_entry->basey, desktop_entry->basex,
                           desktop_entry->basey + desktop_entry->h - 1,
                           desktop_entry->basex + ENTRYWIDTH);
}


void paint_entry(struct app_entry_t *desktop_entry)
{
    int x, y, x1;
    size_t i, pixels;
    int selected = (selected_entry == desktop_entry);
    uint32_t text_color = selected ? 0x000000FF : 0xFFFFFFFF;
    uint32_t bg_color = selected ? HIGHLIGHT_COLOR : BG_COLOR;
    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    int charh = char_height(font, ' ');

    // paint the icon (highlighted if necessary)
    gc_blit_bitmap_highlighted(desktop_window->gc, &desktop_entry->icon_bitmap,
                               desktop_entry->basex + LEFT_MARGIN,
                               desktop_entry->basey,
                               0, 0, ICONWIDTH, ICONWIDTH,
                               (selected_entry == desktop_entry) ?
                                                HIGHLIGHT_COLOR : 0);

    x = desktop_entry->basex;
    y = desktop_entry->basey + ICONWIDTH;

    if(!desktop_entry->name || !*desktop_entry->name)
    {
        return;
    }

    pixels = MAX(desktop_entry->name_line_pixels[0], 
                 desktop_entry->name_line_pixels[1]);

    // draw a background box (color depends on whether the entry 
    // is highlighted)
    gc_fill_rect(desktop_window->gc,
                 x + ((ENTRYWIDTH - pixels) / 2), y,
                 pixels, charh, bg_color);

    // draw the first line
    x1 = x + ((ENTRYWIDTH - desktop_entry->name_line_pixels[0]) / 2);
    i = desktop_entry->name_line_end[0] -
              desktop_entry->name_line_start[0];

    {
        char buf[i + 1];

        memcpy(buf,
                desktop_entry->name + desktop_entry->name_line_start[0], i);
        buf[i] = '\0';

        gc_draw_text(desktop_window->gc, buf, x1, y, text_color, 0);
    }

    // draw the second line, if there is one
    if(desktop_entry->name_line_pixels[1])
    {
        y += charh;
        x1 = x + ((ENTRYWIDTH - desktop_entry->name_line_pixels[1]) / 2);
        i = desktop_entry->name_line_end[1] -
                  desktop_entry->name_line_start[1];

        char buf[i + 1];

        memcpy(buf,
                desktop_entry->name + desktop_entry->name_line_start[1], i);
        buf[i] = '\0';

        gc_fill_rect(desktop_window->gc,
                     x + ((ENTRYWIDTH - pixels) / 2), y,
                     pixels, charh, bg_color);
        gc_draw_text(desktop_window->gc, buf, x1, y, text_color, 0);
    }
}


void desktop_mouseover(struct window_t *window, int x, int y,
                       mouse_buttons_t buttons, unsigned long long ticks)
{
    int lbutton_down = buttons & MOUSE_LBUTTON_DOWN;
    int last_lbutton_down = window->last_button_state & MOUSE_LBUTTON_DOWN;
    int clicked = (lbutton_down && !last_lbutton_down);
    int dragging = (lbutton_down && last_lbutton_down);
    struct app_entry_t *old_selected_entry = selected_entry;
    struct app_entry_t *desktop_entry;
    
    if(clicked)
    {
        selected_entry = NULL;
        desktop_cancel_alttab();
    }

    for(desktop_entry = first_entry;
        desktop_entry != NULL;
        desktop_entry = desktop_entry->next)
    {
        if(!(x >= desktop_entry->x && 
             x < (desktop_entry->x + desktop_entry->w) &&
             y >= desktop_entry->y && 
             y < (desktop_entry->y + desktop_entry->h)))
        {
            continue;
        }

        if(!desktop_entry->icon_bitmap.data)
        {
            break;
        }

        if(clicked)
        {
            // move it to the front so it gets mouse events first
            if(first_entry != desktop_entry)
            {
                if(desktop_entry->next)
                {
                    desktop_entry->next->prev = desktop_entry->prev;
                }

                if(desktop_entry->prev)
                {
                    desktop_entry->prev->next = desktop_entry->next;
                }

                desktop_entry->next = first_entry;
                desktop_entry->prev = NULL;
                first_entry->prev = desktop_entry;
                first_entry = desktop_entry;
            }

            selected_entry = desktop_entry;

            if(desktop_entry->click_count &&
                (ticks - desktop_entry->click_ticks < DOUBLE_CLICK_THRESHOLD))
            {
                desktop_entry->click_count = 2;
            }
            else
            {
                desktop_entry->click_count = 1;
                desktop_entry->click_ticks = ticks;
                desktop_entry->mouse_bdx = x - desktop_entry->basex;
                desktop_entry->mouse_bdy = y - desktop_entry->basey;
                desktop_entry->mouse_dx = x - desktop_entry->x;
                desktop_entry->mouse_dy = y - desktop_entry->y;
            }
        }

        break;
    }
    
    if(old_selected_entry && old_selected_entry != selected_entry)
    {
        paint_entry(old_selected_entry);
        old_selected_entry->click_count = 0;
        old_selected_entry->mouse_bdx = 0;
        old_selected_entry->mouse_bdy = 0;
        old_selected_entry->mouse_dx = 0;
        old_selected_entry->mouse_dy = 0;
        invalidate_entry_rect(old_selected_entry);
    }
    
    if(selected_entry)
    {
        if(selected_entry->click_count > 1)
        {
            // handle double clicks
            selected_entry->click_count = 0;

            if(!fork())
            {
                char *argv[] = { selected_entry->command, NULL };
                int fd;

                //setenv("GCOV_PREFIX", "/root", 1);
                //setenv("GCOV_PREFIX_STRIP", "8", 1);

                /* Release our controlling tty */
                (void)ioctl(0, TIOCSCTTY, 0);

                fd = open("/dev/null", O_RDWR);
                dup2(fd, 0);
                dup2(fd, 1);
                dup2(fd, 2);
                close(fd);
                close(GLOB.serverfd);

                execvp(selected_entry->command, argv);
                exit(EXIT_FAILURE);
            }
        }
        else if(dragging)
        {
            // handle dragging
            int left = selected_entry->x;
            int top = selected_entry->y;
            int right = left + selected_entry->w;
            int bottom = top + selected_entry->h;

            // first, redraw desktop background and any entries this one might
            // have been overlapping
            redraw_desktop_background(left, top,
                                      selected_entry->w,
                                      selected_entry->h);
            window_invalidate_rect(desktop_window, top, left,
                                   bottom - 1, right - 1);

            for(desktop_entry = first_entry;
                desktop_entry != NULL;
                desktop_entry = desktop_entry->next)
            {
                if(!(left <= desktop_entry->x + desktop_entry->w &&
            	     right >= desktop_entry->x &&
            	     top <= desktop_entry->y + desktop_entry->h &&
            	     bottom >= desktop_entry->y))
            	{
                    continue;
                }

                if(desktop_entry == selected_entry || 
                   !desktop_entry->icon_bitmap.data)
                {
                    continue;
                }

                paint_entry(desktop_entry);
                invalidate_entry_rect(desktop_entry);
            }

            // now reposition and redraw the entry
            selected_entry->basex = x - selected_entry->mouse_bdx;
            selected_entry->basey = y - selected_entry->mouse_bdy;
            selected_entry->x = x - selected_entry->mouse_dx;
            selected_entry->y = y - selected_entry->mouse_dy;
            paint_entry(selected_entry);
            invalidate_entry_rect(selected_entry);
        }
        else if(old_selected_entry != selected_entry)
        {
            // only repaint if newly selected
            paint_entry(selected_entry);
            invalidate_entry_rect(selected_entry);
        }
    }
    
    window->last_button_state = buttons;
}


void load_desktop_entries(void)
{
    struct app_entry_t *desktop_entry, *next;
    char *p;
    int x = 0, y = TOPPANEL_HEIGHT;
    int desktop_height = GLOB.screen.h - TOPPANEL_HEIGHT - BOTTOMPANEL_HEIGHT;
    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    int charh = char_height(font, ' ');

    if(!(p = malloc(PATH_MAX)))
    {
        return;
    }

    if(ftree(DEFAULT_DESKTOP_PATH, p) != 0)
    {
        free(p);
        return;
    }

    // remove the apps that don't show on the desktop
    desktop_entry = first_entry;

    while(desktop_entry)
    {
        next = desktop_entry->next;

        if(!(desktop_entry->flags & APPLICATION_FLAG_SHOW_ON_DESKTOP))
        {
            if(desktop_entry->next)
            {
                desktop_entry->next->prev = desktop_entry->prev;
            }

            if(desktop_entry->prev)
            {
                desktop_entry->prev->next = desktop_entry->next;
            }

            if(desktop_entry == first_entry)
            {
                first_entry = desktop_entry->next;

                if(first_entry)
                {
                    first_entry->prev = NULL;
                }
            }

            if(desktop_entry == last_entry)
            {
                last_entry = desktop_entry->prev;

                if(last_entry)
                {
                    last_entry->next = NULL;
                }
            }

            free_tmp(desktop_entry->name, desktop_entry->command,
                     desktop_entry->iconpath, desktop_entry->icon);
            free(desktop_entry);
        }

        desktop_entry = next;
    }

    // now paint the apps on the desktop
    for(desktop_entry = first_entry;
        desktop_entry != NULL;
        desktop_entry = desktop_entry->next)
    {
        snprintf(p, PATH_MAX, "%s/%s.ico", desktop_entry->iconpath, 
                                           desktop_entry->icon);

        desktop_entry->icon_bitmap.width = ICONWIDTH;
        desktop_entry->icon_bitmap.height = ICONWIDTH;
        desktop_entry->icon_bitmap.data = NULL;

        if(!(image_load(p, &desktop_entry->icon_bitmap)))
        {
            desktop_entry->icon_bitmap.width = ICONWIDTH;
            desktop_entry->icon_bitmap.height = ICONWIDTH;

            if(!(image_load(DEFAULT_EXE_ICON_PATH, 
                            &desktop_entry->icon_bitmap)))
            {
                continue;
            }
        }
        
        desktop_entry->basex = x;
        desktop_entry->basey = y;
        desktop_entry->x = x + LEFT_MARGIN;
        desktop_entry->y = y;
        desktop_entry->w = ICONWIDTH;
        desktop_entry->h = ICONWIDTH + charh;

        split_two_lines(font, desktop_entry->name,
                              desktop_entry->name_line_start,
                              desktop_entry->name_line_end,
                              desktop_entry->name_line_pixels,
                              ENTRYWIDTH);

        paint_entry(desktop_entry);

        desktop_entry->w = MAX(desktop_entry->name_line_pixels[0], 
                               desktop_entry->name_line_pixels[1]);
        desktop_entry->w = MAX(desktop_entry->w, ICONWIDTH);

        desktop_entry->x = x + ((ENTRYWIDTH - desktop_entry->w) / 2);

        if(desktop_entry->name_line_pixels[1])
        {
            desktop_entry->h += charh;
        }

        y += ICONWIDTH + (charh * 2);
        
        if(y + ICONWIDTH + charh >= desktop_height)
        {
            x += ENTRYWIDTH;
            y = TOPPANEL_HEIGHT;
            
            if(x >= GLOB.screen.w)
            {
                break;
            }
        }
    }
    
    free(p);
}

