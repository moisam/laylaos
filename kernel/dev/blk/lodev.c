/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: lodev.c
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
 *  \file lodev.c
 *
 *  General read and write functions for Loopback devices (major = 7).
 */

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/pcache.h>
#include <kernel/ata.h>
#include <kernel/loop.h>
#include <kernel/loop_internal.h>
#include <kernel/fio.h>
#include <fs/dummy.h>
#include <mm/kheap.h>
#include <kernel/gpt_mbr.h>


/*
 * Loopback device (major = 7) read/write interface.
 */

// Loopback device table
struct lodev_t *lodev[NR_LODEV] = { 0, };

// and the lock to access the above table
volatile struct kernel_mutex_t lodev_lock;

// array to hold loopback device partitions (only partitioned devices have
// entries in this list)
struct parttab_s *lodev_disk_part[MAX_LODEV_PARTITIONS];

// and the lock to access the above array
volatile struct kernel_mutex_t lodev_part_lock;

// check the given loop index is valid
#define VALID_LOOP_INDEX(n)         ((n) >= 0 && (n) < NR_LODEV)

// possible states of a loopback device
#define LODEV_STATE_UNBOUND         0
#define LODEV_STATE_BOUND           1
#define LODEV_STATE_RUNDOWN         2
#define LODEV_STATE_DELETING        3

// flags that can be set using LOOP_SET_STATUS(64)
#define LOOP_SET_STATUS_SETTABLE_FLAGS  (LO_FLAGS_AUTOCLEAR | LO_FLAGS_PARTSCAN)

// flags that can be cleared using LOOP_SET_STATUS(64)
#define LOOP_SET_STATUS_CLEARABLE_FLAGS (LO_FLAGS_AUTOCLEAR)

// flags that can be set using LOOP_CONFIGURE
#define LOOP_CONFIGURE_SETTABLE_FLAGS   \
        (LO_FLAGS_READ_ONLY | LO_FLAGS_AUTOCLEAR | \
         LO_FLAGS_PARTSCAN  | LO_FLAGS_DIRECT_IO)


// check the given devid is a valid loop device devid
STATIC_INLINE int valid_loop_devid(int maj, int min)
{
    if(maj != LODEV_MAJ && maj != LODEV_PART_MAJ)
    {
        return 0;
    }

    if(min < 0 || min >= 256)
    {
        return 0;
    }

    return 1;
}


/*
 * General Block Read/Write Operations.
 *
 * This function is used for R/W operations on both loopback devices (major 7),
 * as well as their partitions (major 259).
 */
long lodev_strategy(struct disk_req_t *req)
{
    int min = MINOR(req->dev);
    int maj = MAJOR(req->dev);
    ssize_t res;
    size_t count, pos, pos_end;
    off_t off;
    struct lodev_t *lo;
    struct parttab_s *part;
    struct fs_node_t *node;

    if(!valid_loop_devid(maj, min))
    {
        return -ENODEV;
    }

    kernel_mutex_lock(&lodev_lock);

    if(maj == LODEV_MAJ)
    {
        lo = lodev[min];
        part = NULL;
    }
    else
    {
        part = lodev_disk_part[min];
        lo = part ? part->priv : NULL;
    }

    if(lo == NULL || lo->state != LODEV_STATE_BOUND ||
       lo->file == NULL || lo->file->node == NULL)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -ENODEV;
    }

    node = lo->file->node;
    kernel_mutex_unlock(&lodev_lock);

    pos = lo->offset + (part ? (part->lba * lo->blocksz) : 0);
    pos += (req->blockno * req->fs_blocksz);
    count = req->datasz;

    if(pos >= node->size)
    {
        return -EINVAL;
    }

    if(pos + count > node->size)
    {
        count = node->size - pos;
    }

    pos_end = pos + count;

    if(lo->sizelimit && (pos_end > lo->offset + lo->sizelimit))
    {
        return -EINVAL;
    }

    if(part && (pos_end > (part->lba + part->total_sectors) * lo->blocksz))
    {
        return -EINVAL;
    }

    off = (off_t)pos;

    if(req->write)
    {
        if((lo->flags & LO_FLAGS_READ_ONLY))
        {
            return -EROFS;
        }

        res = vfs_write_node(node, &off, (unsigned char *)req->data, count, 1);
    }
    else
    {
        res = vfs_read_node(node, &off, (unsigned char *)req->data, count, 1);
    }

    return res;
}


static int lodev_register_part(struct lodev_t *lo, struct parttab_s *part, int n)
{
    char name[16];
    int i;

    kernel_mutex_lock(&lodev_part_lock);

    for(i = 0; i < MAX_LODEV_PARTITIONS; i++)
    {
        if(lodev_disk_part[i] == NULL)
        {
            lodev_disk_part[i] = part;
            break;
        }
    }

    kernel_mutex_unlock(&lodev_part_lock);

    if(i == MAX_LODEV_PARTITIONS)
    {
        return -ENOBUFS;
    }

    ksprintf(name, sizeof(name), "loop%dp%d", lo->number, n);
    add_dev_node(name, TO_DEVID(LODEV_PART_MAJ, i), (S_IFBLK | 0664));

    return 0;
}


static void lodev_remove_parts(struct lodev_t *lo)
{
    int i;
    struct parttab_s *part;

    kernel_mutex_lock(&lodev_part_lock);

    for(i = 0; i < MAX_LODEV_PARTITIONS; i++)
    {
        if(lodev_disk_part[i] == NULL || lodev_disk_part[i]->priv != lo)
        {
            continue;
        }

        part = lodev_disk_part[i];
        lodev_disk_part[i] = NULL;
        kfree(part);
        remove_dev_node(TO_DEVID(LODEV_PART_MAJ, i));
    }

    kernel_mutex_unlock(&lodev_part_lock);
}


static int read_sector(struct lodev_t *lo, uint8_t *ide_buf, uint32_t lba)
{
    off_t pos = lo->offset + (lba * lo->blocksz);

    return vfs_read_node(lo->file->node, &pos, ide_buf, lo->blocksz, 1);
}


/*
 * Read the given device's GUID Partition Table (GPT).
 *
 * For details on GPT partition table format, see:
 *    https://wiki.osdev.org/GPT
 */
static int lodev_read_gpt(struct lodev_t *lo, uint8_t *ide_buf)
{
    int dev_index;
    uint32_t gpthdr_lba = 0, off;
    uint32_t gptent_lba = 0, gptent_count = 0, gptent_sz = 0;
    struct gpt_part_entry_t *ent;
    struct parttab_s *part;

    // Sector 0 has already been read for us.
    if((gpthdr_lba = get_gpthdr_lba(ide_buf)) == 0)
    {
        // This shouldn't happen
        return -EIO;
    }

    // Read the Partition Table Header
    if(read_sector(lo, ide_buf, gpthdr_lba) <= 0)
    {
        printk("lodev: skipping disk with error status\n");
        return -EIO;
    }

    // Verify GPT signature
    if(!valid_gpt_signature(ide_buf))
    {
        return -EIO;
    }

    // Get partition entry starting lba, entry size and count
    gptent_lba = get_dword(ide_buf + 0x48);
    gptent_count = get_dword(ide_buf + 0x50);
    gptent_sz = get_dword(ide_buf + 0x54);
    off = 0;
    dev_index = 1;

    printk("lodev: found GPT with %u entries (sz %u)\n", gptent_count, gptent_sz);

    // Read the first set of partition entries
    if(read_sector(lo, ide_buf, gptent_lba) <= 0)
    {
        printk("lodev: skipping disk with invalid GPT entries\n");
        return -EIO;
    }

    while(gptent_count--)
    {
        if(off >= lo->blocksz)
        {
            // Read the next set of partition entries
            if(read_sector(lo, ide_buf, ++gptent_lba) <= 0)
            {
                printk("lodev: skipping disk with invalid GPT entries\n");
                return -EIO;
            }

            off = 0;
        }

        ent = (struct gpt_part_entry_t *)(ide_buf + off);

        // Check for unused entries
        if(unused_gpt_entry(ent))
        {
            printk("lodev: skipping unused GPT entry\n");
            off += gptent_sz;
            continue;
        }

        if(!(part = part_from_gpt_ent(ent)))
        {
            return -ENOMEM;
        }
        
        part->priv = lo;
        lodev_register_part(lo, part, dev_index);
        dev_index++;
        off += gptent_sz;
    }

    return 0;
}


static int lodev_read_mbr(struct lodev_t *lo)
{
    int i, res = 0;
    off_t pos;
    uint8_t *ide_buf;
    uintptr_t tmp_phys, tmp_virt;
    struct parttab_s *part;

    if(!lo || !lo->file || !lo->file->node)
    {
        return -EINVAL;
    }

    /* Read the MBR */
    if(get_next_addr(&tmp_phys, &tmp_virt, PTE_FLAGS_PW, REGION_DMA) != 0)
    {
        kpanic("lodev: insufficient memory to reload partition table\n");
        return -ENOMEM;
    }

    pos = lo->offset;
    ide_buf = (uint8_t *)tmp_virt;
    A_memset(ide_buf, 0, PAGE_SIZE);

    if((res = vfs_read_node(lo->file->node, &pos, ide_buf, lo->blocksz, 1)) < 0)
    {
        return res;
    }

    /* add the partitions */
    for(i = 0; i < 4; i++)
    {
        // Check for unused entries
        if(ide_buf[mbr_offset[i] + 4] == 0)
        {
            continue;
        }

        // Check for GPT partition table
        if(ide_buf[mbr_offset[i] + 4] == 0xEE)
        {
            res = lodev_read_gpt(lo, ide_buf);
            vmmngr_unmap_page((void *)tmp_virt);
            return res;
        }

        // Check partition start sector is legal
        if((ide_buf[mbr_offset[i] + 2] & 0x3f) == 0)
        {
            continue;
        }

        if(!(part = part_from_mbr_buf(ide_buf, i)))
        {
            res = -ENOMEM;
            goto out;
        }

        part->priv = lo;

        if((res = lodev_register_part(lo, part, i + 1)) < 0)
        {
            goto out;
        }
    }

out:

    vmmngr_unmap_page((void *)tmp_virt);
    return res;
}


static void info_to_info64(struct loop_info64 *info64, struct loop_info *info)
{
    A_memset(info64, 0, sizeof(struct loop_info64));
    info64->lo_number = info->lo_number;
    info64->lo_device = info->lo_device;
    info64->lo_inode = info->lo_inode;
    info64->lo_rdevice = info->lo_rdevice;
    info64->lo_offset = info->lo_offset;
    info64->lo_sizelimit = 0;
    info64->lo_flags = info->lo_flags;
    A_memcpy(info64->lo_file_name, info->lo_name, LO_NAME_SIZE);
}


static int info64_to_info(struct loop_info *info, struct loop_info64 *info64)
{
    A_memset(info, 0, sizeof(struct loop_info));
    info->lo_number = info64->lo_number;
    info->lo_device = info64->lo_device;
    info->lo_inode = info64->lo_inode;
    info->lo_rdevice = info64->lo_rdevice;
    info->lo_offset = info64->lo_offset;
    info->lo_flags = info64->lo_flags;
    A_memcpy(info->lo_name, info64->lo_file_name, LO_NAME_SIZE);

    // ensure no value truncation
    if(info->lo_device != info64->lo_device ||
       info->lo_rdevice != info64->lo_rdevice ||
       info->lo_inode != info64->lo_inode ||
       (uint64_t)info->lo_offset != info64->lo_offset)
    {
        return -EOVERFLOW;
    }

    return 0;
}


static void __lodev_update_directio(struct lodev_t *lo, int use_directio)
{
    struct file_t *f = lo->file;

    if(!!(lo->flags & LO_FLAGS_DIRECT_IO) == !!use_directio)
    {
        return;
    }

    vfs_fsync(f->node);

    if(use_directio)
    {
        lo->flags |= LO_FLAGS_DIRECT_IO;
    }
    else
    {
        lo->flags &= ~LO_FLAGS_DIRECT_IO;
    }
}


static void lodev_update_directio(struct lodev_t *lo)
{
    __lodev_update_directio(lo, (lo->file->flags & O_DIRECT) | (lo->flags & LO_FLAGS_DIRECT_IO));
}


static void lodev_reconfig_blocksz(struct lodev_t *lo, unsigned int __blocksz)
{
    struct file_t *f = lo->file;
    struct fs_node_t *node = f->node;
    unsigned int blocksz = __blocksz;
    dev_t dev = 0;

    if(f->node)
    {
    	dev = S_ISBLK(node->mode) ? node->blocks[0] : node->dev;
    }

    if(blocksz == 0)
    {
        if((f->flags & O_DIRECT) && dev != 0)
        {
            if(!bdev_tab[MAJOR(dev)].ioctl ||
               bdev_tab[MAJOR(dev)].ioctl(dev, BLKSSZGET, (char *)&blocksz, 1) < 0)
            {
                blocksz = 512;
            }
        }
        else
        {
            blocksz = 512;
        }
    }

    lo->blocksz = blocksz;
}


static size_t lodev_get_size(struct lodev_t *lo, struct file_t *f)
{
    size_t losz = f->node->size;

    if(lo->offset != 0)
    {
        if(lo->offset > losz)
        {
            return 0;
        }

        losz -= lo->offset;
    }

    if(lo->sizelimit != 0 && lo->sizelimit < losz)
    {
        losz = lo->sizelimit;
    }

    return losz;
}


/*
 * To change a loopdevice's backing store the following must be true:
 *   - the device is used read-only
 *   - new backing store is the same size and type as the old backing store
 */
static int lodev_change_fd(struct lodev_t *lo, unsigned int fd)
{
    struct file_t *f = NULL, *oldf;
    struct fs_node_t *node = NULL;

    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    kernel_mutex_lock(&lodev_lock);

    if(lo->state != LODEV_STATE_BOUND)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -ENXIO;
    }

    if(!(lo->flags & LO_FLAGS_READ_ONLY))
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EINVAL;
    }

    if(!S_ISREG(node->mode) && !S_ISBLK(node->mode))
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EINVAL;
    }

    oldf = lo->file;

    if(lodev_get_size(lo, f) != lodev_get_size(lo, oldf))
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EINVAL;
    }

    lo->file = f;
    f->node->flags |= FS_NODE_LOOP_BACKING;
    lodev_update_directio(lo);
    kernel_mutex_unlock(&lodev_lock);

    kernel_mutex_lock(&f->lock);
    //f->refs++;
    __sync_fetch_and_add(&(f->refs), 1);
    kernel_mutex_unlock(&f->lock);
    oldf->node->flags &= ~FS_NODE_LOOP_BACKING;
    closef(oldf);

    /*
     * re-read the partition table
     */
    if(lo->flags & LO_FLAGS_PARTSCAN)
    {
        lodev_remove_parts(lo);
        lodev_read_mbr(lo);
    }

    return 0;
}


int disk_openers(struct file_t *f)
{
    int fd, openers = 0;

    if(!f)
    {
        return 0;
    }

    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(!*t)
        {
            continue;
        }

        elevated_priority_unlock(&task_table_lock);

        for(fd = 0; fd < NR_OPEN; fd++)
        {
            if(*t && (*t)->ofiles->ofile[fd] == f)
            {
                openers++;
                break;
	        }
	    }

        elevated_priority_relock(&task_table_lock);
    }

    elevated_priority_unlock(&task_table_lock);

    return openers;
}


static int lodev_clear_fd(struct lodev_t *lo)
{
    struct file_t *f = lo->file;

    kernel_mutex_lock(&lodev_lock);

    if(lo->state != LODEV_STATE_BOUND)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -ENXIO;
    }

    // remove on last close
    lo->flags |= LO_FLAGS_AUTOCLEAR;

    kernel_mutex_unlock(&lodev_lock);

    // check if we are the only one who has this device open
    if(disk_openers(f) <= 1)
    {
        lo->state = LODEV_STATE_RUNDOWN;
    }

    return 0;
}


static int lodev_set_status_from_info64(struct lodev_t *lo, struct loop_info64 *info)
{
    if(info->lo_encrypt_key_size > LO_KEY_SIZE)
    {
        return -EINVAL;
    }

    // we don't support these (and they are deprecated in Linux anyway)
    if(info->lo_encrypt_type)
    {
        return -EINVAL;
    }

    if(info->lo_offset > LLONG_MAX || info->lo_sizelimit > LLONG_MAX)
    {
        return -EOVERFLOW;
    }

    lo->offset = info->lo_offset;
    lo->sizelimit = info->lo_sizelimit;
    A_memcpy(lo->filename, info->lo_file_name, LO_NAME_SIZE);
    lo->filename[LO_NAME_SIZE - 1] = '\0';
    lo->flags = info->lo_flags;

    return 0;
}


static int lodev_set_status(struct lodev_t *lo, struct loop_info64 *info)
{
    int size_changed = 0;
    int old_flags, res;

    kernel_mutex_lock(&lodev_lock);

    if(lo->state != LODEV_STATE_BOUND)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -ENXIO;
    }

    kernel_mutex_unlock(&lodev_lock);

    if(lo->offset != info->lo_offset || lo->sizelimit != info->lo_sizelimit)
    {
        size_changed = 1;

        remove_unreferenced_cached_pages(NULL);
        // defined in fs/update.c
        update(lo->file->node->dev);
        remove_cached_disk_pages(TO_DEVID(LODEV_MAJ, lo->number));
    }

    old_flags = lo->flags;

    if((res = lodev_set_status_from_info64(lo, info)) < 0)
    {
        return res;
    }

    // mask out unsettable flags
    lo->flags &= LOOP_SET_STATUS_SETTABLE_FLAGS;
    // use previous flags for unsettable flags
    lo->flags |= old_flags & ~LOOP_SET_STATUS_SETTABLE_FLAGS;
    // user previous flags for unclearable flags
    lo->flags |= old_flags & ~LOOP_SET_STATUS_CLEARABLE_FLAGS;

    if(size_changed)
    {
        lo->sizelimit = lodev_get_size(lo, lo->file);
    }

    __lodev_update_directio(lo, (lo->flags & LO_FLAGS_DIRECT_IO));

    /*
     * re-read the partition table
     */
    if(lo->flags & LO_FLAGS_PARTSCAN)
    {
        lodev_remove_parts(lo);
        lodev_read_mbr(lo);
    }

    return 0;
}


static int lodev_get_status(struct lodev_t *lo, struct loop_info64 *info)
{
    kernel_mutex_lock(&lodev_lock);

    if(lo->state != LODEV_STATE_BOUND)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -ENXIO;
    }

    A_memset(info, 0, sizeof(struct loop_info64));
    info->lo_number = lo->number;
    info->lo_offset = lo->offset;
    info->lo_sizelimit = lo->sizelimit;
    info->lo_flags = lo->flags;
    info->lo_device = lo->file->node->dev;
    info->lo_inode = lo->file->node->inode;
    info->lo_rdevice = S_ISBLK(lo->file->node->mode) ? 
                        lo->file->node->blocks[0] : lo->file->node->dev;
    A_memcpy(info->lo_file_name, lo->filename, LO_NAME_SIZE);

    kernel_mutex_unlock(&lodev_lock);

    return 0;
}


static int lodev_set_status_user(struct lodev_t *lo, char *arg)
{
    struct loop_info info;
    struct loop_info64 info64;

    if(copy_from_user(&info, arg, sizeof(struct loop_info)) != 0)
    {
        return -EFAULT;
    }

    info_to_info64(&info64, &info);

    return lodev_set_status(lo, &info64);
}


static int lodev_set_status_user64(struct lodev_t *lo, char *arg)
{
    struct loop_info64 info64;

    if(copy_from_user(&info64, arg, sizeof(struct loop_info64)) != 0)
    {
        return -EFAULT;
    }

    return lodev_set_status(lo, &info64);
}


static int lodev_get_status_to_info(struct lodev_t *lo, char *arg)
{
    struct loop_info info;
    struct loop_info64 info64;
    int res;

    if(!arg)
    {
        return -EINVAL;
    }

    if((res = lodev_get_status(lo, &info64)) < 0)
    {
        return res;
    }

    if((res = info64_to_info(&info, &info64)) < 0)
    {
        return res;
    }

    return copy_to_user(arg, &info, sizeof(struct loop_info));
}


static int lodev_get_status_to_info64(struct lodev_t *lo, char *arg)
{
    struct loop_info64 info64;
    int res;

    if(!arg)
    {
        return -EINVAL;
    }

    if((res = lodev_get_status(lo, &info64)) < 0)
    {
        return res;
    }

    return copy_to_user(arg, &info64, sizeof(struct loop_info64));
}


static int lodev_config(struct lodev_t *lo, struct loop_config *loconf)
{
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int res;

    if(fdnode(loconf->fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    kernel_mutex_lock(&lodev_lock);

    if(lo->state != LODEV_STATE_UNBOUND)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EBUSY;
    }

    if(!S_ISREG(node->mode) && !S_ISBLK(node->mode))
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EINVAL;
    }

    if(loconf->info.lo_flags & ~LOOP_CONFIGURE_SETTABLE_FLAGS)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EINVAL;
    }

    if(loconf->block_size > PAGE_SIZE)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EINVAL;
    }

    if((res = lodev_set_status_from_info64(lo, &loconf->info)) < 0)
    {
        kernel_mutex_unlock(&lodev_lock);
        return res;
    }

    if(!(f->flags & (O_WRONLY | O_RDWR)) || node->read == dummyfs_read)
    {
        lo->flags |= LO_FLAGS_READ_ONLY;
    }

    lo->file = f;
    f->node->flags |= FS_NODE_LOOP_BACKING;
    lodev_reconfig_blocksz(lo, loconf->block_size);
    lodev_update_directio(lo);
    lo->sizelimit = lodev_get_size(lo, f);
    lo->state = LODEV_STATE_BOUND;
    kernel_mutex_unlock(&lodev_lock);

    kernel_mutex_lock(&f->lock);
    //f->refs++;
    __sync_fetch_and_add(&(f->refs), 1);
    kernel_mutex_unlock(&f->lock);

    /*
     * re-read the partition table
     */
    if(lo->flags & LO_FLAGS_PARTSCAN)
    {
        lodev_remove_parts(lo);
        lodev_read_mbr(lo);
    }

    return 0;
}


/*
 * General block device control function.
 */
long lodev_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel)
{
    int min = MINOR(dev_id);
    int maj = MAJOR(dev_id);
    struct lodev_t *lo;
    struct parttab_s *part;

    if(!valid_loop_devid(maj, min))
    {
        return -EINVAL;
    }

    if(maj == LODEV_MAJ)
    {
        lo = lodev[min];
        part = NULL;
    }
    else
    {
        part = lodev_disk_part[min];
        lo = part ? part->priv : NULL;
    }

    if(lo == NULL)
    {
        return -EINVAL;
    }
    
    switch(cmd)
    {
        case LOOP_SET_FD:
        {
            struct loop_config loconf;

            A_memset(&loconf, 0, sizeof(struct loop_config));
            loconf.fd = (unsigned int)(uintptr_t)arg;

            return lodev_config(lo, &loconf);
        }

        case LOOP_CONFIGURE:
        {
            struct loop_config loconf;

            if(kernel)
            {
                A_memcpy(&loconf, arg, sizeof(struct loop_config));
            }
            else
            {
                COPY_FROM_USER(&loconf, arg, sizeof(struct loop_config));
            }

            return lodev_config(lo, &loconf);
        }

        case LOOP_CHANGE_FD:
            return lodev_change_fd(lo, (unsigned int)(uintptr_t)arg);

        case LOOP_CLR_FD:
            return lodev_clear_fd(lo);

        case LOOP_SET_STATUS:
            return lodev_set_status_user(lo, arg);

        case LOOP_GET_STATUS:
            return lodev_get_status_to_info(lo, arg);

        case LOOP_SET_STATUS64:
            return lodev_set_status_user64(lo, arg);

        case LOOP_GET_STATUS64:
            return lodev_get_status_to_info64(lo, arg);

        case LOOP_SET_CAPACITY:
            if(lo->state != LODEV_STATE_BOUND)
            {
                return -ENXIO;
            }

            lo->sizelimit = lodev_get_size(lo, lo->file);
            return 0;

        case LOOP_SET_DIRECT_IO:
            if(lo->state != LODEV_STATE_BOUND)
            {
                return -ENXIO;
            }

            __lodev_update_directio(lo, !!arg);
            return 0;

        case LOOP_SET_BLOCK_SIZE:
            if(lo->state != LODEV_STATE_BOUND)
            {
                return -ENXIO;
            }

            if((unsigned int)(uintptr_t)arg > PAGE_SIZE)
            {
                return -EINVAL;
            }

            remove_unreferenced_cached_pages(NULL);
            // defined in fs/update.c
            update(lo->file->node->dev);
            remove_cached_disk_pages(TO_DEVID(LODEV_MAJ, lo->number));

            lodev_reconfig_blocksz(lo, (unsigned int)(uintptr_t)arg);
            lodev_update_directio(lo);

            return 0;

        case BLKSSZGET:
        {
            // get the block size in bytes
            int blocksz = (int)lo->blocksz;
            RETURN_IOCTL_RES(int, arg, blocksz, kernel);
        }

        case BLKGETSIZE:
        {
            // get disk size in 512-blocks
            long sects = (long)(lo->sizelimit / 512);
            RETURN_IOCTL_RES(long, arg, sects, kernel);
        }

        case BLKGETSIZE64:
        {
            // get disk size in bytes
            unsigned long long bytes = lo->sizelimit;
            RETURN_IOCTL_RES(unsigned long long, arg, bytes, kernel);
        }

        case BLKRRPART:
        {
            // force re-reading the partition table
            // NOTE: NOT TESTED!
            int i;

            // get the min devid of the parent disk
            min = (min / 16) * 16;

            // first ensure none of the partitions (or the whole disk) is mounted
            if(get_mount_info(TO_DEVID(LODEV_MAJ, lo->number)))
            {
                return -EBUSY;
            }

            for(i = 0; i < MAX_LODEV_PARTITIONS; i++)
            {
                if(lodev_disk_part[i] == NULL || lodev_disk_part[i]->priv != lo)
                {
                    continue;
                }

                if(get_mount_info(TO_DEVID(LODEV_PART_MAJ, i)))
                {
                    return -EBUSY;
                }
            }

            // now remove the partitions and their /dev nodes, but leave the
            // parent disk intact
            lodev_remove_parts(lo);

            // finally read the new partition table
            lodev_read_mbr(lo);

            return 0;
        }
    }
    
    return -EINVAL;
}


int lodev_first_free(void)
{
    struct lodev_t **lo, **llo = &lodev[NR_LODEV];

    kernel_mutex_lock(&lodev_lock);

    for(lo = lodev; lo < llo; lo++)
    {
        if(!*lo)
        {
            kernel_mutex_unlock(&lodev_lock);
            return lo - lodev;
        }
    }

    kernel_mutex_unlock(&lodev_lock);

    return -ENOSPC;
}


static void lodev_make_name(char *buf, long n)
{
    int j;

    buf[0] = 'l';
    buf[1] = 'o';
    buf[2] = 'o';
    buf[3] = 'p';

    j = (n >= 100) ? 7 : ((n >= 10) ? 6 : 5);
    buf[j--] = '\0';

    do
    {
        buf[j--] = '0' + (int)(n % 10);
        n /= 10;
    } while(n);
}


int lodev_add_index(long n)
{
    char name[16];
    struct lodev_t *lo;

    if(!VALID_LOOP_INDEX(n))
    {
        return -EINVAL;
    }

    if(!(lo = kmalloc(sizeof(struct lodev_t))))
    {
        return -ENOMEM;
    }

    A_memset(lo, 0, sizeof(struct lodev_t));
    lo->number = n;
    kernel_mutex_lock(&lodev_lock);

    // someone else grabbed it
    if(lodev[n])
    {
        kernel_mutex_unlock(&lodev_lock);
        kfree(lo);
        return -EEXIST;
    }

    lodev[n] = lo;
    kernel_mutex_unlock(&lodev_lock);

    lodev_make_name(name, n);
    add_dev_node(name, TO_DEVID(LODEV_MAJ, n), (S_IFBLK | 0664));

    return n;
}


static void lodev_remove(struct lodev_t *lo, int n)
{
    struct file_t *f = lo->file;
    dev_t dev = TO_DEVID(LODEV_MAJ, n);

    kernel_mutex_lock(&lodev_lock);
    lo->file = NULL;
    lo->offset = 0;
    lo->sizelimit = 0;
    A_memset(lo->filename, 0, LO_NAME_SIZE);
    lodev[n] = NULL;
    kernel_mutex_unlock(&lodev_lock);

    lo->flags = 0;
    lo->state = LODEV_STATE_UNBOUND;
    remove_dev_node(dev);
    lodev_remove_parts(lo);
    kfree(lo);

    remove_cached_disk_pages(dev);

    if(f)
    {
        if(f->node)
        {
            update(f->node->dev);
            f->node->flags &= ~FS_NODE_LOOP_BACKING;
        }

        // this will remove the last reference and free the file struct
        closef(f);
    }
}


int lodev_remove_index(long n)
{
    struct lodev_t *lo;
    dev_t dev = TO_DEVID(LODEV_MAJ, n);

    if(!VALID_LOOP_INDEX(n))
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&lodev_lock);
    lo = lodev[n];

    if(lo == NULL)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -ENODEV;
    }

    if(lo->state != LODEV_STATE_UNBOUND || disk_openers(lo->file) > 0)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EBUSY;
    }

    if(get_mount_info(dev))
    {
        kernel_mutex_unlock(&lodev_lock);
        return -EBUSY;
    }

    /*
    if(lo->file && lo->file->refs)
    {
        //lo->file->refs--;
        __sync_fetch_and_sub(&(lo->file->refs), 1);
    }
    */

    lo->state = LODEV_STATE_DELETING;
    //lodev[n] = NULL;
    kernel_mutex_unlock(&lodev_lock);
    //remove_dev_node(dev);
    //kfree(lo);
    lodev_remove(lo, n);

    return n;
}


int lodev_open(dev_t dev)
{
    int min = MINOR(dev);
    struct lodev_t *lo;

    if(!VALID_LOOP_INDEX(min))
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&lodev_lock);
    lo = lodev[min];

    if(lo == NULL)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -ENODEV;
    }

    if(lo->state == LODEV_STATE_DELETING || lo->state == LODEV_STATE_RUNDOWN)
    {
        kernel_mutex_unlock(&lodev_lock);
        return -ENXIO;
    }

    kernel_mutex_unlock(&lodev_lock);

    return 0;
}


void lodev_release(struct file_t *f)
{
    struct lodev_t *lo = NULL, **plo, **llo = &lodev[NR_LODEV];
    int n = 0;

    if(disk_openers(f) > 0)
    {
        return;
    }

    kernel_mutex_lock(&lodev_lock);

    for(plo = lodev; plo < llo; plo++)
    {
        if(*plo && (*plo)->file == f)
        {
            lo = *plo;
            n = plo - lodev;
            break;
        }
    }

    if(lo == NULL)
    {
        kernel_mutex_unlock(&lodev_lock);
        return;
    }

    if(get_mount_info(TO_DEVID(LODEV_MAJ, n)))
    {
        kernel_mutex_unlock(&lodev_lock);
        return;
    }

    if(lo->state == LODEV_STATE_BOUND && (lo->flags & LO_FLAGS_AUTOCLEAR))
    {
        lo->state = LODEV_STATE_RUNDOWN;
    }

    kernel_mutex_unlock(&lodev_lock);

    if(lo->state == LODEV_STATE_RUNDOWN)
    {
        lodev_remove(lo, n);
    }
}

