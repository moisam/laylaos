/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: loop_internal.h
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
 *  \file loop_internal.h
 *
 *  Loopback functions and macros internal to the kernel.
 */

#ifndef __KERNEL_LOOPDEV_INTERNAL_H__
#define __KERNEL_LOOPDEV_INTERNAL_H__


/**
 * @struct lodev_t
 * @brief Loopback device.
 *
 * A structure to represent a loopback block device inside the kernel.
 */
struct lodev_t
{
    size_t offset,      /**< offset in bytes in the backing file */
           sizelimit;   /**< device size in bytes, zero for the whole backing file */
    int flags;          /**< internal flags (see loop.h) */
    int state;          /**< device state (see loop.c) */
    int number;         /**< device index */
    size_t blocksz;     /**< logical block size for R/W operations */
    char filename[64];  /**< backing file name */
    struct file_t *file;            /**< pointer to the backing file */
};


/* Our master table for loopback devices and their partitions */
extern struct lodev_t *lodev[];
extern struct parttab_s *lodev_disk_part[];


/****************************************
 * Internal kernel functions
 ****************************************/
int lodev_first_free(void);
int lodev_add_index(long n);
int lodev_remove_index(long n);
int lodev_open(dev_t dev);
void lodev_release(struct file_t *f);

#endif      /* __KERNEL_LOOPDEV_INTERNAL_H__ */
