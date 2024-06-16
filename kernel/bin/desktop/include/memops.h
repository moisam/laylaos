/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: memops.h
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
 *  \file memops.h
 *
 *  Inlined fast memset to fill destination with 32 or 16 bit values.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __x86_64__
#include <immintrin.h>
#endif


#define memcpy32    __builtin_memcpy


/*
 * n = number of dwords (4 bytes).
 */
static inline void *memset32(void *dest, uint32_t val32, size_t n)
{
    if(!n)
    {
        return dest;
    }

#ifdef __x86_64__

    //__asm__ __volatile__ ("xchg %%bx, %%bx"::);
    
    size_t i, qn;
    uint8_t *_dest;
    uint32_t *wdest = (uint32_t *)dest;
    
    switch(n)
    {
        case 4: *wdest++ = val32;
        case 3: *wdest++ = val32;
        case 2: *wdest++ = val32;
        case 1: *wdest = val32; return dest;
    }
    
    switch((uintptr_t)wdest & 0xf)
    {
        case 4: *wdest++ = val32; n--;
        case 8: *wdest++ = val32; n--;
        case 12: *wdest++ = val32; n--;
    }
    
    qn = n / 8;
    _dest = (uint8_t *)wdest;
    
    __m128i m0 = _mm_setr_epi32(val32, val32, val32, val32);

    for(i = 0; i < qn; i++)
    {
        _mm_storeu_si128((__m128i *)_dest, m0);
        _mm_storeu_si128((__m128i *)(_dest + 16), m0);

        _dest += 32;
    }
    
    /*
    struct
    {
        uint32_t w1, w2, w3, w4;
    } __attribute__((packed)) val128 = { val32, val32, val32, val32 };

    __asm__ __volatile__ ("movups (%0), %%xmm0" :: "r"(&val128));

    for(i = 0; i < qn; i++)
    {
        //_mm_storeu_si128(((__m128i *)_dest) + 0, m0);
        //_mm_storeu_si128(((__m128i *)_dest) + 1, m0);

        __asm__ __volatile__ (
                "movntdq %%xmm0, (%0)\n"
                "movntdq %%xmm0, 16(%0)\n"
                :: "r"(_dest) : "memory");

        _dest += 32;
    }
    */

    wdest = (uint32_t *)_dest;
    
    switch(n & 7)
    {
        case 7: *wdest++ = val32;
        case 6: *wdest++ = val32;
        case 5: *wdest++ = val32;
        case 4: *wdest++ = val32;
        case 3: *wdest++ = val32;
        case 2: *wdest++ = val32;
        case 1: *wdest++ = val32;
    }

#else

    uint32_t num_qwords = n >> 1;
    uint32_t num_dwords = n & 1;
    uint64_t *dest64 = (uint64_t*)dest;
    uint32_t *dest32 = (uint32_t*)(&dest64[num_qwords]);
    uint64_t val64 = val32|((uint64_t)val32<<32);
    uint64_t *ldest64 = &dest64[num_qwords];

    while(dest64 < ldest64)
    {
        *dest64++ = val64;
    }

    if(num_dwords)
    {
        *dest32 = val32;
    }

#endif

    return dest;
}


/*
 * n = number of words (2 bytes).
 */
static inline void *memset16(void *dest, uint16_t val16, size_t n)
{
    if(!n)
    {
        return dest;
    }

    uint32_t num_qwords = n/4;
    uint32_t num_dwords = (n%4)/2;
    uint32_t num_words = (n%4)%2;
    uint64_t *dest64 = (uint64_t*)dest;
    uint32_t *dest32 = (uint32_t*)(&dest64[num_qwords]);
    uint16_t *dest16 = (uint16_t*)(&dest32[num_dwords]);
    uint32_t val32 = val16|((uint32_t)val16<<16);
    uint64_t val64 = val32|((uint64_t)val32<<32);
    uint32_t i;

    for(i = 0; i < num_qwords; i++)
    {
        dest64[i] = val64;
    }

    for(i = 0; i < num_dwords; i++)
    {
        dest32[i] = val32;
    }

    if(num_words)
    {
        *dest16 = val16;
    }

    return dest;
}

