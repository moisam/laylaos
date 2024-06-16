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
static inline uint32_t to_rgb32(uint32_t color)
{
    uint32_t r = ((color >> 24) & 0xff);
    uint32_t g = ((color >> 16) & 0xff);
    uint32_t b = ((color >> 8) & 0xff);

    return (r << vbe_framebuffer.red_pos) |
           (g << vbe_framebuffer.green_pos) |
           (b << vbe_framebuffer.blue_pos);
}


/*
 * Helper function to convert an RGBA color to 24bit representation.
 */
static inline uint32_t to_rgb24(uint32_t color)
{
    uint32_t r = ((color >> 24) & 0xff);
    uint32_t g = ((color >> 16) & 0xff);
    uint32_t b = ((color >> 8) & 0xff);

    return (r << vbe_framebuffer.red_pos) |
           (g << vbe_framebuffer.green_pos) |
           (b << vbe_framebuffer.blue_pos);
}


/*
 * Helper function to convert an RGBA color to 16bit representation.
 */
static inline uint16_t to_rgb16(uint32_t color)
{
    uint16_t r = ((color >> 24) & 0xff);// >> 3;
    uint16_t g = ((color >> 16) & 0xff);// >> 3;
    uint16_t b = ((color >> 8) & 0xff);// >> 3;

    r = (r * ((1 << vbe_framebuffer.red_mask_size) - 1)) / 0xff;
    g = (g * ((1 << vbe_framebuffer.green_mask_size) - 1)) / 0xff;
    b = (b * ((1 << vbe_framebuffer.blue_mask_size) - 1)) / 0xff;

    return (r << vbe_framebuffer.red_pos) |
           (g << vbe_framebuffer.green_pos) |
           (b << vbe_framebuffer.blue_pos);
}


/*
 * Helper function to convert an RGBA color to 8bit representation (i.e.
 * palette-indexed).
 *
 * NOTE: the color approximation done by this function is not good.
 * TODO: fix this!
 */
static inline uint8_t to_rgb8(uint32_t color)
{
    uint32_t r = ((color >> 24) & 0xff);
    uint32_t g = ((color >> 16) & 0xff);
    uint32_t b = ((color >> 8) & 0xff);
    uint8_t res = 0;
    unsigned i, distance, best_distance = 4 * 256 * 256;
    struct rgba_color_t *palette = vbe_framebuffer.palette_virt_addr;

    for (i = 0; i < vbe_framebuffer.palette_num_colors; i++)
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

