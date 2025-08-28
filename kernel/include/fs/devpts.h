/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: devpts.h
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
 *  \file devpts.h
 *
 *  Functions and macros for working with the devpts filesystem. This filesystem
 *  is used to interface with pseudo-terminal (pty) devices. Master pty devices
 *  have a major devid of \ref PTY_MASTER_MAJ, while Slave pty devices have a
 *  major devid of \ref PTY_SLAVE_MAJ (the minor devid identifies each device).
 */

#ifndef __DEVPTS_FSYS_H__
#define __DEVPTS_FSYS_H__

#include <stdint.h>
#include <sys/types.h>
//#include <kernel/vfs.h>
#include <kernel/pcache.h>

/**
 * \def PTY_MASTER_MAJ
 *
 * major device id for pseudoterminal master devices
 */
#define PTY_MASTER_MAJ          2

/**
 * \def PTY_SLAVE_MAJ
 *
 * major device id for pseudoterminal slave devices
 */
#define PTY_SLAVE_MAJ           136

/**
 * \def MAX_PTY_DEVICES
 *
 * max number of pseudoterminal devices we can handle
 */
#define MAX_PTY_DEVICES         64

#include <kernel/tty.h>

struct pty_t
{
    uid_t uid;
    gid_t gid;
    mode_t mode;
    //struct fs_node_t *node;
    int index;
    int refs;
    struct tty_t tty;
};

/**
 * @var devpts_ops
 * @brief devpts filesystem ops.
 *
 * The devpts filesystem operations structure.
 */
extern struct fs_ops_t devpts_ops;

/**********************************************
 * general functions
 **********************************************/

/**
 * @brief Initialize the devpts virtual filesystem.
 *
 * Initialize the devpts virtual filesystem. This function calls fs_register()
 * to registers the devpts filesystem in the kernel's recognized filesystem
 * list.
 *
 * @return  nothing.
 *
 * @see     fs_register
 */
void devpts_init(void);

/**
 * @brief Read devpts superblock and root inode.
 *
 * Read the devpts filesystem's superblock and root inode. This function fills
 * in the mount info struct's \a block_size, \a super, and \a root fields.
 *
 * @param   dev                 device id
 * @param   d                   pointer to the mount info struct on which
 *                                we'll mount \a dev
 * @param   bytes_per_sector    bytes per sector
 *
 * @return  zero on success, -(errno) on failure.
 */
long devpts_read_super(dev_t dev, struct mount_info_t *d,
                       size_t bytes_per_sector);

/**
 * @brief Release the filesystem's superblock and its buffer.
 *
 * Release the devpts filesystem's superblock and its buffer. This function is
 * called when unmounting the devpts filesystem.
 *
 * @param   dev device id
 * @param   sb  pointer to the buffer containing the superblock's data
 *
 * @return  nothing.
 */
void devpts_put_super(dev_t dev, struct superblock_t *sb);

/**
 * @brief Read inode data structure.
 *
 * Read inode data structure from the devpts virtual filesystem.
 *
 * @param   node    node struct whose inode field tells us which inode to read
 *
 * @return  zero on success, -(errno) on failure.
 */
long devpts_read_inode(struct fs_node_t *node);

/**
 * @brief Write inode data structure.
 *
 * Write inode data structure to the devpts virtual filesystem.
 *
 * @param   node    node struct to write
 *
 * @return  zero on success, -(errno) on failure.
 */
long devpts_write_inode(struct fs_node_t *node);

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
long devpts_finddir(struct fs_node_t *dir, char *filename,
                    struct dirent **entry,
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
long devpts_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                             struct dirent **entry,
                             struct cached_page_t **dbuf, size_t *dbuf_off);

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
long devpts_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz);

/**
 * @brief Get a pty device's tty struct.
 *
 * Get the terminal structure (\ref tty_t "struct tty_t") that represents the
 * pseudo-terminal device (pty) that is identified by the device id given in
 * \a dev. The device's major id but be \ref PTY_MASTER_MAJ or
 * \ref PTY_SLAVE_MAJ. If the device is a slave pty, the master pty must not 
 * be closed.
 *
 * @param   dev device id
 *
 * @return  pointer to the pseudo-terminal device's tty struct.
 */
struct tty_t *devpts_get_struct_tty(dev_t dev);


/**********************************************
 * functions to manipulate pty master devices
 **********************************************/

/**
 * @brief Perform a select operation on a master pty device.
 *
 * After opening a master pseudo-terminal (pty) device, a select operation
 * can be performed on the device (accessed via the file struct \a f).
 * The operation (passed in \a which), can be FREAD, FWRITE, 0, or a 
 * combination of these.
 *
 * @param   f       open file struct
 * @param   which   the select operation to perform
 *
 * @return  1 if there are selectable events, 0 otherwise.
 */
long pty_master_select(struct file_t *f, int which);

/**
 * @brief Perform a poll operation on a master pty device.
 *
 * After opening a master pseudo-terminal (pty) device, a poll operation
 * can be performed on the device (accessed via the file struct \a f).
 * The \a events field of \a pfd contains a combination of requested events
 * (POLLIN, POLLOUT, ...). The resultant events are returned in the \a revents
 * member of \a pfd.
 *
 * @param   f       open file struct
 * @param   pfd     poll events
 *
 * @return  1 if there are pollable events, 0 otherwise.
 */
long pty_master_poll(struct file_t *f, struct pollfd *pfd);

/**
 * @brief Create a new master pty device.
 *
 * Create a new master pseudo-terminal (pty) device. The new device node is
 * returned in \a master.
 *
 * @param   master  the new device node is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long pty_master_create(struct fs_node_t **master);

/**
 * @brief Close a master pty device.
 *
 * Close a master pseudo-terminal (pty) device.
 *
 * @param   node    file node of the device to close
 *
 * @return  nothing.
 */
void pty_master_close(struct fs_node_t *node);

/**
 * @brief Read from a master pty device.
 *
 * Read data from a master pseudo-terminal (pty) device. This is handled
 * separate from the slave device, as the master "reads" from the write queue.
 *
 * Slave pseudo-terminal (pty) devices are handled by ttyx_read(), much like 
 * regular terminal devices.
 *
 * @param   f       open file struct
 *                  device number is found in blocks[0] of the node struct:
 *                    major should be \ref PTY_MASTER_MAJ, while minor should
 *                    be between 0 and \ref MAX_PTY_DEVICES - 1, inclusive
 * @param   pos     position in file to read from
 * @param   buf     data is copied to this buffer
 * @param   count   number of characters to read
 * @param   kernel  0 if call is coming from userland, non-zero if from kernel
 *
 * @return  number of characters read on success, -(errno) on failure
 */
ssize_t pty_master_read(struct file_t *f, off_t *pos,
                        unsigned char *buf, size_t count, int kernel);

/**
 * @brief Write to a master pty device.
 *
 * Write data to a master pseudo-terminal (pty) device. This is handled
 * separate from the slave device, as the master "writes" to the read queue.
 *
 * Slave pseudo-terminal (pty) devices are handled by ttyx_write(), much like
 * regular terminal devices.
 *
 * @param   f       open file struct
 *                  device number is found in blocks[0] of the node struct:
 *                    major should be \ref PTY_MASTER_MAJ, while minor should
 *                    be between 0 and \ref MAX_PTY_DEVICES - 1, inclusive
 * @param   pos     position in file to write to
 * @param   buf     data is copied from this buffer
 * @param   count   number of characters to write
 * @param   kernel  0 if call is coming from userland, non-zero if from kernel
 *
 * @return  number of characters written on success, -(errno) on failure
 */
ssize_t pty_master_write(struct file_t *f, off_t *pos,
                         unsigned char *buf, size_t count, int kernel);


/**********************************************
 * functions to manipulate pty slave devices
 **********************************************/

/**
 * @brief Open a slave pty device.
 *
 * Open a slave pseudo-terminal (pty) device.
 *
 * @param   node    file node of the device to open
 *
 * @return  nothing.
 */
long pty_slave_open(struct fs_node_t *node);

/**
 * @brief Close a slave pty device.
 *
 * Close a slave pseudo-terminal (pty) device.
 *
 * @param   node    file node of the device to close
 *
 * @return  nothing.
 */
void pty_slave_close(struct fs_node_t *node);

/**
 * @brief Get slave pty device number from a device id.
 *
 * Given a pty master device number, return the corresponding slave device
 * number. This is used to implement the ptsname() function.
 *
 * @param   dev device id
 *
 * @return  slave pty device index on success, -(errno) on failure.
 */
int pty_slave_index(dev_t dev);

#endif      /* __DEVPTS_FSYS_H__ */
