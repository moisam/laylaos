/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: bmp.h
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
 *  \file bmp.h
 *
 *  Declarations and struct definitions for working with BMP images.
 */

#ifndef _BMP_H
#define _BMP_H

//different names of headers for Windows and OS/2 bmp files
typedef enum
{
    BITMAPCOREHEADER    = 12,
    OS21XBITMAPHEADER   = BITMAPCOREHEADER,
    OS22XBITMAPHEADER   = 64,
    BITMAPINFOHEADER    = 40,
    BITMAPV2INFOHEADER  = 52,
    BITMAPV3INFOHEADER  = 56,
    BITMAPV4HEADER      = 108,
    BITMAPV5HEADER      = 124
} bmp_header_name;

//Compression method. See 'comp' in 'dib_header' below
typedef enum
{
    BI_RGB              = 0,        //None
    BI_RLE8             = 1,        //RLE 8bit/pixel
    BI_RLE4             = 2,        //RLE 4bit/pixel
    BI_BITFIELDS        = 3,        //RGB bitfield masks
    BI_JPEG             = 4,        //JPEG image for printing
    BI_PNG              = 5,        //PNG image for printing
    BI_ALPHABITFIELDS   = 6,        //RGBA bitfield masks
    BI_CMYK             = 11,       //Windows Metafile None
    BI_CMYKRLE8         = 12,       //Windows Metafile RLE-8
    BI_CMYKTLE          = 13,       //Windows Metafile RLE-4
} pixel_compression;

//bitmap file header
typedef struct bmp_header_s
{
    int32_t file_size;          //size of the BMP file
    int32_t unused;
    int32_t offset;             //to the pixel array (bmp data)
} bmp_header;

//dib (device-independent bitmap) header
typedef struct dib_header_s
{
    int32_t dib_size;           //size of header
    int32_t width;              //width of bitmap in pixels
    int32_t height;             //height of bitmap in pixels
    int16_t planes;             //No. of color planes
    int16_t bpp;                //bits per pixel
    int32_t comp;               //pixel array compression used
    int32_t data_size;          //size of raw bitmap data
    int32_t print_h;            //horizontal print resolution in pixels/meter
    int32_t print_v;            //vertical print resolution in pixels/meter
    int32_t pal_colors;         //No. of colors in the palette
    int32_t imp_colors;         //No. of important colors
} dib_header;

#endif      /* _BMP_H */
