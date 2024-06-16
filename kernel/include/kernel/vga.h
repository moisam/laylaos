/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: vga.h
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
 *  \file vga.h
 *
 *  Helper functions and macros for working with VGA devices.
 */

#ifndef __ARCH_I386_VGA_H__
#define __ARCH_I386_VGA_H__

#include <stdint.h>

#define STANDARD_VGA_WIDTH	    80      /**< standard VGA screen width */
#define STANDARD_VGA_HEIGHT	    25      /**< standard VGA screen height */
#define VGA_MEMORY_PHYSICAL     0xB8000 /**< VGA physical buffer address */

#ifdef __x86_64__
# define VGA_MEMORY_VIRTUAL     0xFFFF8000000B8000  /**< VGA virtual buffer 
                                                         address for the 
                                                         64-bit kernel */
#else
# define VGA_MEMORY_VIRTUAL     0xC00B8000          /**< VGA virtual buffer 
                                                         address for the 
                                                         32-bit kernel */
#endif


#define VGA_MEMORY_SIZE(tty)    ((tty)->vga_width * (tty)->vga_height * 2)


/**
 * \enum vga_color
 *
 * Standard VGA colors
 */
enum vga_color
{
    COLOR_BLACK		    = 0,
    COLOR_BLUE		    = 1,
    COLOR_GREEN		    = 2,
    COLOR_CYAN		    = 3,
    COLOR_RED		    = 4,
    COLOR_MAGENTA		= 5,
    COLOR_BROWN		    = 6,
    COLOR_LIGHT_GREY	= 7,
    COLOR_DARK_GREY	    = 8,
    COLOR_LIGHT_BLUE	= 9,
    COLOR_LIGHT_GREEN	= 10,
    COLOR_LIGHT_CYAN	= 11,
    COLOR_LIGHT_RED	    = 12,
    COLOR_LIGHT_MAGENTA	= 13,
    COLOR_LIGHT_BROWN	= 14,
    COLOR_WHITE		    = 15,
};

static inline uint8_t make_color(enum vga_color fg, enum vga_color bg)
{
   return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)c | (uint16_t)color << 8;
}

#define INVERT_COLOR_AT_POS(buf, index)             \
    (buf)[index] = ((buf)[index] & 0x0f00) << 4 |   \
                   ((buf)[index] & 0xf000) >> 4 |   \
                   ((buf)[index] & 0x00ff)

#endif      /* __ARCH_I386_VGA_H__ */
