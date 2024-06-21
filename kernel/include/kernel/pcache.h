/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: pcache.h
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
 *  \file pcache.h
 *
 *  Functions to work with the page cache.
 */

#ifndef __KERNEL_PCACHE_H__
#define __KERNEL_PCACHE_H__

//#include <kernel/vfs.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <kernel/bits/vfs-defs.h>
#include <kernel/bits/pcache-defs.h>


/**
 * \def NODEV
 *
 * Device id that does not identify any device
 */
#ifndef NODEV
#define NODEV                   (dev_t)(-1)
#endif


/***********************
 * Function prototypes
 ***********************/

/**
 * @brief Initialise page cache.
 *
 * Initialise the page cache. Called once during boot.
 *
 * @return  nothing.
 */
void init_pcache(void);

/**
 * @brief Sync cached page.
 *
 * Synchronise the given cached page by writing it to disk.
 *
 * @param   pcache      page to sync
 *
 * @return  zero on success, -(errno) on failure.
 */
int sync_cached_page(struct cached_page_t *pcache);

/**
 * @brief Release cached page.
 *
 * Release the cached page so that other tasks can access it.
 *
 * @param   pcache      page to release
 *
 * @return  nothing.
 */
void release_cached_page(struct cached_page_t *pcache);

/**
 * @brief Get cached page.
 *
 * If the requested page (starting at the given \a offset in the file \a node)
 * is present in the page cache, it is returned. Otherwise, the page is loaded
 * to cache and returned to the caller.
 *
 * @param   node        file node
 * @param   offset      offset in file (must be page-aligned)
 * @param   flags       page cache flags
 *
 * @return  loaded page.
 */
struct cached_page_t *get_cached_page(struct fs_node_t *node, 
                                      off_t offset, int flags);

/**
 * @brief Free cached page.
 *
 * Release the cached page identified with the given \a pkey and free its 
 * memory page.
 *
 * @param   pkey        hash key
 * @param   pcache      page to release
 *
 * @return  nothing.
 */
void free_cached_page(struct pcache_key_t *pkey, struct cached_page_t *pcache);

/**
 * @brief Flush cached pages.
 *
 * Synchronise the cached pages from the given device by writing them to disk.
 *
 * @param   dev         device whose pages to sync
 *
 * @return  nothing.
 */
void flush_cached_pages(dev_t dev);

/**
 * @brief Remove cached disk pages.
 *
 * Remove the cached pages from the given device from cache.
 *
 * @param   dev         device whose pages to remove
 *
 * @return  zero on success, -(errno) on failure.
 */
int remove_cached_disk_pages(dev_t dev);

/**
 * @brief Remove cached node pages.
 *
 * Remove the cached pages from the given node from cache.
 *
 * @param   node        file node
 *
 * @return  zero on success, -(errno) on failure.
 */
int remove_cached_node_pages(struct fs_node_t *node);

/**
 * @brief Get cached page count.
 *
 * Return the count of all the cached pages from all the open files on 
 * the system.
 * That is, get all cached page count minus the value returned by
 * get_cached_block_count().
 *
 * @return  number of cached pages.
 *
 * @see     get_cached_block_count()
 */
size_t get_cached_page_count(void);

/**
 * @brief Get cached page count.
 *
 * Return the count of all the cached disk blocks with no file backing.
 * That is, get all cached page count minus the value returned by
 * get_cached_page_count().
 *
 * @return  number of cached pages.
 *
 * @see     get_cached_page_count()
 */
size_t get_cached_block_count(void);

#endif      /* __KERNEL_PCACHE_H__ */
