/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gc-poly.c
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
 *  \file gc-poly.c
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


void gc_polygon_fill_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                             int *vertices, int nvertex, uint32_t color)
{
    int nnodes, i, j;
    float nodeX[255], pixelx, pixely, swap;

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

    // Loop through the rows of the image.
    for(pixely = 0; pixely < gc->h; pixely++)
    {
        // Build a list of nodes.
        nnodes = 0;
        j = nvertex - 1;

#define POLYX(z)    vertices[(z) * 2]
#define POLYY(z)    vertices[(z) * 2 + 1]

        for(i = 0; i < nvertex; i++)
        {
            if((POLYY(i) < pixely && POLYY(j) >= pixely) ||
               (POLYY(j) < pixely && POLYY(i) >= pixely))
            {
                nodeX[nnodes++] = (POLYX(i) + (pixely - POLYY(i)) /
                                  (POLYY(j) - POLYY(i)) *
                                  (POLYX(j) - POLYX(i)));
            }

            j = i;
        }

        // Sort the nodes, via a simple "Bubble" sort.
        i = 0;

        while(i < nnodes - 1)
        {
            if(nodeX[i] > nodeX[i + 1])
            {
                swap = nodeX[i];
                nodeX[i] = nodeX[i + 1];
                nodeX[i + 1] = swap;
                
                if(i)
                {
                    i--;
                }
            }
            else
            {
                i++;
            }
        }

        // Fill the pixels between node pairs.
        for(i = 0; i < nnodes; i += 2)
        {
            if(nodeX[i] >= gc->w)
            {
                break;
            }

            if(nodeX[i + 1] > 0)
            {
                if(nodeX[i] < 0)
                {
                    nodeX[i] = 0;
                }

                if(nodeX[i + 1] > gc->w)
                {
                    nodeX[i + 1] = gc->w;
                }

                for(pixelx = nodeX[i]; pixelx < nodeX[i + 1]; pixelx++)
                {
                    __PIXEL(pixelx, pixely);
                }
            }
        }
    }
}


void gc_polygon_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                        int *vertices, int nvertex,
                        int thickness, uint32_t color)
{
    int i, j;
    
    if(nvertex < 3)
    {
        return;
    }
    
    // This is a very dumb implementation that simply connects vertices
    // with lines. A side effect is that corners have gaps!
    // Ugly but simple and it works.

    for(i = 0; i < nvertex - 1; i++)
    {
        j = i * 2;
        gc_line_clipped(gc, __clipping,
                        vertices[j], vertices[j + 1],
                        vertices[j + 2], vertices[j + 3],
                        thickness, color);
    }
    
    // connect the last vertex to the first if needed
    j = i * 2;

    if(vertices[0] != vertices[j] && vertices[1] != vertices[j + 1])
    {
        gc_line_clipped(gc, __clipping,
                        vertices[j], vertices[j + 1],
                        vertices[0], vertices[1],
                        thickness, color);
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_polygon(struct gc_t *gc, int *vertices, int nvertex,
                                 int thickness, uint32_t color)
{
    gc_polygon_clipped(gc, &gc->clipping, vertices, nvertex,
                                          thickness, color);
}


void gc_polygon_fill(struct gc_t *gc, int *vertices, int nvertex,
                                      uint32_t color)
{
    gc_polygon_fill_clipped(gc, &gc->clipping, vertices, nvertex, color);
}

