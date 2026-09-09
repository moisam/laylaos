/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: cursor-struct-alloc.h
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
 *  \file cursor-struct-alloc.h
 *
 *  Functions to alloc and free cursor structures.
 */

#ifndef CURSOR_STRUCT_ALLOC_H
#define CURSOR_STRUCT_ALLOC_H

#include <string.h>

static inline struct cursor_t *cursor_struct_alloc(int count)
{
    struct cursor_t *ba;
    size_t sz = sizeof(struct cursor_t) + 
                        ((count - 1) * sizeof(struct cursor_bitmap_t));

    if((ba = (struct cursor_t *)malloc(sz)))
    {
        memset(ba, 0, sz);
        ba->count = count;
    }
    
    return ba;
}

static inline void cursor_struct_free(struct cursor_t *ba)
{
    int i;
    
    for(i = 0; i < ba->count; i++)
    {
        if(ba->bitmaps[i].data)
        {
            free(ba->bitmaps[i].data);
        }
    }
    
    free(ba);
}

#endif      /* CURSOR_STRUCT_ALLOC_H */
