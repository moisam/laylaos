/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: magic.h
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
 *  \file magic.h
 *
 *  Macros defining different filesystem magic numbers.
 */

#ifndef __MAGIC_H__
#define __MAGIC_H__

/*
 * For a full list of filesys magic numbers, see:
 *
 *    https://man7.org/linux/man-pages/man2/statfs.2.html
 *
 * and (under a GNU/Linux system with kernel headers installed):
 *
 *    /usr/include/linux/magic.h
 */

#undef EXT2_SUPER_MAGIC

/*
 * Different filesystem magic numbers.
 */
#define TMPFS_MAGIC             0x01021994  /**< tmpfs magic number */
#define PIPEFS_MAGIC            0x50495045  /**< pipefs magic number */
#define PROC_SUPER_MAGIC        0x9fa0      /**< procfs magic number */
#define EXT2_SUPER_MAGIC        0xef53      /**< ext2 magic number */
#define EXT3_SUPER_MAGIC        0xef53      /**< ext3 magic number */
#define EXT4_SUPER_MAGIC        0xef53      /**< ext4 magic number */
#define DEVFS_SUPER_MAGIC       0x1373      /**< devfs magic number */

#define EXFAT_SUPER_MAGIC	    0x2011BAB0  /**< exfat magic number */

#endif      /* __MAGIC_H__ */
