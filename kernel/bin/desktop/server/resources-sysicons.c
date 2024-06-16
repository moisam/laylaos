/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: resources-sysicons.c
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
 *  \file resources-sysicons.c
 *
 *  Functions to work with system icon resources on the server side.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/icolib.h"
#include "../include/bitmap.h"

#define SYSICONS_DIR        "/usr/share/gui/desktop"
#define SYSICONS_FILE       "sysicons.icolib"
#define SYSICONS_PATH        SYSICONS_DIR "/" SYSICONS_FILE


FILE *sysicons_file = NULL;
struct icolib_hdr_t sysicons_filehdr;

void *tags_src = NULL;
char **tags = NULL;


void server_init_sysicon_resources(void)
{
    int i;
    char *p;

    memset(&sysicons_filehdr, 0, sizeof(sysicons_filehdr));

    if(!(sysicons_file = fopen(SYSICONS_PATH, "r")))
    {
        fprintf(stderr, "Failed to open %s: %s", 
                        SYSICONS_PATH, strerror(errno));
        return;
    }

    if((i = fread(&sysicons_filehdr, 1, sizeof(sysicons_filehdr),
                                sysicons_file)) != sizeof(sysicons_filehdr))
    {
        fprintf(stderr,
                "Failed to read file header (got %d bytes, expected %lu)",
                i, sizeof(sysicons_filehdr));
        goto fail;
    }

    if(sysicons_filehdr.signature[0] != ICOLIB_HDR0 ||
       sysicons_filehdr.signature[1] != ICOLIB_HDR1 ||
       sysicons_filehdr.signature[2] != ICOLIB_HDR2 ||
       sysicons_filehdr.signature[3] != ICOLIB_HDR3)
    {
        fprintf(stderr, "Invalid header signature");
        goto fail;
    }

    if(sysicons_filehdr.version != 1)
    {
        fprintf(stderr, "Invalid header version (%d)", 
                        sysicons_filehdr.version);
        goto fail;
    }

    if(!(tags_src = malloc(sysicons_filehdr.tagsz)))
    {
        fprintf(stderr, "Failed to alloc memory to check tags");
        goto fail;
    }

    if((i = fread(tags_src, 1, 
                    sysicons_filehdr.tagsz, sysicons_file)) != 
                                                sysicons_filehdr.tagsz)
    {
        fprintf(stderr, "Failed to read tags (got %d bytes, expected %u)", 
                        i, sysicons_filehdr.tagsz);
        goto fail;
    }

    if(!(tags = malloc(sysicons_filehdr.icocount * sizeof(char *))))
    {
        fprintf(stderr, "Failed to alloc memory to store tags");
        goto fail;
    }

    p = tags_src;

    for(i = 0; i < sysicons_filehdr.icocount; i++)
    {
        tags[i] = p;
        p += strlen(p) + 1;
    }

    return;

fail:

    fclose(sysicons_file);
    sysicons_file = NULL;

    if(tags_src)
    {
        free(tags_src);
        tags_src = NULL;
    }
}


struct bitmap32_array_t *server_sysicon_resource_load(char *name)
{
    struct bitmap32_array_t *bmpa;
    off_t off, base = sysicons_filehdr.dataoff;
    size_t bufsz;
    int i, w, n;
    char *buf;

    // find the index of the icon
    for(n = 0; n < sysicons_filehdr.icocount; n++)
    {
        if(strcmp(tags[n], name) == 0)
        {
            break;
        }
    }

    // invalid icon tag
    if(n == sysicons_filehdr.icocount)
    {
        return NULL;
    }

    // find how many sizes are there
    for(i = 0; i < 8; i++)
    {
        if(sysicons_filehdr.icosz[i] == 0)
        {
            break;
        }
    }

    // this should not happen
    if(i == 0 || sysicons_file == NULL)
    {
        return NULL;
    }

    // get a buffer
    if(!(bmpa = bitmap32_array_alloc(i)))
    {
        return NULL;
    }

    // read the icon in all available sizes
    for(i = 0; i < 8; i++)
    {
        if(sysicons_filehdr.icosz[i] == 0)
        {
            break;
        }

        w = sysicons_filehdr.icosz[i];
        bufsz = w * w * 4;
        off = (bufsz * n) + base;

        if(!(buf = malloc(bufsz)))
        {
            break;
        }

        if(fseek(sysicons_file, off, SEEK_SET) < 0)
        {
            break;
        }

        if((fread(buf, 1, bufsz, sysicons_file)) != bufsz)
        {
            break;
        }

        bmpa->bitmaps[i].width = w;
        bmpa->bitmaps[i].height = w;
        bmpa->bitmaps[i].data = (uint32_t *)buf;

        base += bufsz * sysicons_filehdr.icocount;
    }

    return bmpa;
}

