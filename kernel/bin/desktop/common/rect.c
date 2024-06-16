/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: rect.c
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
 *  \file rect.c
 *
 *  Functions to work with rectangles. The code is divided into 3 files:
 *  - common/rect.c: initialises the rect cache,
 *  - include/rect.h: inlined rect creation and manipulation functions,
 *  - include/rect-struct.h: rect struct definition.
 */

#include <stdlib.h>
#include "../include/rect.h"

volatile Rect *rect_cache = NULL;

#define NRECTS          4096


void prep_rect_cache(void)
{
    int i;
    volatile Rect *rect;
    
    for(i = 0; i < NRECTS; i++)
    {
        if((rect = (Rect *)malloc(sizeof(Rect))))
        {
            rect->next = (Rect *)rect_cache;
            rect_cache = rect;
        }
    }
}

