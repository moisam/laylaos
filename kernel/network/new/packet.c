/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: packet.c
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
 *  \file packet.c
 *
 *  Helper functions for working with network packets.
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/net/packet.h>
#include <mm/kheap.h>


struct packet_t *packet_alloc(size_t len, int type)
{
    struct packet_t *p;
    static size_t min_size = sizeof(struct packet_t) + 20 + IPv6_HLEN + ETHER_HLEN;
    size_t offset = 0;
    
    switch(type)
    {
        case PACKET_LINK:
            offset = ETHER_HLEN;
            break;

        case PACKET_IP:
            offset = IPv6_HLEN + ETHER_HLEN;
            break;

        case PACKET_TRANSPORT:
            offset = 20 + IPv6_HLEN + ETHER_HLEN;
            break;

        case PACKET_RAW:
            offset = 0;
            break;

        default:
            printk("net: unknown package type: 0x%x\n", type);
            return NULL;
    }

    if(!(p = kmalloc(len + min_size)))
    {
        return NULL;
    }
    
    A_memset(p, 0, sizeof(struct packet_t));
    /*
    p->flags = 0;
    p->ifp = NULL;
    p->next = NULL;
    p->sock = NULL;
    p->nfailed = 0;
    */

    p->count = len;
    p->malloced = len + min_size;
    p->data = (void *)((char *)p + sizeof(struct packet_t) + offset);
    /*
    p->transport_hdr = NULL;
    p->free_packet = NULL;
    */
    
    return p;
}


struct packet_t *packet_duplicate(struct packet_t *p)
{
    struct packet_t *copy;
    
    if(!p || !p->malloced)
    {
        return NULL;
    }
    
    if(!(copy = kmalloc(p->malloced)))
    {
        return NULL;
    }
    
    A_memcpy(copy, p, p->malloced);
    
    //copy->flags = p->flags;
    //copy->frag = p->frag;
    //copy->transport_flags_saved = p->transport_flags_saved;
    //copy->ifp = p->ifp;
    copy->next = NULL;
    //copy->sock = p->sock;
    copy->nfailed = 0;

    //copy->count = p->count;
    //copy->malloced = p->malloced;
    copy->data = (void *)((uintptr_t)copy + 
                            ((uintptr_t)p->data - (uintptr_t)p));
    
    if(p->transport_hdr)
    {
        copy->transport_hdr = (void *)((uintptr_t)copy + 
                            ((uintptr_t)p->transport_hdr - (uintptr_t)p));
    }
    /*
    else
    {
        copy->transport_hdr = NULL;
    }
    
    A_memcpy(&copy->remote_addr, &p->remote_addr, sizeof(p->remote_addr));
    copy->remote_port = p->remote_port;
    */
    
    // XXX
    //copy->free_packet = p->free_packet;
    copy->free_packet = NULL;
    
    return copy;
}


void packet_free(struct packet_t *p)
{
    //struct packet_t *next;
    
    if(!p)
    {
        return;
    }
    
    //while(p)
    {
        //next = p->next;
    
        // some modules (e.g. ne2000) have their own packet free functions
        if(p->free_packet)
        {
            p->free_packet(p);
        }
        else
        {
            kfree(p);
        }

        //p = next;
    }
}


int packet_add_header(struct packet_t *p, size_t hdr_len)
{
    char *data = (char *)p->data - hdr_len;
    char *not_below = (char *)p + sizeof(struct packet_t);
    
    if(data < not_below)
    {
        //printk("net: no space for header in package\n");
        return -ENOBUFS;
    }
    
    p->count += hdr_len;
    p->data = (void *)((char *)p->data - hdr_len);
    
    return 0;
}

