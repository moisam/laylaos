/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: list.c
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
 *  \file list.c
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

#include <inttypes.h>
#include <stdlib.h>
#include "../include/list.h"

volatile List *list_cache = NULL;
volatile RectList *rectlist_cache = NULL;

#define NLISTS          2048
#define NRECTLISTS      2048

mutex_t cache_lock = MUTEX_INITIALIZER;


void prep_list_cache(void)
{
    int i;
    volatile List *list;
    volatile RectList *rlist;
    
    for(i = 0; i < NLISTS; i++)
    {
        if((list = malloc(sizeof(List))))
        {
            list->next = (List *)list_cache;
            list_cache = list;
        }
    }

    for(i = 0; i < NRECTLISTS; i++)
    {
        if((rlist = malloc(sizeof(RectList))))
        {
            rlist->next = (RectList *)rectlist_cache;
            rectlist_cache = rlist;
        }
    }
}


// Remove the item at the specified index from the list and return the item 
// that was removed.
void *List_remove_at(List *list, int index)
{
    void *payload; 

    if(list->count == 0 || index >= list->count) 
    {
        return (void*)0;
    }

    // Iterate through the items
    ListNode *current_node = list->root_node;

    for(int current_index = 0;
        (current_index < index) && current_node;
        current_index++)
    {
        current_node = current_node->next;
    }

    if(!current_node)
    {
        return (void*)0;
    }

    // Stash the payload so we don't lose it when we delete the node
    payload = current_node->payload;
 
    // Re-point neighbors to each other 
    if(current_node->prev)
    {
        current_node->prev->next = current_node->next;
    }

    if(current_node->next)
    {
        current_node->next->prev = current_node->prev;
    }

    // If the item was the root item, we need to make the node following it
    // the new root
    if(index == 0)
    {
        list->root_node = current_node->next;
    }
    
    // do the same for the last node
    if(current_node == list->last_node)
    {
        list->last_node = current_node->prev;
    }

    // Now that we've clipped the node out of the list, free its memory
    Listnode_free(current_node); 

    // Make sure the count of items is up-to-date
    list->count--; 

    // Finally, return the payload
    return payload;
}

