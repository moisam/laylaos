/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: x11cur.c
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
 *  \file x11cur.c
 *
 *  This file contains a shared function that loads X11 cursor files.
 *
 *  @See: https://invisible-island.net/xterm/xcursor/xcursor.html
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <endian.h>
#include "../include/cursor-struct.h"
#include "../include/cursor-struct-alloc.h"

struct xcur_hdr_t
{
    uint32_t magic;
    uint32_t bytes;
    uint32_t ver;
    uint32_t ntoc;
} __attribute((packed));

struct toc_entry_t
{
    uint32_t type;
    uint32_t subtype;
    uint32_t filepos;
} __attribute((packed));

struct chunk_hdr_t
{
    uint32_t bytes;
    uint32_t type;
    uint32_t subtype;
    uint32_t ver;
} __attribute((packed));

struct comment_chunk_hdr_t
{
    struct chunk_hdr_t hdr;
    uint32_t strsz;
    uint8_t str[];
} __attribute((packed));

struct img_chunk_hdr_t
{
    struct chunk_hdr_t hdr;
    uint32_t w;
    uint32_t h;
    uint32_t xhot;
    uint32_t yhot;
    uint32_t delay;
    uint32_t pixels[];
} __attribute((packed));

#define invalid_image(msg)                  \
{                                           \
    fprintf(stderr, msg);                   \
    if(res_bitmap)                          \
        cursor_struct_free(res_bitmap);     \
    res_bitmap = NULL;                      \
    goto fin;                               \
}

#define FREAD(a, c, b, fx)                   \
    if(fread(a, c, b, fx) < 0)               \
        invalid_image("x11cur: prematue EOF\n");


struct cursor_t *x11_cursor_load_sz(char *file_name, int reqsz)
{
    int i, j;
    int index, count = 0;
    size_t sz;
    FILE *file = NULL;
    struct cursor_t *res_bitmap = NULL;
    struct toc_entry_t *toc = NULL;
    struct img_chunk_hdr_t *x, *chunks = NULL;
    struct xcur_hdr_t hdr;

    if((file = fopen(file_name, "rb")) == NULL)
    {
        fprintf(stderr, "x11cur: error opening '%s': %s\n", file_name, strerror(errno));
        return NULL;
    }

    // read in the file header
    FREAD(&hdr, sizeof(struct xcur_hdr_t), 1, file);

#if BYTE_ORDER == BIG_ENDIAN
    hdr.magic = swap_dword(hdr.magic);
    hdr.bytes = swap_dword(hdr.bytes);
    hdr.ntoc = swap_dword(hdr.ntoc);
#endif

    // verify header magic
    if(hdr.magic != 0x72756358)
    {
        fprintf(stderr, "x11cur: invalid file header\n");
        fclose(file);
        return NULL;
    }

    // read the table of contents
    if(!(toc = malloc(sizeof(struct toc_entry_t) * hdr.ntoc)))
    {
        invalid_image("x11cur: insufficient memory\n");
    }

    if(!(chunks = malloc(sizeof(struct img_chunk_hdr_t) * hdr.ntoc)))
    {
        invalid_image("x11cur: insufficient memory\n");
    }

    FREAD(toc, sizeof(struct toc_entry_t) * hdr.ntoc, 1, file);

    // count image headers and ignore comment headers
    for(index = 0; index < hdr.ntoc; index++)
    {
        x = &chunks[index];
        fseek(file, toc[index].filepos, SEEK_SET);
        FREAD(x, sizeof(struct chunk_hdr_t), 1, file);

#if BYTE_ORDER == BIG_ENDIAN
        x->hdr.bytes = swap_dword(x->hdr.bytes);
        x->hdr.type = swap_dword(x->hdr.type);
        x->hdr.subtype = swap_dword(x->hdr.subtype);
#endif

        if(x->hdr.type != 0xfffd0002 ||      // not an image header
           x->hdr.bytes != 36)               // or wrong size
        {
            continue;
        }

        fseek(file, toc[index].filepos, SEEK_SET);
        FREAD(x, sizeof(struct img_chunk_hdr_t), 1, file);

        if(x->w == 0 || x->h == 0)           // invalid dimensions
        {
            continue;
        }

        if(reqsz != 0 && reqsz != x->w)      // find the requested size
        {
            continue;
        }

        count++;

#if BYTE_ORDER == BIG_ENDIAN
        x->w = swap_dword(x->w);
        x->h = swap_dword(x->h);
        x->xhot = swap_dword(x->xhot);
        x->yhot = swap_dword(x->yhot);
        x->delay = swap_dword(x->delay);
#endif
    }

    if(count == 0)
    {
        invalid_image("x11cur: invalid image count\n");
    }

    if(!(res_bitmap = cursor_struct_alloc(count)))
    {
        invalid_image("x11cur: insufficient memory\n");
    }

    count = 0;

    // read cursor images
    for(index = 0; index < hdr.ntoc; index++)
    {
        x = &chunks[index];

        if(x->hdr.type != 0xfffd0002 ||      // not an image header
           x->hdr.bytes != 36)               // or wrong size
        {
            continue;
        }

        if(x->w == 0 || x->h == 0)           // invalid dimensions
        {
            continue;
        }

        if(reqsz != 0 && reqsz != x->w)      // find the requested size
        {
            continue;
        }

        sz = x->w * x->h * 4;

        if(!(res_bitmap->bitmaps[count].data = malloc(sz)))
        {
            invalid_image("x11cur: insufficient memory\n");
        }

        fseek(file, toc[index].filepos + sizeof(struct img_chunk_hdr_t), SEEK_SET);
        FREAD(res_bitmap->bitmaps[count].data, sz, 1, file);

#if BYTE_ORDER == BIG_ENDIAN
        for(j = x->w * x->h, i = 0; i < j; i++)
        {
            res_bitmap->bitmaps[count].data[i] =
                swap_dword(res_bitmap->bitmaps[count].data[i]);
        }
#endif

        // convert from ARGB to RGBA
        for(j = x->w * x->h, i = 0; i < j; i++)
        {
            res_bitmap->bitmaps[count].data[i] =
                (res_bitmap->bitmaps[count].data[i] >> 24) |
                (res_bitmap->bitmaps[count].data[i] << 8);
        }

        res_bitmap->bitmaps[count].w = x->w;
        res_bitmap->bitmaps[count].h = x->h;
        res_bitmap->bitmaps[count].hotx = x->xhot;
        res_bitmap->bitmaps[count].hoty = x->yhot;
        res_bitmap->bitmaps[count].delay = x->delay;
        res_bitmap->bitmaps[count].flags = CURSOR_FLAG_MALLOCED;

        // ensure we have a sensible delay in millisecs
        if(res_bitmap->bitmaps[count].delay == 0)
        {
            res_bitmap->bitmaps[count].delay = 50;
        }

        if(res_bitmap->bitmaps[count].delay > 100)
        {
            res_bitmap->bitmaps[count].delay = 100;
        }

        count++;
    }


fin:
    fclose(file);

    if(toc)
    {
        free(toc);
    }
    
    if(chunks)
    {
        free(chunks);
    }

    return res_bitmap;
}


struct cursor_t *x11_cursor_load(char *file_name)
{
    return x11_cursor_load_sz(file_name, 0);
}

