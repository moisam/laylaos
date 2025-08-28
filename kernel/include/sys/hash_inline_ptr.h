/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: hash_inline_ptr.h
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
 *  \file hash_inline_ptr.h
 *
 *  Inlined hashing and comparison functions for use with pointer-type
 *  hash keys. If this file is used, it should be included AFTER including
 *  hash_inline.h.
 */

#ifndef HASHTAB_INLINE_PTR_H
#define HASHTAB_INLINE_PTR_H

#include <kernel/laylaos.h>


/*
 * Calculate and return the hash index of the given pointer.
 *
 * Algorithm taken from Thomas Wang's paper: https://gist.github.com/badboy/6267743
 */
STATIC_INLINE uint32_t inlined_calc_hash_for_ptr(struct hashtab_t *h, void *ptr)
{
    if(!h || !ptr)
    {
        return 0;
    }

#ifdef __x86_64__

    uint64_t key64 = (uint64_t)ptr;

    key64 = (~key64) + (key64 << 18); // key = (key << 18) - key - 1;
    key64 = key64 ^ (key64 >> 31);
    key64 = key64 * 21; // key = (key + (key << 2)) + (key << 4);
    key64 = key64 ^ (key64 >> 11);
    key64 = key64 + (key64 << 6);
    key64 = key64 ^ (key64 >> 22);
    
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    return (uint32_t)key64 & (h->count - 1);

#else
    
    uint32_t ct = 0x27d4eb2d; // a prime or an odd constant
    uint32_t key = (uint32_t)ptr;

    key = (key ^ 61) ^ (key >> 16);
    key = key + (key << 3);
    key = key ^ (key >> 4);
    key = key * c2;
    key = key ^ (key >> 15);

    return key & (h->count - 1);

#endif
}


/*
 * Compare two pointers (used to compare hash keys).
 *
 * Returns 0 if the two pointers are equal (similar to strmp(), which we use
 * to compare string hash keys).
 */
STATIC_INLINE int inlined_ptr_compare(void *p1, void *p2)
{
    return !((uintptr_t)p1 == (uintptr_t)p2);
}

#endif      /* HASHTAB_INLINE_PTR_H */
