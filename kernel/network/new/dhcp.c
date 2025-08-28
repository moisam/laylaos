/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: dhcp.c
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
 *  \file dhcp.c
 *
 *  Dynamic Host Configuration Protocol (DHCP) implementation.
 */

//#define __DEBUG

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <endian.h>
#include <kernel/asm.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/timer.h>
#include <kernel/clock.h>
#include <kernel/mutex.h>
#include <kernel/select.h>
#include <kernel/net.h>
#include <kernel/net/socket.h>
#include <kernel/net/dhcp.h>
#include <kernel/net/ether.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/route.h>
#include <kernel/net/arp.h>
#include <kernel/net/udp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/nettimer.h>
#include <kernel/net/checksum.h>
#include <mm/kheap.h>

#include "../../kernel/task_funcs.c"

extern volatile uint16_t ip_id;

static int dhcp_state_transition(struct dhcp_binding_t *binding, int req_state);
static void dhcp_check(struct dhcp_binding_t *binding);
static void dhcp_bind(struct dhcp_binding_t *binding);
static void dhcp_handle_offer(struct dhcp_binding_t *binding);
static void dhcp_handle_nak(struct dhcp_binding_t *binding);
static void dhcp_handle_ack(struct dhcp_binding_t *binding);
static int dhcp_alloc_msg(struct dhcp_binding_t *binding);
static void dhcp_renewing_timeout(void *arg);
static void dhcp_rebinding_timeout(void *arg);
static void dhcp_declining_timeout(void *arg);
static void dhcp_requesting_timeout(void *arg);
static void dhcp_checking_timeout(void *arg);
static void dhcp_t1_timeout(void *arg);
static void dhcp_t2_timeout(void *arg);
static void dhcp_lease_timeout(void *arg);
static void dhcp_task_func(void *unused);
static void dhcp_sock_func(void *unused);

/*
 * Client-server interaction to alloate an IP address:
 *   -> Client broadcasts a DHCPDISCOVER msg
 *   <- Server(s) respond with DHCPOFFER msg
 *     Suggested IP address will be in the yiaddr field
 *     Other config options may be included as well
 *   -> Client responds with a DHCPREQUEST to either:
 *     (a) accept server's offer (and decline others)
 *     (b) confirm correctness of information, e.g. after reboot
 *     (c) extend the lease on an IP address
 *   <- Server responds with a DHCPACK msg to confirm address allocation
 *   <- Server responds with a DHCPNAK msg to indicate incorrect info or
 *      lease expiry
 *   -> Client may send DHCPDECLINE to indicate address is in use
 *   -> Client may send DHCPRELEASE to release address and cancel lease
 *   -> Client can ask for config params by sending a DHCPINFORM msg
 */

int dhcp_tasks = 0;
struct dhcp_binding_t *dhcp_bindings = NULL;
volatile struct kernel_mutex_t dhcp_lock;
volatile struct task_t *dhcp_sock_task = NULL;
struct socket_t *dhcp_sock = NULL;

#define FIX_PACKET_LEN(p, l)                        \
    (p)->count = sizeof(struct dhcp_msg_t) + (l);

#define dhcp_discover(b)            \
    dhcp_state_transition((struct dhcp_binding_t *)(b), DHCP_SELECTING)
#define dhcp_select(b)              \
    dhcp_state_transition((struct dhcp_binding_t *)(b), DHCP_REQUESTING)
#define dhcp_release(b)             \
    dhcp_state_transition((struct dhcp_binding_t *)(b), DHCP_RELEASING)
#define dhcp_rebind(b)              \
    dhcp_state_transition((struct dhcp_binding_t *)(b), DHCP_REBINDING)
#define dhcp_decline(b)             \
    dhcp_state_transition((struct dhcp_binding_t *)(b), DHCP_DECLINING)
#define dhcp_renew(b)               \
    dhcp_state_transition((struct dhcp_binding_t *)(b), DHCP_RENEWING)

#define DHCP_CAP_TRIES          16


/*
 * Initialize DHCP.
 */
void dhcp_init(void)
{
    int res;

    init_kernel_mutex(&dhcp_lock);

    res = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &dhcp_sock);

    if(res < 0)
    {
        printk("dhcp: failed to create socket (err %d)\n", res);
    }
    else
    {
        dhcp_sock->local_addr.ipv4 = 0;
        dhcp_sock->remote_addr.ipv4 = 0;
        dhcp_sock->local_port = DHCP_CLIENT_PORT;
        dhcp_sock->remote_port = DHCP_SERVER_PORT;

        (void)start_kernel_task("dhcp", dhcp_sock_func, NULL, &dhcp_sock_task, 0);
    }
}


static inline void dhcp_add_option(struct dhcp_binding_t *binding,
                                   struct dhcp_msg_t *msg,
                                   uint8_t type, uint8_t len)
{
    uint8_t *opts = (uint8_t *)msg + sizeof(struct dhcp_msg_t);
    opts[binding->out_opt_len++] = type;
    opts[binding->out_opt_len++] = len;
}


static inline void dhcp_add_optionb(struct dhcp_binding_t *binding,
                                    struct dhcp_msg_t *msg,
                                    uint8_t type, uint8_t len, uint8_t val)
{
    dhcp_add_option(binding, msg, type, len);
    
    uint8_t *opts = (uint8_t *)msg + sizeof(struct dhcp_msg_t);
    opts[binding->out_opt_len++] = val;
}


static inline void dhcp_add_options(struct dhcp_binding_t *binding,
                                    struct dhcp_msg_t *msg,
                                    uint8_t type, uint8_t len, uint16_t val)
{
    dhcp_add_option(binding, msg, type, len);

    uint8_t *opts = (uint8_t *)msg + sizeof(struct dhcp_msg_t);
    opts[binding->out_opt_len++] = (uint8_t)((val & 0xFF00) >> 8);
    opts[binding->out_opt_len++] = (uint8_t)((val & 0x00FF) >> 0);
}


static inline void dhcp_add_optionl(struct dhcp_binding_t *binding,
                                    struct dhcp_msg_t *msg,
                                    uint8_t type, uint8_t len, uint32_t val)
{
    dhcp_add_option(binding, msg, type, len);

    uint8_t *opts = (uint8_t *)msg + sizeof(struct dhcp_msg_t);
    opts[binding->out_opt_len++] = (uint8_t)((val & 0xFF000000) >> 24);
    opts[binding->out_opt_len++] = (uint8_t)((val & 0x00FF0000) >> 16);
    opts[binding->out_opt_len++] = (uint8_t)((val & 0x0000FF00) >> 8);
    opts[binding->out_opt_len++] = (uint8_t)((val & 0x000000FF) >> 0);
}


static inline void dhcp_add_option_cid(struct dhcp_binding_t *binding,
                                       struct dhcp_msg_t *msg)
{
    uint8_t i;
    
    dhcp_add_option(binding, msg, DHCP_OPTION_DHCP_CLIENT_IDENTIFIER,
                    ETHER_ADDR_LEN + 1);

    uint8_t *opts = (uint8_t *)msg + sizeof(struct dhcp_msg_t);
    
    opts[binding->out_opt_len++] = 1;       // ethernet address

    for(i = 0; i < ETHER_ADDR_LEN; i++)
    {
        opts[binding->out_opt_len++] = binding->ifp->hwaddr[i];
    }
}


static inline void dhcp_add_option_paramlist(struct dhcp_binding_t *binding,
                                             struct dhcp_msg_t *msg)
{
    dhcp_add_option(binding, msg, DHCP_OPTION_DHCP_PARAMETER_REQUEST_LIST, 13);

    uint8_t *opts = (uint8_t *)msg + sizeof(struct dhcp_msg_t);
    
    opts[binding->out_opt_len++] = DHCP_OPTION_SUBNET_MASK;
    opts[binding->out_opt_len++] = DHCP_OPTION_TIME_OFFSET;
    opts[binding->out_opt_len++] = DHCP_OPTION_ROUTERS;
    opts[binding->out_opt_len++] = DHCP_OPTION_DOMAIN_NAME_SERVERS;
    opts[binding->out_opt_len++] = DHCP_OPTION_HOST_NAME;
    opts[binding->out_opt_len++] = DHCP_OPTION_DOMAIN_NAME;
    opts[binding->out_opt_len++] = DHCP_OPTION_INTERFACE_MTU;
    opts[binding->out_opt_len++] = DHCP_OPTION_BROADCAST_ADDRESS;
    opts[binding->out_opt_len++] = DHCP_OPTION_STATIC_ROUTES;
    opts[binding->out_opt_len++] = DHCP_OPTION_NIS_DOMAIN;
    opts[binding->out_opt_len++] = DHCP_OPTION_NIS_SERVERS;
    opts[binding->out_opt_len++] = DHCP_OPTION_NTP_SERVERS;
    opts[binding->out_opt_len++] = DHCP_OPTION_ROOT_PATH;
}


static inline void dhcp_add_option_end(struct dhcp_binding_t *binding,
                                       struct dhcp_msg_t *msg)
{
    volatile int len;
    uint8_t *opts = (uint8_t *)msg + sizeof(struct dhcp_msg_t);
    
    opts[binding->out_opt_len++] = DHCP_OPTION_END;
    len = binding->out_opt_len;

    // pad to min packet size, and make sure it is 4-byte aligned
    while(len < DHCP_MIN_OPTIONS_LEN || (len & 3))
    {
        opts[len++] = 0;
    }
    
    binding->out_opt_len = len;
}


static inline uint8_t dhcp_get_optionb(uint8_t *p)
{
    return *p;
}


static inline uint16_t dhcp_get_options(uint8_t *p)
{
    uint16_t val;
    val = (p[0] << 8) | p[1];
    return val;
}


static inline uint32_t dhcp_get_optionl(uint8_t *p)
{
    uint32_t val;
    val = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    return val;
}


static void dhcp_set_state(struct dhcp_binding_t *binding, int state)
{
    if(state != binding->state)
    {
        binding->state = state;
        binding->tries = 0;
    }
}


static struct dhcp_binding_t *dhcp_binding_by_netif(struct netif_t *ifp)
{
    struct dhcp_binding_t *binding = dhcp_bindings;
    
    while(binding)
    {
        if(binding->ifp == ifp)
        {
            return binding;
        }
        
        binding = binding->next;
    }
    
    return NULL;
}


static struct dhcp_binding_t *dhcp_binding_by_task(volatile struct task_t *task)
{
    struct dhcp_binding_t *binding = dhcp_bindings;
    
    while(binding)
    {
        if(binding->task == task)
        {
            return binding;
        }
        
        binding = binding->next;
    }
    
    return NULL;
}


/*
 * Start DHCP.
 */
struct dhcp_binding_t *dhcp_start(struct netif_t *ifp)
{
    struct dhcp_binding_t *binding, *tmp;
    int res;
    pid_t pid;
    char buf[8];
    
    if((binding = dhcp_binding_by_netif(ifp)) != NULL)
    {
        printk("dhcp: already active on interface %s\n", ifp->name);
        
        // restart the negotiation
        if((res = dhcp_discover(binding)) < 0)
        {
            return NULL;
        }
        
        return binding;
    }

    if(!(binding = kmalloc(sizeof(struct dhcp_binding_t))))
    {
        return NULL;
    }

    A_memset(binding, 0, sizeof(struct dhcp_binding_t));

    binding->ifp = ifp;
    binding->xid = now();

    ksprintf(buf, sizeof(buf), "dhcp%d", dhcp_tasks++);
    pid = start_kernel_task(buf, dhcp_task_func, NULL, &binding->task,
                            0 /* KERNEL_TASK_ELEVATED_PRIORITY */);
    binding->task = get_task_by_id(pid);
    
    kernel_mutex_lock(&dhcp_lock);
    
    if(dhcp_bindings == NULL)
    {
        dhcp_bindings = binding;
    }
    else
    {
        for(tmp = dhcp_bindings; tmp->next != NULL; tmp = tmp->next)
        {
            ;
        }
        
        tmp->next = binding;
    }
    
    kernel_mutex_unlock(&dhcp_lock);
    
    dhcp_discover(binding);

    return binding;
}


static void udp_send(struct netif_t *ifp, struct packet_t *p,
                     uint32_t src, uint32_t dest)
{
    struct udp_hdr_t *udph;
    struct ipv4_hdr_t *iph;
    uint8_t dest_ethernet[ETHER_ADDR_LEN];
    int res;

    if(dest == 0xffffffff)
    {
        for(res = 0; res < ETHER_ADDR_LEN; res++)
        {
            dest_ethernet[res] = 0xff;
        }
    }
    else
    {
        if(!arp_to_eth(dest, dest_ethernet))
        {
            printk("dhcp: failed to resolve server Ethernet address\n");
            free_packet(p);
            return;
        }
    }

    iph = IPv4_HDR(p);
    udph = UDP_HDR(p);

    packet_add_header(p, UDP_HLEN);
    udph->len = htons(p->count);
    udph->srcp = DHCP_CLIENT_PORT;
    udph->destp = DHCP_SERVER_PORT;
    udph->checksum = 0;
    udph->checksum = udp_v4_checksum(p, src, dest);

    packet_add_header(p, IPv4_HLEN);
    iph->ttl = IPDEFTTL;
    iph->proto = IPPROTO_UDP;
    iph->src = src;
    iph->dest = dest;
    iph->tos = 0;
    iph->ver = 4;
    iph->hlen = 5;     // hlen of 5 (=20 bytes)
    iph->tlen = htons((uint16_t)p->count);
    iph->offset = htons(IP_DF);
    iph->id = htons(ip_id);
    ip_id++;
    iph->checksum = 0;
    iph->checksum = inet_chksum((uint16_t *)p->data, IPv4_HLEN, 0);

    if((res = ethernet_send(ifp, p, dest_ethernet)) < 0)
    {
        printk("dhcp: failed to send packet (err %d)\n", res);
    }
}


int dhcp_state_transition(struct dhcp_binding_t *binding, int req_state)
{
    static int msg_type[] =
    {
        // DHCP msg type              req_state
        0,                      // 0  invalid state
        DHCP_REQUEST,           // 1  DHCP_REQUESTING
        0,                      // 2  DHCP_INIT
        0,                      // 3  DHCP_REBOOTING
        DHCP_REQUEST,           // 4  DHCP_REBINDING
        DHCP_REQUEST,           // 5  DHCP_RENEWING
        DHCP_DISCOVER,          // 6  DHCP_SELECTING
        0,                      // 7  DHCP_INFORMING
        0,                      // 8  DHCP_CHECKING
        0,                      // 9  DHCP_PERMANENT
        0,                      // 10 DHCP_BOUND
        DHCP_DECLINE,           // 11 DHCP_DECLINING
        DHCP_RELEASE,           // 12 DHCP_RELEASING
    };
    
    int res;
    int secs = 0, tries;
    struct dhcp_msg_t *msg;

	//KDEBUG("dhcp_state_transition: req_state %d\n", req_state);
	//__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(req_state == DHCP_SELECTING)
    {
        binding->ipaddr = 0;
    }

    if(req_state != DHCP_REQUESTING)
    {
        binding->xid++;
    }
    
    tries = DHCP_CAP_TRIES;

    dhcp_set_state(binding, req_state);
    
    if((res = dhcp_alloc_msg(binding)) == 0)
    {
        msg = (struct dhcp_msg_t *)binding->out_packet->data;
        dhcp_add_optionb(binding, msg, DHCP_OPTION_DHCP_MESSAGE_TYPE, 1,
                            msg_type[req_state]);

        dhcp_add_option_cid(binding, msg);
        dhcp_add_option_paramlist(binding, msg);

        dhcp_add_options(binding, msg, DHCP_OPTION_DHCP_MAX_MESSAGE_SIZE,
                         2, 576);

        if(req_state == DHCP_SELECTING || req_state == DHCP_REQUESTING)
        {
            msg->flags = htons(DHCP_BROADCAST_FLAG);
            // this MUST be cleared during the discovery phase
            msg->ciaddr = 0;
        }
        else if(req_state == DHCP_REBINDING)
        {
            msg->flags = htons(DHCP_BROADCAST_FLAG);
        }

        if(req_state == DHCP_REQUESTING)
        {
            dhcp_add_optionl(binding, msg, DHCP_OPTION_DHCP_REQUESTED_ADDRESS,
                                4, htonl(binding->ipaddr));
            dhcp_add_optionl(binding, msg, DHCP_OPTION_DHCP_SERVER_IDENTIFIER,
                                4, htonl(binding->saddr));
        }

        dhcp_add_option_end(binding, msg);
        FIX_PACKET_LEN(binding->out_packet, binding->out_opt_len);
        
        if(req_state == DHCP_REQUESTING)
        {
            udp_send(binding->ifp, binding->out_packet, 0x00, 0xffffffff);
            binding->out_packet = NULL;
        }
        else if(req_state == DHCP_RENEWING || req_state == DHCP_RELEASING)
        {
            // this should be unicast to the DHCP server, but we 
            // shouldn't include a 'server identifier' in the msg, 
            // according to RFC 2131
            udp_send(binding->ifp, binding->out_packet, binding->ipaddr, binding->saddr);
            binding->out_packet = NULL;
        }
        else
        {
            udp_send(binding->ifp, binding->out_packet, binding->ipaddr, 0xffffffff);
            binding->out_packet = NULL;
        }
    }
    
    binding->tries++;
    
    // roundup
    if(binding->tries == 0)
    {
        binding->tries = 1;
    }
    
    // In case of RENEWING and REBINDING, schedule timeout only once. Why?
    //   - For RENEWING, we wait 1/2 of T2, then retransmit the request.
    //     The next timeout will be at least T2, at which point we need
    //     to move to the REBINDING state.
    //   - For REBINDING, we wait 1/2 of the lease time, then retransmit.
    //     The next timeout will be at least the duration of the lease,
    //     at which point we should move to the INIT state and we reinit
    //     discovery.
    if(req_state == DHCP_RENEWING && tries == 1)
    {
        // RFC 2131 says in the RENEWING state, we should wait 1/2 of the
        // remaining time until T2, down to a minimum of 60 seconds, before
        // retransmitting the DHCPREQUEST message
        secs = (binding->ut2 >> 1) - now();
        secs = (secs < 60) ? 60 : secs;

        if(secs)
        {
            nettimer_release(binding->dhcp_renewing_timer);
            binding->dhcp_renewing_timer = 
                    nettimer_add(secs * PIT_FREQUENCY, dhcp_renewing_timeout, binding);
        }
    }
    else if(req_state == DHCP_REBINDING && tries == 1)
    {
        // RFC 2131 says in the REBINDING state, we should wait 1/2 of the
        // remaining lease time, down to a minimum of 60 seconds, before
        // retransmitting the DHCPREQUEST message
        secs = (binding->ulease >> 1) - now();
        secs = (secs < 60) ? 60 : secs;

        if(secs)
        {
            nettimer_release(binding->dhcp_rebinding_timer);
            binding->dhcp_rebinding_timer = 
                    nettimer_add(secs * PIT_FREQUENCY, dhcp_rebinding_timeout, binding);
        }
    }
    else if(req_state == DHCP_DECLINING)
    {
        // RFC 2131 says we should wait 10 secs when we decline an offer
        // before restarting the configuration process
        secs = 10;

        if(secs)
        {
            nettimer_release(binding->dhcp_declining_timer);
            binding->dhcp_declining_timer = 
                    nettimer_add(secs * PIT_FREQUENCY, dhcp_declining_timeout, binding);
        }
    }
    else if(req_state == DHCP_RELEASING)
    {
        secs = 0;
    }
    else if(req_state == DHCP_REQUESTING || req_state == DHCP_SELECTING)
    {
        // RFC 2131 Section 4.1 suggests the client should delay
        // retransmissions, allowing time for server response. The first
        // delay should be 4 secs, then 8 secs, then double up to a maximum
        // of 64 secs. We simply multiply the number of tries by 4, capping
        // it at 64 secs.
        //
        // NOTE: RFC 2131 says we should randomize retransmissions 'by the
        //       value of a uniform random number chosen from the range
        //       -1 to +1.' We currently ignore this :(
        secs = MIN(binding->tries, tries) * 4;

        if(secs)
        {
            nettimer_release(binding->dhcp_requesting_timer);
            binding->dhcp_requesting_timer = 
                    nettimer_add(secs * PIT_FREQUENCY, dhcp_requesting_timeout, binding);
        }
    }
    
    // we need to record our current time if we are in the DHCPDISCOVER or
    // the DHCPRENEW state, so that we can calculate lease expiration time
    if(req_state == DHCP_SELECTING || req_state == DHCP_RENEWING)
    {
        binding->binding_time = now();
    }
    
    if(req_state == DHCP_RELEASING)
    {
        //route_free_ipv4(binding->ipaddr, binding->gateway, binding->netmask);
        route_free_for_ifp(binding->ifp);

        // NOTE: do we need to do this?
        remove_arp_entry(binding->saddr);
    }

    return res;
}


void dhcp_check(struct dhcp_binding_t *binding)
{
    int ticks;

    printk("dhcp: sending APR request for ip 0x%x\n", htonl(binding->ipaddr));
    arp_request(binding->ifp, 0x00, binding->ipaddr);

    dhcp_set_state(binding, DHCP_CHECKING);
    binding->tries++;

    // roundup
    if(binding->tries == 0)
    {
        binding->tries = 1;
    }

    // wait in 500ms increments
    ticks = MIN(binding->tries, DHCP_CAP_TRIES) * 1 * (PIT_FREQUENCY / 2);

    nettimer_release(binding->dhcp_checking_timer);
    binding->dhcp_checking_timer = nettimer_add(ticks, dhcp_checking_timeout, binding);
}


void dhcp_bind(struct dhcp_binding_t *binding)
{
    uint32_t netmask, gateway;

    dhcp_set_state(binding, DHCP_BOUND);
    
    // For each of T1, T2 and the lease time, check we have a valid time
    // and its not infinity, then start a timer for each
    if(binding->t1 != 0 && binding->t1 != 0xFFFFFFFF)
    {
        nettimer_add(binding->t1 * PIT_FREQUENCY, dhcp_t1_timeout, binding);
    }

    if(binding->t2 != 0 && binding->t2 != 0xFFFFFFFF)
    {
        nettimer_add(binding->t2 * PIT_FREQUENCY, dhcp_t2_timeout, binding);
    }

    if(binding->lease != 0 && binding->lease != 0xFFFFFFFF)
    {
        nettimer_add(binding->lease * PIT_FREQUENCY, dhcp_lease_timeout, binding);
    }

    netmask = binding->netmask;
    gateway = binding->gateway;

    // If no subnet mask was provided, pick a mask according to the
    // network class
    if(netmask == 0)
    {
        uint8_t msb = (uint8_t)(binding->ipaddr & 0xFF000000);

        if(msb <= 127)
        {
            netmask = htonl(0xFF000000);
        }
        else if(msb >= 192)
        {
            netmask = htonl(0xFFFFFF00);
        }
        else
        {
            netmask = htonl(0xFFFF0000);
        }
    }

    // If no gateway was provided, pick one
    if(gateway == 0)
    {
        gateway = binding->ipaddr;
        gateway &= netmask;
        gateway |= htonl(0x01000000);
    }

    route_free_for_ifp(binding->ifp);
    route_add_ipv4(binding->ipaddr, gateway, netmask, RT_HOST, 0, binding->ifp);
    route_add_ipv4(0, binding->gateway, 0, RT_GATEWAY, 0, binding->ifp);

    printk("dhcp: addr binding->ipaddr 0x%x, netmask 0x%x, gateway 0x%x\n",
            htonl(binding->ipaddr), 
            htonl(netmask), 
            htonl(gateway));

    arp_set_expiry(binding->saddr, 0);
}


/*
 * Handle ARP reply.
 */
void dhcp_arp_reply(uint32_t addr)
{
    struct dhcp_binding_t *binding = dhcp_bindings;
    
    while(binding)
    {
        if(binding->state == DHCP_CHECKING)
        {
            if(addr == binding->ipaddr)
            {
                dhcp_decline(binding);
            }
        }
        
        binding = binding->next;
    }
}


/*
 * Search the packet's options (if there are any) for the given
 * option and return a pointer to the first character in the option.
 * If the 'standard' options field doesn't contain the option we're
 * looking for, we look into the file and sname fields.
 */
static uint8_t *dhcp_option_ptr(struct dhcp_msg_t *msg,
                                uint8_t *opts, size_t opts_len,
                                uint8_t type)
{
    uint8_t overload = DHCP_OVERLOAD_NONE;
    size_t offset = 0;
    
    if(opts == NULL || opts_len == 0)
    {
        return NULL;
    }
    
    while((offset < opts_len) && (opts[offset] != DHCP_OPTION_END))
    {
        // check if the sname and/or the file fields are overloaded
        // with options
        if(opts[offset] == DHCP_OPTION_DHCP_OPTION_OVERLOAD)
        {
            offset += 2;
            overload = opts[offset++];
        }
        else if(opts[offset] == type)
        {
            return &opts[offset];
        }
        else if(opts[offset] == DHCP_OPTION_PAD)
        {
            offset++;
        }
        else
        {
            offset++;
            offset += 1 + opts[offset];
        }
    }
    
    // check if the message is overloaded
    if(overload == DHCP_OVERLOAD_NONE)
    {
        return NULL;
    }
    
    if(overload == DHCP_OVERLOAD_FILE)
    {
        opts = msg->file;
        opts_len = 128;
    }
    else if(overload == DHCP_OVERLOAD_SNAME)
    {
        opts = msg->sname;
        opts_len = 64;
    }
    else
    {
        // If both sname and file are overloaded, RFC 2131 says we should
        // check file first, followed by sname. We definitely are doing
        // it the wrong way here (because it is simple - look at the DHCP msg's
        // structure in dhcp.h). FIXME!
        opts = msg->sname;
        opts_len = 128 + 64;
    }
    
    offset = 0;

    while((offset < opts_len) && (opts[offset] != DHCP_OPTION_END))
    {
        if(opts[offset] == type)
        {
            return &opts[offset];
        }
        else if(opts[offset] == DHCP_OPTION_PAD)
        {
            offset++;
        }
        else
        {
            offset++;
            offset += 1 + opts[offset];
        }
    }
    
    return NULL;
}


static void dhcp_sock_func(void *unused)
{
    struct packet_t *p;
    struct dhcp_binding_t *binding;

    UNUSED(unused);
    
    while(1)
    {
        selrecord(&(dhcp_sock->selrecv));
        block_task(&dhcp_sock_task, 1);

        SOCKET_LOCK(dhcp_sock);

#define DROP_PACKET(p)      \
    free_packet(p);         \
    SOCKET_LOCK(dhcp_sock); \
    continue;

        while(dhcp_sock->inq.head)
        {
            IFQ_DEQUEUE(&dhcp_sock->inq, p);
            SOCKET_UNLOCK(dhcp_sock);

            if(!(binding = dhcp_binding_by_netif(p->ifp)))
            {
                printk("dhcp: cannot find binding -- dropping packet\n");
                DROP_PACKET(p);
            }

            struct dhcp_msg_t *msg = (struct dhcp_msg_t *)p->data;
            uint8_t *opts = NULL, *opt_ptr;
            size_t opts_len = 0;

            if(msg->op != DHCP_BOOTP_REPLY)
            {
                printk("dhcp: invalid op -- dropping packet\n");
                DROP_PACKET(p);
            }

            if(memcmp(binding->ifp->hwaddr, msg->chaddr, ETHER_ADDR_LEN) != 0)
            {
                printk("dhcp: invalid Ethernet address -- dropping packet\n");
                DROP_PACKET(p);
            }

            if(ntohl(msg->xid) != binding->xid)
            {
                printk("dhcp: invalid xid -- dropping packet\n");
                DROP_PACKET(p);
            }

            binding->in_packet = p;

            // check if the packet has options
            if(p->count > sizeof(struct dhcp_msg_t))
            {
                opts = (uint8_t *)p->data + sizeof(struct dhcp_msg_t);
                opts_len = p->count - sizeof(struct dhcp_msg_t);
            }

            if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_DHCP_MESSAGE_TYPE)) != NULL)
            {
                uint8_t msg_type = dhcp_get_optionb(opt_ptr + 2);

                if(msg_type == DHCP_ACK)
                {
                    if(binding->state == DHCP_REQUESTING)
                    {
                        dhcp_handle_ack(binding);
                        binding->tries = 0;
                        dhcp_check(binding);
                    }
                    else if(binding->state == DHCP_REBOOTING ||
                            binding->state == DHCP_REBINDING ||
                            binding->state == DHCP_RENEWING)
                    {
                        dhcp_handle_ack(binding);
                        binding->tries = 0;
                        dhcp_bind(binding);
                    }
                }
                else if(msg_type == DHCP_NAK)
                {
                    if(binding->state == DHCP_REBOOTING ||
                       binding->state == DHCP_REQUESTING ||
                       binding->state == DHCP_REBINDING ||
                       binding->state == DHCP_RENEWING)
                    {
                        dhcp_handle_nak(binding);
                    }
                }
                else if(msg_type == DHCP_OFFER && binding->state == DHCP_SELECTING)
                {
                    dhcp_handle_offer(binding);
                }
            }

            free_packet(binding->in_packet);
            binding->in_packet = NULL;
            binding->in_opt_len = 0;

            SOCKET_LOCK(dhcp_sock);
        }

        SOCKET_UNLOCK(dhcp_sock);
    }
}


static inline void dhcp_release_timers(struct dhcp_binding_t *binding)
{
    nettimer_release(binding->dhcp_renewing_timer);
    nettimer_release(binding->dhcp_rebinding_timer);
    nettimer_release(binding->dhcp_declining_timer);
    nettimer_release(binding->dhcp_requesting_timer);
    nettimer_release(binding->dhcp_checking_timer);
}

void dhcp_handle_offer(struct dhcp_binding_t *binding)
{
    struct packet_t *p = binding->in_packet;
    struct dhcp_msg_t *msg = (struct dhcp_msg_t *)p->data;
    uint8_t *opts = NULL, *opt_ptr;
    size_t opts_len = 0;

    // check if the packet has options
    if(p->count > sizeof(struct dhcp_msg_t))
    {
        opts = (uint8_t *)p->data + sizeof(struct dhcp_msg_t);
        opts_len = p->count - sizeof(struct dhcp_msg_t);
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                  DHCP_OPTION_DHCP_SERVER_IDENTIFIER)) != NULL)
    {
        binding->saddr = htonl(dhcp_get_optionl(opt_ptr + 2));
        binding->ipaddr = msg->yiaddr;

        dhcp_release_timers(binding);
        dhcp_select(binding);
    }
}


void dhcp_handle_nak(struct dhcp_binding_t *binding)
{
    // RFC 2131 says we should restart the configuration process when we
    // receive a NAK message
    dhcp_release_timers(binding);
    dhcp_discover(binding);
}


void dhcp_handle_ack(struct dhcp_binding_t *binding)
{
    struct packet_t *p = binding->in_packet;
    struct dhcp_msg_t *msg = (struct dhcp_msg_t *)p->data;
    uint8_t *opts = NULL, *opt_ptr;
    size_t opts_len = 0;

    dhcp_release_timers(binding);

    binding->netmask = 0;
    binding->gateway = 0;
    binding->broadcast = 0;
    binding->dns[0] = 0;
    binding->dns[1] = 0;
    binding->ntp[0] = 0;
    binding->ntp[1] = 0;
    
    binding->t1 = 0;
    binding->t2 = 0;
    binding->lease = 0;

    // check if the packet has options
    if(p->count > sizeof(struct dhcp_msg_t))
    {
        opts = (uint8_t *)p->data + sizeof(struct dhcp_msg_t);
        opts_len = p->count - sizeof(struct dhcp_msg_t);
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_DHCP_LEASE_TIME)) != NULL)
    {
        binding->lease = dhcp_get_optionl(opt_ptr + 2);
        binding->t1 = binding->lease / 2;
        binding->t2 = 0.875 * binding->lease;
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_DHCP_RENEWAL_TIME)) != NULL)
    {
        binding->t1 = dhcp_get_optionl(opt_ptr + 2);
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_DHCP_REBINDING_TIME)) != NULL)
    {
        binding->t2 = dhcp_get_optionl(opt_ptr + 2);
    }

    binding->ut1 = binding->binding_time + binding->t1;
    binding->ut2 = binding->binding_time + binding->t2;
    binding->ulease = binding->binding_time + binding->lease;
    
    binding->ipaddr = msg->yiaddr;

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_SUBNET_MASK)) != NULL)
    {
        binding->netmask = htonl(dhcp_get_optionl(opt_ptr + 2));
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_ROUTERS)) != NULL)
    {
        binding->gateway = htonl(dhcp_get_optionl(opt_ptr + 2));
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_BROADCAST_ADDRESS)) != NULL)
    {
        binding->broadcast = htonl(dhcp_get_optionl(opt_ptr + 2));
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_DOMAIN_NAME_SERVERS)) != NULL)
    {
        if(opt_ptr[1] >= 4)
        {
            binding->dns[0] = htonl(dhcp_get_optionl(opt_ptr + 2));
        }

        if(opt_ptr[1] >= 8)
        {
            binding->dns[1] = htonl(dhcp_get_optionl(opt_ptr + 6));
        }
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_NTP_SERVERS)) != NULL)
    {
        if(opt_ptr[1] >= 4)
        {
            binding->ntp[0] = htonl(dhcp_get_optionl(opt_ptr + 2));
        }

        if(opt_ptr[1] >= 8)
        {
            binding->ntp[1] = htonl(dhcp_get_optionl(opt_ptr + 6));
        }
    }

    if((opt_ptr = dhcp_option_ptr(msg, opts, opts_len,
                                    DHCP_OPTION_DOMAIN_NAME)) != NULL)
    {
        A_memcpy(binding->domain, opt_ptr + 2, opt_ptr[1]);
        binding->domain[opt_ptr[1]] = '\0';
    }
}


int dhcp_alloc_msg(struct dhcp_binding_t *binding)
{
    size_t len = sizeof(struct dhcp_msg_t) + DHCP_OPTIONS_LEN;
    struct dhcp_msg_t *msg;
    
    if(!(binding->out_packet = alloc_packet(PACKET_SIZE_UDP(len))))
    {
        return -ENOMEM;
    }

    packet_add_header(binding->out_packet, -PACKET_SIZE_UDP(0));
    msg = (struct dhcp_msg_t *)binding->out_packet->data;
    A_memset(binding->out_packet->data, 0, len);
    //binding->xid++;
    msg->op = DHCP_BOOTP_REQUEST;
    msg->htype = 1;         // Ethernet
    msg->hlen = ETHER_ADDR_LEN;
    msg->xid = htonl(binding->xid);
    msg->ciaddr = binding->ipaddr;

    A_memcpy(&msg->chaddr, &binding->ifp->hwaddr, ETHER_ADDR_LEN);
    msg->cookie = htonl(0x63825363);    // magic cookie (see RFC 2131)
    binding->out_opt_len = 0;
    
    return 0;
}


STATIC_INLINE void dhcp_schedule_task(void *arg, int event)
{
    struct dhcp_binding_t *binding = (struct dhcp_binding_t *)arg;
    
    if(!binding)
    {
        return;
    }
    
    binding->events |= event;
    unblock_kernel_task(binding->task);
}


void dhcp_renewing_timeout(void *arg)
{
    dhcp_schedule_task(arg, DHCP_EVENT_RENEWING_TIMEOUT);
}


void dhcp_rebinding_timeout(void *arg)
{
    dhcp_schedule_task(arg, DHCP_EVENT_REBINDING_TIMEOUT);
}


void dhcp_declining_timeout(void *arg)
{
    dhcp_schedule_task(arg, DHCP_EVENT_DECLINING_TIMEOUT);
}


void dhcp_requesting_timeout(void *arg)
{
    dhcp_schedule_task(arg, DHCP_EVENT_REQUESTING_TIMEOUT);
}


void dhcp_checking_timeout(void *arg)
{
    dhcp_schedule_task(arg, DHCP_EVENT_CHECKING_TIMEOUT);
}


void dhcp_t1_timeout(void *arg)
{
    dhcp_schedule_task(arg, DHCP_EVENT_T1_TIMEOUT);
}


void dhcp_t2_timeout(void *arg)
{
    dhcp_schedule_task(arg, DHCP_EVENT_T2_TIMEOUT);
}


void dhcp_lease_timeout(void *arg)
{
    dhcp_schedule_task(arg, DHCP_EVENT_LEASE_TIMEOUT);
}


void dhcp_task_func(void *unused)
{
    UNUSED(unused);
    
    while(1)
    {
        volatile struct dhcp_binding_t *binding = dhcp_binding_by_task(this_core->cur_task);
        
        if(!binding)
        {
            printk("dhcp: cannot find binding for task %d\n", this_core->cur_task->pid);
            //kpanic("dhcp failed");

            block_task(&dhcp_bindings, 0);
            continue;
        }

        if(binding->events & DHCP_EVENT_RENEWING_TIMEOUT)
        {
            if(binding->state == DHCP_RENEWING)
            {
                printk("dhcp: RENEWING timed out (%d tries)\n", binding->tries);

                if(binding->tries <= 1)
                {
                    dhcp_renew(binding);
                }
            }

            binding->events &= ~DHCP_EVENT_RENEWING_TIMEOUT;
        }

        if(binding->events & DHCP_EVENT_REBINDING_TIMEOUT)
        {
            if(binding->state == DHCP_REBINDING)
            {
                printk("dhcp: REBINDING timed out (%d tries)\n", binding->tries);

                if(binding->tries <= 1)
                {
                    dhcp_rebind(binding);
                }
            }

            binding->events &= ~DHCP_EVENT_REBINDING_TIMEOUT;
        }

        if(binding->events & DHCP_EVENT_DECLINING_TIMEOUT)
        {
            if(binding->state == DHCP_DECLINING)
            {
                printk("dhcp: DECLINING timed out - restarting discovery\n");

                dhcp_discover(binding);
            }

            binding->events &= ~DHCP_EVENT_DECLINING_TIMEOUT;
        }

        if(binding->events & DHCP_EVENT_CHECKING_TIMEOUT)
        {
            if(binding->state == DHCP_CHECKING)
            {
                printk("dhcp: CHECKING ARP request timed out (%d tries)\n", binding->tries);

                if(binding->tries <= 1)
                {
                    printk("dhcp: retrying ARP request\n");
                    dhcp_check((struct dhcp_binding_t *)binding);
                }
                else
                {
                    printk("dhcp: binding interface\n");
                    dhcp_bind((struct dhcp_binding_t *)binding);
                }
            }

            binding->events &= ~DHCP_EVENT_CHECKING_TIMEOUT;
        }
        
        if(binding->events & DHCP_EVENT_REQUESTING_TIMEOUT)
        {
            switch(binding->state)
            {
                case DHCP_SELECTING:
                    printk("dhcp: SELECT timed out - restarting discovery\n");
                    dhcp_discover(binding);
                    break;

                case DHCP_REQUESTING:
                    printk("dhcp: REQUEST timed out\n");
                    
                    // RFC 2131 suggests to try upto 4 times, or 60 secs,
                    // before bailing out and restarting the process
                    if(binding->tries <= 5)
                    {
                        printk("dhcp: retrying REQUEST (%d tries)\n", binding->tries);
                        dhcp_select(binding);
                    }
                    else
                    {
                        printk("dhcp: restarting REQUEST\n");
                        dhcp_release(binding);
                        dhcp_discover(binding);
                    }
                    break;
            }

            binding->events &= ~DHCP_EVENT_REQUESTING_TIMEOUT;
        }

        // This gets triggered the first time (when T1 expires)
        if(binding->events & DHCP_EVENT_T1_TIMEOUT)
        {
            if(binding->state == DHCP_BOUND)
            {
                printk("dhcp: T1 timeout - scheduling renewal\n");
                dhcp_renew(binding);
            }

            binding->events &= ~DHCP_EVENT_T1_TIMEOUT;
        }

        if(binding->events & DHCP_EVENT_T2_TIMEOUT)
        {
            if(binding->state == DHCP_BOUND ||
               binding->state == DHCP_RENEWING)
            {
                printk("dhcp: T2 timeout - scheduling rebind\n");
                dhcp_rebind(binding);
            }
            
            binding->events &= ~DHCP_EVENT_T2_TIMEOUT;
        }

        if(binding->events & DHCP_EVENT_LEASE_TIMEOUT)
        {
            printk("dhcp: lease timeout - restarting discovery\n");

            if(binding->state == DHCP_REBINDING ||
               binding->state == DHCP_BOUND ||
               binding->state == DHCP_RENEWING)
            {
                dhcp_release(binding);
            }

            dhcp_discover(binding);

            binding->events &= ~DHCP_EVENT_LEASE_TIMEOUT;
        }

        block_task(&dhcp_bindings, 0);
    }
}

