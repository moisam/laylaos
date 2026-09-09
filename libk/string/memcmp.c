#include <string.h>

#ifdef __x86_64__

#include <stddef.h>
#include <stdint.h>

int memcmp(const void *aptr, const void *bptr, size_t size)
{
    const char *a = (const char *)aptr;
    const char *b = (const char *)bptr;

    // If memories are unaligned, loop until reaching an 8-byte boundary alignment target
    while(size > 0 && (((uintptr_t)a & 7) || ((uintptr_t)b & 7)))
    {
        if(*a != *b)
        {
            return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        }

        a++;
        b++;
        size--;
    }

    // 64-Bit Block Matching Loop
    const uint64_t *word_a = (const uint64_t *)a;
    const uint64_t *word_b = (const uint64_t *)b;

    while(size >= 8)
    {
        if(*word_a != *word_b)
        {
            break; // Mismatch caught inside this block boundary area
        }

        word_a++;
        word_b++;
        size -= 8;
    }

    // Pinpoint the mismatching byte order or handle final remaining odd tail bytes
    a = (const char *)word_a;
    b = (const char *)word_b;

    while(size > 0)
    {
        if(*a != *b)
        {
            return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        }

        a++;
        b++;
        size--;
    }

    return 0; // Absolute identical match verified
}

#else       /* __x86_64__ */

int memcmp(const void *aptr, const void *bptr, size_t size)
{
  const unsigned char *a = (const unsigned char *)aptr;
  const unsigned char *b = (const unsigned char *)bptr;
  for(size_t i = 0; i < size; i++)
    if(a[i] < b[i])
      return -1;
    else if (b[i] < a[i])
      return 1;
    
  return 0;
}

#endif      /* __x86_64__ */
