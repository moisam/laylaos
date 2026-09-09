/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: blkpg.h
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
 *  \file blkpg.h
 *
 *  Functions and macros for adding, removing and resizing block device 
 *  partitions.  This is based on Linux's BLKPG ioctl and requires its header
 *  file.
 */

#ifndef KERNEL_BLKPG_H
#define KERNEL_BLKPG_H

#include <sys/blkpg.h>

/**
 * @struct blkpg_ops
 * @brief The blkpg_ops structure.
 *
 * A structure to represent block page (BLKPG) operations on disks.
 */
struct blkpg_ops_t
{
    long (*remove)(dev_t devid, int part);  /**< remove a partition */
    long (*overlaps)(dev_t devid, int partno, size_t lba, size_t end);
                                            /**< check for partition overlaps */
    long (*add)(dev_t devid, int partno, size_t lba, size_t end);
                                            /**< add a new partition */
    long (*resize)(dev_t devid, int partno, size_t lba, size_t end);
                                            /**< resize an existing partition */
    int (*maxparts)(dev_t devid);           /**< max partitions for this device */
};


/*********************************************
 * Internal functions
 *********************************************/

long common_blkpg_ioctl(dev_t devid, struct ata_dev_s *dev, 
                        struct parttab_s *part, 
                        struct blkpg_ops_t *ops, char *arg);

#endif      /* KERNEL_BLKPG_H */
