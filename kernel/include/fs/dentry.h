/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: dentry.h
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
 *  \file dentry.h
 *
 *  Functions and macros for working with directory entries (dentry's).
 */

#ifndef __KERNEL_DENTRY_H__
#define __KERNEL_DENTRY_H__

#include <kernel/vfs.h>
#include <kernel/mutex.h>


/**
 * @struct dentry_t
 *
 * @brief The dentry_t structure.
 *
 * A structure to represent a directory entry in the kernel's dentry cache.
 */
struct dentry_t
{
    char *path;                 /**<  dentry's path */
    int refs;                   /**<  reference count */
    //struct fs_node_t *node;
    dev_t dev;                  /**<  device id */
    ino_t inode;                /**<  inode number */
    struct dentry_t *dev_next;  /**<  next dentry in the list */
    //struct kernel_mutex_t lock;
    struct dentry_list_t *list; /**<  containing list */
};

/**
 * @struct dentry_list_t
 *
 * @brief The dentry_list_t structure.
 *
 * A structure to represent a list of directory entries, including a mutex
 * lock to regulate access to the list.
 */
struct dentry_list_t
{
    struct dentry_t *first_dentry;  /**<  first dentry in the list */
    struct kernel_mutex_t lock;     /**<  mutex lock */
};


/**
 * @brief Initialize dentries.
 *
 * Initialize the kernel's directory entry list.
 *
 * @return  nothing.
 */
void init_dentries(void);

/**
 * @brief Get the dentry of the given dir or file node.
 *
 * Get the directory entry of the given dir or file node.
 *
 * @param   dir     file node
 * @param   dent    result directory entry
 *
 * @return  zero on success, -(errno) on failure.
 */
int get_dentry(struct fs_node_t *dir, struct dentry_t **dent);

/**
 * @brief Release a dentry.
 *
 * Release the given directory entry. This function only decrements the
 * dentry's reference count. The dentry is freed elsewhere when the reference
 * count is zero and the inode is released or the device is unmounted.
 *
 * @param   dent    directory entry to release
 *
 * @return  nothing.
 */
void release_dentry(struct dentry_t *dent);

/**
 * @brief Invalidate a dentry.
 *
 * Invalidate the directory entry of the given file or directory node.
 * The dentry is freed if its reference count is zero.
 *
 * @param   dir     file or directory whose dentry to invalidate
 *
 * @return  nothing.
 *
 * @see     invalidate_dev_dentries
 */
void invalidate_dentry(struct fs_node_t *dir);

/**
 * @brief Invalidate a device's dentries.
 *
 * Invalidate the directory entries of a mounted device. This is used when
 * the device is being unmounted.
 *
 * @param   dev     device id of the device whose dentries to invalidate
 *
 * @return  nothing.
 *
 * @see     invalidate_dentry
 */
void invalidate_dev_dentries(dev_t dev);

/**
 * @brief Create a new dentry.
 *
 * Create a directory entry for the given file node under the given directory.
 *
 * @param   dir         parent directory node
 * @param   file        child file node
 * @param   filename    the name that will be concatenated to the parent
 *                        directory's path to create the full pathname of
 *                        the file
 *
 * @return  zero on success, -(errno) on failure.
 */
int create_file_dentry(struct fs_node_t *dir, struct fs_node_t *file,
                       char *filename);

/**
 * @brief Get the path of the given dir or file node.
 *
 * Get the full pathname of the given dir or file node.
 *
 * @param   dir     file node
 * @param   res     result pathname
 *
 * @return  zero on success, -(errno) on failure.
 */
int getpath(struct fs_node_t *dir, char **res);

#endif      /* __KERNEL_DENTRY_H__ */
