/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: devfs.h
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
 *  \file devfs.h
 *
 *  Functions and macros for working with the devfs filesystem.
 */

#ifndef __DEV_FSYS_H__
#define __DEV_FSYS_H__

#include <stdint.h>
#include <sys/types.h>
#include <kernel/vfs.h>
#include <kernel/pcache.h>

/**
 * @struct devnode_t
 * @brief The devnode_t structure.
 *
 * This structure is used internally by the devfs filesystem to represent
 * device nodes.
 */
struct devnode_t
{
    char name[8];           /**<  device name */
    dev_t dev;              /**<  device id number */
    ino_t inode;            /**<  inode number */
    mode_t mode;            /**<  access mode */
    uid_t uid;              /**<  owner's uid */
    gid_t gid;              /**<  owner's gid */
    struct devnode_t *next; /**<  next device in the list */
};

/**
 * @var dev_list
 * @brief Head of the device list.
 *
 * This variable points to the first device on the internal device list.
 */
extern struct devnode_t *dev_list;

/**
 * @var last_dev
 * @brief Tail of the device list.
 *
 * This variable points to the last device on the internal device list.
 */
extern struct devnode_t *last_dev;

/**
 * @var devfs_ops
 * @brief devfs filesystem ops.
 *
 * The devfs filesystem operations structure.
 */
extern struct fs_ops_t devfs_ops;

/**
 * @brief Create the devfs virtual filesystem.
 *
 * Create the devfs virtual filesystem. This function should be called once,
 * on system startup. It calls dev_init() to initialize the kernel's device
 * list.
 *
 * @return  root node of the created devfs
 *
 * @see     dev_init
 */
struct fs_node_t *devfs_create(void);

/**
 * @brief Initialize the devfs virtual filesystem.
 *
 * Initialize the devfs virtual filesystem. This function calls fs_register()
 * to registers the devfs filesystem in the kernel's recognized filesystem
 * list.
 *
 * @return  nothing.
 *
 * @see     fs_register
 */
void devfs_init(void);

#include <fs/dummy.h>
#define devfs_select        dummyfs_select
#define devfs_poll          dummyfs_poll

/**
 * @brief Read devfs superblock and root inode.
 *
 * Read the devfs filesystem's superblock and root inode. This function fills
 * in the mount info struct's \a block_size, \a super, and \a root fields.
 *
 * @param   dev                 device id
 * @param   d                   pointer to the mount info struct on which
 *                                we'll mount \a dev
 * @param   bytes_per_sector    bytes per sector
 *
 * @return  zero on success, -(errno) on failure.
 */
int devfs_read_super(dev_t dev, struct mount_info_t *d,
                     size_t bytes_per_sector);

/**
 * @brief Release the filesystem's superblock and its buffer.
 *
 * Release the devfs filesystem's superblock and its buffer. This function is
 * called when unmounting the devfs filesystem.
 *
 * @param   dev device id
 * @param   sb  pointer to the buffer containing the superblock's data
 *
 * @return  nothing.
 */
void devfs_put_super(dev_t dev, struct superblock_t *sb);

/**
 * @brief Read inode data structure.
 *
 * Read inode data structure from the devfs virtual filesystem.
 *
 * @param   node    node struct whose inode field tells us which inode to read
 *
 * @return  zero on success, -(errno) on failure.
 */
int devfs_read_inode(struct fs_node_t *);

/**
 * @brief Write inode data structure.
 *
 * Write inode data structure to the devfs virtual filesystem.
 *
 * @param   node    node struct to write
 *
 * @return  zero on success, -(errno) on failure.
 */
int devfs_write_inode(struct fs_node_t *);

/**
 * @brief Find the given filename in the parent directory.
 *
 * Find the given \a filename in the parent directory, represented by \a dir.
 * The resultant dirent is returned in \a entry, the disk block containing the
 * dirent is returned in \a dbuf, and the entry's offset in \a dbuf is
 * returned in \a dbuf_off.
 *
 * @param   dir         the parent directory's node
 * @param   filename    the searched-for filename
 * @param   entry       if the \a filename is found, its entry is converted to
 *                        a kmalloc'd dirent struct (by calling
 *                        entry_to_dirent()), and the result is
 *                        stored in this field
 * @param   dbuf        the disk buffer representing the disk block containing
 *                        the found \a filename, this is useful if the caller
 *                        wants to delete the file after finding it
 *                        (vfs_unlink(), for example)
 * @param   dbuf_off    the offset in dbuf->data at which the caller can find
 *                        the file's entry
 *
 * @return  zero on success, -(errno) on failure.
 */
int devfs_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                  struct cached_page_t **dbuf, size_t *dbuf_off);
                  //struct IO_buffer_s **dbuf, size_t *dbuf_off);

/**
 * @brief Find the given inode in the parent directory.
 *
 * Find the given \a node in the parent directory, represented by \a dir.
 * This function is called during pathname resolution when constructing the
 * absolute pathname of a given inode.
 *
 * The resultant dirent is returned in \a entry, the disk block containing the
 * dirent is returned in \a dbuf, and the entry's offset in \a dbuf is
 * returned in \a dbuf_off.
 *
 * @param   dir         the parent directory's node
 * @param   node        the searched-for inode
 * @param   entry       if the \a node is found, its entry is converted to a
 *                        kmalloc'd dirent struct (by calling
 *                        entry_to_dirent()), and the result is stored in
 *                        this field
 * @param   dbuf        the disk buffer representing the disk block containing
 *                        the found file, this is useful if the caller wants to
 *                        delete the file after finding it (vfs_unlink(), 
 *                        for example)
 * @param   dbuf_off    the offset in dbuf->data at which the caller can find
 *                        the file's entry
 *
 * @return  zero on success, -(errno) on failure.
 */
int devfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                           struct dirent **entry,
                           struct cached_page_t **dbuf, size_t *dbuf_off);
                           //struct IO_buffer_s **dbuf, size_t *dbuf_off);

/**
 * @brief Get dir entries.
 *
 * Get the file entries of the given \a dir, including '.' and '..'.
 *
 * @param   dir     node of dir to read from
 * @param   pos     byte position to start reading entries from
 * @param   buf     buffer in which to store dir entries
 * @param   bufsz   max number of bytes to read (i.e. size of \a buf)
 *
 * @return number of bytes read on success, -(errno) on failure
 */
int devfs_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz);

/**
 * @brief Find a device entry under /dev.
 *
 * Find the dirent corresponding to the device whose device id is given
 * in \a dev. If \a blk is non-zero, the function will only look for block
 * devices, otherwise it will look for character devices. The resultant
 * dirent is stored in \a *entry.
 *
 * @param   dev     device id to look for
 * @param   blk     non-zero to look for block devices, zero for char devices
 * @param   entry   the resultant dirent is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int devfs_find_deventry(dev_t dev, int blk, struct dirent **entry);

#endif      /* __DEV_FSYS_H__ */
