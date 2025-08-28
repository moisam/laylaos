/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: tmpfs.c
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
 *  \file tmpfs.c
 *
 *  This file implements tmpfs filesystem functions, which provide access to
 *  the tmpfs virtual filesystem.
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref tmpfs_ops structure.
 */

//#define __DEBUG

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/clock.h>
#include <kernel/dev.h>
#include <kernel/user.h>
#include <mm/kheap.h>
#include <fs/tmpfs.h>
#include <fs/procfs.h>
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/magic.h>
#include <fs/options.h>


// major of the devid of tmpfs
dev_t TMPFS_DEVID = 241;

// last used minor device number
volatile int last_minor = 0;

// root inode
#define TMPFS_ROOT_INO          2

// max # of tmpfs filesystems
#define NR_TMPFS                32

// number of blocks/inodes per bitmap item
#define blocks_per_item         (sizeof(size_t) * CHAR_BIT)
#define inodes_per_item         (sizeof(size_t) * CHAR_BIT)

#define max_items               65536

// defined in ext2fs.c
extern uint8_t page_of_zeroes[];


// filesystem operations
struct fs_ops_t tmpfs_ops =
{
    // inode operations
    .read_inode = tmpfs_read_inode,
    .write_inode = tmpfs_write_inode,
    //.trunc_inode = tmpfs_trunc_inode,
    .alloc_inode = tmpfs_alloc_inode,
    .free_inode = tmpfs_free_inode,
    .bmap = tmpfs_bmap,

    .read_symlink = ext2_read_symlink,
    .write_symlink = ext2_write_symlink,

    // directory operations
    .finddir = tmpfs_finddir,
    .finddir_by_inode = tmpfs_finddir_by_inode,
    //.readdir = tmpfs_readdir,
    .addir = tmpfs_addir,
    .mkdir = tmpfs_mkdir,
    .deldir = tmpfs_deldir,
    .dir_empty = tmpfs_dir_empty,
    .getdents = tmpfs_getdents,

    // device operations
    .mount = tmpfs_mount,
    .umount = NULL,
    .read_super = tmpfs_read_super,
    .write_super = NULL,
    .put_super = tmpfs_put_super,
    .ustat = tmpfs_ustat,
    .statfs = tmpfs_statfs,
};


/*
 * This array holds information about all tmpfs filesystems on the system.
 */
struct
{
    struct fs_node_t *root, *last_node;
    volatile size_t inode_count, free_inodes;
    volatile size_t block_count, free_blocks;
    size_t block_size;
    virtual_addr *blocks;
    volatile size_t *block_bitmap, *inode_bitmap;
    volatile struct kernel_mutex_t lock;
} tmpfs_dev[NR_TMPFS];


volatile struct kernel_mutex_t tmpfs_lock;

/*
 * Internal functions.
 */

static int tmpfs_options_are_valid(char *module,
                                   size_t block_count, size_t block_size,
                                   size_t max_blocks, int report_errs);

//static struct fs_node_t *tmpfs_create_fsnode(void);
static void tmpfs_free_fsnode(struct fs_node_t *node);
static size_t tmpfs_get_frames(virtual_addr *blocks, size_t count);
static void tmpfs_release_frames(virtual_addr *blocks, size_t count);
static size_t tmpfs_needed_pages(size_t block_size, size_t block_count);


/*
 * Mount a tmpfs filesystem. To use tmpfs, we need a two step process:
 *    1 - Mount tmpfs. This function will call tmpfs_create() to create a new
 *        tmpfs system, reserve memory for the virtual disk, create a virtual
 *        inode/block bitmap, and create the root inode for the new tmpfs.
 *    2 - Call tmpfs_read_super(), which will create the root node's directory
 *        by calling tmpfs_mkdir(). The tmpfs system is usable after this step.
 *
 * Inputs:
 *    d => pointer to the mount info struct on which we'll mount tmpfs
 *    flags => currently not used
 *    options => a string of options that MUST include the following comma-
 *               separated options and their values:
 *                   inode_count, block_count, block_size
 *               e.g.
 *                   "inode_count=64,block_count=16,block_size=512"
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long tmpfs_mount(struct mount_info_t *d, int flags, char *options)
{
    UNUSED(flags);

    int ops_count = 3;
    struct ops_t ops[] =
                {
                    { "inode_count", { 0 }, 1, 1, 0 },
                    { "block_count", { 0 }, 1, 1, 0 },
                    { "block_size", { 0 }, 1, 1, 0 },
                };

    // all options are required, fail if any is missing
    if(parse_options("tmpfs", options, ops, ops_count,
                     OPS_FLAG_REPORT_ERRORS) != 0)
    {
        return -EINVAL;
    }
    
    // all options are numeric, fail if any is not
    if(ops[0].is_int == 0 || ops[1].is_int == 0 || ops[2].is_int == 0)
    {
        free_option_strings(ops, ops_count);
        return -EINVAL;
    }

    struct fs_node_t *root = tmpfs_create(ops[0].val.i, ops[1].val.i, ops[2].val.i);

    free_option_strings(ops, ops_count);
    
    if(root)
    {
        d->dev = root->dev;
        return 0;
    }
    
    //KDEBUG("tmpfs_mount: error!\n");
    return -EIO;
}


/*
 * Read the filesystem's superblock and root inode.
 * This function fills in the mount info struct's block_size, super,
 * and root fields.
 */
long tmpfs_read_super(dev_t dev, struct mount_info_t *d,
                      size_t bytes_per_sector)
{
    UNUSED(bytes_per_sector);
    
    size_t min = MINOR(dev) - 1;
    long res;

    if(min >= NR_TMPFS || tmpfs_dev[min].root == NULL)
    {
        return -EINVAL;
    }
    
    kernel_mutex_lock(&tmpfs_lock);
    
    d->block_size = tmpfs_dev[min].block_size;
    d->super = NULL;
    //d->root = tmpfs_dev[min].root;
    d->root = get_node(dev, TMPFS_ROOT_INO, 0);

    kernel_mutex_unlock(&tmpfs_lock);

    if((res = tmpfs_mkdir(d->root, d->root)) < 0)
    //if((res = tmpfs_mkdir(tmpfs_dev[min].root,
    //                        tmpfs_dev[min].root->inode)) < 0)
    {
        d->root = NULL;
        return res;
    }

    return 0;
}


/*
 * Release the filesystem's superblock and its buffer.
 * For tmpfs, we also release the virtual disk's memory, as we expect no one
 * will be using them anymore after this call.
 * Called when unmounting the filesystem.
 */
void tmpfs_put_super(dev_t dev, struct superblock_t *sb)
{
    UNUSED(sb);
    size_t min = MINOR(dev) - 1;
    
    if(min >= NR_TMPFS || tmpfs_dev[min].root == NULL)
    {
        return;
    }

    kernel_mutex_lock(&tmpfs_lock);

    size_t i = tmpfs_dev[min].block_size * tmpfs_dev[min].block_count;
    size_t pages = i / PAGE_SIZE;

    tmpfs_free_fsnode(tmpfs_dev[min].root);
    tmpfs_release_frames(tmpfs_dev[min].blocks, pages);
    kfree(tmpfs_dev[min].blocks);
    kfree((void *)tmpfs_dev[min].block_bitmap);
    kfree((void *)tmpfs_dev[min].inode_bitmap);
    tmpfs_dev[min].block_bitmap = NULL;
    tmpfs_dev[min].inode_bitmap = NULL;
    tmpfs_dev[min].root = NULL;
    tmpfs_dev[min].last_node = NULL;
    tmpfs_dev[min].blocks = NULL;
    kernel_mutex_unlock(&tmpfs_lock);
}


static inline int valid_tmpfs_node(struct fs_node_t *node, size_t min)
{
    if(!node || min >= NR_TMPFS || tmpfs_dev[min].root == NULL)
    {
        return 0;
    }
    
    return 1;
}


/*
 * Reads inode data structure from disk.
 */
long tmpfs_read_inode(struct fs_node_t *node)
{
    size_t min = MINOR(node->dev) - 1;
    volatile struct fs_node_t *tmp;
    
    if(!valid_tmpfs_node(node, min))
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&tmpfs_dev[min].lock);
    
    for(tmp = tmpfs_dev[min].root; tmp; tmp = tmp->next)
    {
        if(tmp->inode == node->inode)
        {
            int j;

            node->mode = tmp->mode;
            node->uid = tmp->uid;
            node->mtime = tmp->mtime;
            node->atime = tmp->atime;
            node->ctime = tmp->ctime;
            node->size = tmp->size;
            node->links = tmp->links;
            node->gid = tmp->gid;
    
            for(j = 0; j < 15; j++)
            {
                node->blocks[j] = tmp->blocks[j];
            }

            kernel_mutex_unlock(&tmpfs_dev[min].lock);
            return 0;
        }
    }
    
    kernel_mutex_unlock(&tmpfs_dev[min].lock);
    return -ENOENT;
}


/*
 * Writes inode data structure to disk.
 */
long tmpfs_write_inode(struct fs_node_t *node)
{
    size_t min = MINOR(node->dev) - 1;
    volatile struct fs_node_t *tmp;
    
    if(!valid_tmpfs_node(node, min))
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&tmpfs_dev[min].lock);
    
    for(tmp = tmpfs_dev[min].root; tmp; tmp = tmp->next)
    {
        KDEBUG("tmpfs_write_inode: tmp 0x%lx\n", tmp);

        if(tmp->inode == node->inode)
        {
            int j;

            tmp->mode = node->mode;
            tmp->uid = node->uid;
            tmp->mtime = node->mtime;
            tmp->atime = node->atime;
            tmp->ctime = node->ctime;
            tmp->size = node->size;
            tmp->links = node->links;
            tmp->gid = node->gid;
    
            for(j = 0; j < 15; j++)
            {
                tmp->blocks[j] = node->blocks[j];
            }
            
            kernel_mutex_unlock(&tmpfs_dev[min].lock);
            return 0;
        }
    }
    
    kernel_mutex_unlock(&tmpfs_dev[min].lock);
    return -ENOENT;
}


static inline virtual_addr block_virtual_address(size_t lblock, int min)
{
    // decide how many blocks fit in one memory page
    size_t blocks_per_page = PAGE_SIZE / tmpfs_dev[min].block_size;

    // get the index to the block address array
    size_t index = ((lblock - 1) / blocks_per_page);

    // get the block's offset in the memory page
    size_t irem = ((lblock - 1) % blocks_per_page);

    // get the block's virtual address in memory
    return tmpfs_dev[min].blocks[index] + (irem * tmpfs_dev[min].block_size);
}


static void fill_zero_block(struct fs_node_t *node, size_t lblock,
                            size_t block_size)
{
    struct disk_req_t req;

    req.dev = node->dev;
    req.data = (virtual_addr)page_of_zeroes;

    //req.blocksz = block_size;
    req.datasz = block_size;
    req.fs_blocksz = block_size;

    req.blockno = lblock;
    req.write = 1;

    bdev_tab[MAJOR(node->dev)].strategy(&req);

    node->ctime = now();
    node->flags |= FS_NODE_DIRTY;
}


/*
 * Helper function called by tmpfs_bmap() to alloc a new block if needed.
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
        if((*block = tmpfs_alloc(node->dev)))
        {
            fill_zero_block(node, *block, block_size);
        }
    }
}


/*
 * Helper function called by tmpfs_bmap() to free a block if not needed anymore.
 */
STATIC_INLINE void bmap_free_block(struct fs_node_t *node, volatile uint32_t *block)
{
    tmpfs_free(node->dev, *block);
    *block = 0;
    node->ctime = now();
    node->flags |= FS_NODE_DIRTY;
}


/*
 * Check if an indirect block is empty, i.e. all pointers are zeroes.
 */
STATIC_INLINE int is_empty_block(volatile uint32_t *buf, unsigned long ptr_per_block)
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
 * Helper function called by tmpfs_bmap() to free a block if not needed anymore.
 * It also frees the single indirect block if it is empty.
 *
 * Returns:
 *    1 if the single indirect block was freed, 0 otherwise
 */
STATIC_INLINE
int bmap_may_free_iblock(struct fs_node_t *node, volatile uint32_t *iblockp,
                         volatile uint32_t *buf, uint32_t block,
                         unsigned long ptr_per_block)
{
    bmap_free_block(node, &(buf[block]));
    
    // free the single indirect block itself if it is empty
    if(is_empty_block(buf, ptr_per_block))
    {
        tmpfs_free(node->dev, *iblockp);
        *iblockp = 0;
        return 1;
    }

    return 0;
}


/*
 * Helper function called by tmpfs_bmap() to free a block if not needed anymore.
 * It also frees the single and double indirect blocks if they are empty.
 *
 * Returns:
 *    1 if the double indirect block was freed, 0 otherwise
 */
STATIC_INLINE
int bmap_may_free_diblock(struct fs_node_t *node, volatile uint32_t *iblockp,
                          volatile uint32_t *buf,
                          volatile uint32_t *buf2,
                          uint32_t block, uint32_t block2,
                          unsigned long ptr_per_block)
{
    // free the single indirect block if it is empty
    bmap_may_free_iblock(node, &(buf[block]),
                         buf2, block2, ptr_per_block);

    // free the double indirect block itself if it is empty
    if(is_empty_block(buf, ptr_per_block))
    {
        tmpfs_free(node->dev, *iblockp);
        *iblockp = 0;
        return 1;
    }

    return 0;
}


/*
 * Helper function called by tmpfs_bmap() to free a block if not needed
 * anymore. It also frees the single, double and triple indirect blocks if
 * they are empty.
 *
 * Inputs and return values are similar to the above, with additional pointers
 * to look into the triple indirect block.
 */
STATIC_INLINE
int bmap_may_free_tiblock(struct fs_node_t *node, volatile uint32_t *iblockp,
                          volatile uint32_t *buf,
                          volatile uint32_t *buf2,
                          volatile uint32_t *buf3,
                          uint32_t block, uint32_t block2, uint32_t block3,
                          unsigned long ptr_per_block)
{
    // free the single indirect block if it is empty
    bmap_may_free_diblock(node, &(buf[block]),
                         buf2, buf3, block2, block3, 
                         ptr_per_block);

    // free the double indirect block itself if it is empty
    if(is_empty_block(buf, ptr_per_block))
    {
        tmpfs_free(node->dev, *iblockp);
        *iblockp = 0;
        return 1;
    }

    return 0;
}


size_t tmpfs_bmap(struct fs_node_t *node, size_t lblock,
                  size_t block_size, int flags)
{
    volatile uint32_t *buf, *buf2, *buf3;
    unsigned long ptr_per_block = block_size / sizeof(uint32_t);
    unsigned long ptr_per_block2 = ptr_per_block * ptr_per_block;
    unsigned long maxptrs = 12 + ptr_per_block + ptr_per_block2 +
                                 (ptr_per_block2 * ptr_per_block);
    size_t i, j, k, l;
    size_t min = MINOR(node->dev) - 1;
    int create = (flags & BMAP_FLAG_CREATE);
    int free = (flags & BMAP_FLAG_FREE);
    volatile uint32_t tmp;

    if(lblock >= maxptrs)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx":::);
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

    // check direct block pointers
    if(lblock < 12)
    {
        tmp = node->blocks[lblock];
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
        // read the single indirect block
        tmp = node->blocks[12];
        bmap_may_create_block(node, &tmp, block_size, create);
        
        if(!(node->blocks[12] = tmp))
        {
            return 0;
        }

        buf = (volatile uint32_t *)block_virtual_address(node->blocks[12], min);
        
        // alloc block if needed for the new block
        bmap_may_create_block(node, &(buf[lblock]), block_size, create);
        i = buf[lblock];

        // free the block and the indirect block if we're shrinking the file
        if(free && i)
        {
            tmp = node->blocks[12];
            bmap_may_free_iblock(node, &tmp, buf, lblock, ptr_per_block);
            node->blocks[12] = tmp;
            return 0;
        }
        
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

        buf = (volatile uint32_t *)block_virtual_address(node->blocks[13], min);

        // find the single indirect block
        j = lblock / ptr_per_block;

        bmap_may_create_block(node, &(buf[j]), block_size, create);
        i = buf[j];
        
        if(!i)
        {
            return 0;
        }

        buf2 = (volatile uint32_t *)block_virtual_address(i, min);

        // find the block
        k = lblock % ptr_per_block;

        bmap_may_create_block(node, &(buf2[k]), block_size, create);
        i = buf2[k];

        // free the block and the indirect blocks if we're shrinking the file
        if(free && i)
        {
            tmp = node->blocks[13];
            bmap_may_free_diblock(node, &tmp, buf, buf2, j, k, 
                                  ptr_per_block);
            node->blocks[13] = tmp;
            return 0;
        }

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
    
    buf = (volatile uint32_t *)block_virtual_address(node->blocks[14], min);

    j = lblock / ptr_per_block2;
    bmap_may_create_block(node, &(buf[j]), block_size, create);
    i = buf[j];

    if(!i)
    {
        return 0;
    }

    buf2 = (volatile uint32_t *)block_virtual_address(i, min);

    lblock = lblock % ptr_per_block2;
    k = lblock / ptr_per_block;
    bmap_may_create_block(node, &(buf2[k]), block_size, create);
    i = buf2[k];

    if(!i)
    {
        return 0;
    }

    buf3 = (volatile uint32_t *)block_virtual_address(i, min);

    l = lblock % ptr_per_block;
    bmap_may_create_block(node, &(buf3[l]), block_size, create);
    i = buf3[l];

    // free the block and the indirect blocks if we're shrinking the file
    if(free && i)
    {
        tmp = node->blocks[14];
        bmap_may_free_tiblock(node, &tmp, buf, buf2, buf3, j, k, l, 
                              ptr_per_block);
        node->blocks[14] = tmp;
        return 0;
    }

    return i;
}


STATIC_INLINE void __tmpfs_free_inode(size_t min, uint32_t inode)
{
    if(inode > tmpfs_dev[min].inode_count || 
       tmpfs_dev[min].free_inodes >= tmpfs_dev[min].inode_count)
    {
        return;
    }
    
    inode--;
    tmpfs_dev[min].inode_bitmap[inode / inodes_per_item] &= 
                                ~((size_t)1 << (inode % inodes_per_item));
    tmpfs_dev[min].free_inodes = tmpfs_dev[min].free_inodes + 1;
}


/*
 * Free an inode and update inode bitmap on disk.
 *
 * MUST write the node to disk if the filesystem supports inode structures
 * separate to their directory entries (e.g. ext2, tmpfs).
 */
long tmpfs_free_inode(struct fs_node_t *node)
{
    size_t min = MINOR(node->dev) - 1;
    struct fs_node_t *prev, *tmp;

    // write out the node before we free it on disk
    // TODO: should we check for errors here?
    tmpfs_write_inode(node);

    if(!valid_tmpfs_node(node, min))
    {
        return -EINVAL;
    }

    // check we're not freeing the filesystem's root
    if(node->inode == tmpfs_dev[min].root->inode)
    {
        printk("tmpfs: trying to free root node!\n");
        return -EINVAL;
    }
    
    kernel_mutex_lock(&tmpfs_dev[min].lock);
    
    // get the previous node
    for(prev = tmpfs_dev[min].root; prev; prev = prev->next)
    {
        if(prev->next && prev->next->inode == node->inode)
        {
            tmp = prev->next;
            prev->next = tmp->next;
            __tmpfs_free_inode(min, node->inode);
            tmpfs_free_fsnode(tmp);

            if(tmp == tmpfs_dev[min].last_node)
            {
                tmpfs_dev[min].last_node = prev;
            }
            
            kernel_mutex_unlock(&tmpfs_dev[min].lock);
            return 0;
        }
    }
    
    kernel_mutex_unlock(&tmpfs_dev[min].lock);
    return -ENOENT;
}


STATIC_INLINE uint32_t __tmpfs_alloc_inode(size_t min)
{
    volatile size_t *inode_bitmap = tmpfs_dev[min].inode_bitmap;
    size_t bytes = tmpfs_dev[min].inode_count / inodes_per_item;
    volatile size_t i, j, k;

    if(tmpfs_dev[min].inode_count % inodes_per_item)
    {
        bytes++;
    }

    for(i = 0; i < bytes; i++)
    {
        for(j = 0, k = 1; j < inodes_per_item; j++, k <<= 1)
        {
            if((inode_bitmap[i] & k) == 0)
            {
                inode_bitmap[i] |= k;
                tmpfs_dev[min].free_inodes = tmpfs_dev[min].free_inodes - 1;
                i = (i * inodes_per_item) + j;

                return i + 1;
            }
        }
    }
    
    return 0;
}


/*
 * Allocate a new inode number and mark it as used in the disk's inode bitmap.
 */
long tmpfs_alloc_inode(struct fs_node_t *node)
{
    size_t min = MINOR(node->dev) - 1;
    struct fs_node_t *tmpnode;
    
    if(!valid_tmpfs_node(node, min))
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&tmpfs_dev[min].lock);
    
    if(tmpfs_dev[min].free_inodes == 0 || !(tmpnode = tmpfs_create_fsnode()))
    {
        kernel_mutex_unlock(&tmpfs_dev[min].lock);
        return -ENOSPC;
    }

    if(!(node->inode = __tmpfs_alloc_inode(min)))
    {
        /*
         * This should not happen as we checked the free inode count above.
         * TODO: should we kpanic here assuming the image is corrupt?
         */
        tmpfs_free_fsnode(tmpnode);
        kernel_mutex_unlock(&tmpfs_dev[min].lock);
        return -ENOSPC;
    }

    tmpfs_dev[min].last_node->next = tmpnode;
    tmpfs_dev[min].last_node = tmpnode;

    tmpnode->inode = node->inode;
    kernel_mutex_unlock(&tmpfs_dev[min].lock);

    tmpnode->dev = node->dev;
    //node->atime = t;
    //node->ctime = t;
    //node->mtime = t;
    //node->flags |= FS_NODE_DIRTY;
    
    for(int i = 0; i < 15; i++)
    {
        node->blocks[i] = tmpnode->blocks[i];
    }
    
    return 0;
}


/*
 * Helper function to allocate a new node struct and zero its memory.
 */
struct fs_node_t *tmpfs_create_fsnode(void)
{
    struct fs_node_t *node = kmalloc(sizeof(struct fs_node_t));
    
    if(!node)
    {
        return NULL;
    }
    
    A_memset(node, 0, sizeof(struct fs_node_t));
    //node->refs = 1;

    return node;
}


/*
 * Helper function to free a node struct. Currently it only calls kfree(),
 * but this might change in the future if more functionality is needed.
 */
static void tmpfs_free_fsnode(struct fs_node_t *node)
{
    kfree(node);
}


/*
 * Free a disk block and update the disk's block bitmap.
 */
void tmpfs_free(dev_t dev, uint32_t block_no)
{
    size_t min = MINOR(dev) - 1;
    volatile struct kernel_mutex_t *lock;
    struct cached_page_t *pcache;
    struct fs_node_header_t tmpnode;
    
    if(min >= NR_TMPFS || tmpfs_dev[min].root == NULL)
    {
        return;
    }

    lock = &tmpfs_dev[min].lock;
    kernel_mutex_lock(lock);

    if(block_no > tmpfs_dev[min].block_count || 
       tmpfs_dev[min].free_blocks >= tmpfs_dev[min].block_count)
    {
        kernel_mutex_unlock(lock);
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
    
    block_no--;
    tmpfs_dev[min].block_bitmap[block_no / blocks_per_item] &= 
                                ~((size_t)1 << (block_no % blocks_per_item));
    tmpfs_dev[min].free_blocks = tmpfs_dev[min].free_blocks + 1;

    kernel_mutex_unlock(lock);
}


/*
 * Allocate a new block number and mark it as used in the disk's block bitmap.
 */
uint32_t tmpfs_alloc(dev_t dev)
{
    size_t min = MINOR(dev) - 1;
    volatile size_t *block_bitmap;
    volatile struct kernel_mutex_t *lock;

    if(min >= NR_TMPFS || tmpfs_dev[min].root == NULL)
    {
        return 0;
    }

    block_bitmap = tmpfs_dev[min].block_bitmap;
    lock = &tmpfs_dev[min].lock;

    size_t bytes = tmpfs_dev[min].block_count / blocks_per_item;
    volatile size_t i, j, k;
    
    if(tmpfs_dev[min].block_count % blocks_per_item)
    {
        bytes++;
    }

    kernel_mutex_lock(lock);

    if(tmpfs_dev[min].free_blocks == 0)
    {
        kernel_mutex_unlock(lock);
        __asm__ __volatile__("xchg %%bx, %%bx":::);
        return 0;
    }

    for(i = 0; i < bytes; i++)
    {
        for(j = 0, k = 1; j < blocks_per_item; j++, k <<= 1)
        {
            if((block_bitmap[i] & k) == 0)
            {
                block_bitmap[i] |= k;
                tmpfs_dev[min].free_blocks = tmpfs_dev[min].free_blocks - 1;
                kernel_mutex_unlock(lock);
                i = (i * blocks_per_item) + j;

                return i + 1;
            }
        }
    }
    
    kernel_mutex_unlock(lock);
    return 0;
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
long tmpfs_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                   struct cached_page_t **dbuf, size_t *dbuf_off)
{
    size_t min = MINOR(dir->dev) - 1;
    
    if(!valid_tmpfs_node(dir, min))
    {
        return -EINVAL;
    }

    return ext2_finddir_internal(dir, filename, entry,
                                 dbuf, dbuf_off, 0);
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
long tmpfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                            struct dirent **entry,
                            struct cached_page_t **dbuf, size_t *dbuf_off)
{
    size_t min = MINOR(dir->dev) - 1;
    
    if(!valid_tmpfs_node(dir, min))
    {
        return -EINVAL;
    }

    return ext2_finddir_by_inode_internal(dir, node, entry,
                                          dbuf, dbuf_off, 0);
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
long tmpfs_addir(struct fs_node_t *dir, struct fs_node_t *file, char *filename)
{
    size_t min = MINOR(dir->dev) - 1;
    
    if(!valid_tmpfs_node(dir, min))
    {
        return -EINVAL;
    }

    return ext2_addir_internal(dir, file, filename, 0, tmpfs_dev[min].block_size);
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
long tmpfs_mkdir(struct fs_node_t *dir, struct fs_node_t *parent)
{
    size_t min = MINOR(dir->dev) - 1;
    
    if(!valid_tmpfs_node(dir, min))
    {
        return -EINVAL;
    }

    return ext2_mkdir_internal(dir, parent->inode, 0, tmpfs_dev[min].block_size);
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
long tmpfs_deldir(struct fs_node_t *dir, struct dirent *entry, int is_dir)
{
    UNUSED(is_dir);
    return ext2_deldir_internal(dir, entry, 0);
}


/*
 * Check if the given directory is empty (called from rmdir).
 */
long tmpfs_dir_empty(struct fs_node_t *dir)
{
    size_t min = MINOR(dir->dev) - 1;
    
    if(!valid_tmpfs_node(dir, min))
    {
        return -EINVAL;
    }

    return ext2_dir_empty_internal("tmpfs", dir);
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
long tmpfs_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz)
{
    size_t min = MINOR(dir->dev) - 1;
    
    if(!valid_tmpfs_node(dir, min))
    {
        return -EINVAL;
    }

    return ext2_getdents_internal(dir, pos, buf, bufsz, 0);
}


/*
 * General Block Read/Write Operations.
 */
long tmpfs_strategy(struct disk_req_t *buf)
{
    size_t min = MINOR(buf->dev) - 1;
    size_t block_size;
    long done = 0;

    if(min >= NR_TMPFS || tmpfs_dev[min].root == NULL)
    {
        return 0;
    }

    if(!buf || !tmpfs_dev[min].blocks || !tmpfs_dev[min].block_size)
    {
        return 0;
    }

    block_size = tmpfs_dev[min].block_size;

    // find out how many "sectors" to read, as the page cache always tries
    // to fill a whole page, which would contain multiple "sectors", unless
    // the tmpfs filesystem was formatted with a blocksize that equals the
    // system's pagesize (currently 4096 bytes)
    volatile int sectors_to_read = buf->datasz / block_size;
    uint32_t sect = buf->blockno;
    virtual_addr addr, userbuf = buf->data;

    kernel_mutex_lock(&tmpfs_dev[min].lock);

    while(sectors_to_read--)
    {
        addr = block_virtual_address(sect, min);

        // now copy the data
        if(buf->write)
        {
            A_memcpy((void *)addr, (void *)userbuf, block_size);
        }
        else
        {
            A_memcpy((void *)userbuf, (void *)addr, block_size);
        }

        userbuf += block_size;
        done += block_size;
        sect++;
    }

    kernel_mutex_unlock(&tmpfs_dev[min].lock);

    return done;
}


/*
 * General block device control function.
 */
long tmpfs_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel)
{
    size_t min = MINOR(dev) - 1;
    
    if(min >= NR_TMPFS || tmpfs_dev[min].root == NULL)
    {
        return -EINVAL;
    }
    
    switch(cmd)
    {
        case BLKSSZGET:
            // get the block size in bytes
            RETURN_IOCTL_RES(int, arg, tmpfs_dev[min].block_size, kernel);

        case BLKGETSIZE:
        {
            // get disk size in 512-blocks
            long sects = (long)((tmpfs_dev[min].block_count * tmpfs_dev[min].block_size) / 512);
            RETURN_IOCTL_RES(long, arg, sects, kernel);
        }

        case BLKGETSIZE64:
        {
            // get disk size in bytes
            unsigned long long bytes = tmpfs_dev[min].block_count * tmpfs_dev[min].block_size;
            RETURN_IOCTL_RES(unsigned long long, arg, bytes, kernel);
        }
    }
    
    return -EINVAL;
}


/*
 * Initialize tmpfs.
 *
 * This function does the following:
 *    - registers the tmpfs filesystem
 *    - adds a "tmpfs" device to the kernel's block device list (we don't
 *      actually add the device, we just set function pointers so that VFS
 *      knows who to call for read/write operations)
 */
void tmpfs_init(void)
{
    fs_register("tmpfs", &tmpfs_ops);
    bdev_tab[TMPFS_DEVID].strategy = tmpfs_strategy;
    bdev_tab[TMPFS_DEVID].ioctl = tmpfs_ioctl;
    bdev_tab[TMPFS_DEVID].select = devfs_select;
}


/*
 * Helper function to alloc memory pages for use by a tmpfs device.
 *
 * Inputs:
 *    blocks => array to hold memory page addresses
 *    count => number of required memory pages (i.e. size of the blocks array)
 *
 * Output:
 *    blocks => the addresses of alloc'd memory pages are stored in this array
 *
 * Returns:
 *    number of alloc'd memory pages
 */
static size_t tmpfs_get_frames(virtual_addr *__blocks, size_t __count)
{
    volatile size_t i = 0;
    volatile virtual_addr *blocks = __blocks;
    volatile size_t count = __count;
    volatile virtual_addr addr, end = TMPFS_END;
    
    for(addr = TMPFS_START; addr < end; addr += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)addr);
        
        if(PTE_FRAME(*pt) == 0)
        {
            if(!vmmngr_alloc_page(pt, PTE_FLAGS_PW))
            {
                break;
            }

            vmmngr_flush_tlb_entry(addr);
            blocks[i++] = addr;
            A_memset((void *)addr, 0, PAGE_SIZE);
            
            if(i == count)
            {
                break;
            }
        }
    }
    
    return i;
}


/*
 * Helper function to free memory pages used by a tmpfs device.
 *
 * Inputs:
 *    blocks => array holding memory page addresses
 *    count => size of the blocks array
 *
 * Returns:
 *    none
 */
static void tmpfs_release_frames(virtual_addr *blocks, size_t count)
{
    size_t i;
    
    for(i = 0; i < count; i++)
    {
        if(blocks[i] == 0)
        {
            continue;
        }
        
        pt_entry *pt = get_page_entry((void *)blocks[i]);
        *pt = 0;
        vmmngr_flush_tlb_entry(blocks[i]);
        blocks[i] = 0;
    }
}


static size_t tmpfs_needed_pages(size_t block_size, size_t block_count)
{
    size_t pages, i;

    // calc how many pages we need to reserve for the tmpfs disk
    i = block_size * block_count;
    pages = i / PAGE_SIZE;
    
    // ensure we get full blocks
    if(i % PAGE_SIZE)
    {
        pages++;
    }
    
    return pages;
}


static int tmpfs_options_are_valid(char *module,
                                   size_t block_count, size_t block_size,
                                   size_t max_blocks, int report_errs)
{
    // block_count must fit into our bitmap
    if(block_count > max_blocks)
    {
        if(report_errs)
        {
            printk("%s: block_count > max (%d)\n", module, max_blocks);
        }
        return 0;
    }
    
    // block_count must be a multiple of 8 (or whatever char size is)
    if(block_count % CHAR_BIT)
    {
        if(report_errs)
        {
            printk("%s: block_size is not a multiple of %d\n",
                   module, CHAR_BIT);
        }
        return 0;
    }

    // block_size must be <= PAGE_SIZE
    if(block_size > PAGE_SIZE)
    {
        if(report_errs)
        {
            printk("%s: block_size > PAGE_SIZE (%d)\n", module, PAGE_SIZE);
        }
        return 0;
    }

    // block_size must be 512, 1024, 2048 or 4096
    if(block_size != 512 && block_size != 1024 &&
       block_size != 2048 && block_size != 4096)
    {
        if(report_errs)
        {
            printk("%s: block_size must be 512, 1024, 2048 or 4096\n", module);
        }
        return 0;
    }

    return 1;
}


/*
 * Create a new tmpfs system (or virtual device) which can be mounted.
 *
 * Inputs:
 *    inode_count => total number of inodes the tmpfs can have
 *    block_count => total number of blocks the tmpfs can have
 *    block_size => size of each block (in bytes)
 *
 * Returns:
 *    pointer to the newly created tmpfs's root node on success,
 *    NULL on failure
 */
struct fs_node_t *tmpfs_create(size_t inode_count, size_t block_count,
                               size_t block_size)
{

#define max_blocks              (max_items * blocks_per_item)

    struct fs_node_t *root;
    size_t pages, i;
    size_t *block_bitmap, *inode_bitmap;
    size_t block_bitmap_sz, inode_bitmap_sz;

    /*
     * TODO: we need to reset last_minor if we've reached the maximum number
     *       of tmpfs devices if some of them were released.
     */
    if(last_minor >= NR_TMPFS)
    {
        printk("tmpfs: maximum tmpfs systems reached!\n");
        return NULL;
    }

    if(!tmpfs_options_are_valid("tmpfs", block_count, block_size,
                                         max_blocks, 1))
    {
        return NULL;
    }

    // create root node
    if(!(root = tmpfs_create_fsnode()))
    {
        printk("tmpfs: failed to create root node for tmpfs!\n");
        return NULL;
    }

    block_bitmap_sz = ((block_count + blocks_per_item - 1) / 
                                    blocks_per_item) * sizeof(size_t);

    inode_bitmap_sz = ((inode_count + inodes_per_item - 1) / 
                                    inodes_per_item) * sizeof(size_t);

    if(!(block_bitmap = kmalloc(block_bitmap_sz)))
    {
        tmpfs_free_fsnode(root);
        printk("tmpfs: failed to alloc block bitmap for tmpfs!\n");
        return NULL;
    }

    if(!(inode_bitmap = kmalloc(inode_bitmap_sz)))
    {
        tmpfs_free_fsnode(root);
        kfree(block_bitmap);
        printk("tmpfs: failed to alloc inode bitmap for tmpfs!\n");
        return NULL;
    }

    root->inode = TMPFS_ROOT_INO;
    root->ops = &tmpfs_ops;
    //root->mode = S_IFDIR | 0555;
    root->mode = S_IFDIR | 0777;
    root->links = 2;
    root->atime = now();
    root->mtime = root->atime;
    root->ctime = root->atime;
    
    kernel_mutex_lock(&tmpfs_lock);

    // use one of the reserved dev ids
    root->dev = TO_DEVID(TMPFS_DEVID, last_minor + 1);
    
    // set up the tmp filesystem
    tmpfs_dev[last_minor].root = root;
    tmpfs_dev[last_minor].last_node = root;
    tmpfs_dev[last_minor].inode_count = inode_count;
    tmpfs_dev[last_minor].free_inodes = inode_count - 3;  // -3 for inodes 0-2
    tmpfs_dev[last_minor].block_count = block_count;
    tmpfs_dev[last_minor].free_blocks = block_count;
    tmpfs_dev[last_minor].block_size = block_size;
    //tmpfs_dev[last_minor].last_inode = 3;
    tmpfs_dev[last_minor].block_bitmap = block_bitmap;
    tmpfs_dev[last_minor].inode_bitmap = inode_bitmap;

    // zero out the bitmaps
    A_memset((void *)tmpfs_dev[last_minor].block_bitmap, 0, block_bitmap_sz);
    A_memset((void *)tmpfs_dev[last_minor].inode_bitmap, 0, inode_bitmap_sz);

    // mark inodes 0-2 as used
    tmpfs_dev[last_minor].inode_bitmap[0] = 7;

    pages = tmpfs_needed_pages(block_size, block_count);
    i = sizeof(virtual_addr) * pages;
    tmpfs_dev[last_minor].blocks = (virtual_addr *)kmalloc(i);
    
    if(tmpfs_dev[last_minor].blocks == NULL)
    {
        tmpfs_free_fsnode(root);
        kfree(block_bitmap);
        kfree(inode_bitmap);
        tmpfs_dev[last_minor].block_bitmap = NULL;
        tmpfs_dev[last_minor].inode_bitmap = NULL;
        tmpfs_dev[last_minor].root = NULL;
        tmpfs_dev[last_minor].last_node = NULL;
        kernel_mutex_unlock(&tmpfs_lock);
        return NULL;
    }
    
    A_memset(tmpfs_dev[last_minor].blocks, 0, i);

    // alloc mem pages for the tmp filesystem
    // then create the file structure of the root dir
    // note that the root node's parent is the root node itself
    if(tmpfs_get_frames(tmpfs_dev[last_minor].blocks, pages) != pages
    /* || tmpfs_mkdir(root, 2) < 0 */)
    {
        tmpfs_free_fsnode(root);
        tmpfs_release_frames(tmpfs_dev[last_minor].blocks, pages);
        kfree(tmpfs_dev[last_minor].blocks);
        kfree(block_bitmap);
        kfree(inode_bitmap);
        tmpfs_dev[last_minor].block_bitmap = NULL;
        tmpfs_dev[last_minor].inode_bitmap = NULL;
        tmpfs_dev[last_minor].root = NULL;
        tmpfs_dev[last_minor].last_node = NULL;
        tmpfs_dev[last_minor].blocks = NULL;
        kernel_mutex_unlock(&tmpfs_lock);
        return NULL;
    }

    last_minor++;
    kernel_mutex_unlock(&tmpfs_lock);
    
    return root;
}


/*
 * Return filesystem statistics.
 */
long tmpfs_ustat(struct mount_info_t *d, struct ustat *ubuf)
{
    size_t min = MINOR(d->dev) - 1;
    
    if(min >= NR_TMPFS || tmpfs_dev[min].root == NULL || !ubuf)
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
    ubuf->f_tfree = tmpfs_dev[min].free_blocks;
    ubuf->f_tinode = tmpfs_dev[min].free_inodes;

    return 0;
}


/*
 * Return detailed filesystem statistics.
 */
long tmpfs_statfs(struct mount_info_t *d, struct statfs *statbuf)
{
    size_t min = MINOR(d->dev) - 1;
    
    if(min >= NR_TMPFS || tmpfs_dev[min].root == NULL)
    {
        return -EINVAL;
    }

    if(!statbuf)
    {
        return -EFAULT;
    }

    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_statfs() function).
     */
    statbuf->f_type = TMPFS_MAGIC;
    statbuf->f_bsize = tmpfs_dev[min].block_size;
    statbuf->f_blocks = tmpfs_dev[min].block_count;
    statbuf->f_bfree = tmpfs_dev[min].free_blocks;
    statbuf->f_bavail = tmpfs_dev[min].free_blocks;
    statbuf->f_files = tmpfs_dev[min].inode_count;
    statbuf->f_ffree = tmpfs_dev[min].free_inodes;
    //statbuf->f_fsid = 0;
    statbuf->f_frsize = 0;
    statbuf->f_namelen = EXT2_MAX_FILENAME_LEN;
    statbuf->f_flags = d->mountflags;

    return 0;
}

