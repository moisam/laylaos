/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
 * 
 *    file: gc.c
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
 *  \file gc.c
 *
 *  Functions to work with graphical contexts on the server side.
 *  The general graphical context functions are found in common/gc.c.
 */

#define GUI_SERVER

#include "../common/gc.c"

#define GLOB                            __global_gui_data


void gc_clipped_window(struct gc_t *gc, struct server_window_t *window,
                                        int x, int y, int max_x, int max_y,
                                        Rect *clip_area)
{
    // Make sure we don't go outside of the clip region:
    if(x < clip_area->left)
    {
        x = clip_area->left;
    }

    if(y < clip_area->top)
    {
        y = clip_area->top;
    }

    if(max_x > clip_area->right + 1)
    {
        max_x = clip_area->right + 1;
    }

    if(max_y > clip_area->bottom + 1)
    {
        max_y = clip_area->bottom + 1;
    }

    if(x > max_x)
    {
        x = max_x;
    }

    if(y > max_y)
    {
        y = max_y;
    }

    unsigned where = x * gc->pixel_width + y * gc->pitch;
    unsigned wwhere = (x - window->client_x) * gc->pixel_width +
                      (y - window->client_y) * window->canvas_pitch;
    uint8_t *buf = (uint8_t *)(gc->buffer + where);
    uint8_t *wbuf = (uint8_t *)(window->canvas + wwhere);

    if(window->flags & WINDOW_TRANSPARENT)
    {
        int x1;

        if(gc->pixel_width == 1)
        {
            for(; y < max_y; y++)
            {
                uint8_t *pd = buf;
                uint8_t *ps = wbuf;

                for(x1 = x; x1 < max_x; x1++)
                {
                    if(*ps != 0)
                    {
                        *pd = *ps;
                    }

                    pd++;
                    ps++;
                }

                buf += gc->pitch;
                wbuf += window->canvas_pitch;
            }
        }
        else if(gc->pixel_width == 2)
        {
            for(; y < max_y; y++)
            {
                uint16_t *pd = (uint16_t *)buf;
                uint16_t *ps = (uint16_t *)wbuf;

                for(x1 = x; x1 < max_x; x1++)
                {
                    if(*ps != 0)
                    {
                        *pd = *ps;
                    }

                    pd++;
                    ps++;
                }

                buf += gc->pitch;
                wbuf += window->canvas_pitch;
            }
        }
        else if(gc->pixel_width == 3)
        {
            for(; y < max_y; y++)
            {
                uint8_t *pd = buf;
                uint8_t *ps = wbuf;

                for(x1 = x; x1 < max_x; x1++)
                {
                    if(*ps != 0)
                    {
                        pd[0] = ps[0];
                        pd[1] = ps[1];
                        pd[2] = ps[2];
                    }

                    pd += 3;
                    ps += 3;
                }

                buf += gc->pitch;
                wbuf += window->canvas_pitch;
            }
        }
        else if(gc->pixel_width == 4)
        {
            for(; y < max_y; y++)
            {
                uint32_t *pd = (uint32_t *)buf;
                uint32_t *ps = (uint32_t *)wbuf;

                for(x1 = x; x1 < max_x; x1++)
                {
                    if(*ps != 0)
                    {
                        *pd = *ps;
                    }

                    pd++;
                    ps++;
                }

                buf += gc->pitch;
                wbuf += window->canvas_pitch;
            }
        }
    }
    else
    {
        int cnt = (max_x - x) * gc->pixel_width;

        for(; y < max_y; y++)
        {
            A_memcpy(buf, wbuf, cnt);
            buf += gc->pitch;
            wbuf += window->canvas_pitch;
        }
    }
}


void gc_copy_window(struct gc_t *gc, struct server_window_t *window)
{
    int x = window->client_x;
    int y = window->client_y;
    int max_x = window->client_xw1 + 1;
    int max_y = window->client_yh1 + 1;
    Rect screen_area;
    Rect *cur_rect;
    
    if(!window->canvas)
    {
        return;
    }
    
    // If there are clipping rects, draw the rect clipped to
    // each of them. Otherwise, draw unclipped (clipped to the screen)
    if(window->clipping.clip_rects && window->clipping.clip_rects->root)
    {
        for(cur_rect = window->clipping.clip_rects->root;
            cur_rect != NULL;
            cur_rect = cur_rect->next)
        {
            gc_clipped_window(gc, window, x, y, max_x, max_y, cur_rect);
        }
    }
    else
    {
        if(!window->clipping.clipping_on)
        {
            screen_area.top = 0;
            screen_area.left = 0;
            screen_area.bottom = gc->h - 1;
            screen_area.right = gc->w - 1;
            gc_clipped_window(gc, window, x, y, max_x, max_y, &screen_area);
        }
    }
}

