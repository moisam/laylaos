/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ipv4_addr.c
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
 *  \file ipv4_addr.c
 *
 *  Internet Protocol (IP) v4 implementation.
 *
 *  The code is divided into the following files:
 *  - ipv4.c: main IPv4 handling code
 *  - ipv4_addr.c: functions for working with IPv4 addresses
 *  - ipv4_frag.c: functions for handling IPv4 packet fragments
 */

#include <errno.h>
#include <stddef.h>
#include <netinet/in.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/misc_macros.h>

#undef ISDIGIT
#define ISDIGIT(x)      (((x) >= '0') && ((x) <= '9'))


int ipv4_is_multicast(uint32_t addr)
{
    uint8_t *bytes = (uint8_t *)&addr;
    
    return ((bytes[0] != 0xff) && ((bytes[0] & 0xe0) == 0xe0));
}


int ipv4_is_broadcast(uint32_t addr)
{
    struct ipv4_link_t *link;
    
    if(addr == INADDR_BROADCAST)
    {
        return 1;
    }

    for(link = ipv4_links; link != NULL; link = link->next)
    {
        if((link->addr.s_addr | (~link->netmask.s_addr)) == addr)
        {
            return 1;
        }
    }
    
    return 0;
}


int ipv4_cmp(struct in_addr *a, struct in_addr *b)
{
    if(a->s_addr < b->s_addr)
    {
        return -1;
    }

    if(a->s_addr > b->s_addr)
    {
        return 1;
    }
    
    return 0;
}


int string_to_ipv4(const char *str, uint32_t *ip)
{
    uint8_t buf[4] = { 0, 0, 0, 0 };
    char c;
    int i = 0;
    
    if(!str || !ip)
    {
        return -EINVAL;
    }
    
    while((c = *str++) && (i < 4))
    {
        if(ISDIGIT(c))
        {
            buf[i] = (uint8_t)((buf[i] * 10) + (c - '0'));
        }
        else if(c == '.')
        {
            i++;
        }
        else
        {
            return -EINVAL;
        }
    }
    
    // handle short notation
    if(i == 1)
    {
        buf[3] = buf[1];
        buf[1] = 0;
        buf[2] = 0;
    }
    else if(i == 2)
    {
        buf[3] = buf[2];
        buf[2] = 0;
    }
    else if(i != 3)
    {
        // String could not be parsed
        return -EINVAL;
    }
    
    *ip = ptr_to_long(buf, 0);
    
    return 0;
}

