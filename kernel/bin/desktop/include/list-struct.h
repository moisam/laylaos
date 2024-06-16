/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: list-struct.h
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
 *  \file list-struct.h
 *
 *  A general linked list implementation. The code is divided into 3 files:
 *  - common/list.c: initialises the list cache,
 *  - include/list.h: inlined list creation and manipulation functions,
 *  - include/list-struct.h: list struct definition.
 *
 *  This code is based on the "Windowing Systems by Example" blog series.
 *  The source code it released under the MIT license and can be found at:
 *  https://github.com/JMarlin/wsbe.
 */

#ifndef LIST_STRUCT_H
#define LIST_STRUCT_H

#include "rect-struct.h"
#include "listnode-struct.h"

// A type to encapsulate a basic dynamic list
typedef struct List_struct
{
    /* unsigned */ int count; 
    ListNode *root_node, *last_node;
    struct List_struct *next;
} List;


typedef struct RectList_struct
{
    //int count; 
    Rect *root, *last;
    struct RectList_struct *next;
} RectList;

#endif      /* LIST_STRUCT_H */
