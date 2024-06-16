/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gc-arc.c
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
 *  \file gc-arc.c
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
extern void xline(struct gc_t *gc, int x1, int x2, int y, 
                  Rect *clip_area, uint32_t color);
extern void yline(struct gc_t *gc, int x, int y1, int y2, 
                  Rect *clip_area, uint32_t color);
extern void pixel(struct gc_t *gc, int x, int y, 
                  Rect *clip_area, uint32_t color);

/*
 * Below code is adopted from the post by 'bkht' on:
 *    https://github.com/lvgl/lvgl/issues/252
 */

int fast_atan2(int x, int y)
{
    // Fast XY vector to integer degree algorithm - Jan 2011 www.RomanBlack.com
    // Converts any XY values including 0 to a degree value that should be
    // within +/- 1 degree of the accurate value without needing
    // large slow trig functions like ArcTan() or ArcCos().
    // NOTE! at least one of the X or Y values must be non-zero!
    // This is the full version, for all 4 quadrants and will generate
    // the angle in integer degrees from 0-360.
    // Any values of X and Y are usable including negative values provided
    // they are between -1456 and 1456 so the 16bit multiply does not overflow.

    unsigned char negflag;
    unsigned char tempdegree;
    unsigned char comp;
    unsigned int degree;     // this will hold the result
    unsigned int ux;
    unsigned int uy;

    // Save the sign flags then remove signs and get XY as unsigned ints
    negflag = 0;

    if(x < 0)
    {
        negflag += 0x01;    // x flag bit
        x = (0 - x);        // is now +
    }

    ux = x;                // copy to unsigned var before multiply

    if(y < 0)
    {
        negflag += 0x02;    // y flag bit
        y = (0 - y);        // is now +
    }

    uy = y;                // copy to unsigned var before multiply

    // 1. Calc the scaled "degrees"
    if(ux > uy)
    {
        degree = (uy * 45) / ux;   // degree result will be 0-45 range
        negflag += 0x10;    // octant flag bit
    }
    else
    {
        degree = (ux * 45) / uy;   // degree result will be 0-45 range
    }

    // 2. Compensate for the 4 degree error curve
    comp = 0;
    tempdegree = degree;    // use an unsigned char for speed!

    if(tempdegree > 22)      // if top half of range
    {
        if(tempdegree <= 44) comp++;
        if(tempdegree <= 41) comp++;
        if(tempdegree <= 37) comp++;
        if(tempdegree <= 32) comp++;  // max is 4 degrees compensated
    }
    else    // else is lower half of range
    {
        if(tempdegree >= 2) comp++;
        if(tempdegree >= 6) comp++;
        if(tempdegree >= 10) comp++;
        if(tempdegree >= 15) comp++;  // max is 4 degrees compensated
    }

    degree += comp;   // degree is now accurate to +/- 1 degree!

    // Invert degree if it was X>Y octant, makes 0-45 into 90-45
    if(negflag & 0x10) degree = (90 - degree);

    // 3. Degree is now 0-90 range for this quadrant,
    // need to invert it for whichever quadrant it was in
    if(negflag & 0x02)   // if -Y
    {
        if(negflag & 0x01)   // if -Y -X
            degree = (180 + degree);
        else        // else is -Y +X
            degree = (180 - degree);
    }
    else    // else is +Y
    {
        if(negflag & 0x01)   // if +Y -X
            degree = (360 - degree);
    }

    return degree;
}


#define __ITERATOR_PREFIX       \
    for(clip_area = clipping->clip_rects->root; \
        clip_area != NULL;  \
        clip_area = clip_area->next)


/*
 * NOTE: angle1 and angle2 are given in degrees.
 */
void gc_arc_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                    int xc, int yc, int radius, int angle1, int angle2,
                    int thickness, uint32_t color)
{
    thickness++;
    radius += (thickness / 2);
    
    int radius2 = radius - thickness;
    int deg = fast_atan2(-radius, 0);
    int x, y;

    struct clipping_t tmp_clipping;
    struct clipping_t *clipping;

    Rect *clip_area;
    Rect screen_area;
    RectList clip_rects;

    if(gc->pixel_width == 2)
    {
        color = to_rgb16(gc, color);
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

#define __XLINE(x1, x2, y)  \
    __ITERATOR_PREFIX \
    {   \
        xline(gc, (x1), (x2), (y), clip_area, color);   \
    }

    if((deg >= angle1) && (deg <= angle2))
    {
        // Left Middle
        __XLINE(xc - radius + 1, xc - radius + 1 + thickness, yc);
    }

    deg = fast_atan2(radius2, 0);
    
    if((deg >= angle1) && (deg <= angle2))
    {
        // Right Middle
        __XLINE(xc + radius2, xc + radius2 + thickness, yc);
    }

#undef __XLINE

#define __YLINE(x, y1, y2)  \
    __ITERATOR_PREFIX \
    {   \
        yline(gc, (x), (y1), (y2), clip_area, color);   \
    }

    deg = fast_atan2(0, -radius);

    if((deg >= angle1) && (deg <= angle2))
    {
        // Top Middle
        __YLINE(xc, yc - radius + 1, yc - radius + 1 + thickness);
    }

    deg = fast_atan2(0, radius2);

    if((deg >= angle1) && (deg <= angle2))
    {
        // Bottom middle
        __YLINE(xc, yc + radius2, yc + radius2 + thickness);
    }

#undef __YLINE

#define __PIXEL(x, y)   \
    __ITERATOR_PREFIX \
    {   \
        pixel(gc, (x), (y), clip_area, color);  \
    }

    int radius_sqr = radius * radius;
    int radius2_sqr = radius2 * radius2;

    for(y = -radius; y < 0; y++)
    {
        for(x = -radius; x < 0; x++)
        {
            uint32_t r2 = x * x + y * y;

            if((r2 <= radius_sqr) && (r2 >= radius2_sqr))
            {
                deg = fast_atan2(x, y);

                if ((deg >= angle1) && (deg <= angle2))
                {
                    __PIXEL(xc + x, yc + y);
                }

                deg = fast_atan2(x, -y);

                if ((deg >= angle1) && (deg <= angle2))
                {
                    __PIXEL(xc + x, yc - y);
                }

                deg = fast_atan2(-x, y);

                if ((deg >= angle1) && (deg <= angle2))
                {
                    __PIXEL(xc - x, yc + y);
                }

                deg = fast_atan2(-x, -y);

                if ((deg >= angle1) && (deg <= angle2))
                {
                    __PIXEL(xc - x, yc - y);
                }
            }
        }
    }
}

#undef __PIXEL
#undef __ITERATOR_PREFIX


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_arc(struct gc_t *gc, int xc, int yc,
                             int radius, int angle1, int angle2,
                             int thickness, uint32_t color)
{
    gc_arc_clipped(gc, &gc->clipping, xc, yc, radius, angle1, angle2,
                                      thickness, color);
}

