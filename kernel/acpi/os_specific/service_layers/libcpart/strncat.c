#include <string.h>

char *strncat(char *__restrict dest, const char *__restrict src, size_t len)
{
    size_t dest_len = strlen(dest);
    size_t i;

    for(i = 0 ; i < len && src[i] != '\0' ; i++)
        dest[dest_len + i] = src[i];

    dest[dest_len + i] = '\0';

    return dest;
}
