/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: socket.h
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
 *  \file socket.h
 *
 *  Functions and macros to implement the kernel's socket layer.
 */

#ifndef __KERNEL_SOCKETS_H__
#define __KERNEL_SOCKETS_H__

#include <kernel/select.h>
#include <kernel/net/netif.h>

#define SOCKET_FLAG_TCPNODELAY      0x01
#define SOCKET_FLAG_NONBLOCK        0x02
#define SOCKET_FLAG_IPHDR_INCLUDED  0x04    /* for raw sockets */
#define SOCKET_FLAG_BROADCAST       0x08    /* for udp & raw sockets */
#define SOCKET_FLAG_RECVTOS         0x10
#define SOCKET_FLAG_RECVTTL         0x20    /* not for stream sockets */
#define SOCKET_FLAG_RECVOPTS        0x40    /* not for stream sockets */

#define SOCKET_LINGER_TIMEOUT       3000
#define SOCKET_BOUND_TIMEOUT        3000
#define SOCKET_DEFAULT_QUEUE_SIZE   (8 * 1024)

// generic socket states
#define SOCKET_STATE_UNDEFINED      0x00
#define SOCKET_STATE_SHUT_LOCAL     0x01
#define SOCKET_STATE_SHUT_REMOTE    0x02
#define SOCKET_STATE_BOUND          0x04
#define SOCKET_STATE_CONNECTED      0x08
#define SOCKET_STATE_CLOSING        0x10
#define SOCKET_STATE_CLOSED         0x20
#define SOCKET_STATE_CONNECTING     0x40
#define SOCKET_STATE_LISTENING      0x80

// TCP socket states
#define SOCKET_STATE_TCP                0xff00
#define SOCKET_STATE_TCP_UNDEF          0x00ff
#define SOCKET_STATE_TCP_CLOSED         0x0100
#define SOCKET_STATE_TCP_LISTEN         0x0200
#define SOCKET_STATE_TCP_SYN_SENT       0x0300
#define SOCKET_STATE_TCP_SYN_RECV       0x0400
#define SOCKET_STATE_TCP_ESTABLISHED    0x0500
#define SOCKET_STATE_TCP_CLOSE_WAIT     0x0600
#define SOCKET_STATE_TCP_LAST_ACK       0x0700
#define SOCKET_STATE_TCP_FIN_WAIT1      0x0800
#define SOCKET_STATE_TCP_FIN_WAIT2      0x0900
#define SOCKET_STATE_TCP_CLOSING        0x0a00
#define SOCKET_STATE_TCP_TIME_WAIT      0x0b00
#define SOCKET_STATE_TCP_ARRAYSIZ       0x0c

// socket events
#define SOCKET_EV_RD                    0x01
#define SOCKET_EV_WR                    0x02
#define SOCKET_EV_CONN                  0x04
#define SOCKET_EV_CLOSE                 0x08
#define SOCKET_EV_FIN                   0x10
#define SOCKET_EV_ERR                   0x80


#include <sys/un.h>

/**
 * @union ip_addr_t
 * @brief The ip_addr_t union.
 *
 * A union to represent IP v4/v6 addresses.
 */
union ip_addr_t
{
    struct in_addr ipv4;    /**< IPv4 address */
    struct in6_addr ipv6;   /**< IPv6 address */
    struct sockaddr_un sun; /**< Unix address */
};


/**
 * @struct socket_t
 * @brief The socket_t structure.
 *
 * A structure to represent a socket internally within the kernel.
 */
struct socket_t
{
    int type;       /**< Socket type */
    int domain;     /**< Socket domain */
    int state;      /**< Socket state */
    int flags;      /**< Socket flags */

    struct proto_t *proto;      /**< pointer to protocol operations struct */
    
    union ip_addr_t local_addr;     /**< Local IP address */
    union ip_addr_t remote_addr;    /**< Remote IP address */

    uint16_t local_port;            /**< Local port */
    uint16_t remote_port;           /**< Remote port */

    struct netif_queue_t inq;       /**< Input queue */
    struct netif_queue_t outq;      /**< Output queue */

    struct selinfo recvsel,     /**< Select channel for waiting receivers */
                   sendsel;     /**< Select channel for waiting senders */
    
    void (*wakeup)(struct socket_t *so, uint16_t ev);
                                /**< Callback function */
    
    uint8_t tos;                /**< Type of service */
    int ttl;                    /**< ttl value (for IPv4) or unicast hops 
                                     (for IPv6), can be 0-255, while -1 means
                                     to use route default */

    unsigned long long timestamp;   /**< Current timestamp */
    
    struct netif_t *ifp;            /**< Network interface */
    
    struct socket_t *next;          /**< Pointer to next socket */

    struct socket_t *pairedsock;    /**< Pointer to paired socket */

    pid_t pid;  /**< Process id of task connected to this socket */
    uid_t uid;  /**< Effective user id */
    gid_t gid;  /**< Effective group id */

    /** fields for the TCP backlog queue */
    struct socket_t *backlog;   /**< Pointer to socket backlog */
    struct socket_t *parent;    /**< Pointer to parent socket */
    uint16_t max_backlog;       /**< Length of backlog list */
    uint16_t pending_connections;   /**< Pending connections */

    uint16_t pending_events;    /**< Pending events */
};

/**
 * @struct sockport_t
 * @brief The sockport_t structure.
 *
 * A structure to represent a socket port.
 */
struct sockport_t
{
    struct socket_t *sockets;   /**< Sockets connected to this port */
    uint16_t number;            /**< Port number */
    struct proto_t *proto;      /**< Pointer to protocol operations struct */
    struct sockport_t *next;    /**< Pointer to next port */
};

/**
 * @struct sockops_t
 * @brief The sockops_t structure.
 *
 * Each member of this structure contains a pointer to a function that
 * implements one of the socket syscall handlers, e.g. accept(), bind(), ...
 * Each protocol (TCP, UDP, RAW, UNIX, ...) will have its own structure
 * that points to the particular protocol's function handlers.
 */
struct sockops_t
{
    int (*connect2)(struct socket_t *, struct socket_t *);
            /**< Handler for the connect() call */
    int (*getsockopt)(struct socket_t *, int, int, void *, int *);
            /**< Handler for the getsockopt() call */
    int (*recvmsg)(struct socket_t *, struct msghdr *, unsigned int);
            /**< Handler for the recvmsg() call */
    int (*sendmsg)(struct socket_t *, struct msghdr *, unsigned int);
            /**< Handler for the sendmsg() call */
    int (*setsockopt)(struct socket_t *, int, int, void *, int);
            /**< Handler for the setsockopt() call */
    int (*socket)(int, int, struct socket_t **);
            /**< Handler for the socket() call */
};


/*******************************************
 * External definitions (syscall/socket.c)
 *******************************************/

extern struct kernel_mutex_t sockunix_lock;
extern struct socket_t *unix_socks;

extern struct kernel_mutex_t sockport_lock;
extern struct sockport_t *tcp_ports;
extern struct sockport_t *udp_ports;

extern struct kernel_mutex_t sockraw_lock;
extern struct socket_t *raw_socks;


/*****************************
 * Helper functions
 *****************************/

struct sockport_t *get_sockport(uint16_t proto, uint16_t port);
int is_port_free(int domain, uint16_t proto, uint16_t port, struct sockaddr *addr);
int sock_create(int domain, int type, int protocol, struct socket_t **res);

int do_sendto(struct socket_t *so, struct msghdr *msg, 
              void *src, void *dest, int flags, int kernel);

int sendto_pre_checks(struct socket_t *so, 
                      struct sockaddr *to, socklen_t tolen,
                      char *src_namebuf, char *dest_namebuf);

int socket_check(struct socket_t *so);
void socket_close(struct socket_t *so);
int socket_clone(struct socket_t *so, struct socket_t **res);
int socket_add(struct socket_t *so);
int socket_delete(struct socket_t *so);
void socket_wakeup(struct socket_t *so, uint16_t ev);

int socket_update_state(struct socket_t *so, uint16_t more_states, 
                        uint16_t less_states, uint16_t tcp_state);
int socket_error(struct packet_t *p, uint8_t proto);

//void socket_process_outqueues(void);

uint32_t socket_get_mss(struct socket_t *so);
struct netif_t *sock_get_ifp(struct socket_t *so);

void packet_copy_remoteaddr(struct socket_t *so, 
                            struct packet_t *p, struct msghdr *msg);

int sendto_get_ipv4_src(struct socket_t *so, 
                        struct sockaddr_in *dest,
                        struct sockaddr_in *res);

int socket_getsockopt(struct socket_t *so, int level, int optname,
                      void *optval, int *optlen);
int socket_setsockopt(struct socket_t *so, int level, int optname,
                      void *optval, int optlen);


/***********************
 * Function prototypes
 ***********************/

/**
 * @brief Handler for syscall socketcall().
 *
 * Switch function for socket operations.
 *
 * @param   call    one of the SOCK_* operations defined in sys/sockops.h
 * @param   args    call arguments (depend on the \a call)
 *
 * @return  zero or positive result on success, -(errno) on failure.
 */
int syscall_socketcall(int call, unsigned long *args);

/**
 * @brief Handler for syscall socket().
 *
 * Create a new socket.
 *
 * @param   domain      communication domain (see the AF_* definitions in 
 *                        sys/socket.h)
 * @param   type        socket type (see the SOCK_* definitions in 
 *                        sys/socket.h)
 * @param   protocol    communication protocol
 *
 * @return  zero or positive file descriptor on success, -(errno) on failure.
 */
int syscall_socket(int domain, int type, int protocol);

/**
 * @brief Handler for syscall bind().
 *
 * Bind a name to a socket.
 *
 * @param   s       file descriptor of open socket
 * @param   name    socket address
 * @param   namelen size of \a name in bytes
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_bind(int s, struct sockaddr *name, socklen_t namelen);

/**
 * @brief Handler for syscall connect().
 *
 * Connect to a socket.
 *
 * @param   s       file descriptor of open socket
 * @param   name    socket address
 * @param   namelen size of \a name in bytes
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_connect(int s, struct sockaddr *name, socklen_t namelen);

/**
 * @brief Handler for syscall listen().
 *
 * Listen for connections on a socket.
 *
 * @param   s       file descriptor of open socket
 * @param   backlog size of pending connections queue
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_listen(int s, int backlog);

/**
 * @brief Handler for syscall accept().
 *
 * Accept a connection on a socket.
 *
 * @param   s       file descriptor of open socket
 * @param   name    socket address
 * @param   namelen size of \a name in bytes
 *
 * @return  zero or positive file descriptor on success, -(errno) on failure.
 */
int syscall_accept(int s, struct sockaddr *name, socklen_t *namelen);

/**
 * @brief Handler for syscall getsockname().
 *
 * Get socket name.
 *
 * @param   s       file descriptor of open socket
 * @param   name    socket address
 * @param   namelen size of \a name in bytes
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getsockname(int s, struct sockaddr *name, socklen_t *namelen);

/**
 * @brief Handler for syscall getpeername().
 *
 * Get name of connected peer socket.
 *
 * @param   s       file descriptor of open socket
 * @param   name    socket address
 * @param   namelen size of \a name in bytes
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getpeername(int s, struct sockaddr *name, socklen_t *namelen);

/**
 * @brief Handler for syscall socketpair().
 *
 * Create a pair of connected sockets.
 *
 * @param   domain      communication domain (see the AF_* definitions in 
 *                        sys/socket.h)
 * @param   type        socket type (see the SOCK_* definitions in 
 *                        sys/socket.h)
 * @param   protocol    communication protocol
 * @param   rsv         file descriptors for 2 new sockets are returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_socketpair(int domain, int type, int protocol, int *rsv);

/**
 * @brief Handler for syscall sendto().
 *
 * Send a message on a socket.
 *
 * @param   __args      packed syscall arguments (see syscall.h and 
 *                        sys/socket.h)
 *
 * @return  number of bytes sent on success, -(errno) on failure.
 */
int syscall_sendto(struct syscall_args *__args);

/**
 * @brief Handler for syscall sendmsg().
 *
 * Send a message on a socket.
 *
 * @param   s       file descriptor of open socket
 * @param   _msg    message buffer (see struct msghdr definition in 
 *                     sys/socket.h)
 * @param   flags   flags (see the MSG_* definitions in sys/socket.h)
 *
 * @return  number of bytes sent on success, -(errno) on failure.
 */
int syscall_sendmsg(int s, struct msghdr *_msg, int flags);

/**
 * @brief Handler for syscall recvfrom().
 *
 * Receive a message from a socket.
 *
 * @param   __args      packed syscall arguments (see syscall.h and 
 *                        sys/socket.h)
 *
 * @return  number of bytes received on success, -(errno) on failure.
 */
int syscall_recvfrom(struct syscall_args *__args);

/**
 * @brief Handler for syscall recvmsg().
 *
 * Receive a message from a socket.
 *
 * @param   s       file descriptor of open socket
 * @param   _msg    message buffer (see struct msghdr definition in 
 *                     sys/socket.h)
 * @param   flags   flags (see the MSG_* definitions in sys/socket.h)
 *
 * @return  number of bytes received on success, -(errno) on failure.
 */
int syscall_recvmsg(int s, struct msghdr *_msg, int flags);

/**
 * @brief Handler for syscall shutdown().
 *
 * Shudown part of a full duplex connection.
 *
 * @param   s       file descriptor of open socket
 * @param   how     one of: SHUT_RD, SHUT_WR, or SHUT_RDWR (see sys/socket.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_shutdown(int s, int how);

/**
 * @brief Handler for syscall setsockopt().
 *
 * Set socket options.
 *
 * @param   s       file descriptor of open socket
 * @param   level   option level
 * @param   name    option name
 * @param   val     value buffer
 * @param   valsize size of buffer
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setsockopt(int s, int level, int name, void *val, int valsize);

/**
 * @brief Handler for syscall getsockopt().
 *
 * Get socket options.
 *
 * @param   s       file descriptor of open socket
 * @param   level   option level
 * @param   name    option name
 * @param   val     value buffer
 * @param   valsize size of buffer
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getsockopt(int s, int level, int name, void *val, int *valsize);

#endif      /* __KERNEL_SOCKETS_H__ */
