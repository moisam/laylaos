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
#include <kernel/net/socket.h>
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


static size_t get_tcp_or_udp(char **buf, struct sockport_t *ports)
{
    struct sockport_t *sp;
    struct socket_t *so;
    size_t len, count = 0, bufsz = 1024;
    char tmp[84];
    char *p;
    int i = 0;

    PR_MALLOC(*buf, bufsz);
    p = *buf;

    ksprintf(p, 84, "Num  LocalAddr    RemoteAddr   St   Fl\n");
    len = strlen(p);
    count += len;
    p += len;

    kernel_mutex_lock(&sockport_lock);

    for(sp = ports; sp != NULL; sp = sp->next)
    {
        for(so = sp->sockets; so != NULL; so = so->next, i++)
        {
            ksprintf(tmp, 84, "%3d: ", i);

            print_addr(tmp + 5, 84 - 5,
                       so->local_addr.ipv4.s_addr, so->local_port);
            len = strlen(tmp);

            print_addr(tmp + len, 84 - len,
                       so->remote_addr.ipv4.s_addr, so->remote_port);
            len = strlen(tmp);

            ksprintf(tmp + len, 84 - len, "%04x %02x\n", 
                       so->state, so->flags);
            len = strlen(tmp);

            if(count + len >= bufsz)
            {
                char *tmp;

                if(!(tmp = krealloc(*buf, bufsz * 2)))
                {
                    kernel_mutex_unlock(&sockport_lock);
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
    }

    kernel_mutex_unlock(&sockport_lock);
    return count;
}


/*
 * Read /proc/net/tcp.
 */
size_t get_net_tcp(char **buf)
{
    return get_tcp_or_udp(buf, tcp_ports);
}


/*
 * Read /proc/net/udp.
 */
size_t get_net_udp(char **buf)
{
    return get_tcp_or_udp(buf, udp_ports);
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

    kernel_mutex_lock(&sockunix_lock);

    for(so = unix_socks; so != NULL; so = so->next, i++)
    {
        // we print sun_path as a string here because we trust the unix
        // socket layer has ensured the name is null-terminated
        ksprintf(tmp, 128, "%3d: %04x %04x %02x %s\n",
                    i, so->type, so->state, so->flags,
                       so->local_addr.sun.sun_path[0] ? 
                            so->local_addr.sun.sun_path : " ");
        len = strlen(tmp);

        if(count + len >= bufsz)
        {
            char *tmp;

            if(!(tmp = krealloc(*buf, bufsz * 2)))
            {
                kernel_mutex_unlock(&sockunix_lock);
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

    kernel_mutex_unlock(&sockunix_lock);
    return count;
}


/*
 * Read /proc/net/raw.
 */
size_t get_net_raw(char **buf)
{
    struct socket_t *so;
    size_t len, count = 0, bufsz = 1024;
    char tmp[84];
    char *p;
    int i = 0;

    PR_MALLOC(*buf, bufsz);
    p = *buf;

    ksprintf(p, 84, "Num  LocalAddr    RemoteAddr   St   Fl\n");
    len = strlen(p);
    count += len;
    p += len;

    kernel_mutex_lock(&sockraw_lock);

    for(so = raw_socks; so != NULL; so = so->next, i++)
    {
        ksprintf(tmp, 84, "%3d: ", i);

        print_addr(tmp + 5, 84 - 5,
                       so->local_addr.ipv4.s_addr, so->local_port);
        len = strlen(tmp);

        print_addr(tmp + len, 84 - len,
                       so->remote_addr.ipv4.s_addr, so->remote_port);
        len = strlen(tmp);

        ksprintf(tmp + len, 84 - len, "%04x %02x\n", 
                       so->state, so->flags);
        len = strlen(tmp);

        if(count + len >= bufsz)
        {
            char *tmp;

            if(!(tmp = krealloc(*buf, bufsz * 2)))
            {
                kernel_mutex_unlock(&sockraw_lock);
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

    kernel_mutex_unlock(&sockraw_lock);
    return count;
}

