/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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
 *  - gc-round-rect.c: functions to draw round edge rectangles (hollow and filled),
 *  - gc-ttf.c: functions to draw text using TrueType Fonts (TTF),
 *  - gc-gc.c: functions to convert pixels between different gc formats,
 *  - gc-grad-vert.c: functions to draw vertical gradients,
 */

#include "../include/gui.h"
#include "../include/gc.h"
#include "../include/rgb.h"

#include "gc-inlines.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct { float x, y; } Point2D;


static float distance_to_segment(float px, float py, Point2D p1, Point2D p2)
{
    float dx = p2.x - p1.x; float dy = p2.y - p1.y;

    if(dx == 0.0f && dy == 0.0f)
    {
        return sqrtf((px - p1.x) * (px - p1.x) + (py - p1.y) * (py - p1.y));
    }

    float t = ((px - p1.x) * dx + (py - p1.y) * dy) / (dx * dx + dy * dy);

    if(t < 0.0f)
    {
        t = 0.0f;
    }
    else if(t > 1.0f)
    {
        t = 1.0f;
    }

    return sqrtf((px - (p1.x + t * dx)) * (px - (p1.x + t * dx)) + 
                 (py - (p1.y + t * dy)) * (py - (p1.y + t * dy)));
}


static int point_in_polygon(float x, float y, Point2D *pts, int count)
{
    int i, j, inside = 0;

    for(i = 0, j = count - 1; i < count; j = i++)
    {
        if(((pts[i].y > y) != (pts[j].y > y)) &&
            (x < (pts[j].x - pts[i].x) * (y - pts[i].y) / (pts[j].y - pts[i].y) + pts[i].x))
        {
            inside = !inside;
        }
    }

    return inside;
}


static void __gc_poly_clipped(struct gc_t *gc, Rect *clip_area,
                              int *vertices, int nvertex,
                              int thickness, uint32_t color, int ispolygon, int filled)
{
    int x, y, i, j;

    if(nvertex < 2)
    {
        return;
    }

    // Convert to floating point structures and find spatial bounding box
    Point2D *pts = (Point2D *)malloc(sizeof(Point2D) * nvertex);

    float min_x = vertices[0], max_x = vertices[0];
    float min_y = vertices[1], max_y = vertices[1];

    for(i = 0, j = 0; i < nvertex; i++, j += 2)
    {
        pts[i].x = (float)vertices[j];
        pts[i].y = (float)vertices[j + 1];

        if(pts[i].x < min_x)
        {
            min_x = pts[i].x;
        }

        if(pts[i].x > max_x)
        {
            max_x = pts[i].x;
        }

        if(pts[i].y < min_y)
        {
            min_y = pts[i].y;
        }

        if(pts[i].y > max_y)
        {
            max_y = pts[i].y;
        }
    }

    int start_x = (int)(min_x - thickness - 1);
    int end_x   = (int)(max_x + thickness + 1);
    int start_y = (int)(min_y - thickness - 1);
    int end_y   = (int)(max_y + thickness + 1);

    if(start_x < 0)
    {
        start_x = 0;
    }

    if(end_x >= gc->w)
    {
        end_x = gc->w - 1;
    }

    if(start_y < 0)
    {
        start_y = 0;
    }

    if(end_y >= gc->h)
    {
        end_y = gc->h - 1;
    }

    for(y = start_y; y <= end_y; y++)
    {
        for(x = start_x; x <= end_x; x++)
        {
            float px = (float)x + 0.5f; float py = (float)y + 0.5f;
            float min_dist = 1e9f;

            // Compute distance to segments
            for(i = 0; i < nvertex - 1; i++)
            {
                float d = distance_to_segment(px, py, pts[i], pts[i+1]);

                if(d < min_dist)
                {
                    min_dist = d;
                }
            }

            if(ispolygon)   // Connect last point back to start
            {
                float d = distance_to_segment(px, py, pts[nvertex - 1], pts[0]);

                if(d < min_dist)
                {
                    min_dist = d;
                }
            }

            int alpha = 0;

            if(ispolygon && filled)
            {
                if(point_in_polygon(px, py, pts, nvertex))
                {
                    alpha = (min_dist >= 0.5f) ? 255 : (int)((min_dist + 0.5f) * 255);
                }
                else
                {
                    alpha = (min_dist >= 0.5f) ? 0 : (int)((0.5f - min_dist) * 255);
                }
            }
            else        // Hollow Stroke
            {
                float half_w = thickness * 0.5f;
                float dist_from_stroke = fabsf(min_dist - half_w);
                alpha = (dist_from_stroke >= 0.5f) ? 0 : (int)((0.5f - dist_from_stroke) * 255);
            }

            if(alpha > 0)
            {
                if(alpha > 255)
                {
                    alpha = 255;
                }

                __fill_pixel(gc, y, x, color, alpha);
            }
        }
    }

    free(pts);
}


static void gc_poly_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                            int *vertices, int nvertex,
                            int thickness, uint32_t color, int ispolygon, int filled)
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
        __gc_poly_clipped(gc, clip_area, vertices, nvertex, thickness, color, ispolygon, filled);
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_polyline(struct gc_t *gc, int *vertices, int nvertex,
                                  int thickness, uint32_t color)
{
    gc_poly_clipped(gc, &gc->clipping, vertices, nvertex, thickness, color, 0, 0);
}


void gc_polygon(struct gc_t *gc, int *vertices, int nvertex,
                                 int thickness, uint32_t color)
{
    gc_poly_clipped(gc, &gc->clipping, vertices, nvertex, thickness, color, 1, 0);
}


void gc_polygon_filled(struct gc_t *gc, int *vertices, int nvertex, uint32_t color)
{
    gc_poly_clipped(gc, &gc->clipping, vertices, nvertex, 1, color, 1, 1);
}

