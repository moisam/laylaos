/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: checksum.h
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
 *  \file checksum.h
 *
 *  Helper functions for calculating internet checksums.
 */

#include <kernel/laylaos.h>

STATIC_INLINE uint32_t chksum(void *addr, int len)
{
    volatile uint16_t w;
    uint16_t *p = addr;
    uint32_t acc = 0;
    
    while(len > 1)
    {
        w = *p;
        acc += w;
        p++;
        len -= 2;
    }
    
    // account for a leftover byte
    if(len)
    {
        volatile uint8_t b = *(volatile uint8_t *)p;
        acc += b;
    }
    
    return acc;
}


STATIC_INLINE uint16_t inet_chksum(uint16_t *addr, int len, uint32_t start)
{
    volatile uint32_t acc = start;

    acc += chksum(addr, len);

    while(acc >> 16)
    {
        acc = (acc & 0xFFFF) + (acc >> 16);
    }

    return (uint16_t) ~(acc & 0xFFFF);
}


STATIC_INLINE
uint16_t __udp_v4_checksum(struct packet_t *p, uint32_t src, uint32_t dest, uint16_t proto)
{
    uint32_t acc = 0;

    // add the source and destination as two 16-bit entities each, as a broadcast
    // address (0xffffffff) would result in carry, which will be truncated when
    // we perform 32-bit addition
    acc += (src >> 16) & 0xffff;
    acc += src & 0xffff;

    acc += (dest >> 16) & 0xffff;
    acc += dest & 0xffff;

    acc += htons(proto);
    acc += htons(p->count);

    return inet_chksum((uint16_t *)p->data, p->count, acc);
}


STATIC_INLINE uint16_t udp_v4_checksum(struct packet_t *p, uint32_t src, uint32_t dest)
{
    return __udp_v4_checksum(p, src, dest, IPPROTO_UDP);
}


STATIC_INLINE uint16_t tcp_v4_checksum(struct packet_t *p, uint32_t src, uint32_t dest)
{
    return __udp_v4_checksum(p, src, dest, IPPROTO_TCP);
}

