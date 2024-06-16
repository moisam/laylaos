/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gc-line.c
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
 *  \file gc-line.c
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

// functions defined in gc-circle.c
extern void pixel(struct gc_t *gc, int x, int y, 
                  Rect *clip_area, uint32_t color);


#define __ITERATOR_PREFIX       \
    for(clip_area = clipping->clip_rects->root; \
        clip_area != NULL;  \
        clip_area = clip_area->next)

#define __PIXEL(x, y)   \
    __ITERATOR_PREFIX \
    {   \
        pixel(gc, (x), (y), clip_area, color);  \
    }


void gc_line_simple_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                            int x1, int y1, int x2, int y2, uint32_t color)
{
    int dx =  abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1; 
    int err = dx + dy, e2;  // error value e_xy

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
 
    for(;;)
    {
        __PIXEL(x1, y1);

        if(x1 == x2 && y1 == y2)
        {
            break;
        }

        e2 = 2 * err;

        if(e2 >= dy)
        {
            err += dy;
            x1 += sx;
        }   // e_xy + e_x > 0
        
        if(e2 <= dx)
        {
            err += dx;
            y1 += sy;
        }   // e_xy + e_y < 0
    }
}


#undef ABS
#define ABS(x)      ((x) < 0 ? -(x) : (x))

#undef MAX
#define MAX(x, y)   ((x) < (y) ? (y) : (x))

void gc_line_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                     int x1, int y1, int x2, int y2,
                     int __thickness, uint32_t color)
{
    struct clipping_t tmp_clipping;
    struct clipping_t *clipping;

    Rect *clip_area;
    Rect screen_area;
    RectList clip_rects;

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

    float thickness = __thickness;

    if(thickness <= 1)
    {
        gc_line_simple_clipped(gc, __clipping, x1, y1, x2, y2, color);
        return;
    }

    int dx = ABS(x2 - x1), sx = x1 < x2 ? 1 : -1; 
    int dy = ABS(y2 - y1), sy = y1 < y2 ? 1 : -1; 
    int err = dx - dy, e2, xx, yy;  /* error value e_xy */
    int alpha;
    float ed = dx + dy == 0 ? 1 : sqrt((float)dx * dx + (float)dy * dy);
    uint32_t color_no_alpha = color & 0xffffff00;    // remove alpha
   
    for(thickness = (thickness + 1) / 2; ; )
    {                                   /* pixel loop */
        alpha = MAX(0, 255 * (ABS(err - dx + dy) / ed - thickness + 1));
        //color = color_no_alpha | alpha;

        if(gc->pixel_width == 1)
        {
            color = to_rgb8(gc, (color_no_alpha | alpha));
        }
        else if(gc->pixel_width == 2)
        {
            color = to_rgb16(gc, (color_no_alpha | alpha));
        }
        else if(gc->pixel_width == 3)
        {
            color = to_rgb24(gc, (color_no_alpha | alpha));
        }
        else
        {
            color = to_rgb32(gc, (color_no_alpha | alpha));
        }

        __PIXEL(x1, y1);

        e2 = err; xx = x1;

        if(2 * e2 >= -dx)
        {                          /* x step */
            for(e2 += dy, yy = y1; 
                e2 < ed * thickness && (y2 != yy || dx > dy); 
                e2 += dx)
            {
                alpha = MAX(0, 255 * (ABS(e2) / ed - thickness + 1));
                //color = color_no_alpha | alpha;

                if(gc->pixel_width == 1)
                {
                    color = to_rgb8(gc, (color_no_alpha | alpha));
                }
                else if(gc->pixel_width == 2)
                {
                    color = to_rgb16(gc, (color_no_alpha | alpha));
                }
                else if(gc->pixel_width == 3)
                {
                    color = to_rgb24(gc, (color_no_alpha | alpha));
                }
                else
                {
                    color = to_rgb32(gc, (color_no_alpha | alpha));
                }

                yy += sy;
                __PIXEL(x1, yy);
            }

            if(x1 == x2)
            {
                break;
            }

            e2 = err; err -= dy; x1 += sx; 
        }

        if(2 * e2 <= dy)
        {                          /* y step */
            for(e2 = dx - e2; 
                e2 < ed * thickness && (x2 != xx || dx < dy); 
                e2 += dy)
            {
                alpha = MAX(0, 255 * (ABS(e2) / ed - thickness + 1));
                //color = color_no_alpha | alpha;

                if(gc->pixel_width == 1)
                {
                    color = to_rgb8(gc, (color_no_alpha | alpha));
                }
                else if(gc->pixel_width == 2)
                {
                    color = to_rgb16(gc, (color_no_alpha | alpha));
                }
                else if(gc->pixel_width == 3)
                {
                    color = to_rgb24(gc, (color_no_alpha | alpha));
                }
                else
                {
                    color = to_rgb32(gc, (color_no_alpha | alpha));
                }

                xx += sx;
                __PIXEL(xx, y1);
            }

            if(y1 == y2)
            {
                break;
            }

            err += dx; y1 += sy; 
        }
    }
}

#undef __PIXEL
#undef __ITERATOR_PREFIX


/*
 * NOTE: this function should be moved to a separate gc-rect.c file, along
 *       with other rect functions from gc.c.
 */
void gc_draw_rect_thick_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                int x, int y, 
                                unsigned int width, unsigned int height,
                                int thickness, uint32_t color)
{
    // top
    gc_line_clipped(gc, clipping, x, y, x + width, y, thickness, color);

    // left
    gc_line_clipped(gc, clipping, x, y, x, y + height, thickness, color);

    // bottom
    gc_line_clipped(gc, clipping, x, y + height, 
                                  x + width, y + height, thickness, color);

    // right
    gc_line_clipped(gc, clipping, x + width, y, 
                                  x + width, y + height, thickness, color);
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_line(struct gc_t *gc, int x1, int y1, int x2, int y2,
                              int __thickness, uint32_t color)
{
    gc_line_clipped(gc, &gc->clipping, x1, y1, x2, y2,
                                       __thickness, color);
}

void gc_draw_rect_thick(struct gc_t *gc, int x, int y, 
                                         unsigned int width,
                                         unsigned int height,
                                         int thickness, uint32_t color)
{
    gc_draw_rect_thick_clipped(gc, &gc->clipping, x, y, width, height,
                                                  thickness, color);
}

