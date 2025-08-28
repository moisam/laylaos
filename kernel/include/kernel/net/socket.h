/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
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
 *  Functions and macros for working with sockets.
 */

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <sys/un.h>
#include <kernel/net/netif.h>

#define SOCKSTATE_FREE              0
#define SOCKSTATE_UNCONNECTED       1
#define SOCKSTATE_CONNECTING        2
#define SOCKSTATE_CONNECTED         3
#define SOCKSTATE_DISCONNECTING     4
#define SOCKSTATE_LISTENING         5

#define SOCKET_FLAG_TCPNODELAY      0x01
#define SOCKET_FLAG_NONBLOCK        0x02
#define SOCKET_FLAG_IPHDR_INCLUDED  0x04    /* for raw sockets */
#define SOCKET_FLAG_BROADCAST       0x08    /* for udp & raw sockets */
#define SOCKET_FLAG_RECVTOS         0x10
#define SOCKET_FLAG_RECVTTL         0x20    /* not for stream sockets */
#define SOCKET_FLAG_RECVOPTS        0x40    /* not for stream sockets */
#define SOCKET_FLAG_SHUT_LOCAL      0x80
#define SOCKET_FLAG_SHUT_REMOTE     0x100

#define SOCKET_DEFAULT_QUEUE_SIZE   (8 * 1024)

#define SOCKET_LOCK(s)                      \
    kernel_mutex_lock(&((s)->lock));

#define SOCKET_UNLOCK(s)                    \
    kernel_mutex_unlock(&((s)->lock));

#define RAW_SOCKET(so)      ((so)->proto && (so)->proto->sockops == &raw_sockops)

#define SOCK_PROTO(so)      (RAW_SOCKET(so) ? IPPROTO_RAW : \
                                              (so)->proto ? \
                                                 (so)->proto->protocol : 0)


/**
 * @union ip_addr_t
 * @brief The ip_addr_t union.
 *
 * A union to represent IP v4/v6 addresses.
 */
union ip_addr_t
{
    uint32_t ipv4;          /**< IPv4 address */
    uint8_t ipv6[16];       /**< IPv6 address */
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
    int flags;      /**< Socket flags */
    int refs;       /**< Socket references */
    volatile int state;      /**< Socket state */
    volatile int err;        /**< Last socket error */
    size_t peek_offset;     /**< peak offset (used with MSG_PEEK) */

    struct proto_t *proto;      /**< pointer to protocol operations struct */

    union ip_addr_t local_addr;     /**< Local IP address */
    union ip_addr_t remote_addr;    /**< Remote IP address */

    uint16_t local_port;            /**< Local port */
    uint16_t remote_port;           /**< Remote port */

    struct netif_queue_t inq;       /**< Input queue */
    struct netif_queue_t outq;      /**< Output queue */

    struct selinfo selrecv,     /**< Select channel for waiting receivers */
                   sleep;       /**< Select channel for everything else */

    uint8_t tos;                /**< Type of service */
    int ttl;                    /**< ttl value (for IPv4) or unicast hops 
                                     (for IPv6), can be 0-255, while -1 means
                                     to use route default */

#if 0

    unsigned long long timestamp;   /**< Current timestamp */
    
    struct netif_t *ifp;            /**< Network interface */

#endif

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

    uint16_t poll_events;       /**< Pending events */

    volatile struct kernel_mutex_t lock;     /**< Socket lock */
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
    long (*connect)(struct socket_t *);
            /**< Handler for the connect() call */
    long (*connect2)(struct socket_t *, struct socket_t *);
            /**< Handler for the connect2() call */
    long (*getsockopt)(struct socket_t *, int, int, void *, int *);
            /**< Handler for the getsockopt() call */
    long (*read)(struct socket_t *, struct msghdr *, unsigned int);
            /**< Handler for the recvmsg() call */
    long (*write)(struct socket_t *, struct msghdr *, int);
            /**< Handler for the sendmsg() call */
    long (*setsockopt)(struct socket_t *, int, int, void *, int);
            /**< Handler for the setsockopt() call */
    struct socket_t *(*socket)(void);
            /**< Handler for the socket() call */
};


/*******************************************
 * External definitions (socket.c)
 *******************************************/

/**
 * @var sock_head
 * @brief Socket head.
 *
 * The head of the socket list.
 */
extern struct socket_t sock_head;

/**
 * @var sock_lock
 * @brief Socket lock.
 *
 * The socket list lock.
 */
extern volatile struct kernel_mutex_t sock_lock;


/*****************************
 * Helper functions
 *****************************/

long sock_create(int domain, int type, int protocol, struct socket_t **res);
long sendto_pre_checks(struct socket_t *so, 
                       struct sockaddr *to, socklen_t tolen);
void socket_close(struct socket_t *so);
void socket_delete(struct socket_t *so, uint32_t expiry);
void sock_connected(struct socket_t *so);

struct socket_t *sock_find(struct socket_t *find);
struct socket_t *sock_lookup(uint16_t proto, uint16_t remoteport, uint16_t localport);

void socket_copy_remoteaddr(struct socket_t *so, struct msghdr *msg);

long socket_getsockopt(struct socket_t *so, int level, int optname,
                       void *optval, int *optlen);
long socket_setsockopt(struct socket_t *so, int level, int optname,
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
long syscall_socketcall(int call, unsigned long *args);

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
long syscall_socket(int domain, int type, int protocol);

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
long syscall_bind(int s, struct sockaddr *name, socklen_t namelen);

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
long syscall_connect(int s, struct sockaddr *name, socklen_t namelen);

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
long syscall_listen(int s, int backlog);

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
long syscall_accept(int s, struct sockaddr *name, socklen_t *namelen);

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
long syscall_getsockname(int s, struct sockaddr *name, socklen_t *namelen);

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
long syscall_getpeername(int s, struct sockaddr *name, socklen_t *namelen);

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
long syscall_socketpair(int domain, int type, int protocol, int *rsv);

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
long syscall_sendto(struct syscall_args *__args);

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
long syscall_sendmsg(int s, struct msghdr *_msg, int flags);

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
long syscall_recvfrom(struct syscall_args *__args);

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
long syscall_recvmsg(int s, struct msghdr *_msg, int flags);

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
long syscall_shutdown(int s, int how);

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
long syscall_setsockopt(int s, int level, int name, void *val, int valsize);

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
long syscall_getsockopt(int s, int level, int name, void *val, int *valsize);

#endif      /* NET_SOCKET_H */
