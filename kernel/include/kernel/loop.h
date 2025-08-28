/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: loop.h
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
 *  \file loop.h
 *
 *  Functions and macros for working with loopback block devices.
 *  We maintain compatibility with Linux's flags and structs for loopback devices.
 */

#ifndef __KERNEL_LOOPDEV_H__
#define __KERNEL_LOOPDEV_H__

#include <stdint.h>

/*
 * Loopback devices major device number
 */
#define LODEV_MAJ                   7

/*
 * Major device number used for loopback device partitions
 */
#define LODEV_PART_MAJ              259

/*
 * Maximum supported loopback partitions (system-wide)
 */
#define MAX_LODEV_PARTITIONS        256

/*
 * Sizes for different array members of struct loop_info
 */
#define LO_NAME_SIZE                64
#define LO_KEY_SIZE                 32

/*
 * Loopback device flags
 */
#define LO_FLAGS_READ_ONLY          1
#define LO_FLAGS_AUTOCLEAR          4
#define LO_FLAGS_PARTSCAN           8
#define LO_FLAGS_DIRECT_IO          16


struct loop_info
{
    int lo_number;
    dev_t lo_device;
    unsigned long lo_inode;
    dev_t lo_rdevice;
    int lo_offset;
    int lo_encrypt_type;
    int lo_encrypt_key_size;
    int lo_flags;
    char lo_name[LO_NAME_SIZE];
    unsigned char lo_encrypt_key[LO_KEY_SIZE];
    unsigned long lo_init[2];
    char reserved[4];
};

struct loop_info64
{
    uint64_t lo_device;
    uint64_t lo_inode;
    uint64_t lo_rdevice;
    uint64_t lo_offset;
    uint64_t lo_sizelimit;
    uint32_t lo_number;
    uint32_t lo_encrypt_type;
    uint32_t lo_encrypt_key_size;
    uint32_t lo_flags;
    uint8_t lo_file_name[LO_NAME_SIZE];
    uint8_t lo_crypt_name[LO_NAME_SIZE];
    uint8_t lo_encrypt_key[LO_KEY_SIZE];
    uint64_t lo_init[2];
};

struct loop_config
{
    uint32_t fd;
    uint32_t block_size;
    struct loop_info64 info;
    uint64_t __reserved[8];
};

/*
 * /dev/loop* ioctl commands
 */

#define LOOP_SET_FD             0x4C00
#define LOOP_CLR_FD             0x4C01
#define LOOP_SET_STATUS         0x4C02
#define LOOP_GET_STATUS         0x4C03
#define LOOP_SET_STATUS64       0x4C04
#define LOOP_GET_STATUS64       0x4C05
#define LOOP_CHANGE_FD          0x4C06
#define LOOP_SET_CAPACITY       0x4C07
#define LOOP_SET_DIRECT_IO      0x4C08
#define LOOP_SET_BLOCK_SIZE     0x4C09
#define LOOP_CONFIGURE          0x4C0A

/*
 * /dev/loop-control ioctl commands
 */
#define LOOP_CTL_ADD            0x4C80
#define LOOP_CTL_REMOVE         0x4C81
#define LOOP_CTL_GET_FREE       0x4C82

#endif      /* __KERNEL_LOOPDEV_H__ */
