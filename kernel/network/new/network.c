/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: network.c
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
 *  \file network.c
 *
 *  Core network driver code.
 */

//#define __DEBUG
#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/net/protocol.h>
#include <kernel/net/netif.h>
#include <kernel/net/packet.h>
#include <kernel/net/ether.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/udp.h>
#include <kernel/net/tcp.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/icmp6.h>
#include <kernel/net/raw.h>
#include <kernel/net/notify.h>
#include <kernel/user.h>

struct netstats netstats = { 0, };

struct task_t *network_task = NULL;
struct task_t *network_dev_task = NULL;

void network_task_func(void *arg);
void network_dev_task_func(void *arg);
void stats_init(void);


void network_init(void)
{
    printk("Initializing network protocols..\n");
    proto_init();

    // fork the network task
    (void)start_kernel_task("network", network_task_func, NULL,
                            &network_task, 0);
    (void)start_kernel_task("netdev", network_dev_task_func, NULL,
                            &network_dev_task, 0);

    // attach pseudo-devices
    printk("Attaching pseudo-devices..\n");
    loop_attach();

    // init stats
    printk("Initializing network statistics..\n");
    stats_init();
}


int get_netstats(struct netstats *ns)
{
    struct netstats tmp;
    
    if(!ns)
    {
        return -EINVAL;
    }
    
    A_memcpy(&tmp, &netstats, sizeof(struct netstats));
    return copy_to_user(ns, &tmp, sizeof(struct netstats));
}


void stats_init(void)
{
    static int inited = 0;
    
    if(inited)
    {
        return;
    }
    
    inited = 1;
    A_memset(&netstats, 0, sizeof(struct netstats));
}


void transport_enqueue_in(struct packet_t *p, uint8_t proto, int is_ipv6)
{
    /*
     * IP packets are delivered to the transport layer with the IP header
     * intact as it contains information some protocols need to process
     * the packet. It is the protocol's responsibility to remove the IP
     * header from the packet to get to the protocol's header.
     */
    
    struct netif_queue_t *q;

    switch(proto)
    {
        case IPPROTO_ICMP:
            q = &icmp4_inq;
            break;

        case IPPROTO_ICMPV6:
            q = &icmp6_inq;
            break;

        case IPPROTO_TCP:
            q = &tcp_inq;
            break;

        case IPPROTO_UDP:
            q = &udp_inq;
            break;

        case IPPROTO_RAW:
            q = &raw_inq;
            break;
        
        default:
            KDEBUG("ipv%d: dropping packet with unknown protocol (%d)\n", 
                    is_ipv6 ? 6 : 4, proto);
            notify_proto_unreachable(p, is_ipv6);
            packet_free(p);
            return;
    }

    kernel_mutex_lock(&q->lock);

    if(IFQ_FULL(q))
    {
        kernel_mutex_unlock(&q->lock);
        notify_proto_unreachable(p, is_ipv6);
        packet_free(p);
    }
    else
    {
        IFQ_ENQUEUE(q, p);
        kernel_mutex_unlock(&q->lock);
    }
}


void device_process_input(void)
{
    struct netif_t *ifp;

    for(ifp = netif_list; ifp != NULL; ifp = ifp->next)
    {
        if(ifp->process_input)
        {
            ifp->process_input(ifp);
        }
    }
}


void device_process_output(void)
{
    struct netif_t *ifp;

    for(ifp = netif_list; ifp != NULL; ifp = ifp->next)
    {
        if(ifp->process_output)
        {
            ifp->process_output(ifp);
        }
    }
}


void process_queue(struct netif_queue_t *q, int (*f)(struct packet_t *p))
{
    struct packet_t *p;
    int i = 0;

    while(1)
    {
        kernel_mutex_lock(&q->lock);
        IFQ_DEQUEUE(q, p);
        kernel_mutex_unlock(&q->lock);
        
        if(!p)
        {
            break;
        }
        
        f(p);
        
        if(++i >= 32)
        {
            break;
        }
    }
}


void network_dev_task_func(void *arg)
{
    UNUSED(arg);

    for(;;)
    {
        KDEBUG("network_dev_task_func: processing devices in\n");
        device_process_input();
        KDEBUG("network_dev_task_func: processing eth in\n");
        process_queue(&ethernet_inq, ethernet_receive);

        KDEBUG("network_dev_task_func: processing eth out\n");
        process_queue(&ethernet_outq, ethernet_send);
        KDEBUG("network_dev_task_func: processing devices out\n");
        device_process_output();

        KDEBUG("network_dev_task_func: sleeping\n");
        block_task2(&network_dev_task, PIT_FREQUENCY);
    }
}


/*
 * Network stack task function.
 */
void network_task_func(void *arg)
{
    UNUSED(arg);

    for(;;)
    {
        //KDEBUG("network_task_func: processing devices in\n");
        //device_process_input();
        //KDEBUG("network_task_func: processing eth in\n");
        //process_queue(&ethernet_inq, ethernet_receive);
        ////arp_process_input();
        KDEBUG("network_task_func: processing ipv4 in\n");
        process_queue(&ipv4_inq, ipv4_receive);
        KDEBUG("network_task_func: processing ipv6 in\n");
        process_queue(&ipv6_inq, ipv6_receive);
        KDEBUG("network_task_func: processing udp in\n");
        process_queue(&udp_inq, udp_receive);
        KDEBUG("network_task_func: processing tcp in\n");
        process_queue(&tcp_inq, tcp_receive);
        KDEBUG("network_task_func: processing icmp4 in\n");
        process_queue(&icmp4_inq, icmp4_receive);
        KDEBUG("network_task_func: processing icmp6 in\n");
        process_queue(&icmp6_inq, icmp6_receive);
        //socket_process_input();

        //KDEBUG("network_task_func: processing sockets\n");
        ////socket_process_output();
        //socket_process_outqueues();
        ////tcp_process_output();
        //KDEBUG("network_task_func: processing tcp out\n");
        //process_queue(&tcp_outq, tcp_process_out);
        //KDEBUG("network_task_func: processing udp out\n");
        //process_queue(&udp_outq, udp_process_out);
        KDEBUG("network_task_func: processing ipv6 out\n");
        process_queue(&ipv6_outq, ipv6_process_out);
        KDEBUG("network_task_func: processing ipv4 out\n");
        process_queue(&ipv4_outq, ipv4_process_out);
        ////arp_process_output();
        //KDEBUG("network_task_func: processing eth out\n");
        //process_queue(&ethernet_outq, ethernet_send);
        //KDEBUG("network_task_func: processing devices out\n");
        //device_process_output();
        
        KDEBUG("network_task_func: sleeping\n");
        block_task2(&network_task, PIT_FREQUENCY);
    }
}

