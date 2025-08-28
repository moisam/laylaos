/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: pcache-defs.h
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
 *  \file pcache-defs.h
 *
 *  Page-cache-related structure and macro definitions.
 */

#ifndef __KERNEL_PCACHE_DEFS_H__
#define __KERNEL_PCACHE_DEFS_H__

#include <mm/mmngr_virtual.h>

#define PCACHE_NOINODE              (0)

// values for the flags field of struct cached_page_t
#define PCACHE_FLAG_DIRTY           0x01
#define PCACHE_FLAG_WANTED          0x02
#define PCACHE_FLAG_BUSY            0x04
#define PCACHE_FLAG_ALWAYS_DIRTY    0x08
#define PCACHE_FLAG_STALE           0x10

// values for the flags parameter of function get_cached_page()
#define PCACHE_AUTO_ALLOC           0x01
#define PCACHE_PEEK_ONLY            0x02
#define PCACHE_IGNORE_STALE         0x04

#define ONE_MINUTE                  (1 * 60 * PIT_FREQUENCY)
#define TWO_MINUTES                 (2 * 60 * PIT_FREQUENCY)
#define THREE_MINUTES               (3 * 60 * PIT_FREQUENCY)
#define FIVE_MINUTES                (5 * 60 * PIT_FREQUENCY)

/**
 * @struct cached_page_t
 * @brief The cached_page_t structure.
 *
 * A structure to represent a cached page.
 */
struct cached_page_t
{
    dev_t dev;          /**< device id */
    ino_t ino;          /**< inode number, zero if this page is not part of an 
                             inode, i.e. a disk metadata block */
    off_t offset;       /**< page offset in file */
    struct fs_node_t *node; /**< pointer to inode struct, NULL if ino == 0 */
    //int refs;           /**< number of references to this page */
    virtual_addr virt;  /**< virtual address to which page is loaded */
    physical_addr phys; /**< physical address to which page is loaded */
    int len;            /**< number of bytes in cached page */
    int flags;          /**< cache flags */
    pid_t pid;          /**< last task to access the page */
    unsigned long long last_accessed;   /**< last access time in ticks */
    struct cached_page_t *next; /**< next page in cache list */
};

/**
 * @struct disk_req_t
 * @brief The disk_req_t structure.
 *
 * A structure to represent a disk I/O request.
 */
struct disk_req_t
{
    dev_t dev;          /**< device id */
    virtual_addr data;  /**< virtual address to which page is read/written */
    unsigned long datasz;       /**< buffer size */
	//unsigned long blocksz;     /**< buffer size */
	unsigned long blockno;     /**< LBA address of disk block to read
	                                or write */
    unsigned long fs_blocksz;   /**< filesystem (logical) block size */
    int write;          /**< 1 = write op; 0 = read op */
};

/**
 * @struct pcache_key_t
 * @brief The pcache_key_t structure.
 *
 * A structure to represent a cached page key. Used to find cached pages
 * in the cache hashtable.
 */
struct pcache_key_t
{
    dev_t dev;          /**< device id */
    ino_t ino;          /**< inode number */
    off_t offset;       /**< page offset in file */
};

/**
 * @var pcachetab_lock
 * @brief pcache lock.
 *
 * A lock used to regulate access to the page cache.
 */
extern volatile struct kernel_mutex_t pcachetab_lock;

#endif      /* __KERNEL_PCACHE_DEFS_H__ */
