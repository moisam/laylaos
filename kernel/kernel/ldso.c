/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ldso.c
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
 *  \file ldso.c
 *
 *  Load the dynamic loader (ldso) to memory.
 */

//#define __DEBUG

#include <errno.h>
#include <fcntl.h>
#include <kernel/laylaos.h>
#include <kernel/elf.h>
#include <kernel/task.h>
#include <kernel/pcache.h>


/*
 * Load the dynamic loader (ldso).
 */
int ldso_load(size_t *auxv)
{
	int open_flags = OPEN_KERNEL_CALLER | OPEN_FOLLOW_SYMLINK |
	                 OPEN_CREATE_DENTRY;
    struct fs_node_t *filenode = NULL;
    struct cached_page_t *buf = NULL;
    int res;
    char **tmp;

    static char *ldso_paths[] =
    {
        "/usr/lib/ld.so",
        "/usr/local/lib/ld.so",
        "/bin/ld.so",
        NULL
    };
    
    for(tmp = ldso_paths; *tmp; tmp++)
    {
        // get the executable's file node
        if((res = vfs_open_internal(*tmp, AT_FDCWD,
                                    &filenode, open_flags)) == 0)
        {
            break;
        }
    }
    
    if(!filenode)
    {
        return -ENOENT;
    }

    // check it is a regular file
    if(!S_ISREG(filenode->mode))
    {
        KDEBUG("ldso_load - filenode->mode = %d\n", filenode->mode);
        printk("Kernel: failed to load ld.so (1, errno %d)\n", EACCES);
        release_node(filenode);
        return -EACCES;
    }

    // read executable header
    if(!(buf = get_cached_page(filenode, 0, 0)))
    {
        release_node(filenode);
        printk("Kernel: failed to load ld.so (2, errno %d)\n", EACCES);
        return -EACCES;
	}

    // load ELF file sections to memory
    res = elf_load_file(filenode, buf, auxv, 0);
    release_node(filenode);
    release_cached_page(buf);

    if(res != 0)
    {
        KDEBUG("ldso_load - 9a - res = %d\n", res);
        printk("Kernel: failed to load ld.so (3, errno %d)\n", -res);
        return res;
    }
    
    KDEBUG("ldso_load: ldso loaded\n");
    
    return 0;
}

