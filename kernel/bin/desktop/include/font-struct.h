/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: font-struct.h
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
 *  \file font-struct.h
 *
 *  Definitions of font-related structures.
 */

#ifndef GUI_FONT_STRUCT_H
#define GUI_FONT_STRUCT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SIZES_H
#include <freetype/ftglyph.h>

struct Cached_TGlyph
{
    FT_UInt    index;   // glyph index
    FT_Glyph   image;   // glyph image 
    int ptsz;
    FT_Pos     advance_x;
};

struct font_cache_t
{
    int ptsz;
    FT_UInt    glyph_count;
    struct Cached_TGlyph *glyphs;
    struct font_cache_t *next;
};

struct font_t
{
    int ptsz;
    int charw, charh;
    uint8_t *data;
    size_t datasz;

    FT_Face ft_face;  // only useful for non-fixed-width fonts
    struct font_cache_t *glyph_caches;
    FT_Size ftsize;

    mutex_t lock;

#define FONT_FLAG_FIXED_WIDTH       0x00
#define FONT_FLAG_TRUE_TYPE         0x01
#define FONT_FLAG_DATA_SHMEM        0x02
#define FONT_FLAG_SYSTEM_FONT       0x04
    int flags;
    int shmid;        // only if FONT_FLAG_DATA_SHMEM is set
};

#endif      /* GUI_FONT_STRUCT_H */
