/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: options.c
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
 *  \file options.c
 *
 *  This file defines the parse_options() function, which is called by
 *  different filesystem modules to parse mounting options.
 */

//#define __DEBUG

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <fs/options.h>
#include <mm/kheap.h>


/*
 * Helper function to find the end of an option name/value. If is_val is non-
 * zero, it means we are reading an option's value, otherwise its a name. The
 * only difference is that a value might be followed by ',' if there is 
 * another option coming after it, while options should end with '=', followed
 * by the option's value.
 */
static int optend(char *str, int is_val)
{
    char *s2 = str;
    
    while(*s2)
    {
        if(*s2 == '=')
        {
            break;
        }
        
        if(*s2 == ',' && is_val)
        {
            break;
        }
        
        s2++;
    }
    
    return s2 - str;
}


/*
 * Helper function to convert a string to a decimal number.
 */
static size_t getint(char *str, int count)
{
    int i;
    size_t n = 0;
    
    for(i = 0; i < count; i++)
    {
        n = (n * 10) + (str[i] - '0');
    }
    
    return n;
}


/*
 * Helper function to get kalloc'd string value.
 */
static char *getstr(char *str, int count)
{
    char *s;
    
    if((s = kmalloc(count + 1)))
    {
        memcpy(s, str, count);
        s[count] = '\0';
    }
    
    return s;
}


static inline int any_required(struct ops_t *ops, int ops_count)
{
    int i;
    
    for(i = 0; i < ops_count; i++)
    {
        if(ops[i].is_required)
        {
            return 1;
        }
    }
    
    return 0;
}


#define REPORT_ERR                      if(report_err) printk

#define SKIP_IF_NOT_REQUIRED()          \
    if(!ops[j].is_required)             \
    {                                   \
        goto skip;                      \
    }


/*
 * Parse mount options.
 */
int parse_options(char *module, char *str, struct ops_t *ops,
                  int ops_count, int flags)
{
    int i, j;
    int report_err = (flags & OPS_FLAG_REPORT_ERRORS);
    int ingnore_unknown = (flags & OPS_FLAG_IGNORE_UNKNOWN);
    
    if(!module || !ops || !ops_count)
    {
        return -EINVAL;
    }

    for(i = 0; i < ops_count; i++)
    {
        ops[i].val.i = 0;
        ops[i].is_present = 0;
    }
    
    KDEBUG("parse_options: str '%s'\n", str);
    
    // sanity checks
    if(!str || !*str)
    {
        if(any_required(ops, ops_count))
        {
            REPORT_ERR("%s: missing options\n", module);
            return -EINVAL;
        }
        
        return 0;
    }
    
    // parse the options string
    while(*str)
    {
        if((i = optend(str, 0)) == 0)
        {
            break;
        }
        
        for(j = 0; j < ops_count; j++)
        {
            //if(strncmp(str, ops[j].name, i) == 0)
            if(memcmp(str, ops[j].name, i) == 0)
            {
                str += i;
                
                // record that the option is present
                ops[j].is_present = 1;
                
                // option name could be followed by '='
                if(*str != '=')
                {
                    SKIP_IF_NOT_REQUIRED();
                    REPORT_ERR("%s: option '%s' must be followed by "
                                "'=' and a value\n",
                                module, ops[j].name);
                    return -EINVAL;
                }
                
                // option name with/without '=' but no value
                if(!str[1])
                {
                    SKIP_IF_NOT_REQUIRED();
                    REPORT_ERR("%s: option '%s' is missing value\n",
                               module, ops[j].name);
                    return -EINVAL;
                }
                
                // get the value after the '='
                if((i = optend(++str, 1)) == 0)
                {
                    SKIP_IF_NOT_REQUIRED();
                    REPORT_ERR("%s: option '%s' is missing value\n",
                               module, ops[j].name);
                    return -EINVAL;
                }
                
                if(*str >= '0' && *str <= '9')
                {
                    ops[j].val.i = getint(str, i);
                    ops[j].is_int = 1;
                }
                else
                {
                    ops[j].val.s = getstr(str, i);
                    ops[j].is_int = 0;
                }
                
                str += i;

                KDEBUG("parse_options: ops[%d].val %d, str '%s'\n", j, ops[j].val, str);

skip:
                if(*str == ',')
                {
                    str++;
                }
                
                break;
            }
        }
        
        if(j == ops_count && !ingnore_unknown)
        {
            REPORT_ERR("%s: unknown option string: '%s'\n", module, str);
            return -EINVAL;
        }
    }
    
    for(j = 0; j < ops_count; j++)
    {
        if(!ops[j].is_present && ops[j].is_required)
        {
            REPORT_ERR("%s: missing or invalid option: '%s'\n",
                        module, ops[j].name);
            return -EINVAL;
        }
    }
    
    return 0;
}


void free_option_strings(struct ops_t *ops, int ops_count)
{
    int i;

    for(i = 0; i < ops_count; i++)
    {
        if(ops[i].is_int == 0 && ops[i].val.s)
        {
            kfree(ops[i].val.s);
            ops[i].val.s = NULL;
        }
    }
}

