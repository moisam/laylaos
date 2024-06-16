#include <string.h>
//#include <kernel/laylaos.h>

int strncmp(const char *str1, const char *str2, size_t n)
//int strncmp(const char *str1, const char *str2, int n)
{
    int res = 0;
    unsigned char *s1 = (unsigned char *)str1;
    unsigned char *s2 = (unsigned char *)str2;
    
    //printk("strncmp: s1 0x%lx, s2 0x%lx\n", str1, str2);
    //if(!str1 || !str2)
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    //    return -1;

    while(n-- && !(res = *s1 - *s2) && *s2)
        ++s1, ++s2;

    if(res < 0)
        return -1;
    else if(res > 0)
        return 1;
    
    return 0;
}

