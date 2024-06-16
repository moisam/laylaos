#include <stdint.h>
#include <sys/hash.h>

/*
 * We will use FNV-1a hashing. The following variables and functions implement
 * this hashing function. These are the default values recommended by:
 * http://isthe.com/chongo/tech/comp/fnv/.
 */
const uint32_t fnv1a_prime = 0x01000193; /* 16777619 */
const uint32_t fnv1a_seed  = 0x811C9DC5; /* 2166136261 */


/*
 * The FNV-1a hasing function.
 * Returns a 32-bit hash index.
 */
uint32_t fnv1a(char *text, uint32_t hash)
{
    if(!text)
    {
        return 0;
    }
    
    unsigned char *p = (unsigned char *)text;

    while(*p)
    {
        hash = (*p++ ^ hash) * fnv1a_prime;
    }

    return hash;
}


/*
 * Calculate and return the hash index of the given string.
 * 
 * TODO: If you want to use another hashing algorithm, change the
 *       function call to fnv1a() to any other function.
 */
uint32_t calc_hash_for_str(struct hashtab_t *h, void *text)
//uint32_t calc_hash(struct hashtab_t *h, char *text)
{
    if(!h || !text)
    {
        return 0;
    }

    return fnv1a((char *)text, fnv1a_seed) % h->count;
}

