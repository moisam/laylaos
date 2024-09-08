/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: jpeg.c
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
 *  \file jpeg.c
 *
 *  This file contains a shared function that loads JPEG (*.JPEG; *.JPG) 
 *  image files. It relies on libjpeg to do the actual loading, then convert
 *  the image to our standard RGBA format.
 */

#include <errno.h>
#include <stdio.h>
#include <jpeglib.h>
#include "../include/bitmap.h"

struct bitmap32_t *jpeg_load(char *file_name, struct bitmap32_t *loaded_bitmap)
{
    FILE *file;
    struct bitmap32_t *res_bitmap = NULL;

    if((file = fopen(file_name, "rb")) == NULL)
    {
        fprintf(stderr, "Error opening '%s': %s\n", file_name, strerror(errno));
        return NULL;
    }
    
    res_bitmap = jpeg_load_file(file, loaded_bitmap);
    fclose(file);

    return res_bitmap;
}


/*
 * See:
 *   https://stackoverflow.com/questions/5616216/need-help-in-reading-jpeg-file-using-libjpeg
 */
struct bitmap32_t *jpeg_load_file(FILE *file, struct bitmap32_t *loaded_bitmap)
{
    struct bitmap32_t *res_bitmap = NULL;
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;
    unsigned int x, y, w, h, chan;
    unsigned char *data = NULL, *row;
    size_t datasz;
    uint32_t *bitmap, *tmp, color;

    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);
    jpeg_stdio_src(&info, file);

    if(jpeg_read_header(&info, TRUE) != JPEG_HEADER_OK)
    {
        fprintf(stderr, "jpeg: failed to read header\n");
        goto fin;
    }

    jpeg_start_decompress(&info);

    w = info.output_width;
    h = info.output_height;
    chan = info.num_components;     // 3 = RGB, 4 = RGBA
    datasz = w * h * chan;

    if(!(data = malloc(datasz)))
    {
        fprintf(stderr, "jpeg: failed to alloc buffer: %s\n", strerror(errno));
        goto fin;
    }

    while(info.output_scanline < h)
    {
        row = data + info.output_scanline * w * chan;
        jpeg_read_scanlines(&info, &row, 1);
    }

    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);

    if(err.num_warnings != 0)
    {
        fprintf(stderr, "jpeg: failed to load image: corrupt data\n");
        goto fin;
    }

    if(!(bitmap = malloc(sizeof(uint32_t) * w * h)))
    {
        fprintf(stderr, "jpeg: failed to alloc bitmap: %s\n", strerror(errno));
        goto fin;
    }

    for(tmp = bitmap, y = 0; y < h; y++)
    {
        row = data + y * w * chan;

        for(x = 0; x < w; x++)
        {
            color = ((uint32_t)row[0] << 24) |
                    ((uint32_t)row[1] << 16) |
                    ((uint32_t)row[2] << 8 ) |
                    ((chan == 4) ? ((uint32_t)row[3]) : 0xff);
            *tmp++ = color;
            row += chan;
        }
    }

    loaded_bitmap->data = bitmap;
    loaded_bitmap->width = w;
    loaded_bitmap->height = h;

    res_bitmap = loaded_bitmap;

fin:

    if(data)
    {
        free(data);
    }

    return res_bitmap;
}

