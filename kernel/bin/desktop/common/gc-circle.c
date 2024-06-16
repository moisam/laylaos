/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
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
 *  - gc-ttf.c: functions to draw text using TrueType Fonts (TTF),
 */

#include <math.h>
#include "../include/gui.h"
#include "../include/gc.h"
#include "../include/rgb.h"


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

#undef ABS
#define ABS(x)      ((x) < 0 ? -(x) : (x))

void gc_circle_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                       int xc, int yc, int radius, int thickness, 
                       uint32_t color)
{
    int xo = radius;
    int inner = radius - ABS(thickness) + 1;
    int xi = inner;
    int y = 0;
    int erro = 1 - xo;
    int erri = 1 - xi;

    struct clipping_t tmp_clipping;
    struct clipping_t *clipping;

    Rect *clip_area;
    Rect screen_area;
    RectList clip_rects;

    if(gc->pixel_width == 1)
    {
        color = to_rgb8(gc, color);
    }
    else if(gc->pixel_width == 2)
    {
        color = to_rgb16(gc, color);
    }
    else if(gc->pixel_width == 3)
    {
        color = to_rgb24(gc, color);
    }
    else
    {
        color = to_rgb32(gc, color);
    }

    if(__clipping && __clipping->clip_rects && __clipping->clip_rects->root)
    {
        clipping = __clipping;
    }
    else
    {
        clipping = &tmp_clipping;
        tmp_clipping.clipping_on = 0;
        tmp_clipping.clip_rects = &clip_rects;

        if(!__clipping || !__clipping->clipping_on)
        {
            clip_rects.root = &screen_area;
            screen_area.top = 0;
            screen_area.left = 0;
            screen_area.bottom = gc->h - 1;
            screen_area.right = gc->w - 1;
            screen_area.next = NULL;
        }
        else
        {
            clip_rects.root = NULL;
        }
    }

    while(xo >= y)
    {
        for(clip_area = clipping->clip_rects->root;
            clip_area != NULL;
            clip_area = clip_area->next)
        {
            xline(gc, xc + xi, xc + xo, yc + y,  clip_area, color);
            yline(gc, xc + y,  yc + xi, yc + xo, clip_area, color);
            xline(gc, xc - xo, xc - xi, yc + y,  clip_area, color);
            yline(gc, xc - y,  yc + xi, yc + xo, clip_area, color);
            xline(gc, xc - xo, xc - xi, yc - y,  clip_area, color);
            yline(gc, xc - y,  yc - xo, yc - xi, clip_area, color);
            xline(gc, xc + xi, xc + xo, yc - y,  clip_area, color);
            yline(gc, xc + y,  yc - xo, yc - xi, clip_area, color);
        }

        y++;

        if(erro < 0)
        {
            erro += 2 * y + 1;
        }
        else
        {
            xo--;
            erro += 2 * (y - xo + 1);
        }

        if(y > inner)
        {
            xi = y;
        }
        else
        {
            if(erri < 0)
            {
                erri += 2 * y + 1;
            }
            else
            {
                xi--;
                erri += 2 * (y - xi + 1);
            }
        }
    }
}


void gc_circle_filled_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                              int xc, int yc, int radius, uint32_t color)
{
    int x, y;
    int hh, rx, ph;
    int radius_sqr = radius * radius;

    struct clipping_t tmp_clipping;
    struct clipping_t *clipping;

    Rect *clip_area;
    Rect screen_area;
    RectList clip_rects;

    if(gc->pixel_width == 1)
    {
        color = to_rgb8(gc, color);
    }
    else if(gc->pixel_width == 2)
    {
        color = to_rgb16(gc, color);
    }
    else if(gc->pixel_width == 3)
    {
        color = to_rgb24(gc, color);
    }
    else
    {
        color = to_rgb32(gc, color);
    }

    if(__clipping && __clipping->clip_rects && __clipping->clip_rects->root)
    {
        clipping = __clipping;
    }
    else
    {
        clipping = &tmp_clipping;
        tmp_clipping.clipping_on = 0;
        tmp_clipping.clip_rects = &clip_rects;

        if(!__clipping || !__clipping->clipping_on)
        {
            clip_rects.root = &screen_area;
            screen_area.top = 0;
            screen_area.left = 0;
            screen_area.bottom = gc->h - 1;
            screen_area.right = gc->w - 1;
            screen_area.next = NULL;
        }
        else
        {
            clip_rects.root = NULL;
        }
    }
    
    for(x = -radius; x < radius; x++)
    {
        hh = sqrt(radius_sqr - x * x);
        rx = xc + x;
        ph = yc + hh;
        
        for(y = yc - hh; y < ph; y++)
        {
            for(clip_area = clipping->clip_rects->root;
                clip_area != NULL;
                clip_area = clip_area->next)
            {
                pixel(gc, rx, y, clip_area, color);
            }
        }
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
    gc_circle_clipped(gc, &gc->clipping, xc, yc, radius, thickness, color);
}


void gc_circle_filled(struct gc_t *gc, int xc, int yc,
                                       int radius, uint32_t color)
{
    gc_circle_filled_clipped(gc, &gc->clipping, xc, yc, radius, color);
}

