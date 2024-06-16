//#include <_ansi.h>
#include <string.h>

void *memcpy(void *__restrict dst0, const void *__restrict src0, size_t len0)
{
    char *dst = (char *)dst0;
    char *src = (char *)src0;

    void *save = dst0;

    while(len0--)
    {
        *dst++ = *src++;
    }

    return save;
}

