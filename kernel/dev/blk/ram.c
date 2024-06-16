/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ram.c
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
 *  \file ram.c
 *
 *  General read and write functions for RAM disks (major = 1).
 */

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/pcache.h>
#include <gunzip/member.h>
#include <gunzip/deflate.h>


/*
 * RAM disk (major = 1) read/write interface.
 *
 * Currently, all RAM disks MUST be formatted with a block size of 1024 bytes.
 */

#define RAMDISK_BLKSIZE             1024


// RAM disk table
struct ramdisk_s ramdisk[NR_RAMDISK] = { 0, };


/*
 * General Block Read/Write Operations.
 */
int ramdev_strategy(struct disk_req_t *req)
{
    int min = MINOR(req->dev);
    struct ramdisk_s *rd;
    
    if(min >= NR_RAMDISK)
    {
        return 0;
    }
    
    rd = &ramdisk[min];
    
    if(!rd->start)
    {
        return 0;
    }

    // get the block's virtual address in memory
    virtual_addr addr = rd->start + (req->blockno * RAMDISK_BLKSIZE);

    // now copy the data
    if(!req->write)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        A_memcpy((void *)req->data, (void *)addr, RAMDISK_BLKSIZE);
    }
    else
    {
        A_memcpy((void *)addr, (void *)req->data, RAMDISK_BLKSIZE);
    }

    return (int)RAMDISK_BLKSIZE;
}


/*
 * RFC 1952 (GZIP File Format Specification) defines GZIP File format as:
 * 
 *    A gzip file consists of a series of "members" (compressed data
 *    sets).  The format of each member is specified in the following
 *    section.  The members simply appear one after another in the file,
 *    with no additional information before, between, or after them.
 */

static char *gunzip_geterror(int err)
{
    switch(err)
    {
        case GZIP_INVALID_SIGNATURE  : return "Invalid signature";
        case GZIP_INVALID_CM         : return "Invalid compression method";
        case GZIP_INVALID_XFL        : return "Invalid Extra flags";
        case GZIP_INVALID_OS         : return "Invalid OS value";
        case GZIP_INVALID_FLG        : return "Invalid flags";
        case GZIP_INVALID_CRC32      : return "Invalid crc32 value";
        case GZIP_INVALID_ISIZE      : return "Invalid input size";
        case GZIP_INVALID_BLOCKLEN   : return "Invalid block length";
        case GZIP_INVALID_BLOCKDATA  : return "Invalid block data";
        case GZIP_INVALID_ENCODING   : return "Invalid encoding";
        case GZIP_INSUFFICIENT_MEMORY: return "Insufficient memory";
    }

    return "Unknown error";
}


/*
 * Decompress the initial RAM disk (initrd).
 */
int ramdisk_init(virtual_addr data_start, virtual_addr data_end)
{
    virtual_addr addr = 0;
    size_t sz = 0;
    
    // decompress the initrd
    int res = read_member((char *)data_start, (long)(data_end - data_start),
                            &addr, &sz);

    if(res != GZIP_VALID_ARCHIVE)
    {
        printk("    Invalid/Corrupt file: %s\n", gunzip_geterror(res));
        res = -EINVAL;
        goto fin;
    }
    
    // ram0
    ramdisk[0].start = addr;
    ramdisk[0].end = addr + sz;

    // initrd
    ramdisk[250].start = addr;
    ramdisk[250].end = addr + sz;

    res = 0;
    
    printk("    Decompressed initrd successfully..\n");

fin:

    return res;
}


/*
 * General block device control function.
 */
int ramdev_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel)
{
    UNUSED(arg);
    UNUSED(kernel);
    
    int min = MINOR(dev_id);
    struct ramdisk_s *rd;
    
    if(min >= NR_RAMDISK)
    {
        return -EINVAL;
    }
    
    rd = &ramdisk[min];
    
    if(!rd->start)
    {
        return -EINVAL;
    }

    switch(cmd)
    {
        case DEV_IOCTL_GET_BLOCKSIZE:
            // get the block size in bytes
            return RAMDISK_BLKSIZE;
    }
    
    return -EINVAL;
}

