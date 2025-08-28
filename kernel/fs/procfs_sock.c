/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: procfs_sock.c
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
 *  \file procfs_sock.c
 *
 *  This file implements the functions needed to read socket info from
 *  /proc/net/tcp, /proc/net/udp, /proc/net/unix, and /proc/net/raw.
 */

#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/raw.h>
#include <mm/kheap.h>
#include <fs/procfs.h>

#define ADDR_BYTE(addr, shift)      (((addr) >> (shift)) & 0xff)

static inline void print_addr(char *buf, size_t bufsz, 
                              uint32_t addr, uint16_t port)
{
    ksprintf(buf, bufsz, "%02x%02x%02x%02x:%02x%02x ",
                          ADDR_BYTE(addr,  0),
                          ADDR_BYTE(addr,  8),
                          ADDR_BYTE(addr, 16),
                          ADDR_BYTE(addr, 24),
                          ADDR_BYTE(port, 0),
                          ADDR_BYTE(port, 8));
}

#undef ADDR_BYTE


static size_t get_non_unix(char **buf, int proto)
{
    struct socket_t *so;
    size_t len, count = 0, bufsz = 1024;
    char tmp[84];
    char *p;
    int i = 0;

    PR_MALLOC(*buf, bufsz);
    p = *buf;

    ksprintf(p, 84, "Num  LocalAddr     RemoteAddr    St   Fl   TxQueue:RxQueue\n");
    len = strlen(p);
    count += len;
    p += len;

    kernel_mutex_lock(&sock_lock);

    for(so = sock_head.next; so != NULL; so = so->next)
    {
        if(SOCK_PROTO(so) != proto)
        {
            continue;
        }

        ksprintf(tmp, 84, "%3d: ", i++);

        print_addr(tmp + 5, 84 - 5,
                       so->local_addr.ipv4, so->local_port);
        len = strlen(tmp);

        print_addr(tmp + len, 84 - len,
                       so->remote_addr.ipv4, so->remote_port);
        len = strlen(tmp);

        ksprintf(tmp + len, 84 - len, "%04x %02x  ", 
                       so->state, so->flags);
        len = strlen(tmp);

        ksprintf(tmp + len, 84 - len, "%08x:%08x\n", 
                       so->outq.count, so->inq.count);
        len = strlen(tmp);

        if(count + len >= bufsz)
        {
            char *tmp;

            if(!(tmp = krealloc(*buf, bufsz * 2)))
            {
                kernel_mutex_unlock(&sock_lock);
                return count;
            }

            bufsz *= 2;
            *buf = tmp;
            p = *buf + count;
        }

        count += len;
        strcpy(p, tmp);
        p += len;
    }

    kernel_mutex_unlock(&sock_lock);
    return count;
}


/*
 * Read /proc/net/tcp.
 */
size_t get_net_tcp(char **buf)
{
    return get_non_unix(buf, IPPROTO_TCP);
}


/*
 * Read /proc/net/udp.
 */
size_t get_net_udp(char **buf)
{
    return get_non_unix(buf, IPPROTO_UDP);
}


/*
 * Read /proc/net/raw.
 */
size_t get_net_raw(char **buf)
{
    return get_non_unix(buf, IPPROTO_RAW);
}


/*
 * Read /proc/net/unix.
 */
size_t get_net_unix(char **buf)
{
    struct socket_t *so;
    size_t len, count = 0, bufsz = 1024;
    char tmp[128];
    char *p;
    int i = 0;

    PR_MALLOC(*buf, bufsz);
    p = *buf;

    ksprintf(p, 128, "Num  Type St   Fl Path\n");
    len = strlen(p);
    count += len;
    p += len;

    kernel_mutex_lock(&sock_lock);

    for(so = sock_head.next; so != NULL; so = so->next)
    {
        if(so->domain != AF_UNIX)
        {
            continue;
        }

        // we print sun_path as a string here because we trust the unix
        // socket layer has ensured the name is null-terminated
        ksprintf(tmp, 128, "%3d: %04x %04x %02x %s\n",
                    i++, so->type, so->state, so->flags,
                         so->local_addr.sun.sun_path[0] ? 
                              so->local_addr.sun.sun_path : " ");
        len = strlen(tmp);

        if(count + len >= bufsz)
        {
            char *tmp;

            if(!(tmp = krealloc(*buf, bufsz * 2)))
            {
                kernel_mutex_unlock(&sock_lock);
                return count;
            }

            bufsz *= 2;
            *buf = tmp;
            p = *buf + count;
        }

        count += len;
        strcpy(p, tmp);
        p += len;
    }

    kernel_mutex_unlock(&sock_lock);
    return count;
}

