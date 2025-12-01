/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: ext2fs_hash.c
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
 *  \file ext2fs_hash.c
 *
 *  This file implements ext2 filesystem functions that are need to access
 *  files in indexed directories. The main ext2 driver is implemented in the
 *  file ext2fs.c.
 *
 *  This code is largely based on Haiku's implementation. See:
 *    https://github.com/haiku/haiku/blob/master/src/add-ons/kernel/file_systems/ext2/HTree.cpp
 *    https://github.com/haiku/haiku/blob/master/src/add-ons/kernel/file_systems/ext2/HTreeEntryIterator.cpp
 */

//#define __DEBUG
#define __EXT2_INTERNAL_DRIVER__

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>         // NAME_MAX
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/pcache.h>
#include <fs/ext2.h>
#include <fs/ext2_hash.h>
#include <mm/kheap.h>

#define EXT_DIRTYPE(d)      \
    (d->super) ? \
        is_ext_dir_type((struct ext2_superblock_t *)(d->super->data)) : 0

#define FAKE_DIRENT_SIZE    (sizeof(struct htree_fake_ent_t) + 4)


static void init_hash_seed(volatile struct htree_incore_t *info);
static uint32_t get_hash(volatile struct htree_incore_t *info,
                         char *filename, size_t fnamelen, uint8_t hash_ver);


STATIC_INLINE int is_valid_hash_root(struct htree_root_t *hroot)
{
    if(hroot->res != 0)
    {
        return 0;
    }

    if(hroot->hash_ver != HTREE_HASH_LEGACY &&
       hroot->hash_ver != HTREE_HASH_HALF_MD4 &&
       hroot->hash_ver != HTREE_HASH_TEA)
    {
        return 0;
    }

    if(hroot->root_infolen != 8)
    {
        return 0;
    }

    if(hroot->levels > 1)
    {
        return 0;
    }

    return 1;
}


STATIC_INLINE size_t max_block_size(struct mount_info_t *d)
{
    //struct ext2_superblock_t *super;
    size_t maxsz = d->block_size;

    /*
    if(d->super && (super = (struct ext2_superblock_t *)(d->super->data)))
    {
        if(super->readonly_features & EXT2_FEATURE_RO_COMPAT_METADATA_CHKSUM)
        {
            maxsz -= sizeof(struct ext2_dirent_tail_t);
        }
    }
    */

    return maxsz;
}


static int init_count_limit(struct cached_page_t *buf, volatile struct htree_incore_t *info)
{
    struct htree_count_limit_t *count_limit;
    size_t maxsz;

    count_limit = (struct htree_count_limit_t *)
                    (&((struct htree_ent_t *)buf->virt)[info->first_ent]);

    info->count = count_limit->count;
    info->limit = count_limit->limit;

    //printk("init_count_limit: count %d, limit %d\n", info->count, info->limit);

    if(info->count > info->limit)
    {
        printk("ext2: bad htree count of %u (limit %u)\n", info->count, info->limit);
        return -EINVAL;
    }

    maxsz = info->d->block_size;

    if(info->super->readonly_features & EXT2_FEATURE_RO_COMPAT_METADATA_CHKSUM)
    {
        maxsz -= sizeof(struct htree_tail_t);
    }

    //printk("init_count_limit: maxsz %d\n", maxsz);

    if(info->limit != (maxsz / sizeof(struct htree_ent_t) - info->first_ent))
    {
        printk("ext2: bad htree limit of %u (should be %u)\n", 
                info->limit, 
                (maxsz / sizeof(struct htree_ent_t) - info->first_ent));
        return -EINVAL;
    }

    return 0;
}


static uint32_t get_next_block(volatile struct htree_incore_t *info, int collisions_only)
{
    struct htree_ent_t *ent;
    struct cached_page_t *buf;
    int eob;
    uint32_t relative_block;
    uint16_t cur_ent;

    cur_ent = info->cur_ent + 1;
    eob = (cur_ent >= (info->count + info->first_ent));

    if(eob)
    {
        if(info->parent == NULL)
        {
            return 0;
        }

        if(!(relative_block = get_next_block(info->parent, 0)))
        {
            return 0;
        }

        if(!(buf = get_relative_block(info->dir, info->d, relative_block, 0)))
        {
            return 0;
        }

        // before we commit, make sure the next block has a collision so we 
        // can use it
        ent = &((struct htree_ent_t *)buf)[1];

        // We use collisions when looking up entries.
        // We don't use them when getting all the dentries.
        if(collisions_only && (ent->hash & 1) != 1)
        {
            release_cached_page(buf);
            return 0;
        }

        // Skip fake dir ent
        info->first_ent = 1;
        info->cur_ent = 1;
        cur_ent = 1;

        if(init_count_limit(buf, info) < 0)
        {
            release_cached_page(buf);
            return 0;
        }

        release_cached_page(info->htree_block);
        info->htree_block = buf;
    }

    ent = &((struct htree_ent_t *)info->htree_block)[cur_ent];

    // We use collisions when looking up entries.
    // We don't use them when getting all the dentries.
    if(collisions_only && (ent->hash & 1) != 1)
    {
        return 0;
    }

    info->cur_ent = cur_ent;

    return ent->block;
}


static size_t block_count_for_newent(volatile struct htree_incore_t *info)
{
    size_t n = 0;

    if(info->count == info->limit)
    {
        n++;

        if(info->parent)
        {
            n += block_count_for_newent(info->parent);
        }
        else
        {
            if(info->levels == 1)
            {
                // XXX: max level of supported indirections
                return -ENOBUFS;
            }

            n++;
        }
    }

    return n;
}


static struct htree_incore_t *init_info_struct(struct fs_node_t *dir, 
                                               struct mount_info_t *d,
                                               int ext_dir_type)
{
    struct cached_page_t *buf;
    struct htree_root_t *hroot;
    struct htree_incore_t *info;
    size_t offset = 0;

    if(!(buf = get_relative_block(dir, d, 0, 0)))
    {
        return NULL;
    }

    hroot = (struct htree_root_t *)buf->virt;

    if(!is_valid_hash_root(hroot))
    {
        release_cached_page(buf);
        return NULL;
    }

    offset = hroot->root_infolen + (2 * (sizeof(struct htree_fake_ent_t) + 4));

    if(!(info = kmalloc(sizeof(struct htree_incore_t))))
    {
        release_cached_page(buf);
        return NULL;
    }

    info->lblock = dir->blocks[0];
    info->first_ent = (offset / sizeof(struct htree_ent_t));
    info->cur_ent = info->first_ent;
    info->super = (struct ext2_superblock_t *)(d->super->data);
    info->d = d;
    info->htree_block = buf;
    info->ext_dir_type = ext_dir_type;
    info->dir = dir;
    info->parent = NULL;
    info->levels = hroot->levels;
    info->hash_ver = hroot->hash_ver;

    if(init_count_limit(buf, info) < 0)
    {
        kfree(info);
        release_cached_page(buf);
        return NULL;
    }

    return info;
}


static int init_info2_struct(volatile struct htree_incore_t *info, 
                             size_t lblock,
                             volatile struct htree_incore_t **res)
{
    struct cached_page_t *buf;
    struct htree_incore_t *info2;

    *res = NULL;

    if(!(buf = get_relative_block(info->dir, info->d, lblock, 0)))
    {
        return -EIO;
    }

    if(!(info2 = kmalloc(sizeof(struct htree_incore_t))))
    {
        release_cached_page(buf);
        return -ENOMEM;
    }

    //info2->lblock = lblock;
    info2->first_ent = 1;
    info2->cur_ent = 1;
    info2->super = info->super;
    info2->d = info->d;
    info2->htree_block = buf;
    info2->ext_dir_type = info->ext_dir_type;
    info2->dir = info->dir;
    info2->parent = info;
    info2->levels = info->levels - 1;
    info2->buf = info->buf;
    info2->bufsz = info->bufsz;
    info2->lookup_flags = info->lookup_flags;

    if(init_count_limit(buf, info2) < 0)
    {
        kfree(info2);
        release_cached_page(buf);
        return -EINVAL;
    }

    *res = info2;

    return 0;
}


/*
static void set_dirent_checksum(struct fs_node_t *dir, struct cached_page_t *buf)
{
    if(info->super->readonly_features & EXT2_FEATURE_RO_COMPAT_METADATA_CHKSUM)
    {
        struct ext2_dirent_tail_t *tail = 
                (struct ext2_dirent_tail_t *)
                        (buf->virt + d->block_size - 
                                     sizeof(struct ext2_dirent_tail_t));

        tail->zero1 = 0;
        tail->reclen = 12;
        tail->zero2 = 0;
        tail->filetype = 0xde;
        tail->checksum = dirent_checksum(dir, buf, d);
    }
}
*/


STATIC_INLINE size_t namelen_to_entsize(size_t fnamelen)
{
    size_t entsize = fnamelen + sizeof(struct ext2_dirent_t);

    // adjust the entry size to make sure it is 4-byte aligned
    if(entsize & 3)
    {
        entsize = (entsize & ~3) + 4;
    }

    return entsize;
}


long linear_dir_add(struct fs_node_t *dir, struct fs_node_t *file,
                    char *filename, struct mount_info_t *d, 
                    size_t fnamelen, size_t offset)
{
    struct cached_page_t *buf;
    struct ext2_dirent_t *ent;
    int found = 0;
    size_t entsize = namelen_to_entsize(fnamelen);
    size_t actual_size, sz;
    size_t max_size = max_block_size(d);
    unsigned char *blk, *end;
    char *namebuf;
    int ext_dir_type = EXT_DIRTYPE(d);

    if(!(buf = get_relative_block(dir, d, offset, 0)))
    {
        return -EIO;
    }

    blk = (unsigned char *)buf->virt;
    end = blk + max_size /* d->block_size */;

    while(blk < end)
    {
        ent = (struct ext2_dirent_t *)blk;
            
        // 1 - Check if we reached the last entry in the block.
        if(!ent->entry_size)
        {
            sz = end - blk;
            ent->entry_size = sz;

            if(sz >= entsize)
            {
                found = 1;
            }
            else
            {
                // mark this as a deleted entry
                ent->inode = 0;
                __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
            }

            break;
        }

        // 2 - Check for deleted entries and if that entry is large enough
        //     to fit us. A corrupt (but still valid) directory might have
        //     '.' and '..' entries with 0 inode numbers. Avoid overwriting
        //     these entries.
        if(ent->inode == 0)
        {
            namebuf = (char *)((unsigned char *)ent + sizeof(struct ext2_dirent_t));

            if(namebuf[0] == '.' && 
                    (ent->name_length_lsb == 1 ||
                        (namebuf[1] == '.' && ent->name_length_lsb == 2)))
            {
                blk += ent->entry_size;
                continue;
            }

            if(ent->entry_size >= (fnamelen + sizeof(struct ext2_dirent_t)))
            {
                found = 1;
                break;
            }
        }

        // 3 - Entries at the end of a block occupy the whole space left.
        //     Check if this is the case and if we can fit ourself there.
        actual_size = sizeof(struct ext2_dirent_t) +
                                    ext2_entsz(ent, ext_dir_type);

        // adjust the entry size to make sure it is 4-byte aligned
        if(actual_size & 3)
        {
            actual_size = (actual_size & ~3) + 4;
        }

        if(ent->entry_size > actual_size)
        {
            // is there room for another entry?
            if(ent->entry_size - actual_size >= entsize)
            {
                entsize = ent->entry_size - actual_size;

                // truncate the existing entry
                ent->entry_size = actual_size;

                // create a new entry
                ent = (struct ext2_dirent_t *)((char *)ent + actual_size);
                ent->entry_size = entsize;

                found = 1;
                break;
            }
        }

        blk += ent->entry_size;
    }
        
    if(!found)
    {
        release_cached_page(buf);
        return -ENOBUFS;
    }

    namebuf = (char *)((unsigned char *)ent + sizeof(struct ext2_dirent_t));
    A_memcpy(namebuf, filename, fnamelen);
    ent->name_length_lsb = fnamelen;

    if(!ext_dir_type)
    {
        ent->type_indicator = (fnamelen >> 8) & 0xffff;
    }
    else
    {
        ent->type_indicator = mode_to_ext2_type(file->mode);
    }

    ent->inode = file->inode;
    //set_dirent_checksum(dir, buf);

    __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
    release_cached_page(buf);

    return 0;
}


struct hashed_ent_t
{
    void *pos;
    uint32_t hash;
    struct hashed_ent_t *next;
};


static void __insert(struct hashed_ent_t *entry_head, struct hashed_ent_t *newent)
{
    struct hashed_ent_t *prev;

    newent->next = NULL;

    if(entry_head->next == NULL)
    {
        entry_head->next = newent;
    }
    else
    {
        for(prev = entry_head; prev->next != NULL; prev = prev->next)
        {
            if(prev->next->hash >= newent->hash)
            {
                break;
            }
        }

        newent->next = prev->next;
        prev->next = newent;
    }
}


static void __free_entries(struct hashed_ent_t *entry_head)
{
    struct hashed_ent_t *tmp, *next;

    for(tmp = entry_head->next; tmp != NULL; )
    {
        next = tmp->next;
        kfree(tmp);
        tmp = next;
    }
}


static size_t __get_count(struct hashed_ent_t *entry_head)
{
    size_t count = 0;
    struct hashed_ent_t *tmp;

    for(tmp = entry_head->next; tmp != NULL; tmp = tmp->next)
    {
        count++;
    }

    return count;
}


static void add_htree_ent(volatile struct htree_incore_t *info, 
                          uint32_t hash, size_t block, int collision)
{
    struct htree_ent_t *entries;
    struct htree_count_limit_t *count_limit;
    uint16_t limit, count;

    entries = (struct htree_ent_t *)info->htree_block->virt;
    count_limit = (struct htree_count_limit_t *)&entries[info->first_ent];

    limit = count_limit->limit;
    count = count_limit->count;

    if(count == limit)
    {
        kpanic("Unable to split HTree node -- not yet implemented!\n");
    }

    if(count > 0)
    {
        char *tmp;
        size_t sz;

        sz = (count + info->first_ent - info->cur_ent - 1) * sizeof(struct htree_ent_t);

        /*
        switch_tty(1);
        printk("add_htree_ent: count %u, first_ent %u, cur_ent %u\n", count, info->first_ent, info->cur_ent);
        printk("add_htree_ent: sz %lu\n", sz);
        */

        if(sz)
        {
            if(!(tmp = kmalloc(sz)))
            {
                kpanic("Failed to alloc temp memory (in add_htree_ent())\n");
            }

            A_memcpy(tmp, &entries[info->cur_ent + 1], sz);
            A_memcpy(&entries[info->cur_ent + 2], tmp, sz);
            kfree(tmp);
        }
    }

    if(info->cur_ent == info->first_ent)
    {
        entries[info->cur_ent + 1].hash = hash;
    }
    else
    {
        uint32_t old_hash = entries[info->cur_ent].hash;

        entries[info->cur_ent].hash = collision ? (old_hash | 1) :
                                                  (old_hash & ~1);
        entries[info->cur_ent + 1].hash = (old_hash & 1) == 0 ?
                                          (hash & ~1) : (hash | 1);
    }

    entries[info->cur_ent + 1].block = block;
    info->count = count + 1;
    count_limit->count = info->count;
    __sync_or_and_fetch(&info->htree_block->flags, PCACHE_FLAG_DIRTY);

    //set_htree_checksum(dir, buf2, i, 2);
}


long split_indexed_block(volatile struct htree_incore_t *info,
                         struct fs_node_t *file,
                         char *filename, size_t block1, int first_split)
{
    struct cached_page_t *buf1 = NULL, *buf2 = NULL;
    struct htree_root_t *hroot;
    struct htree_fake_ent_t *fent;
    struct ext2_superblock_t *super;
    struct hashed_ent_t *hent, entry_head;
    struct ext2_dirent_t *dent, *newdent;
    char *tmp = NULL, *namebuf = NULL, *name, *block;
    size_t i, j, mid, max_size, len;
    size_t blocks;
    size_t fnamelen = strlen(filename);
    long res = 0;
    int ext_dir_type = EXT_DIRTYPE(info->d);
    int collision;
    uint32_t prev_hash;

    entry_head.next = NULL;
    blocks = ((info->dir->size + (info->d->block_size - 1)) / info->d->block_size);

    if(!info->d->super || 
       !(super = (struct ext2_superblock_t *)(info->d->super->data)))
    {
        return -EINVAL;
    }

    if(!(tmp = kmalloc(info->d->block_size)))
    {
        return -ENOMEM;
    }

    if(!(namebuf = kmalloc(1024)))
    {
        kfree(tmp);
        return -ENOMEM;
    }

    if(!(buf1 = get_relative_block(info->dir, info->d, block1, 0)))
    {
        kfree(tmp);
        kfree(namebuf);
        return -EIO;
    }

    if(first_split)
    {
        if(!(buf2 = get_relative_block(info->dir, info->d, 0, 0)))
        {
            res = -EIO;
            goto err;
        }

        // Save entries from the first block into the buffer
        A_memcpy(tmp, (void *)buf2->virt, info->d->block_size);

        // Mark the inode as indexed
        info->dir->flags |= FS_NODE_INDEXED_DIR;

        // Copy the old dot and dot dot
        A_memcpy((void *)buf2->virt, tmp, 2 * FAKE_DIRENT_SIZE);

        // Initialize the root
        hroot = (struct htree_root_t *)buf2->virt;
        hroot->hash_ver = DEFAULT_HASH(info->d);
        hroot->root_infolen = 8;
        hroot->levels = 0;
        hroot->res = 0;
        hroot->flags = 0;

        size_t maxsz = info->d->block_size;

        if(super->readonly_features & EXT2_FEATURE_RO_COMPAT_METADATA_CHKSUM)
        {
            maxsz -= sizeof(struct htree_tail_t);
        }

        hroot->count_limit[0].limit = 
            (maxsz - ((virtual_addr)hroot->count_limit - buf2->virt)) /
                                                sizeof(struct htree_ent_t);
        hroot->count_limit[0].count = 2;

        // Adjust dot dot length
        fent = (struct htree_fake_ent_t *)&hroot->dotdot;
        fent->entry_size = info->d->block_size - FAKE_DIRENT_SIZE;

        __sync_or_and_fetch(&buf2->flags, PCACHE_FLAG_DIRTY);

        init_hash_seed(info);
    }
    else
    {
        // Save entries from the first block into the buffer
        A_memcpy(tmp, (void *)buf1->virt, info->d->block_size);
    }

    // Read all entries and sort them by hash value
    i = first_split ? (2 * FAKE_DIRENT_SIZE) : 0;
    max_size = max_block_size(info->d);

    while(i < max_size)
    {
        if(!(hent = kmalloc(sizeof(struct hashed_ent_t))))
        {
            res = -ENOMEM;
            goto err;
        }

        hent->pos = &tmp[i];
        dent = (struct ext2_dirent_t *)&tmp[i];

        name = &tmp[i] + sizeof(struct ext2_dirent_t);
        len = ext2_entsz(dent, ext_dir_type);
        A_memcpy(namebuf, name, len);
        namebuf[len] = '\0';

        hent->hash = get_hash(info, name, len, info->hash_ver);
        __insert(&entry_head, hent);
        i += dent->entry_size;
    }

    // Create a new entry and insert it into the list
    if(!(hent = kmalloc(sizeof(struct hashed_ent_t))))
    {
        res = -ENOMEM;
        goto err;
    }

    len = namelen_to_entsize(fnamelen);

    if(!(newdent = kmalloc(len)))
    {
        kfree(hent);
        res = -ENOMEM;
        goto err;
    }

    newdent->inode = file->inode;
    newdent->entry_size = len;
    newdent->name_length_lsb = fnamelen;
    name = (char *)((unsigned char *)newdent + sizeof(struct ext2_dirent_t));
    A_memcpy(name, filename, fnamelen);

    if(!ext_dir_type)
    {
        newdent->type_indicator = (fnamelen >> 8) & 0xffff;
    }
    else
    {
        newdent->type_indicator = mode_to_ext2_type(file->mode);
    }

    hent->pos = newdent;
    hent->hash = get_hash(info, name, fnamelen, info->hash_ver);
    __insert(&entry_head, hent);

    // Move first half of entries to the first block
    mid = __get_count(&entry_head) / 2;
    j = 0;
    hent = entry_head.next;
    block = (char *)buf1->virt;

    dent = (struct ext2_dirent_t *)hent->pos;
    prev_hash = hent->hash;

    for(i = 0; i < mid; i++)
    {
        dent = (struct ext2_dirent_t *)hent->pos;
        prev_hash = hent->hash;
        len = namelen_to_entsize(ext2_entsz(dent, ext_dir_type));
        dent->entry_size = len;
        A_memcpy(&block[j], dent, len);
        j += len;
        hent = hent->next;
    }

    // Update last entry in the block
    len = dent->entry_size;
    dent = (struct ext2_dirent_t *)&block[j - len];
    dent->entry_size = max_size - j + len;
    //set_dirent_checksum(dir, buf1);
    __sync_or_and_fetch(&buf1->flags, PCACHE_FLAG_DIRTY);

    collision = 0;

    // Look for collisions and try to keep them in the first block
    while(hent->next && hent->hash == prev_hash)
    {
        dent = (struct ext2_dirent_t *)hent->pos;

        if(j + dent->entry_size > max_size)
        {
            // Not enough space in block
            collision = 1;
            break;
        }

        A_memcpy(&block[j], dent, dent->entry_size);
        j += dent->entry_size;
        hent = hent->next;
    }

    mid = hent->hash;

    // Update the parent
    if(first_split)
    {
        struct htree_ent_t *htree_ent;

        hroot = (struct htree_root_t *)buf2->virt;
        htree_ent = (struct htree_ent_t *)hroot->count_limit;
        htree_ent->block = 1;
        htree_ent++;
        htree_ent->block = 2;
        htree_ent->hash = mid;
        /*
        i = hroot->root_infolen + (2 * FAKE_DIRENT_SIZE);
        set_htree_checksum(dir, buf2, i, 2);
        */
        __sync_or_and_fetch(&buf2->flags, PCACHE_FLAG_DIRTY);
        release_cached_page(buf2);
    }
    else
    {
        // Insert new hash entry
        add_htree_ent(info, mid, blocks - 1, collision);
    }

    // Move second half of entries to the last block
    if(!(buf2 = get_relative_block(info->dir, info->d, blocks - 1, 0)))
    {
        res = -EIO;
        goto err;
    }

    j = 0;
    block = (char *)buf2->virt;

    while(hent != NULL)
    {
        dent = (struct ext2_dirent_t *)hent->pos;
        len = namelen_to_entsize(ext2_entsz(dent, ext_dir_type));
        dent->entry_size = len;
        A_memcpy(&block[j], dent, len);
        j += len;
        hent = hent->next;
    }

    // Update last entry in the block
    len = dent->entry_size;
    dent = (struct ext2_dirent_t *)&block[j - len];
    dent->entry_size = max_size - j + len;
    //set_dirent_checksum(dir, buf2);
    __sync_or_and_fetch(&buf2->flags, PCACHE_FLAG_DIRTY);
    release_cached_page(buf2);

    buf2 = NULL;
    res = 0;

    // FALLTHROUGH

err:

    if(tmp)
    {
        kfree(tmp);
    }

    if(namebuf)
    {
        kfree(namebuf);
    }

    if(buf1)
    {
        release_cached_page(buf1);
    }

    if(buf2)
    {
        release_cached_page(buf2);
    }

    if(entry_head.next)
    {
        __free_entries(&entry_head);
    }

    return res;
}


int add_blocks(struct fs_node_t *dir, struct mount_info_t *d, size_t n)
{
    struct cached_page_t *buf;
    size_t blocks = ((dir->size + (d->block_size - 1)) / d->block_size);
    volatile size_t i;

    for(i = 0; i < n; i++)
    {
        if(!(buf = get_relative_block(dir, d, blocks + i, BMAP_FLAG_CREATE)))
        {
            return -EIO;
        }

        release_cached_page(buf);
    }

    dir->mtime = now();
    dir->ctime = dir->mtime;
    dir->size = (blocks + n) * d->block_size;
    dir->flags |= FS_NODE_DIRTY;

    return 0;
}


static long hash_add(volatile struct htree_incore_t *info, 
                     struct fs_node_t *file,
                     char *filename, size_t fnamelen,
                     uint32_t hash)
{
    struct htree_ent_t *start, *end, *mid;
    volatile struct htree_incore_t *info2;
    size_t lblock, tmp;
    long res;

    //switch_tty(1);
    //printk("hash_add: count %u\n", info->count);

    if(info->count == 0)
    {
        return -ENOENT;
    }

    start = (struct htree_ent_t *)info->htree_block->virt + info->cur_ent + 1;
    end = (struct htree_ent_t *)info->htree_block->virt + info->count + info->first_ent - 1;
    mid = start;

    //switch_tty(1);
    //printk("hash_add: s 0x%lx, e 0x%lx, m 0x%lx\n", start, end, mid);

    while(start <= end)
    {
        mid = (struct htree_ent_t *)((end - start) / 2 + start);

        if(hash >= mid->hash)
        {
            start = mid + 1;
        }
        else
        {
            end = mid - 1;
        }

        //printk("hash_add: s 0x%lx, e 0x%lx, m 0x%lx\n", start, end, mid);
    }

    --start;
    info->cur_ent = (((uint8_t *)start - (uint8_t *)info->htree_block->virt) / sizeof(struct htree_ent_t));
    //printk("hash_add: info->cur_ent 0x%lx\n", info->cur_ent);

    if(info->levels == 0)
    {
        lblock = start->block;

        while((res = linear_dir_add(info->dir, file, filename,
                                       info->d, fnamelen, lblock)) < 0)
        {
            //printk("hash_add: res %d\n", res);

            /*
            if(res != -ENOBUFS)
            {
                return res;
            }
            */

            if((tmp = get_next_block(info, 1)) != 0)
            {
                //printk("hash_add: tmp %d\n", tmp);
                lblock = tmp;
                continue;
            }

            // create new blocks
            size_t n = block_count_for_newent(info) + 1;

            //printk("hash_add: n %d\n", n);

            if(add_blocks(info->dir, info->d, n) < 0)
            {
                return -ENOBUFS;
            }

            res = split_indexed_block(info, file, filename, lblock, 0);
            //printk("hash_add: split res %d\n", res);
            break;
        }

        //printk("hash_add: res %d\n", res);
        //kpanic("****\n");
        return res;
    }

    if((res = init_info2_struct(info, start->block, &info2)) < 0)
    {
        return res;
    }

    res = hash_add(info2, file, filename, fnamelen, hash);
    release_cached_page(info2->htree_block);
    kfree((void *)info2);

    return res;
}


long dents_fill_buf(struct fs_node_t *dir, 
                    size_t relative_block, size_t *block_offset,
                    void *buf, int bufsz, 
                    struct mount_info_t *d)
{
    struct cached_page_t *dbuf = NULL;
    struct dirent *dent = NULL;
    struct ext2_dirent_t *ent;
    size_t reclen, namelen;
    size_t count = 0;
    unsigned char *start, *blk, *end;
    int ext_dir_type;
    char *n, *b = (char *)buf;

    if(!(dbuf = get_relative_block(dir, d, relative_block, 0)))
    {
        return 0;
    }

    blk = (unsigned char *)(dbuf->virt + *block_offset);
    end = (unsigned char *)(dbuf->virt + d->block_size);
    start = blk;
    ext_dir_type = EXT_DIRTYPE(d);
    *block_offset = 0;

    while(blk < end)
    {
        ent = (struct ext2_dirent_t *)blk;

        // last entry in dir
        if(!ent->entry_size)
        {
            break;
        }

        // deleted entry - skip
        if(ent->inode == 0)
        {
            blk += ent->entry_size;
            continue;
        }

        // get filename length
        namelen = ext2_entsz(ent, ext_dir_type);

        // calc dirent record length
        reclen = GET_DIRENT_LEN(namelen);

        // check the buffer has enough space for this entry
        if((count + reclen) > (size_t)bufsz)
        {
            break;
        }
            
        n = (char *)(blk + sizeof(struct ext2_dirent_t));
        dent = (struct dirent *)b;

        ext2_entry_to_dirent(ent, dent, n, namelen, 0, ext_dir_type);

        //printk("'%s'", dent->d_name);

        dent->d_reclen = reclen;
        b += reclen;
        count += reclen;
        blk += ent->entry_size;
    }

    *block_offset = (blk - start);
    release_cached_page(dbuf);
    return count;
}


static long hash_getdents(volatile struct htree_incore_t *info,
                          off_t *pos, size_t *actual_bytes, int *done)
{
    struct htree_ent_t *start, *end;
    volatile struct htree_incore_t *info2;
    size_t lblock, i, start_offset;
    long res, count = 0;

    if(info->count == 0)
    {
        return 0;
    }

    start_offset = (*pos) / info->d->block_size;
    i = (*pos) % info->d->block_size;

    start = (struct htree_ent_t *)info->htree_block->virt + info->cur_ent;
    end = (struct htree_ent_t *)info->htree_block->virt + info->count + info->first_ent - 1;

    /*
    //switch_tty(1);
    printk("hash_getdents: count %u, limit %u, first %u, cur %u, start_offset %lu\n", info->count, info->limit, info->first_ent, info->cur_ent, start_offset);

    printk("block: ");
    for(int z = 0; z < 48; z++) printk("%2x ", ((char *)info->htree_block->virt)[z]);
    printk("\n");
    */

    while(start <= end)
    {
        //printk("hash_getdents: levels %d\n", info->levels);
        //printk("hash_getdents: start->hash %u, start->block %u\n", start->hash, start->block);

        if(info->levels == 0)
        {
            if(start_offset != 0)
            {
                (*pos) -= info->d->block_size;
                start_offset--;
                start++;
                continue;
            }

            lblock = start->block;
            res = dents_fill_buf(info->dir, lblock, &i, info->buf, info->bufsz, info->d);

            //printk("\nhash_getdents: res %ld, count %ld\n", res, count);

            if(res >= 0)
            {
                count += res;
                info->buf = (char *)info->buf + res;
                info->bufsz -= res;
                *actual_bytes += i;
            }

            // if the buf is underfilled, we either reached end of dir
            // or the last entry does not fit in the buf
            if(i < info->d->block_size)
            {
                *done = 1;
                return count;
            }

            i = 0;
            start++;
            continue;
        }

        if((res = init_info2_struct(info, start->block, &info2)) < 0)
        {
            if(res == -ENOENT)
            {
                break;
            }

            return res;
        }

        res = hash_getdents(info2, pos, actual_bytes, done);

        info->buf = info2->buf;
        info->bufsz = info2->bufsz;

        release_cached_page(info2->htree_block);
        kfree((void *)info2);
        start++;

        if(res >= 0)
        {
            count += res;
        }

        if(*done)
        {
            break;
        }
    }

    //printk("hash_getdents: count %ld\n", count);

    return count;
}


long linear_dir_lookup(struct fs_node_t *dir,
                       char *filename,
                       struct dirent **entry,
                       struct mount_info_t *d,
                       int flags,
                       size_t fnamelen,
                       uint32_t relative_block)
{
    struct cached_page_t *buf;
    struct ext2_dirent_t *ent;
    size_t len;
    unsigned char *blk, *end;
    char *n;

    // for safety
    if(entry)
    {
        *entry = NULL;
    }

    if(!(buf = get_relative_block(dir, d, relative_block, 0)))
    {
        return -ENOMEM;
    }

    blk = (unsigned char *)buf->virt;
    end = blk + d->block_size;

    while(blk < end)
    {
        ent = (struct ext2_dirent_t *)blk;

        if(!ent->entry_size)
        {
            break;
        }

        len = ext2_entsz(ent, (flags & DIR_LOOKUP_FLAG_HAS_DIRTYPE));
        n = (char *)(blk + sizeof(struct ext2_dirent_t));

        if(ent->inode == 0 || len != fnamelen)
        {
            blk += ent->entry_size;
            continue;
        }

        if(memcmp(n, filename, len) == 0)
        {
            if(flags & DIR_LOOKUP_FLAG_REMOVE)
            {
                ent->inode = 0;
                //set_dirent_checksum(buf);
                __sync_or_and_fetch(&buf->flags, PCACHE_FLAG_DIRTY);
            }
            else
            {
                *entry = ext2_entry_to_dirent(ent, NULL, n, len, 0, 0);
            }

            release_cached_page(buf);
            return 0;
        }

        blk += ent->entry_size;
    }
    
    release_cached_page(buf);

    return -ENOENT;
}


static long hash_lookup(volatile struct htree_incore_t *info, 
                        char *filename, size_t fnamelen,
                        uint32_t hash,
                        struct dirent **entry)
{
    struct htree_ent_t *start, *end, *mid;
    volatile struct htree_incore_t *info2;
    size_t lblock;
    long res;

    if(info->count == 0)
    {
        return -ENOENT;
    }

    start = (struct htree_ent_t *)info->htree_block->virt + info->cur_ent + 1;
    end = (struct htree_ent_t *)info->htree_block->virt + info->count + info->first_ent - 1;
    mid = start;

    //switch_tty(1);
    //printk("ext2_finddir_hashed: s 0x%lx, e 0x%lx, m 0x%lx\n", start, end, mid);

    while(start <= end)
    {
        mid = (struct htree_ent_t *)((end - start) / 2 + start);

        if(hash >= mid->hash)
        {
            start = mid + 1;
        }
        else
        {
            end = mid - 1;
        }

        //printk("ext2_finddir_hashed: s 0x%lx, e 0x%lx, m 0x%lx\n", start, end, mid);
    }

    --start;
    info->cur_ent = (((uint8_t *)start - (uint8_t *)info->htree_block->virt) / sizeof(struct htree_ent_t));
    //printk("ext2_finddir_hashed: info->cur_ent 0x%lx\n", info->cur_ent);

    if(info->levels == 0)
    {
        lblock = start->block;

        while((res = linear_dir_lookup(info->dir, filename, entry,
                                       info->d, info->lookup_flags, 
                                       fnamelen, lblock)) < 0)
        {
            //printk("ext2_finddir_hashed: res %d\n", res);
            if((lblock = get_next_block(info, 1)) == 0)
            {
                return -ENOENT;
            }
        }

        //printk("ext2_finddir_hashed: res %d\n", res);
        //kpanic("****\n");
        return res;
    }

    if((res = init_info2_struct(info, start->block, &info2)) < 0)
    {
        return res;
    }

    res = hash_lookup(info2, filename, fnamelen, hash, entry);
    release_cached_page(info2->htree_block);
    kfree((void *)info2);

    return res;
}


static uint32_t hash_legacy(char *filename, size_t fnamelen)
{
    uint32_t hash = 0x12a3fe2d;
    uint32_t prev = 0x37abe8f9;

    for( ; fnamelen > 0; --fnamelen, ++filename)
    {
        uint32_t next = prev + (hash ^ (*filename * 7152373));

        if((next & 0x80000000) != 0)
        {
            next -= 0x7fffffff;
        }

        prev = hash;
        hash = next;
    }

    return (hash << 1);
}


static void prep_block_for_hash(char *s, uint32_t len, uint32_t *blocks, uint32_t nblocks)
{
    uint32_t padding = len;
    uint32_t nbytes = nblocks * 4;
    uint32_t iter, i, v;

    padding |= (padding << 8);
    padding |= (padding << 16);

    if(len > nbytes)
    {
        len = nbytes;
    }

    iter = (len / 4);

    for(i = 0; i < iter; i++)
    {
        v = (padding << 8) + *(s++);
        v = (v << 8) + *(s++);
        v = (v << 8) + *(s++);
        v = (v << 8) + *(s++);
        blocks[i] = v;
    }

    if(iter < nblocks)
    {
        nbytes = len % 4;
        v = padding;

        for(i = 0; i < nbytes; i++)
        {
            v = (v << 8) + *(s++);
        }

        blocks[iter] = v;

        for(i = iter + 1; i < nblocks; i++)
        {
            blocks[i] = padding;
        }
    }
}


#define MD4F(x, y, z)       ((z) ^ ((x) & ((y) ^ (z))))
#define MD4G(x, y, z)       (((x) & (y)) + (((x) ^ (y)) & (z)))
#define MD4H(x, y, z)       ((x) ^ (y) ^ (z))

#define MD4RotateVars(a, b, c, d)   \
    { uint32_t oldd = d; d = c; c = b; b = a; a = oldd; }


static void half_md4_transform(uint32_t buf[4], uint32_t blocks[8])
{
    uint32_t a = buf[0], b = buf[1], c = buf[2], d = buf[3];
    uint32_t sh, shifts[4] = { 3, 7, 11, 19 };
    int i, j;

    // Round 1
    for(i = 0; i < 8; i++)
    {
        a += MD4F(b, c, d) + blocks[i];
        sh = shifts[i % 4];
        a = (a << sh) | (a >> (32 - sh));
        MD4RotateVars(a, b, c, d);
    }

    // Round 2
    shifts[1] = 5;
    shifts[2] = 9;
    shifts[3] = 13;

    for(j = 1; j >= 0; j--)
    {
        for(i = j; i < 8; i += 2)
        {
            a += MD4G(b, c, d) + blocks[i] + 013240474631UL;
            sh = shifts[i / 2];
            a = (a << sh) | (a >> (32 - sh));
            MD4RotateVars(a, b, c, d);
        }
    }

    // Round 3
    shifts[1] = 9;
    shifts[2] = 11;
    shifts[3] = 15;

    for(i = 0; i < 4; i++)
    {
        a += MD4H(b, c, d) + blocks[3 - i] + 015666365641UL;
        sh = shifts[i * 2 % 4];
        a = (a << sh) | (a >> (32 - sh));
        MD4RotateVars(a, b, c, d);

        a += MD4H(b, c, d) + blocks[7 - i] + 015666365641UL;
        sh = shifts[(i * 2 + 1) % 4];
        a = (a << sh) | (a >> (32 - sh));
        MD4RotateVars(a, b, c, d);
    }

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}


static uint32_t hash_half_md4(volatile struct htree_incore_t *info, 
                              char *filename, size_t fnamelen)
{
    uint32_t buf[4];
    uint32_t blocks[8];
    ssize_t len = fnamelen;

    buf[0] = info->hash_seed[0];
    buf[1] = info->hash_seed[1];
    buf[2] = info->hash_seed[2];
    buf[3] = info->hash_seed[3];

    for( ; len > 0; len -= 32)
    {
        prep_block_for_hash(filename, (uint32_t)len, blocks, 8);
        half_md4_transform(buf, blocks);
        filename += 32;
    }

    return buf[1];
}


static void tea_transform(uint32_t buf[4], uint32_t blocks[4])
{
    uint32_t x = buf[0], y = buf[1];
    uint32_t a = blocks[0], b = blocks[1], c = blocks[2], d = blocks[3];
    uint32_t sum = 0;
    int i;

    for(i = 16; i > 0; i--)
    {
        sum += 0x9E3779B9;
        x += ((y << 4) + a) ^ (y + sum) ^ ((y >> 5) + b);
        y += ((x << 4) + c) ^ (x + sum) ^ ((x >> 5) + d);
    }

    buf[0] += x;
    buf[1] += y;
}


static uint32_t hash_tea(volatile struct htree_incore_t *info, 
                         char *filename, size_t fnamelen)
{
    uint32_t buf[4];
    uint32_t blocks[4];
    ssize_t len = fnamelen;

    buf[0] = info->hash_seed[0];
    buf[1] = info->hash_seed[1];
    buf[2] = info->hash_seed[2];
    buf[3] = info->hash_seed[3];

    for( ; len > 0; len -= 16)
    {
        prep_block_for_hash(filename, (uint32_t)len, blocks, 4);
        tea_transform(buf, blocks);
        filename += 16;
    }

    return buf[0];
}


static uint32_t get_hash(volatile struct htree_incore_t *info,
                         char *filename, size_t fnamelen, uint8_t hash_ver)
{
    switch(hash_ver)
    {
        case HTREE_HASH_LEGACY:
            return hash_legacy(filename, fnamelen) & ~1;

        case HTREE_HASH_HALF_MD4:
            return hash_half_md4(info, filename, fnamelen) & ~1;

        case HTREE_HASH_TEA:
            return hash_tea(info, filename, fnamelen) & ~1;

        default:
            kpanic("ext2: unknown hash function (this should not happen!)\n");
            return 0;
    }
}


static void init_hash_seed(volatile struct htree_incore_t *info)
{
    info->hash_seed[0] = 0;
    info->hash_seed[1] = 0;
    info->hash_seed[2] = 0;
    info->hash_seed[3] = 0;

    if(info->super->version_major >= 1)
    {
        info->hash_seed[0] = info->super->hash_seed[0];
        info->hash_seed[1] = info->super->hash_seed[1];
        info->hash_seed[2] = info->super->hash_seed[2];
        info->hash_seed[3] = info->super->hash_seed[3];
    }

    if(info->hash_seed[0] == 0 && info->hash_seed[1] == 0 &&
       info->hash_seed[2] == 0 && info->hash_seed[3] == 0)
    {
        info->hash_seed[0] = 0x67452301;
        info->hash_seed[1] = 0xefcdab89;
        info->hash_seed[2] = 0x98badcfe;
        info->hash_seed[3] = 0x10325476;
    }
}


#define FALLBACK_TO_LINEAR_SEARCH()     \
    return ext2_finddir_internal(dir, filename, entry, d, flags);


long ext2_finddir_hashed(struct fs_node_t *dir, char *filename,
                         struct dirent **entry, struct mount_info_t *d, 
                         int flags)
{
    volatile struct htree_incore_t *info;
    long res;
    uint32_t hash;
    size_t fnamelen = strlen(filename);

    // for safety
    if(entry)
    {
        *entry = NULL;
    }

    if(!fnamelen)
    {
        return -EINVAL;
    }

    if(fnamelen > NAME_MAX || fnamelen > EXT2_MAX_FILENAME_LEN)
    {
        return -ENAMETOOLONG;
    }

    if(!(dir->flags & FS_NODE_INDEXED_DIR) ||
       (filename[0] == '.' &&
        (filename[1] == '\0' || (filename[1] == '.' && filename[2] == '\0'))))
    {
        FALLBACK_TO_LINEAR_SEARCH();
    }

    if(!(info = init_info_struct(dir, d, !!(flags & DIR_LOOKUP_FLAG_HAS_DIRTYPE))))
    {
        FALLBACK_TO_LINEAR_SEARCH();
    }

    init_hash_seed(info);
    info->lookup_flags = flags;
    hash = get_hash(info, filename, fnamelen, info->hash_ver);

    res = hash_lookup(info, filename, fnamelen, hash, entry);
    release_cached_page(info->htree_block);
    kfree((void *)info);

    return res;
}


#define FALLBACK_TO_LINEAR_GETDENTS()   \
    return ext2_getdents_internal(dir, pos, buf, bufsz, d);


long ext2_getdents_hashed(struct fs_node_t *dir, 
                          off_t *pos, void *buf, int bufsz,
                          struct mount_info_t *d)
{
    volatile struct htree_incore_t *info;
    int ext_dir_type, done = 0;
    long res;
    size_t actual_bytes = 0;
    off_t old_pos = *pos;

    ext_dir_type = EXT_DIRTYPE(d);

    if(!(info = init_info_struct(dir, d, ext_dir_type)))
    {
        FALLBACK_TO_LINEAR_GETDENTS();
    }

    info->buf = buf;
    info->bufsz = bufsz;

    /*
    switch_tty(1);
    printk("ext2_getdents_hashed: buf 0x%lx, bufsz %ld, *pos %ld\n", buf, bufsz, *pos);
    printk("ext2_getdents_hashed: actual_bytes %ld\n", actual_bytes);

    static volatile int runs = 0;
    if(info->count == 4 && info->limit == 124) runs++;
    */

    res = hash_getdents(info, pos, &actual_bytes, &done);
    release_cached_page(info->htree_block);
    kfree((void *)info);

    (*pos) = old_pos + actual_bytes;

    /*
    printk("ext2_getdents_hashed: buf 0x%lx, bufsz %ld, *pos %ld\n", buf, bufsz, *pos);
    printk("ext2_getdents_hashed: actual_bytes %ld\n", actual_bytes);

    if(runs > 1) kpanic("****\n");
    */

    return res;
}


long ext2_deldir_hashed(struct fs_node_t *dir, struct dirent *entry, 
                        struct mount_info_t *d)
{
    int ext_dir_type = EXT_DIRTYPE(d);
    int flags = DIR_LOOKUP_FLAG_REMOVE;

    if(ext_dir_type)
    {
        flags |= DIR_LOOKUP_FLAG_HAS_DIRTYPE;
    }

    return ext2_finddir_hashed(dir, entry->d_name, NULL, d, flags);
}


long ext2_addir_hashed(struct fs_node_t *dir, struct fs_node_t *file,
                       char *filename, struct mount_info_t *d)
{
    volatile struct htree_incore_t *info;
    long res;
    uint32_t hash;
    size_t fnamelen = strlen(filename);
    int ext_dir_type = EXT_DIRTYPE(d);
    int flags = ext_dir_type ? DIR_LOOKUP_FLAG_HAS_DIRTYPE : 0;

    if(!fnamelen)
    {
        return -EINVAL;
    }

    if(fnamelen > NAME_MAX || fnamelen > EXT2_MAX_FILENAME_LEN)
    {
        return -ENAMETOOLONG;
    }

    if(!(info = init_info_struct(dir, d, ext_dir_type)))
    {
        return -ENOMEM;
    }

    init_hash_seed(info);
    info->lookup_flags = flags;
    hash = get_hash(info, filename, fnamelen, info->hash_ver);

    res = hash_add(info, file, filename, fnamelen, hash);
    release_cached_page(info->htree_block);
    kfree((void *)info);

    return res;
}

