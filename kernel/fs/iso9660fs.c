/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
 * 
 *    file: iso9660fs.c
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
 *  \file iso9660fs.c
 *
 *  This file implements ISO9660 filesystem functions, which provides access to
 *  CD-ROMs and media formatted using the ISO9660 filesystem.
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the iso9660fs_ops structure.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/clock.h>
#include <kernel/dev.h>
#include <fs/iso9660fs.h>
#include <fs/ext2.h>
#include <fs/procfs.h>      // ALIGN_WORD
#include <fs/options.h>
#include <fs/magic.h>
#include <mm/kheap.h>

extern time_t timegm(struct tm *tm);

static long copy_symlink(struct iso9660_dirent_t *dent, 
                         char *buf, size_t bufsz, int size_only);

/*
 * See: https://wiki.osdev.org/ISO_9660
 */

#define IS_ISO9660_DIR(flags)   ((flags) & (1 << 1))

#if BYTE_ORDER == LITTLE_ENDIAN
# define GET_DWORD(d)           ((d).little)
# define GET_WORD(w)            ((w).little)
#else
# define GET_DWORD(d)           ((d).big)
# define GET_WORD(w)            ((w).big)
#endif      /* BYTE_ORDER */

#define SYMLINK_COMPONENT_FLAG_CONTINUE     (1 << 0)
#define SYMLINK_COMPONENT_FLAG_CURRENT      (1 << 1)
#define SYMLINK_COMPONENT_FLAG_PARENT       (1 << 2)
#define SYMLINK_COMPONENT_FLAG_ROOT         (1 << 3)

#define ALTNAME_FLAG_CONTINUE               (1 << 0)
#define ALTNAME_FLAG_CURRENT                (1 << 1)
#define ALTNAME_FLAG_PARENT                 (1 << 2)

#define MAY_FREE(need_free, n)  \
    if(need_free) { kfree(n); n = NULL; }


/*
 * As ISO9660 has no notion of inode numbers, we cheat by using LBA addresses
 * as inode numbers. To avoid having to walk down the directory tree every
 * time we want to access a file/dir, we cache the LBA of each entry we 
 * encounter, with the LBA of its parent, so that we can read the parent dir 
 * to find the file.
 */
struct lba_cacheent_t
{
    ino_t ino, parent_ino;
    size_t lba_parent, llba_parent;
    struct lba_cacheent_t *next;
};

/*
 * Each ISO9660 device has its own cache list. Store upto 8 devices, but we can
 * add more if needed.
 */
#define MAX_ISO9660_DEVICES         8

struct lba_cache_t
{
    dev_t dev;
    struct lba_cacheent_t lba_cache_head;
    volatile struct kernel_mutex_t lock;
} lba_cache[MAX_ISO9660_DEVICES];


// filesystem operations
struct fs_ops_t iso9660fs_ops =
{
    // inode operations
    .read_inode = iso9660fs_read_inode,
    .write_inode = NULL,    // iso9660fs_write_inode,
    //.trunc_inode = NULL,
    .alloc_inode = iso9660fs_alloc_inode,
    .free_inode = iso9660fs_free_inode,
    .bmap = iso9660fs_bmap,

    .read_symlink = iso9660fs_read_symlink,
    .write_symlink = iso9660fs_write_symlink,
    
    // directory operations
    .finddir = iso9660fs_finddir,
    .finddir_by_inode = iso9660fs_finddir_by_inode,
    //.readdir = procfs_readdir,
    .addir = iso9660fs_addir,
    .mkdir = iso9660fs_mkdir,
    .deldir = iso9660fs_deldir,
    .dir_empty = iso9660fs_dir_empty,
    .getdents = iso9660fs_getdents,
    
    //.read = procfs_read,
    //.write = procfs_write,
    
    // device operations
    .mount = NULL,
    .umount = NULL,
    .read_super = iso9660fs_read_super,
    .write_super = NULL,
    .put_super = iso9660fs_put_super,
    .ustat = iso9660fs_ustat,
    .statfs = iso9660fs_statfs,
};


static uint32_t iso9660_timedate_to_posix_time(uint8_t *date)
{
    struct tm ftm;
    ftm.tm_year = date[0];
    ftm.tm_mon  = date[1]-1;   /* 1-12 */
    ftm.tm_mday = date[2];
    ftm.tm_hour = date[3];
    ftm.tm_min  = date[4];
    ftm.tm_sec  = date[5];
    uint32_t res = (uint32_t)timegm(&ftm);
    return res;
}


static uint32_t iso9660_long_timedate_to_posix_time(uint8_t *date)
{
    struct tm ftm;

#define __DIGIT(b, i, s)    (b[i] >= '0' && b[i] <= '9') ?  \
                                    ((b[i] - '0') * s) : 0

    ftm.tm_year = __DIGIT(date, 0, 1000) + __DIGIT(date, 1, 100) +
                  __DIGIT(date, 2, 10) + __DIGIT(date, 3, 1);
    ftm.tm_mon  = __DIGIT(date, 4, 10) + __DIGIT(date, 5, 1) - 1;
    ftm.tm_mday = __DIGIT(date, 6, 10) + __DIGIT(date, 7, 1);
    ftm.tm_hour = __DIGIT(date, 8, 10) + __DIGIT(date, 9, 1);
    ftm.tm_min  = __DIGIT(date, 10, 10) + __DIGIT(date, 11, 1);
    ftm.tm_sec  = __DIGIT(date, 12, 10) + __DIGIT(date, 13, 1);

#undef __DIGIT

    uint32_t res = (uint32_t)timegm(&ftm);
    return res;
}


/*
 * Get a System Use Sharing Protocol (SUSP) field given its signature.
 */
static 
struct iso9660_susp_field_t *get_susp_field(struct iso9660_dirent_t *dent,
                                            uint8_t sig1, uint8_t sig2, uint8_t ver)
{
    size_t x = (sizeof(struct iso9660_dirent_t) + dent->namelen + 1) & ~1;
    char *s = (char *)dent + x;
    char *s2 = (char *)dent + dent->reclen;

    while(s < s2)
    {
        if(s[0] == sig1 && s[1] == sig2 && s[3] == ver)
        {
            return (struct iso9660_susp_field_t *)s;
        }

        s += s[2];
    }

    return NULL;
}


static void set_node_flags(struct fs_node_t *node, 
                           struct iso9660_dirent_t *dent, int is_root)
{
    struct iso9660_susp_field_t *field;

    // If the CD supports the RockRidge extension, use it to get POSIX file
    // attributes (PX) field. The root node is stored in the superblock and
    // does not have a System Use area.
    // See: https://people.freebsd.org/~emaste/rrip112.pdf
    if(!is_root && (field = get_susp_field(dent, 'P', 'X', 1)))
    {
        struct iso9660_rrip_px_t *px = (struct iso9660_rrip_px_t *)field;

        node->mode = GET_DWORD(px->mode);
        node->links = GET_DWORD(px->links);
        node->uid = GET_DWORD(px->uid);
        node->gid = GET_DWORD(px->gid);

        // If this is a char or block device, get its device number
        if(S_ISBLK(node->mode) || S_ISCHR(node->mode))
        {
            if((field = get_susp_field(dent, 'P', 'N', 1)))
            {
                struct iso9660_rrip_pn_t *pn = (struct iso9660_rrip_pn_t *)field;

                node->blocks[0] = GET_DWORD(pn->devlo);

                if(sizeof(size_t) > 4)
                {
                    node->blocks[0] |= (size_t)GET_DWORD(pn->devhi) << 32;
                }
            }
        }
        // If its a symbolic link, get its size, as the dirent does not
        // record symlink sizes in ISO9660
        else if(S_ISLNK(node->mode))
        {
            node->size = copy_symlink(dent, NULL, 0, 1);
        }
    }
    else
    {
        node->mode = 0;

        if(IS_ISO9660_DIR(dent->flags))
        {
            node->mode |= S_IFDIR;
            node->mode |= (S_IXUSR | S_IXGRP | S_IXOTH);

            // give directories a link count of 2 at least, to account for dot and
            // dot-dot entries
            node->links = 2;
        }
        else
        {
            node->mode |= S_IFREG;

            // give files a link count of 1, as we don't support hard links on CDs
            node->links = 1;
        }

        node->uid = 0;
        node->gid = 0;
        node->mode |= (S_IRUSR | S_IRGRP | S_IROTH);
    }

    node->ctime = iso9660_timedate_to_posix_time(dent->datetime);

    // If the CD supports the RockRidge extension, use it to get the Time
    // Fields (TF) field. The root node is stored in the superblock and
    // does not have a System Use area.
    // See: https://people.freebsd.org/~emaste/rrip112.pdf
    if(!is_root && (field = get_susp_field(dent, 'T', 'F', 1)))
    {
        struct iso9660_rrip_tf_t *tf = (struct iso9660_rrip_tf_t *)field;
        size_t sz = (tf->flags & (1 << 7)) ? 17 : 7;

#define NODE_GET_TIME(which, off)                                             \
    if(tf->flags & (1 << off)) {                                              \
        uint8_t *tmp = (uint8_t *)tf + 5 + (sz * off);                        \
        node->which = (sz == 17) ? iso9660_long_timedate_to_posix_time(tmp) : \
                                   iso9660_timedate_to_posix_time(tmp);       \
    } else node->which = node->ctime;

        NODE_GET_TIME(ctime, 0);
        NODE_GET_TIME(mtime, 1);
        NODE_GET_TIME(atime, 2);

#undef NODE_GET_TIME

    }
    else
    {
        node->mtime = node->ctime;
        node->atime = node->ctime;
    }
}


static struct lba_cacheent_t *get_cacheent(dev_t dev, ino_t ino)
{
    struct lba_cache_t *c;
    struct lba_cacheent_t *cent;
    
    for(c = lba_cache; c < &lba_cache[MAX_ISO9660_DEVICES]; c++)
    {
        kernel_mutex_lock(&c->lock);
        
        if(c->dev != dev)
        {
            kernel_mutex_unlock(&c->lock);
            continue;
        }
        
        for(cent = c->lba_cache_head.next; cent != NULL; cent = cent->next)
        {
            if(cent->ino != ino)
            {
                continue;
            }

            kernel_mutex_unlock(&c->lock);
            return cent;
        }
        
        kernel_mutex_unlock(&c->lock);
        break;
    }
    
    return NULL;
}


static struct lba_cacheent_t *alloc_cacheent(ino_t ino, ino_t parent_ino,
                                             size_t lba_parent, size_t llba_parent)
{
    struct lba_cacheent_t *cent;

    if(!(cent = kmalloc(sizeof(struct lba_cacheent_t))))
    {
        return NULL;
    }
    
    A_memset(cent, 0, sizeof(struct lba_cacheent_t));
    cent->ino = ino;
    cent->parent_ino = parent_ino;
    cent->lba_parent = lba_parent;
    cent->llba_parent = llba_parent;

    return cent;
}


static int add_cacheent(struct fs_node_t *dir, ino_t ino, uint32_t block_size)
{
    struct lba_cache_t *c;
    struct lba_cacheent_t *cent;
    size_t lba_parent, llba_parent;
    dev_t dev = dir->dev;
    size_t blocks = dir->size / block_size;

    if(dir->size % block_size)
    {
        blocks++;
    }

    lba_parent = dir->blocks[1];
    llba_parent = lba_parent + blocks;

    // try to find a cache queue with the same dev id
    for(c = lba_cache; c < &lba_cache[MAX_ISO9660_DEVICES]; c++)
    {
        kernel_mutex_lock(&c->lock);
        
        if(c->dev == dev)
        {
            // find out if this lba is already cached
            for(cent = c->lba_cache_head.next; cent != NULL; cent = cent->next)
            {
                if(cent->ino == ino)
                {
                    // it is, don't do anything
                    kernel_mutex_unlock(&c->lock);
                    return 0;
                }
            }
            
            // it isn't, add a new entry
            if(!(cent = alloc_cacheent(ino, dir->inode, lba_parent, llba_parent)))
            {
                kernel_mutex_unlock(&c->lock);
                return -ENOMEM;
            }
            
            cent->next = c->lba_cache_head.next;
            c->lba_cache_head.next = cent;
            kernel_mutex_unlock(&c->lock);
            return 0;
        }

        kernel_mutex_unlock(&c->lock);
    }
    
    // create a new cache queue for this dev id
    for(c = lba_cache; c < &lba_cache[MAX_ISO9660_DEVICES]; c++)
    {
        kernel_mutex_lock(&c->lock);
        
        if(c->dev != 0)
        {
            kernel_mutex_unlock(&c->lock);
            continue;
        }

        if(!(cent = alloc_cacheent(ino, dir->inode, lba_parent, llba_parent)))
        {
            kernel_mutex_unlock(&c->lock);
            return -ENOMEM;
        }
        
        c->dev = dev;
        c->lba_cache_head.next = cent;
        kernel_mutex_unlock(&c->lock);
        return 0;
    }
    
    return -ENOMEM;
}


/*
 * ISO9660 filenames take the format: 'FILENAME;ID'.
 *
 * This function copies the name of the file/dir from 'src' to 'dest',
 * converting uppercase letters to lowercase and ignoring the file ID number,
 * including the semicolon.
 */
static void iso9660_strncpy(char *dest, char *src, size_t len, int isdir)
{
    KDEBUG("iso9600_strncpy: src '%s'\n", src);
    
    char *lsrc = src + len;
    char *odest = dest;
    
    while(src < lsrc)
    {
        if(!*src || *src == ';')
        {
            break;
        }
        
        if(*src >= 'A' && *src <= 'Z')
        {
            *dest = (*src - 'A') + 'a';
        }
        else
        {
            *dest = *src;
        }
        
        dest++;
        src++;
    }
    
    *dest = '\0';
    
    // check if the name is a filename and, if so, check the file extension
    // and remove the final dot if there is no extension
    if(!isdir && len > 1 && dest != odest && dest[-1] == '.')
    {
        dest[-1] = '\0';
    }
}


/*
 * Initialise and register the ISO9660 filesystem.
 */
void iso9660fs_init(void)
{
    A_memset(lba_cache, 0, sizeof(lba_cache));

    fs_register("iso9660", &iso9660fs_ops);
}


/*
 * Read the filesystem's superblock and root inode.
 * This function fills in the mount info struct's block_size, super,
 * and root fields.
 */
long iso9660fs_read_super(dev_t dev, struct mount_info_t *d,
                          size_t bytes_per_sector)
{
    struct superblock_t *super;
    struct disk_req_t req;
    physical_addr phys;
    int maj = MAJOR(dev);
    char *buf;

    if(maj >= NR_DEV || !bdev_tab[maj].strategy)
    {
        return -EIO;
    }

    if(!(super = kmalloc(sizeof(struct superblock_t))))
    {
        return -EAGAIN;
    }

    A_memset(super, 0, sizeof(struct superblock_t));

   	if(!(phys = (physical_addr)pmmngr_alloc_block()))
   	{
        kfree(super);
        return -EAGAIN;
    }

    super->data = PHYS_TO_HIMEM(phys);

    /* Volume Descriptors start at sector 0x10 */
    super->blockno = 0x10;
    super->blocksz = bytes_per_sector;
    super->dev = dev;
    
read:

    KDEBUG("iso9660fs_read_super: dev 0x%x, blk 0x%x, bps 0x%x\n", dev, super->blockno, bytes_per_sector);

    req.dev = dev;
    req.data = super->data;

    //req.blocksz = super->blocksz;
    req.datasz = super->blocksz;
    req.fs_blocksz = super->blocksz;

    req.blockno = super->blockno;
    req.write = 0;

#define BAIL_OUT(err)   \
        pmmngr_free_block((void *)phys);   \
        kfree(super);   \
        return err;

    if(bdev_tab[maj].strategy(&req) < 0)
    {
        KDEBUG("iso9660fs_read_super: failed\n");

        BAIL_OUT(-EIO);
    }
    
    buf = (char *)super->data;
    
    /* Check the identifier 'CD001' */
    if(buf[1] != 'C' || buf[2] != 'D' || buf[3] != '0' ||
       buf[4] != '0' || buf[5] != '1')
    {
        KDEBUG("iso9660fs_read_super: invalid signature (%x%x%x%x%x)\n",
                buf[1], buf[2], buf[3], buf[4], buf[5]);

        BAIL_OUT(-EINVAL);
    }
    
    /* Primary Volume Descriptor */
    if(*buf == 1)
    {
        KDEBUG("iso9660fs_read_super: got pvd\n");

        d->block_size = buf[128] | (buf[129] << 8);
        d->super = super;
        d->mountflags |= MS_RDONLY;
        d->flags |= FS_SUPER_RDONLY;
        
        // the root node is stored in the Primary Volume Descriptor (PVD)
        d->root = get_node(dev, 2, 0);

        KDEBUG("iso9660fs_read_super: d->block_size 0x%x\n", d->block_size);
        KDEBUG("iso9660fs_read_super: got root node - lba 0x%x\n", dent->lba);

        //atapi_lock_media(dev, NULL, NULL);

        return 0;
    }
    
    /* Any more Volume Descriptors? */
    /* 255 is for Volume Descriptor Set Terminator */
    if(*(unsigned char *)buf != 255)
    {
        KDEBUG("iso9660fs_read_super: *buf 0x%x\n", *(unsigned char *)buf);

        super->blockno++;
        goto read;
    }

    KDEBUG("iso9660fs_read_super: done\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    BAIL_OUT(-EINVAL);

#undef BAIL_OUT

}


/*
 * Release the filesystem's superblock and its buffer.
 * Called when unmounting the filesystem.
 */
void iso9660fs_put_super(dev_t dev, struct superblock_t *super)
{
    struct lba_cache_t *c;
    struct lba_cacheent_t *cent, *next;
    physical_addr phys;

    for(c = lba_cache; c < &lba_cache[MAX_ISO9660_DEVICES]; c++)
    {
        kernel_mutex_lock(&c->lock);
        
        if(c->dev != dev)
        {
            kernel_mutex_unlock(&c->lock);
            continue;
        }
        
        for(cent = c->lba_cache_head.next; cent != NULL; )
        {
            next = cent->next;
            kfree(cent);
            cent = next;
        }
        
        c->dev = 0;
        c->lba_cache_head.next = NULL;
        
        kernel_mutex_unlock(&c->lock);

        if((phys = get_phys_addr(super->data)))
        {
            pmmngr_free_block((void *)phys);
        }

        kfree(super);

        break;
    }
}


static ino_t get_inode_number(struct iso9660_dirent_t *dent,
                              size_t parent_block, size_t parent_off)
{
    struct iso9660_susp_field_t *field;

    // If the CD supports the RockRidge extension, use it to get POSIX file
    // attributes (PX) field, which should contain an inode number.
    // See: https://people.freebsd.org/~emaste/rrip112.pdf
    if((field = get_susp_field(dent, 'P', 'X', 1)) && field->len >= 44)
    {
        struct iso9660_rrip_px_t *px = (struct iso9660_rrip_px_t *)field;
        ino_t ino = GET_DWORD(px->ino);

        KDEBUG("get_inode_number: ino 0x%lx (lba 0x%lx)\n", ino, GET_DWORD(dent->lba));

        if(ino != 0)
        {
            return ino;
        }
    }

    //KDEBUG("get_inode_number: fallback ino 0x%lx (b 0x%lx, o 0x%lx)\n", (parent_block << 16) | (parent_off & 0xffff), parent_block, parent_off);

    // Fall back to the made up inode number, which we create from the block
    // containing the dirent in the parent, and the dirent's offset in block
    return (parent_block << 16) | (parent_off & 0xffff);
}


/*
 * Reads inode data structure from disk.
 */
long iso9660fs_read_inode(struct fs_node_t *node)
{
    struct mount_info_t *d;
    char *buf, *lbuf;
    ino_t root;
    size_t lba_parent;
    size_t block_size;
    struct iso9660_dirent_t *dent;
    struct lba_cacheent_t *cent;
    struct cached_page_t *blk;

    if((d = get_mount_info(node->dev)) == NULL || !(d->super))
    {
        return -EINVAL;
    }
    
    A_memset(node->blocks, 0, sizeof(node->blocks));

    // the root node is stored in the Primary Volume Descriptor (PVD)
    buf = (char *)d->super->data + 156;
    dent = (struct iso9660_dirent_t *)buf;
    root = GET_DWORD(dent->lba);
    
    if(node->inode == 2 /* root */)
    {
        // Store the LBA in blocks[1] as blocks[0] may be used by the VFS for
        // block and char device numbers 
        node->blocks[1] = root;
        node->size = GET_DWORD(dent->size);
        set_node_flags(node, dent, 1);
        return 0;
    }

    // other nodes (not root)
    if((cent = get_cacheent(node->dev, node->inode)) != NULL)
    {
        struct fs_node_header_t tmpnode;

        tmpnode.inode = PCACHE_NOINODE;
        tmpnode.dev = node->dev;

        lba_parent = cent->lba_parent;
        block_size = d->block_size;
        
        while(lba_parent < cent->llba_parent)
        {
            if(!(blk = get_cached_page((struct fs_node_t *)&tmpnode, lba_parent, 0)))
            {
                return -EIO;
            }
            
            buf = (char *)blk->virt;
            lbuf = buf + block_size;
            
            while(buf < lbuf)
            {
                dent = (struct iso9660_dirent_t *)buf;

                // end of sector might be zero-padded if:
                //   - we reached the end of directory
                //   - the next entry cannot fit in the remaining space in the
                //     sector
                //
                // in both cases, we skip to the next sector. if it past the 
                // directory size, our work is done (case 1 above), otherwise 
                // we continue reading the next sector to get the next entry
                // (case 2 above).
                if(dent->reclen == 0)
                {
                    buf = lbuf;
                    continue;
                }

                if(node->inode == get_inode_number(dent, lba_parent, (uintptr_t)buf - blk->virt))
                //if(node->inode == lba)
                {
                    // Store the LBA in blocks[1] as blocks[0] may be used by 
                    // the VFS for block and char device numbers 
                    node->blocks[1] = GET_DWORD(dent->lba);

                    node->size = GET_DWORD(dent->size);
                    set_node_flags(node, dent, 0);
                    release_cached_page(blk);
                    return 0;
                }
                
                buf += dent->reclen;
            }
            
            release_cached_page(blk);
            lba_parent++;
        }
    }

    return -ENOENT;
}


/*
 * Map file position to disk block number using inode struct's block pointers.
 *
 * Inputs:
 *    node => node struct
 *    lblock => block number we want to map
 *    block_size => filesystem's block size in bytes
 *    flags => BMAP_FLAG_CREATE, BMAP_FLAG_FREE or BMAP_FLAG_NONE which creates
 *             the block if it doesn't exist, frees the block (when shrinking
 *             files), or simply maps, respectively
 *
 * Returns:
 *    disk block number on success, 0 on failure
 */
size_t iso9660fs_bmap(struct fs_node_t *node, size_t lblock,
                      size_t block_size, int flags)
{
    UNUSED(flags);

    // Node size for symlinks is made up by us, ISO9660 does not actually
    // store a node size here
    if(S_ISLNK(node->mode))
    {
        return 0;
    }

    size_t blocks = node->size / block_size;
    
    if(node->size % block_size)
    {
        blocks++;
    }
    
    if(lblock >= blocks)
    {
        return 0;
    }

    return node->blocks[1] + lblock;
}


/*
 * Free an inode and update inode bitmap on disk.
 */
long iso9660fs_free_inode(struct fs_node_t *node)
{
    UNUSED(node);
    
    return -EROFS;
}


/*
 * Allocate a new inode number and mark it as used in the disk's inode bitmap.
 */
long iso9660fs_alloc_inode(struct fs_node_t *node)
{
    UNUSED(node);
    
    return -EROFS;
}


/*
 * Free a disk block and update the disk's block bitmap.
 */
void iso9660fs_free(dev_t dev, uint32_t block_no)
{
    UNUSED(dev);
    UNUSED(block_no);
}


/*
 * Allocate a new block number and mark it as used in the disk's block bitmap.
 *
 * This function also updates the mount info struct's free block pool if all
 * the cached block numbers have been used by searching the disk for free
 * block numbers.
 *
 * Input:
 *    dev => device id
 *
 * Returns:
 *    new alloc'd block number on success, 0 on failure
 */
uint32_t iso9660fs_alloc(dev_t dev)
{
    UNUSED(dev);
    
    return -EROFS;
}


STATIC_INLINE
struct dirent *iso9660_entry_to_dirent(struct dirent *__ent, ino_t inode,
                                       char *name, uint8_t namelen,
                                       int off, uint8_t flags)
{
    unsigned short reclen = GET_DIRENT_LEN(namelen);
    unsigned char d_type = DT_UNKNOWN;
    struct dirent *entry = __ent ? __ent : kmalloc(reclen);

    if(!entry)
    {
        return NULL;
    }

    if(IS_ISO9660_DIR(flags))
    {
        d_type = DT_DIR;
    }
    else
    {
        d_type = DT_REG;
    }
    
    entry->d_reclen = reclen;
    entry->d_ino = inode;
    entry->d_off = off;
    entry->d_type = d_type;

    memcpy(entry->d_name, name, namelen + 1);
    
    return entry;
}


static ino_t inode_for_dirent(struct fs_node_t *dir, 
                              struct iso9660_dirent_t *dent, 
                              char *n, size_t block, size_t offset)
{
    ino_t ino;

    if(n[0] == '.' && n[1] == '\0')
    {
        ino = dir->inode;
    }
    else if(n[0] == '.' && n[1] == '.' && n[2] == '\0')
    {
        struct lba_cacheent_t *cent;

        if((cent = get_cacheent(dir->dev, dir->inode)) != NULL)
        {
            ino = cent->parent_ino;
        }
        else
        {
            // XXX: this reparents everything that's broken to root
            ino = 2;
        }
    }
    else
    {
        //ino = GET_DWORD(dent->lba);
        ino = get_inode_number(dent, block, offset);
    }

    return ino;
}


static int copy_altname(struct iso9660_dirent_t *dent, char *namebuf, size_t *namelen)
{
    struct iso9660_susp_field_t *field;
    struct iso9660_rrip_nm_t *nm;
    char *end = (char *)dent + dent->reclen;
    char *p = namebuf;
    uint8_t nm_len;

    // If the CD supports the RockRidge extension, use it to
    // get the Alternate Name (NM) field.
    // See: https://people.freebsd.org/~emaste/rrip112.pdf

    if(!(field = get_susp_field(dent, 'N', 'M', 1)))
    {
        return -EINVAL;
    }

read:

    nm = (struct iso9660_rrip_nm_t *)field;
    nm_len = nm->hdr.len;

    if(nm_len < 5)
    {
        return -EINVAL;
    }

    if(nm->flags & ALTNAME_FLAG_CURRENT)
    {
        *p++ = '.';
        *p = '\0';
    }
    else if(nm->flags & ALTNAME_FLAG_PARENT)
    {
        *p++ = '.';
        *p++ = '.';
        *p = '\0';
    }
    else
    {
        memcpy(p, (char *)nm + 5, nm->hdr.len - 5);
        p += (nm->hdr.len - 5);
        *p = '\0';
    }

    // If this NM entry has its CONTINUE flag set, search for the next
    // NM entry and parse it
    if(nm->flags & ALTNAME_FLAG_CONTINUE)
    {
        char *tmp = (char *)nm + nm_len;

        while(tmp < end)
        {
            if(tmp[0] == 'N' && tmp[1] == 'M')
            {
                field = (struct iso9660_susp_field_t *)tmp;
                goto read;
            }

            // Skip to the next entry
            tmp += tmp[2];
        }
    }

    //switch_tty(1);
    //printk("*** namebuf '%s'\n", namebuf);

    *namelen = p - namebuf;
    return 0;
}


static size_t get_name(struct iso9660_dirent_t *dent, char *namebuf, uint8_t flags)
{
    char *n = (char *)dent + sizeof(struct iso9660_dirent_t);
    size_t len = 0;

    // An empty string signifies '.' and a '\1' string signifies '..'
    if(n[0] == '\0')
    {
        namebuf[0] = '.';
        namebuf[1] = '\0';
        len = 1;
    }
    else if(n[0] == '\1')
    {
        namebuf[0] = '.';
        namebuf[1] = '.';
        namebuf[2] = '\0';
        len = 2;
    }
    else
    {
        if(copy_altname(dent, namebuf, &len) < 0)
        {
            // No Alternate Name (NM) entry or no memory. Use the 8.3 name
            iso9660_strncpy(namebuf, n, dent->namelen, IS_ISO9660_DIR(flags));
            len = dent->namelen;
        }
    }

    return len;
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
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long iso9660fs_finddir(struct fs_node_t *dir, char *filename,
                       struct dirent **entry)
{
    size_t offset, namelen, last_block;
    size_t fnamelen, blocksz;
    ino_t lba;
    unsigned char *blk, *end;
    char *namebuf;
    struct cached_page_t *buf;
    struct mount_info_t *d;
    struct iso9660_dirent_t *dent;
    struct fs_node_header_t tmpnode;

    tmpnode.inode = PCACHE_NOINODE;
    tmpnode.dev = dir->dev;

    // for safety
    *entry = NULL;

    if(!dir || !filename)
    {
        return -EINVAL;
    }

    if(!(fnamelen = strlen(filename)))
    {
        return -EINVAL;
    }

    if(fnamelen > NAME_MAX)
    {
        return -ENAMETOOLONG;
    }

    if((d = get_mount_info(dir->dev)) == NULL || !(d->super))
    {
        return -EINVAL;
    }

    if(!(namebuf = kmalloc(1024)))
    {
        return -ENOMEM;
    }

    offset = dir->blocks[1];
    blocksz = d->block_size;
    last_block = offset + ((dir->size + (blocksz - 1)) / blocksz);

    while(offset < last_block)
    {
        if(!(buf = get_cached_page((struct fs_node_t *)&tmpnode, offset, 0)))
        {
            offset++;
            continue;
        }

        blk = (unsigned char *)buf->virt;
        end = blk + blocksz;

        while(blk < end)
        {
            dent = (struct iso9660_dirent_t *)blk;

            // end of sector might be zero-padded if:
            //   - we reached the end of directory
            //   - the next entry cannot fit in the remaining space in the sector
            // in both cases, we skip to the next sector. if it past the directory
            // size, our work is done (case 1 above), otherwise we continue reading
            // the next sector to get the next entry (case 2 above).
            if(dent->reclen == 0)
            {
                blk = end;
                continue;
            }

            namelen = get_name(dent, namebuf, dent->flags);

            /*
            if(memcmp(filename, "calcapp", 7) == 0)
            {
                switch_tty(1);
                printk("*** filename '%s', namebuf '%s', namelen %ld\n", filename, namebuf, namelen);
            }
            */

            if(fnamelen == namelen && memcmp(namebuf, filename, namelen) == 0)
            {
                //printk("*** filename '%s', namebuf '%s', namelen %ld -- match\n", filename, namebuf, namelen);
                lba = inode_for_dirent(dir, dent, namebuf, offset,
                                       (uintptr_t)blk - buf->virt);

                add_cacheent(dir, lba, blocksz);

                *entry = iso9660_entry_to_dirent(NULL, lba, namebuf, namelen,
                                  (offset * blocksz) + (blk - (unsigned char *)buf->virt),
                                  dent->flags);

                kfree(namebuf);
                release_cached_page(buf);
                return 0;
            }

            blk += dent->reclen;
        }
        
        release_cached_page(buf);
        offset++;
    }

    kfree(namebuf);
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
 *             dirent struct, and the result is stored in this field
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long iso9660fs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                                struct dirent **entry)
{
    size_t offset = 0, blocksz, namelen, last_block;
    ino_t lba;
    unsigned char *blk, *end;
    char *namebuf;
    struct cached_page_t *buf;
    struct mount_info_t *d;
    struct iso9660_dirent_t *dent;
    struct fs_node_header_t tmpnode;

    tmpnode.inode = PCACHE_NOINODE;
    tmpnode.dev = dir->dev;

    // for safety
    *entry = NULL;

    if(!dir || !node)
    {
        return -EINVAL;
    }

    if((d = get_mount_info(dir->dev)) == NULL || !(d->super))
    {
        return -EINVAL;
    }

    if(!(namebuf = kmalloc(1024)))
    {
        return -ENOMEM;
    }

    offset = dir->blocks[1];
    blocksz = d->block_size;
    last_block = offset + ((dir->size + (blocksz - 1)) / blocksz);

    while(offset < last_block)
    {
        if(!(buf = get_cached_page((struct fs_node_t *)&tmpnode, offset, 0)))
        {
            offset++;
            continue;
        }
        
        blk = (unsigned char *)buf->virt;
        end = blk + blocksz;

        while(blk < end)
        {
            dent = (struct iso9660_dirent_t *)blk;

            // end of sector might be zero-padded if:
            //   - we reached the end of directory
            //   - the next entry cannot fit in the remaining space in the
            //     sector
            //
            // in both cases, we skip to the next sector. if it past the 
            // directory size, our work is done (case 1 above), otherwise 
            // we continue reading the next sector to get the next entry
            // (case 2 above).
            if(dent->reclen == 0)
            {
                blk = end;
                continue;
            }

            namelen = get_name(dent, namebuf, dent->flags);
            lba = inode_for_dirent(dir, dent, namebuf, offset,
                                   (uintptr_t)blk - buf->virt);

            if(matching_node(dir->dev, lba, node))
            {
                add_cacheent(dir, lba, blocksz);

                *entry = iso9660_entry_to_dirent(NULL, lba, namebuf, namelen,
                                 (offset * blocksz) + (blk - (unsigned char *)buf->virt),
                                 dent->flags);

                kfree(namebuf);
                release_cached_page(buf);
                return 0;
            }

            blk += dent->reclen;
        }
        
        release_cached_page(buf);
        offset++;
    }

    kfree(namebuf);
    return -ENOENT;
}


/*
 * Add the given file as an entry in the given parent directory.
 */
long iso9660fs_addir(struct fs_node_t *dir, struct fs_node_t *file, char *filename)
{
    UNUSED(dir);
    UNUSED(filename);
    UNUSED(file);

    return -EROFS;
}


/*
 * Make a new, empty directory by allocating a free block and initializing
 * the '.' and '..' entries to point to the current and parent directory
 * inodes, respectively.
 */
long iso9660fs_mkdir(struct fs_node_t *dir, struct fs_node_t *parent)
{
    UNUSED(dir);
    UNUSED(parent);
    
    return -EROFS;
}


/*
 * Remove an entry from the given parent directory.
 */
long iso9660fs_deldir(struct fs_node_t *dir, struct dirent *entry, int is_dir)
{
    UNUSED(dir);
    UNUSED(entry);
    UNUSED(is_dir);

    return -EROFS;
}


/*
 * Check if the given directory is empty (called from rmdir).
 */
long iso9660fs_dir_empty(struct fs_node_t *dir)
{
    size_t offset, blocksz, last_block;
    uint32_t lba;
    unsigned char *blk, *end;
    char *n;
    struct cached_page_t *buf;
    struct mount_info_t *d;
    struct iso9660_dirent_t *ent;
    struct fs_node_header_t tmpnode;

    tmpnode.inode = PCACHE_NOINODE;
    tmpnode.dev = dir->dev;

    if((d = get_mount_info(dir->dev)) == NULL || !(d->super))
    {
        return 0;
    }
    
    if(!dir->size || !dir->blocks[1])
    {
        printk("iso9660: bad directory inode at 0x%x:0x%x\n",
               dir->dev, dir->inode);
        return 1;
    }

    offset = dir->blocks[1];
    blocksz = d->block_size;
    last_block = offset + ((dir->size + (blocksz - 1)) / blocksz);

    while(offset < last_block)
    {
        if(!(buf = get_cached_page((struct fs_node_t *)&tmpnode, offset, 0)))
        {
            return 1;
        }

        blk = (unsigned char *)buf->virt;
        end = (unsigned char *)buf->virt + blocksz;

        while(blk < end)
        {
            n = (char *)(blk + sizeof(struct iso9660_dirent_t));
            ent = (struct iso9660_dirent_t *)blk;

            // do not check '.' and '..' special entries
            if(n[0] != '\0' && n[0] != '\1')
            {
                lba = GET_DWORD(ent->lba);

                if(lba)
                {
                    release_cached_page(buf);
                    return 0;
                }
            }

            // end of sector might be zero-padded if:
            //   - we reached the end of directory
            //   - the next entry cannot fit in the remaining space in the 
            //     sector
            //
            // in both cases, we skip to the next sector. if it past the 
            // directory size, our work is done (case 1 above), otherwise 
            // we continue reading the next sector to get the next entry
            // (case 2 above).
            if(ent->reclen == 0)
            {
                blk = end;
                continue;
            }

            blk += ent->reclen;
        }

        release_cached_page(buf);
        offset++;
    }
    
    return 1;
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
long iso9660fs_getdents(struct fs_node_t *dir, off_t *pos,
                        void *buf, int bufsz)
{
    volatile int done = 0;
    size_t i, count = 0, bytes = 0;
    size_t reclen, blocksz, last_block;
    struct cached_page_t *dbuf = NULL;
    struct dirent *dent = NULL;
    char *b = (char *)buf;
    unsigned char *blk, *end;
    char *namebuf;
    size_t offset, namelen;
    ino_t lba;
    struct mount_info_t *d;
    struct iso9660_dirent_t *ent;
    struct fs_node_header_t tmpnode;

    tmpnode.inode = PCACHE_NOINODE;
    tmpnode.dev = dir->dev;

    if(!dir || !pos || !buf || !bufsz)
    {
        return -EINVAL;
    }

    if((d = get_mount_info(dir->dev)) == NULL || !(d->super))
    {
        return 0;
    }

    if(!(namebuf = kmalloc(1024)))
    {
        return -ENOMEM;
    }

    blocksz = d->block_size;
    offset = dir->blocks[1] + ((*pos) / blocksz);
    i = (*pos) % blocksz;
    last_block = dir->blocks[1] + ((dir->size + (blocksz - 1)) / blocksz);

    while(offset < last_block)
    {
        if(!(dbuf = get_cached_page((struct fs_node_t *)&tmpnode, offset, 0)))
        {
            offset++;
            bytes += blocksz;
            continue;
        }
        
        blk = (unsigned char *)(dbuf->virt + i);
        end = (unsigned char *)(dbuf->virt + blocksz);

        // we use i only for the first round, as we might have been asked to
        // read from the middle of a block
        i = 0;

        while(blk < end)
        {
            ent = (struct iso9660_dirent_t *)blk;

            // end of sector might be zero-padded if:
            //   - we reached the end of directory
            //   - the next entry cannot fit in the remaining space in the 
            //     sector
            //
            // in both cases, we skip to the next sector. if it past the
            // directory size, our work is done (case 1 above), otherwise
            // we continue reading the next sector to get the next entry
            // (case 2 above).
            if(ent->reclen == 0)
            {
                bytes += blocksz - (blk - (unsigned char *)dbuf->virt);
                blk = end;
                continue;
            }

            namelen = get_name(ent, namebuf, ent->flags);

            // calc dirent record length
            reclen = GET_DIRENT_LEN(namelen);

            // check the buffer has enough space for this entry
            if((count + reclen) > (size_t)bufsz)
            {
                done = 1;
                break;
            }
            
            dent = (struct dirent *)b;

            lba = inode_for_dirent(dir, ent, namebuf, offset,
                                   (uintptr_t)blk - dbuf->virt);

            add_cacheent(dir, lba, blocksz);

            iso9660_entry_to_dirent(dent, lba, namebuf, namelen,
                                 (offset * blocksz) + (blk - (unsigned char *)dbuf->virt), 
                                 ent->flags);

            b += reclen;
            count += reclen;
            blk += ent->reclen;
            bytes += ent->reclen;
        }
        
        release_cached_page(dbuf);

        if(done)
        {
            break;
        }

        offset++;
    }

    *pos += bytes;
    kfree(namebuf);

    return count;
}


/*
 * Return filesystem statistics.
 */
long iso9660fs_ustat(struct mount_info_t *d, struct ustat *ubuf)
{
    if(!d)
    {
        return -EINVAL;
    }

    if(!ubuf)
    {
        return -EFAULT;
    }
    
    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_ustat() function).
     */
    ubuf->f_tfree = 0;
    ubuf->f_tinode = 0;

    return 0;
}


/*
 * Return detailed filesystem statistics.
 */
long iso9660fs_statfs(struct mount_info_t *d, struct statfs *statbuf)
{
    if(!d)
    {
        return -EINVAL;
    }

    //struct statfs tmp;
    struct iso9660_pvd_t *pvd = (struct iso9660_pvd_t *)d->super->data;

    if(!statbuf)
    {
        return -EFAULT;
    }

    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_statfs() function).
     */
    statbuf->f_type = 0  /* TODO: ISO9660 super magic ??? */;
    statbuf->f_bsize = d->block_size;
    statbuf->f_blocks = GET_DWORD(pvd->blocks);
    statbuf->f_bfree = 0;
    statbuf->f_bavail = 0;
    statbuf->f_files = 0 /* TODO: get the number of files on disk ??? */;
    statbuf->f_ffree = 0;
    //statbuf->f_fsid = 0;
    statbuf->f_namelen = 12;    /* assume 8.3 format */
    statbuf->f_frsize = 0;
    statbuf->f_flags = d->mountflags;

    return 0;
}


static long copy_symlink(struct iso9660_dirent_t *dent, 
                         char *buf, size_t bufsz, int size_only)
{
    struct iso9660_susp_field_t *field;
    struct iso9660_rrip_sl_t *sl;
    char *comp, *lcomp;
    char *end = (char *)dent + dent->reclen;
    char *p = buf, *lp = buf + bufsz;
    uint8_t sl_len, cont = 1;

    // If the CD supports the RockRidge extension, use it to
    // get the Symbolic Link (SL) field.
    // See: https://people.freebsd.org/~emaste/rrip112.pdf

    if(!(field = get_susp_field(dent, 'S', 'L', 1)))
    {
        return -EINVAL;
    }

read:

    sl = (struct iso9660_rrip_sl_t *)field;
    sl_len = sl->hdr.len;
    comp = (char *)sl + 5;
    lcomp = (char *)sl + sl_len;

    KDEBUG("*** sl_len = %d, sl->flags 0x%x\n", sl_len, sl->flags);
    KDEBUG("*** bufsz = %ld\n", bufsz);

    if(sl_len < 5)
    {
        return -EINVAL;
    }

#define APPEND_TO_BUF(s, l)             \
    if(size_only) p += l;               \
    else {                              \
        if(p + l > lp) return p - buf;  \
        memcpy(p, s, l);                \
        p += l;                         \
    }

    while(comp < lcomp)
    {
        KDEBUG("*** cont %d, comp[0] 0x%x\n", cont, comp[0]);

        if(!cont)
        {
            APPEND_TO_BUF("/", 1);
        }

        if(comp[0] & SYMLINK_COMPONENT_FLAG_CURRENT)
        {
            APPEND_TO_BUF(".", 1);
        }
        else if(comp[0] & SYMLINK_COMPONENT_FLAG_PARENT)
        {
            APPEND_TO_BUF("..", 2);
        }
        else if(comp[0] & SYMLINK_COMPONENT_FLAG_ROOT)
        {
            APPEND_TO_BUF("/", 1);
        }
        else
        {
            APPEND_TO_BUF(comp + 2, comp[1]);
        }

        cont = (comp[0] & SYMLINK_COMPONENT_FLAG_CONTINUE);
        comp += comp[1] + 2;
    }

#undef APPEND_TO_BUF

    // If this SL entry has its CONTINUE flag set, search for the next
    // SL entry and parse it
    if(sl->flags & (1 << 0))
    {
        comp = lcomp;

        while(comp < end)
        {
            if(comp[0] == 'S' && comp[1] == 'L')
            {
                field = (struct iso9660_susp_field_t *)comp;
                goto read;
            }

            // Skip to the next entry
            comp += comp[2];
        }
    }

    return p - buf;
}


/*
 * Read the contents of a symbolic link. As different filesystems might have
 * different ways of storing symlinks (e.g. ext2 stores links < 60 chars in
 * length in the inode struct itself), we hand over this task to the
 * filesystem.
 *
 * Inputs:
 *    link => the symlink's inode
 *    buf => the buffer in which we will read and store the symlink's target
 *    bufsz => size of buffer above 
 *    kernel => set if the caller is a kernel function (i.e. 'buf' address
 *              is in kernel memory), 0 if 'buf' is a userspace address
 *
 * Returns:
 *    number of chars read on success, -errno on failure
 */
long iso9660fs_read_symlink(struct fs_node_t *link,
                            char *target, size_t targetsz, int kernel)
{
    struct mount_info_t *d;
    char *buf, *lbuf;
    ino_t root;
    uint32_t lba, lba_parent;
    size_t block_size;
    struct iso9660_dirent_t *dent;
    struct lba_cacheent_t *cent;
    struct cached_page_t *blk;

    UNUSED(kernel);

    if((d = get_mount_info(link->dev)) == NULL || !(d->super))
    {
        return -EINVAL;
    }
    
    // the root node is stored in the Primary Volume Descriptor (PVD)
    buf = (char *)d->super->data + 156;
    dent = (struct iso9660_dirent_t *)buf;
    root = GET_DWORD(dent->lba);

    KDEBUG("*** root 0x%lx, ino 0x%lx\n", root, link->inode);

    if(link->inode == root)
    {
        return -EINVAL;
    }

    // other nodes (not root)
    if((cent = get_cacheent(link->dev, link->inode)) != NULL)
    {
        struct fs_node_header_t tmpnode;

        tmpnode.inode = PCACHE_NOINODE;
        tmpnode.dev = link->dev;

        lba_parent = cent->lba_parent;
        block_size = d->block_size;
        
        while(lba_parent < cent->llba_parent)
        {
            if(!(blk = get_cached_page((struct fs_node_t *)&tmpnode, lba_parent, 0)))
            {
                return -EIO;
            }
            
            buf = (char *)blk->virt;
            lbuf = buf + block_size;
            
            while(buf < lbuf)
            {
                dent = (struct iso9660_dirent_t *)buf;

                // end of sector might be zero-padded if:
                //   - we reached the end of directory
                //   - the next entry cannot fit in the remaining space in the
                //     sector
                //
                // in both cases, we skip to the next sector. if it past the 
                // directory size, our work is done (case 1 above), otherwise 
                // we continue reading the next sector to get the next entry
                // (case 2 above).
                if(dent->reclen == 0)
                {
                    buf = lbuf;
                    continue;
                }

                //lba = GET_DWORD(dent->lba);
                lba = get_inode_number(dent, lba_parent, (uintptr_t)buf - blk->virt);

                if(link->inode == lba)
                {
                    long res = copy_symlink(dent, target, targetsz, 0);

                    release_cached_page(blk);

                    return res;
                }
                
                buf += dent->reclen;
            }
            
            release_cached_page(blk);
            lba_parent++;
        }
    }

    return -ENOENT;
}


/*
 * Write the contents of a symbolic link. As different filesystems might have
 * different ways of storing symlinks (e.g. ext2 stores links < 60 chars in
 * length in the inode struct itself), we hand over this task to the
 * filesystem.
 *
 * Inputs:
 *    link => the symlink's inode
 *    target => the buffer containing the symlink's target to be saved
 *    len => size of buffer above
 *    kernel => set if the caller is a kernel function (i.e. 'target' address
 *              is in kernel memory), 0 if 'target' is a userspace address
 *
 * Returns:
 *    number of chars written on success, -errno on failure
 */
size_t iso9660fs_write_symlink(struct fs_node_t *link, char *target,
                               size_t len, int kernel)
{
    UNUSED(link);
    UNUSED(target);
    UNUSED(len);
    UNUSED(kernel);
    
    return -ENOSYS;
}

