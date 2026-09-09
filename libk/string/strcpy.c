#include <stdint.h>
#include <string.h>

#ifdef __x86_64__

char *strcpy(char *restrict s1, const char *restrict s2)
{
    char *dest = s1;

    // Align the source pointer to an 8-byte memory boundary first
    while((uintptr_t)s2 & 7)
    {
        if((*s1++ = *s2++) == '\0')
        {
            return dest;
        }
    }

    // Parallelized Block Word Copying
    uint64_t *restrict dest_word = (uint64_t *)s1;
    const uint64_t *restrict src_word  = (const uint64_t *)s2;

    const uint64_t high_bits = 0x8080808080808080ULL;
    const uint64_t low_bits  = 0x0101010101010101ULL;

    while(1)
    {
        uint64_t word = *src_word;
        
        // Scan ahead to verify if this 8-byte segment contains a null termination byte
        uint64_t has_zero = (word - low_bits) & ~word & high_bits;
        
        if(has_zero != 0)
        {
            // A null terminator exists in this chunk. Break out to scalar cleanup.
            break;
        }

        // Blit 8 bytes
        *dest_word++ = word;
        src_word++;
    }

    // Scalar cleanup copy pass to transfer the remaining string tail bytes
    s1 = (char *)dest_word;
    s2 = (const char *)src_word;

    while((*s1++ = *s2++)) ;

    return dest;
}

#else       /* __x86_64__ */

char *strcpy(char *s1, const char *s2)
{
  char *s3 = (char *)s1;
  while((*s1++ = *s2++)) ;
  return s3;
}

#endif      /* __x86_64__ */
