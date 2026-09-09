/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: gc-round-rect.c
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
 *  \file gc-round-rect.c
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

static float get_aa_corner_alpha(int j, int i, float cx, float cy, float aw, float ah, int outer)
{
    float dx = (j + 0.5f - cx);
    float dy = (i + 0.5f - cy);

    // Normalized distance squared
    float dist2 = (dx * dx) / (aw * aw) + (dy * dy) / (ah * ah);
    float dist = sqrtf(dist2);

    if(outer)
    {
        // Outer edge: 1.0 inside, fades to 0.0 outside
        if(dist <= 1.0f)
        {
            float pixel_dist = (1.0f - dist) * aw;
            return (pixel_dist > 1.0f) ? 1.0f : pixel_dist;
        }
        else
        {
            float pixel_dist = (dist - 1.0f) * aw;
            return (pixel_dist > 1.0f) ? 0.0f : (1.0f - pixel_dist);
        }
    }
    else
    {
        // Inner edge: 0.0 inside the hole, fades to 1.0 inside the stroke thickness
        if(dist >= 1.0f)
        {
            float pixel_dist = (dist - 1.0f) * aw;
            return (pixel_dist > 1.0f) ? 1.0f : pixel_dist;
        }
        else
        {
            float pixel_dist = (1.0f - dist) * aw;
            return (pixel_dist > 1.0f) ? 0.0f : (1.0f - pixel_dist);
        }
    }
}


static void __gc_roundrect_clipped(struct gc_t *gc, Rect *clip_area,
                                   int x, int y, int w, int h,
                                   int arcW, int arcH, int thickness, uint32_t color)
{
    float aw_out = arcW / 2.0f;
    float ah_out = arcH / 2.0f;
    float aw_in = aw_out - thickness;
    float ah_in = ah_out - thickness;
    int i, j;

    // Remove alpha
    color &= 0xffffff00;

    // Expand boundaries slightly by 1 pixel to catch anti-aliasing bleeds
    for(i = y - 1; i < y + h + 1; i++)
    {
        for(j = x - 1; j < x + w + 1; j++)
        {
            if(j < clip_area->left || j > clip_area->right)
            {
                continue;
            }

            if(i < clip_area->top || i > clip_area->bottom)
            {
                continue;
            }

            float alpha_out = 1.0f;
            float alpha_in = 1.0f;

            // --- 1. DETERMINE IF PIXEL LIES IN A CORNER REGION
            if(j < x + aw_out && i < y + ah_out)                // Top-Left Corner
            {
                alpha_out = get_aa_corner_alpha(j, i, x + aw_out, y + ah_out, aw_out, ah_out, 1);

                if(aw_in > 0 && ah_in > 0)
                {
                    alpha_in = get_aa_corner_alpha(j, i, x + thickness + aw_in, 
                                                   y + thickness + ah_in, aw_in, ah_in, 0);
                }
            }
            else if(j > x + w - aw_out && i < y + ah_out)       // Top-Right Corner
            {
                alpha_out = get_aa_corner_alpha(j, i, x + w - aw_out, y + ah_out, aw_out, ah_out, 1);

                if(aw_in > 0 && ah_in > 0)
                {
                    alpha_in = get_aa_corner_alpha(j, i, x + w - thickness - aw_in, 
                                                   y + thickness + ah_in, aw_in, ah_in, 0);
                }
            }
            else if(j < x + aw_out && i > y + h - ah_out)       // Bottom-Left Corner
            {
                alpha_out = get_aa_corner_alpha(j, i, x + aw_out, y + h - ah_out, aw_out, ah_out, 1);

                if(aw_in > 0 && ah_in > 0)
                {
                    alpha_in = get_aa_corner_alpha(j, i, x + thickness + aw_in, 
                                                   y + h - thickness - ah_in, aw_in, ah_in, 0);
                }
            }
            else if(j > x + w - aw_out && i > y + h - ah_out)   // Bottom-Right Corner
            {
                alpha_out = get_aa_corner_alpha(j, i, x + w - aw_out, y + h - ah_out, aw_out, ah_out, 1);

                if(aw_in > 0 && ah_in > 0)
                {
                    alpha_in = get_aa_corner_alpha(j, i, x + w - thickness - aw_in, 
                                                   y + h - thickness - ah_in, aw_in, ah_in, 0);
                }
            }
            // --- 2. EVALUATE STRAIGHT EDGES (IF NOT IN A CORNER)
            else
            {
                // Check if pixel is within the straight outer frame limits
                if(j < x || j >= x + w || i < y || i >= y + h)
                {
                    // Out-of-bounds or outer anti-aliasing slope edge
                    if(j < x)
                    {
                        alpha_out = 1.0f - (x - (j + 0.5f));
                    }
                    else if(j >= x + w)
                    {
                        alpha_out = 1.0f - ((j + 0.5f) - (x + w));
                    }

                    if(i < y)
                    {
                        alpha_out = 1.0f - (y - (i + 0.5f));
                    }
                    else if(i >= y + h)
                    {
                        alpha_out = 1.0f - ((i + 0.5f) - (y + h));
                    }
                }

                // Check inner box boundaries explicitly to drop the hollow core
                // A pixel is inside the inner "hole" if it clears the stroke thickness on all walls
                if(j >= x + thickness && j < x + w - thickness &&
                   i >= y + thickness && i < y + h - thickness)
                {
                    // It's inside the center hole area -- calculate inner edge profile fade:
                    float dist_l = (j + 0.5f) - (x + thickness);
                    float dist_r = (x + w - thickness) - (j + 0.5f);
                    float dist_t = (i + 0.5f) - (y + thickness);
                    float dist_b = (y + h - thickness) - (i + 0.5f);

                    float min_dist = dist_l;

                    if(dist_r < min_dist)
                    {
                        min_dist = dist_r;
                    }

                    if(dist_t < min_dist)
                    {
                        min_dist = dist_t;
                    }

                    if(dist_b < min_dist)
                    {
                        min_dist = dist_b;
                    }

                    if(min_dist >= 1.0f)
                    {
                        alpha_in = 0.0f; // Pure center hollow space - skip completely
                    }
                    else if(min_dist > 0.0f)
                    {
                        alpha_in = min_dist; // Inside slope transition line
                    }
                    else
                    {
                        alpha_in = 0.0f;
                    }
                }
            }

            // Combine outer and inner visibility factors
            float final_alpha_f = alpha_out * alpha_in;

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


static void gc_roundrect_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                                 int x, int y, int w, int h, 
                                 int arcW, int arcH, int thickness, uint32_t color)
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
        __gc_roundrect_clipped(gc, clip_area, x, y, w, h, arcW, arcH, thickness, color);
    }
}


static void __gc_roundrect_filled_clipped(struct gc_t *gc, Rect *clip_area,
                                          int x, int y, int w, int h,
                                          int arcW, int arcH, uint32_t color)
{
    float aw = arcW / 2.0f;
    float ah = arcH / 2.0f;
    float invAW2 = 1.0f / (aw * aw);
    float invAH2 = 1.0f / (ah * ah);
    float cx, cy;
    int i, j, in_corner;

    // Remove alpha
    color &= 0xffffff00;

    for(i = y; i < y + h; i++)
    {
        for(j = x; j < x + w; j++)
        {
            if(j < clip_area->left || j > clip_area->right)
            {
                continue;
            }

            if(i < clip_area->top || i > clip_area->bottom)
            {
                continue;
            }

            in_corner = 0;

            // Identify if we are in one of the 4 corners
            if(j < x + aw && i < y + ah)                // top left
            {
                cx = x + aw;
                cy = y + ah;
                in_corner = 1;
            }
            else if(j > x + w - aw && i < y + ah)       // top right
            {
                cx = x + w - aw;
                cy = y + ah;
                in_corner = 1;
            }
            else if(j < x + aw && i > y + h - ah)       // bottom left
            {
                cx = x + aw;
                cy = y + h - ah;
                in_corner = 1;
            }
            else if(j > x + w - aw && i > y + h - ah)   // bottom right
            {
                cx = x + w - aw;
                cy = y + h - ah;
                in_corner = 1;
            }

            if(in_corner)
            {
                float dx = (j + 0.5f - cx);
                float dy = (i + 0.5f - cy);
                float dx2 = dx * dx;
                float dy2 = dy * dy;
                float dist = dx2 * invAW2 + dy2 * invAH2;

                if(dist <= 1.0f)
                {
                    // Inside or on the edge of the corner arc
                    // Simple antialias: check the "steepness" of the exit
                    float edge_dist = 1.0f - dist;
                    float width_at_angle = sqrtf(dx2 + dy2);
                    float alpha_f = edge_dist * width_at_angle * 2.0f; // Softness factor
                    uint8_t alpha = (alpha_f > 1.0f) ? 255 : (uint8_t)(alpha_f * 255);

                    __fill_pixel(gc, i, j, color, alpha);
                }
            }
            else
            {
                __fill_pixel(gc, i, j, color, 255);
            }
        }
    }
}


static void gc_roundrect_filled_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                                        int x, int y, int w, int h, 
                                        int arcW, int arcH, uint32_t color)
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
        __gc_roundrect_filled_clipped(gc, clip_area, x, y, w, h, arcW, arcH, color);
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_roundrect_filled(struct gc_t *gc, int x, int y, int w, int h, 
                                          int arcW, int arcH, uint32_t color)
{
    gc_roundrect_filled_clipped(gc, &gc->clipping, x, y, w, h, arcW, arcH, color);
}


void gc_roundrect(struct gc_t *gc, int x, int y, int w, int h, 
                                   int arcW, int arcH, int thickness, uint32_t color)
{
    gc_roundrect_clipped(gc, &gc->clipping, x, y, w, h, arcW, arcH, thickness, color);
}

