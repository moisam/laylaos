/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: gc-gc.c
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
 *  \file gc-gc.c
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
#include "../include/screen.h"


#ifndef __x86_64__

int gc_copy_gc(struct gc_t *gcdest, struct gc_t *gcsrc)
{
    fprintf(stderr, "libgui: gc_copy_gc: function is not implemented on this arch\n");
    return -1;
}

#else       /* !__x86_64__ */

#include <tmmintrin.h>
#include <cpuid.h>
#include <stdbool.h>

typedef void (*copyfunc)(struct gc_t *, struct gc_t *, int, int, size_t, int, int, __m128i);

// forward declarations
#define DECLARE(f)  static void f(struct gc_t *, struct gc_t *, \
                                  int, int, size_t, int, int, __m128i);

DECLARE(copy_4_to_4_ssse3);
DECLARE(copy_4_to_3_ssse3);
DECLARE(copy_3_to_3_ssse3);
DECLARE(copy_3_to_4_ssse3);
DECLARE(copy_4_to_4);
DECLARE(copy_4_to_3);
DECLARE(copy_3_to_3);
DECLARE(copy_3_to_4);

#undef DECLARE

// function pointers
static copyfunc copy44 = copy_4_to_4;
static copyfunc copy43 = copy_4_to_3;
static copyfunc copy33 = copy_3_to_3;
static copyfunc copy34 = copy_3_to_4;

static int __has_ssse3 = 0;
static int __checked_ssse3 = 0;


static inline void check_ssse3(void)
{
    unsigned int eax, ebx, ecx, edx;

    if(__checked_ssse3)
    {
        return;
    }

    if(__get_cpuid(1, &eax, &ebx, &ecx, &edx))
    {
        __has_ssse3 = (ecx & (1 << 9)) != 0; // Bit 9 of ECX is SSSE3
        __checked_ssse3 = 1;

        if(__has_ssse3)
        {
            copy44 = copy_4_to_4_ssse3;
            copy43 = copy_4_to_3_ssse3;
            copy33 = copy_3_to_3_ssse3;
            copy34 = copy_3_to_4_ssse3;
        }
    }
}


int get_screen_color_format(struct screen_t *screen)
{
    if(!screen->rgb_mode)
    {
        return SCREEN_COLOR_FORMAT_UNKNOWN;
    }

    if(screen->pixel_width == 4)
    {
        if(screen->red_pos == 24 && screen->green_pos == 16 && screen->blue_pos == 8)
        {
            return SCREEN_COLOR_FORMAT_RGBA;
        }
        else if(screen->red_pos == 8 && screen->green_pos == 16 && screen->blue_pos == 24)
        {
            return SCREEN_COLOR_FORMAT_BGRA;
        }
        else if(screen->red_pos == 16 && screen->green_pos == 8 && screen->blue_pos == 0)
        {
            return SCREEN_COLOR_FORMAT_ARGB;
        }
        else if(screen->red_pos == 0 && screen->green_pos == 8 && screen->blue_pos == 16)
        {
            return SCREEN_COLOR_FORMAT_ABGR;
        }
    }
    else if(screen->pixel_width == 3)
    {
        if(screen->red_pos == 16 && screen->green_pos == 8 && screen->blue_pos == 0)
        {
            return SCREEN_COLOR_FORMAT_RGB;
        }
        else if(screen->red_pos == 0 && screen->green_pos == 8 && screen->blue_pos == 16)
        {
            return SCREEN_COLOR_FORMAT_BGR;
        }
    }

    return SCREEN_COLOR_FORMAT_UNKNOWN;
}


/*
 * Get the GC's pixel format. This currently handles only the formats
 * listed above. It does not handle 16-bit or 8-bit palette-based formats.
 */
static int get_rgb_format(struct gc_t *gc)
{
    return get_screen_color_format(gc->screen);
}


/*
 * Get the mask we need to shuffle bytes from source to convert pixels to
 * the destination format.
 */
static inline __m128i get_mask(int desttype, int srctype)
{
    switch(srctype)
    {
        case SCREEN_COLOR_FORMAT_RGBA:
            switch(desttype)
            {
                case SCREEN_COLOR_FORMAT_RGBA:       // RGBA[0123] -> RGBA[0123]
                    return _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

                case SCREEN_COLOR_FORMAT_BGRA:       // RGBA[0123] -> BGRA[0321]
                    return _mm_setr_epi8(0, 3, 2, 1, 4, 7, 6, 5, 8, 11, 10, 9, 12, 15, 14, 13);

                case SCREEN_COLOR_FORMAT_ARGB:       // RGBA[0123] -> ARGB[1230]
                    return _mm_setr_epi8(1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12);

                case SCREEN_COLOR_FORMAT_ABGR:       // RGBA[0123] -> ABGR[3210]
                    return _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);

                case SCREEN_COLOR_FORMAT_RGB:        // RGBA[0123] -> RGB[123]
                    return _mm_setr_epi8(1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15, -1, -1, -1, -1);

                case SCREEN_COLOR_FORMAT_BGR:        // RGBA[0123] -> BGR[321]
                    return _mm_setr_epi8(3, 2, 1, 7, 6, 5, 11, 10, 9, 15, 14, 13, -1, -1, -1, -1);
            }
            break;

        case SCREEN_COLOR_FORMAT_BGRA:
            switch(desttype)
            {
                case SCREEN_COLOR_FORMAT_RGBA:       // BGRA[0123] -> RGBA[0321]
                    return _mm_setr_epi8(0, 3, 2, 1, 4, 7, 6, 5, 8, 11, 10, 9, 12, 15, 14, 13);

                case SCREEN_COLOR_FORMAT_BGRA:       // BGRA[0123] -> BGRA[0123]
                    return _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

                case SCREEN_COLOR_FORMAT_ARGB:       // BGRA[0123] -> ARGB[3210]
                    return _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);

                case SCREEN_COLOR_FORMAT_ABGR:       // BGRA[0123] -> ABGR[1230]
                    return _mm_setr_epi8(1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12);

                case SCREEN_COLOR_FORMAT_RGB:        // BGRA[0123] -> RGB[321]
                    return _mm_setr_epi8(3, 2, 1, 7, 6, 5, 11, 10, 9, 15, 14, 13, -1, -1, -1, -1);

                case SCREEN_COLOR_FORMAT_BGR:        // BGRA[0123] -> BGR[123]
                    return _mm_setr_epi8(1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15, -1, -1, -1, -1);
            }
            break;

        case SCREEN_COLOR_FORMAT_ARGB:
            switch(desttype)
            {
                case SCREEN_COLOR_FORMAT_RGBA:       // ARGB[0123] -> RGBA[3012]
                    return _mm_setr_epi8(3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14);

                case SCREEN_COLOR_FORMAT_BGRA:       // ARGB[0123] -> BGRA[3210]
                    return _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);

                case SCREEN_COLOR_FORMAT_ARGB:       // ARGB[0123] -> ARGB[0123]
                    return _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

                case SCREEN_COLOR_FORMAT_ABGR:       // ARGB[0123] -> ABGR[2103]
                    return _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);

                case SCREEN_COLOR_FORMAT_RGB:        // ARGB[0123] -> RGB[012]
                    return _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, -1, -1, -1, -1);

                case SCREEN_COLOR_FORMAT_BGR:        // ARGB[0123] -> BGR[210]
                    return _mm_setr_epi8(2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1, -1);
            }
            break;

        case SCREEN_COLOR_FORMAT_ABGR:
            switch(desttype)
            {
                case SCREEN_COLOR_FORMAT_RGBA:       // ABGR[0123] -> RGBA[3210]
                    return _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);

                case SCREEN_COLOR_FORMAT_BGRA:       // ABGR[0123] -> BGRA[3012]
                    return _mm_setr_epi8(3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14);

                case SCREEN_COLOR_FORMAT_ARGB:       // ABGR[0123] -> ARGB[2103]
                    return _mm_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);

                case SCREEN_COLOR_FORMAT_ABGR:       // ABGR[0123] -> ABGR[0123]
                    return _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

                case SCREEN_COLOR_FORMAT_RGB:        // ABGR[0123] -> RGB[210]
                    return _mm_setr_epi8(2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1, -1);

                case SCREEN_COLOR_FORMAT_BGR:        // ABGR[0123] -> BGR[012]
                    return _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, -1, -1, -1, -1);
            }
            break;

        case SCREEN_COLOR_FORMAT_RGB:
            switch(desttype)
            {
                case SCREEN_COLOR_FORMAT_RGBA:       // RGB[012] -> RGBA[x012]
                    return _mm_setr_epi8(-1, 0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11);

                case SCREEN_COLOR_FORMAT_BGRA:       // RGB[012] -> BGRA[x210]
                    return _mm_setr_epi8(-1, 2, 1, 0, -1, 5, 4, 3, -1, 8, 7, 6, -1, 11, 10, 9);

                case SCREEN_COLOR_FORMAT_ARGB:       // RGB[012] -> ARGB[012x]
                    return _mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1);

                case SCREEN_COLOR_FORMAT_ABGR:       // RGB[012] -> ABGR[210x]
                    return _mm_setr_epi8(2, 1, 0, -1, 5, 4, 3, -1, 8, 7, 6, -1, 11, 10, 9, -1);

                case SCREEN_COLOR_FORMAT_RGB:        // RGB[012] -> RGB[012]
                    return _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1, -1, -1, -1);

                case SCREEN_COLOR_FORMAT_BGR:        // RGB[012] -> BGR[210]
                    return _mm_setr_epi8(2, 1, 0, 5, 4, 3, 8, 7, 6, 11, 10, 9, -1, -1, -1, -1);
            }
            break;

        case SCREEN_COLOR_FORMAT_BGR:
            switch(desttype)
            {
                case SCREEN_COLOR_FORMAT_RGBA:       // BGR[012] -> RGBA[x210]
                    return _mm_setr_epi8(-1, 2, 1, 0, -1, 5, 4, 3, -1, 8, 7, 6, -1, 11, 10, 9);

                case SCREEN_COLOR_FORMAT_BGRA:       // BGR[012] -> BGRA[x012]
                    return _mm_setr_epi8(-1, 0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11);

                case SCREEN_COLOR_FORMAT_ARGB:       // BGR[012] -> ARGB[210x]
                    return _mm_setr_epi8(2, 1, 0, -1, 5, 4, 3, -1, 8, 7, 6, -1, 11, 10, 9, -1);

                case SCREEN_COLOR_FORMAT_ABGR:       // BGR[012] -> ABGR[012x]
                    return _mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1);

                case SCREEN_COLOR_FORMAT_RGB:        // BGR[012] -> RGB[210]
                    return _mm_setr_epi8(2, 1, 0, 5, 4, 3, 8, 7, 6, 11, 10, 9, -1, -1, -1, -1);

                case SCREEN_COLOR_FORMAT_BGR:        // BGR[012] -> BGR[012]
                    return _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1, -1, -1, -1);
            }
            break;
    }

    // XXX: we should not reach here, but return identity format to pacify gcc
    return _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}


static inline int get_alpha_pos(int type)
{
    switch(type)
    {
        case SCREEN_COLOR_FORMAT_RGBA:
        case SCREEN_COLOR_FORMAT_BGRA:
            return 0;

        case SCREEN_COLOR_FORMAT_ARGB:
        case SCREEN_COLOR_FORMAT_ABGR:
            return 24;

        default:
            return 0;
    }
}


/*
 * Both source and destination gc have 4 byte pixels.
 * Copy using SSSE3 instructions.
 * Does not use the flags or src_alpha_pos arguments.
 */
static void copy_4_to_4_ssse3(struct gc_t *gcdest, struct gc_t *gcsrc, 
                              int offsrc, int offdst, size_t sz, 
                              int src_alpha_pos, int flags, __m128i mask)
{
    uint32_t *src = (uint32_t *)(gcsrc->buffer + offsrc);
    uint32_t *dst = (uint32_t *)(gcdest->buffer + offdst);
    size_t i = 0;

    // Copy 4 pixels at a time
    for( ; i <= sz - 4; i += 4)
    {
        __m128i chunk = _mm_loadu_si128((__m128i *)&src[i]);
        __m128i shuffled = _mm_shuffle_epi8(chunk, mask);

        _mm_storeu_si128((__m128i *)&dst[i], shuffled);
    }

    // Clean up remaining pixels (less than 4)
    for( ; i < sz; i++)
    {
        // Convert src pixel to dest pixel format
        uint32_t r = gc_red_component32(gcsrc, src[i]);
        uint32_t g = gc_green_component32(gcsrc, src[i]);
        uint32_t b = gc_blue_component32(gcsrc, src[i]);

        dst[i] = gc_comp_to_rgb32(gcdest, r, g, b);
    }
}


/*
 * Fallback for cpus with no SSSE3 instructions.
 * Does not use the mask argument.
 * Uses the flags and src_alpha_pos arguments to decide if caller wants to
 * copy pixels with zero alpha channel from src to dst.
 */
static void copy_4_to_4(struct gc_t *gcdest, struct gc_t *gcsrc, 
                        int offsrc, int offdst, size_t sz, 
                        int src_alpha_pos, int flags, __m128i mask)
{
    uint32_t *src = (uint32_t *)(gcsrc->buffer + offsrc);
    uint32_t *dst = (uint32_t *)(gcdest->buffer + offdst);
    size_t i = 0;
    int zero_alpha_is_valid = (flags & GC_COPY_FLAG_ZERO_ALPHA_IS_OPAQUE);

    for( ; i < sz; i++)
    {
        uint32_t pixel = src[i];
        uint32_t r = gc_red_component32(gcsrc, pixel);
        uint32_t g = gc_green_component32(gcsrc, pixel);
        uint32_t b = gc_blue_component32(gcsrc, pixel);
        uint32_t a = (pixel >> src_alpha_pos) & 0xff;

        if(a == 0)
        {
            // color that somehow has 0 alpha
            if(pixel != 0 && zero_alpha_is_valid)
            {
                a = 0xff;
            }
            else
            {
                continue;
            }
        }

        // Convert src pixel to dest pixel format
        if(a == 0xff)
        {
            dst[i] = gc_comp_to_rgb32(gcdest, r, g, b);
        }
        else
        {
            uint32_t dest_pixel = dst[i];
            uint32_t dr = gc_red_component32(gcdest, dest_pixel);
            uint32_t dg = gc_green_component32(gcdest, dest_pixel);
            uint32_t db = gc_blue_component32(gcdest, dest_pixel);

            // Fast integer blending math
            uint32_t out_r = ((r * a) + (dr * (255 - a))) / 255;
            uint32_t out_g = ((g * a) + (dg * (255 - a))) / 255;
            uint32_t out_b = ((b * a) + (db * (255 - a))) / 255;

            dst[i] = gc_comp_to_rgb32(gcdest, out_r, out_g, out_b);
        }
    }
}


/*
 * Both source and destination gc have 3 byte pixels.
 * Copy using SSSE3 instructions.
 * Does not use the flags or src_alpha_pos arguments.
 */
static void copy_3_to_3_ssse3(struct gc_t *gcdest, struct gc_t *gcsrc, 
                              int offsrc, int offdst, size_t sz, 
                              int src_alpha_pos, int flags, __m128i mask)
{
    uint8_t *src = (uint8_t *)(gcsrc->buffer + offsrc);
    uint8_t *dst = (uint8_t *)(gcdest->buffer + offdst);
    uint32_t last4;
    size_t i = 0;

    // Copy 4 pixels at a time
    for( ; i <= sz - 4; i += 4)
    {
        __m128i chunk = _mm_loadu_si128((__m128i *)&src[i * 3]);
        __m128i shuffled = _mm_shuffle_epi8(chunk, mask);

        // We only want the first 12 bytes of the 16-bit register
        _mm_storel_epi64((__m128i *)&dst[i * 3], shuffled); // Store first 8 bytes
        _mm_storeu_si32(&last4, _mm_srli_si128(shuffled, 8)); // Store next 4 bytes
        __builtin_memcpy(&dst[i * 3 + 8], &last4, 4);
    }

    // Clean up remaining pixels (less than 4)
    for( ; i < sz; i++)
    {
        // Convert src pixel to dest pixel format
        uint32_t tmp = (uint32_t)src[i * 3] |
                       ((uint32_t)src[i * 3 + 1]) << 8 |
                       ((uint32_t)src[i * 3 + 2]) << 16;

        uint32_t r = gc_red_component24(gcsrc, tmp);
        uint32_t g = gc_green_component24(gcsrc, tmp);
        uint32_t b = gc_blue_component24(gcsrc, tmp);
        uint32_t tmp2 = gc_comp_to_rgb24(gcdest, r, g, b);

        dst[i * 3 + 0] = tmp2 & 0xff;
        dst[i * 3 + 1] = (tmp2 >> 8) & 0xff;
        dst[i * 3 + 2] = (tmp2 >> 16) & 0xff;
    }
}


/*
 * Fallback for cpus with no SSSE3 instructions.
 * Does not use the flags or src_alpha_pos arguments.
 * Does not use the mask argument.
 */
static void copy_3_to_3(struct gc_t *gcdest, struct gc_t *gcsrc, 
                        int offsrc, int offdst, size_t sz, 
                        int src_alpha_pos, int flags, __m128i mask)
{
    uint8_t *src = (uint8_t *)(gcsrc->buffer + offsrc);
    uint8_t *dst = (uint8_t *)(gcdest->buffer + offdst);
    size_t i = 0;

    for( ; i < sz; i++)
    {
        // Convert src pixel to dest pixel format
        uint32_t tmp = (uint32_t)src[i * 3] |
                       ((uint32_t)src[i * 3 + 1]) << 8 |
                       ((uint32_t)src[i * 3 + 2]) << 16;

        uint32_t r = gc_red_component24(gcsrc, tmp);
        uint32_t g = gc_green_component24(gcsrc, tmp);
        uint32_t b = gc_blue_component24(gcsrc, tmp);
        uint32_t tmp2 = gc_comp_to_rgb24(gcdest, r, g, b);

        dst[i * 3 + 0] = tmp2 & 0xff;
        dst[i * 3 + 1] = (tmp2 >> 8) & 0xff;
        dst[i * 3 + 2] = (tmp2 >> 16) & 0xff;
    }
}


/*
 * Source gc has 4 byte pixels, destination gc has 3 byte pixels.
 * Copy using SSSE3 instructions.
 * Does not use the flags or src_alpha_pos arguments.
 */
static void copy_4_to_3_ssse3(struct gc_t *gcdest, struct gc_t *gcsrc, 
                              int offsrc, int offdst, size_t sz, 
                              int src_alpha_pos, int flags, __m128i mask)
{
    uint32_t *src = (uint32_t *)(gcsrc->buffer + offsrc);
    uint8_t *dst = (uint8_t *)(gcdest->buffer + offdst);
    uint32_t last4;
    size_t i = 0;

    // Copy 4 pixels at a time
    for( ; i <= sz - 4; i += 4)
    {
        __m128i chunk = _mm_loadu_si128((__m128i *)&src[i]);
        __m128i shuffled = _mm_shuffle_epi8(chunk, mask);

        // We only want the first 12 bytes of the 16-bit register
        _mm_storel_epi64((__m128i *)&dst[i * 3], shuffled); // Store first 8 bytes
        _mm_storeu_si32(&last4, _mm_srli_si128(shuffled, 8)); // Store next 4 bytes
        __builtin_memcpy(&dst[i * 3 + 8], &last4, 4);
    }

    // Clean up remaining pixels (less than 4)
    for( ; i < sz; i++)
    {
        // Convert src pixel to dest pixel format
        uint32_t r = gc_red_component32(gcsrc, src[i]);
        uint32_t g = gc_green_component32(gcsrc, src[i]);
        uint32_t b = gc_blue_component32(gcsrc, src[i]);
        uint32_t tmp2 = gc_comp_to_rgb24(gcdest, r, g, b);

        dst[i * 3 + 0] = tmp2 & 0xff;
        dst[i * 3 + 1] = (tmp2 >> 8) & 0xff;
        dst[i * 3 + 2] = (tmp2 >> 16) & 0xff;
    }
}


/*
 * Fallback for cpus with no SSSE3 instructions.
 * Does not use the mask argument.
 * Uses the flags and src_alpha_pos arguments to decide if caller wants to
 * copy pixels with zero alpha channel from src to dst.
 */
static void copy_4_to_3(struct gc_t *gcdest, struct gc_t *gcsrc, 
                        int offsrc, int offdst, size_t sz, 
                        int src_alpha_pos, int flags, __m128i mask)
{
    uint32_t *src = (uint32_t *)(gcsrc->buffer + offsrc);
    uint8_t *dst = (uint8_t *)(gcdest->buffer + offdst);
    size_t i = 0;
    int zero_alpha_is_valid = (flags & GC_COPY_FLAG_ZERO_ALPHA_IS_OPAQUE);

    for( ; i < sz; i++)
    {
        uint32_t pixel = src[i];
        uint32_t r = gc_red_component32(gcsrc, pixel);
        uint32_t g = gc_green_component32(gcsrc, pixel);
        uint32_t b = gc_blue_component32(gcsrc, pixel);
        uint32_t a = (pixel >> src_alpha_pos) & 0xff;

        if(a == 0)
        {
            // color that somehow has 0 alpha
            if(pixel != 0 && zero_alpha_is_valid)
            {
                a = 0xff;
            }
            else
            {
                continue;
            }
        }

        // Convert src pixel to dest pixel format
        if(a == 0xff)
        {
            uint32_t tmp2 = gc_comp_to_rgb24(gcdest, r, g, b);

            dst[i * 3 + 0] = tmp2 & 0xff;
            dst[i * 3 + 1] = (tmp2 >> 8) & 0xff;
            dst[i * 3 + 2] = (tmp2 >> 16) & 0xff;
        }
        else
        {
            uint32_t dest_pixel = (uint32_t)dst[i * 3] |
                                  ((uint32_t)dst[i * 3 + 1]) << 8 |
                                  ((uint32_t)dst[i * 3 + 2]) << 16;

            uint32_t dr = gc_red_component24(gcdest, dest_pixel);
            uint32_t dg = gc_green_component24(gcdest, dest_pixel);
            uint32_t db = gc_blue_component24(gcdest, dest_pixel);

            uint32_t out_r = ((r * a) + (dr * (255 - a))) / 255;
            uint32_t out_g = ((g * a) + (dg * (255 - a))) / 255;
            uint32_t out_b = ((b * a) + (db * (255 - a))) / 255;

            uint32_t tmp2 = gc_comp_to_rgb24(gcdest, out_r, out_g, out_b);

            dst[i * 3 + 0] = tmp2 & 0xff;
            dst[i * 3 + 1] = (tmp2 >> 8) & 0xff;
            dst[i * 3 + 2] = (tmp2 >> 16) & 0xff;
        }
    }
}


/*
 * Source gc has 3 byte pixels, destination gc has 4 byte pixels.
 * Copy using SSSE3 instructions.
 * Does not use the flags or src_alpha_pos arguments.
 */
static void copy_3_to_4_ssse3(struct gc_t *gcdest, struct gc_t *gcsrc, 
                              int offsrc, int offdst, size_t sz, 
                              int src_alpha_pos, int flags, __m128i mask)
{
    uint8_t *src = (uint8_t *)(gcsrc->buffer + offsrc);
    uint32_t *dst = (uint32_t *)(gcdest->buffer + offdst);
    size_t i = 0;

    // Copy 4 pixels at a time
    for( ; i <= sz - 4; i += 4)
    {
        __m128i chunk = _mm_loadu_si128((__m128i *)&src[i * 3]);
        __m128i shuffled = _mm_shuffle_epi8(chunk, mask);

        _mm_storeu_si128((__m128i *)&dst[i], shuffled);
    }

    // Clean up remaining pixels (less than 4)
    for( ; i < sz; i++)
    {
        // Convert src pixel to dest pixel format
        uint32_t tmp = (uint32_t)src[i * 3] |
                       ((uint32_t)src[i * 3 + 1]) << 8 |
                       ((uint32_t)src[i * 3 + 2]) << 16;

        uint32_t r = gc_red_component24(gcsrc, tmp);
        uint32_t g = gc_green_component24(gcsrc, tmp);
        uint32_t b = gc_blue_component24(gcsrc, tmp);

        dst[i] = gc_comp_to_rgb32(gcdest, r, g, b);
    }
}


/*
 * Fallback for cpus with no SSSE3 instructions.
 * Does not use the flags or src_alpha_pos arguments.
 * Does not use the mask argument.
 */
static void copy_3_to_4(struct gc_t *gcdest, struct gc_t *gcsrc, 
                        int offsrc, int offdst, size_t sz, 
                        int src_alpha_pos, int flags, __m128i mask)
{
    uint8_t *src = (uint8_t *)(gcsrc->buffer + offsrc);
    uint32_t *dst = (uint32_t *)(gcdest->buffer + offdst);
    size_t i = 0;

    for( ; i < sz; i++)
    {
        // Convert src pixel to dest pixel format
        uint32_t tmp = (uint32_t)src[i * 3] |
                       ((uint32_t)src[i * 3 + 1]) << 8 |
                       ((uint32_t)src[i * 3 + 2]) << 16;

        uint32_t r = gc_red_component24(gcsrc, tmp);
        uint32_t g = gc_green_component24(gcsrc, tmp);
        uint32_t b = gc_blue_component24(gcsrc, tmp);

        dst[i] = gc_comp_to_rgb32(gcdest, r, g, b);
    }
}


#define ALPHA_FLAGS                 (GC_COPY_FLAG_ZERO_ALPHA_IS_OPAQUE| \
                                     GC_COPY_FLAG_HAS_TRANSPARENCY)

#define CHOOSE_FUNC(sz, flags, f1, f2)  ((sz <= 4) ? f1 : ((flags & ALPHA_FLAGS) ? f1 : f2))


int gc_copy_gc(struct gc_t *gcdest, struct gc_t *gcsrc, int flags)
{
    size_t sz;
    int destfmt, srcfmt;
    __m128i mask;

    if(!gcdest || !gcdest->screen || !gcsrc || !gcsrc->screen)
    {
        fprintf(stderr, "libgui: gc_copy_gc: invalid arguments\n");
        return -1;
    }

    if(gcdest->w != gcsrc->w || gcdest->h != gcsrc->h)
    {
        fprintf(stderr, "libgui: gc_copy_gc: src and dest must have the same width & height\n");
        fprintf(stderr, "libgui: w1 %d, w2 %d, h1 %d, h2 %d\n", gcdest->w, gcsrc->w, gcdest->h, gcsrc->h);
        return -1;
    }

    destfmt = get_rgb_format(gcdest);
    srcfmt = get_rgb_format(gcsrc);

    if(destfmt == SCREEN_COLOR_FORMAT_UNKNOWN || srcfmt == SCREEN_COLOR_FORMAT_UNKNOWN)
    {
        fprintf(stderr, "libgui: gc_copy_gc: unknown gc format\n");
        return -1;
    }

    check_ssse3();
    mask = get_mask(destfmt, srcfmt);
    sz = gcdest->w * gcdest->h;

    if(gcsrc->pixel_width == 4)
    {
        int src_alpha_pos = get_alpha_pos(srcfmt);

        if(gcdest->pixel_width == 4)
        {
            // don't use the SSE3 functions if we need to inspect each pixel to
            // decide if we need to copy those with zero alpha
            copyfunc do_copy44 = CHOOSE_FUNC(sz, flags, copy_4_to_4, copy44);

            do_copy44(gcdest, gcsrc, 0, 0, sz, src_alpha_pos, flags, mask);
        }
        else if(gcdest->pixel_width == 3)
        {
            // don't use the SSE3 functions if we need to inspect each pixel to
            // decide if we need to copy those with zero alpha
            copyfunc do_copy43 = CHOOSE_FUNC(sz, flags, copy_4_to_3, copy43);

            do_copy43(gcdest, gcsrc, 0, 0, sz, src_alpha_pos, flags, mask);
        }
    }
    else if(gcsrc->pixel_width == 3)
    {
        if(gcdest->pixel_width == 4)
        {
            copy34(gcdest, gcsrc, 0, 0, sz, 0, flags, mask);
        }
        else if(gcdest->pixel_width == 3)
        {
            copy33(gcdest, gcsrc, 0, 0, sz, 0, flags, mask);
        }
    }

    return 0;
}


int gc_copy_part_gc(struct gc_t *gcdest, struct gc_t *gcsrc,
                    int dx, int dy, int sx, int sy, int w, int h, int flags)
{
    int i, dyh = dy + h;
    int destfmt, srcfmt;
    int offsrc, offdst;
    __m128i mask;

    if(!gcdest || !gcdest->screen || !gcsrc || !gcsrc->screen)
    {
        fprintf(stderr, "libgui: gc_copy_part_gc: invalid arguments\n");
        return -1;
    }

    destfmt = get_rgb_format(gcdest);
    srcfmt = get_rgb_format(gcsrc);

    if(destfmt == SCREEN_COLOR_FORMAT_UNKNOWN || srcfmt == SCREEN_COLOR_FORMAT_UNKNOWN)
    {
        fprintf(stderr, "libgui: gc_copy_part_gc: unknown gc format\n");
        return -1;
    }

    check_ssse3();
    mask = get_mask(destfmt, srcfmt);
    offsrc = (sy * gcsrc->pitch) + (sx * gcsrc->pixel_width);
    offdst = (dy * gcdest->pitch) + (dx * gcdest->pixel_width);

    if(gcsrc->pixel_width == 4)
    {
        int src_alpha_pos = get_alpha_pos(srcfmt);

        if(gcdest->pixel_width == 4)
        {
            // don't use the SSE3 functions if we need to inspect each pixel to
            // decide if we need to copy those with zero alpha
            copyfunc do_copy44 = CHOOSE_FUNC(w, flags, copy_4_to_4, copy44);

            for(i = dy; i < dyh; i++)
            {
                do_copy44(gcdest, gcsrc, offsrc, offdst, w, src_alpha_pos, flags, mask);
                offsrc += gcsrc->pitch;
                offdst += gcdest->pitch;
            }
        }
        else if(gcdest->pixel_width == 3)
        {
            // don't use the SSE3 functions if we need to inspect each pixel to
            // decide if we need to copy those with zero alpha
            copyfunc do_copy43 = CHOOSE_FUNC(w, flags, copy_4_to_3, copy43);

            for(i = dy; i < dyh; i++)
            {
                do_copy43(gcdest, gcsrc, offsrc, offdst, w, src_alpha_pos, flags, mask);
                offsrc += gcsrc->pitch;
                offdst += gcdest->pitch;
            }
        }
    }
    else if(gcsrc->pixel_width == 3)
    {
        if(gcdest->pixel_width == 4)
        {
            for(i = dy; i < dyh; i++)
            {
                copy34(gcdest, gcsrc, offsrc, offdst, w, 0, flags, mask);
                offsrc += gcsrc->pitch;
                offdst += gcdest->pitch;
            }
        }
        else if(gcdest->pixel_width == 3)
        {
            for(i = dy; i < dyh; i++)
            {
                copy33(gcdest, gcsrc, offsrc, offdst, w, 0, flags, mask);
                offsrc += gcsrc->pitch;
                offdst += gcdest->pitch;
            }
        }
    }

    return 0;
}

#undef ALPHA_FLAGS
#undef CHOOSE_FUNC

#endif      /* !__x86_64__ */
