/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: ext2_hash.h
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
 *  \file ext2_hash.h
 *
 *  Include header file for working with ext2 indexed directories.
 */

#ifndef __EXT2_FSYS_HASH_H__
#define __EXT2_FSYS_HASH_H__

#include <stdint.h>

/*
 * Hash types
 */
#define HTREE_HASH_LEGACY               0
#define HTREE_HASH_HALF_MD4             1
#define HTREE_HASH_TEA                  2

/*
 * Flags for linear_dir_lookup()
 */
#define DIR_LOOKUP_FLAG_HAS_DIRTYPE     0x01
#define DIR_LOOKUP_FLAG_REMOVE          0x02


#define DEFAULT_HASH(d)     \
    ((d)->super) ? \
        (((struct ext2_superblock_t *)((d)->super->data))->default_hash_ver) : 0

/*********************************
 * Disk structures
 *********************************/

/**
 * @struct htree_fake_ent_t
 * @brief The htree_fake_ent_t structure.
 *
 * A structure to represent a fake directory entry in an indexed directory.
 */
struct htree_fake_ent_t
{
    uint32_t inode;             /**<  inode number */
    uint16_t entry_size;        /**<  total size of this entry */
    uint8_t  name_length;       /**<  name length */
    uint8_t  type_indicator;    /**<  type indicator */
} __attribute__((packed));

struct htree_count_limit_t
{
    uint16_t limit;
    uint16_t count;
} __attribute__((packed));

struct htree_ent_t
{
    uint32_t hash;
    uint32_t block;
} __attribute__((packed));

struct htree_root_t
{
    struct htree_fake_ent_t dot;
    char dot_name[4];
    struct htree_fake_ent_t dotdot;
    char dotdot_name[4];
    uint32_t res;
    uint8_t hash_ver;
    uint8_t root_infolen;
    uint8_t levels;
    uint8_t flags;
    struct htree_count_limit_t count_limit[];
} __attribute__((packed));

struct htree_tail_t
{
    uint32_t res;
    uint32_t checksum;
} __attribute__((packed));

struct ext2_dirent_tail_t
{
    uint32_t zero1;
    uint16_t reclen;        // must be 12
    uint8_t  zero2;
    uint8_t  filetype;      // must be 0xde
    uint32_t checksum;
} __attribute__((packed));


/*********************************
 * Internal structures
 *********************************/

struct htree_incore_t
{
    uint32_t hash_seed[4];
    uint16_t limit;
    uint16_t count;
    uint8_t hash_ver;
    size_t lblock;
    size_t cur_ent, first_ent;
    int levels;
    int ext_dir_type;
    int lookup_flags;
    struct ext2_superblock_t *super;
    struct mount_info_t *d;
    struct cached_page_t *htree_block;
    struct fs_node_t *dir;
    volatile struct htree_incore_t *parent;
    void *buf;
    int bufsz;
};

/*********************************
 * Function prototypes
 *********************************/

long ext2_finddir_hashed(struct fs_node_t *dir, char *filename,
                         struct dirent **entry, struct mount_info_t *d,
                         int flags);

long ext2_getdents_hashed(struct fs_node_t *dir, 
                          off_t *pos, void *buf, int bufsz,
                          struct mount_info_t *d);

long ext2_deldir_hashed(struct fs_node_t *dir, struct dirent *entry, 
                        struct mount_info_t *d);

long ext2_addir_hashed(struct fs_node_t *dir, struct fs_node_t *file,
                       char *filename, struct mount_info_t *d);

long linear_dir_lookup(struct fs_node_t *dir,
                       char *filename,
                       struct dirent **entry,
                       struct mount_info_t *d,
                       int flags,
                       size_t fnamelen,
                       uint32_t relative_block);

long linear_dir_add(struct fs_node_t *dir, struct fs_node_t *file,
                    char *filename, struct mount_info_t *d, 
                    size_t fnamelen, size_t offset);

long dents_fill_buf(struct fs_node_t *dir, 
                    size_t relative_block, size_t *block_offset,
                    void *buf, int bufsz, 
                    struct mount_info_t *d);

long split_indexed_block(volatile struct htree_incore_t *info,
                         struct fs_node_t *file,
                         char *filename, size_t block1, int first_split);

int add_blocks(struct fs_node_t *dir, struct mount_info_t *d, size_t n);

#endif      /* __EXT2_FSYS_HASH_H__ */
