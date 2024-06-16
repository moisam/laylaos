/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: tmpfs.h
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
 *  \file tmpfs.h
 *
 *  Functions and macros for working with the tmpfs filesystem.
 */

#ifndef __TMP_FSYS_H__
#define __TMP_FSYS_H__

#include <stdint.h>
#include <dirent.h>
#include <sys/types.h>
#include <kernel/vfs.h>
#include <kernel/pcache.h>

/**
 * @var tmpfs_ops
 * @brief tmpfs filesystem ops.
 *
 * The tmpfs filesystem operations structure.
 */
extern struct fs_ops_t tmpfs_ops;

/**
 * @var tmpfs_lock
 * @brief tmpfs filesystem lock.
 *
 * A lock to synchronize access to the tmpfs filesystem table.
 */
extern struct kernel_mutex_t tmpfs_lock;


/**
 * @brief Mount a tmpfs filesystem.
 *
 * Mount a tmpfs filesystem using the given \a mount_info_t struct and
 * \a options.
 * To use tmpfs, we need a two step process:
 *    (1) Mount tmpfs. This function will call tmpfs_create() to create a new
 *        tmpfs system, reserve memory for the virtual disk, create a virtual
 *        inode/block bitmap, and create the root inode for the new tmpfs.
 *    (2) Call tmpfs_read_super(), which will create the root node's directory
 *        by calling tmpfs_mkdir(). The tmpfs system is usable after this step.
 *
 * @param   d       pointer to the mount info struct on which we'll mount tmpfs
 * @param   flags   currently not used
 * @param   options a string of options that MUST include the following
 *                    comma-separated options and their values:
 *                      inode_count, block_count, block_size,
 *                      e.g.
 *                      "inode_count=64,block_count=16,block_size=512"
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_mount(struct mount_info_t *d, int flags, char *options);

/**
 * @brief Read tmpfs superblock and root inode.
 *
 * Read a tmpfs filesystem's superblock and root inode. This function fills
 * in the mount info struct's \a block_size, \a super, and \a root fields.
 *
 * @param   dev                 device id
 * @param   d                   pointer to the mount info struct on which
 *                                we'll mount \a dev
 * @param   bytes_per_sector    bytes per sector
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_read_super(dev_t dev, struct mount_info_t *d,
                     size_t bytes_per_sector);

/**
 * @brief Release the filesystem's superblock and its buffer.
 *
 * Release a tmpfs filesystem's superblock and its buffer. This function is
 * called when unmounting the tmpfs filesystem.
 * For tmpfs, we also release the virtual disk's memory, as we expect no one
 * will be using them anymore after this call.
 *
 * @param   dev device id
 * @param   sb  pointer to the buffer containing the superblock's data
 *
 * @return  nothing.
 */
void tmpfs_put_super(dev_t dev, struct superblock_t *sb);

/**
 * @brief Read inode data structure.
 *
 * Read inode data structure from a tmpfs filesystem.
 *
 * @param   node    node struct whose inode field tells us which inode to read
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_read_inode(struct fs_node_t *);

/**
 * @brief Write inode data structure.
 *
 * Write inode data structure to a tmpfs filesystem.
 *
 * @param   node    node struct to write
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_write_inode(struct fs_node_t *);

/**
 * @brief Map logical block to disk block.
 *
 * Map file position to disk block number using inode struct's block pointers.
 * For simplicifty, tmpfs uses the 15 inode block pointers as direct block
 * pointers, which gives a max. file size of 15*block_size (e.g. 30720 bytes if
 * using 2048-byte blocks).
 *
 * @param   node        node struct
 * @param   lblock      block number we want to map
 * @param   block_size  filesystem's block size in bytes
 * @param   flags       could be \ref BMAP_FLAG_CREATE, \ref BMAP_FLAG_FREE or
 *                        \ref BMAP_FLAG_NONE which creates the block if it
 *                        doesn't exist, frees the block (when shrinking
 *                        files), or simply maps, respectively
 *
 * @return  disk block number on success, 0 on failure
 */
size_t tmpfs_bmap(struct fs_node_t *node, size_t lblock,
                  size_t block_size, int flags);

/**
 * @brief Free an inode.
 *
 * Free an inode and update the inode bitmap on disk.
 *
 * @param   node    node struct to free
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_free_inode(struct fs_node_t *node);

/**
 * @brief Allocate a new inode.
 *
 * Allocate a new inode number and mark it as used in the disk's inode bitmap.
 *
 * This function also updates the mount info struct's free inode pool if all
 * the cached inode numbers have been used by searching the disk for free
 * inode numbers.
 *
 * @param node  node struct in which we'll store the new alloc'd inode number
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_alloc_inode(struct fs_node_t *node);

/**
 * @brief Free a disk block.
 *
 * Free a disk block and update the disk's block bitmap.
 *
 * @param   dev         device id
 * @param   block_no    block number to be freed
 *
 * @return  nothing.
 */
void tmpfs_free(dev_t dev, uint32_t block_no);

/**
 * @brief Allocate a new disk block.
 *
 * Allocate a new block number and mark it as used in the disk's block bitmap.
 *
 * This function also updates the mount info struct's free block pool if all
 * the cached block numbers have been used by searching the disk for free
 * block numbers.
 *
 * @param   dev         device id
 *
 * @return  new alloc'd block number on success, 0 on failure.
 */
uint32_t tmpfs_alloc(dev_t dev);

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
 *                        ext2_entry_to_dirent()), and the result is
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
int tmpfs_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                  struct cached_page_t **dbuf, size_t *dbuf_off);

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
int tmpfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                           struct dirent **entry,
                           struct cached_page_t **dbuf, size_t *dbuf_off);

/**
 * @brief Add new entry to a directory.
 *
 * Add the given \a filename as an entry in the given parent \a dir. The new
 * entry will be assigned inode number \a n.
 *
 * @param   dir         the parent directory's node
 * @param   filename    the new file's name
 * @param   n           the new file's inode number
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_addir(struct fs_node_t *dir, char *filename, ino_t n);

/**
 * @brief Create a new directory.
 *
 * Make a new, empty directory by allocating a free block and initializing
 * the '.' and '..' entries to point to the current and \a parent directory
 * inodes, respectively.
 *
 * @param   parent  the parent directory's inode number
 * @param   dir     node struct containing the new directory's inode number
 *                    (the directory's link count and block[0] will be updated)
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_mkdir(struct fs_node_t *dir, ino_t parent);

/**
 * @brief Check if a directory is empty.
 *
 * Check if the given \a dir is empty (called from rmdir()).
 *
 * @param   dir         the directory's node
 *
 * @return  1 if \a dir is empty, 0 if it is not
 */
int tmpfs_dir_empty(struct fs_node_t *dir);

/**
 * @brief Get dir entries.
 *
 * Get the file entries of the given \a dir, including '.' and '..'.
 *
 * @param   dir     node of dir to read from
 * @param   pos     byte position to start reading entries from
 * @param   buf     buffer in which to store dir entries
 * @param   bufsz   max number of bytes to read (i.e. size of \a dp)
 *
 * @return number of bytes read on success, -(errno) on failure
 */
int tmpfs_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz);

/**
 * @brief General Block Read/Write Operations.
 *
 * This is a swiss-army knife function that handles both read and write
 * operations from disk/media. The buffer specified in \a buf tells the
 * function everything it needs to know: how many bytes to read or write,
 * where to read/write the data to/from, which device to use for the
 * read/write operation, and whether the operation is a read or write.
 *
 * @param   buf     disk I/O request struct with read/write details
 *
 * @return number of bytes read or written on success, -(errno) on failure
 */
int tmpfs_strategy(struct disk_req_t *buf);

/**
 * @brief General block device control function.
 *
 * Perform ioctl operations on a tmpfs filesystem.
 *
 * @param   dev     device id of the tmpfs filesystem
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int tmpfs_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel);

/**
 * @brief Initialize the tmpfs virtual filesystem.
 *
 * Initialize the tmpfs virtual filesystem. This function calls fs_register()
 * to registers the tmpfs filesystem in the kernel's recognized filesystem
 * list.
 *
 * @return  nothing.
 *
 * @see     fs_register
 */
void tmpfs_init(void);

/**
 * @brief Create a tmpfs virtual filesystem.
 *
 * Create a new tmpfs system (or virtual device) which can then be mounted.
 *
 * @param   inode_count total number of inodes the tmpfs can have
 * @param   block_count total number of blocks the tmpfs can have
 * @param   block_size  size of each block (in bytes)
 *
 * @return  root node of the newly created tmpfs disk
 *
 * @see     dev_init
 */
struct fs_node_t *tmpfs_create(size_t inode_count,
                               size_t block_count, size_t block_size);

/**
 * @brief Return filesystem statistics.
 *
 * Return filesystem statistics, e.g. number of free blocks and inodes.
 *
 * @param   d       the mounted filesystem
 * @param   ubuf    struct to be filled with the statistics
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_ustat(struct mount_info_t *d, struct ustat *ubuf);

/**
 * @brief Return detailed filesystem statistics.
 *
 * Return detailed filesystem statistics, e.g. number of free blocks and
 * inodes, block size, etc.
 *
 * @param   d       the mounted filesystem
 * @param   statbuf struct to be filled with the statistics
 *
 * @return  zero on success, -(errno) on failure.
 */
int tmpfs_statfs(struct mount_info_t *d, struct statfs *statbuf);

#endif      /* __TMP_FSYS_H__ */
