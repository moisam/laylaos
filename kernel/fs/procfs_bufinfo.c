/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: procfs_bufinfo.c
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
 *  \file procfs_bufinfo.c
 *
 *  This file implements the functions needed to read /proc/buffers.
 */

#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/dev.h>
#include <kernel/net/socket.h>
#include <kernel/net/tcp.h>
#include <kernel/net/raw.h>
#include <fs/procfs.h>
#include <fs/dentry.h>
#include <mm/kheap.h>


static void inodeentry_getinfo(int i);
static void taskentry_getinfo(int i);
static void tcpsock_getinfo(int i);
static void udpsock_getinfo(int i);
static void unixsock_getinfo(int i);
static void rawsock_getinfo(int i);
static void pcache_getinfo(int i);
static void superblocks_getinfo(int i);
static void dentries_getinfo(int i);


struct
{
    char *name;
    int active, num, itemsz, totalsz;
    void (*getinfo)(int);
} proc_bufinfo[] =
{
    { "inode_entry", 0, 0, 0, 0, inodeentry_getinfo, },
    { "task_entry", 0, 0, 0, 0, taskentry_getinfo, },
    { "tcp_socks", 0, 0, 0, 0, tcpsock_getinfo, },
    { "udp_socks", 0, 0, 0, 0, udpsock_getinfo, },
    { "unix_socks", 0, 0, 0, 0, unixsock_getinfo, },
    { "raw_socks", 0, 0, 0, 0, rawsock_getinfo, },
    { "page_cache", 0, 0, 0, 0, pcache_getinfo, },
    { "superblocks", 0, 0, 0, 0, superblocks_getinfo },
    { "dentry", 0, 0, 0, 0, dentries_getinfo },
    { NULL, 0, 0, 0, 0, NULL, },
};


static void inodeentry_getinfo(int i)
{
    struct fs_node_t *node;
    struct fs_node_t *lnode = &node_table[NR_INODE];

    //proc_bufinfo[i].active = 0;
    proc_bufinfo[i].num = NR_INODE;
    proc_bufinfo[i].itemsz = sizeof(struct fs_node_t);
    proc_bufinfo[i].totalsz = NR_INODE * sizeof(struct fs_node_t);

    for(node = node_table; node < lnode; node++)
    {
        if(node->refs)
        {
            proc_bufinfo[i].active++;
        }
    }
}


static void taskentry_getinfo(int i)
{
    int j;

    for(j = 0; j < NR_TASKS; j++)
    {
        if(task_table[j] != NULL)
        {
            proc_bufinfo[i].active++;
        }
    }

    proc_bufinfo[i].num = NR_TASKS;
    proc_bufinfo[i].itemsz = sizeof(struct task_t);
    proc_bufinfo[i].totalsz = NR_TASKS * sizeof(struct task_t);
}


static void tcp_udp_sock_getinfo(int i, struct sockport_t *ports)
{
    struct sockport_t *sp;
    struct socket_t *so;

    kernel_mutex_lock(&sockport_lock);

    for(sp = ports; sp != NULL; sp = sp->next)
    {
        for(so = sp->sockets; so != NULL; so = so->next)
        {
            proc_bufinfo[i].active++;
        }
    }

    kernel_mutex_unlock(&sockport_lock);

    /*
     * NOTE: The calculated number of active sockets is not correct.
     * TODO: Maybe only count connected sockets?
     */
    proc_bufinfo[i].num = proc_bufinfo[i].active;
}


static void tcpsock_getinfo(int i)
{
    tcp_udp_sock_getinfo(i, tcp_ports);

    proc_bufinfo[i].itemsz = sizeof(struct socket_tcp_t);
    proc_bufinfo[i].totalsz = proc_bufinfo[i].num * sizeof(struct socket_tcp_t);
}


static void udpsock_getinfo(int i)
{
    tcp_udp_sock_getinfo(i, udp_ports);

    proc_bufinfo[i].itemsz = sizeof(struct socket_t);
    proc_bufinfo[i].totalsz = proc_bufinfo[i].num * sizeof(struct socket_t);
}


static void unixsock_getinfo(int i)
{
    struct socket_t *so;

    kernel_mutex_lock(&sockunix_lock);

    for(so = unix_socks; so != NULL; so = so->next, i++)
    {
        proc_bufinfo[i].active++;
    }

    kernel_mutex_unlock(&sockunix_lock);

    /*
     * NOTE: The calculated number of active sockets is not correct.
     * TODO: Maybe only count connected sockets?
     */
    proc_bufinfo[i].num = proc_bufinfo[i].active;

    proc_bufinfo[i].itemsz = sizeof(struct socket_t);
    proc_bufinfo[i].totalsz = proc_bufinfo[i].num * sizeof(struct socket_t);
}


static void rawsock_getinfo(int i)
{
    struct socket_t *so;

    kernel_mutex_lock(&sockraw_lock);

    for(so = raw_socks; so != NULL; so = so->next, i++)
    {
        proc_bufinfo[i].active++;
    }

    kernel_mutex_unlock(&sockraw_lock);

    /*
     * NOTE: The calculated number of active sockets is not correct.
     * TODO: Maybe only count connected sockets?
     */
    proc_bufinfo[i].num = proc_bufinfo[i].active;

    proc_bufinfo[i].itemsz = sizeof(struct socket_raw_t);
    proc_bufinfo[i].totalsz = proc_bufinfo[i].num * sizeof(struct socket_raw_t);
}


static void pcache_getinfo(int i)
{
    proc_bufinfo[i].active = get_cached_page_count() + get_cached_block_count();
    proc_bufinfo[i].num = proc_bufinfo[i].active;
    proc_bufinfo[i].itemsz = PAGE_SIZE;
    proc_bufinfo[i].totalsz = proc_bufinfo[i].num * PAGE_SIZE;
}


static void superblocks_getinfo(int i)
{
    struct mount_info_t *d = mounttab;
    struct mount_info_t *ld = &mounttab[NR_SUPER];
    
    for( ; d < ld; d++)
    {
        if(d->dev == 0 || !d->super)
        {
            continue;
        }

        proc_bufinfo[i].active++;
        proc_bufinfo[i].totalsz += d->super->blocksz;
    }

    proc_bufinfo[i].itemsz = 0;     // not all superblocks have the same size
    proc_bufinfo[i].num = NR_SUPER;
}


static void dentries_getinfo(int i)
{
    struct dentry_t *ent;
    struct dentry_list_t *list;
    int maj, min;

    for(maj = 0; maj < NR_DEV; maj++)
    {
        if(!bdev_tab[maj].dentry_list)
        {
            continue;
        }

        for(min = 0; min < NR_DEV; min++)
        {
            list = &(bdev_tab[maj].dentry_list[min]);

            for(ent = list->first_dentry; ent != NULL; ent = ent->dev_next)
            {
                if(ent->refs)
                {
                    proc_bufinfo[i].active++;
                }

                proc_bufinfo[i].num++;
            }
        }
    }

    /*
     * NOTE: the itemsz we store here DOES NOT include the memory allocated
     *       to the dentry's actual path component
     */
    proc_bufinfo[i].itemsz = sizeof(struct dentry_t);
    proc_bufinfo[i].totalsz = proc_bufinfo[i].num * sizeof(struct dentry_t);
}


/*
 * Read /proc/buffers.
 */
size_t get_buffer_info(char **buf)
{
    size_t len, count = 0, bufsz = 2048;
    char tmp[64];
    char *p;
    int i;

    PR_MALLOC(*buf, bufsz);
    p = *buf;
    *p = '\0';

    ksprintf(p, 256, "name             active  num itemsz totalsz\n");
    len = strlen(p);
    count += len;
    p += len;

    // first collect the needed info
    for(i = 0; proc_bufinfo[i].name != NULL; i++)
    {
        proc_bufinfo[i].active = 0;
        proc_bufinfo[i].num = 0;
        proc_bufinfo[i].itemsz = 0;
        proc_bufinfo[i].totalsz = 0;
        proc_bufinfo[i].getinfo(i);
    }

    // then print to the buffer
    for(i = 0; proc_bufinfo[i].name != NULL; i++)
    {
        ksprintf(tmp, 64, "%-16s %6d %4d %6d %7d\n",
                          proc_bufinfo[i].name, proc_bufinfo[i].active,
                          proc_bufinfo[i].num, proc_bufinfo[i].itemsz,
                          proc_bufinfo[i].totalsz);
        len = strlen(tmp);

        if(count + len >= bufsz)
        {
            PR_REALLOC(*buf, bufsz, count);
            p = *buf + count;
        }

        count += len;
        strcpy(p, tmp);
        p += len;
    }

    return count;
}

