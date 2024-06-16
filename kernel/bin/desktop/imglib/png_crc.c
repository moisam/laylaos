/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: png_crc.c
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
 *  \file png_crc.c
 *
 *  Helper function to calculate CRC when loading PNG files.
 *
 *  The CRC calculation algorithm and CRC table are adopted from the work
 *  of Lode Vandevenne (2005-2012) with only minor modifications. The original
 *  code is part of his LodePNG codec, the source of which can be found
 *  on the website: http://lodev.org/lodepng/.
 *
 *  CRC calculation code is adopted from the PNG specification. You can
 *  find the full information at:
 *  http://www.libpng.org/pub/png/spec/1.0/PNG-CRCAppendix.html.
 */

/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];
   
/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;
   
/* Make the table for a fast CRC. */
void make_crc_table(void)
{
    unsigned long c;
    int n, k;
   
    for(n = 0; n < 256; n++)
    {
        c = (unsigned long) n;

        for(k = 0; k < 8; k++)
        {
            if(c & 1) c = 0xedb88320L ^ (c >> 1);
            else c = c >> 1;
        }

        crc_table[n] = c;
    }

    crc_table_computed = 1;
}
   
/*
 * Update a running CRC with the bytes buf[0..len-1]--the CRC
 * should be initialized to all 1's, and the transmitted value
 * is the 1's complement of the final running CRC (see the
 * crc() routine below)).
 */

unsigned long update_crc(unsigned long crc, unsigned char *buf, int len)
{
    unsigned long c = crc;
    int n;
   
    if(!crc_table_computed) make_crc_table();

    for(n = 0; n < len; n++)
    {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }

    return c;
}
   
/* Return the CRC of the bytes buf[0..len-1]. */
unsigned long calculate_crc32(unsigned char *buf, int len)
{
     return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

