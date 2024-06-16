/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: list.h
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
 *  \file list.h
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

#ifndef GUI_LIST_H
#define GUI_LIST_H

#include "list-struct.h"
#include "listnode.h"
#include <stdlib.h>
#include "mutex.h"


#ifdef __cplusplus
extern "C" {
#endif

// defined in common/list.c
void* List_remove_at(List* list, int index);
void prep_list_cache(void);

extern volatile List *list_cache;
extern mutex_t cache_lock;


// Basic list constructor
static inline List *List_new(void)
{
    List *list;

    if(list_cache)
    {
        list = (List *)list_cache;
        list_cache = list_cache->next;
    }
    // Malloc and/or fail null
    else if(!(list = (List *)malloc(sizeof(List))))
    {
        return list;
    }

    // Fill in initial property values
    // (All we know for now is that we start out with no items) 
    list->count = 0;
    list->root_node = (ListNode *)0;
    list->last_node = (ListNode *)0;
    list->next = (List *)0;

    return list;
}

static inline void List_free(List *list)
{
    list->next = (List *)list_cache;
    list_cache = list;
}

// Insert a payload at the end of the list
// Zero is fail, one is success
static inline int List_add(List *list, void *payload)
{
    // Try to make a new node, exit early on fail 
    ListNode *new_node;

    if(!(new_node = ListNode_new(payload)))
    {
        return 0;
    }

    // If there aren't any items in the list yet, assign the
    // new item to the root node
    if(!list->root_node)
    {
        list->root_node = new_node;        
        list->last_node = new_node;        
    }
    else
    {
        // Otherwise, we'll find the last node and add our new node after it
        /* volatile */ ListNode *last = list->last_node;
        last->next = new_node;
        new_node->prev = (ListNode *)last;
        list->last_node = new_node;
    }

    // Update the number of items in the list and return success
    list->count++;

    return 1;
}

static inline int List_add_unlocked(List *list, void *payload)
{
    // Try to make a new node, exit early on fail 
    ListNode *new_node;

    if(!(new_node = ListNode_new_unlocked(payload)))
    {
        return 0;
    }

    // If there aren't any items in the list yet, assign the
    // new item to the root node
    if(!list->root_node)
    {
        list->root_node = new_node;        
        list->last_node = new_node;        
    }
    else
    {
        // Otherwise, we'll find the last node and add our new node after it
        /* volatile */ ListNode *last = list->last_node;
        last->next = new_node;
        new_node->prev = (ListNode *)last;
        list->last_node = new_node;
    }

    // Update the number of items in the list and return success
    list->count++;

    return 1;
}


// defined in common/rect.c
void prep_rectlist_cache(void);

extern volatile RectList *rectlist_cache;


// Basic list constructor
static inline RectList *RectList_new(void)
{
    RectList *list;

    if(rectlist_cache)
    {
        list = (RectList *)rectlist_cache;
        rectlist_cache = rectlist_cache->next;
    }
    // Malloc and/or fail null
    else if(!(list = (RectList *)malloc(sizeof(RectList))))
    {
        return list;
    }

    // Fill in initial property values
    // (All we know for now is that we start out with no items) 
    //list->count = 0;
    list->root = 0;
    list->last = 0;
    list->next = 0;

    return list;
}

static inline RectList *RectList_new_unlocked(void)
{
    RectList *list;

    if(rectlist_cache)
    {
        list = (RectList *)rectlist_cache;
        rectlist_cache = rectlist_cache->next;
    }
    // Malloc and/or fail null
    else
    {
        if(!(list = (RectList *)malloc(sizeof(RectList))))
        {
            return list;
        }
    }

    // Fill in initial property values
    // (All we know for now is that we start out with no items) 
    //list->count = 0;
    list->root = 0;
    list->last = 0;
    list->next = 0;

    return list;
}

static inline void RectList_free(RectList *list)
{
    list->next = (RectList *)rectlist_cache;
    rectlist_cache = list;
}

static inline void RectList_free_unlocked(RectList *list)
{
    list->next = (RectList *)rectlist_cache;
    rectlist_cache = list;
}

// Insert a payload at the end of the list
// Zero is fail, one is success
static inline void RectList_add(RectList *list, Rect *rect)
{
    rect->next = NULL;

    // If there aren't any items in the list yet, assign the
    // new item to the root node
    if(!list->root)
    {
        list->root = rect;        
        list->last = rect;        
    }
    else
    {
        // Otherwise, we'll find the last node and add our new node after it
        list->last->next = rect;
        list->last = rect;
    }

    // Update the number of items in the list and return success
    //list->count++;
}


#ifdef __cplusplus
}
#endif

#endif //GUI_LIST_H
