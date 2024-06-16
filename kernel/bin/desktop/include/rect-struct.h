/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: rect-struct.h
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
 *  \file rect-struct.h
 *
 *  Functions to work with rectangles. The code is divided into 3 files:
 *  - common/rect.c: initialises the rect cache,
 *  - include/rect.h: inlined rect creation and manipulation functions,
 *  - include/rect-struct.h: rect struct definition.
 *
 *  This code is based on the "Windowing Systems by Example" blog series.
 *  The source code it released under the MIT license and can be found at:
 *  https://github.com/JMarlin/wsbe.
 */

#ifndef RECT_STRUCT_H
#define RECT_STRUCT_H

typedef struct Rect_struct
{
    int top;
    int left;
    int bottom;
    int right;
    struct Rect_struct *next;
} Rect;

#endif      /* RECT_STRUCT_H */
