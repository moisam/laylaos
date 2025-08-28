/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: ext2.h
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
 *  \file ext2.h
 *
 *  Include header file for working with the ext2 filesystem.
 */

#ifndef __EXT2_FSYS_H__
#define __EXT2_FSYS_H__

#include <stdint.h>


/**
 * @struct ext2_superblock_t
 * @brief The ext2_superblock_t structure.
 *
 * A structure to represent the superblock of an ext2 filesystem.
 */
struct ext2_superblock_t
{
    uint32_t total_inodes;  /**<  total number of inodes in the filesystem */
    uint32_t total_blocks;  /**<  total number of blocks in the filesystem */
    uint32_t reserved_blocks;   /**<  number of blocks reserved for superuser */
    uint32_t unalloc_blocks;    /**<  total number of unallocated blocks */
    uint32_t unalloc_inodes;    /**<  total number of unallocated inodes */
    uint32_t superblock_block;  /**<  block number of the block containing 
                                      the superblock */
    uint32_t log2_block_size;   /**<  log2 (block size) - 10. (that is, the
                                      number to shift 1,024 to the left by to
                                      obtain the block size */
    uint32_t log2_fragment_size;    /**<  log2 (fragment size) - 10. (that is,
                                          the number to shift 1,024 to the left
                                          by to obtain the fragment size) */
    uint32_t blocks_per_group;  /**<  number of blocks in each block group */
    uint32_t fragments_per_group;   /**<  number of fragments in each
                                          block group */
    uint32_t inodes_per_group;  /**<  number of inodes in each block group */
    uint32_t last_mount_time;   /**<  last mount time */
    uint32_t last_written_time; /**<  last written time */
    uint16_t mounts_since_last_check;   /**<  number of times the volume has
                                              been mounted since its last
                                              consistency check (using fsck) */
    uint16_t mounts_before_check;   /**<  number of mounts allowed before a
                                          consistency check (using fsck) must
                                          be done */
    uint16_t signature;			    /**<  Ext2 signature (0xEF53) */
    uint16_t filesystem_state;		/**<  file system state:
                                            1 = clean,
                                            2 = has errors */
    uint16_t on_error_do;			/**<  what to do when an error is detected:
                                            1 = ignore,
                                            2 = remount as R/O,
                                            3 = kernel panic */
    uint16_t version_min;   /**<  minor part of the version */
    uint32_t last_check_time;   /**<  time of last consistency check
                                      (using fsck) */
    uint32_t check_interval;    /**<  interval between forced consistency
                                      checks (using fsck) */
    uint32_t sys_id;    /**<  operating system ID from which the filesystem
                              was created:
                                0 = Linux,
                                1 = GNU Hurd,
                                2 = MASIX, 
                                3 = FreeBSD,
                                4 = Other "Lites" e.g. NetBSD, OpenBSD */
    uint32_t version_major; /**<  major part of the version */
    uint16_t reserved_uid;  /**<  user ID that can use reserved blocks */
    uint16_t reserved_gid;  /**<  group ID that can use reserved blocks */

    /* Extended SuperBlock        */
    /* only if version_major >= 1 */
    uint32_t first_nonreserved_inode;   /**<  first non-reserved inode in
                                              filesystem (fixed as 11 for
                                                          versions < 1.0) */
    uint16_t inode_size;    /**<  size of inode structure in bytes (fixed as
                                  128 for versions < 1.0) */
    uint16_t block_group;   /**<  block group that this superblock is part of
                                  (if backup copy) */
    uint32_t optional_features; /**<  optional features present */
    uint32_t required_features; /**<  required features present */
    uint32_t readonly_features; /**<  features that if not supported, the 
                                      volume must be mounted read-only) */
    char filesystem_id[16];     /**<  filesystem ID */
    char volume_label[16];      /**<  volume name (null-terminated) */
    char last_mount_path[64];   /**<  path volume was last mounted to
                                      (null-terminated) */
    uint32_t compression;   /**<  compression algorithms used */
    uint8_t file_prealloc;  /**<  number of blocks to preallocate for files */
    uint8_t dir_prealloc;   /**<  number of blocks to preallocate for dirs */
    uint16_t reserved;      /**<  Reserved */
    char journal_id[16];    /**<  journal ID (same style as filesystem ID) */
    uint32_t journal_inode; /**<  journal inode */
    uint32_t journal_device;    /**<  journal device */
    uint32_t orphan_list_head;  /**<  head of orphan inode list */
    /* rest of 1024 bytes are unused */
} __attribute__((packed));


/**
 * @struct block_group_desc_t
 * @brief The block_group_desc_t structure.
 *
 * A structure to represent a Block Group Descriptor.
 */
struct block_group_desc_t
{
    uint32_t block_bitmap_addr; /**<  block address of block usage bitmap */
    uint32_t inode_bitmap_addr; /**<  block address of inode usage bitmap */
    uint32_t inode_table_addr;  /**<  starting block address of inode table */
    uint16_t unalloc_blocks;    /**<  number of unallocated blocks in group */
    uint16_t unalloc_inodes;    /**<  number of unallocated inodes in group */
    uint16_t dir_count; /**<  number of directories in group */
    char unused[14];    /**<  Reserved */
} __attribute__((packed));


/**
 * @struct inode_data_t
 * @brief The inode_data_t structure.
 *
 * A structure to represent an inode on disk.
 */
struct inode_data_t
{
    uint16_t permissions;   /**<  type and permissions */
    uint16_t user_id;       /**<  user id */
    uint32_t size_lsb;      /**<  lower 32 bits of size in bytes */
    uint32_t last_access_time;  /**<  last access time */
    uint32_t creation_time;     /**<  creation time */
    uint32_t last_modification_time;    /**<  last modification time */
    uint32_t deletion_time;     /**<  deletion time */
    uint16_t group_id;      /**<  group id */
    uint16_t hard_links;    /**<  count of hard links. When this reaches 0,
                                  data blocks are marked as unallocated */
    uint32_t disk_sectors;  /**<  count of disk sectors (not Ext2 blocks) in
                                  use by this inode, not counting the actual
                                  inode structure or directory entries linking
                                  to the inode */
    uint32_t flags;         /**<  inode flags */
    uint32_t OS_specific1;  /**<  OS-specific value 1 */
    uint32_t block_p[12];   /**<  12 direct block pointers */
    uint32_t single_indirect_pointer;   /**<  single indirect block pointer */
    uint32_t double_indirect_pointer;   /**<  double indirect block pointer */
    uint32_t triple_indirect_pointer;   /**<  triple indirect block pointer */
    uint32_t generation_number; /**<  generation number (used for NFS) */
    uint32_t ext_attribute_block;   /**<  extended attribute block (for
                                          version >= 1) */
    uint32_t size_msb;  /**<  for version >= 1, the upper 32 bits of file size
                             (if feature bit set) if it's a file, or directory
                             ACL if it's a directory */
    uint32_t fragment_block_addr;   /**<  block address of fragment */
    uint8_t  OS_specific2[12];      /**<  OS-specific value 2 */
} __attribute__((packed));


/**
 * @struct ext2_dirent_t
 * @brief The ext2_dirent_t structure.
 *
 * A structure to represent a directory entry on disk.
 */
struct ext2_dirent_t
{
    uint32_t inode;         /**<  inode number */
    uint16_t entry_size;    /**<  total size of this entry */
    uint8_t  name_length_lsb;   /**<  name length least-significant 8 bits */
    uint8_t  type_indicator;    /**<  type indicator (if the feature bit for
                                      "directory entries have file type byte"
                                      is set, otherwise the most-significant 8
                                      bits of the name length) */
} __attribute__((packed));


#define MAX_HARD_LINKS          256 /**<  max number of hard links */


/* magic superblock field value(s) */
#undef EXT2_SUPER_MAGIC
#define EXT2_SUPER_MAGIC        0xEF53  /**<  ext2 filesystem magic number */

/* filesystem_state superblock field value(s) */
#define EXT2_VALID_FS           1   /**<  filesystem unmounted cleanly */
#define EXT2_ERROR_FS           2   /**<  errors detected (not unmounted
                                          cleanly) */

/* on_error_do superblock field value(s) */
#define EXT2_ERRORS_CONTINUE    1   /**<  ignore and carry on  */
#define EXT2_ERROR_RO           2   /**<  remount as R/O       */
#define EXT2_ERROR_PANIC        3   /**<  cause a kernel panic */

/* sys_id superblock field value(s) */
#define EXT2_OS_LINUX           0   /**<  Linux     */
#define EXT2_OS_HURD            1   /**<  GNU HURD  */
#define EXT2_OS_MASIX           2   /**<  MASIX     */
#define EXT2_OS_FREEBSD         3   /**<  FreeBSD   */
#define EXT2_OS_LITES           4   /**<  Lites     */

/* s_feature_compat superblock field value(s) */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC    0x0001  //Block pre-allocation for new directories
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES   0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL     0x0004  //An Ext3 journal exists
#define EXT2_FEATURE_COMPAT_EXT_ATTR        0x0008  //Extended inode attributes are present
#define EXT2_FEATURE_COMPAT_RESIZE_INO      0x0010  //Non-standard inode size used
#define EXT2_FEATURE_COMPAT_DIR_INDEX       0x0020  //Directory indexing (HTree)

/* s_feature_incompat superblock field value(s) */
#define EXT2_FEATURE_INCOMPAT_COMPRESSION   0x0001  //Disk/File compression is used
#define EXT2_FEATURE_INCOMPAT_FILETYPE      0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER       0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV   0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG       0x0010

/* s_feature_ro_compat superblock field value(s) */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001  //Sparse Superblock
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002  //Large file support, 64-bit file size
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR    0x0004  //Binary tree sorted directory files

/* reserved inodes */
#define EXT2_BAD_INO            1 /**<  bad blocks inode */
#define EXT2_ROOT_INO           2 /**<  root directory inode */
#define EXT2_ACL_IDX_INO        3 /**<  ACL index inode (deprecated?) */
#define EXT2_ACL_DATA_INO       4 /**<  ACL data inode (deprecated?) */
#define EXT2_BOOT_LOADER_INO    5 /**<  boot loader inode */
#define EXT2_UNDEL_DIR_INO      6 /**<  undelete directory inode */

/*
 * Values for the type_indicator field of the ext2_dirent_t struct.
 */
#define EXT2_FT_UNKNOWN     0   /**< Unknown File Type */
#define EXT2_FT_REG_FILE    1   /**< Regular File      */
#define EXT2_FT_DIR         2   /**< Directory File    */
#define EXT2_FT_CHRDEV      3   /**< Character Device  */
#define EXT2_FT_BLKDEV      4   /**< Block Device      */
#define EXT2_FT_FIFO        5   /**< Buffer File       */
#define EXT2_FT_SOCK        6   /**< Socket File       */
#define EXT2_FT_SYMLINK     7   /**< Symbolic Link     */

/*
 * Values for the flags field of the inode_data_t struct.
 */
#define EXT2_SECRM_FL           0x00000001  // secure deletion
#define EXT2_UNRM_FL            0x00000002  // record for undelete
#define EXT2_COMPR_FL           0x00000004  // compressed file
#define EXT2_SYNC_FL            0x00000008  // synchronous updates
#define EXT2_IMMUTABLE_FL       0x00000010  // immutable file
#define EXT2_APPEND_FL          0x00000020  // append only
#define EXT2_NODUMP_FL          0x00000040  // do not dump/delete file
#define EXT2_NOATIME_FL         0x00000080  // do not update .i_atime
#define EXT2_DIRTY_FL           0x00000100  // Dirty (modified)
#define EXT2_COMPRBLK_FL        0x00000200  // compressed blocks
#define EXT2_NOCOMPR_FL         0x00000400  // access raw compressed data
#define EXT2_ECOMPR_FL          0x00000800  // compression error
#define EXT2_BTREE_FL           0x00001000  // b-tree format directory
#define EXT2_INDEX_FL           0x00001000  // hash indexed directory
#define EXT2_IMAGIC_FL          0x00002000  // AFS directory
#define EXT3_JOURNAL_DATA_FL    0x00004000  // journal file data
#define EXT2_RESERVED_FL        0x80000000  // reserved for ext2 library

/**
 * \def EXT2_MAX_FILENAME_LEN
 *
 * max file name length
 */
#define EXT2_MAX_FILENAME_LEN   255


/**
 * @var ext2fs_ops
 * @brief ext2 filesystem ops.
 *
 * The ext2 filesystem operations structure.
 */
extern struct fs_ops_t ext2fs_ops;

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
uint32_t ext2_alloc(dev_t dev);

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
void ext2_free(dev_t dev, uint32_t block_no);

/**
 * @brief Update inode bitmap.
 *
 * Update inode bitmap on disk.
 *
 * @param   n           count of inodes to update on disk
 * @param   arr         inode numbers which we want to update on disk
 *                        (updated inode numbers are zeroed out as they are
 *                         updated, so the caller shouldn't rely on this
 *                         array's contents after this call!)
 * @param   dev         device id
 * @param   op          operation to perform: 1 to free inodes, 2 to set them
 *
 * @return  zero on success, -1 on failure.
 */
int update_inode_bitmap(int n, uint32_t *arr, dev_t dev, char op);

/**
 * @brief Update block bitmap.
 *
 * Update block bitmap on disk.
 *
 * @param   n           count of blocks to update on disk
 * @param   arr         block numbers which we want to update on disk
 *                        (updated block numbers are zeroed out as they are
 *                         updated, so the caller shouldn't rely on this
 *                         array's contents after this call!)
 * @param   dev         device id
 * @param   op          operation to perform: 1 to free blocks, 2 to set them
 *
 * @return  zero on success, -1 on failure.
 */
int update_block_bitmap(int n, uint32_t *arr, dev_t dev, char op);

/**
 * @brief Find free inodes.
 *
 * Search an ext2 filesystem for free inode numbers. The function returns at
 * most \a n inode numbers, and the list of found inodes is returned in
 * \a needed_inodes.
 *
 * @param   dev             device id
 * @param   n               maximum number of inode numbers to return
 * @param   needed_inodes   list of free inodes is returned here
 *
 * @return  number of free inodes on success, 0 on failure.
 */
int find_free_inodes(dev_t dev, int n, ino_t *needed_inodes);

/**
 * @brief Find free disk blocks.
 *
 * Search an ext2 filesystem for free disk blocks. The function returns at
 * most \a n block numbers, and the list of found blocks is returned in
 * \a needed_blocks.
 *
 * @param   dev             device id
 * @param   n               maximum number of blocks to return
 * @param   needed_blocks   list of free blocks is returned here
 *
 * @return  number of free inodes on success, 0 on failure.
 */
int find_free_blocks(dev_t dev, int n, ino_t *needed_blocks);

/**
 * @brief Read ext2 superblock and root inode.
 *
 * Read the ext2 filesystem's superblock and root inode. This function fills
 * in the mount info struct's \a block_size, \a super, and \a root fields.
 *
 * @param   dev                 device id
 * @param   d                   pointer to the mount info struct on which
 *                                we'll mount \a dev
 * @param   bytes_per_sector    bytes per sector
 *
 * @return  zero on success, -(errno) on failure.
 */
long ext2_read_super(dev_t dev, struct mount_info_t *d, size_t bytes_per_sector);

/**
 * @brief Write the filesystem's superblock to disk.
 *
 * Write the ext2 filesystem's superblock to disk.
 *
 * @param   dev device id
 * @param   sb  pointer to the buffer containing the superblock's data
 *
 * @return  zero on success, -(errno) on failure.
 */
long ext2_write_super(dev_t dev, struct superblock_t *sb);

/**
 * @brief Release the filesystem's superblock and its buffer.
 *
 * Release the ext2 filesystem's superblock and its buffer. This function is
 * called when unmounting the ext2 filesystem.
 *
 * @param   dev device id
 * @param   sb  pointer to the buffer containing the superblock's data
 *
 * @return  nothing.
 */
void ext2_put_super(dev_t dev, struct superblock_t *sb);

/**
 * @brief Read inode data structure.
 *
 * Read inode data structure from an ext2 filesystem.
 *
 * @param   node    node struct whose inode field tells us which inode to read
 *
 * @return  zero on success, -(errno) on failure.
 */
long ext2_read_inode(struct fs_node_t *node);

/**
 * @brief Write inode data structure.
 *
 * Write inode data structure to an ext2 filesystem.
 *
 * @param   node    node struct to write
 *
 * @return  zero on success, -(errno) on failure.
 */
long ext2_write_inode(struct fs_node_t *node);

/**
 * @brief Allocate a new inode.
 *
 * Allocate a new inode number and mark it as used in the disk's inode bitmap.
 *
 * This function also updates the mount info struct's free inode pool if all
 * the cached inode numbers have been used by searching the disk for free
 * inode numbers.
 *
 * @param   new_node    node struct in which we'll store the new alloc'd 
 *                        inode number
 *
 * @return  zero on success, -(errno) on failure.
 */
long ext2_alloc_inode(struct fs_node_t *new_node);

/**
 * @brief Free an inode.
 *
 * Free an inode and update the inode bitmap on disk.
 *
 * @param   node    node struct to free
 *
 * @return  zero on success, -(errno) on failure.
 */
long ext2_free_inode(struct fs_node_t *node);

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
size_t ext2_bmap(struct fs_node_t *node, size_t lblock,
                 size_t block_size, int flags);

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
long ext2_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
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
long ext2_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
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
long ext2_addir(struct fs_node_t *dir, struct fs_node_t *file, char *filename);

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
long ext2_mkdir(struct fs_node_t *dir, struct fs_node_t *parent);

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
long ext2_deldir(struct fs_node_t *dir, struct dirent *entry, int is_dir);

/**
 * @brief Check if a directory is empty.
 *
 * Check if the given \a dir is empty (called from rmdir()).
 *
 * @param   dir         the directory's node
 *
 * @return  1 if \a dir is empty, 0 if it is not
 */
long ext2_dir_empty(struct fs_node_t *dir);

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
long ext2_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz);

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
long ext2_ustat(struct mount_info_t *d, struct ustat *ubuf);

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
long ext2_statfs(struct mount_info_t *d, struct statfs *statbuf);

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
long ext2_read_symlink(struct fs_node_t *link, char *buf,
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
size_t ext2_write_symlink(struct fs_node_t *link, char *target,
                          size_t len, int kernel);

/*
 * Internal functions.
 */

long ext2_mkdir_internal(struct fs_node_t *dir, ino_t parent, 
                         int ext_dir_type, size_t block_size);
long ext2_dir_empty_internal(char *module, struct fs_node_t *dir);
long ext2_getdents_internal(struct fs_node_t *dir, off_t *pos, void *buf,
                            int bufsz, int ext_dir_type);
long ext2_finddir_internal(struct fs_node_t *dir, char *filename,
                           struct dirent **entry, struct cached_page_t **dbuf,
                           size_t *dbuf_off, int ext_dir_type);
long ext2_finddir_by_inode_internal(struct fs_node_t *dir,
                                    struct fs_node_t *node,
                                    struct dirent **entry,
                                    struct cached_page_t **dbuf,
                                    size_t *dbuf_off, int ext_dir_type);
long ext2_addir_internal(struct fs_node_t *dir, struct fs_node_t *file,
                         char *filename, int ext_dir_type, size_t block_size);
long ext2_deldir_internal(struct fs_node_t *dir, struct dirent *entry, int ext_dir_type);

int matching_node(dev_t dev, ino_t ino, struct fs_node_t *node);

#endif      /* __EXT2_FSYS_H__ */
