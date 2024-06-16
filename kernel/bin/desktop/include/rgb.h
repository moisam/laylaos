/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: rgb.h
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
 *  \file rgb.h
 *
 *  Inlined functions to work with RGB colors. We need different functions
 *  to deal with 32, 24, 16 and 8 bit colors.
 */

#ifndef MIN
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#endif

/*
 * Helper function to convert an RGBA color to 32bit representation.
 */
static inline uint32_t to_rgb32(struct gc_t *gc, uint32_t color)
{
    uint32_t r = ((color >> 24) & 0xff);
    uint32_t g = ((color >> 16) & 0xff);
    uint32_t b = ((color >> 8) & 0xff);

    return (r << gc->screen->red_pos) |
           (g << gc->screen->green_pos) |
           (b << gc->screen->blue_pos);
}


static inline uint32_t gc_comp_to_rgb32(struct gc_t *gc, uint32_t r,
                                        uint32_t g, uint32_t b)
{
    return (r << gc->screen->red_pos) |
           (g << gc->screen->green_pos) |
           (b << gc->screen->blue_pos);
}


static inline uint32_t gc_red_component32(struct gc_t *gc, uint32_t color)
{
    return (color >> gc->screen->red_pos) & 0xff;
}


static inline uint32_t gc_green_component32(struct gc_t *gc, uint32_t color)
{
    return (color >> gc->screen->green_pos) & 0xff;
}


static inline uint32_t gc_blue_component32(struct gc_t *gc, uint32_t color)
{
    return (color >> gc->screen->blue_pos) & 0xff;
}


/*
 * Helper function to convert an RGBA color to 24bit representation.
 */
static inline uint32_t to_rgb24(struct gc_t *gc, uint32_t color)
{
    uint32_t r = ((color >> 24) & 0xff);
    uint32_t g = ((color >> 16) & 0xff);
    uint32_t b = ((color >> 8) & 0xff);

    return (r << gc->screen->red_pos) |
           (g << gc->screen->green_pos) |
           (b << gc->screen->blue_pos);
}


static inline uint32_t gc_comp_to_rgb24(struct gc_t *gc, uint32_t r,
                                        uint32_t g, uint32_t b)
{
    return (r << gc->screen->red_pos) |
           (g << gc->screen->green_pos) |
           (b << gc->screen->blue_pos);
}


static inline uint32_t gc_red_component24(struct gc_t *gc, uint32_t color)
{
    return (color >> gc->screen->red_pos) & 0xff;
}


static inline uint32_t gc_green_component24(struct gc_t *gc, uint32_t color)
{
    return (color >> gc->screen->green_pos) & 0xff;
}


static inline uint32_t gc_blue_component24(struct gc_t *gc, uint32_t color)
{
    return (color >> gc->screen->blue_pos) & 0xff;
}


/*
 * Helper function to convert an RGBA color to 16bit representation.
 */
static inline uint16_t to_rgb16(struct gc_t *gc, uint32_t color)
{
    uint16_t r = ((color >> 24) & 0xff);// >> 3;
    uint16_t g = ((color >> 16) & 0xff);// >> 3;
    uint16_t b = ((color >> 8) & 0xff);// >> 3;
    //uint16_t a = ((color >> 0) & 0xff) >> 7;

    r = (r * ((1 << gc->screen->red_mask_size) - 1)) / 0xff;
    g = (g * ((1 << gc->screen->green_mask_size) - 1)) / 0xff;
    b = (b * ((1 << gc->screen->blue_mask_size) - 1)) / 0xff;

    return (r << gc->screen->red_pos) |
           (g << gc->screen->green_pos) |
           (b << gc->screen->blue_pos);
}


static inline uint16_t gc_comp_to_rgb16(struct gc_t *gc, uint32_t r,
                                        uint32_t g, uint32_t b)
{
    r = (r * ((1 << gc->screen->red_mask_size) - 1)) / 0xff;
    g = (g * ((1 << gc->screen->green_mask_size) - 1)) / 0xff;
    b = (b * ((1 << gc->screen->blue_mask_size) - 1)) / 0xff;

    return (r << gc->screen->red_pos) |
           (g << gc->screen->green_pos) |
           (b << gc->screen->blue_pos);
}


static inline uint16_t gc_red_component16(struct gc_t *gc, uint16_t color)
{
    uint16_t r, m;

    m = ((1 << gc->screen->red_mask_size) - 1);
    r = (((color >> gc->screen->red_pos) & m) * 0xff) / m;

    return r;
}


static inline uint16_t gc_green_component16(struct gc_t *gc, uint16_t color)
{
    uint16_t g, m;

    m = ((1 << gc->screen->green_mask_size) - 1);
    g = (((color >> gc->screen->green_pos) & m) * 0xff) / m;

    return g;
}


static inline uint16_t gc_blue_component16(struct gc_t *gc, uint16_t color)
{
    uint16_t b, m;

    m = ((1 << gc->screen->blue_mask_size) - 1);
    b = (((color >> gc->screen->blue_pos) & m) * 0xff) / m;

    return b;
}


/*
 * Helper function to convert an RGBA color to 8bit representation (i.e.
 * palette-indexed).
 *
 * NOTE: the color approximation done by this function is not good.
 * TODO: fix this!
 */
static inline uint8_t gc_comp_to_rgb8(struct gc_t *gc, uint32_t r,
                                      uint32_t g, uint32_t b)
{
    uint8_t res = 0;
    unsigned i, distance, best_distance = 4 * 256 * 256;
    struct rgba_color_t *palette = gc->screen->palette;

    for (i = 0; i < gc->screen->color_count; i++)
    {
        distance = (b - palette[i].blue) * (b - palette[i].blue) +
                   (r - palette[i].red) * (r - palette[i].red) +
                   (g - palette[i].green) * (g - palette[i].green);

        if(distance < best_distance)
        {
            res = i;
            best_distance = distance;
        }
    }

    return res;
}


static inline uint8_t to_rgb8(struct gc_t *gc, uint32_t color)
{
    uint32_t r = ((color >> 24) & 0xff);
    uint32_t g = ((color >> 16) & 0xff);
    uint32_t b = ((color >> 8) & 0xff);

    return gc_comp_to_rgb8(gc, r, g, b);
}


static inline uint8_t gc_red_component8(struct gc_t *gc, uint8_t color)
{
    return gc->screen->palette[color].red;
}


static inline uint8_t gc_green_component8(struct gc_t *gc, uint8_t color)
{
    return gc->screen->palette[color].green;
}


static inline uint8_t gc_blue_component8(struct gc_t *gc, uint8_t color)
{
    return gc->screen->palette[color].blue;
}


/*
 * Helper function to make an RGB color more bright.
 */
static inline uint32_t brighten(uint32_t color)
{
    uint32_t r = ((color >> 24) & 0xff);
    uint32_t g = ((color >> 16) & 0xff);
    uint32_t b = ((color >> 8) & 0xff);

    r = MIN(255, r * 1.5);
    g = MIN(255, g * 1.5);
    b = MIN(255, b * 1.5);

    return (r << 24) | (g << 16) | (b << 8);
}


/*
 * See: https://stackoverflow.com/questions/726549/algorithm-for-additive-color-mixing-for-rgb-values
 */
static inline uint32_t highlight(uint32_t color, uint32_t hir,
                                 uint32_t hig, uint32_t hib)
{
    uint32_t r = ((color >> 24) & 0xff);
    uint32_t g = ((color >> 16) & 0xff);
    uint32_t b = ((color >> 8 ) & 0xff);
    uint32_t a = ((color      ) & 0xff);

    r = (r + hir) / 2;
    g = (g + hig) / 2;
    b = (b + hib) / 2;

    return (r << 24) | (g << 16) | (b << 8) | a;
}


/*
 * Alpha blending using the alpha channel of c1.
 * See: https://www.virtualdub.org/blog2/entry_117.html
 */
static inline uint32_t alpha_blend32(struct gc_t *gc, uint32_t c1, uint32_t c2)
{
    uint32_t rbsrc, rbdst, gsrc, gdst, tmp, tmp2;
    uint32_t alpha = (c1 & 0xff);

    rbsrc = (c1 & 0xff00ff00) >> 8;
    rbdst = (gc_red_component32(gc, c2) << 16) |
            (gc_blue_component32(gc, c2));
    gsrc  = (c1 & 0x00ff0000) >> 16;
    gdst  = gc_green_component32(gc, c2);
    tmp = (rbdst + (((rbsrc - rbdst) * alpha + 0x800080) >> 8)) & 0xff00ff;
    tmp2 = (gdst + (((gsrc - gdst) * alpha + 0x80) >> 8)) & 0xff;

    return to_rgb32(gc, (tmp << 8) | (tmp2 << 16) | 0xff);
}


static inline uint32_t alpha_blend24(struct gc_t *gc, uint32_t c1, uint32_t c2)
{
    uint32_t rbsrc, rbdst, gsrc, gdst, tmp, tmp2;
    uint32_t alpha = (c1 & 0xff);

    rbsrc = (c1 & 0xff00ff00) >> 8;
    rbdst = (gc_red_component24(gc, c2) << 16) |
            (gc_blue_component24(gc, c2));
    gsrc  = (c1 & 0x00ff0000) >> 16;
    gdst  = gc_green_component24(gc, c2);
    tmp = (rbdst + (((rbsrc - rbdst) * alpha + 0x800080) >> 8)) & 0xff00ff;
    tmp2 = (gdst + (((gsrc - gdst) * alpha + 0x80) >> 8)) & 0xff;

    return to_rgb24(gc, (tmp << 8) | (tmp2 << 16) | 0xff);
}


static inline uint16_t alpha_blend16(struct gc_t *gc, uint32_t c1, uint16_t c2)
{
    uint32_t rbsrc, rbdst, gsrc, gdst, tmp, tmp2;
    uint32_t alpha = (c1 & 0xff);

    rbsrc = (c1 & 0xff00ff00) >> 8;
    rbdst = (gc_red_component16(gc, c2) << 16) |
            (gc_blue_component16(gc, c2));
    gsrc  = (c1 & 0x00ff0000) >> 16;
    gdst  = gc_green_component16(gc, c2);
    tmp = (rbdst + (((rbsrc - rbdst) * alpha + 0x800080) >> 8)) & 0xff00ff;
    tmp2 = (gdst + (((gsrc - gdst) * alpha + 0x80) >> 8)) & 0xff;

    return to_rgb16(gc, (tmp << 8) | (tmp2 << 16) | 0xff);
}


static inline uint8_t alpha_blend8(struct gc_t *gc, uint32_t c1, uint8_t c2)
{
    uint32_t rbsrc, rbdst, gsrc, gdst, tmp, tmp2;
    uint32_t alpha = (c1 & 0xff);

    rbsrc = (c1 & 0xff00ff00) >> 8;
    rbdst = (gc_red_component8(gc, c2) << 16) |
            (gc_blue_component8(gc, c2));
    gsrc  = (c1 & 0x00ff0000) >> 16;
    gdst  = gc_green_component8(gc, c2);
    tmp = (rbdst + (((rbsrc - rbdst) * alpha + 0x800080) >> 8)) & 0xff00ff;
    tmp2 = (gdst + (((gsrc - gdst) * alpha + 0x80) >> 8)) & 0xff;

    return to_rgb8(gc, (tmp << 8) | (tmp2 << 16) | 0xff);
}

