/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: member.c
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
 *  \file member.c
 *
 *  This low-level part of unzipping a GZipped archive is done by the 
 *  deflate() function, which is defined in deflate.c. This file defines the
 *  read_member() function, which performs the high-level end of the process,
 *  and is the function we call early during boot to unzip the initial ramdisk.
 */

#include <stdio.h>
#include <string.h>
#include <kernel/laylaos.h>
//#include <mm/mmngr_virtual.h>
#include <mm/memregion.h>
#include <gunzip/deflate.h>
#include <gunzip/member.h>

/* defined in crc.c */
extern unsigned long crc(unsigned char *buf, int len);

#define KB          (1024)
#define MB          ((KB)*1024)


/*
 * Print human-friendly archive size.
 */
char *get_mbs(long size)
{
    static char buf[32];

    if(size < KB)
    {
        ksprintf(buf, sizeof(buf), "%ld bytes", size);
    }
    else if(size < MB)
    {
        ksprintf(buf, sizeof(buf), "%ldKiB", size/KB);
    }
    else
    {
        ksprintf(buf, sizeof(buf), "%ldMiB", size/MB);
    }
    
    return buf;
}


static inline void free_pages(virtual_addr start, size_t sz)
{
    virtual_addr end = start + sz;
    
    while(start < end)
    {
        vmmngr_free_page(get_page_entry((void *) start));
        vmmngr_flush_tlb_entry(start);
        start += PAGE_SIZE;
    }
}


/*
 * For details on GZIP format, see:
 *      https://datatracker.ietf.org/doc/html/rfc1952
 *
 * For details on DEFLATE format, see:
 *      https://www.ietf.org/rfc/rfc1951.txt
 */


int read_member(char *src, long srcsize, virtual_addr *dest, size_t *destsize)
{
    printk("    Reading compressed image at " _XPTR_ " (size %s)..\n",
           src, get_mbs(srcsize));

    struct gzip_member_s *hdr = (struct gzip_member_s *)src;

    if(hdr->ID1 != 0x1f || hdr->ID2 != 0x8b)
    {
        return GZIP_INVALID_SIGNATURE;
    }
    
    if(hdr->CM != COMPRESSION_METHOD_DEFLATE)
    {
        return GZIP_INVALID_CM;
    }
    
    uint16_t  fextra_len   = 0   ;
    char     *fname        = NULL;
    uint16_t  fname_len    = 0   ;
    char     *fcomment     = NULL;
    uint16_t  fcomment_len = 0   ;
    char     *comp_blocks  = NULL;
    uint32_t  crc32        = 0   ;
    uint32_t  isize        = 0   ;
    uint32_t  isize2       = 0   ;
    char     *p            = src + sizeof(struct gzip_member_s);
    
    /* check the reserved fields are zero */
    if(hdr->FLG & FLAG_RESERVED)
    {
        return GZIP_INVALID_FLG;
    }
    
    /* NOTE: we are not using the extra fields */
    if(hdr->FLG & FLAG_FEXTRA)
    {
        struct gzip_fextra_s *extra = (struct gzip_fextra_s *)p;
        fextra_len   = extra->XLEN;
        p           += fextra_len + 2;
    }

    if(hdr->FLG & FLAG_FNAME)
    {
        fname        = p;
        fname_len    = strlen(fname);
        p           += fname_len+1;
    }

    if(hdr->FLG & FLAG_FCOMMENT)
    {
        fcomment     = p;
        fcomment_len = strlen(fcomment);
        p           += fcomment_len+1;
    }

    if(hdr->FLG & FLAG_FHCRC)
    {
        p += 2;
    }

    if(hdr->XFL != XFL_MAX_COMPRESSION && hdr->XFL != XFL_FASTEST_ALGORITHM)
    {
        return GZIP_INVALID_XFL;
    }

    if(hdr->OS > 14 && hdr->OS < 255)
    {
        return GZIP_INVALID_OS;
    }

    comp_blocks = p;

    /* 
     * Calculate the correct input size. This is the original size minus
     * the above data (header, ...) and the 8 bytes of CRC32 + ISIZE.
     */
    srcsize -= (p-src)+8;
    int res = 0;

    /* crc check */
    p    += srcsize;
    crc32 = *(uint32_t *)p;
    p    += 4;
    isize = *(uint32_t *)p;
    printk("    Deflated image size is %s..\n", get_mbs(isize));
    
    /* reserve the required memory */
    if((INITRD_START + isize) > INITRD_END)
    {
        kpanic("initrd is too big\n");
    }
    
    if(!vmmngr_alloc_pages(INITRD_START, isize, PTE_FLAGS_PW))
    {
        return GZIP_INSUFFICIENT_MEMORY;
    }

    /* deflate! */
    if((res = deflate_in_memory(comp_blocks, srcsize, NULL, NULL,
                                (char *)INITRD_START, isize, &isize2))
                                    != GZIP_VALID_ARCHIVE)
    {
        free_pages(INITRD_START, isize);
        return res;
    }
    
    printk("    Checking image size..\n");

    if(isize != isize2)
    {
        free_pages(INITRD_START, isize);
        return GZIP_INVALID_ISIZE;
    }

    printk("    Calculating image CRC32..\n");
    uint32_t calc_crc32 = crc((unsigned char *)INITRD_START, (int)isize);

    if(calc_crc32 != crc32)
    {
        free_pages(INITRD_START, isize);
        return GZIP_INVALID_CRC32;
    }

    printk("    Image decompressed at " _XPTR_ " (size %s)\n",
           INITRD_START, get_mbs(isize));
    
    *dest = INITRD_START;
    *destsize = isize;
    
    return GZIP_VALID_ARCHIVE;
}

