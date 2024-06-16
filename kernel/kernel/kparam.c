/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: kparam.c
 *    This file is part of LaylaOS.
 *
 *    LaylaOS is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    LaylaOS is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LaylaOS.  If not, see <http://www.gnu.org/licenses/>.
 */    

/**
 *  \file kparam.c
 *
 *  This file contains functions to query and retrieve the parameters passed
 *  to the kernel during boot.
 */

//#define __DEBUG
#include <string.h>
#include <kernel/laylaos.h>
#include <mm/kheap.h>


char *get_cmdline_param_val(char *name)
{
    char *p, *p2, c;
    size_t namelen = 0, vallen = 0;

    KDEBUG("get_cmdline_param_val: name '%s', cmdline '%s'\n", name, kernel_cmdline);

    if(!name || !*name || !*kernel_cmdline)
    {
        return NULL;
    }

    for(p = name; *p; p++, namelen++)
    {
        ;
    }

    p = kernel_cmdline;

    while(*p)
    {
        if(memcmp(name, p, namelen) == 0)
        {
            c = p[namelen];

            // param=val or param=
            if(c == '=')
            {
                p += namelen + 1;
                p2 = p;

                while(*p2 && *p2 != ' ' && *p2 != '\t')
                {
                    p2++;
                }

                vallen = p2 - p;

                if(!(p2 = kmalloc(vallen + 1)))
                {
                    return NULL;
                }

                memcpy(p2, p, vallen);
                p2[vallen] = '\0';

                return p2;
            }
            else
            {
                return NULL;
            }
        }

        p++;
    }

    return NULL;
}


int has_cmdline_param(char *name)
{
    char *p, *p2, c;
    size_t namelen = 0;

    KDEBUG("has_cmdline_param: name '%s', cmdline '%s'\n", name, kernel_cmdline);

    if(!name || !*name || !*kernel_cmdline)
    {
        return 0;
    }

    for(p = name; *p; p++, namelen++)
    {
        ;
    }

    p = kernel_cmdline;

    while(*p)
    {
        if(!(p2 = strstr(p, name)))
        {
            return 0;
        }

        c = p2[namelen];

        if(c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '=')
        {
            return 1;
        }

        p = p2 + namelen;
    }

    return 0;
}

