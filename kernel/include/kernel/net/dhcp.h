/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: dhcp.h
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
 *  \file dhcp.h
 *
 *  Functions and macros for handling Dynamic Host Configuration Protocol 
 *  (DHCP) packets.
 */

#ifndef NET_DHCP_H
#define NET_DHCP_H

#include <sys/types.h>
#include <netinet/in.h>

#define DHCP_SERVER_PORT                        htons(67)
#define DHCP_CLIENT_PORT                        htons(68)

#define DHCP_OPTIONS_LEN                        512
#define DHCP_MIN_OPTIONS_LEN                    68

// DHCP message types
#define DHCP_DISCOVER                           1
#define DHCP_OFFER                              2
#define DHCP_REQUEST                            3
#define DHCP_DECLINE                            4
#define DHCP_ACK                                5
#define DHCP_NAK                                6
#define DHCP_RELEASE                            7
#define DHCP_INFORM                             8

// DHCP overload types
#define DHCP_OVERLOAD_NONE                      0
#define DHCP_OVERLOAD_FILE                      1
#define DHCP_OVERLOAD_SNAME                     2
#define DHCP_OVERLOAD_SNAME_FILE                3

// DHCP Options and BOOTP Vendor Extensions
// See RFC 1533
#define DHCP_OPTION_PAD                         0
#define DHCP_OPTION_SUBNET_MASK                 1
#define DHCP_OPTION_TIME_OFFSET                 2
#define DHCP_OPTION_ROUTERS                     3
#define DHCP_OPTION_TIME_SERVERS                4
#define DHCP_OPTION_NAME_SERVERS                5
#define DHCP_OPTION_DOMAIN_NAME_SERVERS         6
#define DHCP_OPTION_LOG_SERVERS                 7
#define DHCP_OPTION_COOKIE_SERVERS              8
#define DHCP_OPTION_LPR_SERVERS                 9
#define DHCP_OPTION_IMPRESS_SERVERS             10
#define DHCP_OPTION_RESOURCE_LOCATION_SERVERS   11
#define DHCP_OPTION_HOST_NAME                   12
#define DHCP_OPTION_BOOT_SIZE                   13
#define DHCP_OPTION_MERIT_DUMP                  14
#define DHCP_OPTION_DOMAIN_NAME                 15
#define DHCP_OPTION_SWAP_SERVER                 16
#define DHCP_OPTION_ROOT_PATH                   17
#define DHCP_OPTION_EXTENSIONS_PATH             18
#define DHCP_OPTION_IP_FORWARDING               19
#define DHCP_OPTION_NON_LOCAL_SOURCE_ROUTING    20
#define DHCP_OPTION_POLICY_FILTER               21
#define DHCP_OPTION_MAX_DGRAM_REASSEMBLY        22
#define DHCP_OPTION_DEFAULT_IP_TTL              23
#define DHCP_OPTION_PATH_MTU_AGING_TIMEOUT      24
#define DHCP_OPTION_PATH_MTU_PLATEAU_TABLE      25
#define DHCP_OPTION_INTERFACE_MTU               26
#define DHCP_OPTION_ALL_SUBNETS_LOCAL           27
#define DHCP_OPTION_BROADCAST_ADDRESS           28
#define DHCP_OPTION_PERFORM_MASK_DISCOVERY      29
#define DHCP_OPTION_MASK_SUPPLIER               30
#define DHCP_OPTION_ROUTER_DISCOVERY            31
#define DHCP_OPTION_ROUTER_SOLICITATION_ADDRESS 32
#define DHCP_OPTION_STATIC_ROUTES               33
#define DHCP_OPTION_TRAILER_ENCAPSULATION       34
#define DHCP_OPTION_ARP_CACHE_TIMEOUT           35
#define DHCP_OPTION_IEEE802_3_ENCAPSULATION     36
#define DHCP_OPTION_DEFAULT_TCP_TTL             37
#define DHCP_OPTION_TCP_KEEPALIVE_INTERVAL      38
#define DHCP_OPTION_TCP_KEEPALIVE_GARBAGE       39
#define DHCP_OPTION_NIS_DOMAIN                  40
#define DHCP_OPTION_NIS_SERVERS                 41
#define DHCP_OPTION_NTP_SERVERS                 42
#define DHCP_OPTION_VENDOR_ENCAPSULATED_OPTIONS 43
#define DHCP_OPTION_NETBIOS_NAME_SERVERS        44
#define DHCP_OPTION_NETBIOS_DD_SERVER           45
#define DHCP_OPTION_NETBIOS_NODE_TYPE           46
#define DHCP_OPTION_NETBIOS_SCOPE               47
#define DHCP_OPTION_FONT_SERVERS                48
#define DHCP_OPTION_X_DISPLAY_MANAGER           49
#define DHCP_OPTION_DHCP_REQUESTED_ADDRESS      50
#define DHCP_OPTION_DHCP_LEASE_TIME             51
#define DHCP_OPTION_DHCP_OPTION_OVERLOAD        52
#define DHCP_OPTION_DHCP_MESSAGE_TYPE           53
#define DHCP_OPTION_DHCP_SERVER_IDENTIFIER      54
#define DHCP_OPTION_DHCP_PARAMETER_REQUEST_LIST 55
#define DHCP_OPTION_DHCP_MESSAGE                56
#define DHCP_OPTION_DHCP_MAX_MESSAGE_SIZE       57
#define DHCP_OPTION_DHCP_RENEWAL_TIME           58
#define DHCP_OPTION_DHCP_REBINDING_TIME         59
#define DHCP_OPTION_VENDOR_CLASS_IDENTIFIER     60
#define DHCP_OPTION_DHCP_CLIENT_IDENTIFIER      61
#define DHCP_OPTION_NWIP_DOMAIN_NAME            62
#define DHCP_OPTION_NWIP_SUBOPTIONS             63
#define DHCP_OPTION_USER_CLASS                  77
#define DHCP_OPTION_FQDN                        81
#define DHCP_OPTION_DHCP_AGENT_OPTIONS          82
#define DHCP_OPTION_END                         255


/**
 * @struct dhcp_msg_t
 * @brief The dhcp_msg_t structure.
 *
 * DHCP message structure. Field names are as defined in RFC 2131.
 * Note that RFC 2131 says we should expect an options field of at least
 * 312 byte-length, which makes a DHCP message of 576 bytes.
 */
struct dhcp_msg_t
{
// DHCP opcode types
#define DHCP_BOOTP_REQUEST                      1
#define DHCP_BOOTP_REPLY                        2
    uint8_t op;                 /**< Message opcode/type */

    uint8_t htype;              /**< Hardware address type (1 for Ether) */
    uint8_t hlen;               /**< Hardware address length (6 for Ether) */
    uint8_t hops;               /**< Number of relay agent hops from client */
    uint32_t xid;               /**< Transaction ID */
    uint16_t secs;              /**< Seconds since client began address 
                                     requisition */

#define DHCP_BROADCAST_FLAG         0x8000
    uint16_t flags;             /**< Flags */
    uint32_t ciaddr;            /**< Client IP address if client in BOUND, 
                                     RENUEW or REBINDING state */
    uint32_t yiaddr;            /**< 'Your' client IP address */
    uint32_t siaddr;            /**< IP address of next server to use in 
                                     bootstrap, returned in DHCPOFFER, 
                                     DHCPACK by server */
    uint32_t giaddr;            /**< Relay agent IP address */
    unsigned char chaddr[16];   /**< Client hardware address */
    unsigned char sname[64];    /**< Optional server name (null-terminated) */
    unsigned char file[128];    /**< Boot filename (null-terminated str), 
                                     "generic" name or null in DHCPDISCOVER, 
                                     fully qualified directory-path name 
                                     in DHCPOFFER */
    uint32_t cookie;                /**< Magic cookie - decimal 99, 130, 
                                         83 and 99 */
} __attribute__((packed));


/**
 * @struct dhcp_binding_t
 * @brief The dhcp_binding_t structure.
 *
 * DHCP binding state for each interface.
 */
struct dhcp_binding_t
{
// States of the DHCP client state machine. All states are defined
// by RFC 2131, except the following:
//     INFORMING, CHECKING, PERMANENT, DECLINING, RELEASING
#define DHCP_REQUESTING                         1
#define DHCP_INIT                               2
#define DHCP_REBOOTING                          3
#define DHCP_REBINDING                          4
#define DHCP_RENEWING                           5
#define DHCP_SELECTING                          6
#define DHCP_INFORMING                          7
#define DHCP_CHECKING                           8
#define DHCP_PERMANENT                          9
#define DHCP_BOUND                              10
#define DHCP_DECLINING                          11
#define DHCP_RELEASING                          12
    int state;                  /**< Binding state */

    int tries;                  /**< Number of tries */
    uint32_t xid;               /**< Transaction ID */
    struct netif_t *ifp;        /**< Network interface */
    //struct socket_t *so;        /**< UDP socket pointer */

    struct packet_t *in_packet; /**< Incoming packet */
    int in_opt_len;             /**< Length of incoming packet options */

    struct packet_t *out_packet;    /**< Outgoing packet */
    int out_opt_len;            /**< Length of outgoing packet options */

    volatile struct task_t *task;   /**< Binding task */

    uint32_t saddr;             /**< Server IP address */
    uint32_t ipaddr;            /**< Host IP address */
    uint32_t netmask;           /**< Network mask */
    uint32_t gateway;           /**< Gateway IP address */
    uint32_t broadcast;         /**< Broadcast IP address */

    uint32_t dns[2];            /**< DNS IP addresses */
    uint32_t ntp[2];            /**< NTP IP addresses */

    char domain[256];           /**< Domain name */
    
    unsigned long long binding_time;    /**< Binding time */
    uint32_t lease,             /**< Lease time */
             t1,                /**< T1 time */
             t2;                /**< T2 time */
    time_t   ulease,            /**< Lease time in Unix time */
             ut1,               /**< T1 time in Unix time */
             ut2;               /**< T2 time in Unix time */

    struct nettimer_t *dhcp_renewing_timer;
    struct nettimer_t *dhcp_rebinding_timer;
    struct nettimer_t *dhcp_declining_timer;
    struct nettimer_t *dhcp_requesting_timer;
    struct nettimer_t *dhcp_checking_timer;

#define DHCP_EVENT_REQUESTING_TIMEOUT           0x01
#define DHCP_EVENT_RENEWING_TIMEOUT             0x02
#define DHCP_EVENT_REBINDING_TIMEOUT            0x04
#define DHCP_EVENT_DECLINING_TIMEOUT            0x08
#define DHCP_EVENT_CHECKING_TIMEOUT             0x10
#define DHCP_EVENT_T1_TIMEOUT                   0x20
#define DHCP_EVENT_T2_TIMEOUT                   0x40
#define DHCP_EVENT_LEASE_TIMEOUT                0x80
    int events;                 /**< Events bitmap */
    
    struct dhcp_binding_t *next;    /**< Pointer to next binding */
};


/*******************************************
 * External definitions (dhcp.c)
 *******************************************/

extern struct dhcp_binding_t *dhcp_bindings;


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief Initialize DHCP.
 *
 * Initialize the Dynamic Host Configuration Protocol (DHCP) layer.
 *
 * @return  nothing.
 */
void dhcp_init(void);

/**
 * @brief Start DHCP.
 *
 * Start DHCP discovery and create a DHCP binding for the given network 
 * interface.
 *
 * @param   ifp     network interface
 *
 * @return  DHCP binding on success, NULL on failure.
 */
struct dhcp_binding_t *dhcp_start(struct netif_t *ifp);

/**
 * @brief Handle ARP reply.
 *
 * Handle ARP reply.
 *
 * @param   addr    IPv4 address
 *
 * @return  nothing.
 */
void dhcp_arp_reply(uint32_t addr);

#endif      /* NET_DHCP_H */
