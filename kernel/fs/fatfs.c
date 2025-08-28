/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: fatfs.c
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
 *  \file fatfs.c
 *
 *  This file implements the File Allocation Table (FAT) filesystem functions.
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the fatfs_ops structure.
 */

//#define __DEBUG

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <fs/fatfs.h>
#include <fs/magic.h>
#include <mm/kheap.h>

extern time_t timegm(struct tm *tm);
extern char *utf16_to_utf8_char(uint16_t *str);

static size_t get_next_cluster(struct fat_private_t *priv, size_t cur_cluster);
static size_t count_free_clusters(struct fat_private_t *priv);

/*
 * See: https://wiki.osdev.org/FAT
 */

#if BYTE_ORDER == LITTLE_ENDIAN
# define GET_DWORD(d)           (d)
# define GET_WORD(w)            (w)
#else
# define GET_DWORD(d)           ((((d) & 0xff) << 24) | \
                                 (((d) & 0xff00) << 8) | \
                                 (((d) & 0xff0000) >> 8) | \
                                 (((d) & 0xff000000) >> 24))
# define GET_WORD(w)            ((((w) & 0xff) << 8) | (((w) & 0xff00) >> 8))
#endif      /* BYTE_ORDER */

#define FAT_ROOT_INODE          1

#define VALID_FAT_SIG(s)        ((s) == 0x28 || (s) == 0x29)

#define FAT_12                  0
#define FAT_16                  1
#define FAT_32                  2
#define FAT_EX                  3

#define FAT_DIRENT_SIZE         32

#define CHARS_PER_LFN_ENTRY     13

#define VALID_8_3_CHAR(c)       \
    (((c) >= 'A' && (c) <= 'Z') || \
     ((c) >= 'a' && (c) <= 'z') || \
     ((c) >= '0' && (c) <= '9') || \
     (c) == '$' || (c) == '%' || (c) == '\'' || \
     (c) == '-' || (c) == '_' || (c) == '@' || \
     (c) == '~' || (c) == '`' || (c) == '!' || \
     (c) == '(' || (c) == ')' || (c) == ' ')

static int lfn_char_offsets[] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };

// the following arrays are referenced via one of the FAT_* macros above
static char *fat_namestr[] = { "FAT12", "FAT16", "FAT32", "exFAT" };
static size_t end_of_chain[] = { 0xFF8, 0xFFF8, 0x0FFFFFF8, 0xFFFFFFF8 };
static size_t bad_cluster[] = { 0xFF7, 0xFFF7, 0x0FFFFFF7, 0xFFFFFFF7 };


// filesystem operations
struct fs_ops_t fatfs_ops =
{
    // inode operations
    .read_inode = fatfs_read_inode,
    .write_inode = fatfs_write_inode,
    //.trunc_inode = NULL,
    .alloc_inode = fatfs_alloc_inode,
    .free_inode = fatfs_free_inode,
    .bmap = fatfs_bmap,

    .read_symlink = fatfs_read_symlink,
    .write_symlink = fatfs_write_symlink,
    
    // directory operations
    .finddir = fatfs_finddir,
    .finddir_by_inode = fatfs_finddir_by_inode,
    .addir = fatfs_addir,
    .mkdir = fatfs_mkdir,
    .deldir = fatfs_deldir,
    .dir_empty = fatfs_dir_empty,
    .getdents = fatfs_getdents,

    // device operations
    .mount = NULL,
    .umount = NULL,
    .read_super = fatfs_read_super,
    .write_super = NULL,
    .put_super = fatfs_put_super,
    .ustat = fatfs_ustat,
    .statfs = fatfs_statfs,
};


static uint16_t unix_time_to_fat_time(time_t unix_time)
{
    time_t t = unix_time;
    struct tm *time = gmtime(&t);

    // secs need to be / 2
    uint16_t res = (time->tm_sec / 2) & 0x1F;
    res |= (((time->tm_min) & 0x3F) << 5);
    res |= (((time->tm_hour) & 0x1F) << 11);
    return res;
}

static uint16_t unix_time_to_fat_date(time_t unix_time)
{
    time_t t = unix_time;
    struct tm *time = gmtime(&t);

    // month day is 1-31 in both
    uint16_t res = (time->tm_mday) & 0x1F;
    // DOS month is 1-12, tm month is 0-11
    res |= (((time->tm_mon + 1) & 0x0F) << 5);
    // DOS year is from 1980, tm year is from 1900
    res |= (((time->tm_year - 80) & 0x7F) << 9);
    return res;
}

static time_t fat_timedate_to_unix_time(uint16_t date, uint16_t time)
{
    struct tm ftm;
    // DOS year is from 1980, tm year is from 1900
    ftm.tm_year = ((date >> 9) & 0x7F) + 80;
    // DOS month is 1-12, tm month is 0-11
    ftm.tm_mon  = ((date >> 5) & 0x0F) - 1;
    // month day is 1-31 in both
    ftm.tm_mday = (date & 0x1F);
    ftm.tm_hour = (time >> 11) & 0x1F;
    ftm.tm_min  = (time >> 5 ) & 0x3F;
    // secs need to be * 2
    ftm.tm_sec  = (time & 0x1F) * 2;
    return timegm(&ftm);
}


static void dirent_to_node(struct fs_node_t *node, struct fat_dirent_t *dent)
{
    node->mode = 0;
    
    if(dent->attribs & FAT_ATTRIB_DIRECTORY)
    {
        node->mode |= S_IFDIR;
        node->mode |= (S_IXUSR | S_IXGRP /* | S_IXOTH */);
        
        // give directories a link count of 2 at least, to account for dot and
        // dot-dot entries
        node->links = 2;
    }
    else
    {
        if(!(dent->attribs & FAT_ATTRIB_VOLUMEID))
        {
            node->mode |= S_IFREG;
        }

        // give files a link count of 1, as we don't support hard links on CDs
        node->links = 1;
    }

    if(!(dent->attribs & FAT_ATTRIB_READONLY))
    {
        node->mode |= (S_IWUSR | S_IWGRP /* | S_IWOTH */);
    }

    node->ctime = fat_timedate_to_unix_time(dent->cdate, dent->ctime);
    node->mtime = fat_timedate_to_unix_time(dent->mdate, dent->mtime);
    node->atime = fat_timedate_to_unix_time(dent->adate, 0);

    node->uid = 0;
    node->gid = 0;
    node->mode |= (S_IRUSR | S_IRGRP /* | S_IROTH */);
    node->size = dent->size;
}


static void node_to_dirent(volatile struct fat_dirent_t *dent, struct fs_node_t *node)
{
    /*
     * TODO: update other stuff in addition to the times
     */
    dent->cdate = unix_time_to_fat_date(node->ctime);
    dent->ctime = unix_time_to_fat_time(node->ctime);
    dent->mdate = unix_time_to_fat_date(node->mtime);
    dent->mtime = unix_time_to_fat_time(node->mtime);
    dent->adate = unix_time_to_fat_date(node->atime);
}


static int get_cacheent(struct fat_private_t *priv, size_t cluster, size_t *pc)
{
    struct fat_cacheent_t *cent;

    kernel_mutex_lock(&priv->lock);

    for(cent = priv->cacheent; cent != NULL; cent = cent->next)
    {
        if(cent->child_cluster != cluster)
        {
            continue;
        }

        *pc = cent->parent_cluster;
        kernel_mutex_unlock(&priv->lock);
        return 0;
    }

    kernel_mutex_unlock(&priv->lock);
    return -ENOENT;
}


static void remove_cacheent(struct fat_private_t *priv, size_t cluster)
{
    struct fat_cacheent_t *cent, *prev = NULL;

    kernel_mutex_lock(&priv->lock);

    for(cent = priv->cacheent; cent != NULL; cent = cent->next)
    {
        if(cent->child_cluster != cluster)
        {
            prev = cent;
            continue;
        }

        if(prev)
        {
            prev->next = cent->next;
        }
        else
        {
            priv->cacheent = cent->next;
        }

        kernel_mutex_unlock(&priv->lock);
        kfree(cent);
        return;
    }

    kernel_mutex_unlock(&priv->lock);
}


static struct fat_cacheent_t *alloc_cacheent(size_t child_cluster, 
                                             size_t parent_cluster)
{
    struct fat_cacheent_t *cent;

    if(!(cent = kmalloc(sizeof(struct fat_cacheent_t))))
    {
        return NULL;
    }

    A_memset(cent, 0, sizeof(struct fat_cacheent_t));
    cent->child_cluster = child_cluster;
    cent->parent_cluster = parent_cluster;

    return cent;
}


static int add_cacheent(struct fat_private_t *priv, 
                        size_t child_cluster, size_t parent_cluster)
{
    struct fat_cacheent_t *cent;
    
    // an entry with first cluster == 0 refers to the root directory on
    // FAT12/16, and is an empty file/dir on FAT32. In either case, we
    // don't want to add this to the cache.
    if(child_cluster == 0)
    {
        return 0;
    }

    // if child & parent clusters are equal, assume this refers to a '.'
    // directory entry and ignore it
    if(child_cluster == parent_cluster)
    {
        return 0;
    }

    kernel_mutex_lock(&priv->lock);

    // find out if this lba is already cached
    for(cent = priv->cacheent; cent != NULL; cent = cent->next)
    {
        if(cent->child_cluster == child_cluster)
        {
            // it is, just update parent as file might have been moved
            cent->parent_cluster = parent_cluster;
            kernel_mutex_unlock(&priv->lock);
            return 0;
        }
    }

    // it isn't, add a new entry
    if(!(cent = alloc_cacheent(child_cluster, parent_cluster)))
    {
        kernel_mutex_unlock(&priv->lock);
        return -ENOMEM;
    }

    cent->next = priv->cacheent;
    priv->cacheent = cent;
    kernel_mutex_unlock(&priv->lock);
    return 0;
}


/*
 * Initialise and register the FAT filesystem.
 */
void fatfs_init(void)
{
    fs_register("vfat", &fatfs_ops);
}


STATIC_INLINE size_t first_sector_of_cluster(struct fat_private_t *priv, size_t cluster)
{
    return ((cluster - 2) * priv->sectors_per_cluster) + priv->first_data_sector;
}


STATIC_INLINE size_t cluster_from_dirent(struct fat_private_t *priv, struct fat_dirent_t *dent)
{
    if(priv->fattype == FAT_32)
    {
        return (dent->first_cluster_hi << 16) | dent->first_cluster_lo;
    }
    else
    {
        return dent->first_cluster_lo;
    }
}


STATIC_INLINE int get_priv(dev_t dev, struct fat_private_t **priv)
{
    struct mount_info_t *d;

    if((d = get_mount_info(dev)) == NULL || !(d->super))
    {
        return -EINVAL;
    }

    *priv = (struct fat_private_t *)(d->super->privdata);
    return 0;
}


/*
 * Read the filesystem's superblock and root inode.
 * This function fills in the mount info struct's block_size, super,
 * and root fields.
 */
long fatfs_read_super(dev_t dev, struct mount_info_t *d, size_t bytes_per_sector)
{
    struct fat_bootsect_t *boot;
    struct fat_private_t *priv;
    struct superblock_t *super;
    struct disk_req_t req;
    physical_addr ignored;
    int maj = MAJOR(dev);

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

    super->blockno = 0;
    super->blocksz = bytes_per_sector;
    super->dev = dev;
    
    req.dev = dev;
    req.data = super->data;
    req.datasz = super->blocksz;
    req.fs_blocksz = super->blocksz;

    req.blockno = super->blockno;
    req.write = 0;

    printk("vfat: reading superblock (dev 0x%x)\n", dev);

#define BAIL_OUT(err)   \
        vmmngr_free_page(get_page_entry((void *)super->data));  \
        vmmngr_flush_tlb_entry(super->data);    \
        kfree(super);   \
        return err;

    if(bdev_tab[maj].strategy(&req) < 0)
    {
        printk("vfat: failed to read from disk -- aborting mount\n");
        BAIL_OUT(-EIO);
    }
    
    if(!(super->privdata = (virtual_addr)kmalloc(sizeof(struct fat_private_t))))
    {
        printk("vfat: insufficient memory to store private data\n");
        BAIL_OUT(-ENOMEM);
    }

    A_memset((void *)super->privdata, 0, sizeof(struct fat_private_t));

    boot = (struct fat_bootsect_t *)super->data;
    priv = (struct fat_private_t *)super->privdata;

    priv->dev = dev;
    priv->blocksz = GET_WORD(boot->base.bytes_per_sector);
    priv->sectors_per_cluster = boot->base.sectors_per_cluster;

    // first sector in the FAT
    priv->first_fat_sector = GET_WORD(boot->base.reserved_sector_count);

    if(priv->blocksz == 0)
    {
        // NOTE: this could still be a valid exFAT system, but we do not
        //       support this at the moment
        printk("vfat: boot sector with zero bytes per sector -- aborting mount\n");
        kfree((void *)super->privdata);
        BAIL_OUT(-EINVAL);
    }

    // this will be 0 for FAT32
    priv->root_dir_sectors = 
                ((GET_WORD(boot->base.root_entry_count) * 32) + 
                        (priv->blocksz - 1)) / priv->blocksz;

    // FAT size in sectors
    priv->fat_size = GET_WORD(boot->base.table_size_16);

    if(priv->fat_size == 0)
    {
        priv->fat_size = GET_DWORD(boot->fat32.table_size_32);
    }

    // first data sector
    priv->first_data_sector = GET_WORD(boot->base.reserved_sector_count) + 
                (boot->base.table_count * priv->fat_size) + priv->root_dir_sectors;

    // total sectors
    priv->total_sectors = GET_WORD(boot->base.total_sectors_16);

    if(priv->total_sectors == 0)
    {
        priv->total_sectors = GET_DWORD(boot->base.total_sectors_32);
    }

    // data sectors
    priv->data_sectors = priv->total_sectors - priv->first_data_sector;

    if(priv->sectors_per_cluster == 0)
    {
        printk("vfat: boot sector with zero sectors per sector -- aborting mount\n");
        kfree((void *)super->privdata);
        BAIL_OUT(-EINVAL);
    }

    // total clusters
    priv->total_clusters = priv->data_sectors / priv->sectors_per_cluster;

    // now determine the type of FAT system we have
    if(priv->blocksz == 0)
    {
        priv->fattype = FAT_EX;
    }
    else if(priv->total_clusters < 65525)
    {
        if(!VALID_FAT_SIG(boot->fat12_16.boot_signature))
        {
            printk("vfat: invalid boot signature (0x%x) -- aborting mount\n", 
                    boot->fat12_16.boot_signature);
            kfree((void *)super->privdata);
            BAIL_OUT(-EINVAL);
        }

        priv->fattype = (priv->total_clusters < 4085) ? FAT_12 : FAT_16;
        priv->first_root_dir_sector = priv->first_data_sector - priv->root_dir_sectors;
        priv->first_root_dir_cluster = 0;
    }
    else
    {
        if(!VALID_FAT_SIG(boot->fat32.boot_signature))
        {
            printk("vfat: invalid boot signature (0x%x) -- aborting mount\n", 
                    boot->fat32.boot_signature);
            kfree((void *)super->privdata);
            BAIL_OUT(-EINVAL);
        }

        priv->fattype = FAT_32;
        priv->first_root_dir_sector = 0;
        priv->first_root_dir_cluster = GET_DWORD(boot->fat32.root_cluster);
    }

#undef BAIL_OUT

    d->block_size = priv->blocksz;
    d->super = super;

    priv->free_clusters = count_free_clusters(priv);

    printk("vfat: dev 0x%x, fattype %s\n", dev, fat_namestr[priv->fattype]);
    printk("vfat: blocksz %ld, fat_size %ld\n", priv->blocksz, priv->fat_size);
    printk("vfat: total_clusters %ld, total_sectors %ld, data_sectors %ld\n", 
            priv->total_clusters, priv->total_sectors, priv->data_sectors);
    printk("vfat: sectors_per_cluster %ld\n", priv->sectors_per_cluster);
    printk("vfat: first_root_dir_sector %ld, first_root_dir_cluster %ld\n",
            priv->first_root_dir_sector, priv->first_root_dir_cluster);
    printk("vfat: root_dir_sectors %ld, first_fat_sector %ld, first_data_sector %ld\n", 
            priv->root_dir_sectors, priv->first_fat_sector, priv->first_data_sector);

    printk("vfat: reading root node\n");
    d->root = get_node(dev, FAT_ROOT_INODE, 0);

    printk("vfat: mounting done\n");

    return 0;
}


/*
 * Release the filesystem's superblock and its buffer.
 * Called when unmounting the filesystem.
 */
void fatfs_put_super(dev_t dev, struct superblock_t *super)
{
    UNUSED(dev);

    struct fat_private_t *priv;
    struct fat_cacheent_t *cent, *next;

    if(!super || !super->data)
    {
        return;
    }

    //fatfs_write_super(dev, super);
    priv = (struct fat_private_t *)super->privdata;

    if(priv)
    {
        kernel_mutex_lock(&priv->lock);

        for(cent = priv->cacheent; cent != NULL; )
        {
            next = cent->next;
            kfree(cent);
            cent = next;
        }

        super->privdata = 0;
        kernel_mutex_unlock(&priv->lock);
        kfree((void *)priv);
    }

    vmmngr_free_page(get_page_entry((void *)super->data));
    vmmngr_flush_tlb_entry(super->data);
    kfree(super);
}


static void dos_to_unix_name(char *buf, char *name)
{
    char *p = buf;
    int i;

    *p = '\0';

    // get the name
    for(i = 0; i < 8; i++)
    {
        if(name[i] == ' ')
        {
            break;
        }

        *p = (name[i] >= 'A' && name[i] <= 'Z') ?
                (name[i] + ('a' - 'A')) : name[i];
        p++;
    }

    // get the (optional) extension
    if(name[8] != ' ')
    {
        *p++ = '.';

        for(i = 8; i < 11; i++)
        {
            if(name[i] == ' ')
            {
                break;
            }

            *p = (name[i] >= 'A' && name[i] <= 'Z') ?
                    (name[i] + ('a' - 'A')) : name[i];
            p++;
        }
    }

    *p = '\0';
}


/*
 * Compare two names, regardless of their letter case.
 */
static int fat_strcmp(char *__s1, char *__s2)
{
    unsigned char *s1 = (unsigned char *)__s1;
    unsigned char *s2 = (unsigned char *)__s2;
    unsigned char c1, c2;

    while(*s1 && *s2)
    {
        c1 = (*s1 >= 'A' && *s1 <= 'Z') ? (*s1 + 'a' - 'A') : *s1;
        c2 = (*s2 >= 'A' && *s2 <= 'Z') ? (*s2 + 'a' - 'A') : *s2;

        if(c1 != c2)
        {
            return -1;
        }

        s1++;
        s2++;
    }

    return !!(*s1) != !!(*s2);
}


#if BYTE_ORDER == LITTLE_ENDIAN
# define UTF16(l, h)            ((l) | ((h) << 8))
#else
# define UTF16(l, h)            ((h) | ((l) << 8))
#endif      /* BYTE_ORDER */


static char *lfn_finalise(uint16_t *lfn_buf, size_t lfn_len)
{
    uint16_t *p = lfn_buf + lfn_len;

    if(!lfn_buf)
    {
        return NULL;
    }

    // make sure it is NULL-terminated
    lfn_buf[lfn_len] = 0x0000;

    // first get rid of the padding
    while(--p >= lfn_buf)
    {
        if(*p == 0xFFFF)
        {
            *p = 0x0000;
        }
    }

    // next convert from UTF16 to UTF8
    return utf16_to_utf8_char(lfn_buf);
}


#define FINALIZE_AND_SET_LFN(L)                     \
    add_cacheent(priv, cluster_from_dirent(priv, dent), dir->inode);  \
    *dbuf = buf;                                    \
    *dbuf_off = (size_t)(blk - buf->virt);          \
    *__lfn = L;                                     \
    *stream_off = offset;                           \
    if(lfn_buf) kfree(lfn_buf);


static long fat_get_dirent(struct fs_node_t *dir, char *filename, 
                           size_t child_cluster, struct cached_page_t **dbuf, 
                           size_t *dbuf_off, char **__lfn, size_t *stream_off)
{
    struct fat_private_t *priv;
    struct fat_dirent_t *dent;
    struct cached_page_t *buf;
    unsigned char *blk, *end;
    char *lfn = NULL;
    uint16_t *lfn_buf = NULL;
    size_t offset = 0;
    size_t lfn_len = 0;
    size_t cur_cluster;
    int ignore_lfn = 0, i;

    *dbuf = NULL;
    *dbuf_off = 0;
    *__lfn = NULL;

    if(!dir)
    {
        kpanic("fat_get_dirent: called with NULL dir\n");
    }

    KDEBUG("fat_get_dirent: dev 0x%x, ino 0x%x, file '%s'\n", dir->dev, dir->inode, filename ? filename : "(null)");

    if(get_priv(dir->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    if((lfn_buf = kmalloc((NAME_MAX * 2) + 4)))
    {
        A_memset(lfn_buf, 0, (NAME_MAX * 2) + 4);
    }

    while(offset < dir->size)
    {
        KDEBUG("fat_get_dirent: offset 0x%x, dir->size 0x%x\n", offset, dir->size);
        KDEBUG("fat_get_dirent: block 0x%x\n", block);

        if(!(buf = get_cached_page(dir, offset, 0)))
        {
            offset += PAGE_SIZE;
            continue;
        }

        blk = (unsigned char *)buf->virt;
        end = blk + PAGE_SIZE;
        
        while(blk < end)
        {
            KDEBUG("fat_get_dirent: blk 0x%x, end 0x%x\n", blk, end);
            dent = (struct fat_dirent_t *)blk;

            // last entry
            if(blk[0] == 0)
            {
                release_cached_page(buf);

                // account for the case where the short name was deleted but
                // the long name renamed, and free the buffer if alloc'd
                if(lfn)
                {
                    kfree(lfn);
                }

                if(lfn_buf)
                {
                    kfree(lfn_buf);
                }

                return -ENOENT;
            }

            // unused entry
            if(blk[0] == 0xE5)
            {
                // account for the case where the short name was deleted but
                // the long name renamed, and free the buffer if alloc'd
                if(lfn)
                {
                    kfree(lfn);
                    lfn = NULL;
                }

                ignore_lfn = 0;
                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // Long File Name (LFN) entry
            if(dent->attribs == FAT_ATTRIB_LFN)
            {
                if(lfn_buf && !ignore_lfn)
                {
                    // find order of entry in this long name,
                    // the counting is 1-based
                    int x = ((blk[0] & ~0x40) & 0xff);

                    // if the last entry, calculate LFN length
                    if(blk[0] & 0x40)
                    {
                        lfn_len = x * CHARS_PER_LFN_ENTRY;
                    }

                    // even long names should not be too long
                    if(lfn_len >= NAME_MAX || x <= 0 || x >= 0x40)
                    {
                        ignore_lfn = 1;
                    }
                    else
                    {
                        x = (x - 1) * CHARS_PER_LFN_ENTRY;

                        // name chars are scattered through the entry so
                        // we have to collect them
                        for(i = 0; i < CHARS_PER_LFN_ENTRY; i++)
                        {
                            lfn_buf[x + i] = UTF16(blk[lfn_char_offsets[i]],
                                                   blk[lfn_char_offsets[i] + 1]);
                        }
                    }
                }

                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // normal 8.3 entry
            if(!ignore_lfn && lfn_len != 0)
            { 
                lfn = lfn_finalise(lfn_buf, lfn_len);
            }

            ignore_lfn = 0;

            // A - comparison by filename
            if(filename)
            {
                // first check to see if there is an associated LFN entry
                if(lfn)
                {
                    KDEBUG("fat_get_dirent: lfn '%s', filename '%s'\n", lfn, filename);
                    if(fat_strcmp(lfn, filename) == 0)
                    {
                        FINALIZE_AND_SET_LFN(lfn);
                        return 0;
                    }

                    kfree(lfn);
                }

                // LFN does not match or no LFN, try to match the short name
                // and reuse the LFN buffer as its no longer used this round
                lfn = NULL;
                dos_to_unix_name((char *)lfn_buf, (char *)blk);

                if(fat_strcmp((char *)lfn_buf, filename) == 0)
                {
                    FINALIZE_AND_SET_LFN(NULL);
                    return 0;
                }

                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // B - comparison by "inode" number
            cur_cluster = cluster_from_dirent(priv, dent);

            //if(child_cluster) printk("fat_get_dirent: child_cluster %ld, cur_cluster %ld\n", child_cluster, cur_cluster);

            if(cur_cluster == child_cluster ||
               (cur_cluster == 0 && 
                child_cluster == FAT_ROOT_INODE &&
                priv->fattype != FAT_32))
            {
                FINALIZE_AND_SET_LFN(lfn);
                return 0;
            }

            blk += FAT_DIRENT_SIZE;
        }
        
        release_cached_page(buf);
        offset += PAGE_SIZE;
    }

    // account for the case where the short name was deleted but
    // the long name renamed, and free the buffer if alloc'd
    if(lfn && lfn != (char *)lfn_buf)
    {
        kfree(lfn);
    }

    kfree(lfn_buf);
    return -ENOENT;
}


/*
 * Get the size of a directory in bytes.
 * Directories on FAT have a size of 0, so we need to traverse the directory's
 * clusters until we hit the end, then multiply the count by the cluster size.
 */
static size_t get_dir_size(struct fat_private_t *priv, size_t first_cluster)
{
    size_t cur_cluster, size;
    int first;

    cur_cluster = first_cluster;
    size = 0;
    first = 1;

    while(1)
    {
        size++;
        cur_cluster = get_next_cluster(priv, cur_cluster);

        // check for a free entry
        if(first && cur_cluster == 0)
        {
            return 0;
        }

        if(cur_cluster >= end_of_chain[priv->fattype] ||
           cur_cluster == bad_cluster[priv->fattype] ||
           cur_cluster < 2)
        {
            break;
        }

        first = 0;
    }

    size *= priv->sectors_per_cluster * priv->blocksz;
    return size;
}


/*
 * Get the size of the root directory in bytes.
 */
static size_t get_root_size(struct fat_private_t *priv)
{
    // for FAT12/16, this is easy: find it in the boot block
    if(priv->fattype == FAT_12 || priv->fattype == FAT_16)
    {
        return priv->root_dir_sectors * priv->blocksz;
    }

    // for FAT32, we have to traverse the root directory's clusters until
    // we hit the end, then multiply the count by the cluster size
    return get_dir_size(priv, priv->first_root_dir_cluster);
}


static struct fs_node_t *get_parent_node(struct fat_private_t *priv, 
                                         struct fs_node_t *node)
{
    struct fs_node_t *parent;
    size_t parent_cluster;

    //printk("get_parent_node: dev 0x%x, ino 0x%x\n", node->dev, node->inode);

    if(get_cacheent(priv, node->inode, &parent_cluster) != 0)
    {
        return NULL;
    }

    //printk("get_parent_node: dev 0x%x, parent ino 0x%x\n", node->dev, parent_cluster);

    if(node->inode == parent_cluster)
    {
        switch_tty(1);
        printk("get_parent_node: child and parent are the same\n");
        printk("get_parent_node: dev 0x%x, ino 0x%x\n", node->dev, node->inode);
        kpanic("*****\n");
    }

    if(!(parent = get_node(node->dev, parent_cluster, 0)))
    {
        return NULL;
    }

    // parent was deleted and its now a regular file
    if(!S_ISDIR(parent->mode))
    {
        //printk("get_parent_node: parent mode 0x%x\n", parent->mode);
        release_node(parent);
        return NULL;
    }

    //printk("get_parent_node: done\n");

    return parent;
}


/*
 * Reads inode data structure from disk.
 */
long fatfs_read_inode(struct fs_node_t *node)
{
    struct cached_page_t *dbuf;
    size_t dbuf_off, unused;
    char *lfn = NULL;
    struct fat_dirent_t *dent;
    struct fat_private_t *priv;
    struct fs_node_t *parent;

    if(get_priv(node->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    A_memset(node->blocks, 0, sizeof(node->blocks));
    printk("fatfs_read_inode: dev 0x%x, ino 0x%x\n", node->dev, node->inode);

    if(node->inode == FAT_ROOT_INODE)
    {
        /*
         * TODO: we should try reading the root directory to find the
         *       dot entry, which would hopefully have some useful info
         *       about root?
         */
        node->size = get_root_size(priv);
        node->mode = S_IFDIR | 0770;
        node->uid = 0;
        node->gid = 0;
        node->links = 2;
        node->ctime = now();
        node->mtime = node->ctime;
        node->atime = node->ctime;
        return 0;
    }

    // other nodes (not root)
    if((parent = get_parent_node(priv, node)) == NULL)
    {
        printk("fatfs_read_inode: dev 0x%x, ino 0x%x, parent NULL\n", node->dev, node->inode);
        return -ENOENT;
    }

    if(fat_get_dirent(parent, NULL, node->inode, &dbuf, &dbuf_off, &lfn, &unused) != 0)
    {
        printk("fatfs_read_inode: fat_get_dirent res != 0\n");
        release_node(parent);
        return -ENOENT;
    }

    release_node(parent);
    dent = (struct fat_dirent_t *)(dbuf->virt + dbuf_off);
    dirent_to_node(node, dent);
    release_cached_page(dbuf);

    // directory size is 0 on FAT, so we need to calculate the size
    if(S_ISDIR(node->mode))
    {
        node->size = get_dir_size(priv, node->inode);
    }

    if(lfn)
    {
        kfree(lfn);
    }

    return 0;
}


/*
 * Writes inode data structure to disk.
 */
long fatfs_write_inode(struct fs_node_t *node)
{
    struct cached_page_t *dbuf;
    size_t dbuf_off, unused;
    char *lfn = NULL;
    struct fat_dirent_t *dent;
    struct fat_private_t *priv;
    struct fs_node_t *parent;

    printk("fatfs_write_inode: dev 0x%x, ino 0x%x\n", node->dev, node->inode);

    if(get_priv(node->dev, &priv) < 0)
    {
        return -EINVAL;
    }
    
    if(node->inode == FAT_ROOT_INODE)
    {
        /*
         * TODO: we should try reading the root directory to find the
         *       dot entry, which we can then update with at least the
         *       last access time?
         */
        return 0;
    }

    // other nodes (not root)
    if((parent = get_parent_node(priv, node)) == NULL)
    {
        switch_tty(1);
        printk("fatfs_write_inode: could not get parent node\n");
        printk("fatfs_write_inode: dev 0x%x, ino 0x%x\n", node->dev, node->inode);
        kpanic("*****\n");

        return -ENOENT;
    }

    printk("fatfs_write_inode: dev 0x%x, ino 0x%x, refs %d, links %d, pdev 0x%x, pino 0x%x\n", node->dev, node->inode, node->refs, node->links, parent->dev, parent->inode);

    if(fat_get_dirent(parent, NULL, node->inode, &dbuf, &dbuf_off, &lfn, &unused) != 0)
    {
        switch_tty(1);
        printk("fatfs_write_inode: could not get dirent\n");
        printk("fatfs_write_inode: dev 0x%x, ino 0x%x, pdev 0x%x, pino 0x%x\n", node->dev, node->inode, parent->dev, parent->inode);
        kpanic("*****\n");

        release_node(parent);
        return -ENOENT;
    }

    release_node(parent);
    dent = (struct fat_dirent_t *)(dbuf->virt + dbuf_off);
    node_to_dirent(dent, node);

    // directory size is 0 on FAT, only store size if this is a file
    dent->size = S_ISDIR(node->mode) ? 0 : node->size;

    __sync_or_and_fetch(&dbuf->flags, PCACHE_FLAG_DIRTY);
    release_cached_page(dbuf);

    if(lfn)
    {
        kfree(lfn);
    }

    return 0;
}


static size_t count_free_clusters(struct fat_private_t *priv)
{
    struct cached_page_t *blk;
    struct fs_node_header_t tmpnode;
    volatile size_t fat_sector, i, count = 0;
    size_t steps;

    tmpnode.inode = PCACHE_NOINODE;
    tmpnode.dev = priv->dev;

    printk("count_free_clusters: dev 0x%x\n", priv->dev);

    for(fat_sector = priv->first_fat_sector;
        fat_sector < priv->first_fat_sector + priv->fat_size;
        fat_sector++)
    {
        if(!(blk = get_cached_page((struct fs_node_t *)&tmpnode, fat_sector, 0)))
        {
            continue;
        }

        if(priv->fattype == FAT_32 || priv->fattype == FAT_EX)
        {
            uint32_t *fat_table = (uint32_t *)blk->virt;

            for(i = 0, steps = priv->blocksz / 4; i < steps; i++)
            {
                if(fat_table[i] == 0)
                {
                    count++;
                }
            }
        }
        else if(priv->fattype == FAT_16)
        {
            uint16_t *fat_table = (uint16_t *)blk->virt;

            for(i = 0, steps = priv->blocksz / 2; i < steps; i++)
            {
                if(fat_table[i] == 0)
                {
                    count++;
                }
            }
        }
        else
        {
            uint8_t *fat_table = (uint8_t *)blk->virt;

            for(i = 0; i < priv->blocksz; i += 3)
            {
                // ignore the case where the entry is at sector boundary
                // TODO: although this misses only a few entries, we should
                //       find a cleaner solution
                if(i == priv->blocksz - 1)
                {
                    break;
                }

                // check the low 12 bits
                if(fat_table[i] == 0 && (fat_table[i + 1] & 0x0F) == 0)
                {
                    count++;
                }

                // check the high 12 bits
                if((fat_table[i] & 0xF0) == 0 && fat_table[i + 1] == 0)
                {
                    count++;
                }
            }
        }

        release_cached_page(blk);
    }

    printk("count_free_clusters: dev 0x%x -- count %ld\n", priv->dev, count);

    return count;
}


static size_t alloc_cluster(struct fat_private_t *priv)
{
    struct cached_page_t *blk;
    struct fs_node_header_t tmpnode;
    volatile size_t fat_sector, i;
    size_t steps;

    tmpnode.inode = PCACHE_NOINODE;
    tmpnode.dev = priv->dev;

    for(fat_sector = priv->first_fat_sector;
        fat_sector < priv->first_fat_sector + priv->fat_size;
        fat_sector++)
    {
        if(!(blk = get_cached_page((struct fs_node_t *)&tmpnode, fat_sector, 0)))
        {
            continue;
        }

        if(priv->fattype == FAT_32 || priv->fattype == FAT_EX)
        {
            uint32_t *fat_table = (uint32_t *)blk->virt;

            for(i = 0, steps = priv->blocksz / 4; i < steps; i++)
            {
                if(fat_table[i] == 0 &&
                   (fat_sector != priv->first_fat_sector || i > 2))
                {
                    fat_table[i] = 0x0FFFFFFF;
                    __sync_or_and_fetch(&blk->flags, PCACHE_FLAG_DIRTY);
                    priv->free_clusters--;
                    __asm__ __volatile__("":::"memory");
                    release_cached_page(blk);
                    return i + ((fat_sector - priv->first_fat_sector) * steps);
                }
            }
        }
        else if(priv->fattype == FAT_16)
        {
            uint16_t *fat_table = (uint16_t *)blk->virt;

            for(i = 0, steps = priv->blocksz / 2; i < steps; i++)
            {
                if(fat_table[i] == 0 &&
                   (fat_sector != priv->first_fat_sector || i > 2))
                {
                    fat_table[i] = 0xFFFF;
                    __sync_or_and_fetch(&blk->flags, PCACHE_FLAG_DIRTY);
                    priv->free_clusters--;
                    __asm__ __volatile__("":::"memory");
                    release_cached_page(blk);
                    return i + ((fat_sector - priv->first_fat_sector) * steps);
                }
            }
        }
        else
        {
            uint8_t *fat_table = (uint8_t *)blk->virt;

            // TODO: not sure if this gives the right offset for use in the
            //       calculations below
            steps = (priv->blocksz * 2) / 3;

            for(i = 0; i < priv->blocksz; i += 3)
            {
                // ignore the case where the entry is at sector boundary
                // TODO: although this wastes only a few entries, we should
                //       find a cleaner solution
                if(i == priv->blocksz - 1)
                {
                    break;
                }

                // check the low 12 bits
                if(fat_table[i] == 0 && (fat_table[i + 1] & 0x0F) == 0 &&
                   (fat_sector != priv->first_fat_sector || i > 3))
                {
                    fat_table[i] = 0xFF;
                    fat_table[i + 1] |= 0x0F;
                    __sync_or_and_fetch(&blk->flags, PCACHE_FLAG_DIRTY);
                    priv->free_clusters--;
                    __asm__ __volatile__("":::"memory");
                    release_cached_page(blk);
                    return ((i * 2) / 3) +
                            ((fat_sector - priv->first_fat_sector) * steps);
                }

                // check the high 12 bits
                if((fat_table[i] & 0xF0) == 0 && fat_table[i + 1] == 0 &&
                   (fat_sector != priv->first_fat_sector || i > 3))
                {
                    fat_table[i] |= 0xF0;
                    fat_table[i + 1] = 0xFF;
                    __sync_or_and_fetch(&blk->flags, PCACHE_FLAG_DIRTY);
                    priv->free_clusters--;
                    __asm__ __volatile__("":::"memory");
                    release_cached_page(blk);
                    return (((i * 2) / 3) + 1) +
                            ((fat_sector - priv->first_fat_sector) * steps);
                }
            }
        }

        release_cached_page(blk);
    }

    return 0;
}


static size_t __next_cluster(struct fat_private_t *priv, 
                             size_t cur_cluster, size_t next_cluster, int write)
{
    struct cached_page_t *blk;
    struct fs_node_header_t tmpnode;
    size_t fat_offset, fat_sector, ent_offset;
    uint8_t *fat_table;

    tmpnode.inode = PCACHE_NOINODE;
    tmpnode.dev = priv->dev;

    if(priv->fattype == FAT_32 || priv->fattype == FAT_EX)
    {
        uint32_t res;

        fat_offset = cur_cluster * 4;
        fat_sector = priv->first_fat_sector + (fat_offset / priv->blocksz);
        ent_offset = fat_offset % priv->blocksz;

        if(!(blk = get_cached_page((struct fs_node_t *)&tmpnode, fat_sector, 0)))
        {
            return 0;
        }

        fat_table = (uint8_t *)blk->virt;

        if(write)
        {
            *(uint32_t *)&fat_table[ent_offset] = (uint32_t)next_cluster;
            __sync_or_and_fetch(&blk->flags, PCACHE_FLAG_DIRTY);
        }

        res = *(uint32_t *)&fat_table[ent_offset];

        if(priv->fattype == FAT_32)
        {
            // FAT32 uses only 28 bits
            res &= 0x0FFFFFFF;
        }

        release_cached_page(blk);

        return res;
    }
    else if(priv->fattype == FAT_16)
    {
        uint16_t res;

        fat_offset = cur_cluster * 2;
        fat_sector = priv->first_fat_sector + (fat_offset / priv->blocksz);
        ent_offset = fat_offset % priv->blocksz;

        if(!(blk = get_cached_page((struct fs_node_t *)&tmpnode, fat_sector, 0)))
        {
            return 0;
        }

        fat_table = (uint8_t *)blk->virt;

        if(write)
        {
            *(uint16_t *)&fat_table[ent_offset] = (uint16_t)next_cluster;
            __sync_or_and_fetch(&blk->flags, PCACHE_FLAG_DIRTY);
        }

        res = *(uint16_t *)&fat_table[ent_offset];

        release_cached_page(blk);

        return res;
    }
    else
    {
        uint16_t low, hi;

        fat_offset = cur_cluster + (cur_cluster / 2);   // multiply by 1.5
        fat_sector = priv->first_fat_sector + (fat_offset / priv->blocksz);
        ent_offset = fat_offset % priv->blocksz;

        if(!(blk = get_cached_page((struct fs_node_t *)&tmpnode, fat_sector, 0)))
        {
            return 0;
        }

        fat_table = (uint8_t *)blk->virt;

        if(write)
        {
            // special case where the entry is at sector boundary
            // write the lower byte here and the upper byte below
            if(ent_offset == priv->blocksz - 1)
            {
                fat_table[ent_offset] = (uint8_t)(next_cluster & 0xFF);
            }
            else
            {
                *(uint16_t *)&fat_table[ent_offset] = (uint16_t)next_cluster;
            }

            __sync_or_and_fetch(&blk->flags, PCACHE_FLAG_DIRTY);
        }

        low = *(uint16_t *)&fat_table[ent_offset];
        release_cached_page(blk);

        // special case where the entry is at sector boundary
        // we have to read the next sector to get the upper half of the entry
        if(ent_offset == priv->blocksz - 1)
        {
            if(!(blk = get_cached_page((struct fs_node_t *)&tmpnode, ++fat_sector, 0)))
            {
                return 0;
            }

            fat_table = (uint8_t *)blk->virt;

            if(write)
            {
                // special case where the entry is at sector boundary
                // write the upper byte here and the lower byte above
                fat_table[0] &= ~0x0F;
                fat_table[0] |= (uint8_t)((next_cluster >> 8) & 0x0F);
                __sync_or_and_fetch(&blk->flags, PCACHE_FLAG_DIRTY);
            }

            hi = *(uint16_t *)&fat_table[0];
            low &= 0xFF;
            low |= (hi << 8);
            release_cached_page(blk);
        }

        low = (cur_cluster & 1) ? low >> 4 : low & 0xFFF;

        return low;
    }
}


static size_t get_next_cluster(struct fat_private_t *priv, size_t cur_cluster)
{
    return __next_cluster(priv, cur_cluster, 0, 0);
}


static size_t write_next_cluster(struct fat_private_t *priv, 
                                 size_t cur_cluster, size_t next_cluster)
{
    return __next_cluster(priv, cur_cluster, next_cluster, 1);
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
size_t fatfs_bmap(struct fs_node_t *node, size_t lblock,
                  size_t block_size, int flags)
{
    UNUSED(block_size);

    struct fat_private_t *priv;
    size_t cur_cluster, next_cluster;
    int create = (flags & BMAP_FLAG_CREATE);
    int free = (flags & BMAP_FLAG_FREE);

    //if(node->inode == 2) switch_tty(1);
    //printk("fatfs_bmap: dev 0x%x, ino 0x%x, flags 0x%x, lblock 0x%lx\n", node->dev, node->inode, flags, lblock);

    if(get_priv(node->dev, &priv) < 0)
    {
        return 0;
    }

    if(node->inode == FAT_ROOT_INODE)
    {
        // root dir in FAT12/16 is at a fixed position right after the FAT
        if(priv->fattype != FAT_32)
        {
            // we cannot shrink the root dir
            if(free)
            {
                return 0;
            }

            // we cannot expand the root beyond its reserved sectors
            if(create && lblock >= priv->root_dir_sectors)
            {
                return 0;
            }

            //printk("fatfs_bmap: first %ld, lblock %d\n", priv->first_root_dir_sector, lblock);
            return priv->first_root_dir_sector + lblock;
        }

        // root dir in FAT32 is in a cluster, treat it as any other cluster
        cur_cluster = priv->first_root_dir_cluster;
    }
    else
    {
        cur_cluster = node->inode;
    }

    //printk("fatfs_bmap: cur_cluster 0x%lx\n", cur_cluster);
    next_cluster = 0;

    if(lblock == 0)
    {
        // we only create the first sector when we alloc the inode
        // we also free the first sector when we free the inode
        if(free)
        {
            return 0;
        }

        return first_sector_of_cluster(priv, cur_cluster) + lblock;
    }

    while(lblock >= priv->sectors_per_cluster)
    {
        next_cluster = get_next_cluster(priv, cur_cluster);
        lblock -= priv->sectors_per_cluster;

        //printk("fatfs_bmap: loop -- cur_cluster 0x%lx, next_cluster 0x%lx, lblock 0x%lx\n", cur_cluster, next_cluster, lblock);

        if(next_cluster >= end_of_chain[priv->fattype])
        {
            break;
        }

        if(next_cluster == bad_cluster[priv->fattype] ||
           next_cluster < 2)
        {
            return 0;
        }

        cur_cluster = next_cluster;
    }

    //printk("fatfs_bmap: cur_cluster 0x%lx, next_cluster 0x%lx\n", cur_cluster, next_cluster);

    if(next_cluster >= end_of_chain[priv->fattype])
    {
        if(create && (next_cluster = alloc_cluster(priv)))
        {
            //printk("fatfs_bmap: create cur_cluster 0x%lx, next_cluster 0x%lx\n", cur_cluster, next_cluster);
            write_next_cluster(priv, cur_cluster, next_cluster);
            cur_cluster = next_cluster;
        }
        else
        {
            return 0;
        }
    }
    else if(free)
    {
        // only free the cluster if this is the first block in the cluster
        if(lblock == 0)
        {
            next_cluster = get_next_cluster(priv, cur_cluster);

            if(next_cluster < bad_cluster[priv->fattype] &&
               next_cluster >= 2)
            {
                write_next_cluster(priv, next_cluster, 0);
                priv->free_clusters++;
                __asm__ __volatile__("":::"memory");
            }

            write_next_cluster(priv, cur_cluster, end_of_chain[priv->fattype]);
        }

        return 0;
    }

    //if(node->inode == 2 && flags && cur_cluster != 2 && lblock > 0) for(;;);

    return first_sector_of_cluster(priv, cur_cluster) + lblock;
}


static void remove_dirent(struct fat_private_t *priv, 
                          struct fs_node_t *parent, size_t child_cluster)
{
    struct cached_page_t *dbuf;
    struct fat_dirent_t *dent;
    unsigned char *blk, *end;
    int lfn_entry_count;
    size_t lfn_stream_offset, lfn_block_offset;
    size_t first_cluster = 0, offset = 0;

    // We need to mark both the directory entry and its associated Long
    // File Name (LFN) entries as unused. The problem is the LFNs come before
    // their associated entry, so for each directory entry, we have to
    // remember the offset of the first LFN for that entry in the parent
    // directory, then compare the cluster number in the associated entry,
    // and if they match, we have found our entry. We then loop back to 
    // start marking the LFNs and then the entry as unused.
    // Comparing with cluster number (instead of name) can be problematic
    // if we have two entries in the same dir with the same cluster number,
    // which should only happen when we are renaming a file within its parent
    // directory. We always add the new file (see vfs_linkat()) before removing
    // the old one (see vfs_unlinkat()). This driver always adds new entries
    // to the end of the directory stream. This way, when we search for the
    // cluster number to delete, we always end up deleting the old file, which
    // is what we want.
    lfn_stream_offset = -1;
    lfn_block_offset = -1;
    lfn_entry_count = 0;

    while(offset < parent->size)
    {
        if(!(dbuf = get_cached_page(parent, offset, 0)))
        {
            offset += PAGE_SIZE;
            continue;
        }

        blk = (unsigned char *)dbuf->virt;
        end = (unsigned char *)(dbuf->virt + PAGE_SIZE);

        while(blk < end)
        {
            dent = (struct fat_dirent_t *)blk;

            // last entry
            if(blk[0] == 0)
            {
                lfn_stream_offset = -1;
                lfn_block_offset = -1;
                lfn_entry_count = 0;

                // break the loop
                offset = parent->size;
                break;
            }

            // unused entry
            if(blk[0] == 0xE5)
            {
                lfn_stream_offset = -1;
                lfn_block_offset = -1;
                lfn_entry_count = 0;
                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // Long File Name (LFN) entry
            if(dent->attribs == FAT_ATTRIB_LFN)
            {
                if(lfn_entry_count == 0)
                {
                    lfn_stream_offset = offset;
                    lfn_block_offset = (size_t)(blk - dbuf->virt);
                }

                lfn_entry_count++;
                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // normal 8.3 entry, check if this is our match
            if(cluster_from_dirent(priv, dent) == child_cluster)
            {
                // update times
                //node_to_dirent(dent, node);

                first_cluster = cluster_from_dirent(priv, dent);

                if(child_cluster != first_cluster)
                {
                    kpanic("vfat: trying to free a node with inode != first_cluster\n");
                }

                // mark it as unused and break the loop
                dent->filename[0] = 0xE5;
                __sync_or_and_fetch(&dbuf->flags, PCACHE_FLAG_DIRTY);
                offset = parent->size;
                break;
            }

            // no match, ignore the saved LFN, if any
            lfn_stream_offset = -1;
            lfn_block_offset = -1;
            lfn_entry_count = 0;
            blk += FAT_DIRENT_SIZE;
        }
        
        release_cached_page(dbuf);
        offset += PAGE_SIZE;
    }

    // If the entry had LFNs associated with it, loop back to mark them 
    // as unused.
    if(lfn_entry_count > 0)
    {
        offset = lfn_stream_offset;

        while(offset < parent->size)
        {
            if(!(dbuf = get_cached_page(parent, offset, 0)))
            {
                offset += PAGE_SIZE;
                continue;
            }

            blk = (unsigned char *)dbuf->virt;
            end = (unsigned char *)(dbuf->virt + PAGE_SIZE);

            while(blk < end)
            {
                if(offset == lfn_stream_offset &&
                   lfn_block_offset > (size_t)(blk - dbuf->virt))
                {
                    blk += FAT_DIRENT_SIZE;
                    continue;
                }

                dent = (struct fat_dirent_t *)blk;

                // last entry
                if(blk[0] == 0)
                {
                    // break the loop
                    offset = parent->size;
                    break;
                }

                // unused entry
                if(blk[0] == 0xE5)
                {
                    // break the loop
                    // TODO: this should be an error as it means someone
                    //       else intervened between the two loops and the
                    //       directory is not possibly corrupt.
                    offset = parent->size;
                    break;
                }

                // Long File Name (LFN) entry
                if(dent->attribs == FAT_ATTRIB_LFN)
                {
                    // mark it as unused
                    dent->filename[0] = 0xE5;

                    if(--lfn_entry_count == 0)
                    {
                        offset = parent->size;
                        break;
                    }

                    blk += FAT_DIRENT_SIZE;
                    continue;
                }

                // normal 8.3 entry
                // TODO: again, this is an error as in the comment above
                offset = parent->size;
                break;
            }

            __sync_or_and_fetch(&dbuf->flags, PCACHE_FLAG_DIRTY);
            release_cached_page(dbuf);
            offset += PAGE_SIZE;
        }
    }
}


/*
 * Free an inode and update inode bitmap on disk.
 *
 * MUST write the node to disk if the filesystem supports inode structures
 * separate to their directory entries (e.g. ext2, tmpfs).
 */
long fatfs_free_inode(struct fs_node_t *node)
{
    //size_t dbuf_off, unused;
    //char *lfn = NULL;
    struct fat_private_t *priv;
    struct fs_node_t *parent;
    size_t first_cluster = 0;

    if(get_priv(node->dev, &priv) < 0)
    {
        return -EINVAL;
    }
    
    if(node->inode == FAT_ROOT_INODE)
    {
        /*
         * TODO: what error is appropriate here?
         */
        return -EPERM;
    }

    // other nodes (not root)
    if((parent = get_parent_node(priv, node)) == NULL)
    {
        return -ENOENT;
    }

    first_cluster = node->inode;
    remove_dirent(priv, parent, node->inode);
    release_node(parent);

    // mark the first cluster as unused
    // TODO: should we check if the cluster is correctly marked as end of 
    //       cluster, and if not raise an error?
    if(first_cluster)
    {
        write_next_cluster(priv, first_cluster, 0);
        priv->free_clusters++;
        __asm__ __volatile__("":::"memory");
    }

    remove_cacheent(priv, first_cluster);

    return 0;


#if 0
    if(fat_get_dirent(parent, NULL, node->inode, &dbuf, &dbuf_off, &lfn, &unused) != 0)
    {
        release_node(parent);
        return -ENOENT;
    }

    release_node(parent);
    dent = (struct fat_dirent_t *)(dbuf->virt + dbuf_off);

    if(node->size != 0)
    {
        kpanic("vfat: trying to free a node with size != 0\n");
    }

    // update times
    node_to_dirent(dent, node);

    first_cluster = cluster_from_dirent(priv, dent);

    if(node->inode != first_cluster)
    {
        kpanic("vfat: trying to free a node with inode != first_cluster\n");
    }

    // mark it deleted
    // TODO: we should also mark the corresponding LFN entries, if any?
    dent->filename[0] = 0xE5;
    //dent->first_cluster_hi = 0;
    //dent->first_cluster_lo = 0;

    __sync_or_and_fetch(&dbuf->flags, PCACHE_FLAG_DIRTY);
    release_cached_page(dbuf);

    // mark the first cluster as unused
    // TODO: should we check if the cluster is correctly marked as end of 
    //       cluster, and if not raise an error?
    if(first_cluster)
    {
        write_next_cluster(priv, first_cluster, 0);
        priv->free_clusters++;
        __asm__ __volatile__("":::"memory");
    }

    if(lfn)
    {
        kfree(lfn);
    }

    remove_cacheent(priv, first_cluster);

    return 0;
#endif
}


/*
 * Allocate a new inode number and mark it as used in the disk's inode bitmap.
 */
long fatfs_alloc_inode(struct fs_node_t *new_node)
{
    struct fat_private_t *priv;
    size_t first_cluster;

    if(get_priv(new_node->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    // FAT does not have the notion of an inode. We cheat by allocating a
    // new cluster and use it as the inode number, as these are guaranteed
    // to be >= 2 (clusters 0 and 1 are reserved). We do the reverse when
    // freeing an inode, where we free the cluster we allocated here.
    if(!(first_cluster = alloc_cluster(priv)))
    {
        return -ENOSPC;
    }

    new_node->inode = first_cluster;
    A_memset(new_node->blocks, 0, sizeof(new_node->blocks));

    return 0;
}


/*
 * Helper function to convert a disk directory entry to a dirent struct.
 *
 * Inputs:
 *    fatent => the directory entry on disk
 *    __ent => dirent struct to fill (if null, we'll alloc a new struct)
 *    name => the entry's name (filename)
 *    namelen => name's length
 *    off => the value to store in dirent's d_off field
 *
 * Returns:
 *    a kmalloc'd dirent struct on success, NULL on failure
 */
static struct dirent *fatfs_entry_to_dirent(struct fat_private_t *priv,
                                            struct fat_dirent_t *fatent,
                                            struct dirent *__ent,
                                            char *name, size_t namelen, int off)
{
    unsigned int reclen = GET_DIRENT_LEN(namelen);
    unsigned char d_type = DT_REG;

    struct dirent *entry = __ent ? __ent : kmalloc(reclen);

    if(!entry)
    {
        return NULL;
    }

    if(fatent->attribs & FAT_ATTRIB_DIRECTORY)
    {
        d_type = DT_DIR;
    }
    else if(fatent->attribs & FAT_ATTRIB_VOLUMEID)
    {
        d_type = DT_UNKNOWN;
    }

    entry->d_reclen = reclen;
    // use the first cluster as the inode number
    entry->d_ino = cluster_from_dirent(priv, fatent);
    entry->d_off = off;
    entry->d_type = d_type;

    // on FAT12/16, entries referring to the root directory have a first 
    // cluster of 0, which we need to change to refer to the root directory
    // inode number
    if(d_type == DT_DIR && entry->d_ino == 0 &&
       (priv->fattype == FAT_12 || priv->fattype == FAT_16))
    {
        entry->d_ino = FAT_ROOT_INODE;
    }
    
    // name might not be null-terminated
    // fat_get_dirent() ensures we don't get more than NAME_MAX bytes, 
    // currently defined as 255, which should fit here
    A_memcpy(entry->d_name, name, namelen);
    entry->d_name[namelen] = '\0';
    
    return entry;
}

/*
 * Root directory in FAT filesystems do not have '.' and '..' entries,
 * which are essential for us when traversing directory tress to create
 * pathnames. Here we create fake '.' and '..' entries for those who
 * need them.
 * The 'name' argument MUST be '.' or '..' only, nothing else.
 */
static struct dirent *create_root_dirent(char *name)
{
    unsigned int reclen = GET_DIRENT_LEN(4);

    struct dirent *entry = kmalloc(reclen);

    if(!entry)
    {
        return NULL;
    }

    entry->d_ino = FAT_ROOT_INODE;
    entry->d_off = 0;
    entry->d_type = DT_DIR;
    entry->d_reclen = reclen;

    entry->d_name[0] = name[0];
    entry->d_name[1] = name[1];

    if(name[1] == '.')
    {
        entry->d_name[2] = name[2];
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
 *             dirent struct (by calling ext2_entry_to_dirent() above), and the
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
long fatfs_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                   struct cached_page_t **dbuf, size_t *dbuf_off)
{
    size_t stream_off;
    char *lfn = NULL;
    struct fat_dirent_t *dent;
    struct fat_private_t *priv;
    size_t fnamelen = strlen(filename);

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    if(!fnamelen)
    {
        return -EINVAL;
    }

    if(fnamelen > NAME_MAX)
    {
        return -ENAMETOOLONG;
    }

    // special handling for root directory's dot and dot-dot as they don't 
    // exist and we have to create fake entries
    if(dir->inode == FAT_ROOT_INODE &&
       (filename[0] == '.' &&
        (filename[1] == '\0' ||
         (filename[1] == '.' && filename[2] == '\0'))))
    {
        *entry = create_root_dirent(filename);
        return *entry ? 0 : -ENOMEM;
    }

    if(get_priv(dir->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    KDEBUG("fatfs_finddir: dev 0x%x, ino 0x%x, file '%s'\n", dir->dev, dir->inode, filename);

    if(fat_get_dirent(dir, filename, 0, dbuf, dbuf_off, &lfn, &stream_off) != 0)
    {
        KDEBUG("fatfs_finddir: fat_get_dirent res != 0\n");
        return -ENOENT;
    }

    dent = (struct fat_dirent_t *)((*dbuf)->virt + (*dbuf_off));

    if(lfn)
    {
        *entry = fatfs_entry_to_dirent(priv, dent, NULL, lfn, strlen(lfn),
                                                stream_off + (*dbuf_off));
        kfree(lfn);
    }
    else
    {
        *entry = fatfs_entry_to_dirent(priv, dent, NULL, filename, fnamelen,
                                                stream_off + (*dbuf_off));
    }

    return *entry ? 0 : -ENOMEM;
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
long fatfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                            struct dirent **entry,
                            struct cached_page_t **dbuf, size_t *dbuf_off)
{
    size_t stream_off;
    char *lfn = NULL;
    char namebuf[16];
    struct fat_dirent_t *dent;
    struct fat_private_t *priv;

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    if(get_priv(node->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    // the inode number is the first cluster
    if(fat_get_dirent(dir, NULL, node->inode, dbuf, dbuf_off, &lfn, &stream_off) != 0)
    {
        return -ENOENT;
    }

    dent = (struct fat_dirent_t *)((*dbuf)->virt + (*dbuf_off));

    if(lfn)
    {
        *entry = fatfs_entry_to_dirent(priv, dent, NULL, lfn, strlen(lfn),
                                                stream_off + (*dbuf_off));
        kfree(lfn);
    }
    else
    {
        // if no LFN, convert the short name and use it
        dos_to_unix_name(namebuf, (char *)dent);
        *entry = fatfs_entry_to_dirent(priv, dent, NULL, namebuf, strlen(namebuf),
                                                stream_off + (*dbuf_off));
    }

    return *entry ? 0 : -ENOMEM;
}


static void cancel_alloced_entries(struct fs_node_t *dir, int __count,
                                   size_t *entry_stream_offset, size_t *entry_buf_offset)
{
    struct cached_page_t *buf;
    unsigned char *blk, *end;
    size_t offset = 0;
    volatile int count = __count;

    // roll back everything
    for(count = 0, offset = *entry_stream_offset; count < __count; )
    {
        if(!(buf = get_cached_page(dir, offset, 0 /* PCACHE_AUTO_ALLOC */)))
        {
            return;
        }
        
        blk = (unsigned char *)buf->virt;
        end = blk + PAGE_SIZE;
        
        while(blk < end)
        {
            // get to the first entry we marked
            if(offset == *entry_stream_offset && 
                    (size_t)(blk - buf->virt) < *entry_buf_offset)
            {
                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // we reached the last entry -- abort
            if(blk[0] == 0)
            {
                count = __count;
                break;
            }

            blk[0] = 0;
            count++;
            blk += FAT_DIRENT_SIZE;
        }

        __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
        release_cached_page(buf);
        offset += PAGE_SIZE;
    }
}


static int alloc_direntries(struct fat_private_t *priv, struct fs_node_t *dir, 
                            int __count, 
                            size_t *entry_stream_offset, size_t *entry_buf_offset)
{
    struct cached_page_t *buf;
    unsigned char *blk, *end;
    size_t offset = 0;
    int found = 0, res = 0;
    volatile int count = __count;

    *entry_stream_offset = 0;
    *entry_buf_offset = 0;

    while(1)
    {
        // cannot increase the size of the root directory on FAT12/16 as
        // it is fixed before data clusters
        if(dir->inode == FAT_ROOT_INODE && offset >= dir->size &&
           (priv->fattype == FAT_12 || priv->fattype == FAT_16))
        {
            res = -ENOSPC;
            goto err;
        }

        if(!(buf = get_cached_page(dir, offset, 0 /* PCACHE_AUTO_ALLOC */)))
        {
            res = -EIO;
            goto err;
        }
        
        blk = (unsigned char *)buf->virt;
        end = blk + PAGE_SIZE;
        
        while(blk < end)
        {
            if(found)
            {
                if(count == 0)
                {
                    // ensure an end of directory entry is there
                    blk[0] = 0;
                    __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
                    release_cached_page(buf);
                    return 0;
                }

                blk[0] = 0xE5;
                __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
                count--;
                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // look for the last entry
            // TODO: this ignores unused entries, which we should make use of!
            if(blk[0] != 0)
            {
                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // now see if there is enough space and mark entries as unused
            // for now, so no one uses them until fatfs_addir() fills them in
            *entry_stream_offset = offset;
            *entry_buf_offset = (size_t)(blk - buf->virt);
            found = 1;

            // loop back to mark this entry
        }

        release_cached_page(buf);
        offset += PAGE_SIZE;
    }

err:

    if(!found)
    {
        return res;
    }

    cancel_alloced_entries(dir, __count, entry_stream_offset, entry_buf_offset);

    return res;
}


STATIC_INLINE void convert_to_uppercase(char *s)
{
    while(*s)
    {
        if(*s >= 'a' && *s <= 'z')
        {
            *s = (*s) - ('a' - 'A');
        }

        s++;
    }
}


STATIC_INLINE void pad_with_spaces(char *s, int len)
{
    int i;

    for(i = 0; i < len; i++)
    {
        if(s[i] == '\0')
        {
            s[i] = ' ';
            s[i + 1] = '\0';
        }
    }
}


/*
 * Convert a Long File Name (LFN) to a short file name.
 * This is based on Section 3.2.4 Generating 8.3 names from long names
 * from Microsoft's specification:
 *    http://www.osdever.net/documents/LongFileName.pdf
 *
 * The resultant short name will be an 8.3 name, with the name part being
 * truncated to 6 chars at most. The optional extension will be placed at
 * offset 8 onwards. Both the name and the extension will be converted to
 * uppercase letters and NULL-terminated, so we can later add numeric
 * suffixes and check for name collisions.
 */
static int prep_short_name(char *lfn, size_t lfnlen, char **res)
{
    char *sfn, ext[4];
    volatile char *tmp, *s, *d;
    volatile size_t dlen = 0;

    *res = NULL;

    // malloc'd at least 12 bytes (8.3 name + NULL byte) as we will reuse
    // this buffer later
    lfnlen = (lfnlen < 11) ? 11 : lfnlen;

    if(!(sfn = kmalloc(lfnlen + 1)))
    {
        return -ENOMEM;
    }

    s = lfn;
    d = sfn;

    // 1 - Remove all spaces
    while(*s)
    {
        if(*s != ' ')
        {
            *d = *s;
            d++;
            dlen++;
        }

        s++;
    }

    if(dlen == 0)
    {
        kfree(sfn);
        return -EINVAL;
    }

    *d = '\0';
    d = sfn;

    // 2.a - Remove trailing periods
    while(d[dlen - 1] == '.')
    {
        d[dlen - 1] = '\0';

        if(--dlen == 0)
        {
            kfree(sfn);
            return -EINVAL;
        }
    }

    // 2.b - Remove leading periods
    while(*d == '.')
    {
        for(tmp = d; tmp < d + dlen; tmp++)
        {
            tmp[0] = tmp[1];
        }

        dlen--;
    }

    if(dlen == 0)
    {
        kfree(sfn);
        return -EINVAL;
    }

    // 2.c - Remove extra periods before the last period
    // first find the trailing period (and store the optional extension)
    ext[0] = '\0';
    ext[1] = '\0';
    ext[2] = '\0';
    ext[3] = '\0';

    for(tmp = d + dlen - 1; tmp > d; tmp--)
    {
        if(*tmp == '.')
        {
            if(tmp[1])
            {
                ext[0] = tmp[1];

                if(tmp[2])
                {
                    ext[1] = tmp[2];

                    if(tmp[3])
                    {
                        ext[2] = tmp[3];
                    }
                }
            }

            break;
        }
    }

    // we found a trailing period
    if(tmp > d)
    {
        d = tmp - 1;

        while(d > sfn)
        {
            if(*d == '.')
            {
                for(tmp = d; tmp < d + dlen; tmp++)
                {
                    tmp[0] = tmp[1];
                }

                dlen--;
            }

            d--;
        }
    }

    if(dlen == 0)
    {
        kfree(sfn);
        return -EINVAL;
    }

    // 3 - Translate illegal 8.3 chars to '_'
    for(tmp = sfn; tmp < sfn + dlen; tmp++)
    {
        if(!VALID_8_3_CHAR(*tmp))
        {
            *tmp = '_';
        }
    }

    // 4 - Truncate to 6 chars (excluding extension)
    if(dlen > 6)
    {
        sfn[6] = '\0';
        //dlen = 6;
    }
    else
    {
        sfn[dlen] = '\0';
    }

    // convert to uppercase
    convert_to_uppercase(sfn);
    convert_to_uppercase(ext);

    // and add the extension, if any
    sfn[8] = ext[0];
    sfn[9] = ext[1];
    sfn[10] = ext[2];
    sfn[11] = '\0';

    /*
    // convert to uppercase and pad with spaces as needed
    pad_with_spaces(sfn, 8);
    pad_with_spaces(ext, 3);

    // and add the extension, if any
    sfn[8] = ext[0];
    sfn[9] = ext[1];
    sfn[10] = ext[2];
    sfn[11] = '\0';

    *preflen = dlen;
    sfn[dlen++] = '-';
    sfn[dlen++] = '1';

    if(ext[0])
    {
        sfn[dlen++] = '.';
        sfn[dlen++] = ext[0];
        sfn[dlen++] = ext[1];
        sfn[dlen++] = ext[2];
    }

    sfn[dlen++] = '\0';
    */

    *res = sfn;
    return 0;
}


/*
 * Take the prefix we created in prep_short_name() above, the '-', the numeric
 * suffix, and the optional extension and concatenate them together to form a
 * proper filename we can use to search for directory entries.
 */
static void build_short_name(char *namebuf, char *short_name, int prefixlen, int suffix)
{
    // count the digits and the leading '-'
    int suffixlen = (suffix > 99) ? 4 : ((suffix > 9) ? 3 : 2);
    // ensure the name prefix and the '-' and digital suffix do no exceed 8
    int actual_prefixlen = (prefixlen <= (8 - suffixlen)) ? prefixlen : (8 - suffixlen);
    int i;

    for(i = 0; i < actual_prefixlen; i++)
    {
        namebuf[i] = short_name[i];
    }

    namebuf[i++] = '-';

    if(suffix >= 100)
    {
        namebuf[i++] = (suffix % 100) + '0';
        suffix /= 100;
    }

    if(suffix >= 10)
    {
        namebuf[i++] = (suffix % 10) + '0';
        suffix /= 10;
    }

    namebuf[i++] = suffix + '0';

    // the optional extension was stored by prep_short_name() at offset 8
    if(short_name[8])
    {
        namebuf[i++] = '.';
        namebuf[i++] = short_name[8];
        namebuf[i++] = short_name[9];
        namebuf[i++] = short_name[10];
    }

    namebuf[i] = '\0';
}


/*
 * Convert a name like 'file.txt' to the proper 8.3 format like:
 *    'FILE    TXT'
 * so it can be written out to disk.
 */
static void finalise_short_name(char *finalname, char *namebuf)
{
    int i, j;

    for(i = 0; i < 11; i++)
    {
        finalname[i] = ' ';
    }

    // copy the name and extension, which are uppercased for
    // us, but we need to account for space padding
    for(i = 0; i < 8; i++)
    {
        // stop if we reach the end or the file extension
        if(namebuf[i] == '\0' || namebuf[i] == '.')
        {
            break;
        }

        finalname[i] = namebuf[i];
    }

    if(namebuf[i] == '.')
    {
        i++;
    }

    // yes -- this loop says j and i, because we want to continue
    // adding extension chars at position 8 in the entry, but we
    // also want to carry on reading from wherever we stopped at
    // in the name buffer (we check i against 12 due to the extra
    // dot in our namebuf)
    for(j = 8; i < 12; i++, j++)
    {
        // stop if we reach the end
        if(namebuf[i] == '\0')
        {
            break;
        }

        finalname[j] = namebuf[i];
    }
}


/*
 * TODO: this function does not handle UTF-8 and assumes all chars are
 *       ASCII or similar.
 */
STATIC_INLINE size_t calc_needed_direntries(size_t lfnlen)
{
    // add one to account for the short name entry
    return 1 + ((lfnlen + (CHARS_PER_LFN_ENTRY - 1)) / CHARS_PER_LFN_ENTRY);
}


static unsigned char calc_short_name_checksum(char *__buf)
{
    unsigned char *buf = (unsigned char *)__buf;
    unsigned char checksum = buf[0];
    int i;

    for(i = 1; i < 11; i++)
    {
        checksum = (checksum << 7) + (checksum >> 1) + buf[i];
    }

    return checksum;
}


/*
 * Add the given file as an entry in the given parent directory.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    file => contains the new file's inode number
 *    filename => the new file's name
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long fatfs_addir(struct fs_node_t *dir, struct fs_node_t *file, char *filename)
{
    size_t offset = 0;
    size_t fnamelen = strlen(filename);
    size_t needed_entries, count;
    size_t dbuf_off, unused;
    struct fat_private_t *priv;
    struct cached_page_t *dbuf;
    struct fat_dirent_t *ent;
    unsigned char *blk, *end;
    char *lfn = NULL;
    char *short_name;
    char namebuf[16], *tmp;
    size_t entry_stream_offset, entry_buf_offset;
    int res, suffix, prefixlen, null_written;
    int i, done;
    unsigned char short_name_checksum;

    if(!fnamelen)
    {
        return -EINVAL;
    }

    if(fnamelen > NAME_MAX)
    {
        return -ENAMETOOLONG;
    }

    if(get_priv(dir->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    printk("fatfs_addir: filename '%s'\n", filename);

    if((res = prep_short_name(filename, fnamelen, &short_name)) < 0)
    {
        printk("fatfs_addir: prep_short_name res %d\n", res);
        return res;
    }

    printk("fatfs_addir: short_name '%s'\n", short_name);

    // this will be at least 2 (one for the LFN and one for the short name)
    needed_entries = calc_needed_direntries(fnamelen);

    printk("fatfs_addir: needed_entries %ld\n", needed_entries);

    // make sure no one else makes changes to the dir structure
    MARK_NODE_STALE(dir);

    if((res = alloc_direntries(priv, dir, needed_entries,
                               &entry_stream_offset, &entry_buf_offset)) < 0)
    {
        UNMARK_NODE_STALE(dir);
        kfree(short_name);
        printk("fatfs_addir: alloc_direntries res %d\n", res);
        return res;
    }

    prefixlen = strlen(short_name);

    // try and find a suitable short name
    for(suffix = 1; suffix < 256; suffix++)
    {
        build_short_name(namebuf, short_name, prefixlen, suffix);

        // TODO: instead of re-reading disk buffers in every loop iteration,
        //       we better cache all the short names and then compare our
        //       short name to them until we find an unused one
        if(fat_get_dirent(dir, namebuf, 0, &dbuf, &dbuf_off, &lfn, &unused) != 0)
        {
            break;
        }

        release_cached_page(dbuf);

        if(lfn)
        {
            kfree(lfn);
        }
    }

    printk("fatfs_addir: suffix %d\n", suffix);

    if(suffix == 256)
    {
        cancel_alloced_entries(dir, needed_entries, &entry_stream_offset, &entry_buf_offset);
        UNMARK_NODE_STALE(dir);
        kfree(short_name);
        return -EINVAL;
    }

    // Convert the Unix-like short name we created above, to a proper 8.3
    // name. We reuse the short_name string as we will not use it again for
    // anything else, so might as well make use of the alloc'd memory.
    // We also need to do this now so we can calculate the short name checksum
    // that we need to store in the LFN entries.
    finalise_short_name(short_name, namebuf);
    short_name_checksum = calc_short_name_checksum(short_name);

    for(count = needed_entries, offset = entry_stream_offset; count > 0; )
    {
        if(!(dbuf = get_cached_page(dir, offset, 0 /* PCACHE_AUTO_ALLOC */)))
        {
            break;
        }
        
        blk = (unsigned char *)dbuf->virt;
        end = blk + PAGE_SIZE;
        done = 0;
        
        while(blk < end)
        {
            // get to the first entry we marked
            if(offset == entry_stream_offset && 
                    (size_t)(blk - dbuf->virt) < entry_buf_offset)
            {
                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // something is wrong
            if(blk[0] != 0xE5)
            {
                done = 1;
                break;
            }

            if(count == 1)
            {
                // this is the last entry, make it an 8.3 one
                ent = (struct fat_dirent_t *)blk;

                // update times
                node_to_dirent(ent, file);

                ent->first_cluster_hi = 
                        (priv->fattype == FAT_32) ?
                                (file->inode >> 16) : 0;
                ent->first_cluster_lo = (file->inode & 0xFFFF);
                ent->reserved = 0;
                // directory size is 0 on FAT
                ent->size = S_ISDIR(file->mode) ? 0 : file->size;

                // set attribs
                ent->attribs = 0;

                if(S_ISDIR(file->mode))
                {
                    ent->attribs |= FAT_ATTRIB_DIRECTORY;
                }

                if(!(file->mode & (S_IWUSR|S_IWGRP|S_IWOTH)))
                {
                    ent->attribs |= FAT_ATTRIB_READONLY;
                }

                if(filename[0] == '.')
                {
                    ent->attribs |= FAT_ATTRIB_HIDDEN;
                }

                for(i = 0; i < 11; i++)
                {
                    ent->filename[i] = short_name[i];
                }

                // make it zero so we exit the outer loop
                count--;
                break;
            }

            // this is one of the LFN entries
            blk[0] = (count - 1) | (count == needed_entries ? 0x40 : 0);
            blk[11] = FAT_ATTRIB_LFN;
            blk[12] = '\0';
            blk[13] = short_name_checksum;
            blk[26] = '\0';
            blk[27] = '\0';

            tmp = filename + ((count - 2) * CHARS_PER_LFN_ENTRY);
            null_written = 0;

            for(i = 0; i < CHARS_PER_LFN_ENTRY; i++)
            {
                // if we reached the end of the string, pad with NULL,
                // otherwise copy chars
                if(*tmp)
                {
                    /*
                     * TODO: again, this assumes ASCII and not really
                     *       converting to UTF-16.
                     */
                    blk[lfn_char_offsets[i]] = *tmp;
                    blk[lfn_char_offsets[i] + 1] = '\0';
                    tmp++;
                }
                else
                {
                    blk[lfn_char_offsets[i]] = null_written ? 0xFF : 0x00;
                    blk[lfn_char_offsets[i] + 1] = '\0';
                    null_written = 1;
                }
            }

            count--;
            blk += FAT_DIRENT_SIZE;
        }

        __sync_or_and_fetch(&dbuf->flags, PCACHE_FLAG_DIRTY);
        release_cached_page(dbuf);
        offset += PAGE_SIZE;

        if(done)
        {
            break;
        }
    }

    printk("fatfs_addir: count after loop %ld\n", count);

    // check if the loop was terminated prematurely
    if(count != 0)
    {
        cancel_alloced_entries(dir, needed_entries, &entry_stream_offset, &entry_buf_offset);
        UNMARK_NODE_STALE(dir);
        kfree(short_name);
        return -EINVAL;
    }

    dir->mtime = now();
    dir->flags |= FS_NODE_DIRTY;
    
    if(offset + PAGE_SIZE >= dir->size)
    {
        dir->size = offset + PAGE_SIZE;
        dir->ctime = dir->mtime;
    }

    UNMARK_NODE_STALE(dir);
    kfree(short_name);
    add_cacheent(priv, file->inode, dir->inode);

    printk("fatfs_addir: done\n");

    return 0;
}


/*
 * Make a new, empty directory by allocating a free block and initializing
 * the '.' and '..' entries to point to the current and parent directory
 * inodes, respectively.
 *
 * Input:
 *    parent => the parent directory
 *    dir => node struct containing the new directory's inode number
 *
 * Output:
 *    dir => the directory's link count and block[0] will be updated
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long fatfs_mkdir(struct fs_node_t *dir, struct fs_node_t *parent)
{
    struct cached_page_t *buf;
    struct fat_private_t *priv;
    volatile struct fat_dirent_t *ent;
    volatile int i;

    if(get_priv(dir->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    dir->flags |= FS_NODE_DIRTY;
    dir->size = PAGE_SIZE;
    
    if(!(buf = get_cached_page(dir, 0, 0 /* PCACHE_AUTO_ALLOC */)))
    {
        dir->ctime = now();
        dir->flags |= FS_NODE_DIRTY;
        return -ENOSPC;
    }

    // create dot '.'
    ent = (struct fat_dirent_t *)buf->virt;

    // update times
    node_to_dirent(ent, dir);

    ent->first_cluster_hi = (priv->fattype == FAT_32) ? (dir->inode >> 16) : 0;
    ent->first_cluster_lo = (dir->inode & 0xFFFF);
    ent->reserved = 0;
    // directory size is 0 on FAT
    ent->size = 0; // dir->size;

    // set attribs
    ent->attribs = FAT_ATTRIB_DIRECTORY | FAT_ATTRIB_HIDDEN;
    ent->filename[0] = '.';

    for(i = 1; i < 11; i++)
    {
        ent->filename[i] = ' ';
    }

    // create dot-dot '..'
    ent = (struct fat_dirent_t *)(buf->virt + FAT_DIRENT_SIZE);

    // update times
    node_to_dirent(ent, parent);

    if(parent->inode == FAT_ROOT_INODE)
    {
        if(priv->fattype == FAT_32)
        {
            ent->first_cluster_hi = (priv->first_root_dir_cluster >> 16);
            ent->first_cluster_lo = (priv->first_root_dir_cluster & 0xFFFF);
        }
        else
        {
            // for Fat12/16, the root dir has no cluster number
            ent->first_cluster_hi = 0;
            ent->first_cluster_lo = 0;
        }
    }
    else
    {
        ent->first_cluster_hi = (priv->fattype == FAT_32) ? (parent->inode >> 16) : 0;
        ent->first_cluster_lo = (parent->inode & 0xFFFF);
    }

    ent->reserved = 0;
    // directory size is 0 on FAT
    ent->size = 0; // parent->size;

    // set attribs
    ent->attribs = FAT_ATTRIB_DIRECTORY | FAT_ATTRIB_HIDDEN;
    ent->filename[0] = '.';
    ent->filename[1] = '.';

    for(i = 2; i < 11; i++)
    {
        ent->filename[i] = ' ';
    }

    // create an end of dir entry
    ent = (struct fat_dirent_t *)(buf->virt + (FAT_DIRENT_SIZE * 2));
    A_memset((void *)ent, 0, sizeof(struct fat_dirent_t));

    __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
    release_cached_page(buf);

    return 0;
}


/*
 * Remove an entry from the given parent directory.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    entry => the entry to be removed
 *    is_dir => non-zero if entry is a directory and this is the last hard
 *              link, i.e. there is no other filename referring to the 
 *              directory's inode
 *
 * Output:
 *    the caller is responsible for writing dbuf to disk and calling brelse
 *
 * Returns:
 *    0 always
 */
long fatfs_deldir(struct fs_node_t *dir, struct dirent *entry, int is_dir)
{
    UNUSED(is_dir);

    struct fat_private_t *priv;

    /*
    UNUSED(dir);
    UNUSED(entry);

    unsigned char *blk = (unsigned char *)dbuf->virt;
    volatile struct fat_dirent_t *ent = (volatile struct fat_dirent_t *)(blk + dbuf_off);
    ent->filename[0] = 0xE5;
    __sync_or_and_fetch(&dbuf->flags, PCACHE_FLAG_DIRTY);
    */

    if(get_priv(dir->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    remove_dirent(priv, dir, entry->d_ino);

    return 0;
}


/*
 * Check if the given directory is empty (called from rmdir).
 *
 * Input:
 *    dir => the directory's node
 *
 * Returns:
 *    1 if dir is empty, 0 if it is not
 */
long fatfs_dir_empty(struct fs_node_t *dir)
{
    unsigned char *blk, *end;
    struct cached_page_t *buf;
    struct fat_dirent_t *ent;
    size_t offset;

    if(!dir->size || !(buf = get_cached_page(dir, 0, 0)))
    {
        // not ideal, but treat this as an empty directory
        printk("vfat: bad directory inode at 0x%x:0x%x\n",
               dir->dev, dir->inode);
        return 1;
    }

    // check for a completely empty directory with no entries
    ent = (struct fat_dirent_t *)buf->virt;

    if(ent->filename[0] == 0)
    {
        // not ideal, but treat this as an empty directory
        release_cached_page(buf);
        return 1;
    }

    blk = (unsigned char *)buf->virt;
    end = (unsigned char *)buf->virt + PAGE_SIZE;
    offset = 0;

    while(offset < dir->size)
    {
        while(blk < end)
        {
            ent = (struct fat_dirent_t *)blk;

            if(ent->filename[0] == 0)
            {
                break;
            }

            if(ent->filename[0] != 0xE5 && ent->attribs != FAT_ATTRIB_LFN)
            {
                // skip dot and dot-dot
                if(ent->filename[0] == '.' &&
                   (ent->filename[1] == ' ' || ent->filename[1] == '.'))
                {
                    blk += FAT_DIRENT_SIZE;
                    continue;
                }

                release_cached_page(buf);
                return 0;
            }

            blk += FAT_DIRENT_SIZE;
        }

        release_cached_page(buf);
        buf = NULL;
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

    if(buf)
    {
        release_cached_page(buf);
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
long fatfs_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz)
{
    size_t i, count = 0;
    size_t reclen, namelen;
    struct fat_private_t *priv;
    struct cached_page_t *dbuf = NULL;
    struct dirent *dent = NULL;
    struct fat_dirent_t *ent;
    char *b = (char *)buf;
    unsigned char *blk, *end;
    size_t offset;
    char *lfn = NULL;
    uint16_t *lfn_buf = NULL;
    size_t lfn_len = 0;
    int ignore_lfn = 0;

    if(!dir || !pos || !buf)
    {
        return -EINVAL;
    }

    if(get_priv(dir->dev, &priv) < 0)
    {
        return -EINVAL;
    }

    if((lfn_buf = kmalloc((NAME_MAX * 2) + 4)))
    {
        A_memset(lfn_buf, 0, (NAME_MAX * 2) + 4);
    }

    //switch_tty(1);
    offset = (*pos) & ~(PAGE_SIZE - 1);
    i = (*pos) % PAGE_SIZE;
    
    while(offset < dir->size)
    {
        KDEBUG("fatfs_getdents: 0 offset 0x%x, dir->size 0x%x\n", offset, dir->size);

        if(!(dbuf = get_cached_page(dir, offset, 0)))
        {
            offset += PAGE_SIZE;
            continue;
        }
        
        blk = (unsigned char *)(dbuf->virt + i);
        end = (unsigned char *)(dbuf->virt + PAGE_SIZE);
        
        // we use i only for the first round, as we might have been asked to
        // read from the middle of a block
        i = 0;
        
        while(blk < end)
        {
            KDEBUG("fatfs_getdents: 1 blk 0x%x, end 0x%x\n", blk, end);

            ent = (struct fat_dirent_t *)blk;
            *pos = offset + (blk - (unsigned char *)dbuf->virt);
            
            // last entry
            if(blk[0] == 0)
            {
                // account for the case where the short name was deleted but
                // the long name renamed, and free the buffer if alloc'd
                if(lfn && lfn != (char *)lfn_buf)
                {
                    kfree(lfn);
                    lfn = NULL;
                }

                ignore_lfn = 0;
                offset = dir->size;
                break;
            }

            // unused entry
            if(blk[0] == 0xE5)
            {
                // account for the case where the short name was deleted but
                // the long name renamed, and free the buffer if alloc'd
                if(lfn && lfn != (char *)lfn_buf)
                {
                    kfree(lfn);
                    lfn = NULL;
                }

                ignore_lfn = 0;
                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // Long File Name (LFN) entry
            if(ent->attribs == FAT_ATTRIB_LFN)
            {
                if(lfn_buf && !ignore_lfn)
                {
                    // find order of entry in this long name,
                    // the counting is 1-based
                    int x = ((blk[0] & ~0x40) & 0xff);

                    // if the last entry, calculate LFN length
                    if(blk[0] & 0x40)
                    {
                        lfn_len = x * CHARS_PER_LFN_ENTRY;
                    }

                    // even long names should not be too long
                    if(lfn_len >= NAME_MAX || x <= 0 || x >= 0x40)
                    {
                        ignore_lfn = 1;
                    }
                    else
                    {
                        x = (x - 1) * CHARS_PER_LFN_ENTRY;

                        // name chars are scattered through the entry so
                        // we have to collect them
                        for(i = 0; i < CHARS_PER_LFN_ENTRY; i++)
                        {
                            lfn_buf[x + i] = UTF16(blk[lfn_char_offsets[i]],
                                                   blk[lfn_char_offsets[i] + 1]);
                        }
                    }
                }

                blk += FAT_DIRENT_SIZE;
                continue;
            }

            // normal 8.3 entry
            dent = (struct dirent *)b;

            // calc dirent record length
            if(!ignore_lfn && lfn_len != 0)
            {
                // if we cannot get the LFN, break the loop instead of 
                // returning the short name, as it might be a mangled one
                // TODO: should we instead return -ENOMEM?
                if(!(lfn = lfn_finalise(lfn_buf, lfn_len)))
                {
                    offset = dir->size;
                    break;
                }
            }
            else
            {
                lfn = (char *)lfn_buf;
                dos_to_unix_name(lfn, (char *)blk);
            }

            ignore_lfn = 0;
            namelen = strlen(lfn);
            reclen = GET_DIRENT_LEN(namelen);
            add_cacheent(priv, cluster_from_dirent(priv, ent), dir->inode);

            // check the buffer has enough space for this entry
            if((count + reclen) > (size_t)bufsz)
            {
                KDEBUG("fatfs_getdents: count 0x%x, reclen 0x%x\n", count, reclen);
                release_cached_page(dbuf);

                if(lfn && lfn != (char *)lfn_buf)
                {
                    kfree(lfn);
                }

                if(lfn_buf)
                {
                    kfree(lfn_buf);
                }

                return count;
            }

            fatfs_entry_to_dirent(priv, ent, dent, lfn, namelen, 
                                            (*pos) + FAT_DIRENT_SIZE);

            if(lfn && lfn != (char *)lfn_buf)
            {
                kfree(lfn);
                lfn = NULL;
            }

            b += reclen;
            count += reclen;
            blk += FAT_DIRENT_SIZE;
            KDEBUG("fatfs_getdents: 2 blk 0x%x, end 0x%x\n", blk, end);
        }
        
        release_cached_page(dbuf);
        offset += PAGE_SIZE;
    }

    KDEBUG("fatfs_getdents: count 0x%x, offset 0x%x\n", count, offset);

    if(lfn_buf)
    {
        kfree(lfn_buf);
    }

    *pos = offset;
    return count;
}


/*
 * Return filesystem statistics.
 */
long fatfs_ustat(struct mount_info_t *d, struct ustat *ubuf)
{
    struct fat_private_t *priv;

    if(!(d->super))
    {
        return -EINVAL;
    }

    priv = (struct fat_private_t *)(d->super->privdata);

    if(!ubuf)
    {
        return -EFAULT;
    }
    
    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_ustat() function).
     */
    ubuf->f_tfree = priv->free_clusters * priv->sectors_per_cluster;
    ubuf->f_tinode = 0;

    return 0;
}


/*
 * Return detailed filesystem statistics.
 */
long fatfs_statfs(struct mount_info_t *d, struct statfs *statbuf)
{
    struct fat_private_t *priv;

    if(!(d->super))
    {
        return -EINVAL;
    }
    
    priv = (struct fat_private_t *)(d->super->privdata);

    if(!statbuf)
    {
        return -EFAULT;
    }

    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_statfs() function).
     */
    statbuf->f_type = EXFAT_SUPER_MAGIC;
    statbuf->f_bsize = priv->blocksz;
    statbuf->f_blocks = priv->total_clusters * priv->sectors_per_cluster;
    statbuf->f_bfree = priv->free_clusters * priv->sectors_per_cluster;
    statbuf->f_bavail = statbuf->f_bfree;
    statbuf->f_files = 0 /* TODO: get the number of files on disk ??? */;
    statbuf->f_ffree = 0;
    //statbuf->f_fsid = 0;
    statbuf->f_frsize = 0;
    statbuf->f_namelen = NAME_MAX;
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
long fatfs_read_symlink(struct fs_node_t *link, char *buf,
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
size_t fatfs_write_symlink(struct fs_node_t *link, char *target,
                           size_t len, int kernel)
{
    UNUSED(link);
    UNUSED(target);
    UNUSED(len);
    UNUSED(kernel);
    
    return -ENOSYS;
}

