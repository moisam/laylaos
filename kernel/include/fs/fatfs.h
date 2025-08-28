/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: fatfs.h
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
 *  \file fatfs.h
 *
 *  Functions and macros for working with FAT filesystems.
 */

#ifndef __FAT_FSYS_H__
#define __FAT_FSYS_H__

#include <kernel/mutex.h>

#define FAT_ATTRIB_READONLY         0x01
#define FAT_ATTRIB_HIDDEN           0x02
#define FAT_ATTRIB_SYSTEM           0x04
#define FAT_ATTRIB_VOLUMEID         0x08
#define FAT_ATTRIB_DIRECTORY        0x10
#define FAT_ATTRIB_ARCHIVE          0x20
#define FAT_ATTRIB_LFN              0x0F

/****************************************
 * Structures found on disk
 ****************************************/

/**
 * @struct fat_boot_record_t
 * @brief The fat_boot_record_t structure.
 *
 * A structure to represent the BIOS Parameter Block (BPB).
 */
struct fat_bpb_t
{
    uint8_t         bootjmp[3];
    uint8_t         oem_name[8];
    uint16_t        bytes_per_sector;
    uint8_t         sectors_per_cluster;
    uint16_t        reserved_sector_count;
    uint8_t         table_count;
    uint16_t        root_entry_count;
    uint16_t        total_sectors_16;
    uint8_t         media_type;
    uint16_t        table_size_16;
    uint16_t        sectors_per_track;
    uint16_t        head_side_count;
    uint32_t        hidden_sector_count;
    uint32_t        total_sectors_32;
} __attribute__((packed));

/**
 * @struct fat_extbs_12_16_t
 * @brief The fat_extbs_12_16_t structure.
 *
 * A structure to represent the extended part of the Boot Block for
 * FAT 12 and 16 filesystems.
 */
struct fat_extbs_12_16_t
{
    uint8_t         bios_drive_num;
    uint8_t         reserved1;
    uint8_t         boot_signature;
    uint32_t        volume_id;
    uint8_t         volume_label[11];
    uint8_t         fat_type_label[8];
} __attribute__((packed));

/**
 * @struct fat_extbs_32_t
 * @brief The fat_extbs_32_t structure.
 *
 * A structure to represent the extended part of the Boot Block for
 * FAT 32 filesystems.
 */
struct fat_extbs_32_t
{
    uint32_t        table_size_32;
    uint16_t        extended_flags;
    uint16_t        fat_version;
    uint32_t        root_cluster;
    uint16_t        fat_info;
    uint16_t        backup_BS_sector;
    uint8_t         reserved_0[12];
    uint8_t         drive_number;
    uint8_t         reserved_1;
    uint8_t         boot_signature;
    uint32_t        volume_id;
    uint8_t         volume_label[11];
    uint8_t         fat_type_label[8];
} __attribute__((packed));

/**
 * @struct fat_bootsect_t
 * @brief The fat_bootsect_t structure.
 *
 * A structure to represent the boot sector of a FAT 12, 16 or 32 system.
 */
struct fat_bootsect_t
{
    struct fat_bpb_t                base;

    union
    {
        struct fat_extbs_12_16_t    fat12_16;
        struct fat_extbs_32_t       fat32;
    };
} __attribute__((packed));

/**
 * @struct fat_dirent_t
 * @brief The fat_dirent_t structure.
 *
 * A structure to represent a directory entry in a FAT 12, 16 or 32 system.
 */
struct fat_dirent_t
{
    uint8_t     filename[11];
    uint8_t     attribs;
    uint8_t     reserved;
    uint8_t     ctime_usec;
    uint16_t    ctime;
    uint16_t    cdate;
    uint16_t    adate;
    uint16_t    first_cluster_hi;   // 0 for FAT 12/16
    uint16_t    mtime;
    uint16_t    mdate;
    uint16_t    first_cluster_lo;
    uint32_t    size;
} __attribute__((packed));


/****************************************
 * Structures internal to the kernel
 ****************************************/

/*
 * As FAT has no notion of inode numbers, we cheat by using cluster numbers
 * as inode numbers. To avoid having to walk down the directory tree every
 * time we want to access a file/dir, we cache the first cluster number of 
 * each entry we encounter, with the first cluster number of its parent, 
 * as well as the offset inside the parent directory where the entry is 
 * located, so that we can read the parent dir to find the file.
 */
struct fat_cacheent_t
{
    size_t child_cluster;
    size_t parent_cluster;
    struct fat_cacheent_t *next;
};

/**
 * @struct fat_private_t
 * @brief The fat_private_t structure.
 *
 * Internal structure used by the kernel to cache info about FAT system,
 * such as the FAT type and the total clusters.
 */
struct fat_private_t
{
    size_t blocksz, fat_size;
    size_t total_clusters, total_sectors, data_sectors;
    size_t sectors_per_cluster;
    size_t free_clusters;
    size_t first_root_dir_sector;   // for FAT12/16
    size_t first_root_dir_cluster;  // for FAT32
    size_t root_dir_sectors;
    size_t first_fat_sector, first_data_sector;
    int fattype;
    dev_t dev;
    struct fat_cacheent_t *cacheent;
    volatile struct kernel_mutex_t lock;
};


/**
 * @var fatfs_ops
 * @brief VFAT filesystem ops.
 *
 * The VFAT filesystem operations structure.
 */
extern struct fs_ops_t fatfs_ops;


/**
 * @brief Initialize the VFAT filesystem.
 *
 * Initialize the Virtual File Allocation Table (VFAT) filesystem. This 
 * function calls fs_register() to registers the VFAT filesystem in the
 * kernel's recognized filesystem list.
 *
 * @return  nothing.
 *
 * @see     fs_register
 */
void fatfs_init(void);

/**
 * @brief Read VFAT superblock and root inode.
 *
 * Read the VFAT filesystem's superblock and root inode. This function fills
 * in the mount info struct's \a block_size, \a super, and \a root fields.
 *
 * @param   dev                 device id
 * @param   d                   pointer to the mount info struct on which
 *                                we'll mount \a dev
 * @param   bytes_per_sector    bytes per sector
 *
 * @return  zero on success, -(errno) on failure.
 */
long fatfs_read_super(dev_t dev, struct mount_info_t *d,
                      size_t bytes_per_sector);

/**
 * @brief Release the filesystem's superblock and its buffer.
 *
 * Release the VFAT filesystem's superblock and its buffer. This function is
 * called when unmounting the VFAT filesystem.
 *
 * @param   dev device id
 * @param   sb  pointer to the buffer containing the superblock's data
 *
 * @return  nothing.
 */
void fatfs_put_super(dev_t dev, struct superblock_t *sb);

/**
 * @brief Read inode data structure.
 *
 * Read inode data structure from a VFAT filesystem.
 *
 * @param   node    node struct whose inode field tells us which inode to read
 *
 * @return  zero on success, -(errno) on failure.
 */
long fatfs_read_inode(struct fs_node_t *node);

/**
 * @brief Write inode data structure.
 *
 * Write inode data structure to a VFAT filesystem.
 *
 * @param   node    node struct to write
 *
 * @return  zero on success, -(errno) on failure.
 */
long fatfs_write_inode(struct fs_node_t *node);

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
size_t fatfs_bmap(struct fs_node_t *node, size_t lblock,
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
long fatfs_free_inode(struct fs_node_t *node);

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
long fatfs_alloc_inode(struct fs_node_t *node);

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
 *                        a kmalloc'd dirent struct, and the result is
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
long fatfs_finddir(struct fs_node_t *dir, char *filename,
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
 *                        kmalloc'd dirent struct, and the result is stored in
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
long fatfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                            struct dirent **entry,
                            struct cached_page_t **dbuf, size_t *dbuf_off);

/**
 * @brief Add new entry to a directory.
 *
 * Add the given \a filename as an entry in the given parent \a dir. The new
 * entry will be assigned inode number \a n.
 *
 * @param   dir         the parent directory's node
 * @param   file        the new file's node (contains the new inode number)
 * @param   filename    the new file's name
 *
 * @return  zero on success, -(errno) on failure.
 */
long fatfs_addir(struct fs_node_t *dir, struct fs_node_t *file, char *filename);

/**
 * @brief Create a new directory.
 *
 * Make a new, empty directory by allocating a free block and initializing
 * the '.' and '..' entries to point to the current and \a parent directory
 * inodes, respectively.
 *
 * @param   parent  the parent directory
 * @param   dir     node struct containing the new directory's inode number
 *                    (the directory's link count and block[0] will be updated)
 *
 * @return  zero on success, -(errno) on failure.
 */
long fatfs_mkdir(struct fs_node_t *dir, struct fs_node_t *parent);

/**
 * @brief Remove an entry from a directory.
 *
 * Remove the given \a entry from the given parent \a dir.
 * The caller is responsible for writing dbuf to disk and calling brelse().
 *
 * @param   dir         the parent directory's node
 * @param   entry       the entry to be removed
 * @param   is_dir      non-zero if entry is a directory and this is the last 
 *                        hard link, i.e. there is no other filename referring
 *                        to the directory's inode
 *
 * @return  always zero.
 */
long fatfs_deldir(struct fs_node_t *dir, struct dirent *entry, int is_dir);

/**
 * @brief Check if a directory is empty.
 *
 * Check if the given \a dir is empty (called from rmdir()).
 *
 * @param   dir         the directory's node
 *
 * @return  1 if \a dir is empty, 0 if it is not
 */
long fatfs_dir_empty(struct fs_node_t *dir);

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
long fatfs_getdents(struct fs_node_t *dir, off_t *pos,
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
long fatfs_ustat(struct mount_info_t *d, struct ustat *ubuf);

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
long fatfs_statfs(struct mount_info_t *d, struct statfs *statbuf);

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
long fatfs_read_symlink(struct fs_node_t *link, char *buf,
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
size_t fatfs_write_symlink(struct fs_node_t *link, char *target,
                           size_t len, int kernel);

#endif      /* __FAT_FSYS_H__ */
