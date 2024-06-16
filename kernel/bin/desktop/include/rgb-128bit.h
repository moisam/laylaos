/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: rgb-128bit.h
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
 *  \file rgb-128bit.h
 *
 *  Inlined functions to work with RGB colors. These are optimised to use
 *  SSE instructions and work on multiple pixels at the same time.
 */

#ifdef __x86_64__

#include <immintrin.h>

static inline void blit_bitmap_32_128bit(struct gc_t *gc, uint8_t *dest,
                                         uint32_t *src, unsigned int srcw,
                                         int x, int maxx, int y, int maxy,
                                         uint32_t hicolor)
{
    int curx;
    uint32_t hir = ((hicolor >> 24) & 0xff);
    uint32_t hig = ((hicolor >> 16) & 0xff);
    uint32_t hib = ((hicolor >> 8 ) & 0xff);
    __m128i dst128, src128, alpha128, compalpha128;
    __m128i rsrc, gsrc, bsrc, rdst, gdst, bdst;
    __m128i rshift = _mm_set_epi32(0, 0, 0, gc->screen->red_pos);
    __m128i gshift = _mm_set_epi32(0, 0, 0, gc->screen->green_pos);
    __m128i bshift = _mm_set_epi32(0, 0, 0, gc->screen->blue_pos);
    __m128i ff_mask = _mm_set_epi32(0xff, 0xff, 0xff, 0xff);
    __m128i comp_mask = _mm_set_epi32(0x100, 0x100, 0x100, 0x100);
    //__m128i all_ff = _mm_set1_epi8(255);

    for( ; y < maxy; y++)
    {
        uint32_t *buf32, *src32;

        buf32 = (uint32_t *)dest;
        src32 = (uint32_t *)src;

        for(curx = x; curx < maxx - 3; curx += 4)
        {
            if(hicolor)
            {
                src128 = _mm_set_epi32(highlight(src32[3], hir, hig, hib),
                                       highlight(src32[2], hir, hig, hib),
                                       highlight(src32[1], hir, hig, hib),
                                       highlight(src32[0], hir, hig, hib));
            }
            else
            {
                src128 = _mm_loadu_si128((__m128i const *)src32);
            }

            alpha128 = _mm_and_si128(src128, ff_mask);
            //compalpha128 = _mm_subs_epu8(all_ff, alpha128);
            compalpha128 = _mm_subs_epu16(comp_mask, alpha128);
            dst128 = _mm_loadu_si128((__m128i const *)buf32);

            // red
            rsrc = _mm_srli_si128(src128, 3);
            rsrc = _mm_and_si128(rsrc, ff_mask);
            rsrc = _mm_mullo_epi16(rsrc, alpha128);

            rdst = _mm_srl_epi32(dst128, rshift);
            rdst = _mm_and_si128(rdst, ff_mask);
            rdst = _mm_mullo_epi16(rdst, compalpha128);

            rdst = _mm_add_epi8(rdst, rsrc);
            rdst = _mm_srli_si128(rdst, 1);
            rdst = _mm_sll_epi32(rdst, rshift);

            // green
            gsrc = _mm_srli_si128(src128, 2);
            gsrc = _mm_and_si128(gsrc, ff_mask);
            gsrc = _mm_mullo_epi16(gsrc, alpha128);

            gdst = _mm_srl_epi32(dst128, gshift);
            gdst = _mm_and_si128(gdst, ff_mask);
            gdst = _mm_mullo_epi16(gdst, compalpha128);

            gdst = _mm_add_epi8(gdst, gsrc);
            gdst = _mm_srli_si128(gdst, 1);
            gdst = _mm_sll_epi32(gdst, gshift);

            // blue
            bsrc = _mm_srli_si128(src128, 1);
            bsrc = _mm_and_si128(bsrc, ff_mask);
            bsrc = _mm_mullo_epi16(bsrc, alpha128);

            bdst = _mm_srl_epi32(dst128, bshift);
            bdst = _mm_and_si128(bdst, ff_mask);
            bdst = _mm_mullo_epi16(bdst, compalpha128);

            bdst = _mm_add_epi8(bdst, bsrc);
            bdst = _mm_srli_si128(bdst, 1);
            bdst = _mm_sll_epi32(bdst, bshift);

            rdst = _mm_or_si128(rdst, gdst);
            rdst = _mm_or_si128(rdst, bdst);

            _mm_storeu_si128((__m128i *)buf32, rdst);

            buf32 += 4;
            src32 += 4;
        }

        for( ; curx < maxx; curx++)
        {
            uint32_t compalpha, alpha, r, g, b, tmp;

            if(hicolor)
            {
                tmp = highlight(*src32, hir, hig, hib);
            }
            else
            {
                tmp = *src32;
            }

// our colors are in the RGBA format
#define R(c)            ((c >> 24) & 0xff)
#define G(c)            ((c >> 16) & 0xff)
#define B(c)            ((c >> 8) & 0xff)
#define A(c)            ((c) & 0xff)

            alpha = A(*src32);
            compalpha = 0x100 - alpha;
            r = ((R(tmp) * alpha) >> 8) + 
                ((gc_red_component32(gc, *buf32) * compalpha) >> 8);
            g = ((G(tmp) * alpha) >> 8) + 
                ((gc_green_component32(gc, *buf32) * compalpha) >> 8);
            b = ((B(tmp) * alpha) >> 8) + 
                ((gc_blue_component32(gc, *buf32) * compalpha) >> 8);

#undef R
#undef G
#undef B
#undef A

            *buf32 = gc_comp_to_rgb32(gc, r, g, b);
            buf32++;
            src32++;
        }

        dest += gc->pitch;
        src += srcw;
    }
}

#endif      /* __x86_64__ */
