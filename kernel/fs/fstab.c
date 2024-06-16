/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: fstab.c
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
 *  \file fstab.c
 *
 *  This file contains the master filesystem table, along with different 
 *  functions to register filesystems and to work with the table.
 */

#include <string.h>
#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/mutex.h>
#include <kernel/user.h>
#include <fs/ext2.h>
#include <fs/devfs.h>
#include <fs/tmpfs.h>
#include <fs/sockfs.h>
#include <fs/procfs.h>
#include <fs/iso9660fs.h>
#include <fs/devpts.h>
#include <mm/kheap.h>


struct fs_info_t fstab[NR_FILESYSTEMS];

struct task_t *update_task = NULL;


static void update_function(void *arg)
{
    UNUSED(arg);

    while(1)
    {
        // schedule a disk update every 30 secs
        block_task2(&update_task, PIT_FREQUENCY * 30);
        update(NODEV);
    }
}


/*
 * Initialise the filesystem table.
 */
void init_fstab(void)
{
    memset(fstab, 0, sizeof(fstab));
    memset(mounttab, 0, sizeof(struct mount_info_t) * NR_SUPER);
    memset(node_table, 0, sizeof(struct fs_node_t) * NR_INODE);
    memset(ftab, 0, sizeof(struct file_t) * NR_FILE);
    
    // we need to register this first in order to read initrd
    fs_register("ext2", &ext2fs_ops);

    printk("Initializing tmpfs..\n");
    tmpfs_init();

    //printk("Initializing sockfs..\n");
    //sockfs_init();

    printk("Initializing devfs..\n");
    devfs_init();
    
    printk("Initializing procfs..\n");
    procfs_init();

    printk("Initializing ISO9660..\n");
    iso9660fs_init();
    
    printk("Initializing devpts..\n");
    devpts_init();

    // init rootfs last
    printk("Mounting root file system..\n");
    rootfs_init();

    (void)start_kernel_task("update", update_function, NULL, &update_task, 0);
}


/*
 * Get filesystem by name.
 */
struct fs_info_t *get_fs_by_name(char *name)
{
    struct fs_info_t *f = fstab;
    struct fs_info_t *lf = &fstab[NR_FILESYSTEMS];
    
    for( ; f < lf; f++)
    {
        if(strcmp(name, f->name) == 0)
        {
            return f;
        }
    }
    
    return NULL;
}


/*
 * Get filesystem by index.
 */
struct fs_info_t *get_fs_by_index(unsigned int index)
{
    struct fs_info_t *f = fstab;
    struct fs_info_t *lf = &fstab[NR_FILESYSTEMS];
    
    for( ; f < lf; f++)
    {
        if(f->index == index)
        {
            return f;
        }
    }
    
    return NULL;
}


int get_fs_count(void)
{
    struct fs_info_t *f = fstab;
    struct fs_info_t *lf = &fstab[NR_FILESYSTEMS];
    int count = 0;
    
    for( ; f < lf; f++)
    {
        if(f->name[0] != '\0')
        {
            count++;
        }
    }
    
    return count;
}


/*
 * Register a filesystem.
 */
struct fs_info_t *fs_register(char *name, struct fs_ops_t *ops)
{
    struct fs_info_t *f = fstab;
    struct fs_info_t *lf = &fstab[NR_FILESYSTEMS];
    
    if(!name || !*name || !ops)
    {
        return NULL;
    }
    
    if(strlen(name) > 7)
    {
        return NULL;
    }
    
    // check for duplicates
    if(get_fs_by_name(name))
    {
        printk("vfs: filesystem %s is already registered\n", name);
        return NULL;
    }
    
    for( ; f < lf; f++)
    {
        if(f->name[0] == '\0')
        {
            strcpy(f->name, name);
            f->ops = ops;
            f->index = f - fstab;
            return f;
        }
    }
    
    return NULL;
}


/*
 * Handler for syscall sysfs().
 *
 * Return information about the filesystem types currently present in the
 * kernel, depending on the given 'option'.
 */
int syscall_sysfs(int option, uintptr_t fsid, char *buf)
{
    struct fs_info_t *fs;
    char *name = NULL;
    size_t namelen = 0;

    switch(option)
    {
        case 1:
            if(copy_str_from_user((char *)fsid, &name, &namelen) != 0)
            {
                return -EFAULT;
            }
            
            if(!(fs = get_fs_by_name(name)))
            {
                kfree(name);
                return -EINVAL;
            }
            
            kfree(name);
            return fs->index;

        case 2:
            if(!(fs = get_fs_by_index(fsid)))
            {
                kfree(name);
                return -EINVAL;
            }
            
            COPY_TO_USER(buf, fs->name, strlen(fs->name) + 1);
            return 0;

        case 3:
            return get_fs_count();
    }
    
    return -EINVAL;
}

