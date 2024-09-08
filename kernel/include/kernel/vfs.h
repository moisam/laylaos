/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: vfs.h
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
 *  \file vfs.h
 *
 *  Include header file for working with the kernel's virtual filesystem (VFS).
 */

#ifndef __VFS_H__
#define __VFS_H__

#include <dirent.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <limits.h>      // OPEN_MAX
//#include <sys/syslimits.h>      // OPEN_MAX
#include <sys/mount.h>          // mount flags
#include <kernel/mutex.h>
//#include <kernel/bio.h>
#include <kernel/select.h>

/*******************************
 * macro definitions
 *******************************/

#ifndef OPEN_MAX
#define OPEN_MAX                32      /**< max open files per task */
#endif

#ifndef LINK_MAX
#define	LINK_MAX		        32767	/**< max file link count */
#endif

#define NR_INODE                256     /**< max inodes cached in memory */

#define NR_FILE                 256     /**< max files open on the system */

#define NR_OPEN                 OPEN_MAX    /**< max files open per task 
                                                 (currently 32) */

#define NR_BUFFERS              128     /**< max buffers */

#define NR_SUPER                32      /**< max superblocks (i.e. mounted
                                             devices) */

#define NR_FILESYSTEMS          16      /**< max number of registered
                                             filesystems */

#define NR_RAMDISK              256     /**< max number of ramdisks */

/*
 * Flags for bmap() functions
 */
#define BMAP_FLAG_NONE          0   /**< bmap() function to return the block */
#define BMAP_FLAG_CREATE        1   /**< bmap() function to create the block */
#define BMAP_FLAG_FREE          2   /**< bmap() function to free the block */

/*
 * Flags for has_perm()
 */
#define EXECUTE                 01
#define WRITE                   02
#define READ                    04

/*
 * Open_flags for vfs_open_internal() and vfs_open()
 */
#define OPEN_KERNEL_CALLER      0x1
#define OPEN_USER_CALLER        0x0
#define OPEN_FOLLOW_SYMLINK     0x2
#define OPEN_NOFOLLOW_SYMLINK   0x0
#define OPEN_NOFOLLOW_MPOINT    0x4
#define OPEN_CREATE_DENTRY      0x10

/*
 * MIX and MAX macros
 */
#ifndef MIN
#define MIN(a, b)               (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)               (((a) > (b)) ? (a) : (b))
#endif

/**
 * \def MAJOR
 * Extract major device number from devid
 */
#define MAJOR(a)                (((unsigned)(a)) >> 8)

/**
 * \def MINOR
 * Extract minor device number from devid
 */
#define MINOR(a)                ((a) & 0xff)

/**
 * \def TO_DEVID
 * Create devid from the given major and minor device numbers
 */
#define TO_DEVID(maj, min)      (((maj) << 8) | ((min) & 0xff))

/**
 * \def IS_SOCKET
 * Check if a file node refers to a socket
 */
#define IS_SOCKET(node)         ((node)->flags & FS_NODE_SOCKET)

/**
 * \def IS_PIPE
 * Check if a file node refers to a pipe
 */
#define IS_PIPE(node)           ((node)->flags & FS_NODE_PIPE)

/**
 * \def INC_NODE_REFS
 * Increment incore node references
 */
#define INC_NODE_REFS(node)             \
    kernel_mutex_lock(&node->lock);     \
    node->refs++;                       \
    kernel_mutex_unlock(&node->lock);


/***************************************
 * Get different structure definitions
 ***************************************/

#include <kernel/bits/pcache-defs.h>
#include "bits/vfs-defs.h"

// typedefs for use by char/block device driver interfaces
typedef ssize_t (*rw_char_t)(dev_t dev, unsigned char *buf, size_t count);


/*******************************
 * extern variable definitions
 *******************************/

/*
 * root and other important device ids
 */
extern dev_t DEVPTS_DEVID;      // block (240, 3)
extern dev_t PTMX_DEVID;        // char  (5,   2)
extern dev_t ROOT_DEVID;        // block (240, 1)
extern dev_t DEV_DEVID;         // block (240, 2)
extern dev_t TMPFS_DEVID;       // block (241, x)
                                // NOTE: this only stores the MAJOR(devid)
//extern dev_t SOCKFS_DEVID;      // block (242, 0)
extern dev_t PROCFS_DEVID;      // block (243, 0)

/**
 * @var ftab
 * @brief file table.
 *
 * The master file table (defined in vfs.c).
 */
extern struct file_t ftab[];

/**
 * @var node_table
 * @brief node table.
 *
 * The master node table (defined in node.c).
 */
extern struct fs_node_t node_table[];

/**
 * @var fstab
 * @brief filesystem table.
 *
 * The registered filesystem table (defined in fstab.c).
 */
extern struct fs_info_t fstab[];

/**
 * @var mounttab
 * @brief mount table.
 *
 * The master mount filesystem table (defined in mount.c).
 */
extern struct mount_info_t mounttab[];

/**
 * @var mount_table_mutex
 * @brief mount table lock.
 *
 * Lock to synchronise access to the master mount filesystem table (defined
 * in mount.c).
 */
extern struct kernel_mutex_t mount_table_mutex;

/**
 * @var system_root_node
 * @brief system root node.
 *
 * A pointer to the system's root node (defined in rootfs.c).
 */
extern struct fs_node_t *system_root_node;


/**********************************
 * Functions defined in fio.c
 **********************************/

/**
 * @brief Allocate file descriptor.
 *
 * Allocate a user file descriptor and a file struct, with the descriptor 
 * pointing at the struct.
 *
 * @param   _fd     file descriptor in the range 0..NR_OPEN is returned here
 * @param   _f      pointer to file struct in the master file table is 
 *                    returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int falloc(int *_fd, struct file_t **_f);

/**
 * @brief Close file.
 *
 * Close the given file and perform any housekeeping chores for special
 * devices and sockets.
 *
 * @param    f      pointer to file
 *
 * @return  zero on success, -(errno) on failure.
 */
int closef(struct file_t *f);


/**********************************
 * Functions defined in fstab.c
 **********************************/

/**
 * @brief Initialise the filesystem table.
 *
 * Initialize the kernel's master filesystem table.
 *
 * @return  nothing.
 */
void init_fstab(void);

/**
 * @brief Get filesystem by name.
 *
 * Get a pointer to the filesystem struct given the filesystem's name, which
 * should be one of the filesystems registered with the kernel.
 *
 * @param   name    the filesystem's name, e.g. ext2
 *
 * @return  a pointer to the filesystem's info struct.
 */
struct fs_info_t *get_fs_by_name(char *name);

/**
 * @brief Register a filesystem.
 *
 * Add the filesystem identified by \a name, whose filesystem operations
 * are accessbile via \a ops, to the kernel's recognized filesystem list.
 *
 * @param   name    the filesystem's name, e.g. ext2
 * @param   ops     pointer to the filesystem's operations struct
 *
 * @return  a pointer to the new filesystem's entry in the kernel list.
 *
 * @see     devfs_init, devpts_init, iso9660fs_init, procfs_init, tmpfs_init
 */
struct fs_info_t *fs_register(char *name, struct fs_ops_t *ops);

/**
 * @brief Handler for syscall sysfs().
 *
 * Return information about the filesystem types currently present in the
 * kernel, depending on the given 'option':
 *   - option is 1 => translate the filesystem identifier string \a fsid into
 *                    a filesystem type index 
 *   - option is 2 => translate the filesystem type index \a fsid into a 
 *                    NULL-terminated filesystem identifier string, which will
 *                    be written to the buffer \a buf (it must be big enough!)
 *   - option is 3 => return the total number of filesystem types currently 
 *                    present in the kernel
 *
 * @param   option      1, 2 or 3 (see description above)
 * @param   fsid        string pointer, filesystem type index, or unused, 
 *                        depending on \a option (see description above)
 * @param   buf         buffer (used only if \a option == 2)
 *
 * @return  zero or positive number on success, -(errno) on failure.
 */
int syscall_sysfs(int option, uintptr_t fsid, char *buf);


/**********************************
 * Functions defined in mount.c
 **********************************/

/**
 * @brief Synchronise superblocks.
 *
 * Write out modified superblocks to disk. Called by update().
 * If \a dev == NODEV, all modified superblocks are sync'd.
 *
 * @param   dev     device id
 *
 * @return  nothing.
 *
 * @see     update()
 */
void sync_super(dev_t dev);

/**
 * @brief Get mount info.
 *
 * Get a mounted device's info given the device id.
 *
 * @param   dev     device id
 *
 * @return  pointer to the device's info struct, NULL if device not found.
 */
struct mount_info_t *get_mount_info(dev_t dev);

/**
 * @brief Get mount info.
 *
 * Get a mounted device's info given an inode from the mounted device.
 *
 * @param   node    file node
 *
 * @return  pointer to the device's info struct, NULL if device not found.
 */
struct mount_info_t *get_mount_info2(struct fs_node_t *node);

/**
 * @brief Get an empty mount table entry.
 *
 * Return the first unused entry in the master mount table, used when
 * mounting new devices.
 *
 * @return  pointer to empty mount table entry, NULL if table is full.
 */
struct mount_info_t *mounttab_first_empty(void);

/**
 * @brief Mount the given device on the given path.
 *
 * This function calls the filesystem's mount() function, which should read
 * the superblock and root inode and fill in some fields in the device info
 * struct (as well as the flags field after interpreting them).
 *
 * NOTE: The caller has to ensure path does NOT end in '/'!
 *
 * @param  dev      device id
 * @param  path     mount path
 * @param  fstype   filesystem type
 * @param  flags    mount flags
 * @param  options  filesystem-dependent mount options
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_mount(dev_t dev, char *path, char *fstype, int flags, char *options);

/**
 * @brief Unmount the given device.
 *
 * This function calls the filesystem's write_super() and put_super() 
 * functions, which should write the superblock and release its memory.
 *
 * @param  dev      device id
 * @param  flags    unmount flags
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_umount(dev_t dev, int flags);

/**
 * @brief Initial mount.
 *
 * Called during boot time to mount the early root filesystem, using the
 * /etc/boot_fstab config file.
 *
 * @return  zero on success, -(errno) on failure.
 */
int mountall(void);

int mount_internal(char *module, char *devpath, int boot_mount);

/**
 * @brief Path to device id.
 *
 * Called when mounting devices to convert a pathname to a device id.
 *
 * @param  source   device pathname
 * @param  fstype   filesystem type (for special types e.g. devfs or tmpfs)
 * @param  dev      device id is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_path_to_devid(char *source, char *fstype, dev_t *dev);


/**********************************
 * Functions defined in node.c
 **********************************/

/**
 * @brief Synchronise file nodes.
 *
 * Write out all modified inodes to disk. Called by update().
 *
 * @param   dev     device id
 *
 * @return  nothing.
 *
 * @see     update()
 */
void sync_nodes(dev_t dev);

/**
 * @brief Release file node.
 *
 * If the node is a pipe, wake up sleepers (if any) and free the pipe's 
 * memory page. For dirty nodes, update the node on disk.
 * If the file is empty, truncate it and free the node struct on disk.
 *
 * @param   node    file node
 *
 * @return  nothing.
 */
void release_node(struct fs_node_t *node);

/**
 * @brief Check for an incore node.
 *
 * Check if a node is incore (that is, used by some task).
 *
 * @param   dev     device id
 * @param   n       inode number
 *
 * @return  1 if node is present in memory, 0 if not.
 */
int node_is_incore(dev_t dev, ino_t n);

/**
 * @brief Get an empty node.
 *
 * Get an empty node struct from the master node table.
 *
 * @return  node pointer on success, NULL if the master node table is full.
 */
struct fs_node_t *get_empty_node(void);

/**
 * @brief Read a node.
 *
 * If the node is incore, it is returned. Otherwise, it is read from disk.
 * If the node is a mount point and \a follow_mpoints is non-zero, the mount
 * point is followed and the returned node is the root node of the mounted
 * filesystem.
 *
 * @param   dev             device id
 * @param   n               inode number
 * @param   follow_mpoints  non-zero to follow last mount point
 *
 * @return  node pointer on success, NULL on failure.
 */
struct fs_node_t *get_node(dev_t dev, ino_t n, int follow_mpoints);

/**
 * @brief Read file node.
 *
 * Read inode info from disk.
 *
 * @param   node    file node
 *
 * @return  zero on success, -(errno) on failure.
 */
int read_node(struct fs_node_t *node);

/**
 * @brief Write file node.
 *
 * Write inode info back to disk.
 *
 * @param   node    file node
 *
 * @return  zero on success, -(errno) on failure.
 */
int write_node(struct fs_node_t *node);

/**
 * @brief Truncate node.
 *
 * Truncate a file to a specified size, freeing or allocating disk blocks
 * as needed.
 *
 * @param   node    file node
 * @param   sz      new file size
 *
 * @return  zero on success, -(errno) on failure.
 */
int truncate_node(struct fs_node_t *node, size_t sz);

/**
 * @brief Create a new node.
 *
 * A new inode is allocated on the mounted device identified by \a dev,
 * by calling the respective filesystem alloc_inode() function.
 *
 * @param   dev             device id
 *
 * @return  node pointer on success, NULL on failure.
 */
struct fs_node_t *new_node(dev_t dev);

/**
 * @brief Free node.
 *
 * Free inode on disk. This function calls the respective filesystem 
 * free_inode() function.
 *
 * @param   node    file node
 *
 * @return  nothing.
 */
void free_node(struct fs_node_t *node);


/**********************************
 * Functions defined in vfs.c
 **********************************/

/**
 * @brief Update node's atime.
 *
 * Update the node's last access time.
 *
 * @param   node    file node
 *
 * @return  nothing.
 */
void update_atime(struct fs_node_t *node);

/**
 * @brief Remove trailing slash from path.
 *
 * Get a kalloc()'d copy of the path and remove any trailing'/'s.
 * Used to sanitize pathnames we pass to get_parent_dir().
 *
 * @param   path            path to sanitize
 * @param   kernel          non-zero if the original caller is a kernel 
 *                            function
 * @param   trailing_slash  if not-NULL, set to 1 if the path ends in a slash
 *
 * @return  pointer to the kalloc'd copy on success, NULL on failure.
 *
 * @see     get_parent_dir()
 */
char *path_remove_trailing_slash(char *path, int kernel, int *trailing_slash);

/**
 * @brief Get parent directory.
 *
 * Get the node of the parent directory for the given \a pathname.
 * We don't get the requested file directly, as we might need to create it, 
 * in which case we need access to the parent directory.
 *
 * NOTE: \a pathname should NOT end in '/'. The caller has the responsibility 
 * to ensure that, otherwise the returned node will be of the base file, NOT
 * the parent directory!
 *
 * @param   pathname        path name
 * @param   dirfd           \a pathname is interpreted relative to this 
 *                            directory
 * @param   filename        a pointer to the first char in the basename of 
 *                            the requested path is returned here
 * @param   dirnode         a pointer to the parent directory's node is 
 *                            returned here
 * @param   follow_mpoints  non-zero to follow last mount point
 *
 * @return  zero on success, -(errno) on failure.
 */
int get_parent_dir(char *pathname, int dirfd, char **filename,
                   struct fs_node_t **dirnode, int follow_mpoints);

/**
 * @brief Open a file.
 *
 * Interal function called by other kernel functions that need a quick and
 * dirty file opening service, e.g. the syscall handlers for chown, chdir, 
 * and chmod.
 *
 * @param   pathname        path name
 * @param   dirfd           \a pathname is interpreted relative to this 
 *                            directory
 * @param   filenode        a pointer to the opened file node is returned here
 * @param   open_flags      kernel open flags (see the OPEN_* macros in vfs.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_open_internal(char *pathname, int dirfd, 
                      struct fs_node_t **filenode, int open_flags);

/**
 * @brief Open a file.
 *
 * Called by syscall_open() to do the heavy work of opening a file.
 *
 * @param   pathname        path name
 * @param   flags           open flags (passed by the user)
 * @param   mode            access mode if creating a new file (passed by 
 *                            the user)
 * @param   dirfd           \a pathname is interpreted relative to this 
 *                            directory
 * @param   filenode        a pointer to the opened file node is returned here
 * @param   open_flags      kernel open flags (see the OPEN_* macros in vfs.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_open(char *pathname, int flags, mode_t mode, int dirfd, 
             struct fs_node_t **filenode, int open_flags);

/**
 * @brief Find a file in a directory.
 *
 * Find the file with the given \a filename in the parent directory 
 * represented by the given \a dir node.
 *
 * @param   dir         the parent directory's node
 * @param   filename    the searched-for filename
 * @param   entry       a pointer to a kmalloc()'d struct dirent representing
 *                        the file is returned here. It is the caller's 
 *                        responsibility to call kfree() to release this!
 * @param   dbuf        a pointer to the disk buffer containing the found 
 *                        entry is returned here. Useful for things like 
 *                        removing the entry from parent directory without
 *                        needing to re-read the block from disk again
 * @param   dbuf_off    offset of the entry in the disk buffer
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                struct cached_page_t **dbuf, size_t *dbuf_off);

/**
 * @brief Find an inode in a directory.
 *
 * Find the given inode in the parent directory.
 * Called during pathname resolution when constructing the absolute pathname
 * of a given inode.
 *
 * @param   dir         the parent directory's node
 * @param   node        the searched-for inode
 * @param   entry       if the \a node is found, its entry is converted to a
 *                        kmalloc'd dirent struct, and the result is returned
 *                        in this field
 * @param   dbuf        the disk buffer representing the disk block containing
 *                        the found filename is returned here. This is useful 
 *                        if the caller wants to delete the file after finding
 *                        it (vfs_unlink(), for example)
 * @param   dbuf_off    the offset in dbuf->data at which the caller can find
 *                        the file's entry is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                         struct dirent **entry,
                         struct cached_page_t **dbuf, size_t *dbuf_off);

/**
 * @brief Add a directory entry.
 *
 * Add the given \a filename to the parent directory represented by the given
 * \a dir node. The new file's inode number is passed as the \n n parameter.
 *
 * @param   dir         the parent directory's node
 * @param   filename    the new file name
 * @param   n           the new file inode number
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_addir(struct fs_node_t *dir, char *filename, ino_t n);

/**
 * @brief Generic function to read from a file.
 *
 * Read at most \a count bytes from the given file \a node, starting at
 * offset \a pos in the file. The offset is updated with the count of bytes
 * read from the file.
 *
 * @param   node        file inode
 * @param   pos         file offset to start reading from
 * @param   buf         data is copied to this buffer
 * @param   count       number of characters to read
 * @param   kernel      non-zero if the caller is a kernel function
 *
 * @return  number of characters read on success, -(errno) on failure.
 *
 * @see     vfs_read()
 */
ssize_t vfs_read_node(struct fs_node_t *node, off_t *pos,
                      unsigned char *buf, size_t count, int kernel);

/**
 * @brief Generic function to read from a file.
 *
 * Read at most \a count bytes from the given \a file, starting at
 * offset \a pos in the file. The offset is updated with the count of bytes
 * read from the file.
 *
 * @param   file        file struct
 * @param   pos         file offset to start reading from
 * @param   buf         data is copied to this buffer
 * @param   count       number of characters to read
 * @param   kernel      non-zero if the caller is a kernel function
 *
 * @return  number of characters read on success, -(errno) on failure.
 *
 * @see     vfs_read_node()
 */
ssize_t vfs_read(struct file_t *file, off_t *pos,
                 unsigned char *buf, size_t count, int kernel);

/**
 * @brief Generic function to write to a file.
 *
 * Write at most \a count bytes to the given file \a node, starting at
 * offset \a pos in the file. The offset is updated with the count of bytes
 * written to the file.
 *
 * @param   node        file inode
 * @param   pos         file offset to start writing to
 * @param   buf         data is copied from this buffer
 * @param   count       number of characters to write
 * @param   kernel      non-zero if the caller is a kernel function
 *
 * @return  number of characters written on success, -(errno) on failure.
 *
 * @see     vfs_write()
 */
ssize_t vfs_write_node(struct fs_node_t *node, off_t *pos,
                       unsigned char *buf, size_t count, int kernel);

/**
 * @brief Generic function to write to a file.
 *
 * Write at most \a count bytes to the given \a file, starting at
 * offset \a pos in the file. The offset is updated with the count of bytes
 * written to the file.
 *
 * @param   file        file struct
 * @param   pos         file offset to start writing to
 * @param   buf         data is copied from this buffer
 * @param   count       number of characters to write
 * @param   kernel      non-zero if the caller is a kernel function
 *
 * @return  number of characters written on success, -(errno) on failure.
 *
 * @see     vfs_write_node()
 */
ssize_t vfs_write(struct file_t *file, off_t *pos,
                  unsigned char *buf, size_t count, int kernel);

/**
 * @brief Generic linkat function.
 *
 * Make a new name (i.e. hard link) for a file.
 *
 * @param   olddirfd    \a oldname is interpreted relative to this directory
 * @param   oldname     existing file name
 * @param   newdirfd    \a newname is interpreted relative to this directory
 * @param   newname     file name of the new link
 * @param   flags       zero or AT_SYMLINK_NOFOLLOW (the other flag, 
 *                        AT_EMPTY_PATH, is not supported yet)
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_linkat(int olddirfd, char *oldname, 
               int newdirfd, char *newname, int flags);

/**
 * @brief Generic unlinkat function.
 *
 * Delete a file name and possibly the file it refers to.
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   pathname    existing file name
 * @param   flags       zero or AT_REMOVEDIR
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_unlinkat(int dirfd, char *pathname, int flags);

/**
 * @brief Generic rmdir function.
 *
 * Remove an empty directory.
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   pathname    pathname of directory to remove
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_rmdir(int dirfd, char *pathname);

/**
 * @brief Delete a directory entry.
 *
 * Remove an entry from a parent directory.
 *
 * @param   dir         the parent directory's node
 * @param   entry       the entry to delete
 * @param   dbuf        the disk buffer representing the disk block containing
 *                        the entry
 * @param   dbuf_off    the offset in dbuf->data at which the entry can be
 *                        found
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_deldir(struct fs_node_t *dir, struct dirent *entry,
               struct cached_page_t *dbuf, size_t dbuf_off);

/**
 * @brief Get dir entries.
 *
 * Get dir entries.
 *
 * @param   dir     node of dir to read from
 * @param   pos     byte position to start reading entries from, which will be
 *                    updated after the read to prepare for future reads
 * @param   dp      buffer in which to store dir entries
 * @param   count   max number of bytes to read (i.e. size of \a dp)
 *
 * @return  number of bytes read on success, -(errno) on failure.
 */
int vfs_getdents(struct fs_node_t *dir, off_t *pos, void *dp, int count);

/**
 * @brief Generic mknod function.
 *
 * Create a new special or regular file.
 *
 * @param   pathname        path name (absolute or relative)
 * @param   mode            file mode and access permissions
 * @param   dev             if file type is S_IFCHR or S_IFBLK, this contains
 *                            the new device's device id
 * @param   dirfd           \a pathname is interpreted relative to this 
 *                            directory
 * @param   open_flags      kernel open flags (see the OPEN_* macros in vfs.h)
 * @param   res             a pointer to the new file node is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int vfs_mknod(char *pathname, mode_t mode, dev_t dev, int dirfd,
              int open_flags, struct fs_node_t **res);


/**********************************
 * Functions defined in update.c
 **********************************/

/**
 * @brief Synchronise disk caches.
 *
 * Write out all modified disk buffers and inodes to disk.
 *
 * @param   dev     device id
 *
 * @return  nothing.
 *
 * @see     sync_super(), sync_nodes(), bflush()
 */
void update(dev_t dev);


/**********************************
 * Functions defined in rootfs.c
 **********************************/

/**
 * @brief Initialize the root filesystem.
 *
 * Initialize the root virtual filesystem, the abstract filesystem the kernel
 * boots into. It also mounts the initial ramdisk and then mounts the proper
 * root filesystem, which is used to complete system boot.
 *
 * @return  pointer to the root node.
 */
struct fs_node_t *rootfs_init(void);


static inline struct mount_info_t *node_mount_info(struct fs_node_t *node)
{
    if(node->minfo == NULL)
    {
        node->minfo = get_mount_info(node->dev);
    }

    return node->minfo;
}

#endif      /* __VFS_H__ */
