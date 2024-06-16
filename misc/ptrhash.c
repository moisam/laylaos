#include <stdint.h>
#include <sys/hash.h>


/*
 * Calculate and return the hash index of the given pointer.
 *
 * Algorithm taken from Thomas Wang's paper: https://gist.github.com/badboy/6267743
 */
uint32_t calc_hash_for_ptr(struct hashtab_t *h, void *ptr)
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
int ptr_compare(void *p1, void *p2)
{
    return !((uintptr_t)p1 == (uintptr_t)p2);
}

