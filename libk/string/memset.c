#include <string.h>

void *memset(void *m, int c, size_t n)
{
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    char *s = (char *) m;

    while(n--)
    {
        *s++ = (char) c;
    }

    return m;
}

