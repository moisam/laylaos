/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: mount.c
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
 *  \file mount.c
 *
 *  Functions for mounting and unmounting filesystems.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <mntent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/mutex.h>
#include <kernel/clock.h>
#include <kernel/task.h>
#include <kernel/dev.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/pcache.h>
#include <mm/kheap.h>
#include <fs/devpts.h>
#include <fs/devfs.h>


struct mount_info_t mounttab[NR_SUPER];
struct kernel_mutex_t mount_table_mutex;


/*
 * Write out modified superblocks to disk. Called by update().
 * If dev == NODEV, all modified superblocks are sync'd.
 */
void sync_super(dev_t dev)
{
    struct mount_info_t *d = mounttab;
    struct mount_info_t *ld = &mounttab[NR_SUPER];
    time_t tm = now();

    kernel_mutex_lock(&mount_table_mutex);
    
    for( ; d < ld; d++)
    {
        if(d->dev == 0)
        {
            continue;
        }
        
        if(dev != NODEV && d->dev != dev)
        {
            continue;
        }

        if(kernel_mutex_trylock(&d->flock))
        {
            continue;    // locked means busy, ignore it
        }

        kernel_mutex_unlock(&d->flock);
        
        if(kernel_mutex_trylock(&d->ilock))
        {
            continue;    // locked means busy, ignore it
        }

        kernel_mutex_unlock(&d->ilock);

        if(!(d->flags & FS_SUPER_DIRTY))
        {
            continue;    // update only modified superblocks
        }

        if(d->mountflags & MS_RDONLY)
        {
            continue;    // don't update systems mounted as read-only
        }

        /* update superblock by writing it to disk */
        d->flags &= ~FS_SUPER_DIRTY;
        d->update_time = tm;

        /* NOTE: We pass the superblock here because we can't call getfs() 
         *       from within the call to writesuper(). This will f*** up our
         *       mount_table_mutex.
         */
        if(d->fs && d->fs->ops && d->fs->ops->write_super)
        {
            d->fs->ops->write_super(d->dev, d->super);
        }
    }

    kernel_mutex_unlock(&mount_table_mutex);
}


/*
 * Get a mounted device's info.
 *
 * Returns:
 * pointer to the device's info struct, NULL if device not found
 */
struct mount_info_t *get_mount_info(dev_t dev)
{
    struct mount_info_t *d = mounttab;
    struct mount_info_t *ld = &mounttab[NR_SUPER];
    
    kernel_mutex_lock(&mount_table_mutex);

    for( ; d < ld; d++)
    {
        if(d->dev && d->dev == dev)
        {
            kernel_mutex_unlock(&mount_table_mutex);
            return d;
        }
    }
    
    kernel_mutex_unlock(&mount_table_mutex);

    return NULL;
}


struct mount_info_t *get_mount_info2(struct fs_node_t *node)
{
    struct mount_info_t *d = mounttab;
    struct mount_info_t *ld = &mounttab[NR_SUPER];
    
    kernel_mutex_lock(&mount_table_mutex);

    for( ; d < ld; d++)
    {
        if(d->mpoint == node || d->root == node)
        {
            kernel_mutex_unlock(&mount_table_mutex);
            return d;
        }
    }
    
    kernel_mutex_unlock(&mount_table_mutex);

    return NULL;
}


/*
 * Get an empty mount table entry.
 */
struct mount_info_t *mounttab_first_empty(void)
{
    struct mount_info_t *d = mounttab;
    struct mount_info_t *ld = &mounttab[NR_SUPER];

    kernel_mutex_lock(&mount_table_mutex);

    for( ; d < ld; d++)
    {
        if(d->dev == 0)
        {
            break;
        }
    }
    
    // table is full
    if(d == ld)
    {
        printk("vfs: mount table is full!\n");
        kernel_mutex_unlock(&mount_table_mutex);
        return NULL;
    }
    
    // reset flags and root node
    memset(d, 0, sizeof(struct mount_info_t));
    d->dev = -1;

    kernel_mutex_unlock(&mount_table_mutex);
    
    return d;
}


int vfs_remount(struct fs_node_t *mpoint_node,
                struct mount_info_t *oldd, int flags, char *options)
{
    size_t unused;
    
    // new mount point should be the same we used to mount the first time
    if(mpoint_node->dev != oldd->mpoint->dev ||
       mpoint_node->inode != oldd->mpoint->inode)
    {
        return -EINVAL;
    }

    if(oldd->mountopts)
    {
        kfree(oldd->mountopts);
        oldd->mountopts = NULL;
    }

    if(options &&
       copy_str_from_user(options, &oldd->mountopts, &unused) != 0)
    {
        return -EFAULT;
    }

    oldd->mountflags = flags;
    return 0;
}


/*
 * Mount the given device on the given path.
 *
 * Inputs:
 *   options -> filesystem-dependent mount options
 *              special options include:
 *                sysroot = for mounting sysroot on system start
 *                force = for forcing mount even if the filesystem is mounted
 *
 *   flags -> mount flags
 *
 * This function calls the filesystem's mount() function, which should read
 * the superblock and root inode and fill in some fields in the device info
 * struct (as well as the flags field after interpreting them).
 *
 * NOTE: The caller has to ensure path does NOT end in '/'!
 *
 * Returns:
 * 0 on success, -errno on failure
 */
int vfs_mount(dev_t dev, char *path, char *fstype, int flags, char *options)
{
    struct mount_info_t *d, *oldd;
    struct fs_info_t *fs;
    struct fs_node_t *mpoint_node;
    size_t unused;
    int res;
    int fremount = (flags & MS_REMOUNT);
    int rdonly = (flags & MS_RDONLY);
    int mounting_sysroot = 0;
	int open_flags = OPEN_USER_CALLER | OPEN_NOFOLLOW_MPOINT;
    struct task_t *ct = cur_task;

    if(ct->euid != 0)
    {
        return -EPERM;
    }
    
    if(!path || !*path)
    {
        return -EINVAL;
    }
    
    /*
     * The mount manpage says about the MS_STRICTATIME flag:
     *    "Always update the last access time (atime) when files on this
     *     filesystem are accessed.  (This was the default behavior before
     *     Linux 2.6.30.)  Specifying this flag overrides the effect of
     *     setting the MS_NOATIME and MS_RELATIME flags."
     */
    /*
    if(flags & MS_STRICTATIME)
    {
        flags &= ~(MS_NOATIME | MS_RELATIME);
    }
    */
    
    if(path[0] == '/' && path[1] == '\0')
    {
        /*
        if(!fsysroot)
        {
            return -EINVAL;
        }
        */
        
        mounting_sysroot = 1;
    }

    // get the mount point's node
    if((res = vfs_open(path, rdonly ? O_RDONLY : O_RDWR, 0777, AT_FDCWD, 
                       &mpoint_node, open_flags)) < 0)
    {
        return res;
    }
    
    // check the device is not already mounted
    if((oldd = get_mount_info(dev)) /* && !fforce */ && !fremount)
    {
        printk("vfs: device 0x%x is already mounted!\n", dev);
        release_node(mpoint_node);
        return -EBUSY;
    }
    
    if(!oldd && (oldd = get_mount_info2(mpoint_node)) /* && !fforce */ && !fremount)
    {
        printk("vfs: path is already mounted: %s\n", path);
        release_node(mpoint_node);
        return -EBUSY;
    }
    
    // mount exists, check if we are forcing or remounting it
    if(oldd)
    {
        // this is a remount
        if(fremount)
        {
            res = vfs_remount(mpoint_node, oldd, flags, options);
            release_node(mpoint_node);
            return res;
        }
        
        // this is a force mount, so unmount the device first
        if((res = vfs_umount(oldd->dev, MNT_FORCE)) < 0)
        {
            printk("vfs: failed to unmount device 0x%x\n", oldd->dev);
            release_node(mpoint_node);
            return res;
        }

        // get the mount point's node
        if((res = vfs_open(path, O_RDWR, 0777, AT_FDCWD, 
                           &mpoint_node, open_flags)) < 0)
        {
            return res;
        }
    }
    
    // find an empty slot
    if(!(d = mounttab_first_empty()))
    {
        release_node(mpoint_node);
        return -ENOMEM;
    }
    
    // call the device's open routine
    if(bdev_tab[MAJOR(dev)].open)
    {
        if((res = bdev_tab[MAJOR(dev)].open(dev)) < 0)
        {
            release_node(mpoint_node);
            return res;
        }
    }
    
    // mark the device info struct in use
    d->dev = dev;

    // find the fstab module
    if((fs = get_fs_by_name(fstype)) == NULL)
    {
        res = -EINVAL;
        goto err;
    }
    
    // store this as the filesystem's read_super() might need it to read
    // the root inode.
    d->fs = fs;
    
    if(fs->ops && fs->ops->mount)
    {
        if((res = fs->ops->mount(d, flags, options)) < 0)
        {
            goto err;
        }
    }

    KDEBUG("vfs_mount - mpoint_node->mode = 0x%x (0x%x), refs 0x%x\n", mpoint_node->mode, S_ISDIR(mpoint_node->mode), mpoint_node->refs);

    // check it's a directory and no one is referencing it (except sysroot,
    // which will have at least 2 refs, the one we got above, and the one that
    // is always resident in rootfs)
    res = mounting_sysroot ? 2 : 1;
    
    if(!S_ISDIR(mpoint_node->mode) || mpoint_node->refs > res)
    {
        res = -EBUSY;
        goto err;
    }

    KDEBUG("vfs_mount - maj 0x%x, ioctl @ 0x%lx\n", MAJOR(d->dev), bdev_tab[MAJOR(d->dev)].ioctl);
    res = -EINVAL;

    // get the device's block size (bytes per sector)
    if(!bdev_tab[MAJOR(d->dev)].ioctl ||
       (res = bdev_tab[MAJOR(d->dev)].ioctl(d->dev, 
                                            DEV_IOCTL_GET_BLOCKSIZE, 
                                            0, 1)) < 0)
    {
        KDEBUG("vfs_mount - 3 - res %d\n", res);
        goto err;
    }

    KDEBUG("vfs_mount - 4 - read_super @ 0x%lx\n", fs->ops ? fs->ops->read_super : 0);

    // read the superblock
    if(!fs->ops || !fs->ops->read_super ||
       (res = fs->ops->read_super(d->dev, d, (size_t)res)) < 0)
    {
        KDEBUG("vfs_mount - 4 - res %d\n", res);
        goto err;
    }

    if(options &&
       copy_str_from_user(options, &d->mountopts, &unused) != 0)
    {
        res = -EFAULT;
        KDEBUG("vfs_mount - 5 - res %d\n", res);
        goto err;
    }

    // fill in the rest of the structure
    d->mpoint = mpoint_node;
    d->mountflags = flags;
    mpoint_node->flags |= FS_NODE_MOUNTPOINT;
    mpoint_node->ptr = d->root;

    KDEBUG("vfs_mount - mpoint_node->dev 0x%x, d->root->dev 0x%x\n", mpoint_node->dev, d->root->dev);

    // init incore free block/inode locks
    init_kernel_mutex(&d->flock);
    init_kernel_mutex(&d->ilock);
    
    return 0;

err:
    d->dev = 0;
    d->fs = NULL;
    release_node(mpoint_node);
    return res;
}


/*
 * Unmount the given device.
 *
 * TODO: implement the flags as in:
 *       https://man7.org/linux/man-pages/man2/umount.2.html
 *
 * This function calls the filesystem's write_super() and put_super() 
 * functions, which should write the superblock and release its memory.
 *
 * Returns:
 * 0 on success, -errno on failure
 */
int vfs_umount(dev_t dev, int flags)
{
    struct mount_info_t *d;
    struct file_t *f, *lf;
    int fd;
    int force = (flags & MNT_FORCE /* MS_FORCE */);
    struct task_t *ct = cur_task;

    if(ct->euid != 0)
    {
        return -EPERM;
    }

    // get the device's mount info
    if((d = get_mount_info(dev)) == NULL)
    {
        return -EINVAL;
    }

    // check for open files
    for(f = ftab, lf = &ftab[NR_FILE]; f < lf; f++)
    {
        kernel_mutex_lock(&f->lock);
        
        if(f->node && f->node->dev == dev)
        {
            if(!force)
            {
            	kernel_mutex_unlock(&f->lock);
			    return -EBUSY;
			}
			
			// find the task that has this file open and screw it
			// (you asked for forced unmount, didn't you?)

            elevated_priority_lock(&task_table_lock);

            for_each_taskptr(t)
            {
                if(!*t)
                {
                    continue;
                }

                elevated_priority_unlock(&task_table_lock);
                
            	for(fd = 0; fd < NR_OPEN; fd++)
            	{
            		if(*t && (*t)->ofiles->ofile[fd] == f)
		            {
			            syscall_close(fd);
	                }
	            }

                elevated_priority_relock(&task_table_lock);
            }

            elevated_priority_unlock(&task_table_lock);
	    }
	    
	    kernel_mutex_unlock(&f->lock);
    }

    // check for outstanding inodes
    struct fs_node_t *node;
    struct fs_node_t *llnode = &node_table[NR_INODE];
    
    for(node = node_table; node < llnode; node++)
    {
        kernel_mutex_lock(&node->lock);
        kernel_mutex_unlock(&node->lock);

        if(node->refs &&        // if the node is being used ..
           node->inode != 0 &&  // and is a valid inode ..
           node->dev == dev &&  // and is from the same device ..
           node != d->root)     // and is not the root node, then release it
        {
            if(!force)
            {
			    return -EBUSY;
			}
			
			release_node(node);
	    }
	}
    
    invalidate_dev_dentries(dev);

    // sync nodes
    sync_nodes(dev);
    
    // flush disk buffers
    flush_cached_pages(dev);

    // write the superblock
    if(d->fs->ops->write_super)
    {
        d->fs->ops->write_super(dev, d->super);
    }
    
    // release the mounted filesystem's root node
    release_node(d->root);
    d->root->minfo = NULL;
    d->root = NULL;
    
    // call the device's close routine
    if(bdev_tab[MAJOR(dev)].close)
    {
        bdev_tab[MAJOR(dev)].close(dev);
    }
    
    KDEBUG("vfs_umount: dev 0x%x, d->mpoint->dev 0x%x\n", dev, d->mpoint->dev);
    
    // release the mount point's node
    kernel_mutex_lock(&d->mpoint->lock);
    d->mpoint->flags &= ~FS_NODE_MOUNTPOINT;
    d->mpoint->ptr = NULL;
    kernel_mutex_unlock(&d->mpoint->lock);
    release_node(d->mpoint);
    d->mpoint->minfo = NULL;
    d->mpoint = NULL;

    // release the superblock
    if(d->fs->ops->put_super)
    {
        d->fs->ops->put_super(dev, d->super);
    }

    d->super = NULL;

    if(d->mountopts)
    {
        kfree(d->mountopts);
        d->mountopts = NULL;
    }

    // free mount table slot
    kernel_mutex_lock(&mount_table_mutex);
    d->dev = 0;
    kernel_mutex_unlock(&mount_table_mutex);

    remove_cached_disk_pages(dev);

    // once again, ensure all refs to this device's mount info is invalidated
    for(node = node_table; node < llnode; node++)
    {
        kernel_mutex_lock(&node->lock);
        kernel_mutex_unlock(&node->lock);

        if(node->dev == dev)
        {
            node->minfo = NULL;
	    }
	}
    
    return 0;
}


#define MNTALL_BUFSZ            0x1000
#define BOOT_MNTFILE            "/etc/boot_fstab"
#define MNTFILE                 "/etc/fstab"

static char *malloc_str(char *str, size_t len)
{
    char *s2 = (char *)kmalloc(len + 1);
    
    if(!s2)
    {
        return NULL;
    }
    
    memcpy(s2, str, len);
    s2[len] = '\0';

    return s2;
}


/*
 * Internal mount function (for kernel use only).
 *
 * If boot_mount is non-zero, this is the initial mount during boot time,
 * and we read the contents of /etc/boot_fstab. Otherwise, this is a normal
 * mount (after the system is up and running), and we read /etc/fstab.
 * The latter case happens e.g. when the cdrom task is trying to auto-mount
 * a newly inserted CD-ROM. In such cases devpath is the path of the disk
 * device we want to mount. If devpath is NULL, everything is mounted (this
 * happens only at boottime, so boot_mount should be non-zero here).
 */
int mount_internal(char *module, char *devpath, int boot_mount)
{
    int res = 0;
    dev_t dev;
    struct fs_node_t *fnode;
    off_t fpos = 0;
    char *buf;
    static char word[1024];
    char *wordp, *wordp2 = &word[1024];
    char *fields[6];
    ssize_t i, j;
    int n;
    int flags = MS_RDONLY;
    struct task_t *ct = cur_task;
    
    if(ct->euid != 0)
    {
        printk("%s: permission error: not root user\n", module);
        return -EPERM;
    }
    
    printk("%s: opening %s\n", module, boot_mount ? BOOT_MNTFILE : MNTFILE);
    
    if((res = vfs_open_internal(boot_mount ? BOOT_MNTFILE : MNTFILE,
                                AT_FDCWD, &fnode, 
                                OPEN_KERNEL_CALLER)) < 0)
    {
        printk("%s: failed to open %s (err %d)\n",
                module, boot_mount ? BOOT_MNTFILE : MNTFILE, res);
        return res;
    }

    if(!(buf = (char *)kmalloc(fnode->size)))
    {
        printk("%s: insufficient memory\n", module);
        release_node(fnode);
        return -ENOMEM;
    }
    
    wordp = word;
    n = 0;
    memset(fields, 0, sizeof(fields));

    printk("%s: reading %s\n", module, boot_mount ? BOOT_MNTFILE : MNTFILE);

    if((i = vfs_read_node(fnode, &fpos, (unsigned char *)buf, fnode->size, 1)) > 0)
    {
        char *b = buf;
        char *bend = buf + i;
        
        while(b < bend)
        {
            // skip commented lines
            if(*b == '#')
            {
                while(b < bend && *b != '\n')
                {
                    b++;
                }
                
                if(n == 0)
                {
                    b++;
                    continue;
                }
            }

            if(*b == ' ' || *b == '\t' || *b == '\n')
            {
                *wordp = '\0';

                if(!(fields[n] = malloc_str(word, (size_t)(wordp - word))))
                {
                    printk("%s: insufficient memory!\n", module);
                    res = -ENOMEM;
                    goto done;
                }
                
                if(*b == '\n')
                {
                    if(strcmp(fields[2], "ignore") != 0)
                    {
                        if((res = vfs_path_to_devid(fields[0], 
                                                    fields[2], &dev)) < 0)
                        {
                            printk("%s: unable to resolve device path:"
                                    " %s (err %d)\n", module, fields[0], res);
                            goto done;
                        }
                        
                        if(devpath == NULL ||
                           strcmp(devpath, fields[0]) == 0)
                        {
                            printk("%s: %s, %s, %s, %s, dev 0x%x\n",
                                    module, fields[0], fields[1],
                                    fields[2], fields[3], dev);
                        
                            if((res = vfs_mount(dev, fields[1], fields[2], 
                                                     flags, fields[3])) != 0)
                            {
                                printk("%s: failed to mount %s on %s"
                                        " (err %d)\n", module, fields[0],
                                                       fields[1], res);
                                goto done;
                            }

                            printk("%s: mounted %s on %s\n", 
                                    module, fields[0], fields[1]);
                            //__asm__("xchg %%bx, %%bx"::);
                        }
                    }
                    
                    for(j = 0; j < 6; j++)
                    {
                        kfree(fields[j]);
                    }

                    memset(fields, 0, sizeof(fields));
                    n = 0;
                }
                else
                {
                    n++;
                }
                
                while(*b == ' ' || *b == '\t' || *b == '\n')
                {
                    b++;
                }

                wordp = word;
                continue;
            }
            
            if(wordp == wordp2)
            {
                printk("%s: very long field\n", module);
                res = -ENOMEM;
                goto done;
            }
            
            *wordp++ = *b++;
        }
    }

    for(j = 0; j < 6; j++)
    {
        kfree(fields[j]);
    }

done:

    printk("%s: done\n", module);

    release_node(fnode);
    kfree(buf);
    
    return res;
}


/*
 * Initial mount.
 */
int mountall(void)
{
    return mount_internal("mountall", NULL, 1);
}


int vfs_path_to_devid(char *source, char *fstype, dev_t *dev)
{
    int res;
    struct fs_node_t *fnode = NULL;
	int open_flags = OPEN_USER_CALLER | OPEN_FOLLOW_SYMLINK;
	
	*dev = 0;

    if(!source || !*source)
    {
        return -EINVAL;
    }

    KDEBUG("vfs_path_to_devid - 0a - fstype 0x%lx, dev 0x%lx\n", (uintptr_t)fstype, (uintptr_t)dev);

    if(fstype && *fstype)
    {
        if(strcmp(fstype, "devfs") == 0)
        {
            *dev = DEV_DEVID;
            return 0;
        }
        else if(strcmp(fstype, "tmpfs") == 0)
        {
            *dev = TO_DEVID(TMPFS_DEVID, 0);
            return 0;
        }
        else if(strcmp(fstype, "procfs") == 0)
        {
            *dev = PROCFS_DEVID;
            return 0;
        }
        else if(strcmp(fstype, "devpts") == 0)
        {
            *dev = DEVPTS_DEVID;
            return 0;
        }
    }
    
    if((res = vfs_open_internal(source, AT_FDCWD, &fnode, open_flags)) < 0)
    {
        KDEBUG("vfs_path_to_devid - res %d\n", res);
        return res;
    }

    if(!S_ISBLK(fnode->mode))
    {
        release_node(fnode);
        return -ENOTBLK;
    }

    *dev = (dev_t)fnode->blocks[0];
    release_node(fnode);
    
    KDEBUG("vfs_path_to_devid: dev 0x%x, maj 0x%x, min 0x%x\n", *dev, MAJOR(*dev), MINOR(*dev));

    if(MAJOR(*dev) >= NR_DEV)
    {
        return -ENXIO;
    }

    return 0;
}

