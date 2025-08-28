/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: rootfs.c
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
 *  \file rootfs.c
 *
 *  This file implements rootfs filesystem functions, the abstract virtual 
 *  filesystem the system boots into. We also initialize devfs, mount the
 *  initial ramdisk, and finally mount the proper root system from disk.
 */

//#define __DEBUG

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/clock.h>
#include <kernel/task.h>
#include <kernel/pcache.h>
#include <kernel/kparam.h>
#include <mm/kheap.h>
#include <fs/tmpfs.h>
#include <fs/devfs.h>


// root device of the system
dev_t ROOT_DEVID;


// forward declarations
static long rootfs_read_inode(struct fs_node_t *);
static long rootfs_write_inode(struct fs_node_t *);
static long rootfs_finddir(struct fs_node_t *, char *, struct dirent **, 
                           struct cached_page_t **, size_t *);
static long rootfs_addir(struct fs_node_t *, struct fs_node_t *, char *);

static int mount_initrd(dev_t initrd_dev);
static int switch_sysroot(dev_t initrd_dev);


extern char *basename(char *path);


// filesystem operations
struct fs_ops_t rootfs_ops =
{
    // inode operations
    .read_inode = rootfs_read_inode,
    .write_inode = rootfs_write_inode,
    //.trunc_inode = NULL,
    .alloc_inode = NULL,
    .free_inode = NULL,
    .bmap = NULL,

    .read_symlink = NULL,
    .write_symlink = NULL,

    // directory operations
    .finddir = rootfs_finddir,
    .finddir_by_inode = NULL,
    //.readdir = rootfs_readdir,
    .addir = rootfs_addir,
    .mkdir = NULL,
    .deldir = NULL,
    .dir_empty = NULL,
    .getdents = NULL,

    // device operations
    .mount = NULL,
    .umount = NULL,
    .read_super = NULL,
    .write_super = NULL,
    .put_super = NULL,
    .ustat = NULL,
    .statfs = NULL,
};

// shortcut to get to the root node of the system
struct fs_node_t *system_root_node;

// last inode # used on the root filesystem
static int last_node_num = 2;

// struct to represent root fs tree
struct
{
    char name[8];
    struct fs_node_t *node;
} root_tree[16];


static void rootfs_add_node(char *name, struct fs_node_t *localroot, 
                                        struct fs_info_t *localfs)
{
    struct fs_node_t *node;
    time_t t = now();

    // add an entry to the mount table
    if(!(node = get_empty_node()))
    {
        kpanic("Failed to create a node in rootfs!\n");
        empty_loop();
    }
    
    node->inode = last_node_num++;
    node->dev = system_root_node->dev;
    node->mode = S_IFDIR | 0555;
    node->links = 1;
    node->refs = 1;
    node->mtime = t;
    node->atime = t;
    node->ctime = t;
    node->uid = 0;
    node->gid = 0;
    node->flags = 0 /* FS_NODE_KEEP_INCORE */;

    if(localroot)
    {
        static struct mount_info_t *mtab = &mounttab[0];
        int res;

        mtab->dev = localroot->dev;
        mtab->flags = 0;
        mtab->root = localroot;
        mtab->mpoint = node;
        mtab->fs = localfs;
    
        node->flags |= FS_NODE_MOUNTPOINT;
        node->ptr = localroot;
        
        // special treatment for /mnt
        if(localfs->ops == &tmpfs_ops)
        {
            if(tmpfs_read_super(mtab->dev, mtab,
                tmpfs_ioctl(TO_DEVID(TMPFS_DEVID, 1),
                                        BLKSSZGET, (char *)&res, 1)) < 0)
            {
                kpanic("Failed to mount tmpfs on /mnt\n");
            }
        }
        
        mtab++;
    }
    
    strcpy(root_tree[node->inode].name, name);
    root_tree[node->inode].node = node;
}


struct fs_node_t *rootfs_init(void)
{
    fs_register("rootfs", &rootfs_ops);

    if(!(system_root_node = get_empty_node()))
    {
        kpanic("Failed to create rootfs!\n");
    }
    
    // create root node
    system_root_node->inode = 2;
    // use one of the reserved dev ids
    system_root_node->dev = ROOT_DEVID = TO_DEVID(240, 1);
    system_root_node->ops = &rootfs_ops;
    system_root_node->mode = S_IFDIR | 0555;
    system_root_node->links = 1;
    system_root_node->refs = 1;
    system_root_node->flags = 0 /* FS_NODE_KEEP_INCORE */;

    dev_t initrd_dev;
    struct fs_node_t *initrd, *devroot = devfs_create();
    struct dirent *entry;
    struct cached_page_t *dbuf;
    //struct IO_buffer_s *dbuf;
    size_t dbuf_off;
    char *path;
    int res;
    
    // we need these before calling vfs_mount()
    this_core->cur_task->fs->root = system_root_node;
    this_core->cur_task->fs->cwd = system_root_node;

    printk("Looking for initrd..\n");
    //__asm__("xchg %%bx, %%bx"::);
    
    // check for an initrd and if found, mount it as the sysroot
    if(devfs_finddir(devroot, "initrd", &entry, &dbuf, &dbuf_off) != 0)
    {
        printk("Could not find initrd..\n");
        goto load_default;
    }

    printk("Found initrd, trying to mount as sysroot..\n");
    //__asm__("xchg %%bx, %%bx"::);
    
    initrd = kmalloc(sizeof(struct fs_node_t));
    initrd->inode = entry->d_ino;
    initrd->dev = devroot->dev;
    kfree(entry);
    
    if(devfs_read_inode(initrd) < 0)
    {
        printk("Failed to get /dev/initrd inode..\n");
        kfree(initrd);
        initrd = NULL;
        goto load_default;
    }

    //KDEBUG("found initrd node @ 0x%x\n", &initrd);
    //__asm__("xchg %%bx, %%bx"::);
    
    // get the dev id
    initrd_dev = initrd->blocks[0];
    kfree(initrd);
    initrd = NULL;

    // mount sysroot
    if(mount_initrd(initrd_dev) < 0)
    {
        printk("Failed to mount sysroot!\n");
        //__asm__("xchg %%bx, %%bx"::);
        empty_loop();
    }

    printk("Sysroot mounted successfully..\n");
    //__asm__("xchg %%bx, %%bx"::);

    // adjust these to point to the mounted sysroot
    this_core->cur_task->fs->root = system_root_node;
    this_core->cur_task->fs->cwd = system_root_node;

    printk("Looking for 'root' parameter on kernel cmdline..\n");
    res = -1;

    if(has_cmdline_param("root") && (path = get_cmdline_param_val("root")))
    {
        dev_t dev;
        char *base = basename(path);
        struct fs_node_t *rootdisk;

        printk("Found root='%s'..\n", path);

        /*
        if((res = vfs_path_to_devid(path, "ext2", &dev)) < 0)
        {
            printk("%s: unable to resolve device path: %s (err %d)\n",
                    "rootfs", path, res);
        }
        else
        {
            if((res = vfs_mount(dev, "/rootfs", "ext2", 
                                         MS_RDONLY, "defaults")) != 0)
            {
                printk("%s: failed to mount %s on %s (err %d)\n",
                                "rootfs", path, "/rootfs", res);
            }
            else
            {
                printk("%s: mounted %s on %s\n", "rootfs", path, "/rootfs");
            }
        }
        */

        if(devfs_finddir(devroot, base, &entry, &dbuf, &dbuf_off) == 0)
        {
            if((rootdisk = kmalloc(sizeof(struct fs_node_t))))
            {
                rootdisk->inode = entry->d_ino;
                rootdisk->dev = devroot->dev;

                if(devfs_read_inode(rootdisk) == 0)
                {
                    printk("Mounting '%s'..\n", path);

                    // get the dev id
                    dev = rootdisk->blocks[0];

                    if((res = vfs_mount(dev, "/rootfs", "ext2", 
                                         MS_RDONLY, "defaults")) != 0)
                    {
                        printk("%s: failed to mount %s on %s (err %d)\n",
                                "rootfs", path, "/rootfs", res);
                    }
                    else
                    {
                        printk("%s: mounted %s on %s\n", "rootfs", path, "/rootfs");
                    }
                }
                
                kfree(rootdisk);
            }
            
            kfree(entry);
        }

        kfree(path);
    }

    printk("Trying to remount sysroot readonly..\n");

    if(res < 0)
    //if(mountall() < 0)
    {
        printk("Failed to mount sysroot.. Running from initrd..\n");
        //goto load_default;
        
        /*
        kpanic("Failed to mount sysroot\n");
        //__asm__("xchg %%bx, %%bx"::);
        empty_loop();
        */
    }
    else
    {
        if(switch_sysroot(initrd_dev) != 0)
        {
            printk("Failed to switch sysroot.. Running from initrd..\n");
        }
        else
        {
            printk("Sysroot remounted successfully..\n");
            KDEBUG("sysroot ptr @ 0x%x\n", system_root_node->ptr);
            KDEBUG("sysroot dev 0x%x, ino 0x%x\n", system_root_node->ptr->dev, system_root_node->ptr->inode);
        }
    }

    printk("Mounting /dev..\n");

    if(vfs_mount(DEV_DEVID, "/dev", "devfs", MS_RDONLY | /* MS_FORCE */ MS_REMOUNT, NULL) < 0)
    {
        printk("Failed to mount /dev!\n");
    }

    return system_root_node;


load_default:

    rootfs_add_node("bin", NULL, NULL);
    rootfs_add_node("dev", devroot, get_fs_by_name("devfs"));
    rootfs_add_node("mnt", tmpfs_create(64, 16, 1024), get_fs_by_name("tmpfs"));
    rootfs_add_node("proc", NULL, NULL);
    rootfs_add_node("root", NULL, NULL);
    rootfs_add_node("sbin", NULL, NULL);
    rootfs_add_node("usr", NULL, NULL);
    
    return system_root_node;
}


int mount_initrd(dev_t initrd_dev)
{
    struct mount_info_t *d;
    struct fs_info_t *fs;
    int flags = MS_RDONLY;
    int res, blocksz = 0;

    // find an empty slot
    if(!(d = mounttab_first_empty()))
    {
        return -ENOMEM;
    }

    // mark the device info struct in use
    d->dev = initrd_dev;

    // find the fstab module
    if((fs = get_fs_by_name("ext2")) == NULL)
    {
        d->dev = 0;
        return -EINVAL;
    }

    d->fs = fs;

    if(fs->ops && fs->ops->mount)
    {
        if((res = fs->ops->mount(d, flags, NULL)) < 0)
        {
            d->dev = 0;
            return res;
        }
    }

    // get the device's block size (bytes per sector)
    if(!bdev_tab[MAJOR(d->dev)].ioctl ||
       (res = bdev_tab[MAJOR(d->dev)].ioctl(d->dev, 
                                            BLKSSZGET, 
                                            (char *)&blocksz, 1)) < 0)
    {
        d->dev = 0;
        return -ENODEV;
    }

    // read the superblock
    if(!fs->ops || !fs->ops->read_super ||
       (res = fs->ops->read_super(d->dev, d, (size_t)blocksz)) < 0)
    {
        d->dev = 0;
        return res;
    }

    // fill in the rest of the structure
    d->mpoint = system_root_node;
    d->mountflags = flags;
    system_root_node->flags |= FS_NODE_MOUNTPOINT;
    system_root_node->ptr = d->root;
    //__asm__("xchg %%bx, %%bx"::);
    
    return 0;
}


int switch_sysroot(dev_t initrd_dev)
{
    struct fs_node_t *initrd_node, *rootfs_node;
    struct mount_info_t *d;
    int res;

    printk("Switching sysroot..\n");
    printk("Looking for /rootfs..\n");

    // get the mount point's node
    if((res = vfs_open("/rootfs", O_RDONLY, 0777, AT_FDCWD, 
                       &rootfs_node, OPEN_KERNEL_CALLER|OPEN_NOFOLLOW_MPOINT)) < 0)
    {
        return res;
    }

    // check the device is mounted
    if(!(d = get_mount_info2(rootfs_node)))
    {
        release_node(rootfs_node);
        return -ENOENT;
    }

    release_node(rootfs_node);      // refs = 1, dev 0x1fa, inode 0xf
    system_root_node = d->mpoint;
    system_root_node->ptr = d->root;
    system_root_node->refs++;       // refs = 2, dev /dev/[hs]da, inode 0x2
    system_root_node->flags |= /* FS_NODE_KEEP_INCORE | */ FS_NODE_MOUNTPOINT;
    d->root->refs++;

    d = get_mount_info(initrd_dev);
    remove_cached_node_pages(d->mpoint);
    release_node(d->mpoint);        // refs = 0

    // adjust these to point to the new sysroot
    /*
    ct->fs->root = system_root_node;
    ct->fs->cwd = system_root_node;
    system_root_node->refs += 2;
    */

    for_each_taskptr(t)
    {
        if(*t)
        {
            (*t)->fs->root = system_root_node;
            (*t)->fs->cwd = system_root_node;
            system_root_node->refs += 2;
        }
    }

    printk("Looking for /initrd..\n");
    //__asm__("xchg %%bx, %%bx"::);
    
    if((res = vfs_open("/initrd", O_RDONLY, 0777, AT_FDCWD, 
                       &initrd_node, OPEN_KERNEL_CALLER|OPEN_NOFOLLOW_MPOINT)) == 0)
    {
        d->mpoint = initrd_node;
        initrd_node->flags |= FS_NODE_MOUNTPOINT /* | FS_NODE_KEEP_INCORE */;
        initrd_node->ptr = d->root;
        printk("Found /initrd..\n");
    }
    else
    {
        release_node(d->root);
        d->root = NULL;
        d->dev = 0;
        printk("Failed to find /initrd..\n");
    }

    printk("Switched sysroot..\n");
    
    return 0;
}


long rootfs_read_inode(struct fs_node_t *node)
{
    if(!node)
    {
        return -EINVAL;
    }
    
    // we only handle '/'
    if(node->dev != ROOT_DEVID)
    {
        printk("rootfs: Can only read '/': dev %d, ino %d\n", 
                node->dev, node->inode);
        return -EINVAL;
    }
    
    int i;
    
    for(i = 2; i < last_node_num; i++)
    {
        if(root_tree[i].node && (root_tree[i].node->inode == node->inode))
        {
            node->mtime = root_tree[i].node->mtime;
            node->atime = root_tree[i].node->atime;
            node->ctime = root_tree[i].node->ctime;
            node->uid = root_tree[i].node->uid;
            node->gid = root_tree[i].node->gid;
            return 0;
        }
    }
    
    return -ENOENT;
}


long rootfs_write_inode(struct fs_node_t *node)
{
    if(!node)
    {
        return -EINVAL;
    }
    
    // we only handle '/'
    if(node->dev != ROOT_DEVID)
    {
        printk("rootfs: Can only write to '/': dev %d, ino %d\n", 
                node->dev, node->inode);
        return -EINVAL;
    }
    
    int i;
    
    for(i = 2; i < last_node_num; i++)
    {
        KDEBUG("rootfs_write_inode: i %d\n", i);

        if(root_tree[i].node && (root_tree[i].node->inode == node->inode))
        {
            root_tree[i].node->mtime = node->mtime;
            root_tree[i].node->atime = node->atime;
            root_tree[i].node->ctime = node->ctime;
            root_tree[i].node->uid = node->uid;
            root_tree[i].node->gid = node->gid;
            return 0;
        }
    }
    
    return -ENOENT;
}


STATIC_INLINE struct dirent *entry_to_dirent(int index, int off)
{
    int namelen = strlen(root_tree[index].name);
    unsigned int reclen = GET_DIRENT_LEN(namelen);
    struct dirent *entry = kmalloc(reclen);

    if(!entry)
    {
        return NULL;
    }

    entry->d_ino = index;
    entry->d_off = off;
    entry->d_type = DT_DIR;
    strcpy(entry->d_name, root_tree[index].name);
    entry->d_reclen = reclen;
    
    return entry;
}


/*
 * Find the given filename in the parent directory.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    filename => the searched-for filename
 *
 * Outputs:
 *    entry => if the filename is found, its entry is converted to a kmalloc'd
 *             dirent struct, and the result is stored in this field
 *    dbuf => the disk buffer representing the disk block containing the found
 *            filename, this is useful if the caller wants to delete the file
 *            after finding it (vfs_unlink(), for example)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                file's entry
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long rootfs_finddir(struct fs_node_t *dir, char *filename, 
                    struct dirent **entry,
                    struct cached_page_t **dbuf, size_t *dbuf_off)
{
    if(!dir || !dir->inode || !filename || !*filename)
    {
        return -EINVAL;
    }
    
    // we only handle '/'
    if(dir->dev != ROOT_DEVID || dir->inode != 2)
    {
        printk("Can only read '/': dev %d, ino %d\n", dir->dev, dir->inode);
        return -EINVAL;
    }

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;
    
    int i;
    
    for(i = 3; i < last_node_num; i++)
    {
        //printk("rootfs_finddir: %s, %s\n", root_tree[i].name, filename);

        if(strcmp(root_tree[i].name, filename) == 0)
        {
            *entry = entry_to_dirent(i, i+3);
            return *entry ? 0 : -ENOMEM;
        }
    }
    
    return -ENOENT;
}


long rootfs_addir(struct fs_node_t *dir, struct fs_node_t *file, char *filename)
{
    UNUSED(dir);
    UNUSED(filename);
    UNUSED(file);

    return -ENOSYS;
}

