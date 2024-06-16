/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: kio.c
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
 *  \file kio.c
 *
 *  Helper functions to read config files (e.g. /etc/groups).
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/kio.h>
#include <mm/kheap.h>


int kread_file(char *path, char **data, size_t *len)
{
    int res = 0;
    struct fs_node_t *fnode;
    off_t fpos = 0;
    char *buf;
    
    *data = NULL;
    *len = 0;

    if((res = vfs_open_internal(path, AT_FDCWD, 
                                    &fnode, OPEN_KERNEL_CALLER)) < 0)
    {
        printk("kernel: failed to open %s (err %d in kread_file)\n", 
               path, res);
        return res;
    }

    if(!(buf = (char *)kmalloc(fnode->size)))
    {
        printk("kernel: insufficient memory (kread_file)\n");
        release_node(fnode);
        return -ENOMEM;
    }

    if((res = vfs_read_node(fnode, &fpos, (unsigned char *)buf,
                                fnode->size, 1)) >= 0)
    {
        release_node(fnode);
        *data = buf;
        *len = res;
        return 0;
    }

    // error
    release_node(fnode);
    kfree(buf);
    return res;
}

