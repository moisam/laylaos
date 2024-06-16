/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: icolib.h
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
 *  \file icolib.h
 *
 *  Definition of the icolib header, which is used when loading and working
 *  with system icon library files.
 */

#ifndef ICONLIB_H
#define ICONLIB_H

/*
 * Icon library header structure (all data is Little Endian):
 *
 * Currently the only format supported is RGBA (with R in the high-order byte),
 * and all multibyte values are Little Endian.
 *
 * Offset  +------------------+---------------------+
 * 0       | Signature        | 4 bytes - 'ICLB'    |
 * 4       | Header size      | 4 bytes - '44'      |
 * 8       | Version          | 4 bytes - '1'       |
 * 12      | OS Name          | 8 bytes             |
 * 20      | Icon count       | 2 bytes             |
 * 22      | Bytes per pixel  | 1 byte  - '4'       |
 * 23      | Pixel format     | 1 byte  - '0'       |
 * 24      | Tags offset      | 4 bytes             |
 * 28      | Tags size        | 4 bytes             |
 * 32      | Icon data offset | 4 bytes             |
 * 36      | Icon sizes       | 8 bytes - see below |
 * 44      +------------------+---------------------+
 *
 * This is followed by optional tags that describe each icon, the tag count is
 * equal to the icon count. The tag data is a series of null-terminated strings,
 * one after the other. The total size is found in the 'Tags size' header field.
 *
 * This is then followed by the actual RGBA pixel data. These are found in lumps.
 * Each lump contains the data for all icons with the same icon size. The number
 * of lumps is equal to that found at offset 36 (Icon sizes). Upto 8 sizes could
 * be defined, any unused size is set to 0 and all unused sizes MUST be at the end
 * of the 8 byte list. For example, an icon library with 32, 48 and 72 pixel sized
 * icons will have the bytes { 32, 48, 72, 0, 0, 0, 0, 0 } starting at offset 36.
 *
 * The number of icons in each lump should be equal to the global count, found
 * at offset 20. Therefore, the count is not stored with each lump and is stored
 * only once in the file header.
 *
 * The storage size of a single image with a given pixel size can be found with:
 *   bytes = icon_size * icon_size * 4
 *
 * There are no byte paddings at the end of lines or at the end of an image.
 * When an image ends, the next one begins in the byte immediately following it.
 *
 */

#include <stdint.h>

#define ICOLIB_HDR0         'I'
#define ICOLIB_HDR1         'C'
#define ICOLIB_HDR2         'L'
#define ICOLIB_HDR3         'B'

struct icolib_hdr_t
{
    char signature[4];
    uint32_t hdrsz;
    uint32_t version;
    char osname[8];
    uint16_t icocount;
    uint8_t bpp;
    uint8_t format;
    uint32_t tagoff, tagsz;
    uint32_t dataoff;
    uint8_t icosz[8];
};

#endif      /* ICONLIB_H */
