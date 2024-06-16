/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
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

#define __DEBUG
#include <errno.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/task.h>
#include <kernel/net.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/dhcp.h>
#include <mm/kheap.h>

struct dhcp_client_cookie_t *dhcp_cookies = NULL;
static struct kernel_mutex_t dhcp_cookie_lock = { 0, };
static struct task_t *dhcp_task = NULL;

char dhcp_hostname[64] = { 0, };
char dhcp_domainname[64] = { 0, };

// forward declarations
static void dhcp_state_machine(struct dhcp_client_cookie_t *dhcpc,
                               uint8_t *buf, uint8_t ev);
static void dhcp_client_stop_timers(struct dhcp_client_cookie_t *dhcpc);
static int dhcp_client_init(struct dhcp_client_cookie_t *dhcpc);
static int dhcp_client_reinit(struct dhcp_client_cookie_t *dhcpc);
static void reset(struct dhcp_client_cookie_t *dhcpc);


static int dhcp_client_del_cookie(uint32_t xid)
{
    struct dhcp_client_cookie_t *dhcpc, *prev = NULL;
        
    for(dhcpc = dhcp_cookies; dhcpc != NULL; dhcpc = dhcpc->next)
    {
        if(dhcpc->xid == xid)
        {
            break;
        }
        
        prev = dhcpc;
    }

    if(!dhcpc)
    {
        return -EINVAL;
    }
    
    dhcp_client_stop_timers(dhcpc);
    socket_close(dhcpc->sock);
    dhcpc->sock = NULL;
    ipv4_link_del(dhcpc->ifp, &dhcpc->addr);
    
    if(prev)
    {
        prev->next = dhcpc->next;
    }
    else
    {
        dhcp_cookies = dhcpc->next;
    }
    
    kfree(dhcpc);
    
    return 0;
}


static struct dhcp_client_timer_t *
dhcp_timer_add(struct dhcp_client_cookie_t *dhcpc,
               uint8_t type, uint32_t time)
{
    struct dhcp_client_timer_t *timer = &dhcpc->timer[type];

    timer->xid = dhcpc->xid;
    timer->type = type;
    
    // start timer, converting time from msecs to ticks
    timer->expiry = ticks + (time / MSECS_PER_TICK);
    
    return timer;
}


static void dhcp_client_stop_timers(struct dhcp_client_cookie_t *dhcpc)
{
    int i;
    
    dhcpc->retry = 0;
    
    for(i = 0; i < 7; i++)
    {
        dhcpc->timer[i].expiry = 0;
    }
}


static int dhcp_get_timer_event(struct dhcp_client_cookie_t *dhcpc,
                                unsigned int type)
{
    static int events[] =
    {
        DHCP_EVENT_RETRANSMIT,
        DHCP_EVENT_RETRANSMIT,
        DHCP_EVENT_RETRANSMIT,
        DHCP_EVENT_RETRANSMIT,
        DHCP_EVENT_T1,
        DHCP_EVENT_T2,
        DHCP_EVENT_LEASE,
    };
    
    if(type == DHCPC_TIMER_REQUEST)
    {
        if(++dhcpc->retry > DHCP_CLIENT_RETRIES)
        {
            reset(dhcpc);
            return DHCP_EVENT_NONE;
        }
    }
    else if(type < DHCPC_TIMER_T1)
    {
        dhcpc->retry++;
    }
    
    return events[type];
}


/*
 * Returns 1 if the cookie has been removed, 0 otherwise.
 */
static int dhcp_client_reinit(struct dhcp_client_cookie_t *dhcpc)
{
    int res = 0;
    
    if(dhcpc->sock)
    {
        socket_close(dhcpc->sock);
        dhcpc->sock = NULL;
    }
    
    if(++dhcpc->retry > DHCP_CLIENT_RETRIES)
    {
        if(dhcpc->callback)
        {
            dhcpc->callback(dhcpc, DHCP_ERROR);
        }
        
        dhcp_client_del_cookie(dhcpc->xid);
        res = 1;
    }
    else
    {
        dhcp_client_init(dhcpc);
    }
    
    return res;
}


static struct dhcp_client_cookie_t *
dhcp_client_add_cookie(struct netif_t *ifp, 
                       void (*callback)(void *, int), 
                       uint32_t *uid, uint32_t xid)
{
    struct dhcp_client_cookie_t *dhcpc = NULL, *tmp;
    
    kernel_mutex_lock(&dhcp_cookie_lock);
    
    for(tmp = dhcp_cookies; tmp != NULL; tmp = tmp->next)
    {
        if(tmp->xid == xid)
        {
            break;
        }
    }
    
    // client cookie with same xid was found
    if(tmp)
    {
        kernel_mutex_unlock(&dhcp_cookie_lock);
        return NULL;
    }

    kernel_mutex_unlock(&dhcp_cookie_lock);
    
    if(!(dhcpc = kmalloc(sizeof(struct dhcp_client_cookie_t))))
    {
        return NULL;
    }
    
    A_memset(dhcpc, 0, sizeof(struct dhcp_client_cookie_t));
    dhcpc->state = DHCP_CLIENT_STATE_INIT;
    dhcpc->xid = xid;
    dhcpc->uid = uid;
    *(dhcpc->uid) = 0;
    dhcpc->callback = callback;
    dhcpc->ifp = ifp;
    dhcpc->next = NULL;

    kernel_mutex_lock(&dhcp_cookie_lock);

    if(dhcp_cookies == NULL)
    {
        dhcp_cookies = dhcpc;
    }
    else
    {
        for(tmp = dhcp_cookies; tmp->next; tmp = tmp->next)
        {
            ;
        }
        
        tmp->next = dhcpc;
    }

    kernel_mutex_unlock(&dhcp_cookie_lock);
    
    return dhcpc;
}


static int dhcp_client_msg(struct dhcp_client_cookie_t *dhcpc, uint8_t type)
{
	struct msghdr msg;
	struct iovec aiov;
	int res;
    uint16_t optlen = 0, offset = 0;
    struct dhcp_hdr_t *h = NULL;
    struct sockaddr_in src = { 0, };
    struct sockaddr_in dest =
    {
        .sin_family = AF_INET,
        .sin_addr = { 0xFFFFFFFF },
        .sin_port = htons(DHCP_SERVER_PORT),
    };
    
    // Set again default route for the bcast request
    ipv4_route_set_broadcast_link(ipv4_link_by_ifp(dhcpc->ifp));
    
    switch(type)
    {
        case DHCP_MSG_DISCOVER:
            KDEBUG("dhcp: sent DISCOVER\n");
            
            // include opt len for: msg type, max msg size, params, opt end
            optlen = 3 + 4 + 9 + 1;
            
            if(!(h = kmalloc(sizeof(struct dhcp_hdr_t) + optlen)))
            {
                return -ENOMEM;
            }
            
            A_memset(h, 0, sizeof(struct dhcp_hdr_t) + optlen);
            
            // specific options
            offset = (uint16_t)dhcp_opt_max_msgsize(DHCP_OPT(h, 0),
                                                    DHCP_CLIENT_MAX_MSGSIZE);
            break;

        case DHCP_MSG_REQUEST:
            KDEBUG("dhcp: sent REQUEST\n");
            
            // include opt len for: msg type, max msg size, params,
            // reqip, serverid, opt end
            optlen = 3 + 4 + 9 + 6 + 6 + 1;
            
            if(!(h = kmalloc(sizeof(struct dhcp_hdr_t) + optlen)))
            {
                return -ENOMEM;
            }
            
            A_memset(h, 0, sizeof(struct dhcp_hdr_t) + optlen);
            
            // specific options
            offset = dhcp_opt_max_msgsize(DHCP_OPT(h, 0),
                                          DHCP_CLIENT_MAX_MSGSIZE);
            
            if(dhcpc->state == DHCP_CLIENT_STATE_REQUESTING)
            {
                offset += dhcp_opt_reqip(DHCP_OPT(h, offset), &dhcpc->addr);
                offset += dhcp_opt_serverid(DHCP_OPT(h, offset), &dhcpc->serverid);
            }
            
            break;

        default:
            return -EINVAL;
    }
    
    // common options
    offset += dhcp_opt_msgtype(DHCP_OPT(h, offset), type);
    offset += dhcp_opt_paramlist(DHCP_OPT(h, offset));
    offset += dhcp_opt_end(DHCP_OPT(h, offset));
    
    switch(dhcpc->state)
    {
        case DHCP_CLIENT_STATE_BOUND:
        case DHCP_CLIENT_STATE_RENEWING:
            dest.sin_addr.s_addr = dhcpc->serverid.s_addr;
            h->ciaddr = dhcpc->addr.s_addr;
            break;

        case DHCP_CLIENT_STATE_REBINDING:
            h->ciaddr = dhcpc->addr.s_addr;
            break;
    }
    
    // header info
    h->op = DHCP_OP_REQUEST;
    h->htype = 1;   // Ethernet
    h->hlen = ETHER_ADDR_LEN;
    h->xid = dhcpc->xid;
    h->dhcp_magic = htonl(DHP_MAGIC_COOKIE);
    
    COPY_ETHER_ADDR(h->hwaddr, dhcpc->ifp->ethernet_addr.addr);
    
    if(dest.sin_addr.s_addr == INADDR_BROADCAST)
    {
        ipv4_route_set_broadcast_link(ipv4_link_get(&dhcpc->addr));
    }
    
    // send the message
	if(socket_check(dhcpc->sock) != 0)
	{
	    KDEBUG("dhcp: socket failed check\n");
        return -EINVAL;
	}
	
	/*
	if(!(dhcpc->sock->state & SOCKET_STATE_CONNECTED))
	{
	    KDEBUG("dhcp: socket is not connected (state %d)\n", dhcpc->sock->state);
        return -ENOTCONN;
	}
	*/

    // get src addr
	if((res = sendto_get_ipv4_src(dhcpc->sock, &dest, &src)) != 0)
	{
	    KDEBUG("dhcp: cannot get src addr\n");
	    kfree(h);
	    return res;
	}

    src.sin_port = htons(DHCP_CLIENT_PORT);
    
	msg.msg_namelen = 0;
	msg.msg_name = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = h;
	aiov.iov_len = sizeof(struct dhcp_hdr_t) + optlen;
	msg.msg_control = 0;
	msg.msg_flags = 0;

    res = do_sendto(dhcpc->sock, &msg, &src, &dest, 0, 1);
    
    kfree(h);

    KDEBUG("dhcp_client_msg: res %d\n", res);
    
    return (res < 0) ? res : 0;
}


static inline int dhcp_sock_bind(struct socket_t *so, uint16_t port)
{
    static struct sockaddr_in sin = { .sin_family = AF_INET, 0, };
    
    if(!is_port_free(so->domain, so->proto->protocol, port, (struct sockaddr *)&sin))
    {
        return -EADDRINUSE;
    }
    
    so->local_port = port;
    so->local_addr.ipv4.s_addr = sin.sin_addr.s_addr;
	
	return socket_update_state(so, SOCKET_STATE_BOUND, 0, 0);
}


static int dhcp_client_init(struct dhcp_client_cookie_t *dhcpc)
{
    static struct in_addr addr_any = { 0 };
    static struct in_addr broadcast_netmask = { 0xFFFFFFFF };
    uint16_t port = htons(DHCP_CLIENT_PORT);
    
    if(!dhcpc)
    {
        return -EINVAL;
    }
    
    // adding a link with address 0.0.0.0 and netmask 0.0.0.0,
    // automatically adds a route for a global broadcast
    ipv4_link_add(dhcpc->ifp, &addr_any, &broadcast_netmask);
    
    if(!dhcpc->sock)
    {
        KDEBUG("dhcp_client_init: creating socket\n");

        if(sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &dhcpc->sock) < 0)
        {
            return -EAGAIN;
        }

        dhcpc->sock->wakeup = dhcp_client_wakeup;
    }

    if(!dhcpc->sock)
    {
        KDEBUG("dhcp_client_init: failed to create socket\n");

        return dhcp_timer_add(dhcpc, DHCPC_TIMER_INIT, 
                              DHCP_CLIENT_REINIT) ? 0 : -ENOMEM;
    }
    
    dhcpc->sock->ifp = dhcpc->ifp;

    KDEBUG("dhcp_client_init: binding socket\n");
    
    // bind socket
    if(dhcp_sock_bind(dhcpc->sock, port) < 0)
    {
        KDEBUG("dhcp_client_init: failed to bind socket\n");

        socket_close(dhcpc->sock);
        dhcpc->sock = NULL;

        return dhcp_timer_add(dhcpc, DHCPC_TIMER_INIT, 
                              DHCP_CLIENT_REINIT) ? 0 : -ENOMEM;
    }

    KDEBUG("dhcp_client_init: sending DISCOVER\n");
    
    if(dhcp_client_msg(dhcpc, DHCP_MSG_DISCOVER) < 0)
    {
        KDEBUG("dhcp_client_init: failed to send DISCOVER\n");

        socket_close(dhcpc->sock);
        dhcpc->sock = NULL;

        return dhcp_timer_add(dhcpc, DHCPC_TIMER_INIT, 
                              DHCP_CLIENT_REINIT) ? 0 : -ENOMEM;
    }
    
    dhcpc->retry = 0;
    dhcpc->init_timestamp = ticks;

    KDEBUG("dhcp_client_init: adding retry timer\n");
    
    // timer value is doubled with every retry (exponential backoff)
    if(!dhcp_timer_add(dhcpc, DHCPC_TIMER_INIT, DHCP_CLIENT_RETRANS * 1000))
    {
        KDEBUG("dhcp_client_init: failed to add timer\n");

        socket_close(dhcpc->sock);
        dhcpc->sock = NULL;
        
        return -ENOMEM;
    }
    
    return 0;
}


int dhcp_initiate_negotiation(struct netif_t *ifp, 
                              void (*callback)(void *, int), uint32_t *uid)
{
    int retry = 32;
    uint32_t xid = 0;
    struct dhcp_client_cookie_t *dhcpc = NULL;
    
    if(!ifp || !uid)
    {
        return -EINVAL;
    }
    
    // attempt to generate a correct xid, else fail
    do
    {
        xid = genrand_int32();
    } while(!xid && --retry);
    
    if(!xid)
    {
        return -EAGAIN;
    }
    
    if(!(dhcpc = dhcp_client_add_cookie(ifp, callback, uid, xid)))
    {
        return -ENOMEM;
    }
    
    KDEBUG("dhcp: added client (cookie xid %u)\n", dhcpc->xid);
    *uid = xid;
    
    return dhcp_client_init(dhcpc);
}


int dhcp_are_opts_valid(void *data, int len)
{
    uint8_t optlen = 0;
    uint8_t *p = data;
    
    while(len > 0)
    {
        switch(*p)
        {
            case DHCP_OPT_END:
                return 1;

            case DHCP_OPT_PAD:
                p++;
                len--;
                break;

            default:
                // move pointer from code octet to len octet
                p++;
                len--;
                
                // (*p + 1) to account for len octet
                if((len <= 0) || (len - (*p + 1) < 0))
                {
                    return 0;
                }
                
                optlen = *p;
                p += optlen + 1;
                len -= optlen;
                break;
        }
    }
    
    return 0;
}


struct dhcp_opt_t *dhcp_next_opt(struct dhcp_opt_t **opts)
{
    uint8_t **p = (uint8_t **)opts;
    struct dhcp_opt_t *opt = *opts;
    
    if(opt->code == DHCP_OPT_END)
    {
        return NULL;
    }

    if(opt->code == DHCP_OPT_PAD)
    {
        *p += 1;
        return *opts;
    }
    
    *p += (opt->len + 2);   // (len + 2) to account for code and len octet
    
    return *opts;
}


uint16_t dhcp_opt_max_msgsize(struct dhcp_opt_t *opt, uint16_t size)
{
    opt->code = DHCP_OPT_MAX_MSGSIZE;
    opt->len = 2;
    opt->ext.max_msg_size.size = htons(size);
    
    return 4;
}


uint16_t dhcp_opt_reqip(struct dhcp_opt_t *opt, struct in_addr *ip)
{
    opt->code = DHCP_OPT_REQIP;
    opt->len = 4;
    opt->ext.req_ip.ip.s_addr = ip->s_addr;
    
    return 6;
}


uint16_t dhcp_opt_serverid(struct dhcp_opt_t *opt, struct in_addr *ip)
{
    opt->code = DHCP_OPT_SERVERID;
    opt->len = 4;
    opt->ext.server_id.ip.s_addr = ip->s_addr;
    
    return 6;
}


uint16_t dhcp_opt_msgtype(struct dhcp_opt_t *opt, uint8_t type)
{
    opt->code = DHCP_OPT_MSGTYPE;
    opt->len = 1;
    opt->ext.msg_type.type = type;
    
    return 3;
}


uint16_t dhcp_opt_paramlist(struct dhcp_opt_t *opt)
{
    uint8_t *params = &(opt->ext.param_list.code[0]);
    
    opt->code = DHCP_OPT_PARAMLIST;
    opt->len = 7;
    params[0] = DHCP_OPT_NETMASK;
    params[1] = DHCP_OPT_TIME;
    params[2] = DHCP_OPT_ROUTER;
    params[3] = DHCP_OPT_HOSTNAME;
    params[4] = DHCP_OPT_RENEWAL_TIME;
    params[5] = DHCP_OPT_REBINDING_TIME;
    params[6] = DHCP_OPT_DNS;
    
    return 9;
}


uint16_t dhcp_opt_end(struct dhcp_opt_t *opt)
{
    uint8_t *end = (uint8_t *)opt;
    
    *end = DHCP_OPT_END;
    
    return 1;
}


static int16_t dhcp_client_opt_parse(void *p, int len)
{
    int optlen = len - sizeof(struct dhcp_hdr_t);
	struct dhcp_hdr_t *h = (struct dhcp_hdr_t *)p;
    struct dhcp_opt_t *opt = DHCP_OPT(h, 0);
    
    if(h->dhcp_magic != htonl(DHP_MAGIC_COOKIE))
    {
        return -EINVAL;
    }
    
    if(!dhcp_are_opts_valid(opt, optlen))
    {
        return -EINVAL;
    }
    
    do
    {
        if(opt->code == DHCP_OPT_MSGTYPE)
        {
            return opt->ext.msg_type.type;
        }
    } while(dhcp_next_opt(&opt));
    
    return -EINVAL;
}


/* static */ void dhcp_client_wakeup(struct socket_t *so, uint16_t ev)
{
	struct dhcp_client_cookie_t *dhcpc;

    kernel_mutex_lock(&dhcp_cookie_lock);
    
    for(dhcpc = dhcp_cookies; dhcpc != NULL; dhcpc = dhcpc->next)
    {
        if(dhcpc->sock == so)
        {
            break;
        }
    }

    kernel_mutex_unlock(&dhcp_cookie_lock);
    
    // client cookie not found
    if(!dhcpc)
    {
        KDEBUG("dhcp: cannot find socket to wakeup\n");
        return;
    }
    
    KDEBUG("dhcp: queueing event on socket\n");

    dhcpc->pending_events |= ev;
    
    unblock_kernel_task(dhcp_task);
}


static void
dhcp_client_process_events(struct dhcp_client_cookie_t *dhcpc, uint16_t ev)
{
	struct msghdr msg;
	struct iovec aiov;
	int res;
	char *buf;
	struct dhcp_hdr_t *h;
	
	KDEBUG("dhcp_client_process_events:\n");

    if((ev & SOCKET_EV_RD) == 0 ||
       socket_check(dhcpc->sock) != 0 ||
       !(dhcpc->sock->state & SOCKET_STATE_BOUND))
	{
        return;
	}
    
    if(!(buf = kmalloc(DHCP_CLIENT_MAX_MSGSIZE)))
	{
        return;
	}

	msg.msg_namelen = 0;
	msg.msg_name = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = buf;
	aiov.iov_len = DHCP_CLIENT_MAX_MSGSIZE;
	msg.msg_control = 0;
	msg.msg_flags = 0;
	
	if((res = dhcpc->sock->proto->sockops->recvmsg(dhcpc->sock, &msg, 0)) <= 0)
	{
    	KDEBUG("dhcp_client_process_events: res %d\n", res);
	    kfree(buf);
	    return;
	}
	
	// If the 'xid' of an arriving message does not match the 'xid' of the 
	// most recent transmitted message, the message must be silently discarded
	h = (struct dhcp_hdr_t *)buf;
	    
    for(dhcpc = dhcp_cookies; dhcpc != NULL; dhcpc = dhcpc->next)
    {
        if(dhcpc->xid == h->xid)
        {
            break;
        }
    }

    if(!dhcpc)
    {
    	KDEBUG("dhcp_client_process_events: cannot find cookie for recv\n");
	    kfree(buf);
	    return;
	}
	
	dhcpc->event = (uint8_t)dhcp_client_opt_parse(buf, res);

	KDEBUG("dhcp_client_process_events: dhcpc->event %d\n", dhcpc->event);

	dhcp_state_machine(dhcpc, (uint8_t *)buf, dhcpc->event);
	kfree(buf);
}


static void dhcp_client_recv_params(struct dhcp_client_cookie_t *dhcpc,
                                    struct dhcp_opt_t *opt)
{
    uint32_t maxlen;
    
    do
    {
        switch(opt->code)
        {
            case DHCP_OPT_PAD:
                break;
            
            case DHCP_OPT_END:
                break;
            
            case DHCP_OPT_MSGTYPE:
                dhcpc->event = opt->ext.msg_type.type;
                break;
            
            case DHCP_OPT_LEASE_TIME:
                dhcpc->lease_time = ntohl(opt->ext.lease_time.time);
                break;
            
            case DHCP_OPT_RENEWAL_TIME:
                dhcpc->t1_time = ntohl(opt->ext.renewal_time.time);
                break;
            
            case DHCP_OPT_REBINDING_TIME:
                dhcpc->t2_time = ntohl(opt->ext.rebinding_time.time);
                break;
            
            case DHCP_OPT_ROUTER:
                dhcpc->gateway.s_addr = opt->ext.router.ip.s_addr;
                break;
            
            case DHCP_OPT_DNS:
                dhcpc->dns[0].s_addr = opt->ext.dns1.ip.s_addr;
                
                if(opt->len >= 8)
                {
                    dhcpc->dns[1].s_addr = opt->ext.dns2.ip.s_addr;
                }
                
                break;
            
            case DHCP_OPT_NETMASK:
                dhcpc->netmask.s_addr = opt->ext.netmask.ip.s_addr;
                break;
            
            case DHCP_OPT_SERVERID:
                dhcpc->serverid.s_addr = opt->ext.server_id.ip.s_addr;
                break;
            
            case DHCP_OPT_OVERLOAD:
                KDEBUG("dhcp: option overload - ignoring\n");
                break;
            
            case DHCP_OPT_HOSTNAME:
                maxlen = 64;
                
                if(opt->len < maxlen)
                {
                    maxlen = opt->len;
                }
                
                A_memcpy(dhcp_hostname, opt->ext.string.txt, maxlen);
                dhcp_hostname[maxlen] = '\0';
                break;
            
            case DHCP_OPT_DOMAINNAME:
                maxlen = 64;
                
                if(opt->len < maxlen)
                {
                    maxlen = opt->len;
                }
                
                A_memcpy(dhcp_domainname, opt->ext.string.txt, maxlen);
                dhcp_domainname[maxlen] = '\0';
                break;
            
            default:
                KDEBUG("dhcp: unknown option - %u\n", opt->code);
                break;
        }
    } while(dhcp_next_opt(&opt));
    
    // default values for T1 and T2 when not provided
    if(!dhcpc->t1_time)
    {
        dhcpc->t1_time = dhcpc->lease_time >> 1;
    }

    if(!dhcpc->t2_time)
    {
        dhcpc->t2_time = (dhcpc->lease_time * 875) / 1000;
    }
}


/*
 * DHCP state machine and its helper functions.
 */

static void recv_offer(struct dhcp_client_cookie_t *dhcpc, uint8_t *buf)
{
	struct dhcp_hdr_t *h = (struct dhcp_hdr_t *)buf;
    struct dhcp_opt_t *opt = DHCP_OPT(h, 0);
    
    dhcp_client_recv_params(dhcpc, opt);

    KDEBUG("dhcp: received OFFER\n");
    
    if(dhcpc->event != DHCP_MSG_OFFER || !dhcpc->serverid.s_addr ||
       !dhcpc->netmask.s_addr || !dhcpc->lease_time)
    {
        return;
    }
    
    dhcpc->addr.s_addr = h->yiaddr;
    
    // we skip state SELECTING, process first offer received
    dhcpc->state = DHCP_CLIENT_STATE_REQUESTING;
    dhcpc->retry = 0;
    
    dhcp_client_msg(dhcpc, DHCP_MSG_REQUEST);
    
    // timer value is doubled with every retry (exponential backoff)
    dhcp_timer_add(dhcpc, DHCPC_TIMER_REQUEST, DHCP_CLIENT_RETRANS * 1000);
}


static void recv_ack(struct dhcp_client_cookie_t *dhcpc, uint8_t *buf)
{
	struct dhcp_hdr_t *h = (struct dhcp_hdr_t *)buf;
    struct dhcp_opt_t *opt = DHCP_OPT(h, 0);
    struct ipv4_link_t *link;
    
    dhcp_client_recv_params(dhcpc, opt);
    
    if(dhcpc->event != DHCP_MSG_ACK)
    {
        return;
    }
    
    KDEBUG("dhcp: received ACK\n");
    
    // use the address provided by the server (could be different than the
    // one we got in the OFFER)
    if(dhcpc->state == DHCP_CLIENT_STATE_REQUESTING)
    {
        dhcpc->addr.s_addr = h->yiaddr;
    }
    
    // close the socket
    socket_close(dhcpc->sock);
    dhcpc->sock = NULL;
    
    // Delete all the links before adding the new ip address in case the
    // new address doesn't match the old one 
    link = ipv4_link_by_ifp(dhcpc->ifp);
    
    if(link && dhcpc->addr.s_addr != link->addr.s_addr)
    {
        struct in_addr addr = { 0 };
        struct in_addr any = { 0 };
        
        ipv4_link_del(dhcpc->ifp, &addr);
        link = ipv4_link_by_ifp(dhcpc->ifp);
        
        while(link)
        {
            ipv4_link_del(dhcpc->ifp, &link->addr);
            link = ipv4_link_by_ifp(dhcpc->ifp);
        }
        
        ipv4_link_add(dhcpc->ifp, &dhcpc->addr, &dhcpc->netmask);
        
        // If router option is received, use it as default gateway
        if(dhcpc->gateway.s_addr)
        {
            ipv4_route_add(NULL, &any, &any, &dhcpc->gateway, 1);
        }
    }
    
    /*
    KDEBUG("dhcp: ip ");
    KDEBUG_IPV4_ADDR(ntohl(dhcpc->addr.s_addr));
    KDEBUG(", server ip ");
    KDEBUG_IPV4_ADDR(ntohl(dhcpc->serverid.s_addr));
    KDEBUG(", dns ip ");
    KDEBUG_IPV4_ADDR(ntohl(dhcpc->dns[0].s_addr));
    KDEBUG("\n");

    KDEBUG("dhcp: renewal time (T1) %u\n", (unsigned int)dhcpc->t1_time);
    KDEBUG("dhcp: rebinding time (T2) %u\n", (unsigned int)dhcpc->t2_time);
    KDEBUG("dhcp: lease time %u\n", (unsigned int)dhcpc->lease_time);
    */
    
    dhcpc->retry = 0;
    dhcpc->renew_time = dhcpc->t2_time - dhcpc->t1_time;
    dhcpc->rebind_time = dhcpc->lease_time - dhcpc->t2_time;
    
    // start timers
    dhcp_client_stop_timers(dhcpc);
    
    if(!dhcp_timer_add(dhcpc, DHCPC_TIMER_T1, dhcpc->t1_time * 1000))
    {
        goto timer_err;
    }

    if(!dhcp_timer_add(dhcpc, DHCPC_TIMER_T2, dhcpc->t2_time * 1000))
    {
        goto timer_err;
    }

    if(!dhcp_timer_add(dhcpc, DHCPC_TIMER_LEASE, dhcpc->lease_time * 1000))
    {
        goto timer_err;
    }
    
    *(dhcpc->uid) = dhcpc->xid;

    if(dhcpc->callback)
    {
        dhcpc->callback(dhcpc, DHCP_SUCCESS);
    }
    
    dhcpc->state = DHCP_CLIENT_STATE_BOUND;
    return;

timer_err:

    dhcp_client_stop_timers(dhcpc);
    
    if(dhcpc->callback)
    {
        dhcpc->callback(dhcpc, DHCP_ERROR);
    }
}


static void reset(struct dhcp_client_cookie_t *dhcpc)
{
    struct in_addr addr = { 0 };
    
    if(dhcpc->state == DHCP_CLIENT_STATE_REQUESTING)
    {
        addr.s_addr = INADDR_ANY;
    }
    else
    {
        addr.s_addr = dhcpc->addr.s_addr;
    }

    // close the socket
    socket_close(dhcpc->sock);
    dhcpc->sock = NULL;
    
    // delete the link with the currently in use address
    ipv4_link_del(dhcpc->ifp, &addr);
    
    if(dhcpc->callback)
    {
        dhcpc->callback(dhcpc, DHCP_RESET);
    }
    
    dhcpc->state = DHCP_CLIENT_STATE_INIT;
    dhcp_client_stop_timers(dhcpc);
    dhcp_client_init(dhcpc);
}


static void renew(struct dhcp_client_cookie_t *dhcpc)
{
    uint16_t port = htons(DHCP_CLIENT_PORT);
    uint32_t halftime = 0;
    
    dhcpc->state = DHCP_CLIENT_STATE_RENEWING;
    
    if(sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &dhcpc->sock) < 0)
    {
        KDEBUG("dhcp: failed to open socket on renew\n");
        goto err2;
    }

    dhcpc->sock->wakeup = dhcp_client_wakeup;

    // bind socket
    if(dhcp_sock_bind(dhcpc->sock, port) < 0)
    {
        KDEBUG("dhcp: failed to bind socket on renew\n");
        goto err1;
    }
    
    dhcpc->retry = 0;
    
    if(dhcp_client_msg(dhcpc, DHCP_MSG_REQUEST) < 0)
    {
        KDEBUG("dhcp: failed to send request on renew\n");
        goto err1;
    }
    
    // start renew timer.
    // wait one-half of the remaining time until T2, down to a minimum 
    // of 60 seconds.
    // (dhcpc->retry + 1): initial -> divide by 2, 1st retry -> divide by 4,
    // 2nd retry -> divide by 8, ...
    dhcp_client_stop_timers(dhcpc);
    halftime = dhcpc->renew_time >> 1;
    
    if(halftime < 60)
    {
        halftime = 60;
    }

    if(!dhcp_timer_add(dhcpc, DHCPC_TIMER_RENEW, halftime * 1000))
    {
        goto err1;
    }
    
    return;

err1:
    socket_close(dhcpc->sock);
    dhcpc->sock = NULL;

err2:
    if(dhcpc->callback)
    {
        dhcpc->callback(dhcpc, DHCP_ERROR);
    }
}


static void rebind(struct dhcp_client_cookie_t *dhcpc)
{
    uint32_t halftime = 0;

    dhcpc->state = DHCP_CLIENT_STATE_REBINDING;
    dhcpc->retry = 0;

    if(dhcp_client_msg(dhcpc, DHCP_MSG_REQUEST) < 0)
    {
        KDEBUG("dhcp: failed to send request on rebind\n");
        return;
    }

    // start rebind timer.
    dhcp_client_stop_timers(dhcpc);
    halftime = dhcpc->rebind_time >> 1;
    
    if(halftime < 60)
    {
        halftime = 60;
    }

    if(!dhcp_timer_add(dhcpc, DHCPC_TIMER_REBIND, halftime * 1000))
    {
        KDEBUG("dhcp: failed to start timer on rebind\n");
    }
}


static void retransmit(struct dhcp_client_cookie_t *dhcpc)
{
    uint32_t halftime = 0;

    switch(dhcpc->state)
    {
        case DHCP_CLIENT_STATE_INIT:
            dhcp_client_msg(dhcpc, DHCP_MSG_DISCOVER);

            // timer value is doubled with every retry (exponential backoff)
            dhcp_timer_add(dhcpc, DHCPC_TIMER_INIT, 
                            (DHCP_CLIENT_RETRANS << dhcpc->retry) * 1000);
            break;

        case DHCP_CLIENT_STATE_REQUESTING:
            dhcp_client_msg(dhcpc, DHCP_MSG_REQUEST);

            // timer value is doubled with every retry (exponential backoff)
            dhcp_timer_add(dhcpc, DHCPC_TIMER_REQUEST, 
                            (DHCP_CLIENT_RETRANS << dhcpc->retry) * 1000);
            break;

        case DHCP_CLIENT_STATE_RENEWING:
            dhcp_client_msg(dhcpc, DHCP_MSG_REQUEST);

            // start renew timer.
            // wait one-half of the remaining time until T2, down to a 
            // minimum  of 60 seconds.
            // (dhcpc->retry + 1): initial -> divide by 2, 
            // 1st retry -> divide by 4,
            // 2nd retry -> divide by 8, ...
            dhcp_client_stop_timers(dhcpc);
            halftime = dhcpc->renew_time >> (dhcpc->retry + 1);

            if(halftime < 60)
            {
                halftime = 60;
            }

            dhcp_timer_add(dhcpc, DHCPC_TIMER_RENEW, halftime * 1000);
            break;

        case DHCP_CLIENT_STATE_REBINDING:
            dhcp_client_msg(dhcpc, DHCP_MSG_DISCOVER);

            // start rebind timer.
            dhcp_client_stop_timers(dhcpc);
            halftime = dhcpc->rebind_time >> (dhcpc->retry + 1);
    
            if(halftime < 60)
            {
                halftime = 60;
            }

            dhcp_timer_add(dhcpc, DHCPC_TIMER_REBIND, halftime * 1000);
            break;

        default:
            KDEBUG("dhcp: retransmit in invalid state: %u\n", dhcpc->state);
            break;
    }
}


static void dhcp_state_machine(struct dhcp_client_cookie_t *dhcpc,
                               uint8_t *buf, uint8_t ev)
{
    switch(ev)
    {
        case DHCP_MSG_OFFER:
            KDEBUG("dhcp: received OFFER\n");
            
            if(dhcpc->state == DHCP_CLIENT_STATE_INIT)
            {
                recv_offer(dhcpc, buf);
            }
            
            break;

        case DHCP_MSG_ACK:
            KDEBUG("dhcp: received ACK\n");

            if(dhcpc->state == DHCP_CLIENT_STATE_REQUESTING ||
               dhcpc->state == DHCP_CLIENT_STATE_RENEWING   ||
               dhcpc->state == DHCP_CLIENT_STATE_REBINDING)
            {
                recv_ack(dhcpc, buf);
            }
            
            break;

        case DHCP_MSG_NAK:
            KDEBUG("dhcp: received NAK\n");

            if(dhcpc->state == DHCP_CLIENT_STATE_REQUESTING ||
               dhcpc->state == DHCP_CLIENT_STATE_RENEWING   ||
               dhcpc->state == DHCP_CLIENT_STATE_REBINDING)
            {
                reset(dhcpc);
            }
            
            break;

        case DHCP_EVENT_T1:
            KDEBUG("dhcp: received T1 timeout\n");
            
            if(dhcpc->state == DHCP_CLIENT_STATE_BOUND)
            {
                renew(dhcpc);
            }
            
            break;

        case DHCP_EVENT_T2:
            KDEBUG("dhcp: received T2 timeout\n");
            
            if(dhcpc->state == DHCP_CLIENT_STATE_RENEWING)
            {
                rebind(dhcpc);
            }
            
            break;

        case DHCP_EVENT_LEASE:
            KDEBUG("dhcp: received LEASE timeout\n");
            
            if(dhcpc->state == DHCP_CLIENT_STATE_REBINDING)
            {
                reset(dhcpc);
            }
            
            break;

        case DHCP_EVENT_RETRANSMIT:
            KDEBUG("dhcp: received RETRANSMIT timeout\n");

            if(dhcpc->state == DHCP_CLIENT_STATE_INIT       ||
               dhcpc->state == DHCP_CLIENT_STATE_REQUESTING ||
               dhcpc->state == DHCP_CLIENT_STATE_RENEWING   ||
               dhcpc->state == DHCP_CLIENT_STATE_REBINDING)
            {
                retransmit(dhcpc);
            }
            
            break;

        default:
            KDEBUG("dhcp: received unknown event (%u)\n", ev);
            break;
    }
}


static void dhcp_task_func(void *arg)
{
    UNUSED(arg);
    
    for(;;)
    {
        struct dhcp_client_cookie_t *dhcpc;
        int i;

        //KDEBUG("dhcp_task_func:\n");
        
        kernel_mutex_lock(&dhcp_cookie_lock);
        
        for(dhcpc = dhcp_cookies; dhcpc != NULL; dhcpc = dhcpc->next)
        {
            // check for expired timers
            for(i = 0; i < 7; i++)
            {
                if(dhcpc->timer[i].expiry && dhcpc->timer[i].expiry < ticks)
                {
                    KDEBUG("dhcp: timer %d expired (cookie %lx)\n", i, dhcpc);
                    KDEBUG("dhcp: timer ticks %ld, ticks %ld\n", dhcpc->timer[i].expiry, ticks);

                    dhcpc->timer[i].expiry = 0;

                    if(i == DHCPC_TIMER_INIT)
                    {
                        // this was an INIT timer
                        if(dhcpc->state < DHCP_CLIENT_STATE_SELECTING)
                        {
                            // This call returns 1 if the cookie has been 
                            // removed, 0 otherwise.
                            if(dhcp_client_reinit(dhcpc))
                            {
                                // start over as the list has changed
                                dhcpc = dhcp_cookies;
                                continue;
                            }
                        }
                    }
                    else
                    {
                        // this was not an INIT timer
                        dhcpc->event = dhcp_get_timer_event(dhcpc, i);
                
                        if(dhcpc->event != DHCP_EVENT_NONE)
                        {
                            dhcp_state_machine(dhcpc, NULL, dhcpc->event);
                        }
                    }
                }
            }
            
            // now check for pending events
            if(dhcpc->pending_events)
            {
                uint16_t ev = dhcpc->pending_events;

                KDEBUG("dhcp: pending ev %d (cookie %lx)\n", dhcpc->pending_events, dhcpc);

                dhcpc->pending_events = 0;
                dhcp_client_process_events(dhcpc, ev);
            }
        }
        
        kernel_mutex_unlock(&dhcp_cookie_lock);

        //KDEBUG("dhcp_task_func: sleeping\n");
        
        block_task2(&dhcp_task, PIT_FREQUENCY * 10);
    }
}


void dhcp_init(void)
{
    // fork the DHCP monitor task
    (void)start_kernel_task("dhcp", dhcp_task_func, NULL, &dhcp_task, 0);
}

