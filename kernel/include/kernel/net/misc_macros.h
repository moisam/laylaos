/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: misc_macros.h
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
 *  \file misc_macros.h
 *
 *  Miscellaneous macros used by the network layer.
 */

#ifndef NET_MISC_MACROS_H
#define NET_MISC_MACROS_H

#include <stdint.h>

#if BYTE_ORDER == LITTLE_ENDIAN

static inline uint16_t ptr_to_short(uint8_t *p, int i)
{
    return (uint16_t)p[i] | ((uint16_t)p[i + 1] << 8);
}

static inline uint32_t ptr_to_long(uint8_t *p, int i)
{
    return (uint32_t)p[i] | ((uint32_t)p[i + 1] << 8) | 
           ((uint32_t)p[i + 2] << 16) | ((uint32_t)p[i + 3] << 24);
}

#elif BYTE_ORDER == BIG_ENDIAN

static inline uint16_t ptr_to_short(uint8_t *p, int i)
{
    return (uint16_t)p[i + 1] | ((uint16_t)p[i] << 8);
}

static inline uint32_t ptr_to_long(uint8_t *p, int i)
{
    return (uint32_t)p[i + 3] | ((uint32_t)p[i + 2] << 8) | 
           ((uint32_t)p[i + 1] << 16) | ((uint32_t)p[i] << 24);
}

#else
# error Unknown byte order
#endif

#endif      /* NET_MISC_MACROS_H */
