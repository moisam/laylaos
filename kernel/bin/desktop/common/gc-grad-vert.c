/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: gc-grad-vert.c
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
 *  \file gc-grad-vert.c
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

#include "../include/memops.h"
#include "../include/gui.h"
#include "../include/gc.h"
#include "../include/rgb.h"


void __gc_vertical_gradient_clipped(struct gc_t *gc, int x, int origy,
                                    int max_x, int max_y,
                                    Rect *clip_area,
                                    uint32_t *colorarr)
{
    int y = origy;

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

    // Draw the rectangle into the framebuffer line-by line
    unsigned where = x * gc->pixel_width + y * gc->pitch;
    uint8_t *buf = (uint8_t *)(gc->buffer + where);

    if(gc->pixel_width == 1)
    {
        for( ; y < max_y; y++)
        {
            memset(buf, to_rgb8(gc, colorarr[y - origy]), max_x - x);
            buf += gc->pitch;
        }
    }
    else if(gc->pixel_width == 2)
    {
        for( ; y < max_y; y++)
        {
            memset16(buf, to_rgb16(gc, colorarr[y - origy]), max_x - x);
            buf += gc->pitch;
        }
    }
    else if(gc->pixel_width == 3)
    {
        // A 16-pixel sequence requires exactly 48 bytes (16 pixels * 3 bytes).
        // We pre-calculate three 16-byte blocks (v0, v1, v2) that match the pattern.
        uint8_t pat[48];

        // We process loops in groups of 16 pixels (48 bytes)
        int width = max_x - x;
        int blocks_of_16 = width / 16;
        int remaining_pixels = width % 16;
        int i;
        uint8_t *p;

        for( ; y < max_y; y++)
        {
            uint32_t color = to_rgb24(gc, colorarr[y - origy]);
            uint8_t b0 = color & 0xff;
            uint8_t b1 = ((color >> 8) & 0xff);
            uint8_t b2 = ((color >> 16) & 0xff);

            for(i = 0; i < 16; i++)
            {
                pat[i * 3 + 0] = b0;
                pat[i * 3 + 1] = b1;
                pat[i * 3 + 2] = b2;
            }

            // Load the pattern blocks into 128-bit SIMD registers
            __m128i v0 = _mm_loadu_si128((__m128i const*)&pat[0]);
            __m128i v1 = _mm_loadu_si128((__m128i const*)&pat[16]);
            __m128i v2 = _mm_loadu_si128((__m128i const*)&pat[32]);

            p = buf;

            if(blocks_of_16 > 0)
            {
                int count = blocks_of_16;

                // Loop unrolling: Blast 48 bytes (16 pixels) per step using unaligned stores
                while(count > 0)
                {
                    _mm_storeu_si128((__m128i*)(p + 0),  v0);
                    _mm_storeu_si128((__m128i*)(p + 16), v1);
                    _mm_storeu_si128((__m128i*)(p + 32), v2);
                    p += 48;
                    count--;
                }
            }

            // Clean up any trailing edge pixels under 16-pixel bounds
            for(i = 0; i < remaining_pixels; i++)
            {
                p[0] = b0;
                p[1] = b1;
                p[2] = b2;
                p += 3;
            }

            buf += gc->pitch;
        }
    }
    else
    {
        for( ; y < max_y; y++)
        {
            fill_line_32(buf, to_rgb32(gc, colorarr[y - origy]), max_x - x);
            buf += gc->pitch;
        }
    }
}


void gc_vertical_gradient_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                  int x, int y,  
                                  unsigned int width, 
                                  unsigned int height,
                                  uint32_t *colorarr)
{
    Rect *clip_area;
    Rect screen_area;

    // If there are clipping rects, draw the rect clipped to
    // each of them. Otherwise, draw unclipped (clipped to the screen)
    if(clipping->clip_rects && clipping->clip_rects->root)
    {
        for(clip_area = clipping->clip_rects->root;
            clip_area != NULL;
            clip_area = clip_area->next)
        {
            __gc_vertical_gradient_clipped(gc, x, y, x + width, y + height, clip_area, colorarr);
        }
    }
    else
    {
        if(!clipping->clipping_on)
        {
            screen_area.top = 0;
            screen_area.left = 0;
            screen_area.bottom = gc->h - 1;
            screen_area.right = gc->w - 1;
            __gc_vertical_gradient_clipped(gc, x, y, x + width, y + height, &screen_area, colorarr);
        }
    }
}


void gc_vertical_gradient_fill_colorarr(uint32_t *colorarr, int count, 
                                        uint32_t color1, uint32_t color2)
{
    int i;
    int r1 = (color1 >> 24) & 0xFF;
    int g1 = (color1 >> 16) & 0xFF;
    int b1 = (color1 >> 8 ) & 0xFF;
    int r2 = (color2 >> 24) & 0xFF;
    int g2 = (color2 >> 16) & 0xFF;
    int b2 = (color2 >> 8 ) & 0xFF;
    int r, g, b;

    for(i = 0; i < count; i++)
    {
        r = r1 + ((r2 - r1) * i) / count;
        g = g1 + ((g2 - g1) * i) / count;
        b = b1 + ((b2 - b1) * i) / count;
        colorarr[i] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_vertical_gradient(struct gc_t *gc, int x, int y,  
                          unsigned int width, unsigned int height,
                          uint32_t color1, uint32_t color2)
{
    uint32_t colorarr[height];

    gc_vertical_gradient_fill_colorarr(colorarr, height, color1, color2);
    gc_vertical_gradient_clipped(gc, &gc->clipping, x, y, width, height, colorarr);
}

