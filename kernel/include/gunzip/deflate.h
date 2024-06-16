/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: deflate.h
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
 *  \file deflate.h
 *
 *  Functions and macros for deflating (unzipping) GZip archives. This is
 *  used during boot to unzip the initial ramdisk. These functions work
 *  together with those declared in member.h to unzip the archive.
 */

#ifndef GZIP_DEFLATE_H
#define GZIP_DEFLATE_H

#include <stdint.h>


#ifdef __HUFFMAN_DEFS
/*
 * Table definitions for Huffman decoding algorithms,
 * See RFC 1951 for details.
 */
short LEN_EXTRA_BITS[] = 
  { 
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, -1, -1
  };
int LEN_BASE_VAL[] = 
  { 
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 
    27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163,
    195, 227, 258
  };
short DIST_EXTRA_BITS[] = 
  { 
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 
    7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, -1, -1
  };
int DIST_BASE_VAL[] = 
  { 
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
    6145, 8193, 12289, 16385, 24577
  };
short CODE_LENGTHS_POS[] = 
  { 
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 
    2, 14, 1, 15
  };

#endif  /* __HUFFMAN_DEFS */

/* define structure for a Huffman tree item */
typedef struct
{
  unsigned int len;
  unsigned int code;
} HTREE;

/* error codes returned from deflate() */
#define GZIP_INVALID_BLOCKLEN       (16)    /**< deflate() encountered an
                                                 invalid block length */
#define GZIP_INVALID_BLOCKDATA      (17)    /**< deflate() encountered an
                                                 invalid block data */
#define GZIP_INVALID_ENCODING       (18)    /**< deflate() encountered an
                                                 invalid encoding */
#define GZIP_INSUFFICIENT_MEMORY    (255)   /**< deflate() could not proceed
                                                 due to insufficient memory */

/**
 * @brief Deflate a GZipped archive.
 *
 * This function performs a deflate (unzip) operation on the archive loaded
 * at memory address \a datain with length \a data_length, starting at byte
 * \a inbytepos and bit \a inbitpos in the buffer (those two variables are
 * update at function return). Deflated data is written to the buffer at
 * \a dataout, starting at byte \a outpos in the buffer (this is updated at
 * function return). The output buffer length is specified in \a outlen.
 *
 * @param   datain          input buffer with compressed data
 * @param   data_length     length of input buffer
 * @param   inbitpos        initial bit position in input buffer
 * @param   inbytepos       initial byte position in input buffer
 * @param   dataout         output buffer to be filled with decompressed data
 * @param   outlen          length of output buffer
 * @param   outpos          position in output buffer
 *
 * @return  \ref GZIP_VALID_ARCHIVE (0) on success, non-zero on failure.
 *
 * @see read_member()
 */
int deflate_in_memory(char *datain, uint32_t data_length,
                      uint32_t *inbitpos, uint32_t *inbytepos,
                      char *dataout, long outlen, uint32_t *outpos);

#endif      /* GZIP_DEFLATE_H */
