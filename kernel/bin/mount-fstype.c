/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: mount-fstype.c
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
 *  \file mount-fstype.c
 *
 *  Functions to guess filesystem type for the mount program.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "mount.h"

#define BUFSZ               2048
#define ISO9660_BLOCKSZ     2048


static void exit_failed_guess(char *myname, char *str, char *arg)
{
    fprintf(stderr, "%s: %s: %s\n", myname, str, arg);
    fprintf(stderr, "%s: try specifying filesystem type using the -t option\n", myname);
    fprintf(stderr, "%s: see %s -h for details\n", myname, myname);
    exit(32);
}


static int is_ext2_fs(char *buf)
{
    // the superblock is always at offset 1024
    struct ext2_superblock_t *psuper = (struct ext2_superblock_t *)(buf + 1024);

    return (psuper->signature == EXT2_SUPER_MAGIC);
}


static int is_fat_fs(char *buf)
{
    struct fat_bootsect_t *boot = (struct fat_bootsect_t *)buf;
    size_t blocksz, sectors_per_cluster, total_sectors;
    size_t root_dir_sectors, fat_size, first_data_sector;
    size_t data_sectors, total_clusters;

    blocksz = GET_WORD(boot->base.bytes_per_sector);
    sectors_per_cluster = boot->base.sectors_per_cluster;

    if(blocksz == 0)
    {
        // NOTE: this could still be a valid exFAT system, but we do not
        //       support this at the moment
        return 0;
    }

    // this will be 0 for FAT32
    root_dir_sectors = 
                ((GET_WORD(boot->base.root_entry_count) * 32) + 
                        (blocksz - 1)) / blocksz;

    // FAT size in sectors
    fat_size = GET_WORD(boot->base.table_size_16);

    if(fat_size == 0)
    {
        fat_size = GET_DWORD(boot->fat32.table_size_32);
    }

    if(fat_size == 0)
    {
        return 0;
    }

    // first data sector
    first_data_sector = GET_WORD(boot->base.reserved_sector_count) + 
                (boot->base.table_count * fat_size) + root_dir_sectors;

    // total sectors
    total_sectors = GET_WORD(boot->base.total_sectors_16);

    if(total_sectors == 0)
    {
        total_sectors = GET_DWORD(boot->base.total_sectors_32);
    }

    if(total_sectors == 0)
    {
        return 0;
    }

    // data sectors
    data_sectors = total_sectors - first_data_sector;

    if(sectors_per_cluster == 0)
    {
        return 0;
    }

    // total clusters
    total_clusters = data_sectors / sectors_per_cluster;

    // now determine the type of FAT system we have
    if(blocksz == 0)
    {
        ;
    }
    else if(total_clusters < 65525)
    {
        if(!VALID_FAT_SIG(boot->fat12_16.boot_signature))
        {
            return 0;
        }
    }
    else
    {
        if(!VALID_FAT_SIG(boot->fat32.boot_signature))
        {
            return 0;
        }
    }

    return 1;
}


static int is_iso9660_fs(char *myname, FILE *f, char *buf)
{
    size_t count;
    long blockno;
    long off;

    /* Volume Descriptors start at sector 0x10 */
    blockno = 0x10;
    
read:

    off = blockno * ISO9660_BLOCKSZ;

    if(fseek(f, off, SEEK_SET) < 0)
    {
        fprintf(stderr, "%s: failed to fseek device: %s\n", myname, strerror(errno));
        return 0;
    }

    if(ftell(f) != off)
    {
        fprintf(stderr, "%s: failed to ftell device: %s\n", myname, strerror(errno));
        return 0;
    }

    if((count = fread(buf, 1, BUFSZ, f)) < BUFSZ)
    {
        char *err;

        if(ferror(f))
        {
            err = strerror(errno);
        }
        else if(count == 0)
        {
            err = "fread() returned 0 bytes";
        }
        else
        {
            err = "fread() returned bytes < 2048";
        }

        exit_failed_guess(myname, "failed to read from device", err);
    }

    /* Check the identifier 'CD001' */
    if(buf[1] != 'C' || buf[2] != 'D' || buf[3] != '0' ||
       buf[4] != '0' || buf[5] != '1')
    {
        return 0;
    }
    
    /* Primary Volume Descriptor */
    if(*buf == 1)
    {
        return 1;
    }
    
    /* Any more Volume Descriptors? */
    /* 255 is for Volume Descriptor Set Terminator */
    if(*(unsigned char *)buf != 255)
    {
        blockno++;
        goto read;
    }

    return 0;
}


/*
 * Try to guess the filesystem type of the given device.
 *
 * TODO: Read /proc/filesystems and only check the filesystems supported
 *       by the running kernel.
 */
char *guess_fstype(char *myname, char *devname)
{
    FILE *f;
    struct stat st;
    char buf[BUFSZ];
    size_t count;

    fprintf(stderr, "%s: trying to guess filesystem name for device: %s\n",
                    myname, devname);

    if(stat(devname, &st) < 0)
    {
        exit_failed_guess(myname, "failed stat", strerror(errno));
    }

    if(!S_ISBLK(st.st_mode))
    {
        exit_failed_guess(myname, "cannot mount", "not a block device");
    }

    if(!(f = fopen(devname, "r")))
    {
        exit_failed_guess(myname, "failed to open device", strerror(errno));
    }

    if((count = fread(buf, 1, BUFSZ, f)) < BUFSZ)
    {
        char *err;

        if(ferror(f))
        {
            err = strerror(errno);
        }
        else if(count == 0)
        {
            err = "fread() returned 0 bytes";
        }
        else
        {
            err = "fread() returned bytes < 2048";
        }

        fclose(f);
        exit_failed_guess(myname, "failed to read from device", err);
    }

    if(is_ext2_fs(buf))
    {
        fclose(f);
        return "ext2";
    }

    if(is_fat_fs(buf))
    {
        fclose(f);
        return "vfat";
    }

    if(is_iso9660_fs(myname, f, buf))
    {
        fclose(f);
        return "iso9660";
    }

    fclose(f);
    exit_failed_guess(myname, "failed to guess filesystem type", "unrecognised filesystem");

    return NULL;        // pacify gcc
}

