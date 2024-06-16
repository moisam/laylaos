/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gc-bitmap-stretch.c
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
 *  \file gc-bitmap-stretch.c
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

#include <string.h>
#include "../include/gc.h"
#include "../include/rgb.h"


/*************************************
 *
 * Helper functions for internal use.
 *
 *************************************/

static inline void stretch_bitmap_32(struct gc_t *gc, uint8_t *dest,
                                     uint32_t *__src, unsigned int srcw,
                                     int x, int maxx, int y, int maxy,
                                     float src_dx, float src_dy,
                                     uint32_t hicolor)
{

// our colors are in the RGBA format
#define R(c)            ((c >> 24) & 0xff)
#define G(c)            ((c >> 16) & 0xff)
#define B(c)            ((c >> 8) & 0xff)
#define A(c)            ((c) & 0xff)

    int curx, di;
    uint32_t *src = __src;
    float srcy = 0, si;
    //uint32_t compalpha, alpha, r, g, b, tmp;
    uint32_t tmp;
    ////float alphaf, compalphaf;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);


    for( ; y < maxy; y++)
    {
        /* volatile */ uint32_t *buf32 = (uint32_t *)dest;

        for(si = 0, di = 0, curx = x; curx < maxx; di++, curx++, si += src_dx)
        {
            //alpha = A(src[(int)si]);
            
            if(hicolor)
            {
                tmp = highlight(src[(int)si], hir, hig, hib);
            }
            else
            {
                tmp = src[(int)si];
            }

            buf32[di] = alpha_blend32(gc, tmp, buf32[di]);

            /*
            if(alpha == 0)
            {
                // foreground is transparent - nothing to do here
            }
            else if(alpha == 0xff)
            {
                // foreground is opaque - copy to buffer
                buf32[di] = to_rgb32(gc, tmp);
            }
            else
            {
                compalpha = 0x100 - alpha;
                r = ((R(tmp) * alpha) >> 8) + 
                    ((gc_red_component32(gc, buf32[di]) * compalpha) >> 8);
                g = ((G(tmp) * alpha) >> 8) + 
                    ((gc_green_component32(gc, buf32[di]) * compalpha) >> 8);
                b = ((B(tmp) * alpha) >> 8) + 
                    ((gc_blue_component32(gc, buf32[di]) * compalpha) >> 8);

                buf32[di] = gc_comp_to_rgb32(gc, r, g, b);
            }
            */
        }

        dest += gc->pitch;
        srcy += src_dy;
        src = __src + ((int)srcy * srcw);
    }
}


static inline void stretch_bitmap_24(struct gc_t *gc, uint8_t *dest,
                                     uint32_t *__src, unsigned int srcw,
                                     int x, int maxx, int y, int maxy,
                                     float src_dx, float src_dy,
                                     uint32_t hicolor)
{
    int curx, di;
    uint32_t *src = __src;
    float srcy = 0, si;
    //uint32_t compalpha, alpha, r, g, b, tmp, tmp2;
    uint32_t tmp, tmp2;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);

    for( ; y < maxy; y++)
    {
        /* volatile */ uint8_t *buf8 = (uint8_t *)dest;

        for(si = 0, di = 0, curx = x; curx < maxx; di += 3, curx++, si += src_dx)
        {
            //alpha = A(src[(int)si]);
            
            if(hicolor)
            {
                tmp = highlight(src[(int)si], hir, hig, hib);
            }
            else
            {
                tmp = src[(int)si];
            }

            tmp2 = (uint32_t)buf8[di] |
                   ((uint32_t)buf8[di + 1]) << 8 |
                   ((uint32_t)buf8[di + 2]) << 16;
            tmp2 = alpha_blend24(gc, tmp, tmp2);
            buf8[di + 0] = tmp2 & 0xff;
            buf8[di + 1] = (tmp2 >> 8) & 0xff;
            buf8[di + 2] = (tmp2 >> 16) & 0xff;

            /*
            if(alpha == 0)
            {
                // foreground is transparent - nothing to do here
            }
            else if(alpha == 0xff)
            {
                // foreground is opaque - copy to buffer
                tmp = to_rgb24(gc, tmp);
                buf8[di + 0] = tmp & 0xff;
                buf8[di + 1] = (tmp >> 8) & 0xff;
                buf8[di + 2] = (tmp >> 16) & 0xff;
            }
            else
            {
                // partial opacity - compositing is needed
                // for each color component (R, G, B), the formula is:
                //    color = alpha * fg + (1 - alpha) * bg
                compalpha = 0x100 - alpha;
                tmp2 = (uint32_t)buf8[di] |
                       ((uint32_t)buf8[di + 1]) << 8 |
                       ((uint32_t)buf8[di + 2]) << 16;
                r = ((R(tmp) * alpha) >> 8) + 
                    ((gc_red_component24(gc, tmp2) * compalpha) >> 8);
                g = ((G(tmp) * alpha) >> 8) + 
                    ((gc_green_component24(gc, tmp2) * compalpha) >> 8);
                b = ((B(tmp) * alpha) >> 8) + 
                    ((gc_blue_component24(gc, tmp2) * compalpha) >> 8);

                tmp = gc_comp_to_rgb24(gc, r, g, b);
                buf8[di + 0] = tmp & 0xff;
                buf8[di + 1] = (tmp >> 8) & 0xff;
                buf8[di + 2] = (tmp >> 16) & 0xff;
            }
            */
        }

        dest += gc->pitch;
        srcy += src_dy;
        src = __src + ((int)srcy * srcw);
    }
}


static inline void stretch_bitmap_16(struct gc_t *gc, uint8_t *dest,
                                     uint32_t *__src, unsigned int srcw,
                                     int x, int maxx, int y, int maxy,
                                     float src_dx, float src_dy,
                                     uint32_t hicolor)
{
    int curx, di;
    uint32_t *src = __src;
    float srcy = 0, si;
    //uint32_t compalpha, alpha, r, g, b, tmp;
    uint32_t tmp;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);

    for( ; y < maxy; y++)
    {
        /* volatile */ uint16_t *buf16 = (uint16_t *)dest;

        for(si = 0, di = 0, curx = x; curx < maxx; di++, curx++, si += src_dx)
        {
            //alpha = A(src[(int)si]);
            
            if(hicolor)
            {
                tmp = highlight(src[(int)si], hir, hig, hib);
            }
            else
            {
                tmp = src[(int)si];
            }

            buf16[di] = alpha_blend16(gc, tmp, buf16[di]);

            /*
            if(alpha == 0)
            {
                // foreground is transparent - nothing to do here
            }
            else if(alpha == 0xff)
            {
                // foreground is opaque - copy to buffer
                buf16[di] = to_rgb16(gc, tmp);
            }
            else
            {
                // partial opacity - compositing is needed
                // for each color component (R, G, B), the formula is:
                //    color = alpha * fg + (1 - alpha) * bg
                compalpha = 0x100 - alpha;
                r = ((R(tmp) * alpha) >> 8) + 
                    ((gc_red_component16(gc, buf16[di]) * compalpha) >> 8);
                g = ((G(tmp) * alpha) >> 8) + 
                    ((gc_green_component16(gc, buf16[di]) * compalpha) >> 8);
                b = ((B(tmp) * alpha) >> 8) + 
                    ((gc_blue_component16(gc, buf16[di]) * compalpha) >> 8);

                buf16[di] = gc_comp_to_rgb16(gc, r, g, b);
            }
            */
        }

        dest += gc->pitch;
        srcy += src_dy;
        src = __src + ((int)srcy * srcw);
    }
}


static inline void stretch_bitmap_8(struct gc_t *gc, uint8_t *dest,
                                    uint32_t *__src, unsigned int srcw,
                                    int x, int maxx, int y, int maxy,
                                    float src_dx, float src_dy,
                                    uint32_t hicolor)
{
    int curx, di;
    uint32_t *src = __src;
    float srcy = 0, si;
    //uint32_t compalpha, alpha, r, g, b, tmp;
    uint32_t tmp;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);

    for( ; y < maxy; y++)
    {
        /* volatile */ uint8_t *buf8 = (uint8_t *)dest;

        for(si = 0, di = 0, curx = x; curx < maxx; di++, curx++, si += src_dx)
        {
            //alpha = A(src[(int)si]);
            
            if(hicolor)
            {
                tmp = highlight(src[(int)si], hir, hig, hib);
            }
            else
            {
                tmp = src[(int)si];
            }

            buf8[di] = alpha_blend8(gc, tmp, buf8[di]);

            /*
            if(alpha == 0)
            {
                // foreground is transparent - nothing to do here
            }
            else if(alpha == 0xff)
            {
                // foreground is opaque - copy to buffer
                buf8[di] = to_rgb8(gc, tmp);
            }
            else
            {
                // partial opacity - compositing is needed
                // for each color component (R, G, B), the formula is:
                //    color = alpha * fg + (1 - alpha) * bg
                compalpha = 0x100 - alpha;
                r = ((R(tmp) * alpha) >> 8) + 
                    ((gc_red_component8(gc, buf8[di]) * compalpha) >> 8);
                g = ((G(tmp) * alpha) >> 8) + 
                    ((gc_green_component8(gc, buf8[di]) * compalpha) >> 8);
                b = ((B(tmp) * alpha) >> 8) + 
                    ((gc_blue_component8(gc, buf8[di]) * compalpha) >> 8);

                buf8[di] = gc_comp_to_rgb8(gc, r, g, b);
            }
            */
        }

        dest += gc->pitch;
        srcy += src_dy;
        src = __src + ((int)srcy * srcw);
    }
}


static inline void stretch_for_pixel_width(struct gc_t *gc, 
                                           struct bitmap32_t *bitmap,
                                           int dx, int maxdx,
                                           int dy, int maxdy,
                                           float offx, float offy,
                                           float src_dx, float src_dy,
                                           uint32_t hicolor)
{
    uint32_t *src;
    uint8_t *dest;

    dest = (uint8_t *)(gc->buffer +
                       (dx * gc->pixel_width + dy * gc->pitch));
    src = &bitmap->data[((int)offy * bitmap->width) + (int)offx];

    if(gc->pixel_width == 1)
    {
        stretch_bitmap_8(gc, dest, src, bitmap->width, 
                              dx, maxdx, dy, maxdy, src_dx, src_dy, hicolor);
    }
    else if(gc->pixel_width == 2)
    {
        stretch_bitmap_16(gc, dest, src, bitmap->width, 
                              dx, maxdx, dy, maxdy, src_dx, src_dy, hicolor);
    }
    else if(gc->pixel_width == 3)
    {
        stretch_bitmap_24(gc, dest, src, bitmap->width, 
                              dx, maxdx, dy, maxdy, src_dx, src_dy, hicolor);
    }
    else
    {
        stretch_bitmap_32(gc, dest, src, bitmap->width, 
                              dx, maxdx, dy, maxdy, src_dx, src_dy, hicolor);
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_stretch_bitmap_highlighted(struct gc_t *gc, 
                                   struct bitmap32_t *bitmap,
                                   int destx, int desty,
                                   unsigned int destw, unsigned int desth,
                                   int isrcx, int isrcy,
                                   unsigned int isrcw, unsigned int isrch,
                                   uint32_t hicolor)
{
    int dx, dy, maxdx, maxdy, w, h;
    float src_dx, src_dy;
    float srcx, srcy, offx, offy;
    //float srcw, srch;
    
    struct clipping_t *clipping = &gc->clipping;
    
    if(isrcw == 0)
    {
        isrcw = bitmap->width;
    }
    
    if(isrch == 0)
    {
        isrch = bitmap->height;
    }

    if(destw == 0)
    {
        destw = isrcw;
    }

    if(desth == 0)
    {
        desth = isrch;
    }

    srcx = isrcx;
    srcy = isrcy;
    //srcw = isrcw;
    //srch = isrch;
    src_dx = (float)isrcw / (float)destw;
    src_dy = (float)isrch / (float)desth;
    
    while(destx < 0)
    {
        destx++;
        srcx += src_dx;
        //srcw -= src_dx;
        destw--;
    }
    
    while(desty < 0)
    {
        desty++;
        srcy += src_dy;
        //srch -= src_dy;
        desth--;
    }

    if(srcx < 0 || srcx >= bitmap->width ||
       srcy < 0 || srcy >= bitmap->height ||
       destx >= gc->w || desty >= gc->h /* ||
       (srcx + srcw) > bitmap->width || (srcy + srch) > bitmap->height */)
    {
        return;
    }
    
    // If there are clipping rects, draw the rect clipped to
    // each of them. Otherwise, draw unclipped (clipped to the screen)
    if(clipping->clip_rects && clipping->clip_rects->root)
    {
        Rect *clip_area;

        for(clip_area = clipping->clip_rects->root;
            clip_area != NULL;
            clip_area = clip_area->next)
        {
            if(destx > clip_area->right || desty > clip_area->bottom)
            {
                continue;
            }
            
            dx = (destx < clip_area->left) ? clip_area->left : destx;
            dy = (desty < clip_area->top) ? clip_area->top : desty;
            offx = srcx + ((dx - destx) * src_dx);
            offy = srcy + ((dy - desty) * src_dy);
            w = destw - (dx - destx);
            h = desth - (dy - desty);
            maxdx = dx + w;
            maxdy = dy + h;
            
            if(offx >= bitmap->width || offy >= bitmap->height)
            {
                continue;
            }
            
            if(maxdx > clip_area->right + 1)
            {
                maxdx = clip_area->right + 1;
            }
            
            if(maxdy > clip_area->bottom + 1)
            {
                maxdy = clip_area->bottom + 1;
            }
            
            if(w <= 0 || h <= 0 || maxdx <= dx || maxdy <= dy)
            {
                continue;
            }

            stretch_for_pixel_width(gc, bitmap, dx, maxdx, dy, maxdy, 
                                        offx, offy, src_dx, src_dy, hicolor);
        }
    }
    else
    {
        if(!clipping->clipping_on)
        {
            maxdx = destx + destw;
            maxdy = desty + desth;

            if(maxdx > gc->w)
            {
                maxdx = gc->w;
            }
            
            if(maxdy > gc->h)
            {
                maxdy = gc->h;
            }

            if(maxdx <= destx || maxdy <= desty)
            {
                return;
            }

            stretch_for_pixel_width(gc, bitmap, destx, maxdx, desty, maxdy, 
                                        srcx, srcy, src_dx, src_dy, hicolor);
        }
    }
}


void gc_stretch_bitmap(struct gc_t *gc, 
                       struct bitmap32_t *bitmap,
                       int destx, int desty,
                       unsigned int destw, unsigned int desth,
                       int isrcx, int isrcy,
                       unsigned int srcw, unsigned int srch)
{
    gc_stretch_bitmap_highlighted(gc, bitmap, destx, desty, destw, desth,
                                      isrcx, isrcy, srcw, srch, 0);
}

