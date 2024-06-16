/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: member.h
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
 *  \file member.h
 *
 *  Functions and macros for deflating (unzipping) GZip archives. This is
 *  used during boot to unzip the initial ramdisk. These functions work
 *  together with those declared in deflate.h to unzip the archive.
 */

#ifndef GZIP_MEMBER_H
#define GZIP_MEMBER_H

#include <stdint.h>
#include <stddef.h>

#ifdef KERNEL
#include <mm/mmngr_virtual.h>
#endif

/*
 * RFC 1952 (GZIP File Format Specification) says:

   Each member has the following structure:

         +---+---+---+---+---+---+---+---+---+---+
         |ID1|ID2|CM |FLG|     MTIME     |XFL|OS | (more-->)
         +---+---+---+---+---+---+---+---+---+---+

      (if FLG.FEXTRA set)

         +---+---+=================================+
         | XLEN  |...XLEN bytes of "extra field"...| (more-->)
         +---+---+=================================+

      (if FLG.FNAME set)

         +=========================================+
         |...original file name, zero-terminated...| (more-->)
         +=========================================+

      (if FLG.FCOMMENT set)

         +===================================+
         |...file comment, zero-terminated...| (more-->)
         +===================================+

      (if FLG.FHCRC set)

         +---+---+
         | CRC16 |
         +---+---+

         +=======================+
         |...compressed blocks...| (more-->)
         +=======================+

           0   1   2   3   4   5   6   7
         +---+---+---+---+---+---+---+---+
         |     CRC32     |     ISIZE     |
         +---+---+---+---+---+---+---+---+
*/

struct gzip_member_s
{
    uint8_t  ID1, ID2,  // identification 1 & 2
#define COMPRESSION_METHOD_DEFLATE      (8)
             CM,        // compression method
#define FLAG_FTEXT                      (1 << 0)
#define FLAG_FHCRC                      (1 << 1)
#define FLAG_FEXTRA                     (1 << 2)
#define FLAG_FNAME                      (1 << 3)
#define FLAG_FCOMMENT                   (1 << 4)
#define FLAG_RESERVED                   (3 << 5)
             FLG;       // flags
    uint32_t MTIME;
#define XFL_MAX_COMPRESSION             (2)
#define XFL_FASTEST_ALGORITHM           (4)
    uint8_t  XFL,
#define OS_FAT          (0)   // 0 - FAT filesystem (MS-DOS, OS/2, NT/Win32)
#define OS_AMIGA        (1)   // 1 - Amiga
#define OS_VMS          (2)   // 2 - VMS (or OpenVMS)
#define OS_UNIX         (3)   // 3 - Unix
#define OS_VM_CMS       (4)   // 4 - VM/CMS
#define OS_ATARI        (5)   // 5 - Atari TOS
#define OS_HPFS         (6)   // 6 - HPFS filesystem (OS/2, NT)
#define OS_MACINTOSH    (7)   // 7 - Macintosh
#define OS_ZSYSTEM      (8)   // 8 - Z-System
#define OS_CPM          (9)   // 9 - CP/M
#define OS_TOPS         (10)  // 10 - TOPS-20
#define OS_NTFS         (11)  // 11 - NTFS filesystem (NT)
#define OS_QDOS         (12)  // 12 - QDOS
#define OS_RISCOS       (13)  // 13 - Acorn RISCOS
#define OS_UNKNOWN      (255) // 255 - unknown
             OS;
} __attribute__((packed));

struct gzip_fextra_s
{
    uint16_t XLEN;
    char     data[];
};

/* return codes from read_member() */
#define GZIP_VALID_ARCHIVE         (0)  /**< read_member() returned success */
#define GZIP_INVALID_SIGNATURE     (1)  /**< read_member() encountered an
                                             invalid signature */
#define GZIP_INVALID_CM            (2)  /**< read_member() encountered an
                                             invalid compression method */
#define GZIP_INVALID_XFL           (3)  /**< read_member() encountered an
                                             invalid compression algorithm */
#define GZIP_INVALID_OS            (4)  /**< read_member() encountered an
                                             invalid OS field */
#define GZIP_INVALID_FLG           (5)  /**< read_member() encountered an
                                             invalid flags */

#define GZIP_INVALID_CRC32         (6)  /**< read_member() calculated
                                             invalid CRC */
#define GZIP_INVALID_ISIZE         (7)  /**< read_member() reports
                                             invalid input data size */

#ifdef KERNEL

/**
 * @brief Decompress the initial ramdisk.
 *
 * Called during early boot to decompress the initial ramdisk.
 *
 * @param   src         address of compressed image
 * @param   srcsize     size of compressed image
 * @param   dest        decompressed image address will be returned here
 * @param   destsize    size of decompressed image will be returned here
 *
 * @return  \ref GZIP_VALID_ARCHIVE (0) on success, non-zero on failure.
 *
 * @see deflate()
 */
int read_member(char *src, long srcsize, virtual_addr *dest, size_t *destsize);

/**
 * @brief Print human-friendly archive size.
 *
 * Given \a size in bytes, this function returns a pretty string in the form
 * of "XXX kB". Mainly called during early boot to give the user an update on
 * the state and size of the initial ramdisk after it is deflated.
 *
 * @param   size            size in bytes
 *
 * @return  formatted string, pointer to a static buffer.
 */
char *get_mbs(long size);

#endif      /* KERNEL */

#endif      /* GZIP_MEMBER_H */
