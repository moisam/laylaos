/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: hash_inline.h
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
 *  \file hash_inline.h
 *
 *  General hashtab implementation.
 *  Some kernel components (e.g. the page cache, the select table) are 
 *  accessed frequently and need very fast access. The general hashtab
 *  implementation works well, but is too slow for these components.
 *
 *  Here we reimplement some of the hash functions with inlined hashing
 *  and key comparison functions to reduce the overhead of function calls
 *  associated with our generic hash implementation.
 *
 *  Files including this file must define their inlined hashing and key
 *  comparison functions and define the below two macros BEFORE including
 *  this file.
 */

#ifndef HASHTAB_INLINE_H
#define HASHTAB_INLINE_H

#ifndef CALC_HASH_FUNC
#error you must define CALC_HASH_FUNC
#endif

#ifndef KEY_COMPARE_FUNC
#error you must define KEY_COMPARE_FUNC
#endif

#include <kernel/laylaos.h>
#include <mm/kheap.h>
#include "hash.h"

STATIC_INLINE uint32_t CALC_HASH_FUNC(struct hashtab_t *h, void *key);
STATIC_INLINE int KEY_COMPARE_FUNC(void *p1, void *p2);


static struct hashtab_item_t *hashtab_fast_lookup(struct hashtab_t *h, void *key)
{
    struct hashtab_item_t *hitem;
    unsigned int i;

    if(!h)
    {
        return NULL;
    }
    
    i = CALC_HASH_FUNC(h, key);
    hitem = h->items[i];
    
    while(hitem)
    {
        if(KEY_COMPARE_FUNC(hitem->key, key) == 0)
        {
            return hitem;
        }
        
        hitem = hitem->next;
    }
    
    return NULL;
}


static void hashtab_fast_add_hitem(struct hashtab_t *h,
                                   void *key, struct hashtab_item_t *new_hitem)
{
    struct hashtab_item_t *hitem, *prev;
    unsigned int i;

    if(!h || !key)
    {
        return;
    }
    
    i = CALC_HASH_FUNC(h, key);
    hitem = h->items[i];
    
    if(!hitem)
    {
        h->items[i] = new_hitem;
        return;
    }
    
    prev = NULL;
    
    while(hitem)
    {
        if(KEY_COMPARE_FUNC(hitem->key, key) == 0)
        {
            new_hitem->next = hitem->next;
            
            if(prev)
            {
                prev->next = new_hitem;
            }
            else
            {
                h->items[i] = new_hitem;
            }
            
            kfree(hitem);
            return;
        }
        
        prev = hitem;
        hitem = hitem->next;
    }
    
    prev->next = new_hitem;
}


__attribute__((unused))
static void hashtab_fast_remove(struct hashtab_t *h, void *key)
{
    struct hashtab_item_t *hitem, *prev;
    unsigned int i;

    if(!h || !key)
    {
        return;
    }
    
    i = CALC_HASH_FUNC(h, key);
    hitem = h->items[i];
    
    if(!hitem)
    {
        return;
    }
    
    prev = NULL;
    
    while(hitem)
    {
        if(KEY_COMPARE_FUNC(hitem->key, key) == 0)
        {
            if(prev)
            {
                prev->next = hitem->next;
            }
            else
            {
                h->items[i] = hitem->next;
            }

            kfree(hitem);
            return;
        }
        
        prev = hitem;
        hitem = hitem->next;
    }
}


STATIC_INLINE struct hashtab_item_t *hashtab_fast_alloc_hitem(void *key, void *val)
{
    struct hashtab_item_t *hitem;
    
    if(!(hitem = kmalloc(sizeof(struct hashtab_item_t))))
    {
        return NULL;
    }

    A_memset(hitem, 0, sizeof(struct hashtab_item_t));
    hitem->key = key;
    hitem->val = val;
    
    return hitem;
}

#endif      /* HASHTAB_INLINE_H */
