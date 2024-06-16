/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: vfs-defs.h
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
 *  \file vfs-defs.h
 *
 *  Filesystem, file, and inode-related structure and macro definitions.
 */

#ifndef __VFS_DEFS__
#define __VFS_DEFS__

#if !defined(__VFS_H__) && !defined(__KERNEL_PCACHE_H__)
#error "do not include vfs-defs.h directly, include vfs.h instead"
#endif

#include <kernel/bits/pcache-defs.h>
#include <sys/statfs.h>
#include <poll.h>
#include <kernel/select.h>

#ifndef __STRUCT_USTAT_DEFINED__
#define __STRUCT_USTAT_DEFINED__

typedef fsblkcnt_t __daddr_t;

struct ustat
{
    __daddr_t f_tfree;      /* Total free blocks */
    ino_t   f_tinode;     /* Number of free inodes */
    char    f_fname[6];   /* Filsys name */
    char    f_fpack[6];   /* Filsys pack name */
};

#endif      /* !__STRUCT_USTAT_DEFINED__ */

/*
 * Forward declarations
 */
struct fs_node_t;
struct file_t;
struct dirent;
struct mount_info_t;


struct superblock_t
{
	virtual_addr       data;        /**< physical address of buffer data */
	unsigned long      blocksz;     /**< buffer size */
	unsigned long      blockno;     /**< LBA address of disk block to read
	                                        or write */
	dev_t              dev;         /**< device id */
};


/**
 * @struct fs_ops_t
 * @brief The fs_ops_t structure.
 *
 * A structure holding pointers to different filesystem operations.
 */
struct fs_ops_t
{
    /*
     * inode operations
     */
    int (*read_inode)(struct fs_node_t *);              /**< read inode */
    int (*write_inode)(struct fs_node_t *);             /**< write inode */
    int (*trunc_inode)(struct fs_node_t *, size_t);     /**< truncate inode */
    int (*alloc_inode)(struct fs_node_t *);         /**< allocate new inode */
    int (*free_inode)(struct fs_node_t *);          /**< free inode */

    size_t (*bmap)(struct fs_node_t *, size_t, 
                                       size_t, int);    /**< bmap function */

    int (*read_symlink)(struct fs_node_t *, char *, 
                        size_t, int);       /**< read symbolic link */

    size_t (*write_symlink)(struct fs_node_t *, char *, 
                            size_t, int);   /**< write symbolic link */

    /*
     * directory operations
     */
    int (*finddir)(struct fs_node_t *, char *, struct dirent **,
                   struct cached_page_t **, size_t *);  /**< find file in dir */

    int (*finddir_by_inode)(struct fs_node_t *, struct fs_node_t *,
                            struct dirent **, struct cached_page_t **, 
                            size_t *);      /**< find inode in dir */

    int (*addir)(struct fs_node_t *, char *, ino_t);    /**< add file to dir */
    int (*mkdir)(struct fs_node_t *, ino_t parent);     /**< make new dir */

    int (*deldir)(struct fs_node_t *, struct dirent *,
                  struct cached_page_t *, size_t);  /**< delete file from dir */

    int (*dir_empty)(struct fs_node_t *);         /**< check dir is empty */

    /*
     * device operations
     */
    int (*mount)(struct mount_info_t *, int, char *); /**< mount device */
    int (*umount)(struct mount_info_t *);             /**< unmount device */

    int (*read_super)(dev_t, struct mount_info_t *, 
                             size_t);                 /**< read superblock */

    int (*write_super)(dev_t, struct superblock_t *);  /**< write superblock */
    void (*put_super)(dev_t, struct superblock_t *);   /**< put superblock */

    int (*ustat)(struct mount_info_t *, struct ustat *);    /**< get stats */
    int (*statfs)(struct mount_info_t *, struct statfs *);  /**< more stats */

    int (*getdents)(struct fs_node_t *, off_t *, 
                                        void *, int);  /**< get dir entries */
};

/**
 * @struct fs_info_t
 * @brief The fs_info_t structure.
 *
 * A structure to represent a registered filesystem.
 */
struct fs_info_t
{
    char name[8];           /**< filesystem name */
    unsigned int index;     /**< index in filesystem table */
    struct fs_ops_t *ops;   /**< pointer to filesystem operations struct */
};


/**
 * @struct mount_info_t
 * @brief The mount_info_t structure.
 *
 * A structure to represent mount table entries.
 */
struct mount_info_t
{
    dev_t dev;                  /**< The device on which the 
                                     filesystem resides */
	unsigned long block_size;   /**< The logical block size of the filesystem 
	                                 is held here. Each filesystem could 
	                                 typically support multiple different 
	                                 block sizes as the unit of allocation to 
	                                 a file */
    struct fs_node_t *root;     /**< With the exception of the root 
                                     filesystem, this field points to the 
                                     inode on which the filesystem is mounted.
                                     This is used during pathname traversal */
    struct fs_node_t *mpoint;   /**< The root inode for this filesystem */
    struct fs_info_t *fs;       /**< Pointer to filesystem info struct */
    struct superblock_t *super; /**< Pointer to superblock buffer */
    //struct kernel_mutex_t superblock_mutex;     /**< superblock lock */

#define FS_SUPER_DIRTY      0x01
//#define FS_SUPER_RDONLY     0x02
    int flags;                  /**< Filesystem flags */

    int mountflags;             /**< Mount flags */

    char *mountopts;            /**< Copy of the user-supplied mount options */

    /* fields for housekeeping */
#define NR_FREE_CACHE       100
    int nfree;                  /**< Number of incore free blocks
                                     (between 0 and 100) */
    ino_t free[NR_FREE_CACHE];  /**< incore free blocks */
    int ninode;                 /**< Number of incore inodes (0-100) */
    ino_t inode[NR_FREE_CACHE]; /**< incore free inodes */
    struct kernel_mutex_t flock;    /**< Lock used during free list access */
    struct kernel_mutex_t ilock;    /**< Lock used during inode list access */
    time_t update_time;             /**< Time of last update */
};

/**
 * @struct fs_node_t
 * @brief The fs_node_t structure.
 *
 * A structure to represent an incore (in-memory) node.
 */
struct fs_node_t
{
    dev_t dev;          /**< devid of device containing this inode */
    ino_t inode;        /**< inode number */
    struct mount_info_t *minfo;     /**< device mount info for quick access */
    unsigned short refs;    /**< reference count */

    mode_t mode;        /**< access mode */
    uid_t uid;          /**< user id */
    time_t mtime;       /**< modification time */
    time_t atime;       /**< access time */
    time_t ctime;       /**< creation time */
    size_t size;        /**< file size (for pipes, the pipe's virtual 
                             memory address) */
    unsigned int links; /**< hard link count */
    gid_t gid;          /**< group id */
    unsigned long blocks[15];   /**< pointer to disk blocks (for pipes, 
                                     [0] and [1] point to pipe head/tail */
    struct kernel_mutex_t lock; /**< struct lock */
    struct kernel_mutex_t sleeping_task;    /**< waiting task sleep channel */

#define FS_NODE_DIRTY       0x01
#define FS_NODE_PIPE        0x02
#define FS_NODE_MOUNTPOINT  0x04
#define FS_NODE_SOCKET      0x08
    unsigned int flags;     /**< node flags */
    struct fs_ops_t *ops;   /**< pointer to filesystem operations struct */
    struct fs_node_t *ptr;  /**< alias pointer for symlinks and mount-points */
    struct fs_node_t *next; /**< used by tmpfs to find next node in a
                                 tmpfs device */
    
    void *data;             /**< used by sockets (ptr to struct socket) */

    int (*poll)(struct file_t *, struct pollfd *);  /**< polling function */
    int (*select)(struct file_t *f, int which);     /**< select function */
    ssize_t (*read)(struct file_t *, off_t *,
                  unsigned char *, size_t, int);    /**< read function */
    ssize_t (*write)(struct file_t *, off_t *,
                  unsigned char *, size_t, int);    /**< write function */

    struct selinfo select_channel;  /**< used by pipes to select/poll */
    
    struct alock_t *alocks;         /**< queue of advisory locks */
};

struct fs_node_header_t
{
    dev_t dev;          /**< devid of device containing this inode */
    ino_t inode;        /**< inode number */
    struct mount_info_t *minfo;     /**< device mount info for quick access */
};

/**
 * @struct file_t
 * @brief The file_t structure.
 *
 * A structure to represent files open by tasks (references the
 * fs_node_t struct).
 */
struct file_t
{
#define PREAD_MODE          1   // for pipes: to identify the reading fd
#define PWRITE_MODE         2   // for pipes: to identify the writing fd
    unsigned short mode;        /**< access mode */
    unsigned int flags;         /**< flags */
    unsigned short refs;        /**< reference count */
    struct fs_node_t *node;     /**< pointer to incore node */
    off_t pos;                  /**< read/write position in file */
    struct kernel_mutex_t lock; /**< struct lock */
};

#endif      /* __VFS_DEFS__ */
