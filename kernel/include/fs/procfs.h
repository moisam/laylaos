/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: procfs.h
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
 *  \file procfs.h
 *
 *  Functions and macros for working with the procfs filesystem.
 */

#ifndef __PROC_FSYS_H__
#define __PROC_FSYS_H__

#include <stdint.h>
#include <sys/types.h>
#include <kernel/pci.h>
#include <kernel/pcache.h>

/**
 * \def PROCFS_LINK_SIZE
 *
 * buffer size large enough to hold the contents of a procfs link
 */
#define PROCFS_LINK_SIZE            128

/**
 * \def ALIGN_WORD
 *
 * macro to align an address to a 4-byte boundary
 */
#define ALIGN_WORD(w)               \
    if((w) & 3)                     \
    {                               \
        (w) &= ~3; (w) += 4;        \
    }

/**
 * \def INODE_DIR_BITS
 *
 * macro to extract the dir bits from a procfs inode number (see procfs.c
 * for the structure of a procfs inode number)
 */
#define INODE_DIR_BITS(i)           ((i) & 0xff)

/**
 * \def INODE_SUBDIR_BITS
 *
 * macro to extract the subdir bits from a procfs inode number (see procfs.c
 * for the structure of a procfs inode number)
 */
#define INODE_SUBDIR_BITS(i)        (((i) >> 8) & 0xff)

/**
 * \def INODE_FILE_BITS
 *
 * macro to extract the file bits from a procfs inode number (see procfs.c
 * for the structure of a procfs inode number)
 */
#define INODE_FILE_BITS(i)          (((i) >> 16) & 0xffff)

/**
 * \def MAKE_PROCFS_INODE
 *
 * macro to create a procfs inode number from a \a dir, \a subdir and
 * \a file number (see procfs.c for the structure of a procfs inode number)
 */
#define MAKE_PROCFS_INODE(dir, subdir, file)   \
            (((file) << 16) | ((subdir) << 8) | (dir))

/**
 * \enum dir_proc_enum
 *
 * possible value for the \a dir bits of a procfs inode number
 * (see procfs.c for the structure of a procfs inode number)
 */
enum dir_proc_enum
{
    DIR_PROC             = 1,   /**< dir is /proc */
    DIR_BUS                 ,   /**< dir is /proc/bus */
    DIR_BUS_PCI             ,   /**< dir is /proc/bus/pci */
    DIR_SYS                 ,   /**< dir is /proc/sys */
    DIR_TTY                 ,   /**< dir is /proc/tty */
    DIR_NET                 ,   /**< dir is /proc/net */
    DIR_PID                 ,   /**< dir is /proc/[pid] */
    DIR_PID_FD              ,   /**< dir is /proc/[pid]/fd */
    DIR_PID_TASK            ,   /**< dir is /proc/[pid]/task */
};


/**
 * \def PROCFS_DIR_MODE
 *
 * default type and access mode for a directory under /proc
 */
#define PROCFS_DIR_MODE         (S_IFDIR | 0555)    /* dr-xr-xr-x */

/**
 * \def PROCFS_FILE_MODE
 *
 * default type and access mode for a regular file under /proc
 */
#define PROCFS_FILE_MODE        (S_IFREG | 0444)    /* -r--r--r-- */

/**
 * \def PROCFS_LINK_MODE
 *
 * default type and access mode for a soft link under /proc
 */
#define PROCFS_LINK_MODE        (S_IFLNK | 0555)    /* lr-xr-xr-x */


/* some helper macros */
#define PR_MALLOC(b, s)         if(!(b = (char *)kmalloc(s))) return 0;

#define PR_REALLOC(b, s, c)                     \
{                                               \
    char *tmp;                                  \
    if(!(tmp = (char *)krealloc(b, s * 2)))     \
        return c;                               \
    s *= 2;                                     \
    b = tmp;                                    \
}


/**
 * @var procfs_ops
 * @brief procfs filesystem ops.
 *
 * The procfs filesystem operations structure.
 */
extern struct fs_ops_t procfs_ops;

/**
 * @var procfs_root
 * @brief procfs root node.
 *
 * This variable points to the root node of the procfs filesystem.
 */
extern struct fs_node_t *procfs_root;

/**
 * @brief Initialize the procfs virtual filesystem.
 *
 * Initialize the procfs virtual filesystem. This function calls fs_register()
 * to registers the procfs filesystem in the kernel's recognized filesystem
 * list.
 *
 * @return  nothing.
 *
 * @see     fs_register
 */
void procfs_init(void);

/**
 * @brief General block device control function.
 *
 * Perform ioctl operations on procfs.
 *
 * @param   dev_id  device id
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int procfs_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel);

/**
 * @brief Mount the procfs filesystem.
 *
 * Mount the procfs filesystem using the given \a mount_info_t struct and
 * \a options.
 *
 * @param   d       pointer to the mount info struct on which we'll
 *                    mount procfs
 * @param   flags   currently not used
 * @param   options a string of options that MIGHT include the following
 *                    comma-separated options and their values:
 *                      \a inode_count, \a block_count, \a block_size,
 *                      e.g.
 *                      "inode_count=64,block_count=16,block_size=512"
 *
 * @return  zero on success, -(errno) on failure.
 */
int procfs_mount(struct mount_info_t *d, int flags, char *options);

/**
 * @brief Read procfs superblock and root inode.
 *
 * Read the procfs filesystem's superblock and root inode. This function fills
 * in the mount info struct's \a block_size, \a super, and \a root fields.
 *
 * @param   dev                 device id
 * @param   d                   pointer to the mount info struct on which
 *                                we'll mount \a dev
 * @param   bytes_per_sector    bytes per sector
 *
 * @return  zero on success, -(errno) on failure.
 */
int procfs_read_super(dev_t dev, struct mount_info_t *d,
                      size_t bytes_per_sector);

/**
 * @brief Release the filesystem's superblock and its buffer.
 *
 * Release the procfs filesystem's superblock and its buffer. This function is
 * called when unmounting the procfs filesystem.
 * For procfs, we also release the virtual disk's memory, as we expect no one
 * will be using them anymore after this call.
 *
 * @param   dev device id
 * @param   sb  pointer to the buffer containing the superblock's data
 *
 * @return  nothing.
 */
void procfs_put_super(dev_t dev, struct superblock_t *sb);

/**
 * @brief Read inode data structure.
 *
 * Read inode data structure from the procfs filesystem.
 *
 * @param   node    node struct whose inode field tells us which inode to read
 *
 * @return  zero on success, -(errno) on failure.
 */
int procfs_read_inode(struct fs_node_t *);

/**
 * @brief Write inode data structure.
 *
 * Write inode data structure to the procfs filesystem.
 *
 * @param   node    node struct to write
 *
 * @return  zero on success, -(errno) on failure.
 */
int procfs_write_inode(struct fs_node_t *);

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
int procfs_finddir(struct fs_node_t *dir, char *filename,
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
int procfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
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
 * @param   bufsz   max number of bytes to read (i.e. size of \a dp)
 *
 * @return number of bytes read on success, -(errno) on failure
 */
int procfs_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz);

/**
 * @brief Read a symbolic link.
 *
 * Read the contents of a symbolic link. As different filesystems might have
 * different ways of storing symlinks (e.g. ext2 stores links < 60 chars in
 * length in the inode struct itself), we hand over this task to the
 * filesystem.
 *
 * @param   link    the symlink's inode
 * @param   buf     the buffer in which we will read and store the
 *                    symlink's target
 * @param   bufsz   size of buffer above 
 * @param   kernel  set if the caller is a kernel function (i.e. 'buf' address
 *                    is in kernel memory), 0 if 'buf' is a userspace address
 *
 * @return  number of chars read on success, -(errno) on failure.
 */
int procfs_read_symlink(struct fs_node_t *link, char *buf,
                        size_t bufsz, int kernel);

/**
 * @brief Write a symbolic link.
 *
 * Write the contents of a symbolic link. As different filesystems might have
 * different ways of storing symlinks (e.g. ext2 stores links < 60 chars in
 * length in the inode struct itself), we hand over this task to the
 * filesystem.
 *
 * @param   link    the symlink's inode
 * @param   target  the buffer containing the symlink's target to be saved
 * @param   len     size of buffer above 
 * @param   kernel  set if the caller is a kernel function (i.e. 'target' address
 *                    is in kernel memory), 0 if 'target' is a userspace address
 *
 * @return  number of chars written on success, -(errno) on failure.
 */
size_t procfs_write_symlink(struct fs_node_t *link, char *target,
                            size_t len, int kernel);

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
int procfs_ustat(struct mount_info_t *d, struct ustat *ubuf);

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
int procfs_statfs(struct mount_info_t *d, struct statfs *statbuf);

/**
 * @brief Read from a procfs file.
 *
 * Read at most \a count bytes from the file referred to by the given file
 * \a node, starting at position \a pos in the file. Data is stored into 
 * \a buf.
 *
 * @param   node        file node referring to the open procfs file
 * @param   pos         offset in file to start reading data from
 * @param   buf         buffer to read data into
 * @param   count       maximum amount of bytes to read into \a buf
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t procfs_read_file(struct fs_node_t *node, off_t *pos,
                         unsigned char *buf, size_t count);

/* some helper functions for internal use */
int copy_internal(char *dest, char *src, size_t destsz,
                  size_t srcsz, int kernel);
int copy_string_internal(char *dest, char *src, size_t destsz, int kernel);

#define procfs_deldir       ext2_deldir

/*******************************************
 * Functions defined in procfs_task.c
 *******************************************/
size_t read_other_taskmem(struct task_t *task, off_t pos,
                          virtual_addr memstart, virtual_addr memend,
                          char *buf, size_t count);

size_t write_other_taskmem(struct task_t *task, off_t pos,
                           virtual_addr memstart, virtual_addr memend,
                           char *buf, size_t count);

size_t get_task_rlimits(struct task_t *task, char **_buf);
size_t get_task_mmaps(struct task_t *task, char **_buf);
size_t get_task_posix_timers(struct task_t *task, char **_buf);
size_t get_task_io(struct task_t *task, char **buf);

int copy_task_dirpath(dev_t dev, ino_t ino,
                      char *buf, size_t bufsz, int kernel);

/*******************************************
 * Functions defined in procfs_task_stat.c
 *******************************************/
size_t get_task_stat(struct task_t *task, char **buf);
size_t get_task_statm(struct task_t *task, char **buf);
size_t get_task_status(struct task_t *task, char **buf);

/**************************************
 * Functions defined in procfs_file.c
 **************************************/
size_t get_device_list(char **_buf);
size_t get_fs_list(char **_buf);
size_t get_uptime(char **buf);
size_t get_version(char **buf);
size_t get_vmstat(char **buf);
size_t get_meminfo(char **buf);
size_t get_modules(char **buf);
size_t get_mounts(char **buf);
size_t get_sysstat(char **buf);
size_t get_pci_device_list(char **_buf);
size_t get_pci_device_config_space(struct pci_dev_t *pci, char **_buf);
size_t get_interrupt_info(char **_buf);
size_t get_ksyms(char **buf);

size_t get_partitions(char **buf);      // drivers/ata2.c

size_t get_buffer_info(char **buf);     // fs/procfs_bufinfo.c

size_t get_syscalls(char **buf);        // syscall/syscall.c

size_t get_dns_list(char **buf);
size_t get_arp_list(char **buf);        // net/arp.c
size_t get_net_dev_stats(char **buf);   // net/netif.c

/**************************************
 * Functions defined in procfs_sock.c
 **************************************/
size_t get_net_tcp(char **buf);
size_t get_net_udp(char **buf);
size_t get_net_unix(char **buf);
size_t get_net_raw(char **buf);

/* this is defined in cpudet-clean.c */
extern void detect_cpu(char **buf);

#endif      /* __PROC_FSYS_H__ */
