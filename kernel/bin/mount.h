/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: mount.h
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
 *  \file mount.h
 *
 *  Macros and function declarations for the mount utility.
 */

#ifndef MOUNT_UTILITY_H
#define MOUNT_UTILITY_H

#include <stdint.h>

char *guess_fstype(char *myname, char *devname);

/********************************************
 * Definitions for Linux ext2 filesystems
 ********************************************/

#define EXT2_SUPER_MAGIC        0xEF53  /**<  ext2 filesystem magic number */

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


/********************************************
 * Definitions for FAT filesystems
 ********************************************/

#if BYTE_ORDER == LITTLE_ENDIAN
# define GET_DWORD(d)           (d)
# define GET_WORD(w)            (w)
#else
# define GET_DWORD(d)           ((((d) & 0xff) << 24) | \
                                 (((d) & 0xff00) << 8) | \
                                 (((d) & 0xff0000) >> 8) | \
                                 (((d) & 0xff000000) >> 24))
# define GET_WORD(w)            ((((w) & 0xff) << 8) | (((w) & 0xff00) >> 8))
#endif      /* BYTE_ORDER */

#define VALID_FAT_SIG(s)        ((s) == 0x28 || (s) == 0x29)

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


#endif      /* MOUNT_UTILITY_H */
