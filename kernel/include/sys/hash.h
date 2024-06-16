/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: hash.h
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
 *  \file hash.h
 *
 *  General hashtab implementation.
 */

#ifndef HASHTAB_H
#define HASHTAB_H

struct hashtab_item_t
{
    void *key;
    void *val;
    struct hashtab_item_t *next;
};

struct hashtab_t
{
    struct hashtab_item_t **items;
    int count;
    uint32_t (*hash_func)(struct hashtab_t *, void *);
    int (*compare_func)(void *, void *);
};


#define __PASTEH(p, f)          p ## _ ## f
#define __EVALH(p, f)           __PASTEH(p, f)

#ifdef USE_HASH_FUNC_PREFIX
#define __FNH(f)                __EVALH(USE_HASH_FUNC_PREFIX, f)
#else
#define __FNH(f)                __EVALH(_, f)
#endif


#define hashtab_create          __FNH(hashtab_create)
#define hashtab_free            __FNH(hashtab_free)
#define hashtab_lookup          __FNH(hashtab_lookup)
#define hashtab_add             __FNH(hashtab_add)
#define hashtab_add_hitem       __FNH(hashtab_add_hitem)
#define hashtab_remove          __FNH(hashtab_remove)
#define alloc_hitem             __FNH(alloc_hitem)
#define calc_hash_for_str       __FNH(calc_hash_for_str)
#define calc_hash_for_ptr       __FNH(calc_hash_for_ptr)
#define ptr_compare             __FNH(ptr_compare)


// hash.c
//struct hashtab_t *hashtab_create(int count);
struct hashtab_t *hashtab_create(int count,
                                 uint32_t (*hash_func)(struct hashtab_t *, void *),
                                 int (*compare_func)(void *, void *));
void hashtab_free(struct hashtab_t *h);
struct hashtab_item_t *hashtab_lookup(struct hashtab_t *h, void *key);
void hashtab_add(struct hashtab_t *h, void *key, void *val);
void hashtab_add_hitem(struct hashtab_t *h, void *key, 
                       struct hashtab_item_t *new_hitem);
void hashtab_remove(struct hashtab_t *h, void *key);
struct hashtab_item_t *alloc_hitem(void *key, void *val);

// fnv1a.c
uint32_t calc_hash_for_str(struct hashtab_t *h, void *text);

// ptrhash.c
uint32_t calc_hash_for_ptr(struct hashtab_t *h, void *ptr);
int ptr_compare(void *p1, void *p2);

#endif      /* HASHTAB_H */
