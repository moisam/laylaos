/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: ext2fs.c
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
 *  \file ext2fs.c
 *
 *  This file implements ext2 filesystem functions, which provides access to
 *  disks and media formatted using the second extended filesystem (ext2).
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref ext2fs_ops structure.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>         // NAME_MAX
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/tty.h>
#include <kernel/rtc.h>
#include <kernel/dev.h>
#include <kernel/pcache.h>
#include <kernel/clock.h>
#include <kernel/user.h>
#include <fs/ext2.h>
#include <fs/magic.h>
#include <fs/procfs.h>
#include <mm/kheap.h>
#include <mm/mmap.h>

#define EXT2_SUPPORTED_INCOMPAT_FEATURES        (EXT2_FEATURE_INCOMPAT_FILETYPE)
#define EXT2_SUPPORTED_RO_COMPAT_FEATURES       \
        (EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | EXT2_FEATURE_RO_COMPAT_LARGE_FILE)


static inline int is_empty_block(uint32_t *buf, unsigned long ptr_per_block);
//static void recalc_unalloced_blocks(dev_t dev, struct superblock_t *super);
//static void recalc_unalloced_inodes(dev_t dev, struct superblock_t *super);

uint8_t page_of_zeroes[PAGE_SIZE] = { 0, };


// filesystem operations
struct fs_ops_t ext2fs_ops =
{
    // inode operations
    .read_inode = ext2_read_inode,
    .write_inode = ext2_write_inode,
    //.trunc_inode = ext2_trunc_inode,
    .alloc_inode = ext2_alloc_inode,
    .free_inode = ext2_free_inode,
    .bmap = ext2_bmap,
    
    .read_symlink = ext2_read_symlink,
    .write_symlink = ext2_write_symlink,

    // directory operations
    .finddir = ext2_finddir,
    .finddir_by_inode = ext2_finddir_by_inode,
    //.readdir = ext2_readdir,
    .addir = ext2_addir,
    .mkdir = ext2_mkdir,
    .deldir = ext2_deldir,
    .dir_empty = ext2_dir_empty,
    .getdents = ext2_getdents,

    // device operations
    .mount = NULL,
    .umount = NULL,
    .read_super = ext2_read_super,
    .write_super = ext2_write_super,
    .put_super = ext2_put_super,
    .ustat = ext2_ustat,
    .statfs = ext2_statfs,
};


/*
 * Does dir entries contain a type field insted of filelength MSB?
 */
STATIC_INLINE int is_ext_dir_type(volatile struct ext2_superblock_t *super)
{
    return (super->version_major >= 1 &&
            (super->required_features & EXT2_FEATURE_INCOMPAT_FILETYPE));
}


STATIC_INLINE size_t get_group_count(volatile struct ext2_superblock_t *super)
{
    size_t bgcount = super->total_blocks / super->blocks_per_group;

    if(super->total_blocks % super->blocks_per_group)
    {
        bgcount++;
    }

    return bgcount;
}


STATIC_INLINE uint16_t inode_size(volatile struct ext2_superblock_t *super)
{
    return (super->version_major < 1) ? 128 : super->inode_size;
}


STATIC_INLINE size_t get_bgd_size(volatile struct ext2_superblock_t *super)
{
    size_t block_size, bgd_size;
    size_t bgcount = get_group_count(super);

    block_size = 1024 << super->log2_block_size;
    bgd_size = sizeof(struct block_group_desc_t) * bgcount;

    if(bgd_size % block_size)
    {
        bgd_size &= ~(block_size - 1);
        bgd_size += block_size;
    }

    return bgd_size;
}


/*
 * Read the filesystem's superblock and root inode.
 * This function fills in the mount info struct's block_size, super,
 * and root fields.
 */
long ext2_read_super(dev_t dev, struct mount_info_t *d, size_t bytes_per_sector)
{
    struct superblock_t *super;
    struct ext2_superblock_t *psuper;
    struct disk_req_t req;
    physical_addr ignored;
    int maj = MAJOR(dev);
    uint32_t bgcount[2];
    size_t bgd_size, bgd_block;

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

    KDEBUG("ext2_read_super: super->data 0x%lx\n", super->data);
    A_memset((void *)super->data, 0, PAGE_SIZE);
    KDEBUG("ext2_read_super: pid %d, super->data 0x%lx\n", cur_task ? cur_task->pid : -1, super->data);

    /* Superblock is 1024 bytes long located at 1024 bytes from start */
    if(bytes_per_sector == 512)
    {
        // we use block 1 instead of 2 as we pass a block size of 1024, which will
        // cause the device strategy function to divide 1024 by 512, leading to the
        // wrong block offset
        super->blockno = 2;
        //super->blocksz = 1024;
        req.datasz = 1024;
    }
    else if(bytes_per_sector == 1024)
    {
        super->blockno = 1;
        //super->blocksz = 1024;
        req.datasz = 1024;
    }
    else if(bytes_per_sector == 2048 || bytes_per_sector == 4096)
    {
        super->blockno = 0;
        //super->blocksz = bytes_per_sector;
        req.datasz = bytes_per_sector;
    }
    else
    {
        printk("ext2fs: unknown disk block size: 0x%x\n", bytes_per_sector);
        kpanic("Failed to read ext2 superblock!\n");
    }

    super->blocksz = bytes_per_sector;
    super->dev = dev;

    req.dev = dev;
    req.data = super->data;
    //req.datasz = super->blocksz;
    req.fs_blocksz = super->blocksz;

    req.blockno = super->blockno;
    req.write = 0;

    printk("ext2: reading superblock (dev 0x%x)\n", dev);

#define BAIL_OUT(err)   \
        vmmngr_free_page(get_page_entry((void *)super->data));  \
        vmmngr_flush_tlb_entry(super->data);    \
        kfree(super);   \
        return err;

    if(bdev_tab[maj].strategy(&req) < 0)
    {
        printk("ext2: failed to read from disk -- aborting mount\n");
        BAIL_OUT(-EIO);
    }

    /*
     * Move the superblock data in our buffer if the disk's block size is
     * larger than 1024. We need to preserve the boot block, which is found
     * at offset 0. We do this so the other functions in this module
     * can read the superblock with the boot block out of the way.
     * We'll move the boot block from offset 0 to offset 1024 now, then
     * relocate it in ext2_write_super() before writing it to disk.
     */
    if(bytes_per_sector == 2048 || bytes_per_sector == 4096)
    {
        //char buf[1024];
        char *buf;

        if(!(buf = kmalloc(1024)))
        {
            BAIL_OUT(-ENOMEM);
        }

        A_memcpy((void *)buf, (void *)super->data, 1024);
        A_memcpy((void *)super->data, (void *)(super->data + 1024), 1024);
        A_memcpy((void *)(super->data + 1024), (void *)buf, 1024);

        kfree(buf);
    }

    psuper = (struct ext2_superblock_t *)(super->data);

    /* check boot sector signature */
    if(psuper->signature != EXT2_SUPER_MAGIC)
    {
        printk("ext2: invalid signature -- aborting mount\n");
        BAIL_OUT(-EINVAL);
    }

    if((psuper->required_features & ~EXT2_SUPPORTED_INCOMPAT_FEATURES) ||
       (psuper->readonly_features & ~EXT2_SUPPORTED_RO_COMPAT_FEATURES))
    {
        printk("ext2: unsupported features (req 0x%x, ro 0x%x) -- aborting mount\n",
                psuper->required_features, psuper->readonly_features);
        BAIL_OUT(-EINVAL);
    }

    if(psuper->filesystem_state != EXT2_VALID_FS)
    {
        /*
         * TODO: we should run fsck here.
         */
        printk("ext2: filesystem not clean -- aborting mount\n");
        BAIL_OUT(-EINVAL);
    }

    /* validate block group count */
    bgcount[0] = psuper->total_inodes / psuper->inodes_per_group;
    bgcount[1] = psuper->total_blocks / psuper->blocks_per_group;

    if(psuper->total_inodes % psuper->inodes_per_group)
    {
        bgcount[0]++;
    }

    if(psuper->total_blocks % psuper->blocks_per_group)
    {
        bgcount[1]++;
    }

    if(bgcount[0] != bgcount[1])
    {
        printk("ext2: block group count mismatch -- aborting mount\n");
        BAIL_OUT(-EINVAL);
    }

    printk("ext2: superblock signature 0x%x\n", psuper->signature);
    printk("      total_inodes %u, total_blocks %u, reserved_blocks %u\n",
            psuper->total_inodes, psuper->total_blocks, psuper->reserved_blocks);
    printk("      unalloc_blocks %u, unalloc_inodes %u, superblock_block %u\n",
            psuper->unalloc_blocks, psuper->unalloc_inodes, psuper->superblock_block);
    printk("      block_size %u\n", (1024 << psuper->log2_block_size));
  
    /* inode size */
    /*
    if(psuper->version_major < 1)
    {
        psuper->inode_size = 128;
        psuper->first_nonreserved_inode = 11;
    }
    */
    
    d->block_size = 1024 << psuper->log2_block_size;
    d->super = super;

    /*
     * Now read the block group descriptor table into memory.
     * This table will be accessed every single time an inode or a block
     * is requested, so keep a copy in memory for quick access.
     *
     * Of course, this means we need to sync the table to disk regularly,
     * which we do when the update task calls us to write the superblock.
     */
    bgd_block = (d->block_size <= 1024) ? 2 : 1;
    bgd_size = get_bgd_size(psuper);

    if(!(super->privdata = 
            vmmngr_alloc_and_map(align_up(bgd_size), 0, PTE_FLAGS_PW, NULL, REGION_DMA)))
    {
        printk("ext2: insufficient memory to load block group descriptor table\n");
        BAIL_OUT(-ENOMEM);
    }

    A_memset((void *)super->privdata, 0, bgd_size);
    req.dev = dev;
    req.data = super->privdata;
    req.datasz = bgd_size;
    req.fs_blocksz = d->block_size;
    req.blockno = bgd_block;
    req.write = 0;

    printk("ext2: reading block group descriptor table\n");

    if(bdev_tab[maj].strategy(&req) < 0)
    {
        printk("ext2: failed to read from disk -- aborting mount\n");
        vmmngr_free_pages(super->privdata, super->privdata + align_up(bgd_size));
        BAIL_OUT(-EIO);
    }

#undef BAIL_OUT


    /*
    struct block_group_desc_t *tab = (struct block_group_desc_t *)super->privdata;

    for(int x = 0; x < 20; x++)
    {
        printk("[%d] %d, %d, %d, %d, %d, %d\n", x,
            tab[x].block_bitmap_addr,
            tab[x].inode_bitmap_addr,
            tab[x].inode_table_addr,
            tab[x].unalloc_blocks,
            tab[x].unalloc_inodes,
            tab[x].dir_count);
    }

    if(dev != 0x1fa) for(;;);
    */


    printk("ext2: reading root node\n");
    d->root = get_node(dev, EXT2_ROOT_INO, 0);
    psuper->last_mount_time = now();
    psuper->mounts_since_last_check++;

    /*
     * Documentation says (https://cscie28.dce.harvard.edu/lectures/lect04/6_Extras/ext2-struct.html):
     *
     * 	  When the file system is mounted, state is set to EXT2_ERROR_FS.
     *    After the file system was cleanly unmounted, set to EXT2_VALID_FS.
     */
    psuper->filesystem_state = EXT2_ERROR_FS;

    printk("ext2: mounting done\n");

    return 0;
}


/*
 * TODO: update superblock backups.
 *
 * Documentation says (https://cscie28.dce.harvard.edu/lectures/lect04/6_Extras/ext2-struct.html):
 *
 *    	With the introduction of revision 1 and the sparse superblock 
 *      feature in Ext2, only specific block groups contain copies of the 
 *      superblock and block group descriptor table. All block groups still
 *      contain the block bitmap, inode bitmap, inode table, and data 
 *      blocks. The shadow copies of the superblock can be located in
 *      block groups 0, 1 and powers of 3, 5 and 7.
 */
static int __ext2_write_super(/* struct ext2_superblock_t *super, */
                              struct disk_req_t *req, 
                              dev_t dev /* , size_t block_size */)
{
    return bdev_tab[MAJOR(dev)].strategy(req);
    /*
    int res;
    int off = !!(block_size == 1024);

    // write the superblock
    if((res = bdev_tab[MAJOR(dev)].strategy(req)) < 0)
    {
        return res;
    }

    // write shadow copy at group #1
    req->blockno = super->blocks_per_group + off;

    return (req->blockno < super->total_blocks) ?
            bdev_tab[MAJOR(dev)].strategy(req) : 0;
    */
}

static int __ext2_write_bgd(/* struct ext2_superblock_t *super, */
                            struct disk_req_t *req, 
                            dev_t dev /* , size_t block_size */)
{
    return bdev_tab[MAJOR(dev)].strategy(req);
    /*
    int res;
    int off = !!(block_size == 1024);

    // write the block group descriptor
    if((res = bdev_tab[MAJOR(dev)].strategy(req)) < 0)
    {
        return res;
    }

    // write shadow copy at group #1
    req->blockno = super->blocks_per_group + off + 1;

    return (req->blockno < super->total_blocks) ?
            bdev_tab[MAJOR(dev)].strategy(req) : 0;
    */
}

/*
 * Write the filesystem's superblock to disk.
 */
long ext2_write_super(dev_t dev, struct superblock_t *super)
{
    int res;
    size_t bgd_size, bgd_block, block_size;
    struct ext2_superblock_t *psuper;
    struct disk_req_t req;
    
    if(!super)
    {
        return -EINVAL;
    }

    psuper = (struct ext2_superblock_t *)(super->data);
    psuper->last_written_time = now();

    //recalc_unalloced_blocks(dev, super);
    //recalc_unalloced_inodes(dev, super);

    block_size = 1024 << psuper->log2_block_size;

    req.dev = dev;
    req.data = super->data;
    //req.datasz = super->blocksz;
    req.datasz = (super->blocksz <= 1024) ? 1024 : super->blocksz;
    req.fs_blocksz = super->blocksz;
    req.blockno = super->blockno;
    req.write = 1;

    /*
     * Move the superblock data in our buffer if the disk's block size is
     * larger than 1024. We've moved the boot block from offset 0 to offset
     * 1024 when we read it in ext2_read_super(). We need to relocate the
     * boot block to its correct position before writing out to disk.
     */
    if(super->blocksz == 2048 || super->blocksz == 4096)
    {
        //char buf[1024];
        char *buf;

        if(!(buf = kmalloc(1024)))
        {
            return -ENOMEM;
        }

        A_memcpy((void *)buf, (void *)(super->data + 1024), 1024);
        A_memcpy((void *)(super->data + 1024), (void *)super->data, 1024);
        A_memcpy((void *)super->data, (void *)buf, 1024);

        res = __ext2_write_super(/* psuper, */ &req, dev /* , block_size */);

        A_memcpy((void *)buf, (void *)super->data, 1024);
        A_memcpy((void *)super->data, (void *)(super->data + 1024), 1024);
        A_memcpy((void *)(super->data + 1024), (void *)buf, 1024);

        kfree(buf);
    }
    else
    {
        res = __ext2_write_super(/* psuper, */ &req, dev /* , block_size */);
    }

    if(!super->privdata || res < 0)
    {
        return (res < 0) ? -EIO : 0;
    }

    /*
     * Now write the block group descriptor table.
     */
    bgd_size = get_bgd_size(psuper);
    bgd_block = (block_size <= 1024) ? 2 : 1;

    req.dev = dev;
    req.data = super->privdata;
    req.datasz = bgd_size;
    //req.fs_blocksz = super->blocksz;
    req.fs_blocksz = block_size;
    req.blockno = bgd_block;
    req.write = 1;
    res = __ext2_write_bgd(/* psuper, */ &req, dev /* , block_size */);
    __asm__ __volatile__("":::"memory");

    return (res < 0) ? -EIO : 0;
}


/*
 * Release the filesystem's superblock and its buffer.
 * Called when unmounting the filesystem.
 */
void ext2_put_super(dev_t dev, struct superblock_t *super)
{
    struct ext2_superblock_t *psuper;

    if(!super || !super->data)
    {
        return;
    }

    /*
     * Documentation says (https://cscie28.dce.harvard.edu/lectures/lect04/6_Extras/ext2-struct.html):
     *
     * 	  When the file system is mounted, state is set to EXT2_ERROR_FS.
     *    After the file system was cleanly unmounted, set to EXT2_VALID_FS.
     */
    psuper = (struct ext2_superblock_t *)(super->data);
    psuper->filesystem_state = EXT2_VALID_FS;
    ext2_write_super(dev, super);

    if(super->privdata)
    {
        size_t bgd_size = get_bgd_size((struct ext2_superblock_t *)(super->data));
        vmmngr_free_pages(super->privdata, super->privdata + align_up(bgd_size));
        super->privdata = 0;
    }

    vmmngr_free_page(get_page_entry((void *)super->data));
    vmmngr_flush_tlb_entry(super->data);

    kfree(super);
}


/*
 * Helper function that copies info from an ext2 disk node to an incore
 * (memory resident) node.
 */
void inode_to_incore(struct fs_node_t *n, struct inode_data_t *i)
{
    int j;
    
    n->mode = i->permissions;
    n->uid = i->user_id;
    n->mtime = (time_t)i->last_modification_time & 0xffffffff;
    n->atime = (time_t)i->last_access_time & 0xffffffff;
    n->ctime = (time_t)i->creation_time & 0xffffffff;

    if(sizeof(size_t) == 4)
    {
        n->size = i->size_lsb;
    }
    else
    {
        n->size = i->size_lsb | ((uint64_t)i->size_msb << 32);
    }

    n->links = i->hard_links;
    n->gid = i->group_id;
    
    for(j = 0; j < 12; j++)
    {
        n->blocks[j] = i->block_p[j];
    }
    
    n->blocks[j++] = i->single_indirect_pointer;
    n->blocks[j++] = i->double_indirect_pointer;
    n->blocks[j++] = i->triple_indirect_pointer;
    n->disk_sectors = i->disk_sectors;
    __asm__ __volatile__("":::"memory");
}


/*
 * Helper function that copies info from an incore (memory resident) node to
 * an ext2 disk node.
 */
void incore_to_inode(struct inode_data_t *i, struct fs_node_t *n)
{
    int j;

    i->permissions = n->mode;
    i->user_id = n->uid;
    i->last_modification_time = (uint32_t)n->mtime;
    i->last_access_time = (uint32_t)n->atime;
    i->creation_time = (uint32_t)n->ctime;
    i->size_lsb = n->size & 0xffffffff;     // for pipes, the pipe's physical
                                            // memory address

    if(sizeof(size_t) == 4)
    {
        i->size_msb = 0;
    }
    else
    {
        i->size_msb = ((uint64_t)n->size >> 32) & 0xffffffff;
    }

    i->hard_links = n->links;
    i->group_id = n->gid;
    
    for(j = 0; j < 12; j++)
    {
        i->block_p[j] = n->blocks[j];
    }
    
    i->single_indirect_pointer = n->blocks[j++];
    i->double_indirect_pointer = n->blocks[j++];
    i->triple_indirect_pointer = n->blocks[j++];
    i->disk_sectors = n->disk_sectors;
    __asm__ __volatile__("":::"memory");
}


/*
 * Helper function that returns the filesystem's superblock struct.
 */
STATIC_INLINE int get_super(dev_t dev, struct mount_info_t **d,
                                       volatile struct ext2_superblock_t **super)
{
    if((*d = get_mount_info(dev)) == NULL || !((*d)->super))
    {
        return -EINVAL;
    }
    
    *super = (struct ext2_superblock_t *)((*d)->super->data);
    return 0;
}


/*
 * Helper function to read a block group descriptor table.
 *
 * Input:
 *    dev         => device id
 *
 * Output:
 *    bgd         => pointer to the buffer containing the block group
 *                     descriptor table and superblock
 *
 * Returns:
 *    0 on success, -errno on failure
 */
struct bgd_table_info_t
{
    struct mount_info_t *d;
    volatile struct ext2_superblock_t *super;
    volatile struct block_group_desc_t *bgd_table;
};

STATIC_INLINE int get_bgd_table(dev_t dev, struct bgd_table_info_t *bgd)
{
    if(get_super(dev, &bgd->d, &bgd->super) < 0)
    {
        return -EINVAL;
    }

    if(!(bgd->bgd_table = (struct block_group_desc_t *)bgd->d->super->privdata))
    {
        return -EINVAL;
    }
    
    return 0;
}


STATIC_INLINE uint32_t inode_group(volatile struct ext2_superblock_t *super, uint32_t n)
{
    return (n - 1) / super->inodes_per_group;
}


STATIC_INLINE uint32_t inode_index(volatile struct ext2_superblock_t *super, uint32_t n)
{
    return (n - 1) % super->inodes_per_group;
}


/*
 * The documentation clearly states that blocks are zero-based and inodes
 * are one-based. However, having wasted a few days trying to find out why
 * any new file I create in an ext2 disk ends up overlapping with another
 * file's blocks, I concluded that blocks should be treated as one-based when
 * accessing the block bitmap (at least on ext2 with block size of 1k).
 * I assume other block sizes should still be zero-based, but I have not
 * tested this theory yet.
 */
STATIC_INLINE uint32_t block_group(volatile struct ext2_superblock_t *super, uint32_t n)
{
    return (n - super->superblock_block) / super->blocks_per_group;
}


STATIC_INLINE uint32_t block_index(volatile struct ext2_superblock_t *super, uint32_t n)
{
    return (n - super->superblock_block) % super->blocks_per_group;
}


/*
 * Helper function to read a block table.
 *
 * Input:
 *    n           => number of the inode/block for which we want to find the
 *                     group descriptor table
 *    dev         => device id
 *    bgd         => pointer to the buffer containing the block group
 *                     descriptor table and superblock
 *
 * Output:
 *    inode       => if not null, pointer to the searched inode struct
 *    block_table => pointer to the buffer containing the block table
 *
 * Returns:
 *    0 on success, -errno on failure
 */
STATIC_INLINE int get_block_table(struct bgd_table_info_t *bgd,
                                  struct inode_data_t **inode,
                                  struct cached_page_t **block_table,
                                  dev_t dev, uint32_t n)
{
    struct fs_node_header_t tmp;
    size_t block_size, table_block, off0, off1, off2;

    block_size = 1024 << bgd->super->log2_block_size;
    table_block = bgd->bgd_table[inode_group(bgd->super, n)].inode_table_addr;
    off0 = (inode_index(bgd->super, n) * inode_size(bgd->super));
    off1 = off0 / block_size;
    off2 = off0 % block_size;
    table_block += off1;

    tmp.inode = PCACHE_NOINODE;
    tmp.dev = dev;

    if(table_block == 0)
    {
        switch_tty(1);
        printk("ext2: in get_block_table():\n");
        printk("ext2: dev 0x%x, n 0x%x\n", dev, n);
        printk("ext2: off0 0x%lx, off1 0x%lx, off2 0x%lx\n", off0, off1, off2);
        printk("ext2: invalid table_block: 0x%lx\n", table_block);
        kpanic("Invalid/corrupt disk\n");
    }

    if(!(*block_table = get_cached_page((struct fs_node_t *)&tmp, table_block, 0)))
    {
        return -EIO;
    }

    *inode = (struct inode_data_t *)((*block_table)->virt + off2);
    __asm__ __volatile__("":::"memory");

    return 0;
}


/*
 * Helper function to read a bitmap table.
 *
 * Input & Output similar to the above.
 *
 * Returns:
 *    0 on success, -errno on failure
 */
int get_block_bitmap(struct bgd_table_info_t *bgd,
                     struct cached_page_t **block_bitmap,
                     dev_t dev, uint32_t group, int is_inode)
{
    struct fs_node_header_t tmp;
    size_t table_block;

    table_block = is_inode ? bgd->bgd_table[group].inode_bitmap_addr :
                             bgd->bgd_table[group].block_bitmap_addr;

    tmp.inode = PCACHE_NOINODE;
    tmp.dev = dev;

    if(table_block == 0)
    {
        kpanic("ext2: illegal bitmap address in get_block_bitmap()\n");
    }

    if(!(*block_bitmap = get_cached_page((struct fs_node_t *)&tmp, table_block, 0)))
    {
        return -EIO;
    }

    __asm__ __volatile__("":::"memory");
    
    return 0;
}


#if 0

static void recalc_unalloced_blocks(dev_t dev, struct superblock_t *super)
{
    volatile uint8_t *bitmap;
    volatile uint32_t i, j, k;
    uint32_t blocks_per_group, unalloced, total_unalloced = 0;
    size_t bgcount;
    struct cached_page_t *block_bitmap;
    struct ext2_superblock_t *psuper;
    struct bgd_table_info_t bgd;

    if(!(bgd.bgd_table = (struct block_group_desc_t *)super->privdata))
    {
        return;
    }

    psuper = (struct ext2_superblock_t *)(super->data);
    bgcount = get_group_count(psuper);
    blocks_per_group = psuper->blocks_per_group;

    for(i = 0; i < bgcount; i++)
    {
        if(get_block_bitmap(&bgd, &block_bitmap, dev, i, 0) < 0)
        {
            continue;
        }

        bitmap = (volatile uint8_t *)block_bitmap->virt;
        unalloced = 0;

        for(j = 0; j < blocks_per_group / 8; j++)
        {
            for(k = 0; k < 8; k++)
            {
                if((bitmap[j] & (1 << k)) == 0)
                {
                    unalloced++;
                }
            }
        }

        bgd.bgd_table[i].unalloc_blocks = unalloced;
        total_unalloced += unalloced;
        release_cached_page(block_bitmap);
    }

    psuper->unalloc_blocks = total_unalloced;
}


static void recalc_unalloced_inodes(dev_t dev, struct superblock_t *super)
{
    volatile uint8_t *bitmap;
    volatile uint32_t i, j, k;
    uint32_t inodes_per_group, unalloced, total_unalloced = 0;
    size_t bgcount;
    struct cached_page_t *block_bitmap;
    struct ext2_superblock_t *psuper;
    struct bgd_table_info_t bgd;

    if(!(bgd.bgd_table = (struct block_group_desc_t *)super->privdata))
    {
        return;
    }

    psuper = (struct ext2_superblock_t *)(super->data);
    bgcount = get_group_count(psuper);
    inodes_per_group = psuper->inodes_per_group;

    for(i = 0; i < bgcount; i++)
    {
        if(get_block_bitmap(&bgd, &block_bitmap, dev, i, 1) < 0)
        {
            continue;
        }

        bitmap = (volatile uint8_t *)block_bitmap->virt;
        unalloced = 0;

        for(j = 0; j < inodes_per_group / 8; j++)
        {
            for(k = 0; k < 8; k++)
            {
                if((bitmap[j] & (1 << k)) == 0)
                {
                    unalloced++;
                }
            }
        }

        bgd.bgd_table[i].unalloc_inodes = unalloced;
        total_unalloced += unalloced;
        release_cached_page(block_bitmap);
    }

    psuper->unalloc_inodes = total_unalloced;
}

#endif


/*
 * Reads inode data structure from disk.
 */
long ext2_read_inode(struct fs_node_t *node)
{
    struct cached_page_t *block_table;
    struct inode_data_t *inode;
    struct bgd_table_info_t bgd;
    long res;

    if((res = get_bgd_table(node->dev, &bgd)) < 0)
    {
        return res;
    }

    if((res = get_block_table(&bgd, &inode, &block_table, node->dev, node->inode)) < 0)
    {
        return res;
    }

    inode_to_incore(node, inode);
    release_cached_page(block_table);

    return 0;
}


/*
 * Writes inode data structure to disk.
 */
long ext2_write_inode(struct fs_node_t *node)
{
    struct cached_page_t *block_table;
    struct inode_data_t *inode;
    struct bgd_table_info_t bgd;
    long res;

    if((res = get_bgd_table(node->dev, &bgd)) < 0)
    {
        return res;
    }

    if((res = get_block_table(&bgd, &inode, &block_table, node->dev, node->inode)) < 0)
    {
        return res;
    }
    
    incore_to_inode(inode, node);

    /*
     * Clear the htree index flag if this is a directory and we have written
     * to it.
     *
     * TODO: remove this when we have support for indexed directories.
     */
    if(S_ISDIR(node->mode) && (node->flags & FS_NODE_DIRTY))
    {
        inode->flags &= ~EXT2_INDEX_FL;
    }

    __sync_or_and_fetch(&block_table->flags, PCACHE_FLAG_DIRTY);
    release_cached_page(block_table);

    return 0;
}


STATIC_INLINE void inc_node_disk_blocks(struct fs_node_t *node, uint32_t block_size)
{
    node->disk_sectors += block_size / 512;
}


STATIC_INLINE void dec_node_disk_blocks(struct fs_node_t *node, uint32_t block_size)
{
    node->disk_sectors -= block_size / 512;
}


/*
 * Helper function called by ext2_bmap() to alloc a new block if needed.
 *
 * Returns 1 if a new block was allocated, 0 otherwise. This is so that the
 * caller can zero out the new block before use.
 */
STATIC_INLINE void bmap_may_create_block(struct fs_node_t *node,
                                         volatile uint32_t *block, uint32_t block_size,
                                         int create)
{
    if(create && !*block)
    {
        if((*block = ext2_alloc(node->dev)))
        {
            struct disk_req_t req;

            req.dev = node->dev;
            req.data = (virtual_addr)page_of_zeroes;

            req.datasz = block_size;
            req.fs_blocksz = block_size;

            req.blockno = *block;
            req.write = 1;

            bdev_tab[MAJOR(node->dev)].strategy(&req);

            inc_node_disk_blocks(node, block_size);
            node->ctime = now();
            node->flags |= FS_NODE_DIRTY;
        }
    }
}


/*
 * Helper function called by ext2_bmap() to free a block if not needed anymore.
 */
STATIC_INLINE void bmap_free_block(struct fs_node_t *node, 
                                   volatile uint32_t *block, uint32_t block_size)
{
    ext2_free(node->dev, *block);
    *block = 0;
    dec_node_disk_blocks(node, block_size);
}


STATIC_INLINE void mark_node_dirty(struct fs_node_t *node)
{
    node->ctime = now();
    node->flags |= FS_NODE_DIRTY;
}


/*
 * Helper function called by ext2_bmap() to free a block if not needed anymore.
 * It also frees the single indirect block if it is empty.
 *
 * Input:
 *    node          => file node
 *    iblockp       => address of the single indirect block pointer (can be
 *                       in node->single_indirect_pointer or an entry in a
 *                       double indirect block)
 *    buf           => buffer containing data of the single indirect block
 *    block         => address of the block to be freed in the single indirect
 *                       block buf
 *    ptr_per_block => pointers per block (512 for a standard 2048 byte block)
 *
 * Returns:
 *    1 if the single indirect block was freed, 0 otherwise
 */
STATIC_INLINE
int bmap_may_free_iblock(struct fs_node_t *node, volatile uint32_t *iblockp,
                         struct cached_page_t *pcache, uint32_t block,
                         uint32_t block_size, unsigned long ptr_per_block)
{
    bmap_free_block(node, &(((uint32_t *)pcache->virt)[block]), block_size);
    __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_DIRTY);
    
    // free the single indirect block itself if it is empty
    if(is_empty_block((uint32_t *)pcache->virt, ptr_per_block))
    {
        release_cached_page(pcache);
        ext2_free(node->dev, *iblockp);
        *iblockp = 0;
        dec_node_disk_blocks(node, block_size);
        mark_node_dirty(node);
        return 1;
    }

    release_cached_page(pcache);
    mark_node_dirty(node);
    return 0;
}


/*
 * Helper function called by ext2_bmap() to free a block if not needed anymore.
 * It also frees the single and double indirect blocks if they are empty.
 *
 * Input:
 *    node          => file node
 *    iblockp       => address of the double indirect block pointer (can be
 *                       in node->double_indirect_pointer or an entry in a
 *                       triple indirect block)
 *    buf           => buffer containing data of the double indirect block
 *    buf2          => buffer containing data of the single indirect block
 *    block         => address of the single indrect block in the double
 *                       indirect block buf
 *    block2        => address of the block to be freed in the single indirect
 *                       block buf
 *    ptr_per_block => pointers per block (512 for a standard 2048 byte block)
 *
 * Returns:
 *    1 if the double indirect block was freed, 0 otherwise
 */
STATIC_INLINE
int bmap_may_free_diblock(struct fs_node_t *node, volatile uint32_t *iblockp,
                          struct cached_page_t *pcache,
                          struct cached_page_t *pcache2,
                          uint32_t block, uint32_t block2,
                          uint32_t block_size, unsigned long ptr_per_block)
{
    // free the single indirect block if it is empty
    bmap_may_free_iblock(node, &(((uint32_t *)pcache->virt)[block]),
                         pcache2, block2, block_size, ptr_per_block);
    __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_DIRTY);

    // free the double indirect block itself if it is empty
    if(is_empty_block((uint32_t *)pcache->virt, ptr_per_block))
    {
        release_cached_page(pcache);
        ext2_free(node->dev, *iblockp);
        *iblockp = 0;
        dec_node_disk_blocks(node, block_size);
        mark_node_dirty(node);
        return 1;
    }

    release_cached_page(pcache);
    return 0;
}


/*
 * Helper function called by ext2_bmap() to free a block if not needed
 * anymore. It also frees the single, double and triple indirect blocks if
 * they are empty.
 *
 * Inputs and return values are similar to the above, with additional pointers
 * to look into the triple indirect block.
 */
STATIC_INLINE
int bmap_may_free_tiblock(struct fs_node_t *node, uint32_t *iblockp,
                          struct cached_page_t *pcache,
                          struct cached_page_t *pcache2,
                          struct cached_page_t *pcache3,
                          uint32_t block, uint32_t block2, uint32_t block3,
                          uint32_t block_size, unsigned long ptr_per_block)
{
    // free the single indirect block if it is empty
    bmap_may_free_diblock(node, &(((uint32_t *)pcache->virt)[block]),
                         pcache2, pcache3, block2, block3, 
                         block_size, ptr_per_block);
    __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_DIRTY);

    // free the double indirect block itself if it is empty
    if(is_empty_block((uint32_t *)pcache->virt, ptr_per_block))
    {
        release_cached_page(pcache);
        ext2_free(node->dev, *iblockp);
        *iblockp = 0;
        dec_node_disk_blocks(node, block_size);
        mark_node_dirty(node);
        return 1;
    }

    release_cached_page(pcache);
    return 0;
}


/*
 * Check if an indirect block is empty, i.e. all pointers are zeroes.
 */
STATIC_INLINE int is_empty_block(uint32_t *buf, unsigned long ptr_per_block)
{
    unsigned long i;
    
    for(i = 0; i < ptr_per_block; i++)
    {
        if(*buf)
        {
            return 0;
        }
        
        buf++;
    }
    
    return 1;
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
size_t ext2_bmap(struct fs_node_t *node, size_t lblock,
                 size_t block_size, int flags)
{
    struct cached_page_t *buf, *buf2, *buf3;
    struct fs_node_header_t tmpnode;
    unsigned long ptr_per_block = block_size / sizeof(uint32_t);
    unsigned long ptr_per_block2 = ptr_per_block * ptr_per_block;
    unsigned long maxptrs = 12 + ptr_per_block + ptr_per_block2 +
                                 (ptr_per_block2 * ptr_per_block);
    size_t i, j, k, l;
    int create = (flags & BMAP_FLAG_CREATE);
    int free = (flags & BMAP_FLAG_FREE);
    volatile uint32_t tmp;
    
    if(lblock >= maxptrs)
    {
        return 0;
    }

    /*
     * Symlinks less than 60 chars in length are stored in the inode itself.
     * See:
     *    http://www.nongnu.org/ext2-doc/ext2.html#def-symbolic-links
     */
    if(S_ISLNK(node->mode) && node->size < 60)
    {
        if(free)
        {
            for(i = 0; i < 15; i++)
            {
                node->blocks[i] = 0;
            }
        }

        return 0;
    }

    tmpnode.dev = node->dev;
    tmpnode.inode = PCACHE_NOINODE;

    // check direct block pointers
    if(lblock < 12)
    {
        tmp = node->blocks[lblock];
        bmap_may_create_block(node, &tmp, block_size, create);
        node->blocks[lblock] = tmp;

        // free block if we're shrinking the file
        if(free && node->blocks[lblock])
        {
            bmap_free_block(node, &tmp, block_size);
            node->blocks[lblock] = tmp;
            mark_node_dirty(node);
        }

        return node->blocks[lblock];
    }
    
    // check single indirect block pointer
    lblock -= 12;

    if(lblock < ptr_per_block)
    {
        // read the single indirect block
        tmp = node->blocks[12];
        bmap_may_create_block(node, &tmp, block_size, create);
        
        if(!(node->blocks[12] = tmp))
        {
            return 0;
        }

        if(!(buf = get_cached_page((struct fs_node_t *)&tmpnode, node->blocks[12], 0)))
        {
            return 0;
        }
        
        // alloc block if needed for the new block
        bmap_may_create_block(node, &(((uint32_t *)buf->virt)[lblock]), 
                              block_size, create);
        i = ((uint32_t *)buf->virt)[lblock];
        __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
        
        // free the block and the indirect block if we're shrinking the file
        if(free && i)
        {
            tmp = node->blocks[12];
            bmap_may_free_iblock(node, &tmp, buf, lblock, block_size, ptr_per_block);
            node->blocks[12] = tmp;
            return 0;
        }
        
        release_cached_page(buf);
        
        return i;
    }

    // check double indirect block pointer
    lblock -= ptr_per_block;

    if(lblock < ptr_per_block2)
    {
        // read the double indirect block
        tmp = node->blocks[13];
        bmap_may_create_block(node, &tmp, block_size, create);
        
        if(!(node->blocks[13] = tmp))
        {
            return 0;
        }
        
        if(!(buf = get_cached_page((struct fs_node_t *)&tmpnode, node->blocks[13], 0)))
        {
            return 0;
        }

        // find the single indirect block
        j = lblock / ptr_per_block;

        bmap_may_create_block(node, &(((uint32_t *)buf->virt)[j]), 
                                    block_size, create);
        i = ((uint32_t *)buf->virt)[j];
        __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
        
        if(!i)
        {
            release_cached_page(buf);
            return 0;
        }

        if(!(buf2 = get_cached_page((struct fs_node_t *)&tmpnode, i, 0)))
        {
            release_cached_page(buf);
            return 0;
        }

        // find the block
        k = lblock % ptr_per_block;

        bmap_may_create_block(node, &(((uint32_t *)buf2->virt)[k]), 
                              block_size, create);
        i = ((uint32_t *)buf2->virt)[k];
        __sync_or_and_fetch(&buf2->flags, PCACHE_FLAG_DIRTY);

        // free the block and the indirect blocks if we're shrinking the file
        if(free && i)
        {
            tmp = node->blocks[13];
            bmap_may_free_diblock(node, &tmp, buf, buf2, j, k, 
                                  block_size, ptr_per_block);
            node->blocks[13] = tmp;
            return 0;
        }

        release_cached_page(buf);
        release_cached_page(buf2);

        return i;
    }

    // check triple indirect block pointer
    lblock -= ptr_per_block2;

    tmp = node->blocks[14];
    bmap_may_create_block(node, &tmp, block_size, create);
    
    if(!(node->blocks[14] = tmp))
    {
        return 0;
    }
    
    if(!(buf = get_cached_page((struct fs_node_t *)&tmpnode, node->blocks[14], 0)))
    {
        return 0;
    }

    j = lblock / ptr_per_block2;
    bmap_may_create_block(node, &(((uint32_t *)buf->virt)[j]), 
                                block_size, create);
    i = ((uint32_t *)buf->virt)[j];
    __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
    
    if(!i)
    {
        release_cached_page(buf);
        return 0;
    }

    if(!(buf2 = get_cached_page((struct fs_node_t *)&tmpnode, i, 0)))
    {
        release_cached_page(buf);
        return 0;
    }

    lblock = lblock % ptr_per_block2;
    k = lblock / ptr_per_block;
    bmap_may_create_block(node, &(((uint32_t *)buf2->virt)[k]), block_size, create);
    i = ((uint32_t *)buf2->virt)[k];
    __sync_or_and_fetch(&buf2->flags, PCACHE_FLAG_DIRTY);
    
    if(!i)
    {
        release_cached_page(buf);
        release_cached_page(buf2);
        return 0;
    }

    if(!(buf3 = get_cached_page((struct fs_node_t *)&tmpnode, i, 0)))
    {
        release_cached_page(buf);
        release_cached_page(buf2);
        return 0;
    }

    l = lblock % ptr_per_block;
    bmap_may_create_block(node, &(((uint32_t *)buf3->virt)[l]), block_size, create);
    i = ((uint32_t *)buf3->virt)[l];
    __sync_or_and_fetch(&buf3->flags, PCACHE_FLAG_DIRTY);

    // free the block and the indirect blocks if we're shrinking the file
    if(free && i)
    {
        uint32_t tmp = node->blocks[14];
        bmap_may_free_tiblock(node, &tmp, buf, buf2, buf3, j, k, l, 
                              block_size, ptr_per_block);
        node->blocks[14] = tmp;
        return 0;
    }

    release_cached_page(buf);
    release_cached_page(buf2);
    release_cached_page(buf3);
    
    return i;
}


/*
 * Free an inode and update inode bitmap on disk.
 *
 * MUST write the node to disk if the filesystem supports inode structures
 * separate to their directory entries (e.g. ext2, tmpfs).
 */
long ext2_free_inode(struct fs_node_t *node)
{
    volatile uint8_t *bitmap;
    volatile uint32_t index, group;
    struct cached_page_t *block_bitmap;
    struct bgd_table_info_t bgd;

    // write out the node before we free it on disk
    // TODO: should we check for errors here?
    ext2_write_inode(node);

    if(get_bgd_table(node->dev, &bgd) < 0)
    {
        return -EINVAL;
    }
    
    if(node->inode < 1 || node->inode > bgd.super->total_inodes)
    {
        return -EINVAL;
    }

    index = inode_index(bgd.super, node->inode);
    group = inode_group(bgd.super, node->inode);

    if(get_block_bitmap(&bgd, &block_bitmap, node->dev, group, 1) < 0)
    {
        return -EINVAL;
    }

    bitmap = (volatile uint8_t *)block_bitmap->virt;
    bitmap[index / 8] &= ~(1 << (index % 8));
    __sync_or_and_fetch(&block_bitmap->flags, PCACHE_FLAG_DIRTY);
    __asm__ __volatile__("":::"memory");
    release_cached_page(block_bitmap);

    kernel_mutex_lock(&(bgd.d->lock));
    bgd.bgd_table[group].unalloc_inodes++;
    bgd.super->unalloc_inodes++;
    bgd.d->flags |= FS_SUPER_DIRTY;
    kernel_mutex_unlock(&(bgd.d->lock));

    return 0;
}


/*
 * Allocate a new inode number and mark it as used in the disk's inode bitmap.
 *
 * This function also updates the mount info struct's free inode pool if all
 * the cached inode numbers have been used by searching the disk for free
 * inode numbers.
 *
 * Input:
 *    node => node struct in which we'll store the new alloc'd inode number
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long ext2_alloc_inode(struct fs_node_t *new_node)
{
    volatile uint8_t *bitmap;
    volatile uint32_t i, j, k, b;
    uint32_t total_inodes, inodes_per_group;
    uint32_t min_inode;
    size_t bgcount;
    long res;
    struct cached_page_t *block_bitmap;
    struct bgd_table_info_t bgd;

    if((res = get_bgd_table(new_node->dev, &bgd)) < 0)
    {
        return res;
    }

    /* no need to hustle if there are no free inodes on disk */
    if(bgd.super->unalloc_inodes == 0)
    {
        return -ENOSPC;
    }

    bgcount = get_group_count(bgd.super);
    total_inodes = bgd.super->total_inodes;
    inodes_per_group = bgd.super->inodes_per_group;
    min_inode = (bgd.super->version_major >= 1) ?
                        (bgd.super->first_nonreserved_inode ?
                            bgd.super->first_nonreserved_inode : 11) : 11;

    for(i = 0; i < bgcount; i++)
    {
        if(bgd.bgd_table[i].unalloc_inodes == 0)
        {
            continue;
        }

        if(get_block_bitmap(&bgd, &block_bitmap, new_node->dev, i, 1) < 0)
        {
            continue;
        }

        bitmap = (volatile uint8_t *)block_bitmap->virt;

        for(j = 0; j < inodes_per_group / 8; j++)
        {
            if(bitmap[j] == 0xff)
            {
                continue;
            }

            for(k = 0; k < 8; k++)
            {
                if((bitmap[j] & (1 << k)) == 0)
                {
                    b = (i * inodes_per_group) + (j * 8) + k + 1;

                    if(b >= total_inodes)
                    {
                        break;
                    }

                    /*
                     * for inodes, check the inode is not used incore,
                     * and not lower than the first non-reserved inode
                     */
                    if(node_is_incore(new_node->dev, b) || b < min_inode)
                    {
                        continue;
                    }

                    bitmap[j] |= (1 << k);
                    __sync_or_and_fetch(&block_bitmap->flags, PCACHE_FLAG_DIRTY);
                    __asm__ __volatile__("":::"memory");
                    release_cached_page(block_bitmap);

                    kernel_mutex_lock(&(bgd.d->lock));
                    bgd.bgd_table[i].unalloc_inodes--;
                    bgd.super->unalloc_inodes--;
                    bgd.d->flags |= FS_SUPER_DIRTY;
                    kernel_mutex_unlock(&(bgd.d->lock));

                    new_node->inode = b;

                    for(i = 0; i < 15; i++)
                    {
                        new_node->blocks[i] = 0;
                    }

                    return 0;
                }
            }
        }

        release_cached_page(block_bitmap);
    }

    return -ENOSPC;
}


/*
const int bit_count[256] =
{
    0,1,1,2,1,2,2,3,  1,2,2,3,2,3,3,4,  1,2,2,3,2,3,3,4,  2,3,3,4,3,4,4,5,
    1,2,2,3,2,3,3,4,  2,3,3,4,3,4,4,5,  2,3,3,4,3,4,4,5,  3,4,4,5,4,5,5,6,
    1,2,2,3,2,3,3,4,  2,3,3,4,3,4,4,5,  2,3,3,4,3,4,4,5,  3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,  3,4,4,5,4,5,5,6,  3,4,4,5,4,5,5,6,  4,5,5,6,5,6,6,7,
    1,2,2,3,2,3,3,4,  2,3,3,4,3,4,4,5,  2,3,3,4,3,4,4,5,  3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,  3,4,4,5,4,5,5,6,  3,4,4,5,4,5,5,6,  4,5,5,6,5,6,6,7,
    2,3,3,4,3,4,4,5,  3,4,4,5,4,5,5,6,  3,4,4,5,4,5,5,6,  4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,  4,5,5,6,5,6,6,7,  4,5,5,6,5,6,6,7,  5,6,6,7,6,7,7,8
};
*/


static uint32_t calc_unalloc_blocks(volatile uint8_t *bitmap, uint32_t blocks_per_group)
{
    volatile uint32_t j, count = 0;

    for(j = 0; j < blocks_per_group / 8; j++)
    {
        int i;
        __asm__ __volatile__("popcnt %1, %0" : "=r"(i) : "r"(bitmap[j]) : "cc");
        count += i;
        //count += bit_count[bitmap[j]];
    }

    if(count > blocks_per_group)
    {
        count = blocks_per_group;
    }

    return blocks_per_group - count;
}


/*
 * Free a disk block and update the disk's block bitmap.
 */
void ext2_free(dev_t dev, uint32_t block_no)
{
    volatile uint8_t *bitmap;
    volatile uint32_t index, /* index_in_group, */ group;
    struct cached_page_t *block_bitmap, *pcache;
    struct bgd_table_info_t bgd;
    struct fs_node_header_t tmpnode;

    if(get_bgd_table(dev, &bgd) < 0)
    {
        return;
    }

    if(block_no < 2 || block_no >= bgd.super->total_blocks)
    {
        return;
    }

    // Get the block bitmap
    index = block_index(bgd.super, block_no);
    group = block_group(bgd.super, block_no);
    //index_in_group = (block_no - bgd.super->superblock_block) - (group * bgd.super->blocks_per_group);

    if(get_block_bitmap(&bgd, &block_bitmap, dev, group, 0) < 0)
    {
        return;
    }

    // If this block is cached, invalidated the cache as it might end up
    // overwriting the block if it is re-allocated before the disk update
    // task runs next
    tmpnode.dev = dev;
    tmpnode.inode = PCACHE_NOINODE;

    if((pcache = get_cached_page((struct fs_node_t *)&tmpnode, block_no, 
                                    PCACHE_PEEK_ONLY | PCACHE_IGNORE_STALE)))
    {
        __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_STALE);
        release_cached_page(pcache);
    }

    bitmap = (volatile uint8_t *)block_bitmap->virt;
    bitmap[index / 8] &= ~(1 << (index % 8));
    //bitmap[index_in_group / 8] &= ~(1 << (index_in_group % 8));
    __sync_or_and_fetch(&block_bitmap->flags, PCACHE_FLAG_DIRTY);
    __asm__ __volatile__("":::"memory");
    release_cached_page(block_bitmap);

    kernel_mutex_lock(&(bgd.d->lock));
    bgd.bgd_table[group].unalloc_blocks++;
    bgd.super->unalloc_blocks++;
    bgd.d->flags |= FS_SUPER_DIRTY;

    kernel_mutex_unlock(&(bgd.d->lock));
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
uint32_t ext2_alloc(dev_t dev)
{
    volatile uint8_t *bitmap;
    volatile uint32_t i, j, k, b;
    uint32_t block_size, first_block;
    uint32_t total_blocks, blocks_per_group, inode_table_blocks;
    size_t bgcount;
    struct cached_page_t *block_bitmap;
    struct bgd_table_info_t bgd;

    if(get_bgd_table(dev, &bgd) < 0)
    {
        return 0;
    }

    /* no need to hustle if there is no free blocks on disk */
    if(bgd.super->unalloc_blocks == 0)
    {
        return 0;
    }

    bgcount = get_group_count(bgd.super);
    total_blocks = bgd.super->total_blocks;
    blocks_per_group = bgd.super->blocks_per_group;

    block_size = 1024 << bgd.super->log2_block_size;
    inode_table_blocks = bgd.super->inodes_per_group * inode_size(bgd.super);
    first_block = bgd.super->superblock_block;

    if(inode_table_blocks % block_size)
    {
        inode_table_blocks = (inode_table_blocks / block_size) + 1;
    }
    else
    {
        inode_table_blocks = (inode_table_blocks / block_size);
    }

    for(i = 0; i < bgcount; i++)
    {
        if(bgd.bgd_table[i].unalloc_blocks == 0)
        {
            continue;
        }

        if(get_block_bitmap(&bgd, &block_bitmap, dev, i, 0) < 0)
        {
            continue;
        }

        bitmap = (volatile uint8_t *)block_bitmap->virt;

        for(j = 0; j < blocks_per_group / 8; j++)
        {
            if(bitmap[j] == 0xff)
            {
                continue;
            }

            for(k = 0; k < 8; k++)
            {
                if((bitmap[j] & (1 << k)) == 0)
                {
                    b = (i * blocks_per_group) + (j * 8) + k + first_block;

                    if(b < 2)
                    {
                        continue;
                    }

                    if(b >= total_blocks)
                    {
                        break;
                    }

                    if(b == bgd.bgd_table[i].inode_bitmap_addr ||
                       b == bgd.bgd_table[i].block_bitmap_addr ||
                       (b >= bgd.bgd_table[i].inode_table_addr &&
                        b < bgd.bgd_table[i].inode_table_addr + inode_table_blocks))
                    {
                        /*
                        switch_tty(1);
                        printk("ext2: dev 0x%x, block %d\n", dev, b);
                        kpanic("ext2: unalloced block overlaps with fs metadata!");
                        */
                        continue;
                    }

                    bitmap[j] |= (1 << k);
                    __sync_or_and_fetch(&block_bitmap->flags, PCACHE_FLAG_DIRTY);
                    __asm__ __volatile__("":::"memory");
                    release_cached_page(block_bitmap);

                    kernel_mutex_lock(&(bgd.d->lock));
                    bgd.bgd_table[i].unalloc_blocks--;
                    bgd.super->unalloc_blocks--;
                    bgd.d->flags |= FS_SUPER_DIRTY;
                    kernel_mutex_unlock(&(bgd.d->lock));

                    return b;
                }
            }
        }

        release_cached_page(block_bitmap);
    }

    return 0;
}


/*
 * Helper function to convert a disk directory entry to a dirent struct.
 *
 * Inputs:
 *    ext2_ent => the directory entry on disk
 *    __ent => dirent struct to fill (if null, we'll alloc a new struct)
 *    name => the entry's name (filename)
 *    namelen => name's length
 *    off => the value to store in dirent's d_off field
 *    ext_dir_type => 1 if ent's type_indicator field contains the entry's
 *                    type, 0 otherwise
 *
 * Returns:
 *    a kmalloc'd dirent struct on success, NULL on failure
 */
struct dirent *ext2_entry_to_dirent(struct ext2_dirent_t *ext2_ent,
                                    struct dirent *__ent,
                                    char *name, int namelen, int off,
                                    int ext_dir_type)
{
    unsigned int reclen = GET_DIRENT_LEN(namelen);
    unsigned char d_type = DT_UNKNOWN;
    
    struct dirent *entry = __ent ? __ent : kmalloc(reclen);
    
    if(!entry)
    {
        return NULL;
    }
    
    if(ext_dir_type)
    {
        switch(ext2_ent->type_indicator)
        {
            case EXT2_FT_REG_FILE:
                d_type = DT_REG;
                break;
                
            case EXT2_FT_DIR:
                d_type = DT_DIR;
                break;
                
            case EXT2_FT_CHRDEV:
                d_type = DT_CHR;
                break;
                
            case EXT2_FT_BLKDEV:
                d_type = DT_BLK;
                break;
                
            case EXT2_FT_FIFO:
                d_type = DT_FIFO;
                break;
                
            case EXT2_FT_SOCK:
                d_type = DT_SOCK;
                break;
                
            case EXT2_FT_SYMLINK:
                d_type = DT_LNK;
                break;
                
            case EXT2_FT_UNKNOWN:
            default:
                d_type = DT_UNKNOWN;
                break;
        }
    }
    
    entry->d_reclen = reclen;
    entry->d_ino = ext2_ent->inode;
    entry->d_off = off;
    entry->d_type = d_type;
    
    // name might not be null-terminated
    //strncpy(entry->d_name, name, namelen);
    A_memcpy(entry->d_name, name, namelen);
    entry->d_name[namelen] = '\0';
    
    return entry;
}


static inline size_t ext2_entsz(struct ext2_dirent_t *ent, int ext_dir_type)
{
    size_t len = ent->name_length_lsb;

    if(!ext_dir_type)
    {
        len |= ((size_t)ent->type_indicator << 8);
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
long ext2_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                  struct cached_page_t **dbuf, size_t *dbuf_off)
{
    volatile struct ext2_superblock_t *super;
    struct mount_info_t *d;
    int ext_dir_type = 0;
    
    if(get_super(dir->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }

    ext_dir_type = is_ext_dir_type(super);

    return ext2_finddir_internal(dir, filename, entry,
                                 dbuf, dbuf_off, ext_dir_type);
}


long ext2_finddir_internal(struct fs_node_t *dir, char *filename,
                           struct dirent **entry, struct cached_page_t **dbuf,
                           size_t *dbuf_off, int ext_dir_type)
{
    struct cached_page_t *buf;
    struct ext2_dirent_t *ent;
    size_t len, offset = 0;
    size_t fnamelen = strlen(filename);
    unsigned char *blk, *end;
    char *n;

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    if(!fnamelen)
    {
        return -EINVAL;
    }

    if(fnamelen > NAME_MAX || fnamelen > EXT2_MAX_FILENAME_LEN)
    {
        return -ENAMETOOLONG;
    }

    while(offset < dir->size)
    {
        KDEBUG("ext2_finddir_internal: offset 0x%x, dir->size 0x%x\n", offset, dir->size);
        
        if(!(buf = get_cached_page(dir, offset, 0)))
        {
            offset += PAGE_SIZE;
            continue;
        }

        KDEBUG("ext2_finddir_internal: buf @ 0x%lx\n", buf);
        
        blk = (unsigned char *)buf->virt;

        if(offset + PAGE_SIZE > dir->size)
        {
            end = blk + (dir->size % PAGE_SIZE);
        }
        else
        {
            end = blk + PAGE_SIZE;
        }
        
        while(blk < end)
        {
            KDEBUG("ext2_finddir_internal: blk 0x%x, end 0x%x\n", blk, end);

            ent = (struct ext2_dirent_t *)blk;
            
            KDEBUG("ext2_finddir_internal: sz 0x%x, len 0x%x, ino 0x%x\n", ent->entry_size, ent->name_length_lsb, ent->inode);
            
            if(!ent->entry_size)
            {
                break;
            }
            
            len = ext2_entsz(ent, ext_dir_type);

            n = (char *)(blk + sizeof(struct ext2_dirent_t));
            
            if(ent->inode == 0 || len != fnamelen)
            {
                blk += ent->entry_size;
                continue;
            }

            if(memcmp(n, filename, len) == 0)
            {
                *entry = ext2_entry_to_dirent(ent, NULL, n, len,
                            offset + (blk - (unsigned char *)buf->virt), 0);
                *dbuf = buf;
                *dbuf_off = (size_t)(blk - buf->virt);
                return 0;
            }

            blk += ent->entry_size;
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
long ext2_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                           struct dirent **entry,
                           struct cached_page_t **dbuf, size_t *dbuf_off)
{
    volatile struct ext2_superblock_t *super;
    struct mount_info_t *d;
    int ext_dir_type = 0;
    
    if(get_super(dir->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }

    ext_dir_type = is_ext_dir_type(super);

    return ext2_finddir_by_inode_internal(dir, node, entry,
                                          dbuf, dbuf_off, ext_dir_type);
}


int matching_node(dev_t dev, ino_t ino, struct fs_node_t *node)
{
    if(ino != node->inode)
    {
        return 0;
    }
    
    // if the parent and child inodes are on the same device, return the match
    if(dev == node->dev)
    {
        return 1;
    }
    
    // check if the child is a mountpoint
    struct mount_info_t *d = get_mount_info2(node);
    
    if(d && d->mpoint == node && d->root)
    {
        return 1;
    }
    
    return 0;
}


long ext2_finddir_by_inode_internal(struct fs_node_t *dir,
                                    struct fs_node_t *node,
                                    struct dirent **entry,
                                    struct cached_page_t **dbuf,
                                    size_t *dbuf_off, int ext_dir_type)
{
    struct cached_page_t *buf;
    struct ext2_dirent_t *ent;
    size_t len, offset = 0;
    unsigned char *blk, *end;
    char *n;

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    while(offset < dir->size)
    {
        if(!(buf = get_cached_page(dir, offset, 0)))
        {
            offset += PAGE_SIZE;
            continue;
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
        
        while(blk < end)
        {
            ent = (struct ext2_dirent_t *)blk;
            
            if(!ent->entry_size)
            {
                break;
            }
            
            len = ext2_entsz(ent, ext_dir_type);

            n = (char *)(blk + sizeof(struct ext2_dirent_t));
            
            if(ent->inode == 0)
            {
                blk += ent->entry_size;
                continue;
            }


            /*
            printk("ext2_finddir_by_inode_internal: [%d] ", offset);
            for(size_t x = 0; x < len; x++)
            {
                printk("%c", n[x]);
            }
            printk(" (");
            for(size_t x = 0; x < len; x++)
            {
                printk("%x", n[x]);
            }
            printk(")\n");
            */


            if(matching_node(dir->dev, ent->inode, node))
            {
                *entry = ext2_entry_to_dirent(ent, NULL, n, len,
                            offset + (blk - (unsigned char *)buf->virt), 0);
                *dbuf = buf;
                *dbuf_off = (size_t)(blk - buf->virt);
                return 0;
            }

            blk += ent->entry_size;
        }
        
        release_cached_page(buf);
        offset += PAGE_SIZE;
    }
    
    return -ENOENT;
}


STATIC_INLINE uint8_t mode_to_ext2_type(mode_t mode)
{
    if(S_ISCHR(mode))
    {
        return EXT2_FT_CHRDEV;
    }
    else if(S_ISBLK(mode))
    {
        return EXT2_FT_BLKDEV;
    }
    else if(S_ISFIFO(mode))
    {
        return EXT2_FT_FIFO;
    }
    else if(S_ISSOCK(mode))
    {
        return EXT2_FT_SOCK;
    }
    else if(S_ISLNK(mode))
    {
        return EXT2_FT_SYMLINK;
    }
    else if(S_ISDIR(mode))
    {
        return EXT2_FT_DIR;
    }
    else if(S_ISREG(mode))
    {
        return EXT2_FT_REG_FILE;
    }
    else
    {
        return EXT2_FT_UNKNOWN;
    }
}


/*
 * Add the given file as an entry in the given parent directory.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    filename => the new file's name
 *    n => the new file's inode number
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long ext2_addir(struct fs_node_t *dir, struct fs_node_t *file, char *filename)
{
    struct bgd_table_info_t bgd;
    int res, ext_dir_type = 0;

    if(get_bgd_table(dir->dev, &bgd) < 0)
    {
        return -EINVAL;
    }
    
    ext_dir_type = is_ext_dir_type(bgd.super);

    if((res = ext2_addir_internal(dir, file, filename, ext_dir_type, bgd.d->block_size)) == 0)
    {
        /*
        if(S_ISDIR(file->mode))
        {
            uint32_t group = inode_group(bgd.super, file->inode);

            kernel_mutex_lock(&(bgd.d->lock));
            bgd.bgd_table[group].dir_count++;
            bgd.d->flags |= FS_SUPER_DIRTY;
            kernel_mutex_unlock(&(bgd.d->lock));
        }
        */
    }

    return res;
}


long ext2_addir_internal(struct fs_node_t *dir, struct fs_node_t *file,
                         char *filename, int ext_dir_type, size_t block_size)
{
    size_t sz, offset = 0;
    size_t fnamelen = strlen(filename);
    struct cached_page_t *buf;
    struct ext2_dirent_t *ent;
    int found = 0;
    size_t entsize = fnamelen + sizeof(struct ext2_dirent_t);
    size_t actual_size;
    unsigned char *blk, *end;
    char *namebuf;
    
    // adjust the entry size to make sure it is 4-byte aligned
    if(entsize & 3)
    {
        entsize = (entsize & ~3) + 4;
    }
    
    if(!fnamelen)
    {
        return -EINVAL;
    }

    if(fnamelen > NAME_MAX || fnamelen > EXT2_MAX_FILENAME_LEN)
    {
        return -ENAMETOOLONG;
    }
    
    if(dir->links >= LINK_MAX)
    {
        return -EMLINK;
    }

    while(1)
    {
        if(!(buf = get_cached_page(dir, offset, 0 /* PCACHE_AUTO_ALLOC */)))
        {
            return -EIO;
        }
        
        blk = (unsigned char *)buf->virt;
        end = blk + PAGE_SIZE;
        
        while(blk < end)
        {
            ent = (struct ext2_dirent_t *)blk;
            
            // 1 - Check if we reached the last entry in the block.
            //     We need to be careful here, as we read dirs in a PAGE_SIZE
            //     granularity, while entries should not span disk sectors, 
            //     which are very likely to be less than PAGE_SIZE in size.
            if(!ent->entry_size)
            {
                // down-align our current position to a block boundary, find
                // the end of this block, then subtract our current position
                // to find the remaining space in this block
                sz = (((size_t)blk & ~(block_size - 1)) + block_size) - 
                                                                (size_t)blk;
                //sz = end - blk;

                ent->entry_size = sz;
                
                if(sz >= entsize)
                {
                    // is there room for another entry?
                    /*
                    if(sz - entsize > sizeof(struct ext2_dirent_t))
                    {
                        ent->entry_size = entsize;
                    }
                    else
                    {
                        ent->entry_size = sz;
                    }
                    */

                    found = 1;
                }
                else
                {
                    // mark this as a deleted entry
                    ent->inode = 0;
                    __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
                }

                break;
            }
            
            // 2 - Check for deleted entries and if that entry is large enough
            //     to fit us. A corrupt (but valid still) directory might have
            //     '.' and '..' entries with 0 inode numbers. Avoid overwriting
            //     these entries.
            if(ent->inode == 0)
            {
                namebuf = (char *)((unsigned char *)ent + sizeof(struct ext2_dirent_t));

                if(namebuf[0] == '.' && 
                    (ent->name_length_lsb == 1 ||
                        (namebuf[1] == '.' && ent->name_length_lsb == 2)))
                {
                    blk += ent->entry_size;
                    continue;
                }

                if(ent->entry_size >= (fnamelen +
                                        sizeof(struct ext2_dirent_t)))
                {
                    found = 1;
                    break;
                }
            }

            // 3 - Entries at the end of a block occupy the whole space left.
            //     Check if this is the case and if we can fit ourself there.
            actual_size = sizeof(struct ext2_dirent_t) +
                                    ext2_entsz(ent, ext_dir_type);

            // adjust the entry size to make sure it is 4-byte aligned
            if(actual_size & 3)
            {
                actual_size = (actual_size & ~3) + 4;
            }
            
            if(ent->entry_size > actual_size)
            {
                // is there room for another entry?
                if(ent->entry_size - actual_size >= entsize)
                {
                    entsize = ent->entry_size - actual_size;

                    // truncate the existing entry
                    ent->entry_size = actual_size;
                    
                    // create a new entry
                    ent = (struct ext2_dirent_t *)((char *)ent + actual_size);
                    ent->entry_size = entsize;

                    found = 1;
                    break;
                }
            }

            blk += ent->entry_size;
        }
        
        if(found)
        {
            break;
        }
        
        release_cached_page(buf);
        offset += PAGE_SIZE;
    }
    
    namebuf = (char *)((unsigned char *)ent + sizeof(struct ext2_dirent_t));
    A_memcpy(namebuf, filename, fnamelen);
    ent->name_length_lsb = fnamelen;

    if(!ext_dir_type)
    {
        ent->type_indicator = (fnamelen >> 8) & 0xffff;
    }
    else
    {
        ent->type_indicator = mode_to_ext2_type(file->mode);
    }

    ent->inode = file->inode;

    // Ensure all blocks have valid empty entries until the end of the page
    for(sz = block_size; sz < PAGE_SIZE; sz += block_size)
    {
        ent = (struct ext2_dirent_t *)(buf->virt + sz);

        if(ent->entry_size == 0)
        {
            ent->inode = 0;
            ent->entry_size = block_size;
        }
    }

    dir->mtime = now();
    dir->flags |= FS_NODE_DIRTY;
    
    if(offset + PAGE_SIZE >= dir->size)
    {
        dir->size = offset + PAGE_SIZE;
        dir->ctime = dir->mtime;
    }

    __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
    release_cached_page(buf);

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
long ext2_mkdir(struct fs_node_t *dir, struct fs_node_t *parent)
{
    struct bgd_table_info_t bgd;
    int res, ext_dir_type = 0;

    if(get_bgd_table(dir->dev, &bgd) < 0)
    {
        return -EINVAL;
    }

    ext_dir_type = is_ext_dir_type(bgd.super);

    if((res = ext2_mkdir_internal(dir, parent->inode, ext_dir_type, bgd.d->block_size)) == 0)
    {
        uint32_t group = inode_group(bgd.super, dir->inode);

        kernel_mutex_lock(&(bgd.d->lock));
        bgd.bgd_table[group].dir_count++;
        bgd.d->flags |= FS_SUPER_DIRTY;
        kernel_mutex_unlock(&(bgd.d->lock));
    }

    return res;
}


long ext2_mkdir_internal(struct fs_node_t *dir, ino_t parent, 
                         int ext_dir_type, size_t block_size)
{
    size_t sz;
    char *p;
    struct cached_page_t *buf;
    volatile struct ext2_dirent_t *ent;

    dir->flags |= FS_NODE_DIRTY;
    dir->size = PAGE_SIZE;
    
    if(!(buf = get_cached_page(dir, 0, 0 /* PCACHE_AUTO_ALLOC */)))
    {
        dir->ctime = now();
        dir->flags |= FS_NODE_DIRTY;
        return -ENOSPC;
    }

    sz = sizeof(struct ext2_dirent_t);
    ent = (struct ext2_dirent_t *)buf->virt;
    ent->entry_size = sz + 4;
    ent->name_length_lsb = 1;
    ent->type_indicator = ext_dir_type ? EXT2_FT_DIR : 0;
    ent->inode = dir->inode;
    p = (char *)(buf->virt + sz);
    p[0] = '.';
    p[1] = '\0';
    
    ent = (struct ext2_dirent_t *)(buf->virt + sz + 4);
    ent->inode = parent;
    ent->entry_size = sz + 4;
    ent->name_length_lsb = 2;
    ent->type_indicator = ext_dir_type ? EXT2_FT_DIR : 0;
    p = (char *)(ent) + sz;
    p[0] = '.';
    p[1] = '.';
    p[2] = '\0';
    dir->links = 2;

    ent = (struct ext2_dirent_t *)(buf->virt + (sz * 2) + 8);
    ent->inode = 0;
    ent->entry_size = block_size - ((sz * 2) + 8);

    // We filled the first block. Now fill the other blocks until the end
    // of the page
    for(sz = block_size; sz < PAGE_SIZE; sz += block_size)
    {
        ent = (struct ext2_dirent_t *)(buf->virt + sz);
        ent->inode = 0;
        ent->entry_size = block_size;
    }

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
long ext2_deldir(struct fs_node_t *dir, struct dirent *entry, int is_dir)
{
    struct bgd_table_info_t bgd;
    uint32_t inode = entry->d_ino;
    long res;

    if(get_bgd_table(dir->dev, &bgd) < 0)
    {
        return -EINVAL;
    }

    if((res = ext2_deldir_internal(dir, entry, is_ext_dir_type(bgd.super))) < 0)
    {
        return res;
    }

    if(inode && is_dir)
    {
        uint32_t group = inode_group(bgd.super, inode);

        kernel_mutex_lock(&(bgd.d->lock));
        bgd.bgd_table[group].dir_count--;
        bgd.d->flags |= FS_SUPER_DIRTY;
        kernel_mutex_unlock(&(bgd.d->lock));
    }

    return 0;
}


long ext2_deldir_internal(struct fs_node_t *dir, struct dirent *entry, int ext_dir_type)
{
    struct dirent *entry2;
    struct cached_page_t *dbuf;
    unsigned char *blk;
    volatile struct ext2_dirent_t *ent;
    size_t dbuf_off;
    long res;

    if((res = ext2_finddir_internal(dir, entry->d_name, &entry2, &dbuf,
                                      &dbuf_off, ext_dir_type)) < 0)
    {
        return res;
    }

    blk = (unsigned char *)dbuf->virt;
    ent = (volatile struct ext2_dirent_t *)(blk + dbuf_off);
    ent->inode = 0;
    __sync_or_and_fetch(&dbuf->flags, PCACHE_FLAG_DIRTY);
    release_cached_page(dbuf);
    kfree(entry2);

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
long ext2_dir_empty(struct fs_node_t *dir)
{
    struct mount_info_t *d;
    volatile struct ext2_superblock_t *super;

    if(get_super(dir->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }

    return ext2_dir_empty_internal("ext2fs", dir);
}


long ext2_dir_empty_internal(char *module, struct fs_node_t *dir)
{
    char *p;
    unsigned char *blk, *end;
    struct cached_page_t *buf;
    struct ext2_dirent_t *ent;
    size_t offset;
    size_t sz = sizeof(struct ext2_dirent_t);
    
    if(!dir->size || !dir->blocks[0] || !(buf = get_cached_page(dir, 0, 0)))
    {
        // not ideal, but treat this as an empty directory
        printk("%s: bad directory inode at 0x%x:0x%x\n",
               module, dir->dev, dir->inode);
        return 1;
    }

    // check '.'
    ent = (struct ext2_dirent_t *)buf->virt;
    p = (char *)(buf->virt + sz);

    if(!ent->entry_size)
    {
        // not ideal, but treat this as an empty directory
        release_cached_page(buf);
        return 1;
    }

    if(ent->inode != dir->inode ||
       ent->name_length_lsb != 1 || strncmp(p, ".", 1))
    {
        release_cached_page(buf);
        printk("%s: bad directory inode at 0x%x:0x%x\n",
               module, dir->dev, dir->inode);
        return 0;
    }

    // check '..'
    ent = (struct ext2_dirent_t *)(buf->virt + ent->entry_size);
    p = (char *)(ent) + sz;

    if(!ent->entry_size)
    {
        // not ideal, but treat this as an empty directory
        release_cached_page(buf);
        return 1;
    }

    if(!ent->inode ||
       ent->name_length_lsb != 2 || strncmp(p, "..", 2))
    {
        release_cached_page(buf);
        printk("%s: bad directory inode at 0x%x:0x%x\n",
               module, dir->dev, dir->inode);
        return 0;
    }
    
    //blk = (unsigned char *)buf->virt + (size_t)ent + ent->entry_size;
    blk = (unsigned char *)ent + ent->entry_size;
    end = (unsigned char *)buf->virt + PAGE_SIZE;
    offset = 0;
    //release_cached_page(buf);

    while(offset < dir->size)
    {
        while(blk < end)
        {
            ent = (struct ext2_dirent_t *)blk;
            
            if(!ent->entry_size)
            {
                break;
            }
            
            if(ent->inode)
            {
                release_cached_page(buf);
                return 0;
            }

            blk += ent->entry_size;
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
long ext2_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz)
{
    volatile struct ext2_superblock_t *super;
    struct mount_info_t *d;
    int ext_dir_type = 0;

    if(get_super(dir->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }
    
    ext_dir_type = is_ext_dir_type(super);

    return ext2_getdents_internal(dir, pos, buf, bufsz, ext_dir_type);
}


long ext2_getdents_internal(struct fs_node_t *dir, off_t *pos, void *buf,
                            int bufsz, int ext_dir_type)
{
    size_t i, count = 0;
    size_t reclen, namelen;
    struct cached_page_t *dbuf = NULL;
    struct dirent *dent = NULL;
    struct ext2_dirent_t *ent;
    char *n, *b = (char *)buf;
    unsigned char *blk, *end;
    size_t offset;
    
    if(!dir || !pos || !buf)
    {
        return -EINVAL;
    }

    offset = (*pos) & ~(PAGE_SIZE - 1);
    i = (*pos) % PAGE_SIZE;
    
    while(offset < dir->size)
    {
        KDEBUG("ext2_getdents_internal: 0 offset 0x%x, dir->size 0x%x\n", offset, dir->size);
        
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
            KDEBUG("ext2_getdents_internal: 1 blk 0x%x, end 0x%x\n", blk, end);
            
            ent = (struct ext2_dirent_t *)blk;
            *pos = offset + (blk - (unsigned char *)dbuf->virt);
            
            // last entry in dir
            if(!ent->entry_size)
            {
                break;
            }
            
            // deleted entry - skip
            if(ent->inode == 0)
            {
                blk += ent->entry_size;
                continue;
            }

            // get filename length
            namelen = ext2_entsz(ent, ext_dir_type);
            //KDEBUG("ext2_getdents_internal: namelen 0x%x\n", namelen);

            KDEBUG("ext2_getdents_internal: namelen 0x%x\n", namelen);

            // calc dirent record length
            reclen = GET_DIRENT_LEN(namelen);

            // make it 4-byte aligned
            //ALIGN_WORD(reclen);
            KDEBUG("ext2_getdents_internal: reclen 0x%x\n", reclen);
            
            // check the buffer has enough space for this entry
            if((count + reclen) > (size_t)bufsz)
            {
                KDEBUG("ext2_getdents_internal: count 0x%x, reclen 0x%x\n", count, reclen);
                release_cached_page(dbuf);
                return count;
            }
            
            n = (char *)(blk + sizeof(struct ext2_dirent_t));
            dent = (struct dirent *)b;



            /*
            printk("ext2_getdents_internal: [%d] ", offset);
            for(size_t x = 0; x < namelen; x++)
            {
                printk("%c", n[x]);
            }
            printk("\n");
            */



            ext2_entry_to_dirent(ent, dent, n, namelen,
                                 (*pos) + ent->entry_size,
                                 ext_dir_type);

            dent->d_reclen = reclen;
            b += reclen;
            count += reclen;
            blk += ent->entry_size;
            KDEBUG("ext2_getdents_internal: 2 blk 0x%x, end 0x%x\n", blk, end);
        }
        
        release_cached_page(dbuf);
        offset += PAGE_SIZE;
    }
    
    KDEBUG("ext2_getdents_internal: count 0x%x, offset 0x%x\n", count, offset);
    
    *pos = offset;
    return count;
}


/*
 * Return filesystem statistics.
 */
long ext2_ustat(struct mount_info_t *d, struct ustat *ubuf)
{
    volatile struct ext2_superblock_t *super;

    if(!(d->super))
    {
        return -EINVAL;
    }
    
    super = (struct ext2_superblock_t *)(d->super->data);

    if(!ubuf)
    {
        return -EFAULT;
    }
    
    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_ustat() function).
     */
    ubuf->f_tfree = super->unalloc_blocks;
    ubuf->f_tinode = super->unalloc_inodes;
    
    return 0;
}


/*
 * Return detailed filesystem statistics.
 */
long ext2_statfs(struct mount_info_t *d, struct statfs *statbuf)
{
    volatile struct ext2_superblock_t *super;

    if(!(d->super))
    {
        return -EINVAL;
    }
    
    super = (struct ext2_superblock_t *)(d->super->data);

    if(!statbuf)
    {
        return -EFAULT;
    }

    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_statfs() function).
     */
    statbuf->f_type = EXT2_SUPER_MAGIC;
    statbuf->f_bsize = 1024 << super->log2_block_size;
    statbuf->f_blocks = super->total_blocks;
    statbuf->f_bfree = super->unalloc_blocks;
    statbuf->f_bavail = super->unalloc_blocks;
    statbuf->f_files = super->total_inodes;
    statbuf->f_ffree = super->unalloc_inodes;
    //statbuf->f_fsid = 0;
    statbuf->f_frsize = 0;
    statbuf->f_namelen = EXT2_MAX_FILENAME_LEN;
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
long ext2_read_symlink(struct fs_node_t *link, char *buf,
                       size_t bufsz, int kernel)
{
    off_t fpos = 0;
    size_t i;
    long res;

    /*
     * Symlinks less than 60 chars in length are stored in the inode itself.
     * See:
     *    http://www.nongnu.org/ext2-doc/ext2.html#def-symbolic-links
     */
    if(link->size < 60)
    {
        i = (bufsz < link->size) ? bufsz : link->size;

        char p[64], *p2 = p;

        A_memset(p, 0, 64);

        for(res = 0; res < 15; res++, p2 += 4)
        {
            p2[0] = link->blocks[res] & 0xff;
            p2[1] = (link->blocks[res] >> 8) & 0xff;
            p2[2] = (link->blocks[res] >> 16) & 0xff;
            p2[3] = (link->blocks[res] >> 24) & 0xff;
        }

        if(kernel)
        {
            A_memcpy(buf, p, i);
            return i;
        }
        
        res = copy_to_user(buf, p, i);

        // copy_to_user() returns 0 on success, -errno on failure
        return res ? res : (int)i;
    }

    return (int)vfs_read_node(link, &fpos, (unsigned char *)buf, bufsz, kernel);
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
size_t ext2_write_symlink(struct fs_node_t *link, char *target,
                          size_t len, int kernel)
{
    long res;
    off_t fpos = 0;

    /*
     * Symlinks less than 60 chars in length are stored in the inode itself.
     * See:
     *    http://www.nongnu.org/ext2-doc/ext2.html#def-symbolic-links
     */
    if(len < 60)
    {
        /*
         * TODO: should we raise an error here?
         */
        if(!S_ISLNK(link->mode))
        {
            link->mode &= ~S_IFMT;
            link->mode |= S_IFLNK;
        }

        char p[64], *p2 = p;

        A_memset(p, 0, 64);

        if(kernel)
        {
            A_memcpy(p, target, len);
        }
        else if((res = copy_from_user(p, target, len)) != 0)
        {
            // copy_from_user() returns 0 on success, -errno on failure
            return res;
        }

        for(res = 0; res < 15; res++, p2 += 4)
        {
            link->blocks[res] = p2[0] | (p2[1] << 8) | (p2[2] << 16) | (p2[3] << 24);
        }

        link->size = len;
        return len;
    }

    // if we are rewriting a symlink and the old link target is < 60 chars,
    // it would be stored in the inode itself, and so we need to clean this up
    if(link->size < 60)
    {
        for(res = 0; res < 15; res++)
        {
            link->blocks[res] = 0;
        }

        link->size = 0;
    }

    return vfs_write_node(link, &fpos, (unsigned char *)target, len, kernel);
}

