/* Linux socket syscall subopcodes */

#ifndef _SOCKOPS_H

#define _SOCKOPS_H

#define SOCK_socket           1
#define SOCK_bind             2
#define SOCK_connect          3
#define SOCK_listen           4
#define SOCK_accept           5
#define SOCK_getsockname      6
#define SOCK_getpeername      7
#define SOCK_socketpair       8
#define SOCK_send             9
#define SOCK_recv             10
#define SOCK_sendto           11
#define SOCK_recvfrom         12
#define SOCK_shutdown         13
#define SOCK_setsockopt       14
#define SOCK_getsockopt       15
#define SOCK_sendmsg          16
#define SOCK_recvmsg          17


#if 0

#if defined(DEBUG) || defined(__DEBUG)

// op string names to help in printing debug msgs

static char *sockops_str[] =
{
    "NOOP",
    "SOCK_socket",
    "SOCK_bind",
    "SOCK_connect",
    "SOCK_listen",
    "SOCK_accept",
    "SOCK_getsockname",
    "SOCK_getpeername",
    "SOCK_socketpair",
    "SOCK_send",
    "SOCK_recv",
    "SOCK_sendto",
    "SOCK_recvfrom",
    "SOCK_shutdown",
    "SOCK_setsockopt",
    "SOCK_getsockopt",
    "SOCK_sendmsg",
    "SOCK_recvmsg",
};

#endif      /* DEBUG || __DEBUG */

#endif

#endif      /* _SOCKOPS_H */
