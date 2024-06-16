/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: netif.h
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
 *  \file netif.h
 *
 *  Functions and macros for working with network interface cards.
 */

#ifndef NET_INTERFACE_H
#define NET_INTERFACE_H

#include <sys/socket.h>
#include <net/if.h>         // IFF_* flags, IF_NAMESIZE
#include <kernel/pci.h>
#include <kernel/vfs.h>
#include <kernel/mutex.h>
#include <kernel/net/ether.h>
#include <kernel/net/stats.h>
#include <kernel/net/ipv6.h>


#define IFQ_ENQUEUE(q, p)                   \
{                                           \
    (p)->next = NULL;                       \
    if((q)->tail == NULL)                   \
    {                                       \
        (q)->head = (p);                    \
    }                                       \
    else                                    \
    {                                       \
        (q)->tail->next = (p);              \
    }                                       \
    (q)->tail = (p);                        \
    (q)->count++;                           \
}

#define IFQ_DEQUEUE(q, p)                   \
{                                           \
    (p) = (q)->head;                        \
    if(p)                                   \
    {                                       \
        (q)->head = (p)->next;              \
        if((q)->head == NULL)               \
        {                                   \
            (q)->tail = NULL;               \
        }                                   \
        (p)->next = NULL;                   \
        (q)->count--;                       \
    }                                       \
}

#define IFQ_FULL(q)                 ((q)->count >= (q)->max)
//#define IFQ_DROP(q)                 (q)->drops++

#define NETIF_DEFAULT_QUEUE_LEN     4096


// defined in dev/chr/rand.c
extern unsigned long genrand_int32(void);


/**
 * @struct netif_queue_t
 * @brief The netif_queue_t structure.
 *
 * A structure to represent a network interface packet queue.
 */
struct netif_queue_t
{
    struct packet_t *head;          /**< first packet in queue */
    struct packet_t *tail;          /**< last packet in queue */
    int count,                      /**< number of packets in queue */
        max;                        /**< max number of packets in queue */
    struct kernel_mutex_t lock;     /**< struct lock */
};

/**
 * @struct netif_t
 * @brief The netif_t structure.
 *
 * A structure to represent a network interface.
 */
struct netif_t
{
    char name[IF_NAMESIZE];             /**< interface name */
    struct netif_t *next;               /**< next interface in list */
    struct ether_addr_t ethernet_addr;  /**< hardware address */
    int unit;                           /**< unit number for internal device 
                                             driver use */
    int flags;                          /**< IFF_* flags (defined in 
                                                          net/if.h) */
    int index;                          /**< index in interface list */
    int mtu;                            /**< Maximum Transfer Unit for 
                                             the device */

    uint32_t dhcp_xid;                  /**< DHCP transaction id */

    struct stats_nic stats;             /**< interface stats */
    struct ipv6_nd_hostvars_t hostvars; /**< host variables for IPv6 */

    int (*transmit)(struct netif_t *, 
                    struct packet_t *); /**< transmit function */
    int	(*ioctl)(struct file_t *f, 
                 unsigned int, char *); /**< ioctl function */

    void (*process_input)(struct netif_t *);    /**< process input function */
    void (*process_output)(struct netif_t *);   /**< process output function */
    struct netif_queue_t *inq,          /**< incoming queue */
                         *outq;         /**< outgoing queue */
};


/**
 * @var netif_list
 * @brief Interface list.
 *
 * A pointer to the head of the registered network interfaces (defined in
 * netif.c).
 */
extern struct netif_t *netif_list;


/**********************************
 * Functions defined in netif.c
 **********************************/

/**
 * @brief Interface add.
 *
 * Add a network interface to our list of attached interfaces.
 *
 * @param   ifp     network interface
 *
 * @return  zero on success, -(errno) on failure.
 */
int netif_add(struct netif_t *ifp);

/**
 * @brief Get interface.
 *
 * Get the network interface with the given index.
 *
 * @param   i           interface index
 *
 * @return  pointer to interface on success, NULL on failure.
 */
struct netif_t *netif_by_index(int i);

/**
 * @brief Get interface.
 *
 * Get the network interface with the given name.
 *
 * @param   name        interface name
 *
 * @return  pointer to interface on success, NULL on failure.
 */
struct netif_t *netif_by_name(char *name);


/**************************************
 * Functions defined in netif_ioctl.c
 **************************************/

/**
 * @brief Network interface ioctl.
 *
 * Network interface ioctl function.
 *
 * @param   f       open file
 * @param   cmd     command to perform on the device (see SIOC_* definitions
 *                    in sys/sockio.h)
 * @param   data    optional argument, depends on what cmd is
 *
 * @return  zero on success, -(errno) on failure.
 */
int netif_ioctl(struct file_t *f, int cmd, char *data);

int netif_ipv6_random_ll(struct netif_t *ifp);

#endif      /* NET_INTERFACE_H */
