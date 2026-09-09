/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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
 *  - gc-round-rect.c: functions to draw round edge rectangles (hollow and filled),
 *  - gc-ttf.c: functions to draw text using TrueType Fonts (TTF),
 *  - gc-gc.c: functions to convert pixels between different gc formats,
 *  - gc-grad-vert.c: functions to draw vertical gradients,
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "../include/gui.h"
#include "../include/gc.h"
#include "../include/rgb.h"

#include "gc-inlines.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

typedef struct { float x, y; } Point2D;


// Helper function: Calculate shortest distance from a pixel point to a line segment
static float distance_to_segment(float px, float py, Point2D p1, Point2D p2)
{
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;

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

    float nearest_x = p1.x + t * dx;
    float nearest_y = p1.y + t * dy;

    return sqrtf((px - nearest_x) * (px - nearest_x) + (py - nearest_y) * (py - nearest_y));
}


// Helper function: Check if an angle falls within the specified arc span
static int is_angle_in_arc(float x, float y, float angle1, float angle2)
{
    // We use standard screen-coordinate space inversion (Y goes down)
    // atan2f(-y, x) normalises angles counter-clockwise
    float angle = atan2f(-y, x); 

    if(angle < 0.0f)
    {
        angle += 2.0f * M_PI;
    }
    
    float deg = angle * (180.0f / M_PI);
    float end_angle = angle1 + angle2;
    
    // Normalize target angles to 0-360 window
    if(angle1 < 0.0f)
    {
        angle1 += 360.0f;
        end_angle += 360.0f;
    }

    while(deg < 0.0f)
    {
        deg += 360.0f;
    }

    while(deg >= 360.0f)
    {
        deg -= 360.0f;
    }

    while(angle1 >= 360.0f)
    {
        angle1 -= 360.0f;
        end_angle -= 360.0f;
    }

    if(end_angle > angle1)
    {
        return (deg >= angle1 && deg <= end_angle);
    }
    else        // Handle cross-zero wrapping boundaries
    {
        return (deg >= angle1 || deg <= end_angle);
    }
}


static void __gc_arc_clipped(struct gc_t *gc, Rect *clip_area,
                             int cxi, int cyi, int rxi, int ryi, 
                             int angle1, int angle2, int thickness, 
                             uint32_t color, int filled)
{
    float cx = (float)cxi;
    float cy = (float)cyi;
    float rx = (float)rxi;
    float ry = (float)ryi;
    float rx4 = rx * rx * rx * rx;
    float ry4 = ry * ry * ry * ry;

    // Define the bounding region parameters
    int start_x = (int)(cx - rx - thickness - 1.0f);
    int end_x   = (int)(cx + rx + thickness + 1.0f);
    int start_y = (int)(cy - ry - thickness - 1.0f);
    int end_y   = (int)(cy + ry + thickness + 1.0f);
    int x, y;

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

    // Cache line segments for filled pie-wedges to minimize math calculations inside loops
    Point2D center = { cx, cy };
    Point2D edge_start, edge_end;

    // Remove alpha
    color &= 0xffffff00;

    if(filled)
    {
        float rad_start = angle1 * (M_PI / 180.0f);
        float rad_end = (angle1 + angle2) * (M_PI / 180.0f);

        edge_start.x = cx + rx * cosf(rad_start);
        edge_start.y = cy - ry * sinf(rad_start); // Negative due to screen coordinate inversion
        edge_end.x   = cx + rx * cosf(rad_end);
        edge_end.y   = cy - ry * sinf(rad_end);
    }

    for(y = start_y; y <= end_y; y++)
    {
        for(x = start_x; x <= end_x; x++)
        {
            if(x < clip_area->left || x > clip_area->right)
            {
                continue;
            }

            if(y < clip_area->top || y > clip_area->bottom)
            {
                continue;
            }

            float dx = (float)x + 0.5f - cx;
            float dy = (float)y + 0.5f - cy;

            // Algebraic distance value (Elliptical Vector Space mapping)
            float norm_x = dx / rx;
            float norm_y = dy / ry;
            float elliptic_radius = sqrtf(norm_x * norm_x + norm_y * norm_y);

            // Determine dynamic subpixel distance adjustments using delta approximations
            float pixel_value_delta = sqrtf((dx * dx) / (rx4) + (dy * dy) / (ry4));
            float distance_in_pixels = (elliptic_radius - 1.0f) / pixel_value_delta;

            int in_arc = is_angle_in_arc(dx, dy, angle1, angle2);
            int alpha = 0;

            if(filled)      // Filled Arc
            {
                if(in_arc)
                {
                    if(distance_in_pixels <= -0.5f)
                    {
                        alpha = 255; // Solid inside the ellipse sector
                    }
                    else if (distance_in_pixels >= 0.5f)
                    {
                        alpha = 0;   // Outside the ellipse boundary
                    }
                    else    // Antialiased Outer Curve Edge
                    {
                        alpha = (int)((0.5f - distance_in_pixels) * 255);
                    }
                }
                else
                {
                    // straight lines from the edge back to center (pie slice)
                    float px = (float)x + 0.5f;
                    float py = (float)y + 0.5f;
                    float d_start = distance_to_segment(px, py, center, edge_start);
                    float d_end   = distance_to_segment(px, py, center, edge_end);
                    float min_line_d = (d_start < d_end) ? d_start : d_end;
                    
                    // Render straight cuts if we are geometrically within the ellipse area
                    if(distance_in_pixels < 0.5f && min_line_d < 0.5f)
                    {
                        alpha = (int)((0.5f - min_line_d) * 255);
                    }
                }
            }
            else        // Hollow Stroke outline
            {
                if(!in_arc)
                {
                    continue; 
                }

                float half_w = thickness * 0.5f;
                float dist_from_stroke = fabsf(distance_in_pixels) - half_w;

                if(dist_from_stroke <= -0.5f)
                {
                    alpha = 255;
                }
                else if(dist_from_stroke >= 0.5f)
                {
                    alpha = 0;
                }
                else
                {
                    alpha = (int)((0.5f - dist_from_stroke) * 255);
                }
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
}


static void gc_arc_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                           int xc, int yc, int xr, int yr, 
                           int angle1, int angle2, int thickness, 
                           uint32_t color, int filled)
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
        __gc_arc_clipped(gc, clip_area, xc, yc, xr, yr, angle1, angle2, thickness, color, filled);
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_arc(struct gc_t *gc, int xc, int yc,
                             int xr, int yr, int angle1, int angle2,
                             int thickness, uint32_t color)
{
    gc_arc_clipped(gc, &gc->clipping, xc, yc, xr, yr, angle1, angle2, thickness, color, 0);
}


void gc_arc_filled(struct gc_t *gc, int xc, int yc,
                             int xr, int yr, int angle1, int angle2,
                             uint32_t color)
{
    gc_arc_clipped(gc, &gc->clipping, xc, yc, xr, yr, angle1, angle2, 1, color, 1);
}

