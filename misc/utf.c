
/*
 * Code taken from:
 * https://codereview.stackexchange.com/questions/244060/the-conversion-from-utf-16-to-utf-8
 *
 * With minor formatting and changes.
 */

#include <stdint.h>
#include <stddef.h>
#include <mm/kheap.h>

char *utf16_to_utf8_char(uint16_t *str)
{
    char *dest = kmalloc(256);
    char *p = dest;
    char *end = dest + 256;
    int destsz = 256;

    if(!dest)
    {
        return NULL;
    }

    *p = '\0';

    while(*str)
    {
        unsigned int codepoint = 0x0;

        //-------(1) UTF-16 to codepoint -------

        if(*str <= 0xD7FF)
        {
            codepoint = *str;
            str++;
        }
        else if(*str <= 0xDBFF)
        {
            unsigned short highSurrogate = (*str - 0xD800) * 0x400;
            unsigned short lowSurrogate = *(str + 1) - 0xDC00;
            codepoint = (lowSurrogate | highSurrogate) + 0x10000;
            str += 2;
        }

        if(p + 5 >= end)
        {
            char *tmp;

            if(!(tmp = krealloc(dest, destsz * 2)))
            {
                kfree(dest);
                return NULL;
            }

            p = tmp + (p - dest);
            dest = tmp;
            destsz *= 2;
            end = dest + destsz;
        }

        //-------(2) Codepoint to UTF-8 -------

        if(codepoint <= 0x007F)
        {
            *p++ = (char)codepoint;
            *p = '\0';
        }
        else if(codepoint <= 0x07FF)
        {
            *p++ = ((codepoint >> 6) & 0x1F) | 0xC0;
            *p++ = (codepoint & 0x3F) | 0x80;
            *p = '\0';
        }
        else if(codepoint <= 0xFFFF)
        {
            *p++ = ((codepoint >> 12) & 0x0F) | 0xE0;
            *p++ = ((codepoint >> 6) & 0x3F) | 0x80;
            *p++ = ((codepoint) & 0x3F) | 0x80;
            *p = '\0';
        }
        else if(codepoint <= 0x10FFFF)
        {
            *p++ = ((codepoint >> 18) & 0x07) | 0xF0;
            *p++ = ((codepoint >> 12) & 0x3F) | 0x80;
            *p++ = ((codepoint >> 6) & 0x3F) | 0x80;
            *p++ = ((codepoint) & 0x3F) | 0x80;
            *p = '\0';
        }
    }

    return dest;
}

