/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: common.h
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
 *  \file common.h
 *
 *  Common (and mostly inlined) functions that do not fit anywhere else.
 */

#ifndef KERNEL_COMMON_H
#define KERNEL_COMMON_H

#include <mm/kheap.h>

STATIC_INLINE void *kernel_strdup(void *p, int len)
{
    char *buf = (char *)kmalloc(len + 1);

    if(buf)
    {
        A_memcpy(buf, p, len);
        buf[len] = '\0';
    }
    
    return buf;
}

#endif      /* KERNEL_COMMON_H */
