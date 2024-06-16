/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: listnode-struct.h
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
 *  \file listnode-struct.h
 *
 *  Functions to work with a linked list item. The code is divided into 3 files:
 *  - common/listnode.c: initialises the listnode cache,
 *  - include/listnode.h: inlined listnode creation and manipulation functions,
 *  - include/listnode-struct.h: listnode struct definition.
 *
 *  This code is based on the "Windowing Systems by Example" blog series.
 *  The source code it released under the MIT license and can be found at:
 *  https://github.com/JMarlin/wsbe.
 */

#ifndef LISTNODE_STRUCT_H
#define LISTNODE_STRUCT_H

// A type to encapsulate an individual item in a linked list
typedef struct ListNode_struct
{
    void* payload;
    struct ListNode_struct* prev;
    struct ListNode_struct* next;
    struct ListNode_struct* cache_next;
} ListNode;

#endif      /* LISTNODE_STRUCT_H */
