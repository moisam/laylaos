/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: listnode.h
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
 *  \file listnode.h
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

#ifndef LISTNODE_H
#define LISTNODE_H

#include <stdlib.h>
#include "mutex.h"
#include "listnode-struct.h"


#ifdef __cplusplus
extern "C" {
#endif

// defined in common/listnode.c
void prep_listnode_cache(void);

extern volatile ListNode *listnode_cache;
extern mutex_t cache_lock;


// Basic listnode constructor
static inline ListNode* ListNode_new(void* payload)
{
    ListNode* list_node;

    if(listnode_cache)
    {
        list_node = (ListNode *)listnode_cache;
        listnode_cache = listnode_cache->cache_next;
    }
    // Malloc and/or fail null
    else if(!(list_node = (ListNode*)malloc(sizeof(ListNode))))
    {
        return list_node;
    }

    // Assign initial properties
    list_node->prev = (ListNode*)0;
    list_node->next = (ListNode*)0;
    list_node->cache_next = (ListNode*)0;
    list_node->payload = payload; 

    return list_node;
}

static inline ListNode* ListNode_new_unlocked(void* payload)
{
    ListNode* list_node;

    if(listnode_cache)
    {
        list_node = (ListNode *)listnode_cache;
        listnode_cache = listnode_cache->cache_next;
    }
    // Malloc and/or fail null
    else
    {
        if(!(list_node = (ListNode*)malloc(sizeof(ListNode))))
        {
            return list_node;
        }
    }

    // Assign initial properties
    list_node->prev = (ListNode*)0;
    list_node->next = (ListNode*)0;
    list_node->cache_next = (ListNode*)0;
    list_node->payload = payload; 

    return list_node;
}

static inline void Listnode_free(ListNode *listnode)
{
    listnode->cache_next = (ListNode *)listnode_cache;
    listnode_cache = listnode;
}

static inline void Listnode_free_unlocked(ListNode *listnode)
{
    listnode->cache_next = (ListNode *)listnode_cache;
    listnode_cache = listnode;
}

#ifdef __cplusplus
}
#endif

#endif //LISTNODE_H
