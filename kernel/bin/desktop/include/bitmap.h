/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: bitmap.h
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
 *  \file bitmap.h
 *
 *  This file contains declarations of functions to load bitmaps, and
 *  definition of the struct bitmap32_t that is used in handling bitmaps.
 *
 *  All bitmaps are stored in memory in an RGBA format, where R is at the
 *  highest-order byte and A is at the lowest order byte.
 */

#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include <stdlib.h>     // malloc() & free()
#include <stdio.h>      // FILE
#include <string.h>
#include "endian.h"

#ifndef GUI_SERVER
#undef A_memcpy
#undef A_memset
#define A_memcpy        memcpy /* local_memcpy */
#define A_memset        memset /* local_memset */
#endif

#ifdef __cplusplus
extern "C" {
#endif

// prototypes for optimized memory functions from asmlib.a
// Library source can be downloaded from:
//     https://www.agner.org/optimize/#asmlib
void * A_memcpy(void * dest, const void * src, size_t count);
void * A_memset(void * dest, int c, size_t count);

typedef struct
{
    char red;
    char green;
    char blue;
    char alpha;
} rgb;

#define make_rgba(r, g, b, a)                       \
    ((((r) & 0xff) << 24) | (((g) & 0xff) << 16) |  \
     (((b) & 0xff) << 8 ) | (((a) & 0xff) << 0 ))

typedef struct
{
	char blue;
	char red;
	char green;
	char alpha;
} bgr;

struct bitmap32_t
{
    uint32_t *data;
    unsigned int width, height;
    unsigned int res1, res2;
};

struct bitmap32_array_t
{
    int count;
    struct bitmap32_t bitmaps[];
};


static inline struct bitmap32_array_t *bitmap32_array_alloc(int count)
{
    struct bitmap32_array_t *ba;
    size_t sz = sizeof(struct bitmap32_array_t) + 
                        (count * sizeof(struct bitmap32_t));

    if((ba = (struct bitmap32_array_t *)malloc(sz)))
    {
        //A_memset(ba, 0, sz);
        memset(ba, 0, sz);
        ba->count = count;
    }
    
    return ba;
}

static inline void bitmap32_array_free(struct bitmap32_array_t *ba)
{
    int i;
    
    for(i = 0; i < ba->count; i++)
    {
        if(ba->bitmaps[i].data)
        {
            free(ba->bitmaps[i].data);
        }
    }
    
    free(ba);
}


/*
 * Function prototypes.
 */

struct bitmap32_t *png_load(char *file_name, struct bitmap32_t *loaded_bitmap);
struct bitmap32_t *png_load_file(FILE *file, struct bitmap32_t *loaded_bitmap);

struct bitmap32_t *jpeg_load(char *file_name, struct bitmap32_t *loaded_bitmap);
struct bitmap32_t *jpeg_load_file(FILE *file, struct bitmap32_t *loaded_bitmap);

struct bitmap32_array_t *ico_load(char *file_name);

#ifdef __cplusplus
}
#endif

#endif      /* BITMAP_H */
