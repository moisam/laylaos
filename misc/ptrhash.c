#include <stdint.h>
#include <sys/hash.h>
#include <sys/hash_inline_ptr.h>


uint32_t calc_hash_for_ptr(struct hashtab_t *h, void *ptr)
{
    return inlined_calc_hash_for_ptr(h, ptr);
}


/*
 * Compare two pointers (used to compare hash keys).
 *
 * Returns 0 if the two pointers are equal (similar to strmp(), which we use
 * to compare string hash keys).
 */
int ptr_compare(void *p1, void *p2)
{
    return inlined_ptr_compare(p1, p2);
}

