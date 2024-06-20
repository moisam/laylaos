/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: pcache-internal.h
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
 *  \file pcache-internal.h
 *
 *  Internal functions for the page cache use.
 */

#include <stdint.h>
#include <sys/hash.h>


/*
 * We will use FNV-1a hashing. The following variables and functions implement
 * this hashing function. These are the default values recommended by:
 * http://isthe.com/chongo/tech/comp/fnv/.
 */
static const uint32_t fnv1a_prime = 0x01000193; /* 16777619 */
static const uint32_t fnv1a_seed  = 0x811C9DC5; /* 2166136261 */


/*
 * The FNV-1a hasing function.
 * Returns a 32-bit hash index.
 */
static inline uint32_t fnv1a_internal(void *__key, uint32_t hash)
{
    struct pcache_key_t *key = (struct pcache_key_t *)__key;
    uint8_t *p = (uint8_t *)key;
    uint8_t *lp = p + 24;

    while(p < lp)
    {
        hash = (*p++ ^ hash) * fnv1a_prime;
    }

    return hash;
}


/*
 * Calculate and return the hash index of the given key.
 * 
 * TODO: If you want to use another hashing algorithm, change the
 *       function call to fnv1a() to any other function.
 */
static inline uint32_t calc_hash_for_pcache(struct hashtab_t *h, void *key)
{
    if(!h || !key)
    {
        return 0;
    }

    return fnv1a_internal(key, fnv1a_seed) % h->count;
}


/*
 * Compare two keys (used to compare hash keys).
 *
 * Returns 0 if the two pointers are equal (similar to strmp(), which we use
 * to compare string hash keys).
 */
static inline int pcache_key_compare(void *p1, void *p2)
{
    struct pcache_key_t *k1 = (struct pcache_key_t *)p1;
    struct pcache_key_t *k2 = (struct pcache_key_t *)p2;

    return !(k1->dev == k2->dev && k1->ino == k2->ino && k1->offset == k2->offset);
}


/*
 * Initialise the pcache table.
 */
void init_pcache(void)
{
    if(!(pcachetab = hashtab_create(INIT_HASHSZ, calc_hash_for_pcache, 
                                    pcache_key_compare)))
    {
        kpanic("Failed to initialise kernel page cache table\n");
    }
}


/*
 * The page cache is accessed frequently and needs very fast access.
 * Below we reimplement some of the hash functions with inlined hashing
 * and key comparison functions to reduce the overhead of function calls
 * associated with our generic hash implementation.
 */

static struct hashtab_item_t *pcache_lookup(struct hashtab_t *h, void *key)
{
    struct hashtab_item_t *hitem;
    unsigned int i;

    if(!h)
    {
        return NULL;
    }
    
    i = calc_hash_for_pcache(h, key);
    hitem = h->items[i];
    
    while(hitem)
    {
        if(pcache_key_compare(hitem->key, key) == 0)
        {
            return hitem;
        }
        
        hitem = hitem->next;
    }
    
    return NULL;
}

static void pcache_add_hitem(struct hashtab_t *h,
                             void *key, struct hashtab_item_t *new_hitem)
{
    struct hashtab_item_t *hitem, *prev;
    unsigned int i;

    if(!h || !key)
    {
        return;
    }
    
    i = calc_hash_for_pcache(h, key);
    hitem = h->items[i];
    
    if(!hitem)
    {
        h->items[i] = new_hitem;
        return;
    }
    
    prev = NULL;
    
    while(hitem)
    {
        if(pcache_key_compare(hitem->key, key) == 0)
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

static void pcache_remove(struct hashtab_t *h, void *key)
{
    struct hashtab_item_t *hitem, *prev;
    unsigned int i;

    if(!h || !key)
    {
        return;
    }
    
    i = calc_hash_for_pcache(h, key);
    hitem = h->items[i];
    
    if(!hitem)
    {
        return;
    }
    
    prev = NULL;
    
    while(hitem)
    {
        if(pcache_key_compare(hitem->key, key) == 0)
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

static inline struct hashtab_item_t *pcache_alloc_hitem(void *key, void *val)
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

