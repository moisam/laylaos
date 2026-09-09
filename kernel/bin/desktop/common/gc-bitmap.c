/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
 * 
 *    file: gc-bitmap.c
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
 *  \file gc-bitmap.c
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

#include <limits.h>
#include <string.h>
#include "../include/gc.h"
#include "../include/rgb.h"
#include "../include/rgb-128bit.h"

#ifndef ABS
#define ABS(x)      ((x) < 0 ? -(x) : (x))
#endif


/*************************************
 *
 * Helper functions for internal use.
 *
 *************************************/

static inline void blit_bitmap_32(struct gc_t *gc, uint8_t *dest,
                                  uint32_t *src, unsigned int srcw,
                                  int x, int maxx, int y, int maxy,
                                  uint32_t hicolor, int src_alpha_pos)
{
    // our colors are in the RGBA format
    int curx, i;
    uint32_t tmp;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);


    for( ; y < maxy; y++)
    {
        /* volatile */ uint32_t *buf32 = (uint32_t *)dest;

        for(i = 0, curx = x; curx < maxx; i++, curx++)
        {
            // src bitmap pixels can either be RGBA or ARGB
            // convert to RGBA as the blending function expects this format
            tmp = (src_alpha_pos == 0) ? src[i] : ((src[i] << 8) | (src[i] >> 24));
            tmp = hicolor ? highlight(tmp, hir, hig, hib) : tmp;
            buf32[i] = alpha_blend32(gc, tmp, buf32[i]);
        }

        dest += gc->pitch;
        src += srcw;
    }
}

static inline void blit_bitmap_24(struct gc_t *gc, uint8_t *dest,
                                  uint32_t *src, unsigned int srcw,
                                  int x, int maxx, int y, int maxy, 
                                  uint32_t hicolor, int src_alpha_pos)
{
    int curx, i, j;
    uint32_t tmp, tmp2;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);

    for( ; y < maxy; y++)
    {
        /* volatile */ uint8_t *buf8 = (uint8_t *)dest;

        for(i = 0, j = 0, curx = x; curx < maxx; i++, curx++, j += 3)
        {
            // src bitmap pixels can either be RGBA or ARGB
            // convert to RGBA as the blending function expects this format
            tmp = (src_alpha_pos == 0) ? src[i] : ((src[i] << 8) | (src[i] >> 24));
            tmp = hicolor ? highlight(tmp, hir, hig, hib) : tmp;

            tmp2 = (uint32_t)buf8[j] |
                   ((uint32_t)buf8[j + 1]) << 8 |
                   ((uint32_t)buf8[j + 2]) << 16;

            tmp2 = alpha_blend24(gc, tmp, tmp2);

            buf8[j + 0] = tmp2 & 0xff;
            buf8[j + 1] = (tmp2 >> 8) & 0xff;
            buf8[j + 2] = (tmp2 >> 16) & 0xff;
        }

        dest += gc->pitch;
        src += srcw;
    }
}

static inline void blit_bitmap_16(struct gc_t *gc, uint8_t *dest,
                                  uint32_t *src, unsigned int srcw,
                                  int x, int maxx, int y, int maxy, 
                                  uint32_t hicolor, int src_alpha_pos)
{
    int curx, i;
    uint32_t tmp;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);

    for( ; y < maxy; y++)
    {
        /* volatile */ uint16_t *buf16 = (uint16_t *)dest;

        for(i = 0, curx = x; curx < maxx; i++, curx++)
        {
            // src bitmap pixels can either be RGBA or ARGB
            // convert to RGBA as the blending function expects this format
            tmp = (src_alpha_pos == 0) ? src[i] : ((src[i] << 8) | (src[i] >> 24));
            tmp = hicolor ? highlight(tmp, hir, hig, hib) : tmp;
            buf16[i] = alpha_blend16(gc, tmp, buf16[i]);
        }

        dest += gc->pitch;
        src += srcw;
    }
}

static inline void blit_bitmap_8(struct gc_t *gc, uint8_t *dest,
                                 uint32_t *src, unsigned int srcw,
                                 int x, int maxx, int y, int maxy, 
                                 uint32_t hicolor, int src_alpha_pos)
{
    int curx, i;
    uint32_t tmp;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);

    for( ; y < maxy; y++)
    {
        /* volatile */ uint8_t *buf8 = (uint8_t *)dest;

        for(i = 0, curx = x; curx < maxx; i++, curx++)
        {
            // src bitmap pixels can either be RGBA or ARGB
            // convert to RGBA as the blending function expects this format
            tmp = (src_alpha_pos == 0) ? src[i] : ((src[i] << 8) | (src[i] >> 24));
            tmp = hicolor ? highlight(tmp, hir, hig, hib) : tmp;
            buf8[i] = alpha_blend8(gc, tmp, buf8[i]);
        }

        dest += gc->pitch;
        src += srcw;
    }
}


static inline void blit_for_pixel_width(struct gc_t *gc, 
                                        struct bitmap32_t *bitmap,
                                        int dx, int maxdx, int dy, int maxdy,
                                        int offx, int offy, uint32_t hicolor)
{
    uint32_t *src;
    uint8_t *dest;
    int src_alpha_pos = (bitmap->format == BITMAP_FORMAT_ARGB) ? 24 : 0;

    dest = (uint8_t *)(gc->buffer +
                       (dx * gc->pixel_width + dy * gc->pitch));
    src = &bitmap->data[(offy * bitmap->width) + offx];

    if(gc->pixel_width == 1)
    {
        blit_bitmap_8(gc, dest, src, bitmap->width, 
                                dx, maxdx, dy, maxdy, hicolor, src_alpha_pos);
    }
    else if(gc->pixel_width == 2)
    {
        blit_bitmap_16(gc, dest, src, bitmap->width, 
                                dx, maxdx, dy, maxdy, hicolor, src_alpha_pos);
    }
    else if(gc->pixel_width == 3)
    {
        blit_bitmap_24(gc, dest, src, bitmap->width, 
                                dx, maxdx, dy, maxdy, hicolor, src_alpha_pos);
    }
    else
    {
#ifdef __x86_64__
        blit_bitmap_32_128bit(gc, dest, src, bitmap->width, 
                                  dx, maxdx, dy, maxdy, hicolor, src_alpha_pos);
#else
        blit_bitmap_32(gc, dest, src, bitmap->width, 
                                dx, maxdx, dy, maxdy, hicolor, src_alpha_pos);
#endif
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_blit_bitmap_highlighted(struct gc_t *gc, 
                                struct bitmap32_t *bitmap,
                                int destx, int desty,
                                int offsetx, int offsety,
                                unsigned int width, unsigned int height,
                                uint32_t hicolor)
{
    int dx, dy, offx, offy, maxdx, maxdy, w, h;
    
    struct clipping_t *clipping = &gc->clipping;
    
    if(width == 0)
    {
        width = bitmap->width;
    }
    
    if(height == 0)
    {
        height = bitmap->height;
    }
    
    while(destx < 0)
    {
        destx++;
        offsetx++;
        width--;
    }
    
    while(desty < 0)
    {
        desty++;
        offsety++;
        height--;
    }
    
    if(offsetx < 0 || offsetx >= bitmap->width ||
       offsety < 0 || offsety >= bitmap->height ||
       destx >= gc->w || desty >= gc->h ||
       width > bitmap->width || height > bitmap->height)
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
            offx = offsetx + dx - destx;
            offy = offsety + dy - desty;
            w = width - (dx - destx);
            h = height - (dy - desty);
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

            blit_for_pixel_width(gc, bitmap, dx, maxdx, dy, maxdy, 
                                     offx, offy, hicolor);
        }
    }
    else
    {
        if(!clipping->clipping_on)
        {
            maxdx = destx + width;
            maxdy = desty + height;

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

            blit_for_pixel_width(gc, bitmap, destx, maxdx, desty, maxdy, 
                                     offsetx, offsety, hicolor);
        }
    }
}


void gc_blit_bitmap(struct gc_t *gc, struct bitmap32_t *bitmap,
                                     int destx, int desty,
                                     int offsetx, int offsety,
                                     unsigned int width, unsigned int height)
{
    gc_blit_bitmap_highlighted(gc, bitmap, destx, desty,
                                   offsetx, offsety, width, height, 0);
}


void gc_blit_icon_highlighted(struct gc_t *gc, struct bitmap32_array_t *ba,
                                               int destx, int desty,
                                               int offsetx, int offsety,
                                               unsigned int width, 
                                               unsigned int height,
                                               uint32_t hicolor)
{
    int i, j, least_diff, cur_diff;
    unsigned int wh;
    
    /*
     * Currently, we simply compare both width and height until we find a match.
     * TODO: we should be able to find the "best" match, even if the given width
     *       and height are not an "exact" match.
     */
    for(i = 0; i < ba->count; i++)
    {
        if(ba->bitmaps[i].width == width && ba->bitmaps[i].height == height)
        {
            gc_blit_bitmap_highlighted(gc, &ba->bitmaps[i], destx, desty,
                                           offsetx, offsety, width, height,
                                           hicolor);
            return;
        }
    }

    // An exact match was not found. Find the best match.
    least_diff = INT_MAX;
    j = 0;
    wh = width * height;

    for(i = 0; i < ba->count; i++)
    {
        cur_diff = ABS(ba->bitmaps[i].width * ba->bitmaps[i].height - wh);

        if(cur_diff < least_diff)
        {
            j = i;
            least_diff = cur_diff;
        }
    }

    gc_stretch_bitmap_highlighted(gc, &ba->bitmaps[j],
                                      destx, desty,
                                      width, height,
                                      offsetx, offsety,
                                      ba->bitmaps[j].width - offsetx,
                                      ba->bitmaps[j].height - offsety,
                                      hicolor);
}


void gc_blit_icon(struct gc_t *gc, struct bitmap32_array_t *ba,
                                   int destx, int desty,
                                   int offsetx, int offsety,
                                   unsigned int width, unsigned int height)
{
    gc_blit_icon_highlighted(gc, ba, destx, desty,
                                     offsetx, offsety, width, height, 0);
}

