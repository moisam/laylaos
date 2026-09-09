/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
 * 
 *    file: gc-circle.c
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
 *  \file gc-circle.c
 *
 *  The graphics context implementation is divided into multiple files:
 *  - gc.c: contains functions to create and destroy graphics context objects,
 *    as well as perform basic drawing functions,
 *  - gc-arc.c: functions to draw arcs,
 *  - gc-bitmap.c: functions to copy (blit) bitmaps,
 *  - gc-bitmap-stretch.c: functions to copy bitmaps stretched,
 *  - gc-circle.c: functions to draw circles (hollow and filled),
 *  - gc-line.c: functions to draw lines of different thickness,
 *  - gc-poly.c: functions to draw polygons (hollow and filled),
 *  - gc-round-rect.c: functions to draw round edge rectangles (hollow and filled),
 *  - gc-ttf.c: functions to draw text using TrueType Fonts (TTF),
 *  - gc-gc.c: functions to convert pixels between different gc formats,
 *  - gc-grad-vert.c: functions to draw vertical gradients,
 */

#include <math.h>
#include "../include/gui.h"
#include "../include/gc.h"
#include "../include/rgb.h"

#include "gc-inlines.h"


void xline(struct gc_t *gc, int x1, int x2, int y, 
           Rect *clip_area, uint32_t color)
{
    if(y < clip_area->top || y > clip_area->bottom)
    {
        return;
    }

    if(x1 < clip_area->left)
    {
        x1 = clip_area->left;
    }

    if(x2 > clip_area->right)
    {
        x2 = clip_area->right;
    }
    
    y *= gc->pitch;

    while(x1 <= x2)
    {
        unsigned where = x1 * gc->pixel_width + y;
        uint8_t *buf = (uint8_t *)(gc->buffer + where);

        if(gc->pixel_width == 1)
        {
            *buf = (uint8_t)color;
        }
        else if(gc->pixel_width == 2)
        {
            *(uint16_t *)buf = (uint16_t)color;
        }
        else if(gc->pixel_width == 3)
        {
            buf[0] = color & 0xff;
            buf[1] = (color >> 8) & 0xff;
            buf[2] = (color >> 16) & 0xff;
        }
        else
        {
            *(uint32_t *)buf = color;
        }

        x1++;
    }
}


void yline(struct gc_t *gc, int x, int y1, int y2, 
           Rect *clip_area, uint32_t color)
{
    if(x < clip_area->left || x > clip_area->right)
    {
        return;
    }

    if(y1 < clip_area->top)
    {
        y1 = clip_area->top;
    }

    if(y2 > clip_area->bottom)
    {
        y2 = clip_area->bottom;
    }
    
    x *= gc->pixel_width;
    
    while(y1 <= y2)
    {
        unsigned where = x + y1 * gc->pitch;
        uint8_t *buf = (uint8_t *)(gc->buffer + where);

        if(gc->pixel_width == 1)
        {
            *buf = (uint8_t)color;
        }
        else if(gc->pixel_width == 2)
        {
            *(uint16_t *)buf = (uint16_t)color;
        }
        else if(gc->pixel_width == 3)
        {
            buf[0] = color & 0xff;
            buf[1] = (color >> 8) & 0xff;
            buf[2] = (color >> 16) & 0xff;
        }
        else
        {
            *(uint32_t *)buf = color;
        }

        y1++;
    }
}


void pixel(struct gc_t *gc, int x, int y, 
           Rect *clip_area, uint32_t color)
{
    if(x < clip_area->left || x > clip_area->right)
    {
        return;
    }

    if(y < clip_area->top || y > clip_area->bottom)
    {
        return;
    }
    
    unsigned where = x * gc->pixel_width + y * gc->pitch;
    uint8_t *buf = (uint8_t *)(gc->buffer + where);

    if(gc->pixel_width == 1)
    {
        *buf = (uint8_t)color;
    }
    else if(gc->pixel_width == 2)
    {
        *(uint16_t *)buf = (uint16_t)color;
    }
    else if(gc->pixel_width == 3)
    {
        buf[0] = color & 0xff;
        buf[1] = (color >> 8) & 0xff;
        buf[2] = (color >> 16) & 0xff;
    }
    else
    {
        *(uint32_t *)buf = color;
    }
}


static void __gc_oval_filled_clipped(struct gc_t *gc, Rect *clip_area,
                                     int xci, int yci, int xr, int yr, uint32_t color)
{
    float a = (float)xr;
    float b = (float)yr;
    float invA2 = 1.0f / (a * a);
    float invB2 = 1.0f / (b * b);
    float xc = (float)xci;
    float yc = (float)yci;
    int i, j;

    // Remove alpha
    color &= 0xffffff00;

    for(i = yci - yr; i < yci + yr; i++)
    {
        for(j = xci - xr; j < xci + xr; j++)
        {
            if(j < clip_area->left || j > clip_area->right)
            {
                continue;
            }

            if(i < clip_area->top || i > clip_area->bottom)
            {
                continue;
            }

            // Normalized distance from center
            float dx = (j + 0.5f - xc);
            float dy = (i + 0.5f - yc);
            float dx2 = dx * dx;
            float dy2 = dy * dy;
            float dist = (dx2 * invA2) + (dy2 * invB2);

            if(dist <= 1.0f)
            {
                // Inside or on the edge
                // Simple antialias: check the "steepness" of the exit
                float edge_dist = 1.0f - dist;
                float width_at_angle = sqrtf(dx2 + dy2);
                float alpha_f = edge_dist * width_at_angle * 2.0f; // Softness factor
                uint8_t alpha = (alpha_f > 1.0f) ? 255 : (uint8_t)(alpha_f * 255);

                __fill_pixel(gc, i, j, color, alpha);
            }
        }
    }
}


static void gc_oval_filled_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                                   int xc, int yc, int xr, int yr, uint32_t color)
{
    struct clipping_t tmp_clipping;
    struct clipping_t *clipping;

    Rect *clip_area;
    Rect screen_area;
    RectList clip_rects;

    __prep_clipping_internal(gc, &clipping, __clipping, &tmp_clipping, &clip_rects, &screen_area);
    
    for(clip_area = clipping->clip_rects->root;
        clip_area != NULL;
        clip_area = clip_area->next)
    {
        __gc_oval_filled_clipped(gc, clip_area, xc, yc, xr, yr, color);
    }
}


static void __gc_oval_clipped(struct gc_t *gc, Rect *clip_area,
                                     int xci, int yci, int xr, int yr, int thickness, uint32_t color)
{
    float a_out = (float)xr;
    float b_out = (float)yr;
    float a_in  = a_out - thickness;
    float b_in  = b_out - thickness;
    float invA2 = 1.0f / (a_out * a_out);
    float invB2 = 1.0f / (b_out * b_out);
    float invA2in = 1.0f / (a_in * a_in);
    float invB2in = 1.0f / (b_in * b_in);
    float xc = (float)xci;
    float yc = (float)yci;
    int i, j;

    // Remove alpha
    color &= 0xffffff00;

    for(i = yci - yr - 1; i < yci + yr + 1; i++)
    {
        for(j = xci - xr - 1; j < xci + xr + 1; j++)
        {
            if(j < clip_area->left || j > clip_area->right)
            {
                continue;
            }

            if(i < clip_area->top || i > clip_area->bottom)
            {
                continue;
            }

            float dx = (j + 0.5f - xc);
            float dy = (i + 0.5f - yc);
            float dx2 = dx * dx;
            float dy2 = dy * dy;

            // Normalized distances: 1.0 is exactly on the edge
            float d_out = sqrtf((dx2 * invA2) + (dy2 * invB2));
            float d_in  = (a_in <= 0 || b_in <= 0) ? 0 : sqrtf(dx2 * invA2in + dy2 * invB2in);

            float alpha_out = 1.0f;
            float alpha_in = 1.0f;

            // Antialias the Outer Edge
            // If we are near d_out = 1.0, calculate fade
            if(d_out > 1.0f)
            {
                alpha_out = 1.0f - (d_out - 1.0f) * a_out; // Steepness based on size
            }

            // Antialias the Inner Edge
            // If we are near d_in = 1.0, calculate fade
            if(d_in < 1.0f && d_in > 0)
            {
                alpha_in = (d_in - (1.0f - 1.0f/a_in)) * a_in; 
            }

            // Combine alphas
            float final_alpha_f = alpha_out;

            if(d_in < 1.0f)
            {
                final_alpha_f *= alpha_in;
            }

            if(final_alpha_f > 0.0f)
            {
                if(final_alpha_f > 1.0f)
                {
                    final_alpha_f = 1.0f;
                }

                uint8_t alpha = (uint8_t)(final_alpha_f * 255);

                __fill_pixel(gc, i, j, color, alpha);
            }
        }
    }
}


static void gc_oval_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                                   int xc, int yc, int xr, int yr, int thickness, uint32_t color)
{
    struct clipping_t tmp_clipping;
    struct clipping_t *clipping;

    Rect *clip_area;
    Rect screen_area;
    RectList clip_rects;

    __prep_clipping_internal(gc, &clipping, __clipping, &tmp_clipping, &clip_rects, &screen_area);

    for(clip_area = clipping->clip_rects->root;
        clip_area != NULL;
        clip_area = clip_area->next)
    {
        __gc_oval_clipped(gc, clip_area, xc, yc, xr, yr, thickness, color);
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_circle(struct gc_t *gc, int xc, int yc,
                                int radius, int thickness, uint32_t color)
{
    gc_oval_clipped(gc, &gc->clipping, xc, yc, radius, radius, thickness, color);
}


void gc_circle_filled(struct gc_t *gc, int xc, int yc,
                                       int radius, uint32_t color)
{
    gc_oval_filled_clipped(gc, &gc->clipping, xc, yc, radius, radius, color);
}


void gc_oval(struct gc_t *gc, int xc, int yc,
                              int xr, int yr, int thickness, uint32_t color)
{
    gc_oval_clipped(gc, &gc->clipping, xc, yc, xr, yr, thickness, color);
}


void gc_oval_filled(struct gc_t *gc, int xc, int yc,
                                     int xr, int yr, uint32_t color)
{
    gc_oval_filled_clipped(gc, &gc->clipping, xc, yc, xr, yr, color);
}

