/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: ico.c
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
 *  \file ico.c
 *
 *  This file contains a shared function that loads icon (*.ICO) files.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <endian.h>
#include "../include/bitmap.h"
#include "../include/bmp.h"

#define SHOW_INFO       0


typedef struct
{
    uint16_t reserved;

#define ICON_IMAGE      1
#define CURSOR_IMAGE    2
    uint16_t type;

    uint16_t image_count;
} ICONDIR_S;

typedef struct
{
    uint8_t width;
    uint8_t height;
    uint8_t pal_count;
    uint8_t reserved;
    uint16_t color_planes;
    uint16_t bpp;
    uint32_t data_size;
    uint32_t data_offset;
} ICONDIRENTRY_S;

typedef struct
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t alpha;
} ico_bitmask;

static char png_sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };


char *comp_str(int32_t comp)
{
    switch(comp)
    {
        case 0: return "BI_RGB";
        case 1: return "BI_RLE8";
        case 2: return "BI_RLE4";
        case 3: return "BI_BITFIELDS";
        case 4: return "BI_JPEG";
        case 5: return "BI_PNG";
        case 6: return "BI_ALPHABITFIELDS";
        case 11: return "BI_CMYK";
        case 12: return "BI_CMYKRLE8";
        case 13: return "BI_CMYKTLE";
        default: return "Unknown";
    }
}


void invert_image(uint32_t *bitmap, int32_t width, int32_t height)
{
    int32_t line_bytes = width * sizeof(uint32_t);
    uint32_t *first = &bitmap[0];
    uint32_t *last = &bitmap[(height - 1) * width];
    uint32_t *mid = &bitmap[(height / 2) * width];
    uint32_t *tmp_line;
    
    if(!(tmp_line = malloc(line_bytes)))
    {
        return;
    }
    
    for( ; first < mid; first += width, last -= width)
    {
        memcpy(tmp_line, first, line_bytes);
        memcpy(first, last, line_bytes);
        memcpy(last, tmp_line, line_bytes);
    }
    
    free(tmp_line);
}


#define invalid_image(msg)                  \
{                                           \
    fprintf(stderr, msg);                   \
    if(file) fclose(file);                  \
    if(res_bitmap)                          \
        bitmap32_array_free(res_bitmap);    \
    res_bitmap = NULL;                      \
    goto fin;                               \
}

#define FREAD(a, c, b, f)                   \
    if(fread(a, c, b, f) < 0)               \
        invalid_image("Prematue EOF");


struct bitmap32_array_t *ico_load(char *file_name)
{
    int i;
    int index;
    FILE *file = NULL;
    struct bitmap32_array_t *res_bitmap = NULL;
    ICONDIR_S ICONDIR;
    ICONDIRENTRY_S *ICONDIRENTRY = NULL;

    // boolean to check if there is padding at the end of line of pixels
    volatile int32_t PADDING = 0;
    volatile int color_table_size = 0, color_table_bytes = 0;
    volatile bgr *color_table = NULL;
    volatile uint32_t *bitmap = NULL;

    if((file = fopen(file_name, "rb")) == NULL)
    {
        fprintf(stderr, "Error opening '%s': %s\n", 
                        file_name, strerror(errno));
        return NULL;
    }

    // read in the ICO file header
    FREAD(&ICONDIR, sizeof(ICONDIR), 1, file);

    if(ICONDIR.reserved != 0)
    {
        invalid_image("Error: header is corrupt\n");
    }

#if BYTE_ORDER == BIG_ENDIAN
    ICONDIR.image_count = swap_word(ICONDIR.image_count);
    ICONDIR.type = swap_word(ICONDIR.type);
#endif

    if(ICONDIR.type != ICON_IMAGE && ICONDIR.type != CURSOR_IMAGE)
    {
        invalid_image("Error: Unknown image type\n");
    }

    if(!(ICONDIRENTRY = malloc(ICONDIR.image_count * sizeof(ICONDIRENTRY_S))))
    {
        invalid_image("Error: Insufficient memory\n");
    }

    for(i = 0; i < ICONDIR.image_count; i++)
    {
        FREAD(&ICONDIRENTRY[i], 16, 1, file);

        if(ICONDIRENTRY[i].reserved != 0)
        {
            invalid_image("Error: header is corrupt\n");
        }

#if BYTE_ORDER == BIG_ENDIAN
        ICONDIRENTRY[i]->color_planes = swap_word(ICONDIRENTRY[i].color_planes);
        ICONDIRENTRY[i]->bpp = swap_word(ICONDIRENTRY[i].bpp);
        ICONDIRENTRY[i]->data_offset = swap_dword(ICONDIRENTRY[i].data_offset);
        ICONDIRENTRY[i]->data_size = swap_dword(ICONDIRENTRY[i].data_size);
#endif

    }
    
    if(SHOW_INFO)
    {
        printf("Your system is %s-endian.\n",
#if BYTE_ORDER == LITTLE_ENDIAN
                                        "little"
#else
                                        "big"
#endif
              );

        printf("%s file information:\n", (ICONDIR.type == ICON_IMAGE) ?
                                         "ICO" : "CUR");
        printf("======================\n");
        printf("Image count: %d\n", ICONDIR.image_count);

        for(i = 0; i < ICONDIR.image_count; i++)
        {
            printf("\nImage #%u\n", i+1);
            printf("  Width: %u\n", ICONDIRENTRY[i].width);
            printf("  Height: %u\n", ICONDIRENTRY[i].height);
            printf("  Col count: %u\n", ICONDIRENTRY[i].pal_count);
            printf("  Reserved: %u\n", ICONDIRENTRY[i].reserved);
            printf("  Planes: %u\n", ICONDIRENTRY[i].color_planes);
            printf("  BPP: %u\n", ICONDIRENTRY[i].bpp);
            printf("  Data size: %u\n", ICONDIRENTRY[i].data_size);
            printf("  Data offset: %u\n", ICONDIRENTRY[i].data_offset);
        }
    }
    
    if(!(res_bitmap = bitmap32_array_alloc(ICONDIR.image_count)))
    {
        invalid_image("Error: Insufficient memory\n");
    }
    
    for(index = 0; index < ICONDIR.image_count; index++)
    {
        //volatile long filepos = ftell(file);
        int bitmap_size;
        int count;
        volatile int is_png;
        volatile dib_header dibh;
        volatile ico_bitmask bm;
        volatile uint32_t red_shift, blue_shift, green_shift, alpha_shift;

        // The icon can be either a PNG or a BITMAP
        char x[8];

        fseek(file, ICONDIRENTRY[index].data_offset, SEEK_SET);
        FREAD(&x, 8, 1, file);
        fseek(file, ICONDIRENTRY[index].data_offset, SEEK_SET);

        for(is_png = 1, i = 0; i < 8; i++)
        {
            if(x[i] != png_sig[i])
            {
                is_png = 0;
                break;
            }
        }

        if(is_png)
        {
            if(!(png_load_file(file, &res_bitmap->bitmaps[index])))
            {
                fclose(file);
                bitmap32_array_free(res_bitmap);
                res_bitmap = NULL;
                goto fin;
            }

            continue;
        }

        // not PNG (that is, BMP)
        // read in the dib file header

        FREAD((void *)&dibh, 1, sizeof(dibh), file);

#if BYTE_ORDER == BIG_ENDIAN
        dibh.dib_size = swap_dword(dibh.dib_size);
        dibh.width = swap_dword(dibh.width);
        dibh.height = swap_dword(dibh.height);
        dibh.comp = swap_dword(dibh.comp);
        dibh.data_size = swap_dword(dibh.data_size);
        dibh.print_h = swap_dword(dibh.print_h);
        dibh.print_v = swap_dword(dibh.print_v);
        dibh.pal_colors = swap_dword(dibh.pal_colors);
        dibh.imp_colors = swap_dword(dibh.imp_colors);
        dibh.planes = swap_word(dibh.planes);
        dibh.bpp = swap_word(dibh.bpp);
#endif

        /*
         * NOTE: We can only support those two for now.
         *       For the other types, see bmp.h.
         */
        if(dibh.comp != BI_RGB && dibh.comp != BI_BITFIELDS)
        {
            invalid_image("Unsupported image format\n");
        }

        if(dibh.comp == BI_BITFIELDS)
        {
            FREAD((void *)&bm, 1, sizeof(bm), file);
        }
        else
        {
            /*
             * Uncompressed pixel data is stored as BGR.
             * See: https://atlc.sourceforge.net/bmp.html
             */
            bm.red = 0x00FF0000;
            bm.green = 0x0000FF00;
            bm.blue = 0x000000FF;
            bm.alpha = 0xFF000000;
        }

#define SHIFT(c)    ((c == 0xFF000000) ? 24 :       \
                     ((c == 0x00FF0000) ? 16 :      \
                      ((c == 0x0000FF00) ? 8 : 0)))

        red_shift = SHIFT(bm.red);
        green_shift = SHIFT(bm.green);
        blue_shift = SHIFT(bm.blue);
        alpha_shift = SHIFT(bm.alpha);

#undef SHIFT

        if(dibh.bpp == 1)
        {
            color_table_size = 2;
        }
        else if(dibh.bpp == 4)
        {
            color_table_size = 16;
        }
        else if(dibh.bpp == 8)
        {
            color_table_size = 256;
        }
        else
        {
            color_table_size = 0;
        }

        color_table_bytes = color_table_size * sizeof(bgr);

        if(color_table_bytes)
        {
            if(!(color_table = malloc(color_table_bytes)))
            {
                invalid_image("Error: Insufficient memory\n");
            }

            FREAD((void *)color_table, 1, color_table_bytes, file);
        }
        else
        {
            color_table = NULL;
        }

        if(SHOW_INFO)
        {
            printf("\nImage #%d:\n", index + 1);
            printf("  Data offset: %u\n", ICONDIRENTRY[index].data_offset);
            printf("  Data size: %u bytes\n", ICONDIRENTRY[index].data_size);

            printf("  XOR Image:\n");
            printf("  DIB header size: %d bytes\n", dibh.dib_size);
            printf("  BMP Width: %d pixels\n", dibh.width);
            printf("  BMP Height: %d pixels\n", dibh.height);
            printf("  Color planes: %d\n", dibh.planes);
            printf("  Bits-per-pixel (bpp): %d\n", dibh.bpp);
            printf("  Pixel compression: %s\n", comp_str(dibh.comp));
            printf("  Bitmask: R %#08x, G %#08x, B %#08x, A %#08x\n", 
                   bm.red, bm. green, bm.blue, bm.alpha);
            printf("  Raw bitmap data size: %d bytes\n", dibh.data_size);
            printf("  Horizontal print res.: %d pixels/m\n", dibh.print_h);
            printf("  Vertical print res.: %d pixels/m\n", dibh.print_v);
            printf("  No. of palette colors: %d\n", dibh.pal_colors);
            printf("  No. of important colors: %d\n", dibh.imp_colors);
        }

        // if the line of pixels is not aligned to 4-bytes, that means there
        // will be alignment.. we need to consider this when reading pixel data
        PADDING = ((dibh.bpp / 8) * dibh.width) % 4;

        // go to the start of the pixel array data
        // reserve enough memory for the pixel data
        // if height is positive, the image is upside-down and inverted
        bitmap_size = dibh.width * abs(dibh.height / 2);

        if(!(bitmap = (uint32_t *)malloc(sizeof(uint32_t) * bitmap_size)))
        {
            invalid_image("Insufficient memory\n");
        }

#define R(index)        color_table[(int)index].red
#define G(index)        color_table[(int)index].green
#define B(index)        color_table[(int)index].blue
#define A(index)        color_table[(int)index].alpha

        // read in the pixel data
        for(count = 0, i = 0; i < bitmap_size; i++)
        {
            uint8_t r, g, b, a;

            if(count >= dibh.width)
            {
                if(PADDING)
                {
                    // skip the padding
                    char tmp[PADDING];
                    FREAD(&tmp, 1, sizeof(tmp), file);
                }

                count = 0;
            }

            // read pixel colors according to bits-per-pixel
            // 1 bit-per-pixel, that means each byte represents 8 pixels
            if(dibh.bpp == 1)
            {
                uint8_t j, k;
                int l;

                FREAD(&j, 1, sizeof(j), file);

                for(k = 0; k < 8; k++)
                {
                    l = (j & 0x80) ? 1 : 0;
                    bitmap[i] = make_rgba(R(l), G(l), B(l), A(l));
                    i++;
                    j <<= 1;
                }
            }
            // 4 bits-per-pixel, that means each byte represents 2 pixels
            else if(dibh.bpp == 4)
            {
                uint8_t j;
                int l;

                FREAD(&j, 1, sizeof(j), file);
                l = (j >> 4) & 0x0f;
                bitmap[i] = make_rgba(R(l), G(l), B(l), A(l));
                i++;
                l = j & 0x0f;
                bitmap[i] = make_rgba(R(l), G(l), B(l), A(l));
            }
            // 8 bits-per-pixel, that means each byte represents 1 pixel
            else if(dibh.bpp == 8)
            {
                uint8_t j;

                FREAD(&j, 1, sizeof(j), file);
                bitmap[i] = make_rgba(R(j), G(j), B(j), A(j));
            }
            // 16 bits-per-pixel, that means each pixel is represented by 2 bytes
            else if(dibh.bpp == 16)
            {
                uint8_t j, l;
                int k;

                FREAD(&j, 1, sizeof(j), file);
                FREAD(&l, 1, sizeof(l), file);
                k = (l << 8) | j;

#if BYTE_ORDER == BIG_ENDIAN
                k = swap_word(k);
#endif

                r = ((k >> 10) & 0x1f) << 3;
                g = ((k >> 5) & 0x1f) << 3;
                b = (k & 0x1f) << 3;
                bitmap[i] = make_rgba(r, g, b, 255 /* 0 */);
            }
            else if(dibh.bpp == 24)
            {
                FREAD(&b, 1, sizeof(b), file);
                FREAD(&g, 1, sizeof(g), file);
                FREAD(&r, 1, sizeof(r), file);
                bitmap[i] = make_rgba(r, g, b, 255 /* 0 */);
            }
            else if(dibh.bpp == 32)
            {
                uint32_t tmp;

                FREAD(&tmp, sizeof(tmp), 1, file);

#if BYTE_ORDER == BIG_ENDIAN
                tmp = swap_dword(tmp);
#endif

                a = (tmp & bm.alpha) >> alpha_shift;
                r = (tmp & bm.red) >> red_shift;
                g = (tmp & bm.green) >> green_shift;
                b = (tmp & bm.blue) >> blue_shift;
                bitmap[i] = make_rgba(r, g, b, a);
            }

            count++;
        }

#undef R
#undef G
#undef B
#undef A

        //printf("Image size: %d, pixels read in: %d\n", bitmap_size, i);

        if(dibh.bpp == 32)
        {
            goto skip_AND_image;
        }
      
        // read AND image
        count = 0;

        for(i = 0; i < bitmap_size; i += 8)
        {
            char tmp;
            int j;

            if(count >= dibh.width)
            {
                if(PADDING)
                {
                    // skip the padding
                    char tmp2[PADDING];
                    FREAD(&tmp2, 1, sizeof(tmp2), file);
                }

                count = 0;
            }

            FREAD(&tmp, 1, 1, file);

            for(j = 0; j < 8; j++)
            {
                if(((tmp >> (7 - j)) & 1))
                {
                    bitmap[i + j] |= 0xff;
                }
                else
                {
                    bitmap[i + j] &= ~0xff;
                }
            }

            count += 8;
        }
      
skip_AND_image:

        res_bitmap->bitmaps[index].data = (uint32_t *)bitmap;
        res_bitmap->bitmaps[index].width = dibh.width;
        res_bitmap->bitmaps[index].height = abs(dibh.height / 2);
        
        // if height is positive, the image is upside-down and inverted
        if(dibh.height > 0)
        {
            invert_image((uint32_t *)bitmap, dibh.width, abs(dibh.height / 2));
        }

        bitmap = NULL;

        if(color_table)
        {
            free((void *)color_table);
            color_table = NULL;
        }
    }

    fclose(file);

fin:
    if(ICONDIRENTRY)
    {
        free(ICONDIRENTRY);
    }
    
    if(color_table)
    {
        free((void *)color_table);
    }

    if(bitmap)
    {
        free((void *)bitmap);
    }
    
    return res_bitmap;
}

