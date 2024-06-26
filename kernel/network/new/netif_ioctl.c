/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: netif_ioctl.c
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
 *  \file netif_ioctl.c
 *
 *  The network interface card driver.
 *
 *  The driver's code is split between these files:
 *    - netif.c => general driver functions
 *    - netif_ioctl.c => driver ioctl() function
 */

#define __DEBUG

#define __USE_MISC

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
//#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/net/netif.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <mm/kheap.h>

/*
 * INetwork interface ioctl.
 *
 * For details on ioctl flags and their meanings, see:
 *    https://man7.org/linux/man-pages/man7/netdevice.7.html
 */

#define ACCEPTED_FLAGS          (IFF_UP | IFF_BROADCAST | IFF_DEBUG | \
                                 IFF_LOOPBACK | IFF_POINTOPOINT | \
                                 IFF_RUNNING | IFF_PROMISC | IFF_ALLMULTI | \
                                 IFF_MULTICAST | IFF_PORTSEL)

#define CHECK_PRIVILEGE()                                   \
     if(!suser(ct)) return -EPERM;

#define GET_NETIF(ifp, ifr)                                 \
	if(!(ifp = netif_by_name(ifr.ifr_name))) return -ENXIO;

#define GET_NETIF_PRIV(ifp, ifr)                            \
	if(!(ifp = netif_by_name(ifr.ifr_name))) return -ENXIO; \
	CHECK_PRIVILEGE();


/*
 * Get an interface's IPv4 address, broadcast address, or network mask.
 */
#define WHICH_IPADDR                            1
#define WHICH_BROADCAST                         2
#define WHICH_NETMASK                           3

static int get_addr(struct netif_t *ifp, struct ifreq *ifr, int which)
{
    struct ipv4_link_t *link;
    struct in_addr addr;
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr->ifr_addr;

    if(!(link = ipv4_link_by_ifp(ifp)))
    {
        return -EINVAL;
    }

    switch(which)
    {
        case WHICH_IPADDR:
            sin->sin_family = AF_INET;
            sin->sin_addr.s_addr = link->addr.s_addr;
            return 0;

        case WHICH_BROADCAST:
            // get the network address
            addr.s_addr = link->addr.s_addr & link->netmask.s_addr;
            
            // then get the broadcast address
            addr.s_addr |= ~(link->netmask.s_addr);

            sin->sin_family = AF_INET;
            sin->sin_addr.s_addr = addr.s_addr;
            return 0;

        case WHICH_NETMASK:
            sin->sin_family = AF_INET;
            sin->sin_addr.s_addr = link->netmask.s_addr;
            return 0;
    }

    return -EINVAL;
}


static int set_addr(struct netif_t *ifp, struct ifreq *ifr, int which)
{
    if(ifr->ifr_addr.sa_family == AF_INET)
    {
        struct ipv4_link_t *link;
        struct in_addr addr = { 0 }, mask = { 0 };
        struct in_addr network = { 0 }, gateway = { 0 };
        struct sockaddr_in *sin = (struct sockaddr_in *)&ifr->ifr_addr;

        // AF_INET addresses are deleted by passing an address of 0
        if(sin->sin_addr.s_addr == INADDR_ANY && which == WHICH_IPADDR)
        {
            ipv4_cleanup_links(ifp);
            return 0;
        }
        
        switch(which)
        {
            case WHICH_BROADCAST:
                // XXX
                return -EOPNOTSUPP;

            case WHICH_NETMASK:
                mask.s_addr = sin->sin_addr.s_addr;

                if(!(link = ipv4_link_by_ifp(ifp)))
                {
                    return ipv4_link_add(ifp, &addr, &mask);
                }
                else
                {
                    ipv4_cleanup_routes(link);
                    addr.s_addr = link->addr.s_addr;
                    link->netmask.s_addr = mask.s_addr;
                    network.s_addr = (addr.s_addr & mask.s_addr);
                    ipv4_route_add(link, &network, &mask, &gateway, 1);
                    return 0;
                }

            case WHICH_IPADDR:
                addr.s_addr = sin->sin_addr.s_addr;

                if(!(link = ipv4_link_by_ifp(ifp)))
                {
                    return ipv4_link_add(ifp, &addr, &mask);
                }
                else
                {
                    ipv4_cleanup_routes(link);
                    mask.s_addr = link->netmask.s_addr;
                    link->addr.s_addr = addr.s_addr;
                    network.s_addr = (addr.s_addr & mask.s_addr);
                    ipv4_route_add(link, &network, &mask, &gateway, 1);
                    return 0;
                }
            
            default:
                return -EINVAL;
        }
    }
    else if(ifr->ifr_addr.sa_family == AF_INET6)
    {
        // XXX
        return -EOPNOTSUPP;
    }
    else
    {
        return -EINVAL;
    }
}


/*
 * Return the list of IPv4 addresses for all interfaces on the system.
 */
static int netif_getconf(char *data)
{
    struct ifconf ifconf;
    struct ifreq *ifr;
    struct ifreq zero;
    struct ipv4_link_t *link;
    int bytes = 0, userbytes;
    int dryrun;

	if(copy_from_user(&ifconf, (struct ifconf *)data, 
                      sizeof(struct ifconf)) != 0)
	{
	    return -EINVAL;
	}
	
	ifr = ifconf.ifc_req;
	userbytes = ifconf.ifc_len;
    dryrun = (ifconf.ifc_req == NULL);
    A_memset(&zero, 0, sizeof(struct ifreq));

    // Get all interface addresses
    for(link = ipv4_links; link != NULL; link = link->next)
    {
        bytes += sizeof(struct ifreq);
        
        // don't copy info if the caller requested to know the buffer size
        // needed to store the addresses
        if(dryrun)
        {
            continue;
        }
        
        // don't copy past the size specified by the caller
        if(userbytes && bytes >= userbytes)
        {
            break;
        }
        
        // indirectly zero userspace struct to catch writing to invalid
        // addresses
        if(copy_to_user(ifr, &zero, sizeof(struct ifreq)) != 0)
        {
            return -EFAULT;
        }
        
        // now we know the address is legit, we can write directly there
        // we only need to copy the interface name and address for now
        A_memcpy(ifr->ifr_name, link->ifp->name, sizeof(link->ifp->name));
        ifr->ifr_addr.sa_family = AF_INET;
        ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr.s_addr = 
                                                        link->addr.s_addr;
    }
    
    // tell the caller the size of needed buffer, or how much we copied into
    // said buffer
    ifconf.ifc_len = bytes;
	return copy_to_user((struct ifconf *)data, &ifconf, sizeof(struct ifconf));
}


/*
 * Network interface ioctl.
 */
int netif_ioctl(struct file_t *f, int cmd, char *data)
{
    struct ifreq ifr;
    struct netif_t *ifp;
    int copyback = 0;
    struct task_t *ct = cur_task;
    
	if(!data)
	{
	    return -EINVAL;
	}
	
	// cmd SIOCGIFCONF is handled separately as it passes a struct ifconf,
	// unlike the rest of cmds which pass a struct ifreq.
	if(cmd == SIOCGIFCONF)
	{
	    return netif_getconf(data);
	}
	
	if(copy_from_user(&ifr, (struct ifreq *)data, sizeof(struct ifreq)) != 0)
	{
	    return -EINVAL;
	}
    
	switch(cmd)
	{
	    // Get interface name from its index
	    case SIOCGIFNAME:
	        if(!(ifp = netif_by_index(ifr.ifr_ifindex)))
	        {
	            return -ENXIO;
	        }
	        
	        // our internal name (in struct netif_t) is shorter than the one
	        // in struct ifreq.
	        A_memset(ifr.ifr_name, 0, sizeof(ifr.ifr_name));
	        A_memcpy(ifr.ifr_name, ifp->name, sizeof(ifp->name));
	        copyback = 1;
	        break;

	    // Get interface index from its name
	    case SIOCGIFINDEX:
	        GET_NETIF(ifp, ifr);
	        ifr.ifr_ifindex = ifp->index;
	        copyback = 1;
	        break;

	    // Get interface flags
	    case SIOCGIFFLAGS:
	        GET_NETIF(ifp, ifr);
	        ifr.ifr_flags = ifp->flags;
	        copyback = 1;
	        break;
	        
	    // Set interface flags
	    case SIOCSIFFLAGS:
	        // check flags validity
	        if(ifr.ifr_flags & ~ACCEPTED_FLAGS)
	        {
	            return -EINVAL;
	        }
	        
	        GET_NETIF_PRIV(ifp, ifr);
	        ifp->flags = ifr.ifr_flags;
	        
	        /*
	         * TODO: handle flags changes like bringing the interface up,
	         *       shutting it down, ...
	         */
	        break;

	    // Get interface address (AF_INET only)
	    case SIOCGIFADDR:
	        GET_NETIF(ifp, ifr);

            if(get_addr(ifp, &ifr, WHICH_IPADDR) != 0)
            {
	            return -EINVAL;
            }

	        copyback = 1;
	        break;

	    // Set interface address (AF_INET or AF_INET6)
	    case SIOCSIFADDR:
	        GET_NETIF_PRIV(ifp, ifr);

            if(set_addr(ifp, &ifr, WHICH_IPADDR) != 0)
            {
	            return -EINVAL;
            }

	        break;

	    // Delete interface address (AF_INET6 only)
	    case SIOCDIFADDR:
	    {
	        GET_NETIF_PRIV(ifp, ifr);

            if(ifr.ifr_addr.sa_family != AF_INET6)
            {
                return -EINVAL;
            }
            
            struct ipv6_link_t *link;
            struct in6_addr addr;
            struct sockaddr_in6 *orig = (struct sockaddr_in6 *)&ifr.ifr_addr;
            
            COPY_FROM_USER(addr.s6_addr, orig->sin6_addr.s6_addr, 16);
            
            if(!(link = ipv6_link_get(&addr)))
            {
                return -EINVAL;
            }
            
            return ipv6_link_del(link->ifp, &link->addr);
        }

	    // Get interface broadcast address (AF_INET only)
	    case SIOCGIFBRDADDR:
	        GET_NETIF(ifp, ifr);

            if(ifr.ifr_addr.sa_family != AF_INET)
            {
                return -EINVAL;
            }

            if(get_addr(ifp, &ifr, WHICH_BROADCAST) != 0)
            {
	            return -EINVAL;
            }

	        copyback = 1;
	        break;

	    // Set interface broadcast address (AF_INET only)
	    case SIOCSIFBRDADDR:
	        GET_NETIF_PRIV(ifp, ifr);

            if(ifr.ifr_addr.sa_family != AF_INET)
            {
                return -EINVAL;
            }

            if(set_addr(ifp, &ifr, WHICH_BROADCAST) != 0)
            {
	            return -EINVAL;
            }

	        break;

	    // Get interface netmask (AF_INET only)
	    case SIOCGIFNETMASK:
	        GET_NETIF(ifp, ifr);

            if(ifr.ifr_addr.sa_family != AF_INET)
            {
                return -EINVAL;
            }

            if(get_addr(ifp, &ifr, WHICH_NETMASK) != 0)
            {
	            return -EINVAL;
            }

	        copyback = 1;
	        break;

	    // Set interface netmask (AF_INET only)
	    case SIOCSIFNETMASK:
	        GET_NETIF_PRIV(ifp, ifr);

            if(ifr.ifr_addr.sa_family != AF_INET)
            {
                return -EINVAL;
            }

            if(set_addr(ifp, &ifr, WHICH_NETMASK) != 0)
            {
	            return -EINVAL;
            }

	        break;

	    // Get interface MTU (Maximum Transfer Unit)
	    case SIOCGIFMTU:
	        GET_NETIF(ifp, ifr);
	        ifr.ifr_mtu = ifp->mtu;
	        copyback = 1;
	        break;

	    // Set interface MTU (Maximum Transfer Unit)
	    case SIOCSIFMTU:
	        GET_NETIF_PRIV(ifp, ifr);
	        ifp->mtu = ifr.ifr_mtu;
	        break;

        // Get interface hardware address
	    case SIOCGIFHWADDR:
	        GET_NETIF(ifp, ifr);
	        ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	        A_memcpy(&ifr.ifr_hwaddr.sa_data, ifp->ethernet_addr.addr, 
	                 ETHER_ADDR_LEN);
	        copyback = 1;
	        break;

        // Set interface hardware address
	    case SIOCSIFHWADDR:
	        GET_NETIF_PRIV(ifp, ifr);
	        A_memcpy(ifp->ethernet_addr.addr, &ifr.ifr_hwaddr.sa_data, 
	                 ETHER_ADDR_LEN);
	        break;

        // Get interface hardware parameters
	    case SIOCGIFMAP:
	        GET_NETIF(ifp, ifr);
	        return ifp->ioctl ? ifp->ioctl(f, cmd, (char *)&ifr.ifr_map) :
	                            -EOPNOTSUPP;

        // Set interface hardware parameters
	    case SIOCSIFMAP:
	        GET_NETIF_PRIV(ifp, ifr);
	        return ifp->ioctl ? ifp->ioctl(f, cmd, (char *)&ifr.ifr_map) :
	                            -EOPNOTSUPP;

        // Get transmit queue length
	    case SIOCGIFTXQLEN:
	        GET_NETIF(ifp, ifr);
	        ifr.ifr_qlen = ifp->outq ? ifp->outq->max : -1;
	        copyback = 1;
	        break;

        // Set transmit queue length
	    case SIOCSIFTXQLEN:
	        GET_NETIF_PRIV(ifp, ifr);
	        
	        if(ifr.ifr_qlen >= 128 && ifp->outq && ifr.ifr_qlen > ifp->outq->count)
	        {
	            ifp->outq->max = ifr.ifr_qlen;
	            return 0;
	        }
	        
	        return -EINVAL;

        // Change interface device name
	    case SIOCSIFNAME:
	        GET_NETIF_PRIV(ifp, ifr);
	        A_memcpy(ifp->name, ifr.ifr_newname, IF_NAMESIZE);
	        break;

	    // Get/set extended flags -- UNIMPLEMENTED
	    case SIOCGIFPFLAGS:
	    case SIOCSIFPFLAGS:
            return -EOPNOTSUPP;

	    // Get/set P2P destination address -- UNIMPLEMENTED
	    case SIOCGIFDSTADDR:
	    case SIOCSIFDSTADDR:
            return -EOPNOTSUPP;

	    // Get/set interface metric -- UNIMPLEMENTED
	    case SIOCGIFMETRIC:
	    case SIOCSIFMETRIC:
            return -EOPNOTSUPP;
        
        // Get/set interface hardware broadcast address -- UNIMPLEMENTED
	    case SIOCSIFHWBROADCAST:
            return -EOPNOTSUPP;

        // Add/delete multicast filter address -- TODO: should be implemented
	    case SIOCADDMULTI:
	    case SIOCDELMULTI:
            return -EOPNOTSUPP;

        default:
            return -EOPNOTSUPP;
	}

	return copyback ?
	        copy_to_user((struct ifreq *)data, &ifr, sizeof(struct ifreq)) : 0;
}

