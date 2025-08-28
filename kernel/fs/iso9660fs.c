/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
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


/*
 * As ISO9660 has no notion of inode numbers, we cheat by using LBA addresses
 * as inode numbers. To avoid having to walk down the directory tree every
 * time we want to access a file/dir, we cache the LBA of each entry we 
 * encounter, with the LBA of its parent, so that we can read the parent dir 
 * to find the file.
 */
struct lba_cacheent_t
{
    uint32_t lba;
    uint32_t lba_parent;
    uint32_t llba_parent;
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


static uint32_t iso9660_timedate_to_posix_time(unsigned char *date)
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


static void set_node_flags(struct fs_node_t *node, struct iso9660_dirent_t *dent)
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
    
    node->ctime = iso9660_timedate_to_posix_time(dent->datetime);
    node->mtime = node->ctime;
    node->atime = node->ctime;
    
    /*
     * TODO: read the extended attribute record (if any) for user/group 
     *       permissions.
     */
    
    node->uid = 0;
    node->gid = 0;
    node->mode |= (S_IRUSR | S_IRGRP | S_IROTH);
}


static struct lba_cacheent_t *get_cacheent(dev_t dev, uint32_t lba)
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
            if(cent->lba != lba)
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


static struct lba_cacheent_t *alloc_cacheent(uint32_t lba, uint32_t lba_parent,
                                             uint32_t llba_parent)
{
    struct lba_cacheent_t *cent;

    if(!(cent = kmalloc(sizeof(struct lba_cacheent_t))))
    {
        return NULL;
    }
    
    A_memset(cent, 0, sizeof(struct lba_cacheent_t));
    cent->lba = lba;
    cent->lba_parent = lba_parent;
    cent->llba_parent = llba_parent;
    
    return cent;
}


static int add_cacheent(struct fs_node_t *dir, uint32_t lba, uint32_t block_size)
{
    struct lba_cache_t *c;
    struct lba_cacheent_t *cent;
    uint32_t lba_parent, llba_parent;
    dev_t dev = dir->dev;
    size_t blocks = dir->size / block_size;
    
    if(dir->size % block_size)
    {
        blocks++;
    }
    
    lba_parent = dir->blocks[0];
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
                if(cent->lba == lba)
                {
                    // it is, don't do anything
                    kernel_mutex_unlock(&c->lock);
                    return 0;
                }
            }
            
            // it isn't, add a new entry
            if(!(cent = alloc_cacheent(lba, lba_parent, llba_parent)))
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

        if(!(cent = alloc_cacheent(lba, lba_parent, llba_parent)))
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
 * ISO9660 filenames take the format: 'FILENAME;ID'.
 *
 * This function compares the name of the file/dir passed to us in 'origname'
 * to the ISO9660 name in 'cdname', converting uppercase letters to lowercase
 * and ignoring the file ID number, including the semicolon, in 'cdname'.
 *
 * The length passed in 'len' is the strlen of 'cdname', not 'origname'. This
 * is because we can identify the end of 'origname' by finding the null-
 * terminating byte, while we can't find the end of 'cdname' as it might not
 * be null-terminated.
 *
 * This function enables us to compare a filename like 'boot' to the ISO9660 
 * name 'BOOT;1', for example.
 *
 * Returns:
 *    0 if the names as the same, non-zero otherwise.
 */
static int iso9660_strncmp(char *cdname, char *origname, size_t len, int isdir)
{
    KDEBUG("iso9600_strncpy: cdname '%s', origname '%s'\n", cdname, origname);

    // Alloc filenames on the stack to speed up comparison.
    // This should be ok, as ISO9660 should be <= 255 chars, even with ISO9660
    // extensions like Rock Ridge (upto 255 chars, 8 bits each) and Joliet
    // (upto 64 UCS-2 chars, 16 bit each, for a total of 128 bytes).
    char buf[len + 1];

    iso9660_strncpy(buf, cdname, len, isdir);

    KDEBUG("iso9600_strncpy: cdname '%s', buf '%s'\n", cdname, buf);

    return strcmp(buf, origname);
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
    physical_addr ignored;
    int maj = MAJOR(dev);
    char *buf;
    ino_t root;
    struct iso9660_dirent_t *dent;
    
    if(maj >= NR_DEV || !bdev_tab[maj].strategy)
    {
        return -EIO;
    }

    if(!(super = kmalloc(sizeof(struct superblock_t))))
    {
        return -EAGAIN;
    }

    A_memset(super, 0, sizeof(struct superblock_t));

    if(get_next_addr(&ignored, &super->data, PTE_FLAGS_PW, REGION_PCACHE) != 0)
    {
        kfree(super);
        return -EAGAIN;
    }

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
        vmmngr_free_page(get_page_entry((void *)super->data));  \
        vmmngr_flush_tlb_entry(super->data);    \
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
        
        // the root node is stored in the Primary Volume Descriptor (PVD)
        dent = (struct iso9660_dirent_t *)(buf + 156);
        root = GET_DWORD(dent->lba);
        d->root = get_node(dev, root, 0);

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

        vmmngr_free_page(get_page_entry((void *)super->data));
        vmmngr_flush_tlb_entry(super->data);
        kfree(super);

        break;
    }
}


/*
 * Reads inode data structure from disk.
 */
long iso9660fs_read_inode(struct fs_node_t *node)
{
    struct mount_info_t *d;
    char *buf, *lbuf;
    ino_t root;
    uint32_t lba, lba_parent;
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

    KDEBUG("iso9660fs_read_inode: got root node - n 0x%x, root 0x%x\n", node->inode, root);
    
    if(node->inode == root)
    {
        KDEBUG("iso9660fs_read_inode: got root node - lba 0x%x, sz 0x%x\n", root, dent->size);
        node->blocks[0] = root;
        node->size = GET_DWORD(dent->size);
        set_node_flags(node, dent);
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
                lba = GET_DWORD(dent->lba);
                
                if(node->inode == lba)
                {
                    node->blocks[0] = lba;
                    node->size = GET_DWORD(dent->size);
                    set_node_flags(node, dent);
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
    
    size_t blocks = node->size / block_size;
    
    if(node->size % block_size)
    {
        blocks++;
    }
    
    if(lblock >= blocks)
    {
        return 0;
    }
    
    return node->blocks[0] + lblock;
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
    
    // account for special entries '\0' and '\1', which stand for '.' and '..'
    if(*name == '\0' || *name == '\1')
    {
        namelen++;
    }
    
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
    
    // account for special entries '\0' and '\1', which stand for '.' and '..'
    if(*name == '\0')
    {
        entry->d_name[0] = '.';
        entry->d_name[1] = '\0';
    }
    else if(*name == '\1')
    {
        entry->d_name[0] = '.';
        entry->d_name[1] = '.';
        entry->d_name[2] = '\0';
    }
    else
    {
        //// name might not be null-terminated
        iso9660_strncpy(entry->d_name, name, namelen, IS_ISO9660_DIR(flags));
        //entry->d_name[namelen] = '\0';
    }
    
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
long iso9660fs_finddir(struct fs_node_t *dir, char *filename,
                       struct dirent **entry, struct cached_page_t **dbuf,
                       size_t *dbuf_off)
{
    size_t offset = 0;
    size_t fnamelen;
    uint32_t lba;
    unsigned char *blk, *end;
    char *n;
    struct cached_page_t *buf;
    struct mount_info_t *d;
    struct iso9660_dirent_t *dent;

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

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

    while(offset < dir->size)
    {
        KDEBUG("iso9660fs_finddir: offset 0x%x, dir->size 0x%x\n", offset, dir->size);
        KDEBUG("iso9660fs_finddir: block 0x%x\n", block);

        if(!(buf = get_cached_page(dir, offset, 0)))
        {
            offset += PAGE_SIZE;
            continue;
        }

        KDEBUG("iso9660fs_finddir: buf @ 0x%x\n", buf);
        
        blk = (unsigned char *)buf->virt;
        end = blk + PAGE_SIZE;
        
        while(blk < end)
        {
            n = (char *)(blk + sizeof(struct iso9660_dirent_t));
            KDEBUG("iso9660fs_finddir: blk 0x%x, end 0x%x\n", blk, end);
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

            if(iso9660_strncmp(n, filename, dent->namelen,
                               IS_ISO9660_DIR(dent->flags)) == 0)
            {
                lba = GET_DWORD(dent->lba);

                add_cacheent(dir, lba, d->block_size);

                *entry = iso9660_entry_to_dirent(NULL, lba, n, dent->namelen,
                                  offset + (blk - (unsigned char *)buf->virt),
                                  dent->flags);
                *dbuf = buf;
                *dbuf_off = (size_t)(blk - buf->virt);
                return 0;
            }

            blk += dent->reclen;
        }
        
        release_cached_page(buf);
        offset += PAGE_SIZE;
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
long iso9660fs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                                struct dirent **entry,
                                struct cached_page_t **dbuf, size_t *dbuf_off)
{
    size_t offset = 0;
    uint32_t lba;
    unsigned char *blk, *end;
    char *n;
    struct cached_page_t *buf;
    struct mount_info_t *d;
    struct iso9660_dirent_t *dent;

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    if(!dir || !node)
    {
        return -EINVAL;
    }

    if((d = get_mount_info(dir->dev)) == NULL || !(d->super))
    {
        return -EINVAL;
    }

    while(offset < dir->size)
    {
        if(!(buf = get_cached_page(dir, offset, 0)))
        {
            offset += PAGE_SIZE;
            continue;
        }
        
        blk = (unsigned char *)buf->virt;
        end = blk + PAGE_SIZE;
        
        while(blk < end)
        {
            n = (char *)(blk + sizeof(struct iso9660_dirent_t));
            dent = (struct iso9660_dirent_t *)blk;
            lba = GET_DWORD(dent->lba);

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
            
            if(matching_node(dir->dev, lba, node))
            {
                add_cacheent(dir, lba, d->block_size);

                *entry = iso9660_entry_to_dirent(NULL, lba, n, dent->namelen,
                                 offset + (blk - (unsigned char *)buf->virt),
                                 dent->flags);
                *dbuf = buf;
                *dbuf_off = (size_t)(blk - buf->virt);
                return 0;
            }

            blk += dent->reclen;
        }
        
        release_cached_page(buf);
        offset += PAGE_SIZE;
    }
    
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
    char *p;
    struct cached_page_t *buf;
    struct mount_info_t *d;
    struct iso9660_dirent_t *ent;
    size_t offset;
    size_t sz = sizeof(struct iso9660_dirent_t);
    uint32_t lba;
    unsigned char *blk, *end;

    if((d = get_mount_info(dir->dev)) == NULL || !(d->super))
    {
        return 0;
    }
    
    if(!dir->size || !dir->blocks[0] || !(buf = get_cached_page(dir, 0, 0)))
    {
        printk("iso9660: bad directory inode at 0x%x:0x%x\n",
               dir->dev, dir->inode);
        return 0;
    }

    // check '.'
    ent = (struct iso9660_dirent_t *)buf->virt;
    lba = GET_DWORD(ent->lba);
    //p = (char *)(buf->data + sz);
    
    if(!ent->reclen || lba != dir->inode /* || ent->namelen != 0 */)
    {
        printk("iso9660: bad directory inode at 0x%x:0x%x\n",
               dir->dev, dir->inode);
        return 0;
    }

    // check '..'
    ent = (struct iso9660_dirent_t *)(buf->virt + ent->reclen);
    lba = GET_DWORD(ent->lba);
    p = (char *)(ent) + sz;

    if(!ent->reclen || !lba ||
       /* ent->namelen != 2 || */ strncmp(p, "\1", 1))
    {
        printk("iso9660: bad directory inode at 0x%x:0x%x\n",
               dir->dev, dir->inode);
        return 0;
    }
    
    blk = (unsigned char *)ent + ent->reclen;
    end = (unsigned char *)buf->virt + PAGE_SIZE;
    offset = 0;

    while(offset < dir->size)
    {
        while(blk < end)
        {
            ent = (struct iso9660_dirent_t *)blk;
            lba = GET_DWORD(ent->lba);

            if(lba)
            {
                release_cached_page(buf);
                return 0;
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
        offset += PAGE_SIZE;

        if(offset >= dir->size)
        {
            break;
        }

        if(!(buf = get_cached_page(dir, offset, 0)))
        {
            break;
        }

        blk = (unsigned char *)buf->virt;

        if(offset + PAGE_SIZE > dir->size)
        {
            end = blk + (dir->size % PAGE_SIZE);
        }
        else
        {
            end = blk + PAGE_SIZE;
        }
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
    size_t i, count = 0;
    size_t reclen;
    struct cached_page_t *dbuf = NULL;
    struct dirent *dent = NULL;
    char *b = (char *)buf;
    unsigned char *blk, *end;
    char *n;
    size_t offset;
    uint32_t lba;
    struct mount_info_t *d;
    struct iso9660_dirent_t *ent;

    if(!dir || !pos || !buf || !bufsz)
    {
        return -EINVAL;
    }

    if((d = get_mount_info(dir->dev)) == NULL || !(d->super))
    {
        return 0;
    }

    offset = (*pos) & ~(PAGE_SIZE - 1);
    i = (*pos) % PAGE_SIZE;
    
    while(offset < dir->size)
    {
        KDEBUG("iso9660fs_getdents: 0 offset 0x%x, dir->size 0x%x\n", offset, dir->size);

        if(!(dbuf = get_cached_page(dir, offset, 0)))
        {
            offset += PAGE_SIZE;
            continue;
        }
        
        blk = (unsigned char *)(dbuf->virt + i);
        end = (unsigned char *)(dbuf->virt + d->block_size);
        
        // we use i only for the first round, as we might have been asked to
        // read from the middle of a block
        i = 0;
        
        while(blk < end)
        {
            KDEBUG("iso9660fs_getdents: 1 blk 0x%x, end 0x%x\n", blk, end);
            ent = (struct iso9660_dirent_t *)blk;
            *pos = offset + (blk - (unsigned char *)dbuf->virt);

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

            // calc dirent record length
            reclen = GET_DIRENT_LEN(ent->namelen);

            // make it 4-byte aligned
            //ALIGN_WORD(reclen);
            //KDEBUG("ext2_getdents_internal: reclen 0x%x\n", reclen);

            // check the buffer has enough space for this entry
            if((count + reclen) > (size_t)bufsz)
            {
                KDEBUG("iso9660fs_getdents: count 0x%x, reclen 0x%x\n", count, reclen);
                release_cached_page(dbuf);
                return count;
            }
            
            n = (char *)(blk + sizeof(struct iso9660_dirent_t));
            dent = (struct dirent *)b;
            lba = GET_DWORD(ent->lba);
            add_cacheent(dir, lba, d->block_size);

            iso9660_entry_to_dirent(dent, lba, n, ent->namelen,
                                 (*pos) + ent->reclen, ent->flags);

            KDEBUG("iso9660fs_getdents: name '%s'\n", dent->d_name);
            
            dent->d_reclen = reclen;
            b += reclen;
            count += reclen;
            blk += ent->reclen;
            KDEBUG("iso9660fs_getdents: 2 blk 0x%x, end 0x%x\n", blk, end);
        }
        
        release_cached_page(dbuf);
        offset += PAGE_SIZE;
    }
    
    KDEBUG("iso9660fs_getdents: count 0x%x, offset 0x%x\n", count, offset);
    
    *pos = offset;
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
long iso9660fs_read_symlink(struct fs_node_t *link, char *buf,
                            size_t bufsz, int kernel)
{
    UNUSED(link);
    UNUSED(buf);
    UNUSED(bufsz);
    UNUSED(kernel);
    
    return -ENOSYS;
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

