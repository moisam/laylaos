/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: main.c
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
 *  \file main.c
 *
 *  A simple clock program.
 */

/*
 * Code based on the YouTube tutorial:
 *    https://www.youtube.com/watch?v=dsTVo479KEQ
 */
#include <errno.h>
#include <math.h>
#include "../include/gui.h"
#include "../include/gc.h"
#include "../include/client/window.h"

#define WHITE       0xFFFFFFFF
#define BLACK       0x000000FF

#ifndef ABS
#define ABS(x)      ((x) < 0 ? -(x) : (x))
#endif

int m = 0, h = 0;
int thetamin = 0, thetasec = 0, thetahr = 0;
int x = 120, y = 120, r = 100;

struct window_t *main_window;

static void draw_clock(void)
{
    int i, dec;
    static char num[12][3] = { "3", "2", "1", "12", "11", "10",
                               "9", "8", "7", "6", "5", "4", };

    gc_fill_rect(main_window->gc, 0, 0,
                 main_window->w, main_window->h, WHITE);

    gc_circle(main_window->gc, x, y, r, 4, BLACK);

    for(i = 0; i < 12; i++)
    {
        dec = (i != 3) ? 5 : 15;

        gc_draw_text(main_window->gc, num[i],
                 x + (r - 14) * cos(M_PI / 6 * i) - dec,
                 y - (r - 14) * sin(M_PI / 6 * i) - 10,
                 BLACK, 0);
    }
}


static inline void draw_line(float dist, float angle_in_degrees,
                             int thickness, uint32_t color)
{
    int x2, y2;

    x2 = x + dist * cos(angle_in_degrees * M_PI / 180);
    y2 = y + dist * sin(angle_in_degrees * M_PI / 180);

    gc_line(main_window->gc, x, y, x2, y2, thickness, color);
}


int main(int argc, char **argv)
{
    struct window_attribs_t attribs;
    struct event_t *ev = NULL;

    time_t t = 0;
    struct tm *tm = NULL;

    gui_init(argc, argv);

    attribs.gravity = WINDOW_ALIGN_TOP | WINDOW_ALIGN_RIGHT;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 240;
    attribs.h = 240;
    attribs.flags = WINDOW_NORESIZE;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n",
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    draw_clock();
    window_set_title(main_window, "Clock");
    window_set_icon(main_window, "clock.ico");
    window_show(main_window);

    while(1)
    {
        time(&t);
        tm = gmtime(&t);

        if(m != tm->tm_min)
        {
#if 0
            gc_line(main_window->gc, x, y,
                    x + (r - 40) * cos(thetamin * (M_PI / 180)),
                    y - (r - 40) * sin(thetamin * (M_PI / 180)),
                    2, WHITE);

            gc_line(main_window->gc, x, y,
                    x + (r - 140) * cos(thetahr * (M_PI / 180)),
                    y - (r - 140) * sin(thetahr * (M_PI / 180)),
                    3, WHITE);
            /*
            gc_line(main_window->gc, x, y,
                    x + (r - 140) * cos(M_PI / 6 * h - ((m / 2) * (M_PI / 180))),
                    y - (r - 140) * sin(M_PI / 6 * h - ((m / 2) * (M_PI / 180))),
                    3, WHITE);
            */
#endif

            draw_line(70, (-90 + m * 6), 2, WHITE);
            draw_line(40, (-90 + h * 360 / 12 + (m * 30 / 60)), 3, WHITE);
            draw_line(40, (-90 + h * 360 / 12), 3, WHITE);
        }

        if(tm->tm_hour >= 12)
        {
            tm->tm_hour -= 12;
        }



        m = tm->tm_min;
        h = tm->tm_hour;
        draw_line(75, (-90 + tm->tm_sec * 6), 1, BLACK);
        draw_line(70, (-90 + tm->tm_min * 6), 2, BLACK);
        draw_line(40, (-90 + tm->tm_hour * 360 / 12 + (m * 30 / 60)), 3, BLACK);


#if 0
        /*
        if(tm->tm_hour < 4)
        {
            h = ABS(tm->tm_hour - 3);
        }
        else
        {
            h = 15 - tm->tm_hour;
        }
        */
        thetahr = (15 - tm->tm_hour) * 6;

        m = tm->tm_min;

        if(tm->tm_min <= 15)
        {
            thetamin = (15 - tm->tm_min) * 6;
        }
        else
        {
            thetamin = 450 - tm->tm_min * 6;
        }

        if(tm->tm_sec <= 15)
        {
            thetasec = (15 - tm->tm_sec) * 6;
        }
        else
        {
            thetasec = 450 - tm->tm_sec * 6;
        }

        gc_line(main_window->gc, x, y,
                x + (r - 140) * cos(thetahr * (M_PI / 180)),
                y - (r - 140) * sin(thetahr * (M_PI / 180)),
                3, BLACK);
        /*
        gc_line(main_window->gc, x, y,
                x + (r - 140) * cos(M_PI / 6 * h - ((m / 2) * (M_PI / 180))),
                y - (r - 140) * sin(M_PI / 6 * h - ((m / 2) * (M_PI / 180))),
                3, BLACK);
        */

        gc_line(main_window->gc, x, y,
                x + (r - 40) * cos(thetamin * (M_PI / 180)),
                y - (r - 40) * sin(thetamin * (M_PI / 180)),
                2, BLACK);

        gc_line(main_window->gc, x, y,
                x + (r - 170) * cos(thetasec * (M_PI / 180)),
                y - (r - 170) * sin(thetasec * (M_PI / 180)),
                1, BLACK);
#endif

        window_invalidate(main_window);

        if(pending_events_utimeout(1000000))
        {
            if((ev = next_event_for_seqid(NULL, 0, 0)))
            {
                if(!event_dispatch(ev))
                {
                    switch(ev->type)
                    {
                        case EVENT_WINDOW_CLOSING:
                            window_destroy(main_window);
                            gui_exit(EXIT_SUCCESS);
                            break;

                        default:
                            break;
                    }
                }

                free(ev);
            }
        }

        draw_line(75, (-90 + tm->tm_sec * 6), 1, WHITE);
#if 0
        gc_line(main_window->gc, x, y,
                x + (r - 170) * cos(thetasec * (M_PI / 180)),
                y - (r - 170) * sin(thetasec * (M_PI / 180)),
                1, WHITE);
#endif

    }
}

