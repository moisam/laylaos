/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: png.c
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
 *  \file png.c
 *
 *  This file contains a shared function that loads Portable Network Graphics
 *  (*.PNG) image files.
 */

#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../include/gunzip/deflate.h"
#include "../../../include/gunzip/member.h"
#include "../include/bitmap.h"

/****************************************************************
 * The PNG file is constructed of chunks. Some are critical and
 * MUST be present in each file. Some are additional (or
 * ancillary). Those are (adopted from 
 * http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html):
 * 
 *  Critical chunks (order is important, except PLTE is optional):
           Name  Multiple  Ordering constraints
                   OK?
           IHDR    No      Must be first
           PLTE    No      Before IDAT
           IDAT    Yes     Multiple IDATs must be consecutive
           IEND    No      Must be last
   
   Ancillary chunks (need not appear in this order):
           Name  Multiple  Ordering constraints
                   OK?
           cHRM    No      Before PLTE and IDAT
           gAMA    No      Before PLTE and IDAT
           iCCP    No      Before PLTE and IDAT
           sBIT    No      Before PLTE and IDAT
           sRGB    No      Before PLTE and IDAT
           bKGD    No      After PLTE; before IDAT
           hIST    No      After PLTE; before IDAT
           tRNS    No      After PLTE; before IDAT
           pHYs    No      Before IDAT
           sPLT    Yes     Before IDAT
           tIME    No      None
           iTXt    Yes     None
           tEXt    Yes     None
           zTXt    Yes     None
 ****************************************************************/

#define SHOW_INFO       0

//used for Adler checksum calculation
#define BASE 65521 /* largest prime smaller than 65536 */

// defined in png_crc.c
unsigned long calculate_crc32(unsigned char *buf, int len);

//RGB colors for palettes
typedef struct
{
    char red;
    char green;
    char blue;
} pal_rgb;

typedef struct
{
    uint32_t length;
    char type[4];
    //char *data;
    //DWORD crc;
} png_chunk_header;

//the png image header chunk IHDR:
//Width and height: image dimensions in pixels, Zero is invalid.
//
//Bit depth: integer giving the number of bits per sample or per 
//palette index (not per pixel). Valid values: 1, 2, 4, 8, and 16, 
//note that not all values are allowed for all color types.
//
//Color type: integer that describes the interpretation of the image 
//data. Color type codes are sums of the following values: 
//1 (palette used), 
//2 (color used), 
//and 4 (alpha channel used). Valid values are 0, 2, 3, 4, and 6.
//
//  Color    Allowed      Interpretation
//  Type     Bit Depths
//============================================================   
//   0       1,2,4,8,16  Each pixel is a grayscale sample.
//   2       8,16        Each pixel is an R,G,B triple.
//   3       1,2,4,8     Each pixel is a palette index;
//                         a PLTE chunk must appear.
//   4       8,16        Each pixel is a grayscale sample,
//                         followed by an alpha sample.
//   6       8,16        Each pixel is an R,G,B triple,
//                         followed by an alpha sample.
//
//Compression method and Filter method must be zero.
//
//Interlace method is either: 0 (no interlace) or 1 (Adam7 interlace).
//
typedef struct
{
    uint32_t width;        //in pixels
    uint32_t height;        //in pixels
    uint8_t bit_depth;    //1,2,4,8,16
    uint8_t color_type;    //0,2,3,4,6
    uint8_t compression_method;//must be zero
    uint8_t filter_method;    //must be zero
    uint8_t interlace_method;    //0,1
} IHDR;

typedef struct
{
    pal_rgb color[256];
} PLTE;


//if cHRM chunk is defined, it contains information on 'Primary 
//Chromaticities and White Point', all fields are 4-bytes long.
struct
{
    uint32_t white_point_x;
    uint32_t white_point_y;
    uint32_t red_x;
    uint32_t red_y;
    uint32_t green_x;
    uint32_t green_y;
    uint32_t blue_x;
    uint32_t blue_y;
} cHRM;

//if pHYs chunk is defined, it defines Physical Pixel Dimensions.
//The unit specifier can be 0 (unknown) or 1 (meter).
struct
{
    uint32_t pixels_per_unit_x;
    uint32_t pixels_per_unit_y;
    char unit_spec;
} pHYs;

//If defined, tIME defines Image Last-Modification Time.
struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} tIME;

static char png_sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };

/* Values for Adam7 interlace */
char png_row_start[7]     = { 0, 0, 4, 0, 2, 0, 1 };
char png_row_increment[7] = { 8, 8, 8, 4, 4, 2, 2 };
char png_col_start[7]     = { 0, 4, 0, 2, 0, 1, 0 };
char png_col_increment[7] = { 8, 8, 4, 4, 2, 2, 1 };


int check_crc(FILE *file, png_chunk_header *hdr, long h2, uint32_t crc)
{
    unsigned long crc2;

    if(hdr->length < 256)
    {
        unsigned char crc_data[hdr->length + 4];

        fseek(file, h2, SEEK_SET);
        fread(&crc_data, hdr->length + 4, 1, file);
        fseek(file, sizeof(crc), SEEK_CUR);

        crc2 = calculate_crc32(crc_data, hdr->length + 4);
    }
    else
    {
        unsigned char *crc_data;
        
        if(!(crc_data = malloc(hdr->length + 4)))
        {
            return 0;
        }

        fseek(file, h2, SEEK_SET);
        fread(crc_data, hdr->length + 4, 1, file);
        fseek(file, sizeof(crc), SEEK_CUR);

        crc2 = calculate_crc32(crc_data, hdr->length + 4);
        free(crc_data);
    }
    
    return (crc2 == crc);
}


/*
 * Paeth predictor function used in applying filter method 4
 * to the decoded image data, as defined in the PNG specification.
 */
unsigned char Paeth_predictor(unsigned char a, unsigned char b, unsigned char c)
{
	//a = left, b = above, c = upper left
	short p = a + b - c;	//initial estimate
	short pa = abs(p - a);	//distances to a, b, c
	short pb = abs(p - b);
	short pc = abs(p - c);

	//return nearest of a,b,c,
	//breaking ties in order a,b,c.
	if(pa <= pb && pa <= pc)
	{
	    return a;
	}

	if(pb <= pc)
	{
	    return b;
	}

	return c;
}	 


#define invalid_image(msg)                  \
{                                           \
    fprintf(stderr, msg);                   \
    goto fin;                               \
}


struct bitmap32_t *png_load(char *file_name, struct bitmap32_t *loaded_bitmap)
{
    FILE *file;
    struct bitmap32_t *res_bitmap = NULL;

    if((file = fopen(file_name, "rb")) == NULL)
    {
        fprintf(stderr, "Error opening '%s': %s\n", file_name, strerror(errno));
        return NULL;
    }
    
    res_bitmap = png_load_file(file, loaded_bitmap);
    fclose(file);

    return res_bitmap;
}


#if BYTE_ORDER == LITTLE_ENDIAN
#define CHECK_HDR_CRC()                 \
    fread(&crc, sizeof(crc), 1, file);  \
    crc = swap_dword(crc);              \
    if(!check_crc(file, &hdr, h2, crc)) \
        invalid_image("Error: Bad CRC for image hdr\n");
#else
#define CHECK_HDR_CRC()                 \
    fread(&crc, sizeof(crc), 1, file);  \
    if(!check_crc(file, &hdr, h2, crc)) \
        invalid_image("Error: Bad CRC for image hdr\n");
#endif


struct bitmap32_t *png_load_file(FILE *file, struct bitmap32_t *loaded_bitmap)
{
    char x[8] = { 0, };
    long h;
    struct bitmap32_t *res_bitmap = NULL;

    //if bKGD chunk is defined, the background color can be:
    //1 byte: index into palette (if color type 3)
    //2 bytes: grayscale color (if color type 0,4)
    //6 bytes: RGB color (if color type 2,6)
    char bKGD_type_3 = 0;
    uint16_t bKGD_type_0_4 = 0;
    struct { uint16_t red; uint16_t green; uint16_t blue; } bKGD_type_2_6 = { 0, };

    //if gAMA chunk is defined, it contains the gamma correction, which
    //is a 4-byte data, that should be divided by 100000 to give the gamma.
    uint32_t gAMA = 0;

    //if hIST chunk is defined, it gives the approximate usage
    //frequency of each color in the color palette.
    //It must appear with a palette chunk.
    //It contains a series of 2-byte values, one for each entry in the
    //PLTE chunk
    //uint16_t hIST[256];
    uint16_t *hIST = NULL;

    //if sBIT chunk is defined, it defines significant bits.
    // For color type 0 (grayscale), it contains a single byte
    // For color type 2 (RGB truecolor), it contains three bytes (RGB)
    // For color type 3 (palette color), it contains three bytes (RGB)
    // For color type 4 (grayscale with alpha channel), 2 bytes
    // For color type 6 (RGB truecolor with alpha channel), 4  bytes (RGBA)
    char sBIT_type_0 = 0;
    pal_rgb sBIT_type_2_3 = { 0, };
    struct { char gray; char alpha; } sBIT_type_4 = { 0, };
    rgb sBIT_type_6 = { 0, };

    //if tEXt is defined, it provides textual information about the image.
    //it contains a KEYWORD, a NULL separator, and a TEXT string.
    char *image_text = NULL;
    long image_text_len = 0;

    //if defined, tRNS defines the transparency. It is not allowed for
    //color types 4 and 6, as they have a full alpha channel.
    //char tRNS_type_3[256];
    char *tRNS_type_3 = NULL;
    uint16_t tRNS_type_0 = 0;
    struct { uint16_t red; uint16_t green; uint16_t blue; } tRNS_type_2 = { 0, };

    //boolean to indicate whether transparency data is defined
    char TRANSPARENCY = 0;

    char *deflate_data_in = NULL;
    uint32_t byte_pos = 0;
    uint32_t bit_pos = 0;
    char *output_stream = NULL;
    long output_len = 0;
    uint32_t output_pos = 0;

    uint8_t *output_adam7 = NULL;
    uint8_t *previous = NULL;

    png_chunk_header hdr;
    IHDR png_header;
    PLTE palette;
    int chunk_number = 0;
    uint8_t PALETTE_DEFINED = 0;
    uint8_t DATA_STARTED = 0;
    uint8_t *raw_data;
    int total_data_chunks = 0;
    long data_length = 0;
    uint32_t crc;
    int8_t *tEXt = NULL;

    //read in the png file header    
    fread(&x, 1, sizeof(x), file);

    //check if the file signature is correct
    for(h = 0; h < 8; h++)
    {
        if(x[h] != png_sig[h])
        {
            fprintf(stderr, "Error: not a valid png file\n");
            fprintf(stderr,
                    "       file signature: %02x%02x%02x%02x%02x%02x%02x%02x\n",
                    x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]);
            fprintf(stderr,
                    "       should be     : %02x%02x%02x%02x%02x%02x%02x%02x\n",
                    png_sig[0], png_sig[1], png_sig[2], png_sig[3], 
                    png_sig[4], png_sig[5], png_sig[6], png_sig[7]);
            return NULL;
        }
    }

    //start reading file chunks
    TRANSPARENCY = 0;
    image_text_len = 0;

    if(SHOW_INFO)
    {
        printf("Your system is %s-endian.\n",
#if BYTE_ORDER == LITTLE_ENDIAN
                                        "little"
#else
                                        "big"
#endif
              );
    }

    while((h = fread(&hdr, sizeof(hdr), 1, file)))
    {
        chunk_number++;
        long h2 = ftell(file)-4;

#if BYTE_ORDER == LITTLE_ENDIAN
        hdr.length = swap_dword(hdr.length);
#endif

        /////////////////////////////
        // header chunk (CRITICAL)
        /////////////////////////////
        if(memcmp(hdr.type, "IHDR", 4) == 0)
        {
            if(chunk_number != 1)
            {
                // IHDR chunk should be the first chunk
                invalid_image("File is corrupt: header is misplaced.\n");
            }

            if(hdr.length != 13)    // png IHDR is fixed at 13 bytes
            {
                invalid_image("The image header is corrupt: invalid length.\n");
            }

            if(hdr.length)
            {
                fread(&png_header, hdr.length, 1, file);
            }

            CHECK_HDR_CRC();

#if BYTE_ORDER == LITTLE_ENDIAN
            png_header.width = swap_dword(png_header.width);
            png_header.height = swap_dword(png_header.height);
#endif

            if(png_header.compression_method != 0 ||
               png_header.filter_method != 0 ||
               png_header.interlace_method > 1)
            {
                invalid_image("Image header data is corrupt.\n");
            }
        
            if(SHOW_INFO)
            {
                printf("Image width: %d\n", png_header.width);
                printf("Image height: %d\n", png_header.height);
                printf("Bit depth: %d\n", png_header.bit_depth);
                printf("Color type: %d\n", png_header.color_type);
                printf("Compression method: %d\n", png_header.compression_method);
                printf("Filter method: %d\n", png_header.filter_method);
                printf("Interlace method: %d\n", png_header.interlace_method);
                printf("==============================\n");
                printf("%d. Image header chunk:\n", chunk_number);
                //printf("==========================\n");
                //printf("Chunk size: %u\n", hdr.length);
                //printf("Chunk type: IHDR\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        /////////////////////////////
        // palette chunk (CRITICAL)
        /////////////////////////////
        else if(memcmp(hdr.type, "PLTE", 4) == 0)
        {
            // palette should be present before any image data
            if(DATA_STARTED)
            {
                invalid_image("File is corrupt: palette data is misplaced.\n");
            }

            if(hdr.length % 3)  // if not divisble by 3, it's an error
            {
                invalid_image("The palette data is corrupt.\n");
            }

            if(hdr.length)
            {
                fread(&palette, hdr.length, 1, file);
            }

            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Image palette chunk:\n", chunk_number);
                //printf("===========================\n");
                //printf("Chunk size: %u\n", hdr.length);
                //printf("Chunk type: PLTE\n");
                printf("   Chunk CRC: %u\n", crc);
            }

            PALETTE_DEFINED = 1;
        }
        /////////////////////////////
        // data chunk(s) (CRITICAL)
        /////////////////////////////
        else if(memcmp(hdr.type, "IDAT", 4) == 0)
        {
            if(hdr.length)
            {
                if(!deflate_data_in)
                {
                    deflate_data_in = (char *)malloc(hdr.length);
                }
                else
                {
                    deflate_data_in = (char *)realloc(deflate_data_in,
                                                    data_length + hdr.length);
                }

                raw_data = (uint8_t *)malloc(hdr.length);
                fread(raw_data, hdr.length, 1, file);
                memcpy(deflate_data_in + data_length, raw_data, hdr.length);
                //A_memcpy(deflate_data_in + data_length, raw_data, hdr.length);
                free(raw_data);
                total_data_chunks++;
                data_length += hdr.length;
            }

            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Image data chunk:\n", chunk_number);
                //printf("========================\n");
                printf("   Chunk size: %u\n", hdr.length);
                //printf("Chunk type: IDAT\n");
                printf("   Chunk CRC: %u\n", crc);
            }

            DATA_STARTED = 1;
        }
        /////////////////////////////
        // end chunk (CRITICAL)
        /////////////////////////////
        else if(memcmp(hdr.type, "IEND", 4) == 0)
        {
            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Image end chunk:\n", chunk_number);
                //printf("=======================\n");
                //printf("Chunk size: %u\n", hdr.length);
                //printf("Chunk type: IEND\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        ///////////////////////////////////////////
        // bKGD chunk: Background color (ANCILLARY)
        ///////////////////////////////////////////
        else if(memcmp(hdr.type, "bKGD", 4) == 0)
        {
            if(DATA_STARTED)
            {
                invalid_image("File is corrupt: Background color data is misplaced.\n");
            }

            if(png_header.color_type == 3)
            {
                fread(&bKGD_type_3, hdr.length, 1, file);
            }
            else if(png_header.color_type == 0 ||
                    png_header.color_type == 4)
            {
                fread(&bKGD_type_0_4, hdr.length, 1, file);

#if BYTE_ORDER == LITTLE_ENDIAN
                bKGD_type_0_4 = swap_word(bKGD_type_0_4);
#endif
            }
            else if(png_header.color_type == 2 ||
                    png_header.color_type == 6)
            {
                fread(&bKGD_type_2_6, hdr.length, 1, file);

#if BYTE_ORDER == LITTLE_ENDIAN
                bKGD_type_2_6.blue = swap_word(bKGD_type_2_6.blue);
                bKGD_type_2_6.green = swap_word(bKGD_type_2_6.green);
                bKGD_type_2_6.red = swap_word(bKGD_type_2_6.red);
#endif
            }
            else
            {
                fread(&bKGD_type_2_6, hdr.length, 1, file);
            }

            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Background color chunk:\n", chunk_number);
                //printf("==============================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        ////////////////////////////////////////////////////////////////
        // cHRM chunk: Primary Chromaticities and White Point (ANCILLARY)
        ////////////////////////////////////////////////////////////////
        else if(memcmp(hdr.type, "cHRM", 4) == 0)
        {
            if(DATA_STARTED || PALETTE_DEFINED)
            {
                invalid_image("File is corrupt: Chromatics data is misplaced.\n");
            }

            fread(&cHRM, hdr.length, 1, file);

            CHECK_HDR_CRC();

#if BYTE_ORDER == LITTLE_ENDIAN
            cHRM.blue_x = swap_dword(cHRM.blue_x);
            cHRM.blue_y = swap_dword(cHRM.blue_y);
            cHRM.green_x = swap_dword(cHRM.green_x);
            cHRM.green_y = swap_dword(cHRM.green_y);
            cHRM.red_x = swap_dword(cHRM.red_x);
            cHRM.red_y = swap_dword(cHRM.red_y);
            cHRM.white_point_x = swap_dword(cHRM.white_point_x);
            cHRM.white_point_y = swap_dword(cHRM.white_point_y);
#endif

            if(SHOW_INFO)
            {
                printf("%d. Chromatics chunk:\n", chunk_number);
                //printf("========================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        ///////////////////////////////////////////
        // gAMA chunk: Gamma Correction (ANCILLARY)
        ///////////////////////////////////////////
        else if(memcmp(hdr.type, "gAMA", 4) == 0)
        {
            if(DATA_STARTED || PALETTE_DEFINED)
            {
                invalid_image("File is corrupt: Gamma Correction data is misplaced.\n");
            }

            fread(&gAMA, hdr.length, 1, file);

            CHECK_HDR_CRC();

#if BYTE_ORDER == LITTLE_ENDIAN
            gAMA = swap_dword(gAMA);
#endif

            if(SHOW_INFO)
            {
                printf("%d. Gamma correction chunk:\n", chunk_number);
                //printf("===============================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        //////////////////////////////////////////
        // hIST chunk: Image Histogram (ANCILLARY)
        //////////////////////////////////////////
        else if(memcmp(hdr.type, "hIST", 4) == 0)
        {
            if(DATA_STARTED || !PALETTE_DEFINED)
            {
                invalid_image("File is corrupt: Histogram data is misplaced,"
                              " or the palette data is missing.\n");
            }

            hIST = malloc(256 * sizeof(uint16_t));
            fread(&hIST, hdr.length, 1, file);

            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Image histogram chunk:\n", chunk_number);
                //printf("==============================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        ////////////////////////////////////////////////////
        // pHYs chunk: Physical Pixel Dimensions (ANCILLARY)
        ////////////////////////////////////////////////////
        else if(memcmp(hdr.type, "pHYs", 4) == 0)
        {
            if(DATA_STARTED)
            {
                invalid_image("File is corrupt: Physical Pixel Dimensions data "
                              "is misplaced.\n");
            }

            fread(&pHYs, hdr.length, 1, file);

            CHECK_HDR_CRC();

#if BYTE_ORDER == LITTLE_ENDIAN
            pHYs.pixels_per_unit_x = swap_dword(pHYs.pixels_per_unit_x);
            pHYs.pixels_per_unit_y = swap_dword(pHYs.pixels_per_unit_y);
#endif

            if(SHOW_INFO)
            {
                printf("%d. Physical Pixel Dimensions chunk:\n", chunk_number);
                //printf("=======================================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        ///////////////////////////////////////////
        // sBIT chunk: Significant bits (ANCILLARY)
        ///////////////////////////////////////////
        else if(memcmp(hdr.type, "sBIT", 4) == 0)
        {
            if(DATA_STARTED || PALETTE_DEFINED)
            {
                invalid_image("File is corrupt: Significant bits data are misplaced.\n");
            }

            if(png_header.color_type == 0)
            {
                fread(&sBIT_type_0, hdr.length, 1, file);
            }
            else if(png_header.color_type == 2 ||
                    png_header.color_type == 3)
            {
                fread(&sBIT_type_2_3, hdr.length, 1, file);
            }
            else if(png_header.color_type == 4)
            {
                fread(&sBIT_type_4, hdr.length, 1, file);
            }
            else
            {
                fread(&sBIT_type_6, hdr.length, 1, file);
            }

            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Significant bits chunk:\n", chunk_number);
                //printf("===============================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        ////////////////////////////////////////
        // tEXt chunk: Textual data (ANCILLARY)
        ////////////////////////////////////////
        else if(memcmp(hdr.type, "tEXt", 4) == 0)
        {
            if(hdr.length)
            {
                //h = fread(&tEXt, hdr.length, 1, file);
                if(!image_text)
                {
                    image_text = (char *)malloc(hdr.length + 2);
                }
                else
                {
                    image_text = (char *)realloc(image_text,
                                            image_text_len+hdr.length + 2);
                }

                fread(image_text + image_text_len, hdr.length, 1, file);

                //search for the null separator that is found between
                //the keyword and the text.
                int i, zero = -1;

                for(i = 0; i < hdr.length; i++)
                {
                    if(image_text[image_text_len+i] == '\0')
                    {
                        if(i == 0 || i > 79)
                        {
                            invalid_image("Error: Invalid text data.\n");
                        }
                        else
                        {
                            zero = i;
                            break;
                        }
                    }
                }

                if(zero == -1)
                {
                    invalid_image("Error: Invalid text data.\n");
                }

                image_text[image_text_len + hdr.length + 1] = '\n';
                image_text[image_text_len + hdr.length + 2] = '\0';
                image_text_len += hdr.length + 1;
            }

            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Textual data chunk:\n", chunk_number);
                //printf("===========================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        ///////////////////////////////////////////////////////
        // tIME chunk: Image Last-Modification Time (ANCILLARY)
        ///////////////////////////////////////////////////////
        else if(memcmp(hdr.type, "tIME", 4) == 0)
        {
            if(hdr.length)
            {
                fread(&tIME, hdr.length, 1, file);
            }

            CHECK_HDR_CRC();

#if BYTE_ORDER == LITTLE_ENDIAN
            tIME.year = swap_word(tIME.year);
#endif

            if(SHOW_INFO)
            {
                printf("%d. Image Last-Modification Time chunk:\n", chunk_number);
                //printf("==========================================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        ///////////////////////////////////////////
        // tRNS chunk: Transparency (ANCILLARY)
        ///////////////////////////////////////////
        else if(memcmp(hdr.type, "tRNS", 4) == 0)
        {
            if(DATA_STARTED)
            {
                invalid_image("File is corrupt: Transparency data is misplaced.\n");
            }

            if(png_header.color_type == 3)
            {
                tRNS_type_3 = malloc(256);

                // read transparency data
                fread(&tRNS_type_3, hdr.length, 1, file);

                if(hdr.length < 256)
                {
                    // initialize the remaining transparency channels to opaque
                    int i;

                    for(i = hdr.length; i < 256; i++)
                    {
                        tRNS_type_3[i] = 255;
                    }
                }
            }
            else if(png_header.color_type == 0)
            {
                fread(&tRNS_type_0, hdr.length, 1, file);

#if BYTE_ORDER == LITTLE_ENDIAN
                tRNS_type_0 = swap_word(tRNS_type_0);
#endif
            }
            else if(png_header.color_type == 2)
            {
                fread(&tRNS_type_2, hdr.length, 1, file);

#if BYTE_ORDER == LITTLE_ENDIAN
                tRNS_type_2.red = swap_word(tRNS_type_2.red);
                tRNS_type_2.green = swap_word(tRNS_type_2.green);
                tRNS_type_2.blue = swap_word(tRNS_type_2.blue);
#endif
            }
            else
            {
                // should be no transparency data for levels 4, 6
                invalid_image("Error in image: unexpected transparency data.\n");
            }

            CHECK_HDR_CRC();

            TRANSPARENCY = 1;

            if(SHOW_INFO)
            {
                printf("%d. Transparency chunk:\n", chunk_number);
                //printf("===========================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        //////////////////////////////////////////////////
        // zTXt chunk: Compressed Textual Data (ANCILLARY)
        //////////////////////////////////////////////////
        else if(memcmp(hdr.type, "zTXt", 4) == 0)
        {
            if(hdr.length)
            {
                tEXt = malloc(4096);
                fread(&tEXt, hdr.length, 1, file);
            }

            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Compressed Textual Data chunk:\n", chunk_number);
                //printf("=====================================\n");
                printf("   Chunk CRC: %u\n", crc);
            }
        }
        //////////////////////////////////
        // Non-standard Ancillary chunk(s)
        //////////////////////////////////
        else
        {
            if(hdr.length)
            {
                /*
                raw_data = (int8_t *)malloc(hdr.length);
                fread(raw_data, hdr.length, 1, file);
                free(raw_data);
                */
                fseek(file, hdr.length, SEEK_CUR);
            }

            CHECK_HDR_CRC();

            if(SHOW_INFO)
            {
                printf("%d. Non-standard Ancillary chunk:\n", chunk_number);
                //printf("=====================================\n");
                printf("   Chunk size: %u\n", hdr.length);
                printf("   Chunk type: %c%c%c%c\n", hdr.type[0], hdr.type[1]
                                                  , hdr.type[2], hdr.type[3]);
                printf("   Chunk CRC: %u\n", crc);
            }
        }
    }

    // in color type 3 png images, palette is a must
    if(png_header.color_type == 3 && !PALETTE_DEFINED)
    {
        invalid_image("Error: missing palette data.\n");
    }

    // check the bit depth is correct for the color type
    uint8_t bad_colors = 0;

    switch(png_header.color_type)
    {
        case 0:    /*grey colors*/
            if(png_header.bit_depth != 1 &&
               png_header.bit_depth != 2 &&
               png_header.bit_depth != 4 &&
               png_header.bit_depth != 8 &&
               png_header.bit_depth != 16)
            {
                bad_colors = 1;
            }

            break;

        case 2:    /*RGB colors*/
            if(png_header.bit_depth != 8 && png_header.bit_depth != 16)
            {
                bad_colors = 1;
            } 

            break; 

        case 3:     /*palette indexed*/
            if(png_header.bit_depth != 1 &&
               png_header.bit_depth != 2 &&
               png_header.bit_depth != 4 &&
               png_header.bit_depth != 8)
            {
                bad_colors = 1;
            }

            break; 

        case 4:     /*grey/alpha*/
            if(png_header.bit_depth != 8 && png_header.bit_depth != 16)
            {
                bad_colors = 1;
            }

            break; 

        case 6:     /*RGBA*/
            if(png_header.bit_depth != 8 && png_header.bit_depth != 16)
            {
                bad_colors = 1;
            }

            break; 

        default:
            bad_colors = 1;
            break; 
    }

    if(bad_colors)
    {
        invalid_image("Error: Invalid bit depth for the specified "
                      "color type.\n");
    }
    
    if(SHOW_INFO)
    {
        if(image_text_len)
        {
            printf("%s", image_text);
        }
    }


    //////////////////////////////////////////////////
    //First step: Use 'Deflate' decompression method
    //to decompress the data
    //////////////////////////////////////////////////
    //construct the data stream
    int bpp;    //bytes per pixel

    if(png_header.bit_depth < 8)
    {
        bpp = 1;
    }
    else
    {
        bpp = png_header.bit_depth / 8;
    }
    
    if(png_header.color_type == 2)
    {
        bpp *= 3;    //RGB triple
    }
    else if(png_header.color_type == 4)
    {
        bpp *= 2;    //Gray/Alpha
    }
    else if(png_header.color_type == 6)
    {
        bpp *= 4;    //RGB/Alpha
    }
    
    output_len = png_header.height * png_header.width * bpp;
    output_len += png_header.height;    // 1 extra byte per scanline for 
                                        // the filtering method
    output_pos = 0;

    if(!(output_stream = (char *)malloc(output_len)))
    {
        invalid_image("Insufficient memory\n");
    }
        
    int i = 0, j = 0, k = 0;

    //first byte of block contains Compression Method 
    //(and info) and flags
    uint8_t CMF = deflate_data_in[i];
    uint8_t CM = CMF & 0x0f;

    //8 is the deflate method, the only method used by png
    if(CM != 8)
    {
        invalid_image("Error: Invalid deflate method.\n");
    }

    uint8_t CINFO = (CMF & 0xf0) >> 4;

    //7 is the maximum window size
    if(CINFO > 7)
    {
        invalid_image("Error: Invalid window size.\n");
    }

    i++;

    uint8_t FLG = deflate_data_in[i];

    if((CMF * 256 + FLG) % 31)
    {
        printf("((%d * 256 + %d) %% 31) = %d\n", CMF, FLG, ((CMF * 256 + FLG) % 31));
        invalid_image("Error: Invalid compression flags.\n");
    }

    uint8_t FDICT = (FLG & 0x20) >> 4;        //bit 5
    //char FLEVEL = (FLG & 0xc0) >> 4;    //bits 6,7
    //char DICT[4];
    int res;

    i++;

    if(FDICT)
    {
        // the standard dictates no predefined dictionary
        invalid_image("Error: Invalid inclusion of a predefined dictionary.\n");
        /*DICT[0] = deflate_data_in[i++];
        DICT[1] = deflate_data_in[i++];
        DICT[2] = deflate_data_in[i++];
        DICT[3] = deflate_data_in[i++];*/
    }

    /////////////////////////////////////////////
    // then we go to the Compressed data itself
    /////////////////////////////////////////////
    byte_pos = i;
    bit_pos = 0;

    //fprintf(stderr, "Data length (%d)\n", data_length);

    /* deflate! */
    if((res = deflate_in_memory(deflate_data_in + i, data_length - i,
                                &bit_pos, &byte_pos,
                                output_stream, output_len, &output_pos))
                                    != GZIP_VALID_ARCHIVE)
    {
        //free(output_stream);
        goto fin;
        return NULL;
    }

    ///////////////////////////////////////////////////
    // Finished reading data. Check the Adler checksum,
    // as per the RFC 1950 specification, see:
    // https://www.ietf.org/rfc/rfc1950.txt
    ///////////////////////////////////////////////////
    if(bit_pos)
    {
        byte_pos++;     // skip extra bits
    }

    unsigned char *os = (unsigned char *)output_stream;
    unsigned char *is = (unsigned char *)deflate_data_in;
    uint32_t bp = byte_pos + i;
    unsigned long adler = 1L;
    unsigned long original_adler = ((unsigned long)is[bp+0] << 24) |
                                   ((unsigned long)is[bp+1] << 16) |
                                   ((unsigned long)is[bp+2] <<  8) |
                                   ((unsigned long)is[bp+3]);
    unsigned long s1 = adler & 0xffff;
    unsigned long s2 = (adler >> 16) & 0xffff;
    int n;

    for(n = 0; n < output_pos; n++)
    {
        s1 = (s1 + os[n]) % BASE;
        s2 = (s2 + s1)    % BASE;
    }

    adler = (s2 << 16) + s1;

    if(SHOW_INFO)
    {
        fprintf(stderr, "Output size: %d (actual length %ld)\n", output_pos, output_len);
    }

    if(adler != original_adler)
    {
        invalid_image("Error: Invalid CRC.\n");
    }


    //////////////////////////////////////////////////
    //Second step: Apply filter method to the deflated
    //data to get image data.
    //////////////////////////////////////////////////
    uint32_t scanline_len = png_header.width * bpp;
    uint8_t filter_type;
    char first_pass;

    if(!(previous = malloc(scanline_len)))
    {
        invalid_image("Insufficient memory\n");
    }

    //printf("output_len = %ld, output_pos = %d\n", output_len, output_pos);

    if(png_header.interlace_method == 0)
    {
        /*
         * no interlace 
         */
        first_pass = 1;

        for(k = 0; k < scanline_len; k++)
        {
            previous[k] = 0;
        }

        i = 0;

        while(i < output_pos)
        {
            int j;

            filter_type = output_stream[i];
            i++;
            j = 0;

            switch(filter_type)    // Filter method
            {
                case(0):    // None
                    i += scanline_len;
                    //i--;
                    break;

                case(1):    // Sub
                    while(j < scanline_len)
                    {
                        if(j >= bpp)
                        {
                            output_stream[i] += output_stream[i-bpp];
                        }

                        j++;
                        i++;
                    } //i++;
                    break;

                case(2):    // Up
                    while(j < scanline_len)
                    {
                        if(!first_pass)
                        {
                            output_stream[i] += previous[j];
                        }

                        j++;
                        i++;
                    } //i++;
                    break;

                case(3):    // Average
                    while(j < scanline_len)
                    {
                        if(!first_pass)
                        {
                            if(j < bpp)
                            {
                                output_stream[i] = output_stream[i] + previous[j]/2;
                            }
                            else
                            {
                                output_stream[i] = output_stream[i] +
                                        ((output_stream[i-bpp]+previous[j])/2);
                            }
                        }
                        else
                        {
                            if(j >= bpp)
                            {
                                output_stream[i] = output_stream[i] + 
                                                   output_stream[i-bpp]/2;
                            }
                        }

                        j++;
                        i++;
                    } //i++;
                    break;

                case(4):    // Paeth
                    while(j < scanline_len)
                    {
                        if(j < bpp)
                        {
                            output_stream[i] = (unsigned char)output_stream[i] +
                                    Paeth_predictor(0, previous[j], 0);
                        }
                        else
                        {
                            output_stream[i] = (unsigned char)output_stream[i] +
                                    Paeth_predictor(output_stream[i-bpp], 
                                                    previous[j], previous[j-bpp]);
                        }

                        j++;
                        i++;
                    } //i++;
                    break;

                default:
                    printf("Filter %d: ", filter_type);
                    invalid_image("Unknown filter method.\n");
                    break;
            } //end switch

            first_pass = 0;

            int start = i-scanline_len;

            for(k = 0; k < scanline_len; k++)
            {
                previous[k] = output_stream[start+k];
            }
        }//end while
    }
    else
    {
        /* 
         * interlace is 1 == Adam7 interlace 
         */
        //rgb output_adam7[png_header.width * png_header.height];
        int pass;
        int pos = 0;
        i = 0;

        if(!(output_adam7 = (uint8_t *)malloc(output_pos)))
        {
            invalid_image("Insufficient memory\n");
        }

        for(pass = 0; pass < 7; pass++)
        {
            scanline_len = 0;

            for(k = png_col_start[pass]; 
                k < png_header.width; 
                k += png_col_increment[pass])
            {
                scanline_len++;
            }

            scanline_len *= bpp;

            for(k = 0; k < scanline_len; k++)
            {
                previous[k] = 0;
            }

            int j = png_col_start[pass];
            int row = png_row_start[pass];

            first_pass = 1;

            while(row < png_header.height)
            {
                int pj = 0, jpos = 0;

                filter_type = output_stream[i];
                i++;

                switch(filter_type)    // Filter method
                {
                    case(0):    // None
                        for(j = png_col_start[pass]; 
                           j < png_header.width; 
                           j += png_col_increment[pass])
                        {
                            for(k = 0; k < bpp; k++)
                            {
                                output_adam7[pos++] = output_stream[i++];
                            }
                        }
                        break;

                    case(1):    // Sub
                        for(j = png_col_start[pass]; 
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            for(k = 0; k < bpp; k++)
                            {
                                if(jpos >= bpp)
                                {
                                    output_adam7[pos] = output_stream[i] +
                                                        output_adam7[pos-bpp];
                                }
                                else
                                {
                                    output_adam7[pos] = output_stream[i];
                                }

                                pos++;
                                i++;
                            }

                            jpos += bpp;
                        } //i++;
                        break;

                    case(2):    // Up
                        for(j = png_col_start[pass]; 
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            for(k = 0; k < bpp; k++)
                            {
                                if(!first_pass)
                                {
                                    output_adam7[pos++] = output_stream[i] +
                                                          previous[pj];
                                }
                                else
                                {
                                    output_adam7[pos++] = output_stream[i];
                                }

                                i++;
                                pj++;
                            }
                        } //i++;
                        break;

                    case(3):    // Average
                        for(j = png_col_start[pass]; 
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            for(k = 0; k < bpp; k++)
                            {
                                if(!first_pass)
                                {
                                    if(jpos < bpp)
                                    {
                                        output_adam7[pos] = output_stream[i] + 
                                                            previous[pj]/2;
                                    }
                                    else
                                    {
                                        output_adam7[pos] = output_stream[i] +
                                                    ((output_adam7[pos-bpp] +
                                                      previous[pj])/2);
                                    }
                                }
                                else
                                {
                                    if(jpos >= bpp)
                                    {
                                        output_adam7[pos] = output_stream[i] + 
                                                    output_adam7[pos-bpp]/2;
                                    }
                                    else
                                    {
                                        output_adam7[pos] = output_stream[i];
                                    }
                                }

                                i++;
                                pj++;
                                pos++;
                            }

                            jpos += bpp;
                        } //i++;
                        break;

                    case(4):    // Paeth
                        for(j = png_col_start[pass]; 
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            for(k = 0; k < bpp; k++)
                            {
                                if(!first_pass)
                                {
                                    if(jpos < bpp)
                                    {
                                        output_adam7[pos] = output_stream[i] +
                                                            previous[pj];
                                    }
                                    else
                                    {
                                        output_adam7[pos] = output_stream[i] +
                                            Paeth_predictor(output_adam7[pos-bpp],
                                                   previous[pj], previous[pj-bpp]);
                                    }
                                }
                                else
                                {
                                    if(jpos >= bpp)
                                    {
                                        output_adam7[pos] = output_stream[i] +
                                                            output_adam7[pos-bpp];
                                    }
                                    else
                                    {
                                        output_adam7[pos] = output_stream[i];
                                    }
                                }

                                i++;
                                pj++;
                                pos++;
                            }

                            jpos += bpp;
                        } //i++;
                        break;

                    default:
                        printf("Filter %d: ", filter_type);
                        invalid_image("Unknown filter method.\n");
                        break;
                } //end switch

                first_pass = 0;

                int start = pos-scanline_len;

                for(k = 0; k < scanline_len; k++)
                {
                    previous[k] = output_adam7[start+k];
                }

                row += png_row_increment[pass];
            } //end while
        }
    }
        
    //////////////////////////////////////////////////
    //Third step: get pixel values from the decoded,
    //filtered data.
    //////////////////////////////////////////////////
    /* reserve enough memory for the pixel data */
    int bitmap_size = png_header.width * png_header.height;
    uint32_t *bitmap;
    int pos = 0;
    char r, g, b, a;
    int index;

    // we will be compressing from 16 to 8 bits
    // this is the suggested method by the PNG specification
    int MAXINSAMPLE = (2^16)-1;
    int MAXOUTSAMPLE = (2^8)-1;
    
    if(!(bitmap = (uint32_t *)malloc(sizeof(uint32_t) * bitmap_size)))
    {
        invalid_image("Insufficient memory\n");
    }

    if(png_header.interlace_method == 0)
    {
        /*
         * no interlace 
         */
        i = 1;

        while(i < output_pos)
        {
            switch(png_header.color_type)
            {
                //////////////////////////////////////////////////
                case(0):    // gray-scale

#define ALPHA(r, a)                                 \
    if(TRANSPARENCY && (r & tRNS_type_0)) a = 0;    \
    else a = 255;

                    if(png_header.bit_depth == 1)
                    {
                        for(j = i; j < i + (png_header.width / 8); j++)
                        {
                            r = output_stream[j] >> 7;
                            ALPHA(r, a);
                            bitmap[pos] = make_rgba(r, r, r, a);

                            r = (output_stream[j] >> 6) & 0x01;
                            ALPHA(r, a);
                            bitmap[pos + 1] = make_rgba(r, r, r, a);

                            r = (output_stream[j] >> 5) & 0x01;
                            ALPHA(r, a);
                            bitmap[pos + 2] = make_rgba(r, r, r, a);

                            r = (output_stream[j] >> 4) & 0x01;
                            ALPHA(r, a);
                            bitmap[pos + 3] = make_rgba(r, r, r, a);

                            r = (output_stream[j] >> 3) & 0x01;
                            ALPHA(r, a);
                            bitmap[pos + 4] = make_rgba(r, r, r, a);

                            r = (output_stream[j] >> 2) & 0x01;
                            ALPHA(r, a);
                            bitmap[pos + 5] = make_rgba(r, r, r, a);

                            r = (output_stream[j] >> 1) & 0x01;
                            ALPHA(r, a);
                            bitmap[pos + 6] = make_rgba(r, r, r, a);

                            r = (output_stream[j]) & 0x01;
                            ALPHA(r, a);
                            bitmap[pos + 7] = make_rgba(r, r, r, a);

                            pos += 8;
                        }

                        i += (png_header.width / 8) + 1;
                    }
                    else if(png_header.bit_depth == 2)
                    {
                        for(j = i; j < i + (png_header.width / 4); j++)
                        {
                            r = output_stream[j] >> 6;
                            ALPHA(r, a);
                            bitmap[pos] = make_rgba(r, r, r, a);

                            r = (output_stream[j] >> 4) & 0x03;
                            ALPHA(r, a);
                            bitmap[pos + 1] = make_rgba(r, r, r, a);

                            r = (output_stream[j] >> 2) & 0x03;
                            ALPHA(r, a);
                            bitmap[pos + 2] = make_rgba(r, r, r, a);

                            r = (output_stream[j]) & 0x03;
                            ALPHA(r, a);
                            bitmap[pos + 3] = make_rgba(r, r, r, a);

                            pos += 4;
                        }

                        i += (png_header.width / 4) + 1;
                    }
                    else if(png_header.bit_depth == 4)
                    {
                        for(j = i; j < i + (png_header.width / 2); j++)
                        {
                            r = output_stream[j] >> 4;
                            ALPHA(r, a);
                            bitmap[pos] = make_rgba(r, r, r, a);

                            r = (output_stream[j]) & 0x0f;
                            ALPHA(r, a);
                            bitmap[pos + 1] = make_rgba(r, r, r, a);

                            pos += 2;
                        }

                        i += (png_header.width / 2) + 1;
                    }
                    else if(png_header.bit_depth == 8)
                    {
                        for(j = i; j < i + png_header.width; j++)
                        {
                            r = output_stream[j];
                            ALPHA(r, a);
                            bitmap[pos] = make_rgba(r, r, r, a);

                            pos++;
                        }

                        i += png_header.width + 1;
                    }
                    else if(png_header.bit_depth == 16)
                    {
                        for(j = i; j < i + (png_header.width * 2); j += 2)
                        {
                            r = (((int16_t)output_stream[j] * MAXOUTSAMPLE / 
                                                              MAXINSAMPLE));
                            ALPHA((int16_t)output_stream[j], a);
                            bitmap[pos] = make_rgba(r, r, r, a);

                            pos++;
                        }

                        i += (png_header.width * 2) + 1;
                    }
                    else
                    {
                        invalid_image("Error: Invalid bit depth for the specified "
                                      "color type.\n");
                    }

                    break;

#undef ALPHA

                //////////////////////////////////////////////////
                case(2):    // RGB image

#define ALPHA(r, g, b, a)                               \
    if(TRANSPARENCY && (r & tRNS_type_2.red)            \
                    && (g & tRNS_type_2.green)          \
                    && (b & tRNS_type_2.blue)) a = 0;   \
    else a = 255;

                    if(png_header.bit_depth == 8)
                    {
                        for(j = i; j < i + (png_header.width * 3); j += 3)
                        {
                            r = output_stream[j];
                            g = output_stream[j + 1];
                            b = output_stream[j + 2];
                            ALPHA(r, g, b, a);
                            bitmap[pos] = make_rgba(r, g, b, a);

                            pos++;
                        }

                        i += (png_header.width * 3) + 1;
                    }
                    else if(png_header.bit_depth == 16)
                    {
                        for(j = i; j < i+(png_header.width*6); j+=6)
                        {
                            r = (((int16_t)output_stream[j] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            g = (((int16_t)output_stream[j+1] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            b = (((int16_t)output_stream[j+2] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            ALPHA((int16_t)output_stream[j],
                                  (int16_t)output_stream[j+1],
                                  (int16_t)output_stream[j+2], a);
                            bitmap[pos] = make_rgba(r, g, b, a);

                            pos++;
                        }

                        i += (png_header.width * 6) + 1;
                    }
                    else
                    {
                        invalid_image("Error: Invalid bit depth for the specified "
                                      "color type.\n");
                    }

                    break;

#undef ALPHA

                //////////////////////////////////////////////////
                case(3):    // Palette-indexed image
                    if(png_header.bit_depth == 1)
                    {

#define R(index)        palette.color[index].red
#define G(index)        palette.color[index].green
#define B(index)        palette.color[index].blue
#define A(index)        tRNS_type_3[index]

                        for(j = i; j < i + (png_header.width / 8); j++)
                        {
                            index = (output_stream[j] >> 7) & 0x01;
                            bitmap[pos] = make_rgba(R(index), G(index), 
                                                    B(index), A(index));

                            index = (output_stream[j] >> 6) & 0x01;
                            bitmap[pos + 1] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            index = (output_stream[j] >> 5) & 0x01;
                            bitmap[pos + 2] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            index = (output_stream[j] >> 4) & 0x01;
                            bitmap[pos + 3] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            index = (output_stream[j] >> 3) & 0x01;
                            bitmap[pos + 4] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            index = (output_stream[j] >> 2) & 0x01;
                            bitmap[pos + 5] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            index = (output_stream[j] >> 1) & 0x01;
                            bitmap[pos + 6] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            index = (output_stream[j]) & 0x01;
                            bitmap[pos + 7] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            pos += 8;
                        }

                        i += (png_header.width / 8) + 1;
                    }
                    else if(png_header.bit_depth == 2)
                    {
                        for(j = i; j < i + (png_header.width / 4); j++)
                        {
                            index = (output_stream[j] >> 6) & 0x03;
                            bitmap[pos] = make_rgba(R(index), G(index), 
                                                    B(index), A(index));

                            index = (output_stream[j] >> 4) & 0x03;
                            bitmap[pos + 1] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            index = (output_stream[j] >> 2) & 0x03;
                            bitmap[pos + 2] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            index = (output_stream[j]) & 0x03;
                            bitmap[pos + 3] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            pos += 4;
                        }

                        i += (png_header.width / 4) + 1;
                    }
                    else if(png_header.bit_depth == 4)
                    {
                        for(j = i; j < i + (png_header.width / 2); j++)
                        {
                            index = output_stream[j] >> 4;
                            bitmap[pos] = make_rgba(R(index), G(index), 
                                                    B(index), A(index));

                            index = output_stream[j] & 0x0f;
                            bitmap[pos + 1] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            pos += 2;
                        }

                        i += (png_header.width / 2) + 1;
                    }
                    else if(png_header.bit_depth == 8)
                    {
                        for(j = i; j < i + png_header.width; j++)
                        {
                            index = output_stream[j];
                            bitmap[pos] = make_rgba(R(index), G(index), 
                                                    B(index), A(index));

                            pos++;
                        }

                        i += png_header.width + 1;
                    }
                    else
                    {
                        invalid_image("Error: Invalid bit depth for the specified "
                                      "color type.\n");
                    }

                    break;

#undef R
#undef G
#undef B
#undef A

                //////////////////////////////////////////////////
                case(4):    // Grayscale/Alpha
                    if(png_header.bit_depth == 8)
                    {
                        for(j = i; j < i + (png_header.width * 2); j += 2)
                        {
                            r = output_stream[j];
                            a = output_stream[j + 1];
                            bitmap[pos] = make_rgba(r, r, r, a);

                            pos++;
                        }

                        i += (png_header.width * 2) + 1;
                    }
                    else if(png_header.bit_depth == 16)
                    {
                        for(j = i; j < i + (png_header.width * 4); j += 4)
                        {
                            r = (int16_t)output_stream[j] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE;
                            a = (((int16_t)output_stream[j + 2] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                            bitmap[pos] = make_rgba(r, r, r, a);

                            pos++;
                        }

                        i += (png_header.width * 4) + 1;
                    }
                    else
                    {
                        invalid_image("Error: Invalid bit depth for the specified "
                                      "color type.\n");
                    }

                    break;

                //////////////////////////////////////////////////
                case(6):    // RGB/Alpha
                    if(png_header.bit_depth == 8)
                    {
                        for(j = i; j < i + (png_header.width * 4); j += 4)
                        {
                            r = output_stream[j];
                            g = output_stream[j + 1];
                            b = output_stream[j + 2];
                            a = output_stream[j + 3];
                            bitmap[pos] = make_rgba(r, g, b, a);

                            pos++;
                        }

                        i += (png_header.width * 4) + 1;
                    }
                    else if(png_header.bit_depth == 16)
                    {
                        for(j = i; j < i + (png_header.width * 8); j += 8)
                        {
                            r = (((int16_t)output_stream[j] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                            g = (((int16_t)output_stream[j + 1] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                            b = (((int16_t)output_stream[j + 2] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                            a = (((int16_t)output_stream[j + 3] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                            bitmap[pos] = make_rgba(r, g, b, a);

                            pos++;
                        }

                        i += (png_header.width * 8) + 1;
                    }
                    else
                    {
                        invalid_image("Error: Invalid bit depth for the specified "
                                      "color type.\n");
                    }

                    break;

                default:
                    invalid_image("Error: Invalid color type.\n");
                    break;
            } //end switch
        } //end while
    }
    else
    {
        /*
         * Adam7 interlace
         */
        int pass;
        int max = png_header.height * png_header.width;

        i = 0;

        for(pass = 0; pass < 7; pass++)
        {
            pos = png_row_start[pass]*png_header.width;

            while(pos < max)
            {
                switch(png_header.color_type)
                {
                    //////////////////////////////////////////////////
                    case(0):    // gray-scale

#define ALPHA(r, a)                                 \
    if(TRANSPARENCY && (r & tRNS_type_0)) a = 0;    \
    else a = 255;

                        if(png_header.bit_depth == 1)
                        {
                            static int shift = 7;

                            for(j = png_col_start[pass];
                                j < png_header.width; 
                                j += png_col_increment[pass])
                            {
                                r = (output_adam7[i] >> shift) & 0x01;
                                ALPHA(r, a);
                                bitmap[pos + j] = make_rgba(r, r, r, a);

                                shift--;

                                if(shift < 0)
                                {
                                    shift = 7;
                                    i++;
                                }
                            }

                            pos += (png_row_increment[pass] * png_header.width);
                        }
                        else if(png_header.bit_depth == 2)
                        {
                            static int shift = 6;

                            for(j = png_col_start[pass];
                                j < png_header.width; 
                                j += png_col_increment[pass])
                            {
                                r = (output_adam7[i] >> shift) & 0x03;
                                ALPHA(r, a);
                                bitmap[pos + j] = make_rgba(r, r, r, a);

                                shift -= 2;

                                if(shift < 0)
                                {
                                    shift = 6;
                                    i++;
                                }
                            }

                            pos += (png_row_increment[pass] * png_header.width);
                        }
                        else if(png_header.bit_depth == 4)
                        {
                            static int shift = 4;

                            for(j = png_col_start[pass];
                                j < png_header.width; 
                                j += png_col_increment[pass])
                            {
                                r = (output_adam7[i] >> shift) & 0x0f;
                                ALPHA(r, a);
                                bitmap[pos + j] = make_rgba(r, r, r, a);

                                shift -= 4;

                                if(shift < 0)
                                {
                                    shift = 4;
                                    i++;
                                }
                            }

                            pos += (png_row_increment[pass] * png_header.width);
                        }
                        else if(png_header.bit_depth == 8)
                        {
                            for(j = png_col_start[pass];
                                j < png_header.width; 
                                j += png_col_increment[pass])
                            {
                                r = output_adam7[i];
                                ALPHA(r, a);
                                bitmap[pos + j] = make_rgba(r, r, r, a);

                                i++;
                            }

                            pos += (png_row_increment[pass] * png_header.width);
                        }
                        else if(png_header.bit_depth == 16)
                        {
                            for(j = png_col_start[pass];
                                j < png_header.width; 
                                j += png_col_increment[pass])
                            {
                                r = (((int16_t)output_adam7[i] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                                ALPHA(r, a);
                                bitmap[pos + j] = make_rgba(r, r, r, a);

                                i += 2;
                            }

                            pos += (png_row_increment[pass] * png_header.width);
                        }
                        else
                        {
                            invalid_image("Error: Invalid bit depth for the specified "
                                          "color type.\n");
                        }

                        break;

#undef ALPHA

                    //////////////////////////////////////////////////
                    case(2):    // RGB image

#define ALPHA(r, g, b, a)                               \
    if(TRANSPARENCY && (r & tRNS_type_2.red)            \
                    && (g & tRNS_type_2.green)          \
                    && (b & tRNS_type_2.blue)) a = 0;   \
    else a = 255;

                        if(png_header.bit_depth == 8)
                        {
                            for(j = png_col_start[pass];
                                j < png_header.width; 
                                j += png_col_increment[pass])
                            {
                                r = output_adam7[i];
                                g = output_adam7[i + 1];
                                b = output_adam7[i + 2];
                                ALPHA(r, g, b, a);
                                bitmap[pos + j] = make_rgba(r, g, b, a);

                                i += 3;
                            }

                            pos += (png_row_increment[pass] * png_header.width);
                        }
                        else if(png_header.bit_depth == 16)
                        {
                            for(j = png_col_start[pass];
                                j < png_header.width; 
                                j += png_col_increment[pass])
                            {
                                r = (((int16_t)output_adam7[i] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                                g = (((int16_t)output_adam7[i + 1] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                                b = (((int16_t)output_adam7[i + 2] * 
                                            MAXOUTSAMPLE / MAXINSAMPLE));
                                ALPHA(r, g, b, a);
                                bitmap[pos + j] = make_rgba(r, g, b, a);

                                i += 6;
                            }

                            pos += (png_row_increment[pass] * png_header.width);
                        }
                        else
                        {
                            invalid_image("Error: Invalid bit depth for the specified "
                                          "color type.\n");
                        }

                        break;

#undef ALPHA

                //////////////////////////////////////////////////
                case(3):    // Palette-indexed image

#define R(index)        palette.color[index].red
#define G(index)        palette.color[index].green
#define B(index)        palette.color[index].blue
#define A(index)        tRNS_type_3[index]

                    if(png_header.bit_depth == 1)
                    {
                        static int shift = 7;

                        for(j = png_col_start[pass];
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            index = (output_adam7[i] >> shift) & 0x01;
                            bitmap[pos + j] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            shift--;

                            if(shift < 0)
                            {
                                shift = 7;
                                i++;
                            }
                        }

                        pos += (png_row_increment[pass] * png_header.width);
                    }
                    else if(png_header.bit_depth == 2)
                    {
                        static int shift = 6;

                        for(j = png_col_start[pass];
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            index = (output_adam7[i] >> shift) & 0x03;
                            bitmap[pos + j] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            shift -= 2;

                            if(shift < 0)
                            {
                                shift = 6;
                                i++;
                            }
                        }

                        pos += (png_row_increment[pass] * png_header.width);
                    }
                    else if(png_header.bit_depth == 4)
                    {
                        static int shift = 4;

                        for(j = png_col_start[pass];
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            index = (output_adam7[i] >> shift) & 0x0f;
                            bitmap[pos + j] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            shift -= 4;

                            if(shift < 0)
                            {
                                shift = 4;
                                i++;
                            }
                        }

                        pos += (png_row_increment[pass] * png_header.width);
                    }
                    else if(png_header.bit_depth == 8)
                    {
                        for(j = png_col_start[pass];
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            index = output_adam7[i];
                            bitmap[pos + j] = make_rgba(R(index), G(index), 
                                                        B(index), A(index));

                            i++;
                        }

                        pos += (png_row_increment[pass] * png_header.width);
                    }
                    else
                    {
                        invalid_image("Error: Invalid bit depth for the specified "
                                      "color type.\n");
                    }

                    break;

#undef R
#undef G
#undef B
#undef A

                //////////////////////////////////////////////////
                case(4):    // Grayscale/Alpha
                    if(png_header.bit_depth == 8)
                    {
                        for(j = png_col_start[pass];
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            r = output_adam7[i];
                            a = output_adam7[i + 1];
                            bitmap[pos + j] = make_rgba(r, r, r, a);

                            i += 2;
                        }

                        pos += (png_row_increment[pass] * png_header.width);
                    }
                    else if(png_header.bit_depth == 16)
                    {
                        for(j = png_col_start[pass];
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            r = (((int16_t)output_adam7[i] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            a = (((int16_t)output_adam7[i + 2] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            bitmap[pos + j] = make_rgba(r, r, r, a);

                            i += 4;
                        }

                        pos += (png_row_increment[pass] * png_header.width);
                    }
                    else
                    {
                        invalid_image("Error: Invalid bit depth for the specified "
                                      "color type.\n");
                    }

                    break;

                //////////////////////////////////////////////////
                case(6):    // RGB/Alpha
                    if(png_header.bit_depth == 8)
                    {
                        for(j = png_col_start[pass];
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            r = output_adam7[i];
                            g = output_adam7[i + 1];
                            b = output_adam7[i + 2];
                            a = output_adam7[i + 3];
                            bitmap[pos + j] = make_rgba(r, g, b, a);

                            i += 4;
                        }

                        pos += (png_row_increment[pass] * png_header.width);
                    }
                    else if(png_header.bit_depth == 16)
                    {
                        for(j = png_col_start[pass];
                            j < png_header.width; 
                            j += png_col_increment[pass])
                        {
                            r = (((int16_t)output_adam7[i] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            g = (((int16_t)output_adam7[i + 1] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            b = (((int16_t)output_adam7[i + 2] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            a = (((int16_t)output_adam7[i + 3] * 
                                        MAXOUTSAMPLE / MAXINSAMPLE));
                            bitmap[pos + j] = make_rgba(r, g, b, a);

                            i += 8;
                        }

                        pos += (png_row_increment[pass] * png_header.width);
                    }
                    else
                    {
                        invalid_image("Error: Invalid bit depth for the specified "
                                      "color type.\n");
                    }

                    break;

                default:
                    invalid_image("Error: Invalid color type.\n");
                    break;
            } //end switch
        } //end while
      }
    }
    
    loaded_bitmap->data = bitmap;
    loaded_bitmap->width = png_header.width;
    loaded_bitmap->height = png_header.height;
    
    res_bitmap = loaded_bitmap;

fin:

    if(deflate_data_in)
    {
        free(deflate_data_in);
    }

    if(output_stream)
    {
        free(output_stream);
    }

    if(output_adam7)
    {
        free(output_adam7);
    }
    
    if(tEXt)
    {
        free(tEXt);
    }
    
    if(image_text)
    {
        free(image_text);
    }

    if(previous)
    {
        free(previous);
    }

    if(hIST)
    {
        free(hIST);
    }

    if(tRNS_type_3)
    {
        free(tRNS_type_3);
    }

    return res_bitmap;
}

