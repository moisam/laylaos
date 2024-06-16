/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
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

// dhcp states
#define DHCP_CLIENT_STATE_INIT_REBOOT       0
#define DHCP_CLIENT_STATE_REBOOTING         1
#define DHCP_CLIENT_STATE_INIT              2
#define DHCP_CLIENT_STATE_SELECTING         3
#define DHCP_CLIENT_STATE_REQUESTING        4
#define DHCP_CLIENT_STATE_BOUND             5
#define DHCP_CLIENT_STATE_RENEWING          6
#define DHCP_CLIENT_STATE_REBINDING         7

// dhcp op codes
#define DHCP_OP_REQUEST                     1
#define DHCP_OP_REPLY                       2

// dhcp ports
#define DHCP_CLIENT_PORT                    68
#define DHCP_SERVER_PORT                    67

// dhcp magic (in message headers)
#define DHP_MAGIC_COOKIE                    0x63825363

// dhcp message types
#define DHCP_MSG_DISCOVER                   1
#define DHCP_MSG_OFFER                      2
#define DHCP_MSG_REQUEST                    3
#define DHCP_MSG_DECLINE                    4
#define DHCP_MSG_ACK                        5
#define DHCP_MSG_NAK                        6
#define DHCP_MSG_RELEASE                    7
#define DHCP_MSG_INFORM                     8

// custom message types
#define DHCP_EVENT_T1                       9
#define DHCP_EVENT_T2                       10
#define DHCP_EVENT_LEASE                    11
#define DHCP_EVENT_RETRANSMIT               12
#define DHCP_EVENT_NONE                     0xff

// timer types
#define DHCPC_TIMER_INIT                    0
#define DHCPC_TIMER_REQUEST                 1
#define DHCPC_TIMER_RENEW                   2
#define DHCPC_TIMER_REBIND                  3
#define DHCPC_TIMER_T1                      4
#define DHCPC_TIMER_T2                      5
#define DHCPC_TIMER_LEASE                   6

// timer values
#define DHCP_CLIENT_REINIT                  6000 /* msecs */
#define DHCP_CLIENT_RETRANS                 4    /* secs */
#define DHCP_CLIENT_RETRIES                 3

// codes for callback functions
#define DHCP_SUCCESS                        0
#define DHCP_ERROR                          1
#define DHCP_RESET                          2

// max message size
#define DHCP_CLIENT_MAX_MSGSIZE             (1500 - IPv4_HLEN)

// get an option
#define DHCP_OPT(h, off)            ((struct dhcp_opt_t *)  \
                                     (((uint8_t *)h) +      \
                                     sizeof(struct dhcp_hdr_t) + off))

// option types
#define DHCP_OPT_PAD                        0x00
#define DHCP_OPT_NETMASK                    0x01
#define DHCP_OPT_TIME                       0x02
#define DHCP_OPT_ROUTER                     0x03
#define DHCP_OPT_DNS                        0x06
#define DHCP_OPT_HOSTNAME                   0x0c
#define DHCP_OPT_DOMAINNAME                 0x0f
#define DHCP_OPT_REQIP                      0x32
#define DHCP_OPT_LEASE_TIME                 0x33
#define DHCP_OPT_OVERLOAD                   0x34
#define DHCP_OPT_MSGTYPE                    0x35
#define DHCP_OPT_SERVERID                   0x36
#define DHCP_OPT_PARAMLIST                  0x37
#define DHCP_OPT_MAX_MSGSIZE                0x39
#define DHCP_OPT_RENEWAL_TIME               0x3a
#define DHCP_OPT_REBINDING_TIME             0x3b
#define DHCP_OPT_END                        0xff


/**
 * @struct dhcp_hdr_t
 * @brief The dhcp_hdr_t structure.
 *
 * DHCP message header structure. Field names are as defined in RFC 2131.
 * Note that RFC 2131 says we should expect an options field of at least
 * 312 byte-length, which makes a DHCP message of 576 bytes.
 */
struct dhcp_hdr_t
{
    uint8_t op;             /**< Message opcode/type */
    uint8_t htype;          /**< Hardware address type (1 for Ether) */
    uint8_t hlen;           /**< Hardware address length (6 for Ether) */
    uint8_t hops;           /**< Number of relay agent hops from client */
    uint32_t xid;           /**< Transaction ID */
    uint16_t secs;          /**< Seconds since client began address 
                                 requisition */
    uint16_t flags;         /**< Flags */
    uint32_t ciaddr;        /**< Client IP address if client in BOUND, 
                                 RENEW or REBINDING state */
    uint32_t yiaddr;        /**< 'Your' client IP address */
    uint32_t siaddr;        /**< IP address of next server to use in 
                                 bootstrap, returned in DHCPOFFER, 
                                 DHCPACK by server */
    uint32_t giaddr;        /**< Relay agent IP address */
    uint8_t hwaddr[6];      /**< Client hardware address */
    uint8_t hwaddr_padding[10]; /**< padding */
    char hostname[64];      /**< Optional server name (null-terminated) */
    char bootp_filename[128];   /**< Boot filename (null-terminated str), 
                                     "generic" name or null in DHCPDISCOVER, 
                                     fully qualified directory-path name 
                                     in DHCPOFFER */
    uint32_t dhcp_magic;    /**< Magic cookie - decimal 99, 130, 
                                 83 and 99 */
} __attribute__((packed));

/**
 * @struct dhcp_opt_t
 * @brief The dhcp_opt_t structure.
 *
 * Structure to represent a DHCP option.
 */
struct dhcp_opt_t
{
    uint8_t code, len;
    
    union
    {
        struct
        {
            struct in_addr ip;
        } netmask;

        struct
        {
            struct in_addr ip;
        } router;

        struct
        {
            struct in_addr ip;
        } dns1;

        struct
        {
            struct in_addr ip;
        } dns2;

        struct
        {
            struct in_addr ip;
        } broadcast;

        struct
        {
            struct in_addr ip;
        } req_ip;

        struct
        {
            struct in_addr ip;
        } server_id;

        struct
        {
            uint32_t time;
        } lease_time;

        struct
        {
            uint32_t time;
        } renewal_time;

        struct
        {
            uint32_t time;
        } rebinding_time;

        struct
        {
            uint8_t value;
        } opt_overload;

        struct
        {
            char name[1];
        } tftp_server;

        struct
        {
            char name[1];
        } bootfile;

        struct
        {
            char error[1];
        } message;

        struct
        {
            char txt[1];
        } string;

        struct
        {
            uint8_t code[1];
        } param_list;

        struct
        {
            uint8_t id[1];
        } vendor_id;

        struct
        {
            uint8_t id[1];
        } client_id;

        struct
        {
            uint8_t type;
        } msg_type;

        struct
        {
            uint16_t size;
        } max_msg_size;
    } ext;
} __attribute__((packed));

/**
 * @struct dhcp_client_timer_t
 * @brief The dhcp_client_timer_t structure.
 *
 * Structure to represent a DHCP timer.
 */
struct dhcp_client_timer_t
{
    //int running;
    int type;                   /**< Type of timer */
    uint32_t xid;               /**< Transaction id */
    unsigned long long expiry;  /**< When is the timer going to expire */
};

/**
 * @struct dhcp_client_cookie_t
 * @brief The dhcp_client_cookie_t structure.
 *
 * Structure to represent a DHCP binding.
 */
struct dhcp_client_cookie_t
{
    uint8_t event;          /**< Event type - see dhcp.c */
    uint8_t retry;          /**< Number of retries */
    uint32_t xid;           /**< Transaction id */
    uint32_t *uid;          /**< Pointer to transaction id */
    int state;              /**< Binding state */
    unsigned long long init_timestamp;  /**< Binding time */
    void (*callback)(void *, int);      /**< callback function for events */
    struct socket_t *sock;      /**< pointer to UDP socket used in 
                                     communicating with the server */
    struct in_addr addr;        /**< Host IP address */
    struct in_addr netmask;     /**< Network mask */
    struct in_addr gateway;     /**< Gateway IP address */
    struct in_addr dns[2];      /**< DNS IP addresses */
    struct in_addr serverid;    /**< Server IP address */
    struct netif_t *ifp;        /**< Network interface */
    struct dhcp_client_timer_t timer[7];    /**< Timers */
    uint32_t t1_time,           /**< T1 time */
             t2_time;           /**< T2 time */
    uint32_t lease_time,        /**< Lease time */
             renew_time,        /**< Renewal time */
             rebind_time;       /**< Rebinding time */
    uint16_t pending_events;    /**< Any pending events? */
    struct dhcp_client_cookie_t *next;  /**< Pointer to next cookie */
};


/**
 * @var dhcp_cookies
 * @brief DHCP cookies.
 *
 * Linked list of all DHCP cookies (or bindings).
 * Defined in dhcp.c.
 */
extern struct dhcp_client_cookie_t *dhcp_cookies;


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
 * @brief Initiate DHCP negotiation.
 *
 * When a new network interface is detected, this function is called to
 * initiate DHCP negotiation with the server.
 *
 * @return  zero on success, -(errno) on failure.
 */
int dhcp_initiate_negotiation(struct netif_t *ifp, 
                              void (*callback)(void *, int), uint32_t *uid);

/**********************************
 * Internal functions
 **********************************/

uint16_t dhcp_opt_max_msgsize(struct dhcp_opt_t *opt, uint16_t size);
uint16_t dhcp_opt_reqip(struct dhcp_opt_t *opt, struct in_addr *ip);
uint16_t dhcp_opt_serverid(struct dhcp_opt_t *opt, struct in_addr *ip);
uint16_t dhcp_opt_msgtype(struct dhcp_opt_t *opt, uint8_t type);
uint16_t dhcp_opt_paramlist(struct dhcp_opt_t *opt);
uint16_t dhcp_opt_end(struct dhcp_opt_t *opt);

void dhcp_client_wakeup(struct socket_t *so, uint16_t ev);

#endif      /* NET_DHCP_H */
