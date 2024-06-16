/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
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


#define FREE_BLOCKS         1
#define SET_BLOCKS          2
#define FREE_INODE          1
#define SET_INODE           2

static inline int is_empty_block(uint32_t *buf, unsigned long ptr_per_block);

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
inline int is_ext_dir_type(volatile struct ext2_superblock_t *super)
{
    return (super->version_major >= 1 &&
            (super->required_features & EXT2_FEATURE_INCOMPAT_FILETYPE));
}


/*
 * Read the filesystem's superblock and root inode.
 * This function fills in the mount info struct's block_size, super,
 * and root fields.
 */
int ext2_read_super(dev_t dev, struct mount_info_t *d, size_t bytes_per_sector)
{
    struct superblock_t *super;
    struct ext2_superblock_t *psuper;
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

    KDEBUG("ext2_read_super: super->data 0x%lx\n", super->data);
    A_memset((void *)super->data, 0, PAGE_SIZE);
    KDEBUG("ext2_read_super: pid %d, super->data 0x%lx\n", cur_task ? cur_task->pid : -1, super->data);

    /* Superblock is 1024 bytes long located at 1024 bytes from start */
    if(bytes_per_sector == 512)
    {
        // we use block 1 instead of 2 as we pass a block size of 1024, which will
        // cause the device strategy function to divide 1024 by 512, leading to the
        // wrong block offset
        super->blockno = 1;
        super->blocksz = 1024;
    }
    else if(bytes_per_sector == 1024)
    {
        super->blockno = 1;
        super->blocksz = 1024;
    }
    else if(bytes_per_sector == 2048 || bytes_per_sector == 4096)
    {
        super->blockno = 0;
        super->blocksz = bytes_per_sector;
    }
    else
    {
        printk("ext2fs: unknown disk block size: 0x%x\n", bytes_per_sector);
        kpanic("Failed to read ext2 superblock!\n");
    }

    super->dev = dev;
    req.dev = dev;
    req.data = super->data;

    //req.blocksz = super->blocksz;
    req.datasz = super->blocksz;
    req.fs_blocksz = super->blocksz;

    req.blockno = super->blockno;
    req.write = 0;

    printk("ext2: reading superblock\n");

#define BAIL_OUT(err)   \
        vmmngr_free_page(get_page_entry((void *)super->data));  \
        vmmngr_flush_tlb_entry(super->data);    \
        kfree(super);   \
        return err;

    if(bdev_tab[maj].strategy(&req) < 0)
    {
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

    printk("ext2: superblock signature 0x%x\n", psuper->signature);
    printk("      total_inodes %x, total_blocks %x, reserved_blocks %x\n",
            psuper->total_inodes, psuper->total_blocks, psuper->reserved_blocks);
    printk("      unalloc_blocks %x, unalloc_inodes %x, superblock_block %x\n",
            psuper->unalloc_blocks, psuper->unalloc_inodes, psuper->superblock_block);
    printk("      log2_block_size %x\n", psuper->log2_block_size);

    /* check boot sector signature */
    if(psuper->signature != EXT2_SUPER_MAGIC)
    {
        BAIL_OUT(-EINVAL);
    }

#undef BAIL_OUT
  
    /* inode size */
    if(psuper->version_major < 1)
    {
        psuper->inode_size = 128;
        psuper->first_nonreserved_inode = 11;
    }
    
    d->block_size = 1024 << psuper->log2_block_size;
    d->super = super;
    d->root = get_node(dev, EXT2_ROOT_INO, 0);

    return 0;
}


/*
 * Write the filesystem's superblock to disk.
 */
int ext2_write_super(dev_t dev, struct superblock_t *super)
{
    UNUSED(dev);
    int res;
    struct disk_req_t req;
    
    if(!super)
    {
        return -EINVAL;
    }

    req.dev = dev;
    req.data = super->data;

    //req.blocksz = super->blocksz;
    req.datasz = super->blocksz;
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

        res = bdev_tab[MAJOR(super->dev)].strategy(&req);

        A_memcpy((void *)buf, (void *)super->data, 1024);
        A_memcpy((void *)super->data, (void *)(super->data + 1024), 1024);
        A_memcpy((void *)(super->data + 1024), (void *)buf, 1024);

        kfree(buf);
    }
    else
    {
        res = bdev_tab[MAJOR(super->dev)].strategy(&req);
    }

    return (res < 0) ? -EIO : 0;
}


/*
 * Release the filesystem's superblock and its buffer.
 * Called when unmounting the filesystem.
 */
void ext2_put_super(dev_t dev, struct superblock_t *super)
{
    UNUSED(dev);

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
}


/*
 * Helper function that returns the filesystem's superblock struct.
 */
static inline int get_super(dev_t dev, struct mount_info_t **d,
                                       struct ext2_superblock_t **super)
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
 *    dev      => device id
 *    n        => number of the inode/block for which we want to find the
 *                  group descriptor table
 *    is_inode => 1 if n is an inode number, 0 if it is a block number
 *
 * Output:
 *    index       => index of the inode entry in the table
 *    block_group => pointer to the block group
 *    list_offset => offset of the inode entry in the block group descriptor
 *                     table
 *    super       => pointer to the superblock
 *    bgd_table   => pointer to the buffer containing the block group
 *                     descriptor table
 *
 * Returns:
 *    0 on success, -errno on failure
 */
struct bgd_table_info_t
{
    dev_t dev;
    uint32_t n;
    int is_inode;
    uint32_t index;
    uint32_t block_group;
    struct ext2_superblock_t *super;
    struct cached_page_t *bgd_table;
    //size_t cached_page_offset;
};

static inline int get_bgd_table(struct bgd_table_info_t *bgd)
{
    struct mount_info_t *d;
    struct fs_node_header_t tmp;
    uint32_t block_size, bgd_block, off, blockno;
    uint32_t list_offset;
    
    if(get_super(bgd->dev, &d, &bgd->super) < 0)
    {
        return -EINVAL;
    }

    block_size = 1024 << bgd->super->log2_block_size;
    bgd_block = (block_size <= 1024) ? 2 : 1;
    
    /* read directory at requested inode/block */
    if(bgd->is_inode)
    {
        bgd->block_group = (bgd->n - 1) / bgd->super->inodes_per_group;
        bgd->index = (bgd->n - 1) % bgd->super->inodes_per_group;

        //printk("get_bgd_table: n 0x%x, block_group 0x%x, ipg 0x%x\n", bgd->n, bgd->block_group, bgd->super->inodes_per_group);
    }
    else
    {
        bgd->block_group = bgd->n / bgd->super->blocks_per_group;
        bgd->index = bgd->n % bgd->super->blocks_per_group;

        //printk("get_bgd_table: n 0x%x, block_group 0x%x, bpg 0x%x\n", bgd->n, bgd->block_group, bgd->super->blocks_per_group);
    }
    
    off = bgd->block_group * sizeof(struct block_group_desc_t);
    list_offset = off / block_size;
    blockno = bgd_block + list_offset;

    if(list_offset)
    {
        bgd->block_group = (off % block_size) / sizeof(struct block_group_desc_t);
    }
    
    /* read the block group descriptor table */
    tmp.inode = PCACHE_NOINODE;
    tmp.dev = bgd->dev;

    //printk("get_bgd_table: blockno 0x%x, off 0x%x\n", blockno, off);
    //__asm__("xchg %%bx, %%bx"::);

    if(!(bgd->bgd_table = get_cached_page((struct fs_node_t *)&tmp, blockno, 0)))
    {
        return -EIO;
    }
    
    return 0;
}


/*
 * Helper function to read a block table.
 *
 * Input & Output similar to the above. Additionally:
 *    inode => if not null, pointer to the searched inode struct
 *    block_table => pointer to the buffer containing the block table
 *
 * Returns:
 *    0 on success, -errno on failure
 */
int get_block_table(struct bgd_table_info_t *bgd,
                    struct inode_data_t **inode,
                    struct cached_page_t **block_table)
{
    struct block_group_desc_t *table;
    struct fs_node_header_t tmp;
    uint32_t block_size, table_block, off0, off1;
    int res;

    bgd->is_inode = 1;
    
    if((res = get_bgd_table(bgd)) < 0)
    {
        return res;
    }
    
    table = (struct block_group_desc_t *)bgd->bgd_table->virt;
    block_size = 1024 << bgd->super->log2_block_size;
    table_block = table[bgd->block_group].inode_table_addr;
    off0 = (bgd->index * bgd->super->inode_size);
    off1 = off0 / block_size;

    table_block += off1;

    tmp.inode = PCACHE_NOINODE;
    tmp.dev = bgd->dev;

    //printk("get_block_table: table_block 0x%x, inode_size 0x%x\n", table_block, bgd->super->inode_size);

    if(table_block == 0)
    {
        printk("ext2: invalid table_block 0x%x (in get_block_table())\n", table_block);
        kpanic("Invalid/corrupt disk\n");
    }

    if(!(*block_table = get_cached_page((struct fs_node_t *)&tmp, table_block, 0)))
    {
        release_cached_page(bgd->bgd_table);
        return -EIO;
    }

    if(inode)
    {
        uint32_t off2 = off0 % block_size;

        *inode = (struct inode_data_t *)((*block_table)->virt + off2);
    }
    
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
                     struct cached_page_t **block_bitmap)
{
    struct block_group_desc_t *table;
    struct fs_node_header_t tmp;
    uint32_t table_block;
    int res;
    
    if((res = get_bgd_table(bgd)) < 0)
    {
        return res;
    }
    
    table = (struct block_group_desc_t *)bgd->bgd_table->virt;
    table_block = bgd->is_inode ? table[bgd->block_group].inode_bitmap_addr :
                                  table[bgd->block_group].block_bitmap_addr;
    
    tmp.inode = PCACHE_NOINODE;
    tmp.dev = bgd->dev;

    if(!(*block_bitmap = get_cached_page((struct fs_node_t *)&tmp, table_block, 0)))
    {
        release_cached_page(bgd->bgd_table);
        return -EIO;
    }
    
    return 0;
}


/*
 * Reads inode data structure from disk.
 */
int ext2_read_inode(struct fs_node_t *node)
{
    struct cached_page_t *block_table;
    struct inode_data_t *inode;
    struct bgd_table_info_t bgd;
    int res;

    bgd.dev = node->dev;
    bgd.n = node->inode;

    if((res = get_block_table(&bgd, &inode, &block_table)) < 0)
    {
        return res;
    }

    inode_to_incore(node, inode);
    release_cached_page(bgd.bgd_table);
    release_cached_page(block_table);

    return 0;
}


/*
 * Writes inode data structure to disk.
 */
int ext2_write_inode(struct fs_node_t *node)
{
    struct cached_page_t *block_table;
    struct inode_data_t *inode;
    struct bgd_table_info_t bgd;
    int res;
    
    bgd.dev = node->dev;
    bgd.n = node->inode;

    if((res = get_block_table(&bgd, &inode, &block_table)) < 0)
    {
        return res;
    }
    
    incore_to_inode(inode, node);

    release_cached_page(bgd.bgd_table);
    release_cached_page(block_table);

    return 0;
}


/*
 * Helper function called by ext2_bmap() to alloc a new block if needed.
 *
 * Returns 1 if a new block was allocated, 0 otherwise. This is so that the
 * caller can zero out the new block before use.
 */
static inline void bmap_may_create_block(struct fs_node_t *node,
                                         uint32_t *block, uint32_t block_size,
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
            
            node->ctime = now();
            node->flags |= FS_NODE_DIRTY;
        }
    }
}


/*
 * Helper function called by ext2_bmap() to free a block if not needed anymore.
 */
inline void bmap_free_block(struct fs_node_t *node, uint32_t *block)
{
    ext2_free(node->dev, *block);
    *block = 0;
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
int bmap_may_free_iblock(struct fs_node_t *node, uint32_t *iblockp,
                         struct cached_page_t *pcache, uint32_t block,
                         unsigned long ptr_per_block)
{
    bmap_free_block(node, &(((uint32_t *)pcache->virt)[block]));
    
    // free the single indirect block itself if it is empty
    if(is_empty_block((uint32_t *)pcache->virt, ptr_per_block))
    {
        release_cached_page(pcache);
        ext2_free(node->dev, *iblockp);
        *iblockp = 0;
        return 1;
    }

    release_cached_page(pcache);
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
int bmap_may_free_diblock(struct fs_node_t *node, uint32_t *iblockp,
                          struct cached_page_t *pcache,
                          struct cached_page_t *pcache2,
                          uint32_t block, uint32_t block2,
                          unsigned long ptr_per_block)
{
    // free the single indirect block if it is empty
    bmap_may_free_iblock(node, &(((uint32_t *)pcache->virt)[block]),
                         pcache2, block2, ptr_per_block);

    // free the double indirect block itself if it is empty
    if(is_empty_block((uint32_t *)pcache->virt, ptr_per_block))
    {
        release_cached_page(pcache);
        ext2_free(node->dev, *iblockp);
        *iblockp = 0;
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
int bmap_may_free_tiblock(struct fs_node_t *node, uint32_t *iblockp,
                          struct cached_page_t *pcache,
                          struct cached_page_t *pcache2,
                          struct cached_page_t *pcache3,
                          uint32_t block, uint32_t block2, uint32_t block3,
                          unsigned long ptr_per_block)
{
    // free the single indirect block if it is empty
    bmap_may_free_diblock(node, &(((uint32_t *)pcache->virt)[block]),
                         pcache2, pcache3, block2, block3, ptr_per_block);

    // free the double indirect block itself if it is empty
    if(is_empty_block((uint32_t *)pcache->virt, ptr_per_block))
    {
        release_cached_page(pcache);
        ext2_free(node->dev, *iblockp);
        *iblockp = 0;
        return 1;
    }

    release_cached_page(pcache);
    return 0;
}


/*
 * Check if an indirect block is empty, i.e. all pointers are zeroes.
 */
static inline int is_empty_block(uint32_t *buf, unsigned long ptr_per_block)
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
    
    if(lblock >= maxptrs)
    {
        return 0;
    }

    tmpnode.dev = node->dev;
    tmpnode.inode = PCACHE_NOINODE;
    
    // check direct block pointers
    if(lblock < 12)
    {
        uint32_t tmp = node->blocks[lblock];

        bmap_may_create_block(node, &tmp, block_size, create);
        node->blocks[lblock] = tmp;

        // free block if we're shrinking the file
        if(free && node->blocks[lblock])
        {
            bmap_free_block(node, &tmp);
            node->blocks[lblock] = tmp;
        }

        return node->blocks[lblock];
    }
    
    // check single indirect block pointer
    lblock -= 12;

    if(lblock < ptr_per_block)
    {
        uint32_t tmp = node->blocks[12];

        // read the single indirect block
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
        
        // free the block and the indirect block if we're shrinking the file
        if(free && i)
        {
            tmp = node->blocks[12];
            bmap_may_free_iblock(node, &tmp, buf, lblock, ptr_per_block);
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
        uint32_t tmp = node->blocks[13];

        // read the double indirect block
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

        // free the block and the indirect blocks if we're shrinking the file
        if(free && i)
        {
            tmp = node->blocks[13];
            bmap_may_free_diblock(node, &tmp, buf, buf2, j, k, ptr_per_block);
            node->blocks[13] = tmp;
            return 0;
        }

        release_cached_page(buf);
        release_cached_page(buf2);

        return i;
    }

    // check triple indirect block pointer
    lblock -= ptr_per_block2;

    uint32_t tmp = node->blocks[14];
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

    // free the block and the indirect blocks if we're shrinking the file
    if(free && i)
    {
        uint32_t tmp = node->blocks[14];
        bmap_may_free_tiblock(node, &tmp, buf, buf2, buf3, j, k, l, ptr_per_block);
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
 */
int ext2_free_inode(struct fs_node_t *node)
{
    struct mount_info_t *d;
    struct ext2_superblock_t *super;
    
    if(get_super(node->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }
    
    if(node->inode < 1 || node->inode > super->total_inodes)
    {
        return -EINVAL;
    }
    
    uint32_t tmp[] = { node->inode, 0 };

    update_inode_bitmap(1, tmp, node->dev, FREE_INODE);
    super->unalloc_inodes++;

    /* add to the free inode cache pool */
    kernel_mutex_lock(&d->ilock);

    if(d->ninode < NR_FREE_CACHE)
    {
        d->inode[d->ninode++] = node->inode;
    }

    kernel_mutex_unlock(&d->ilock);

    d->flags |= FS_SUPER_DIRTY;
    //d->update_time = now();

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
int ext2_alloc_inode(struct fs_node_t *new_node)
{
    ino_t inode_no = 0;
    int i;
    struct mount_info_t *d;
    struct ext2_superblock_t *super;
    volatile int ninode;
    
    if(get_super(new_node->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }

    /* no need to hustle if there are no free inodes on disk */
    if(super->unalloc_inodes == 0)
    {
        d->ninode = 0;
        return -ENOSPC;
    }
    
loop:

    kernel_mutex_lock(&d->ilock);
    ninode = d->ninode;

    if(ninode > 0)
    {
        inode_no = d->inode[--d->ninode];
        new_node->inode = inode_no;
        kernel_mutex_unlock(&d->ilock);
        
        for(i = 0; i < 15; i++)
        {
            new_node->blocks[i] = 0;
        }

        /* update the block bitmap to indicate the inode is now occupied */
        uint32_t tmp[] = { inode_no, 0 };

        if((i = update_inode_bitmap(1, tmp, new_node->dev, SET_INODE)) != 0)
        {
            kernel_mutex_lock(&d->ilock);
            d->ninode++;
            new_node->inode = 0;
            kernel_mutex_unlock(&d->ilock);

            printk("ext2: failed to update inode bitmap on dev 0x%x\n",
                   new_node->dev);
            //__asm__ __volatile__("xchg %%bx, %%bx"::);

            return i;
        }
        
        super->unalloc_inodes--;
        kernel_mutex_lock(&d->ilock);
    }

    /* replenish the free inode cache pool */
    if(ninode <= 0)
    {
        /* NOTE: the idea is to read NR_FREE_CACHE new entries of the
         *       free inode list on disk and save it incore.
         */
        A_memset((void *)&d->inode, 0, sizeof(ino_t) * NR_FREE_CACHE);

        if((d->ninode = find_free_inodes(new_node->dev,
                                         NR_FREE_CACHE, d->inode)) != 0)
        {
            if(inode_no == 0)
            {
                kernel_mutex_unlock(&d->ilock);
                goto loop;
            }
        }
        else if(inode_no == 0)
        {
            //d->ninode = 0;
            kernel_mutex_unlock(&d->ilock);
            return -ENOSPC;
        }
    }

    kernel_mutex_unlock(&d->ilock);

    d->flags |= FS_SUPER_DIRTY;
    //d->update_time = now();

    return 0;
}


/*
 * Free a disk block and update the disk's block bitmap.
 */
void ext2_free(dev_t dev, uint32_t block_no)
{
    uint32_t tmp[] = { block_no, 0 };
    struct mount_info_t *d;
    struct ext2_superblock_t *super;

    if(get_super(dev, &d, &super) < 0)
    {
        return;
    }

    if(block_no < 2 || block_no >= super->total_blocks)
    {
        return;
    }

    update_block_bitmap(1, tmp, dev, FREE_BLOCKS);
    super->unalloc_blocks++;

    /* add to the free block cache pool */
    kernel_mutex_lock(&d->flock);

    if(d->nfree < NR_FREE_CACHE)
    {
        d->free[d->nfree++] = block_no;
    }

    kernel_mutex_unlock(&d->flock);

    d->flags |= FS_SUPER_DIRTY;
    //d->update_time = now();
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
    struct ext2_superblock_t *super;
    struct mount_info_t *d;
    uint32_t block_no;

    if(get_super(dev, &d, &super) < 0)
    {
        return 0;
    }

    /* no need to hustle if there is no free blocks on disk */
    if(super->unalloc_blocks == 0)
    {
        d->nfree = 0;
        return 0;
    }
    
loop:

    kernel_mutex_lock(&d->flock);

    do
    {
        block_no = 0;

        if(d->nfree <= 0)
        {
            break;
        }

        block_no = d->free[--d->nfree];
    } while(block_no < 2 || block_no >= super->total_blocks);

    kernel_mutex_unlock(&d->flock);
    
    if(block_no)
    {
        /*
         * Update the block bitmap to indicate the block is now occupied.
         * We use a temporary array as update_block_bitmap modifies the
         * passed array.
         */
        uint32_t tmp[] = { block_no, 0 };

        update_block_bitmap(1, tmp, dev, SET_BLOCKS);
        super->unalloc_blocks--;
    }

    kernel_mutex_lock(&d->flock);

    if(d->nfree <= 0)
    {
        /* NOTE: the idea is to read NR_FREE_CACHE new entries of the
         *       free block list on disk and save it incore.
         */
        A_memset((void *)&d->free, 0, sizeof(ino_t) * NR_FREE_CACHE);

        if((d->nfree = find_free_blocks(dev, NR_FREE_CACHE, d->free)) != 0)
        {
            if(block_no == 0)
            {
                kernel_mutex_unlock(&d->flock);
                goto loop;
            }
        }
        else if(block_no == 0)
        {
            //d->nfree = 0;
            kernel_mutex_unlock(&d->flock);
            return 0;
        }
    }

    kernel_mutex_unlock(&d->flock);

    d->flags |= FS_SUPER_DIRTY;
    //d->update_time = now();

    return block_no;
}


/*
 * Find free blocks or inodes on the filesystem.
 *
 * Inputs:
 *    dev => device id
 *    n => count of needed blocks or inodes
 *    is_inode => 1 if we want inodes, 0 if we want blocks
 *
 * Output:
 *    arr => block or inode numbers are stored in this array, which should
 *           be large enough to hold n items
 *
 * Returns:
 *    number of found blocks/inodes, 0 on failure
 */
int find_free(dev_t dev, int n, ino_t *arr, int is_inode)
{
    int found = 0;
    uint32_t i, j;
    uint32_t block = is_inode ? 1 : 0;
    uint32_t nitems, titems, base;
    uint8_t *bitmap;
    struct cached_page_t *block_bitmap;
    struct bgd_table_info_t bgd;

    bgd.dev = dev;
    bgd.is_inode = is_inode;
    
loop:

    bgd.n = block;

    if(get_block_bitmap(&bgd, &block_bitmap) < 0)
    {
        return found;
    }

    /* search for a free inode */
    bitmap = (uint8_t *)block_bitmap->virt;
    
    if(is_inode)
    {
        nitems = bgd.super->inodes_per_group;
        titems = bgd.super->total_inodes;
        base = (bgd.block_group * nitems) + 1;
    }
    else
    {
        nitems = bgd.super->blocks_per_group;
        titems = bgd.super->total_blocks;
        base = (bgd.block_group * nitems);
    }

    for(i = 0; i < nitems / 8; i++)
    {
        if(bitmap[i] == 0xff)
        {
            continue;
        }

        for(j = 0; j < 8; j++)
        {
            if((bitmap[i] & (1 << j)) == 0)
            {
                uint32_t free_block = (i * 8) + j + base;
                
                /* if looking for inodes, check it's not used incore */
                if(is_inode)
                {
                    if(node_is_incore(dev, free_block))
                    {
                        continue;
                    }
                }
                
                arr[found++] = (ino_t)free_block;

                if(found == n)
                {
                    break;
                }
            }
        }
        
        if(found == n)
        {
            break;
        }
    }
    
    release_cached_page(bgd.bgd_table);
    release_cached_page(block_bitmap);
    
    if(found < n)
    {
        block += nitems;

        if(block < titems)
        {
            goto loop;
        }
    }

    return found;
}


int find_free_blocks(dev_t dev, int n, ino_t *needed_blocks)
{
    return find_free(dev, n, needed_blocks, 0);
}


int find_free_inodes(dev_t dev, int n, ino_t *needed_inodes)
{
    return find_free(dev, n, needed_inodes, 1);
}


/*
 * Update block or inode bitmap on disk.
 *
 * Inputs:
 *    n => count of blocks or inodes to update on disk
 *    arr => block or inode numbers which we want to update on disk
 *    dev => device id
 *    op => operation to perform: 1 to free inodes/blocks, 2 to set them
 *    is_inode => 1 if we are working on inodes, 0 for blocks
 *
 * Output:
 *    arr => updated inode/block numbers are zeroed out as they are updated,
 *           so the caller shouldn't rely on this array's contents after this
 *           call!
 *
 * Returns:
 *    0 on success, -1 on failure
 */
int update_bitmap(int n, uint32_t *arr, dev_t dev,
                  char op, int is_inode)
{
    int i = 0, j;
    uint32_t nitems, block_group2;
    uint8_t *bitmap;
    struct block_group_desc_t *table;
    struct cached_page_t *block_bitmap;
    struct bgd_table_info_t bgd;

    bgd.dev = dev;
    bgd.is_inode = is_inode;
    
loop:

    bgd.n = arr[i];

    if(get_block_bitmap(&bgd, &block_bitmap) < 0)
    {
        return -1;
    }

    table = (struct block_group_desc_t *)bgd.bgd_table->virt;
    bitmap = (uint8_t *)block_bitmap->virt;
    nitems = is_inode ? bgd.super->inodes_per_group :
                        bgd.super->blocks_per_group;

    for(i = 0; i < n; i++)
    {
        if(arr[i] == 0)
        {
            continue;
        }
        
        block_group2 = (arr[i] - !!is_inode) / nitems;

        if(block_group2 != bgd.block_group)
        {
            continue;
        }
        
        j = bgd.block_group * nitems;
        bgd.index = arr[i] - j - !!is_inode;
        arr[i] = 0;

        if(op == SET_BLOCKS)        /* mark as used */
        {
            bitmap[bgd.index / 8] |=  (1 << (bgd.index % 8));
            
            if(is_inode)
            {
                table[bgd.block_group].unalloc_inodes--;
            }
            else
            {
                table[bgd.block_group].unalloc_blocks--;
            }
        }
        else                        /* mark as free/unused */
        {
            bitmap[bgd.index / 8] &= ~(1 << (bgd.index % 8));

            if(is_inode)
            {
                table[bgd.block_group].unalloc_inodes++;
            }
            else
            {
                table[bgd.block_group].unalloc_blocks++;
            }
        }
    }

    release_cached_page(bgd.bgd_table);
    release_cached_page(block_bitmap);

    for(i = 0; i < n; i++)
    {
        if(arr[i] != 0)
        {
            goto loop;
        }
    }
    
    return 0;
}


int update_block_bitmap(int n, uint32_t *arr, dev_t dev, char op)
{
    return update_bitmap(n, arr, dev, op, 0);
}


int update_inode_bitmap(int n, uint32_t *arr, dev_t dev, char op)
{
    return update_bitmap(n, arr, dev, op, 1);
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
    unsigned short reclen = sizeof(struct dirent) + namelen + 1;
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
int ext2_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                 struct cached_page_t **dbuf, size_t *dbuf_off)
{
    struct ext2_superblock_t *super;
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


int ext2_finddir_internal(struct fs_node_t *dir, char *filename,
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
int ext2_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                          struct dirent **entry,
                          struct cached_page_t **dbuf, size_t *dbuf_off)
{
    struct ext2_superblock_t *super;
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


int ext2_finddir_by_inode_internal(struct fs_node_t *dir,
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
int ext2_addir(struct fs_node_t *dir, char *filename, ino_t n)
{
    struct ext2_superblock_t *super;
    struct mount_info_t *d;
    int ext_dir_type = 0;

    if(get_super(dir->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }
    
    ext_dir_type = is_ext_dir_type(super);

    return ext2_addir_internal(dir, filename, n, ext_dir_type, d->block_size);
}


int ext2_addir_internal(struct fs_node_t *dir, char *filename,
                        ino_t n, int ext_dir_type, size_t block_size)
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
        if(!(buf = get_cached_page(dir, offset, PCACHE_AUTO_ALLOC)))
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
                }

                break;
            }
            
            // 2 - Check for deleted entries and if that entry is large enough
            //     to fit us
            if(ent->inode == 0)
            {
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
        ent->type_indicator = 0;
    }
    
    ent->inode = n;

    dir->mtime = now();
    dir->flags |= FS_NODE_DIRTY;
    
    if(offset + PAGE_SIZE >= dir->size)
    {
        dir->size = offset + PAGE_SIZE;
        dir->ctime = dir->mtime;
    }

    release_cached_page(buf);

    return 0;
}


/*
 * Make a new, empty directory by allocating a free block and initializing
 * the '.' and '..' entries to point to the current and parent directory
 * inodes, respectively.
 *
 * Input:
 *    parent => the parent directory's inode number
 *    dir => node struct containing the new directory's inode number
 *
 * Output:
 *    dir => the directory's link count and block[0] will be updated
 *
 * Returns:
 *    0 on success, -errno on failure
 */
int ext2_mkdir(struct fs_node_t *dir, ino_t parent)
{
    struct mount_info_t *d;
    struct ext2_superblock_t *super;

    if(get_super(dir->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }

    return ext2_mkdir_internal(dir, parent);
}


int ext2_mkdir_internal(struct fs_node_t *dir, ino_t parent)
{
    size_t sz;
    char *p;
    struct cached_page_t *buf;
    struct ext2_dirent_t *ent;
    //time_t t = now();

    dir->flags |= FS_NODE_DIRTY;
    dir->size = PAGE_SIZE;
    //dir->mtime = t;
    //dir->atime = t;
    
    if(!(buf = get_cached_page(dir, 0, PCACHE_AUTO_ALLOC)))
    {
        dir->ctime = now();
        dir->flags |= FS_NODE_DIRTY;
        return -ENOSPC;
    }

    sz = sizeof(struct ext2_dirent_t);
    ent = (struct ext2_dirent_t *)buf->virt;
    ent->entry_size = sz + 4;
    ent->name_length_lsb = 1;
    ent->type_indicator = 0;
    ent->inode = dir->inode;
    p = (char *)(buf->virt + sz);
    p[0] = '.';
    p[1] = '\0';
    
    ent = (struct ext2_dirent_t *)(buf->virt + sz + 4);
    ent->inode = parent;
    ent->entry_size = sz + 4;
    ent->name_length_lsb = 2;
    ent->type_indicator = 0;
    p = (char *)(ent) + sz;
    p[0] = '.';
    p[1] = '.';
    p[2] = '\0';
    dir->links = 2;

    ent = (struct ext2_dirent_t *)(buf->virt + (sz * 2) + 8);
    ent->inode = 0;
    ent->entry_size = 0;

    release_cached_page(buf);

    return 0;
}


/*
 * Remove an entry from the given parent directory.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    entry => the entry to be removed
 *    dbuf => the disk buffer representing the disk block containing the entry
 *            we want to remove (we get this from an earlier call to finddir)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                entry to be removed
 *
 * Output:
 *    the caller is responsible for writing dbuf to disk and calling brelse
 *
 * Returns:
 *    0 always
 */
int ext2_deldir(struct fs_node_t *dir, struct dirent *entry,
                struct cached_page_t *dbuf, size_t dbuf_off)
{
    UNUSED(dir);
    UNUSED(entry);
    unsigned char *blk = (unsigned char *)dbuf->virt;
    struct ext2_dirent_t *ent = (struct ext2_dirent_t *)(blk + dbuf_off);
    ent->inode = 0;
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
int ext2_dir_empty(struct fs_node_t *dir)
{
    struct mount_info_t *d;
    struct ext2_superblock_t *super;

    if(get_super(dir->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }

    return ext2_dir_empty_internal("ext2fs", dir);
}


int ext2_dir_empty_internal(char *module, struct fs_node_t *dir)
{
    char *p;
    unsigned char *blk, *end;
    struct cached_page_t *buf;
    struct ext2_dirent_t *ent;
    size_t offset;
    size_t sz = sizeof(struct ext2_dirent_t);
    
    if(!dir->size || !dir->blocks[0] || !(buf = get_cached_page(dir, 0, 0)))
    {
        printk("%s: bad directory inode at 0x%x:0x%x\n",
               module, dir->dev, dir->inode);
        return 0;
    }

    // check '.'
    ent = (struct ext2_dirent_t *)buf->virt;
    p = (char *)(buf->virt + sz);
    
    if(!ent->entry_size || ent->inode != dir->inode ||
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

    if(!ent->entry_size || !ent->inode ||
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

    release_cached_page(buf);

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
int ext2_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz)
{
    struct ext2_superblock_t *super;
    struct mount_info_t *d;
    int ext_dir_type = 0;

    if(get_super(dir->dev, &d, &super) < 0)
    {
        return -EINVAL;
    }
    
    ext_dir_type = is_ext_dir_type(super);

    return ext2_getdents_internal(dir, pos, buf, bufsz, ext_dir_type);
}


int ext2_getdents_internal(struct fs_node_t *dir, off_t *pos, void *buf,
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
            reclen = sizeof(struct dirent) + namelen + 1;

            // make it 4-byte aligned
            ALIGN_WORD(reclen);
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



            ext2_entry_to_dirent(ent, (struct dirent *)dent, n, namelen,
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
int ext2_ustat(struct mount_info_t *d, struct ustat *ubuf)
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
int ext2_statfs(struct mount_info_t *d, struct statfs *statbuf)
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
int ext2_read_symlink(struct fs_node_t *link, char *buf,
                      size_t bufsz, int kernel)
{
    off_t fpos = 0;
    size_t i;
    int res;
    
    /*
     * Symlinks less than 60 chars in length are stored in the inode itself.
     * See:
     *    http://www.nongnu.org/ext2-doc/ext2.html#def-symbolic-links
     */
    if(link->size <= 60)
    {
        i = (bufsz < link->size) ? bufsz : link->size;
        
#ifdef __x86_64__

        char p[60];
        
        for(res = 0; res < 15; res++)
        {
            ((uint32_t *)p)[res] = (uint32_t)link->blocks[res];
        }

#else

        char *p = (char *)&(link->blocks[0]);

#endif

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
    int res;
    off_t fpos = 0;

    /*
     * Symlinks less than 60 chars in length are stored in the inode itself.
     * See:
     *    http://www.nongnu.org/ext2-doc/ext2.html#def-symbolic-links
     */
    if(len <= 60)
    {

        char p[60];
        
        A_memset(p, 0, 60);

        if(kernel)
        {
            A_memcpy(p, target, len);
        }
        else if((res = copy_from_user(p, target, len)) != 0)
        {
            // copy_from_user() returns 0 on success, -errno on failure
            return res;
        }

        for(res = 0; res < 15; res++)
        {
            link->blocks[res] = ((uint32_t *)p)[res];
        }

        link->size = len;
        return len;
    }

    return vfs_write_node(link, &fpos, (unsigned char *)target, len, kernel);
}

