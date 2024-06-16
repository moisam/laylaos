#include <string.h>

char *strncpy(char *__restrict dest, const char *__restrict src, size_t len)
{
    size_t i;
    
    for(i = 0; i < len && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    
    for( ; i < len; i++)
    {
        dest[i] = '\0';
    }
    
    return dest;
}

