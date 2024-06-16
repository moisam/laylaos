/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: listnode.c
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
 *  \file listnode.c
 *
 *  Functions to work with a linked list item. The code is divided into 3 files:
 *  - common/listnode.c: initialises the listnode cache,
 *  - include/listnode.h: inlined listnode creation and manipulation functions,
 *  - include/listnode-struct.h: listnode struct definition.
 */

#include <inttypes.h>
#include <stdlib.h>
#include "../include/listnode.h"

volatile ListNode *listnode_cache = NULL;

#define NLISTNODES      4096


void prep_listnode_cache(void)
{
    int i;
    volatile ListNode *list_node;
    
    for(i = 0; i < NLISTNODES; i++)
    {
        if((list_node = (ListNode *)malloc(sizeof(ListNode))))
        {
            list_node->cache_next = (ListNode *)listnode_cache;
            listnode_cache = list_node;
        }
    }
}

