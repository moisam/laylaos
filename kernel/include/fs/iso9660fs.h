/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: iso9660fs.h
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
 *  \file iso9660fs.h
 *
 *  Functions and macros for working with the ISO9660 filesystem.
 */

#ifndef __ISO9660_FSYS_H__
#define __ISO9660_FSYS_H__

#include <kernel/pcache.h>


/**
 * @struct lebe_dword_t
 * @brief The lebe_dword_t structure.
 *
 * A structure to represent a double word (4 bytes) in both little-endian
 * and big-endian formats.
 */
struct lebe_dword_t
{
    uint32_t little;
    uint32_t big;
} __attribute__((packed));


/**
 * @struct lebe_word_t
 * @brief The lebe_word_t structure.
 *
 * A structure to represent a word (2 bytes) in both little-endian
 * and big-endian formats.
 */
struct lebe_word_t
{
    uint16_t little;
    uint16_t big;
} __attribute__((packed));


/**
 * @struct iso9660_datetime
 * @brief The iso9660_datetime structure.
 *
 * A structure to represent an ISO9660 data and time.
 */
struct iso9660_datetime
{
    uint8_t yr;     /**<  years since 1900 */
    uint8_t mon;    /**<  month 1-12 */
    uint8_t day;    /**<  day of month 1-31 */
    uint8_t hr;     /**<  hour 0-23 */
    uint8_t min;    /**<  minutes 0-59 */
    uint8_t sec;    /**<  seconds 0-59 */
    uint8_t gmtoff; /**<  GMT offset in 15 min intervls from -48 to +52 */
};


/**
 * @struct iso9660_dirent_t
 * @brief The iso9660_dirent_t structure.
 *
 * A structure to represent an ISO9660 directory entry.
 */
struct iso9660_dirent_t
{
    uint8_t reclen;     /**<  Length of Directory Record */
    uint8_t extreclen;  /**<  Extended Attribute Record length */
    struct lebe_dword_t lba;    /**<  Location of extent (LBA) in
                                      both-endian formats */
    struct lebe_dword_t size;   /**<  Data length (size of extent) in
                                      both-endian format */
    uint8_t datetime[7];    /**<  Recording date and time */
    uint8_t flags;          /**<  File flags */
    uint8_t unitsize;   /**<  File unit size for files recorded in 
                              interleaved mode, zero otherwise */
    uint8_t gapsize;    /**<  Interleave gap size for files recorded in 
                              interleaved mode, zero otherwise */
    struct lebe_word_t seqnum;  /**<  Volume sequence number - the volume
                                      that this extent is recorded on, in
                                      16 bit both-endian format */
    uint8_t namelen;    /**<  Length of filename, ending with ';' followed
                              by the file ID number in ASCII coded decimal */
} __attribute__((packed));


/**
 * @struct iso9660_pvd_t
 * @brief The iso9660_pvd_t structure.
 *
 * A structure to represent the Primary Volume Descriptor (PVD).
 */
struct iso9660_pvd_t
{
    uint8_t type;       /**<  Always 0x01 for a Primary Volume Descriptor */
    uint8_t stdid[5];   /**<  Always 'CD001' */
    uint8_t ver;        /**<  Always 0x01 */
    uint8_t unused1;    /**<  Reserved */
    uint8_t sysid[32];  /**<  The name of the system that can act upon sectors
                              0x00-0x0F for the volume */
    uint8_t volid[32];  /**<  Volume identifier */
    uint8_t unused2[8]; /**<  Reserved */
    struct lebe_dword_t blocks; /**<  Number of Logical Blocks in which the 
                                      volume is recorded */
    uint8_t unused3[32];        /**<  Reserved */
    struct lebe_word_t volset_size; /**<  The size of the set in this logical
                                          volume (number of disks) */
    struct lebe_word_t vol_seqnum;  /**<  The number of this disk in the
                                          Volume Set */
    struct lebe_word_t block_size;  /**<  The size in bytes of a
                                          logical block */
    struct lebe_dword_t pathtab_size;   /**<  The size in bytes of the
                                              path table */
    uint32_t pathtab_lba_lsb;       /**<  LBA location of the path table.
                                          The path table pointed to contains
                                          only little-endian values */
    uint32_t opt_pathtab_lba_lsb;   /**<  LBA location of the optional path
                                          table. The path table pointed to 
                                          contains only little-endian values.
                                          Zero means that no optional path
                                          table exists */
    uint32_t pathtab_lba_msb;       /**<  LBA location of the path table. 
                                          The path table pointed to contains 
                                          only big-endian values */
    uint32_t opt_pathtab_lba_msb;   /**<  LBA location of the optional path
                                          table. The path table pointed to
                                          contains only big-endian values. 
                                          Zero means that no optional path 
                                          table exists */
    struct iso9660_dirent_t root;   /**<  This is not an LBA address, but the
                                          actual Directory Record, which 
                                          contains a single byte Directory
                                          Identifier (0x00) */
    uint8_t root_padding;           /**<  Padding */
    uint8_t volsetid[128];      /**<  Identifier of the volume set */
    uint8_t publisherid[128];   /**<  The volume publisher. For extended 
                                      publisher info, first byte is 0x5F, 
                                      followed by a filename in the root
                                      dir. If not specified, all bytes
                                      should be 0x20 */
    uint8_t dataprepid[128];    /**< The identifier of the person(s) who 
                                     prepared the data for this volume. 
                                     Extended preparation info has format
                                     similar to the publisher above */
    uint8_t appid[128];         /**< How the data are recorded on this volume.
                                     Extended info has format similar to
                                     the above */
    uint8_t copyrightid[37];    /**<  Filename of a file in the root dir that 
                                      contains copyright info. If not 
                                      specified, all bytes should be 0x20 */
    uint8_t abstractid[37];     /**<  Filename of a file in the root dir that 
                                      contains abstract info. If not 
                                      specified, all bytes should be 0x20 */
    uint8_t biblioid[37];       /**<  Filename of a file in the root dir that 
                                      contains bibliographic info. If not 
                                      specified, all bytes should be 0x20 */
    struct iso9660_datetime ctime;  /**<  Volume creation date & time */
    struct iso9660_datetime mtime;  /**<  Volume modification date & time */
    struct iso9660_datetime exptime; /**<  Date & time after which this 
                                           volume becomes obsolete. If not 
                                           specified, the volume does not
                                           expire */
    struct iso9660_datetime efftime; /**<  Date and time after which the 
                                           volume may be used. If not 
                                           specified, may be used now */
    uint8_t fstruct_ver;    /**<  The dir records and path table version
                                  (always 0x01) */
    uint8_t unused4;        /**<  Reserved */
    uint8_t appdata[512];   /**<  Contents not defined by ISO 9660 */
    uint8_t reserved[653];  /**<  Reserved by ISO */
} __attribute__((packed));


/**
 * @brief Initialize the ISO9660 filesystem.
 *
 * Initialize the ISO9660 filesystem. This function calls fs_register()
 * to registers the ISO9660 filesystem in the kernel's recognized filesystem
 * list.
 *
 * @return  nothing.
 *
 * @see     fs_register
 */
void iso9660fs_init(void);

/**
 * @brief Read ISO9660 superblock and root inode.
 *
 * Read the ISO9660 filesystem's superblock and root inode. This function fills
 * in the mount info struct's \a block_size, \a super, and \a root fields.
 *
 * @param   dev                 device id
 * @param   d                   pointer to the mount info struct on which
 *                                we'll mount \a dev
 * @param   bytes_per_sector    bytes per sector
 *
 * @return  zero on success, -(errno) on failure.
 */
int iso9660fs_read_super(dev_t dev, struct mount_info_t *d,
                         size_t bytes_per_sector);

/**
 * @brief Release the filesystem's superblock and its buffer.
 *
 * Release the ISO9660 filesystem's superblock and its buffer. This function is
 * called when unmounting the ISO9660 filesystem.
 *
 * @param   dev device id
 * @param   sb  pointer to the buffer containing the superblock's data
 *
 * @return  nothing.
 */
void iso9660fs_put_super(dev_t dev, struct superblock_t *sb);

/**
 * @brief Read inode data structure.
 *
 * Read inode data structure from an ISO9660 filesystem.
 *
 * @param   node    node struct whose inode field tells us which inode to read
 *
 * @return  zero on success, -(errno) on failure.
 */
int iso9660fs_read_inode(struct fs_node_t *node);

/**
 * @brief Map logical block to disk block.
 *
 * Map file position to disk block number using inode struct's block pointers.
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
size_t iso9660fs_bmap(struct fs_node_t *node, size_t lblock,
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
int iso9660fs_free_inode(struct fs_node_t *node);

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
int iso9660fs_alloc_inode(struct fs_node_t *node);

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
void iso9660fs_free(dev_t dev, uint32_t block_no);

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
uint32_t iso9660fs_alloc(dev_t dev);

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
int iso9660fs_finddir(struct fs_node_t *dir, char *filename,
                      struct dirent **entry, struct cached_page_t **dbuf,
                      size_t *dbuf_off);

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
int iso9660fs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
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
int iso9660fs_addir(struct fs_node_t *dir, char *filename, ino_t n);

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
int iso9660fs_mkdir(struct fs_node_t *dir, ino_t parent);

/**
 * @brief Remove an entry from a directory.
 *
 * Remove the given \a entry from the given parent \a dir.
 * The caller is responsible for writing dbuf to disk and calling brelse().
 *
 * @param   dir         the parent directory's node
 * @param   entry       the entry to be removed
 * @param   dbuf        the disk buffer representing the disk block containing
 *                        the entry we want to remove (we get this from an 
 *                        earlier call to finddir)
 * @param   dbuf_off    the offset in dbuf->data at which the caller can find
 *                        the entry to be removed
 *
 * @return  always zero.
 */
int iso9660fs_deldir(struct fs_node_t *dir, struct dirent *entry,
                     struct cached_page_t *dbuf, size_t dbuf_off);

/**
 * @brief Check if a directory is empty.
 *
 * Check if the given \a dir is empty (called from rmdir()).
 *
 * @param   dir         the directory's node
 *
 * @return  1 if \a dir is empty, 0 if it is not
 */
int iso9660fs_dir_empty(struct fs_node_t *dir);

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
int iso9660fs_getdents(struct fs_node_t *dir, off_t *pos,
                       void *buf, int bufsz);

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
int iso9660fs_ustat(struct mount_info_t *d, struct ustat *ubuf);

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
int iso9660fs_statfs(struct mount_info_t *d, struct statfs *statbuf);

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
int iso9660fs_read_symlink(struct fs_node_t *link, char *buf,
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
size_t iso9660fs_write_symlink(struct fs_node_t *link, char *target,
                               size_t len, int kernel);

#endif      /* __ISO9660_FSYS_H__ */
