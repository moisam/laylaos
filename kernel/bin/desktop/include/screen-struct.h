/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: screen-struct.h
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
 *  \file screen-struct.h
 *
 *  Definition of the screen structure.
 */

#ifndef GUI_SCREEN_STRUCT_H
#define GUI_SCREEN_STRUCT_H

#ifndef __RGBA_COLOR_STRUCT_DEFINED__
#define __RGBA_COLOR_STRUCT_DEFINED__   1

struct rgba_color_t
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} __attribute__((packed));

#endif      /* __RGBA_COLOR_STRUCT_DEFINED__ */


struct screen_t
{
    // general fields
    uint16_t w, h;
    uint8_t pixel_width;

    // fields used in rgb mode
    uint8_t red_pos, green_pos, blue_pos;
    uint8_t red_mask_size, green_mask_size, blue_mask_size;

    // fields used in palette-indexed mode
    uint8_t color_count;
    struct rgba_color_t *palette;

    int rgb_mode;   // 0 = palette-indexed, 1 = rgb
};

#endif      /* GUI_SCREEN_STRUCT_H */
