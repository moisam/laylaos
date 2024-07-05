/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: devfs.c
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
 *  \file devfs.c
 *
 *  This file implements the devfs filesystem, which provides access to all
 *  devices on the system, and is usually mounted under /dev.
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref devfs_ops structure.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <kernel/laylaos.h>
#include <kernel/dev.h>
#include <kernel/clock.h>
#include <mm/kheap.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <fs/dummy.h>

// device id for devfs
dev_t DEV_DEVID = TO_DEVID(240, 2);

// filesystem operations
struct fs_ops_t devfs_ops =
{
    // inode operations
    .read_inode = devfs_read_inode,
    .write_inode = devfs_write_inode,
    //.trunc_inode = NULL,
    .alloc_inode = NULL,
    .free_inode = NULL,
    .bmap = NULL,
    .read_symlink = NULL,
    .write_symlink = NULL,

    // directory operations
    .finddir = devfs_finddir,
    .finddir_by_inode = devfs_finddir_by_inode,
    //.readdir = devfs_readdir,
    .addir = NULL,
    .mkdir = NULL,
    .deldir = NULL,
    .dir_empty = NULL,
    .getdents = devfs_getdents,
    //.read = devfs_read,
    //.write = devfs_write,

    // device operations
    .mount = NULL,
    .umount = NULL,
    .read_super = devfs_read_super,
    .write_super = NULL,
    .put_super = devfs_put_super,
    .ustat = NULL,
    .statfs = NULL,
};


// devfs root -> /dev/
struct fs_node_t *devfs_root;

// dev tree
struct devnode_t *dev_list, *last_dev;

// lock to access above tree
struct kernel_mutex_t dev_lock;

// last inode # used on the dev filesystem
static int last_node_num = 2;


/*
 * Initialize devfs.
 */
void devfs_init(void)
{
    fs_register("devfs", &devfs_ops);
    init_kernel_mutex(&dev_lock);
    
    // this will allow us to mount devfs on /dev
    bdev_tab[240].ioctl = dummyfs_ioctl;

    bdev_tab[240].select = devfs_select;
    bdev_tab[240].poll = devfs_poll;
}


/*
 * Create the devfs virtual filesystem.
 * Should be called once, on system startup.
 *
 * Returns:
 *    root node of devfs
 */
struct fs_node_t *devfs_create(void)
{
    // make sure devfs is init'ed only once
    static int inited = 0;
    struct devnode_t *dev;
    
    if(inited)
    {
        printk("devfs: trying to re-init devfs\n");
        return devfs_root;
    }
    
    if(!(devfs_root = get_empty_node()))
    {
        kpanic("Failed to create devfs!\n");
    }

    devfs_root->ops = &devfs_ops;
    devfs_root->mode = S_IFDIR | 0755;  // 0555;
    devfs_root->links = 2;
    devfs_root->refs = 1;
    devfs_root->size = 2;
    devfs_root->atime = now();
    devfs_root->mtime = devfs_root->atime;
    devfs_root->ctime = devfs_root->atime;

    for(dev = dev_list; dev != NULL; dev = dev->next)
    {
        devfs_root->links++;
        devfs_root->size++;
    }

    // we use hard-coded value of 2 as init_fstab() calls devfs_init(), which 
    // in turn calls dev_init() to create device nodes before we're called by
    // rootfs_init() to create the devfs root node
    devfs_root->inode = 2;

    // use one of the reserved dev ids
    devfs_root->dev = DEV_DEVID;
    
    dev_init();
    inited = 1;
    
    return devfs_root;
}


/*
 * Read the filesystem's superblock and root inode.
 * This function fills in the mount info struct's block_size, super,
 * and root fields.
 */
int devfs_read_super(dev_t dev, struct mount_info_t *d,
                     size_t bytes_per_sector)
{
    UNUSED(bytes_per_sector);

    if(MINOR(dev) != 2)
    {
        return -EINVAL;
    }
    
    d->block_size = 0;
    d->super = NULL;
    d->root = devfs_root;

    return 0;
}


/*
 * Release the filesystem's superblock and its buffer.
 * Called when unmounting the filesystem.
 */
void devfs_put_super(dev_t dev, struct superblock_t *sb)
//void devfs_put_super(dev_t dev, struct IO_buffer_s *sb)
{
    UNUSED(dev);
    UNUSED(sb);
}


/*
 * Add a new device node.
 */
int add_dev_node(char *name, dev_t dev, mode_t mode)
{
    struct devnode_t *dnode = kmalloc(sizeof(struct devnode_t));
    
    if(!dnode)
    {
        return -ENOMEM;
    }
    
    memset(dnode, 0, sizeof(struct devnode_t));
    strcpy(dnode->name, name);
    dnode->dev = dev;
    dnode->inode = ++last_node_num;
    dnode->next = NULL;
    dnode->mode = mode;

    kernel_mutex_lock(&dev_lock);
    
    if(!last_dev)
    {
        dev_list = dnode;
        last_dev = dnode;
    }
    else
    {
        last_dev->next = dnode;
        last_dev = dnode;
    }
    
    if(devfs_root)
    {
        devfs_root->links++;
        devfs_root->size++;
    }

    kernel_mutex_unlock(&dev_lock);
    
    return 0;
}


/*
 * Set the given device's gid.
 */
int set_dev_gid(char *devname, gid_t gid)
{
    if(!devname)
    {
        return -EINVAL;
    }

    struct devnode_t *dnode = dev_list;
    
    while(dnode)
    {
        if(strcmp(dnode->name, devname) == 0)
        {
            dnode->gid = gid;
            return 0;
        }
        
        dnode = dnode->next;
    }
    
    return -ENOENT;
}


/*
 * Helper function that copies info from a devfs node to an incore
 * (memory resident) node.
 */
void devfs_inode_to_incore(struct fs_node_t *n, struct devnode_t *i)
{
    int j;
    time_t t = now();
    
    n->inode = i->inode;
    n->mode = i->mode;
    n->uid = i->uid;
    n->atime = t;
    n->mtime = get_startup_time();
    n->ctime = n->mtime;
    n->size = 0;
    //n->size = S_ISDIR(i->mode) ? 2 : 0;
    n->links = S_ISDIR(i->mode) ? 2 : 1;
    n->gid = i->gid;
    
    n->blocks[0] = i->dev;
    
    for(j = 1; j < 15; j++)
    {
        n->blocks[j] = 0;
    }
}


/*
 * Helper function that copies info from an incore (memory resident) 
 * node to a devfs node.
 */
void devfs_incore_to_inode(struct devnode_t *i, struct fs_node_t *n)
{
    i->inode = n->inode;
    i->mode = n->mode;

    i->uid = n->uid;
    i->gid = n->gid;
    
    /*
    i->atime = n->atime;
    i->mtime = n->mtime;
    i->ctime = n->ctime;
    i->size = n->size;
    i->links = n->links;
    */
    
    if(i->dev != n->blocks[0])
    {
        printk("devfs: writing inode with different "
               "devid (0x%x -> 0x%x)\n", i->dev, n->blocks[0]);
    }
    
    i->dev = n->blocks[0];
}


/*
 * Reads inode data structure.
 */
int devfs_read_inode(struct fs_node_t *node)
{
    if(!node)
    {
        return -EINVAL;
    }
    
    // root node
    if(node->inode == devfs_root->inode)
    {
        // preserve incore node's ref count
        int refs = node->refs;
        
        memcpy(node, devfs_root, sizeof(struct fs_node_t));
        node->refs = refs;

        return 0;
    }
    
    // other dev nodes
    struct devnode_t *dnode = dev_list;
    
    while(dnode)
    {
        if(dnode->inode == node->inode)
        {
            devfs_inode_to_incore(node, dnode);
            return 0;
        }

        dnode = dnode->next;
    }
    
    return -ENOENT;
}


/*
 * Writes inode data structure.
 */
int devfs_write_inode(struct fs_node_t *node)
{
    if(!node)
    {
        return -EINVAL;
    }
    
    // root node
    if(node->inode == devfs_root->inode)
    {
        return 0;
    }
    
    // other dev nodes
    struct devnode_t *dnode = dev_list;
    
    while(dnode)
    {
        if(dnode->inode == node->inode)
        {
            devfs_incore_to_inode(dnode, node);
            return 0;
        }

        dnode = dnode->next;
    }
    
    return -ENOENT;
}


static inline struct dirent *entry_to_dirent(int index,
                                             struct devnode_t *dnode)
{
    int namelen = strlen(dnode->name);
    unsigned short reclen = sizeof(struct dirent) + namelen + 1;
    
    struct dirent *entry = (struct dirent *)kmalloc(reclen);
    
    if(!entry)
    {
        return NULL;
    }
    
    entry->d_ino = dnode->inode;
    entry->d_off = index;
    entry->d_type = S_ISDIR(dnode->mode) ? DT_DIR :
                    (S_ISBLK(dnode->mode) ? DT_BLK : DT_CHR);
    strcpy(entry->d_name, dnode->name);
    entry->d_reclen = reclen;
    
    return entry;
}


static inline struct dirent *fs_node_to_dirent(int index, char *name,
                                               struct fs_node_t *dnode)
{
    struct devnode_t tmp;

    strcpy(tmp.name, name);
    tmp.dev = dnode->dev;
    tmp.inode = dnode->inode;
    tmp.mode = dnode->mode;
    tmp.next = NULL;
    
    return entry_to_dirent(index, &tmp);
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
 *             dirent struct (by calling entry_to_dirent() above), and the
 *             result is stored in this field
 *    dbuf => the disk buffer representing the disk block containing the found
 *            filename, this is useful if the caller wants to delete the file
 *            after finding it (vfs_unlink(), for example)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                file's entry
 *
 * Returns:
 *    0 on success, -errno on failure
 */
int devfs_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                  struct cached_page_t **dbuf, size_t *dbuf_off)
{
    if(!dir)
    {
        return -EINVAL;
    }

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;
    
    if(filename[0] == '.')
    {
        if(filename[1] =='\0' ||                            // '.'
           (filename[1] == '.' && filename[2] =='\0'))      // '..'
        {
            *entry = fs_node_to_dirent(0, filename, devfs_root);
            return *entry ? 0 : -ENOMEM;
        }
    }
    
    int i = 2;
    struct devnode_t *dnode = dev_list;
    
    while(dnode)
    {
        //printk("devfs_finddir: dnode->name '%s'\n", dnode->name);

        if(strcmp(dnode->name, filename) == 0)
        {
            *entry = entry_to_dirent(i, dnode);
            return *entry ? 0 : -ENOMEM;
        }
        
        i++;
        dnode = dnode->next;
    }
    
    return -ENOENT;
}


/*
 * Find the given inode in the parent directory.
 * Called during pathname resolution when constructing the absolute pathname
 * of a given inode.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    node => the searched-for inode
 *
 * Outputs:
 *    entry => if the node is found, its entry is converted to a kmalloc'd
 *             dirent struct (by calling entry_to_dirent() above), and the
 *             result is stored in this field
 *    dbuf => the disk buffer representing the disk block containing the found
 *            filename, this is useful if the caller wants to delete the file
 *            after finding it (vfs_unlink(), for example)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                file's entry
 *
 * Returns:
 *    0 on success, -errno on failure
 */
int devfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                           struct dirent **entry,
                           struct cached_page_t **dbuf, size_t *dbuf_off)
{
    if(!dir || !devfs_root || dir->inode != devfs_root->inode || !node)
    {
        return -EINVAL;
    }

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    // devfs root node
    if(node->inode == devfs_root->inode)
    {
        *entry = fs_node_to_dirent(0, ".", devfs_root);
        return *entry ? 0 : -ENOMEM;
    }
    
    // device nodes
    int i = 2;
    struct devnode_t *dnode = dev_list;
    
    while(dnode)
    {
        //printk("devfs_finddir_by_inode: dnode->name '%s'\n", dnode->name);

        if(dnode->inode == node->inode)
        {
            *entry = entry_to_dirent(i, dnode);
            return *entry ? 0 : -ENOMEM;
        }
        
        i++;
        dnode = dnode->next;
    }
    
    return -ENOENT;
}


/*
 * Get dir entries.
 *
 * Inputs:
 *     dir => node of dir to read from
 *     pos => byte position to start reading entries from
 *     dp => buffer in which to store dir entries
 *     count => max number of bytes to read (i.e. size of dp)
 *
 * Returns:
 *     number of bytes read on success, -errno on failure
 */
int devfs_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz)
{
    size_t i, offset, count = 0;
    size_t reclen, namelen;
    struct dirent *dent;
    char *b = (char *)buf;
    struct devnode_t tmp, *ent = NULL;

    /*
     * dir->size for /dev contains the number of devices on the system, plus
     * two entries for '.' and '..'.
     *
     * Offsets in the /dev directory refer to the following entries:
     *     Offset 0 => '.'
     *     Offset 1 => '..'
     *     Offset 2 => first dev entry, i.e. dev_list[0]
     *     Offset 2+n => dev_list[n]
     */

    offset = *pos;
    
    if(offset > 2)
    {
        for(i = 2, ent = dev_list; (i < offset) && (ent != NULL); i++)
        {
            ent = ent->next;
        }
        
        if(!ent)
        {
            return 0;
        }
    }

    while(offset < dir->size)
    {
        if(offset == 0)             // '.'
        {
            strcpy(tmp.name, ".");
            tmp.dev = devfs_root->dev;
            tmp.inode = devfs_root->inode;
            tmp.mode = devfs_root->mode;
            tmp.next = NULL;
            ent = &tmp;
        }
        else if(offset == 1)        // '..'
        {
            strcpy(tmp.name, "..");
            tmp.dev = system_root_node->dev;
            tmp.inode = system_root_node->inode;
            tmp.mode = system_root_node->mode;
            tmp.next = NULL;
            ent = &tmp;
        }
        else if(!ent)
        {
            ent = dev_list;
        }

        if(!ent)
        {
            //KDEBUG("devfs_getdents: count 0x%x (0)\n", count);
            //return count;
            break;
        }
        
        // get filename length
        namelen = strlen(ent->name);

        // calc dirent record length
        reclen = sizeof(struct dirent) + namelen + 1;

        // make it 4-byte aligned
        ALIGN_WORD(reclen);
        
        // check the buffer has enough space for this entry
        if((count + reclen) > (size_t)bufsz)
        {
            //KDEBUG("devfs_getdents: count 0x%x (1)\n", count);
            //return count;
            break;
        }
        
        dent = (struct dirent *)b;

        //printk("devfs_getdents: ent->name '%s'\n", ent->name);

        dent->d_ino = ent->inode;
        dent->d_off = offset;
        dent->d_type = S_ISDIR(ent->mode) ? DT_DIR :
                        (S_ISBLK(ent->mode) ? DT_BLK : DT_CHR);
        strcpy(dent->d_name, ent->name);

        dent->d_reclen = reclen;
        b += reclen;
        count += reclen;
        offset++;
        
        ent = ent->next;
    }
    
    *pos = offset;
    //KDEBUG("devfs_getdents: count 0x%x (2)\n", count);
    return count;
}


/*
 * Find the dirent corresponding to the given device. If blk is non-zero, we
 * only look for block devices, otherwise we look for character devices.
 * The resultant dirent is stored in *entry.
 */
int devfs_find_deventry(dev_t dev, int blk, struct dirent **entry)
{
    int i = 2;
    struct devnode_t *dnode = dev_list;
    
    if(!dev)
    {
        return -EINVAL;
    }

    // for safety
    *entry = NULL;
    
    while(dnode)
    {
        if(dnode->dev == dev)
        {
            if((blk && S_ISBLK(dnode->mode)) ||
               (!blk && !S_ISBLK(dnode->mode)))
            {
                *entry = entry_to_dirent(i, dnode);
                return *entry ? 0 : -ENOMEM;
            }
        }
        
        i++;
        dnode = dnode->next;
    }
    
    return -ENOENT;
}

