/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ipv6_addr.c
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
 *  \file ipv6_addr.c
 *
 *  Internet Protocol (IP) v6 implementation.
 *
 *  The code is divided into the following files:
 *  - ipv6.c: main IPv6 handling code
 *  - ipv6_addr.c: functions for working with IPv6 addresses
 *  - ipv4_nd.c: functions for handling IPv6 neighbour discovery
 */

#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/net/ipv6.h>

#undef ISDIGIT
#define ISDIGIT(x)      (((x) >= '0') && ((x) <= '9'))

#undef ISHEX
#define ISHEX(h)        (ISDIGIT(h) ||                  \
                         ((h) >= 'a' && (h) <= 'f') ||  \
                         ((h) >= 'A' && (h) <= 'F'))

const uint8_t IPv6_ANY[16] = { 0 };
const uint8_t IPv6_LOCALHOST[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 
                                     0, 0, 0, 0, 0, 0, 0, 1 };


int ipv6_is_unspecified(const uint8_t addr[16])
{
    int i;
    
    for(i = 0; i < 16; i++)
    {
        if(addr[i])
        {
            return 0;
        }
    }

    return 1;
}

int ipv6_is_global(const uint8_t addr[16])
{
    // prefix: 2000::/3
    return (((addr[0] >> 5) == 0x01));
}


int ipv6_is_linklocal(const uint8_t addr[16])
{
    // prefix: fe80::/10
    return ((addr[0] == 0xfe) && ((addr[1] >> 6) == 0x02));
}


int ipv6_is_sitelocal(const uint8_t addr[16])
{
    // prefix: fec0::/10
    return ((addr[0] == 0xfe) && ((addr[1] >> 6) == 0x03));
}


int ipv6_is_uniquelocal(const uint8_t addr[16])
{
    // prefix: fc00::/7
    return (((addr[0] >> 1) == 0x7e));
}


int ipv6_is_localhost(const uint8_t addr[16])
{
    int i;
    
    for(i = 0; i < 16; i++)
    {
        if(addr[i] != IPv6_LOCALHOST[i])
        {
            return 0;
        }
    }

    return 1;
}


int ipv6_is_multicast(const uint8_t addr[16])
{
    // prefix: ff00::/8
    return (addr[0] == 0xff);
}


int ipv6_is_unicast(struct in6_addr *addr)
{
    return ipv6_is_global(addr->s6_addr) ||
           ipv6_is_uniquelocal(addr->s6_addr) ||
           ipv6_is_sitelocal(addr->s6_addr) ||
           ipv6_is_linklocal(addr->s6_addr) ||
           ipv6_is_localhost(addr->s6_addr) ||
           ipv6_link_get(addr);
}


int ipv6_is_solnode_multicast(struct netif_t *ifp, const uint8_t addr[16])
{
    struct ipv6_link_t *link;
    
    if(ipv6_is_multicast(addr))
    {
        return 0;
    }
    
    link = ipv6_link_by_ifp(ifp);
    
    while(link)
    {
        if(ipv6_is_linklocal(link->addr.s6_addr))
        {
            int i, match = 0;
            
            for(i = 13; i < 16; i++)
            {
                if(addr[i] == link->addr.s6_addr[i])
                {
                    match++;
                }
            }
            
            // Solicitation: last 3 bytes match a local address
            if(match == 3)
            {
                return 1;
            }
        }
        
        link = ipv6_link_by_ifp_next(ifp, link);
    }
    
    return 0;
}


int ipv6_is_allhosts_multicast(const uint8_t addr[16])
{
    struct in6_addr allhosts =
    {
        .s6_addr =
        {
            0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        }
    };
    
    return !memcmp(allhosts.s6_addr, addr, 16);
}



int ipv6_cmp(struct in6_addr *a, struct in6_addr *b)
{
    uint32_t i;
    
    for(i = 0; i < 16; i++)
    {
        if(a->s6_addr[i] < b->s6_addr[i])
        {
            return -1;
        }

        if(a->s6_addr[i] > b->s6_addr[i])
        {
            return 1;
        }
    }

    return 0;
}


static inline int hex_to_decimal(char c)
{
    return (c >= '0' && c <= '9') ? (c - '0') :
           (c >= 'a' && c <= 'f') ? (10 + c - 'a') :
           (c >= 'A' && c <= 'F') ? (10 + c - 'A') : 0;
}


int string_to_ipv6(const char *str, uint8_t *ip)
{
    uint8_t buf[16] = { 0, };
    char c;
    int i = 0, nibble = 0, hex = 0, colons = 0, diff = 0;
    int zeroes = 0, shift = 0;
    uint8_t doublecolon = 0, byte = 0;
    
    if(!str || !ip)
    {
        return -EINVAL;
    }
    
    IPV6_ZERO_SET(ip);
    
    while((c = *str++))
    {
        if(ISHEX(c) || c == ':' || *str == '\0')
        {
            if(ISHEX(c))
            {
                buf[byte] = (uint8_t)((buf[byte] << 4) + hex_to_decimal(c));
                
                if(++nibble % 2 == 0)
                {
                    byte++;
                }
            }
            
            if(c == ':' || *str == '\0')
            {
                hex++;
                
                if(c == ':')
                {
                    colons++;
                }
                
                diff = (hex * 4) - nibble;
                nibble += diff;
                
                switch(diff)
                {
                    case 0:
                        // 16-bit hex block ok e.g. 1db8
                        break;
                    
                    case 1:
                        // one zero e.g. db8: byte = 1, buf[byte-1] = 0xdb, 
                        // buf[byte] = 0x08
                        buf[byte] |= (uint8_t)(buf[byte - 1] << 4);
                        buf[byte - 1] >>= 4;
                        byte++;
                        break;
                    
                    case 2:
                        // two zeros e.g. b8: byte = 1, buf[byte] = 0x00,
                        // buf[byte-1] = 0xb8
                        buf[byte] = buf[byte - 1];
                        buf[byte - 1] = 0;
                        byte++;
                        break;
                    
                    case 3:
                        // three zeros e.g. 8: byte = 0, buf[byte] = 0x08, 
                        // buf[byte+1] = 0x00
                        buf[byte + 1] = buf[byte];
                        buf[byte] = 0;
                        byte += 2;
                        break;
                    
                    case 4:
                        // case of ::
                        if(doublecolon && colons != 2)
                        {
                            // catch case x::x::x but not ::x
                            return -EINVAL;
                        }
                        
                        doublecolon = byte;
                        break;
                    
                    default:
                        // case of missing colons e.g. 20011db8 instead of 2001:1db8
                        return -EINVAL;
                }
            }
        }
        else
        {
            return -EINVAL;
        }
    }
    
    if(colons < 2)
    {
        // valid IPv6 has at least two colons
        return -EINVAL;
    }
    
    // account for leftout :: zeros
    if((zeroes = 16 - byte))
    {
        shift = 16 - zeroes - doublecolon;
        
        for(i = shift; i >= 0; --i)
        {
            // (i-1) as arrays are indexed from 0
            if((doublecolon + (i - 1)) >= 0)
            {
                buf[doublecolon + zeroes + (i - 1)] = buf[doublecolon + (i - 1)];
            }
        }
        
        A_memset(&buf[doublecolon], 0, zeroes);
    }
    
    memcpy(ip, buf, 16);
    
    return 0;
}

