/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gc-ttf.c
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
 *  \file gc-ttf.c
 *
 *  The graphics context implementation is divided into multiple files:
 *  - gc.c: contains functions to create and destroy graphics context objects,
 *    as well as perform basic drawing functions,
 *  - gc-arc.c: functions to draw arcs,
 *  - gc-bitmap.c: functions to copy (blit) bitmaps,
 *  - gc-bitmap-stretch.c: functions to copy bitmaps stretched,
 *  - gc-circle.c: functions to draw circles (hollow and filled),
 *  - gc-line.c: functions to draw lines of different thickness,
 *  - gc-poly.c: functions to draw polygons (hollow and filled),
 *  - gc-ttf.c: functions to draw text using TrueType Fonts (TTF),
 */

#include "../include/gc.h"
#include "../include/font.h"
#include "../include/rgb.h"
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftglyph.h>


struct TGlyph
{
    FT_UInt    index;  // glyph index
    FT_Vector  pos;    // glyph origin on the baseline
    struct Cached_TGlyph *image;
    int        underlined;
};


// our colors are in the RGBA format
#define R(c)            ((c >> 24) & 0xff)
#define G(c)            ((c >> 16) & 0xff)
#define B(c)            ((c >> 8) & 0xff)
#define A(c)            ((c) & 0xff)


/*
 * Draw a single character with the specified font color at the 
 * specified coordinates.
 */
void __gc_draw_char_clipped_ttf(struct gc_t *gc, struct TGlyph *glyph,
                                int x, int y,
                                uint32_t color, Rect *bound_rect)
{
    FT_BitmapGlyph bit;
    unsigned char *bitmap_buffer;
    unsigned char *srcbuf;
    int destx, desty, srcx, srcy;
    int destx2, desty2, srcx2, srcy2;

    bit = (FT_BitmapGlyph)glyph->image->image;
    bitmap_buffer = bit->bitmap.buffer;
    destx = x + bit->left;
    desty = y - bit->top;
    destx2 = destx + bit->bitmap.width;
    desty2 = desty + bit->bitmap.rows;
    srcx = 0;
    srcy = 0;
    srcx2 = bit->bitmap.width;
    srcy2 = bit->bitmap.rows;

    if(destx > bound_rect->right || desty > bound_rect->bottom)
    {
        return;
    }

    if(destx < bound_rect->left)
    {
        srcx += (bound_rect->left - destx);
        destx = bound_rect->left;
    }

    if(desty < bound_rect->top)
    {
        srcy += (bound_rect->top - desty);
        desty = bound_rect->top;
    }

    if(destx2 > bound_rect->right + 1)
    {
        srcx2 -= (destx2 - (bound_rect->right + 1));
        destx2 = bound_rect->right + 1;
    }

    if(desty2 > bound_rect->bottom + 1)
    {
        srcy2 -= (desty2 - (bound_rect->bottom + 1));
        desty2 = bound_rect->bottom + 1;
    }

    if(destx2 < destx || desty2 < desty)
    {
        return;
    }

    unsigned where = destx * gc->pixel_width + desty * gc->pitch;
    uint8_t *buf = (uint8_t *)(gc->buffer + where);
    uint8_t *buf2;
    int i, l;

    srcbuf = bitmap_buffer + (srcy * bit->bitmap.pitch);
    color &= 0xffffff00;

    if(gc->pixel_width == 1)
    {
        //uint8_t col8 = to_rgb8(gc, color), tmp;

        for(l = srcy; l < srcy2; l++)
        {
            buf2 = buf;

            for(i = srcx; i < srcx2; i++)
            {
                *buf2 = alpha_blend8(gc, color | srcbuf[i], *buf2);
                buf2++;

                /*
                alpha = srcbuf[i];

                if(alpha == 0)
                {
                    // foreground is transparent - nothing to do here
                }
                else if(alpha == 0xff)
                {
                    // foreground is opaque - copy to buffer
                    *buf2 = col8;
                }
                else
                {
                    compalpha = 0x100 - alpha;
                    tmp = *buf2;
                    r = ((R(color) * alpha) >> 8) + 
                        ((gc_red_component8(gc, tmp) * compalpha) >> 8);
                    g = ((G(color) * alpha) >> 8) + 
                        ((gc_green_component8(gc, tmp) * compalpha) >> 8);
                    b = ((B(color) * alpha) >> 8) + 
                        ((gc_blue_component8(gc, tmp) * compalpha) >> 8);

                    *buf2 = gc_comp_to_rgb8(gc, r, g, b);
                }

                buf2 += gc->pixel_width;
                */
            }

            buf += gc->pitch;
            srcbuf += bit->bitmap.pitch;
        }

        // draw underline if needed
        if(glyph->underlined)
        {
            uint8_t col8 = to_rgb8(gc, color | 0xff);

            buf -= gc->pitch;

            for(i = srcx; i < srcx2; i++)
            {
                *buf = col8;
                buf += gc->pixel_width;
            }
        }
    }
    else if(gc->pixel_width == 2)
    {
        //uint16_t col16 = to_rgb16(gc, color), tmp;

        for(l = srcy; l < srcy2; l++)
        {
            buf2 = buf;

            for(i = srcx; i < srcx2; i++)
            {
                *(uint16_t *)buf2 = alpha_blend16(gc, color | srcbuf[i], 
                                                      *(uint16_t *)buf2);
                buf2 += 2;

                /*
                alpha = srcbuf[i];

                if(alpha == 0)
                {
                    // foreground is transparent - nothing to do here
                }
                else if(alpha == 0xff)
                {
                    // foreground is opaque - copy to buffer
                    *(uint16_t *)buf2 = col16;
                }
                else
                {
                    compalpha = 0x100 - alpha;
                    tmp = *(uint16_t *)buf2;
                    r = ((R(color) * alpha) >> 8) + 
                        ((gc_red_component16(gc, tmp) * compalpha) >> 8);
                    g = ((G(color) * alpha) >> 8) + 
                        ((gc_green_component16(gc, tmp) * compalpha) >> 8);
                    b = ((B(color) * alpha) >> 8) + 
                        ((gc_blue_component16(gc, tmp) * compalpha) >> 8);

                    *(uint16_t *)buf2 = gc_comp_to_rgb16(gc, r, g, b);
                }

                buf2 += gc->pixel_width;
                */
            }

            buf += gc->pitch;
            srcbuf += bit->bitmap.pitch;
        }

        // draw underline if needed
        if(glyph->underlined)
        {
            uint16_t col16 = to_rgb16(gc, color | 0xff);

            buf -= gc->pitch;

            for(i = srcx; i < srcx2; i++)
            {
                *(uint16_t *)buf = col16;
                buf += gc->pixel_width;
            }
        }
    }
    else if(gc->pixel_width == 3)
    {
        /*
        uint32_t col24 = to_rgb24(gc, color), tmp;
        uint8_t b0 = col24 & 0xff;
        uint8_t b1 = ((col24 >> 8) & 0xff);
        uint8_t b2 = ((col24 >> 16) & 0xff);
        */
        uint32_t tmp;

        for(l = srcy; l < srcy2; l++)
        {
            buf2 = buf;

            for(i = srcx; i < srcx2; i++)
            {
                tmp = (uint32_t)buf2[0] |
                      ((uint32_t)buf2[1]) << 8 |
                      ((uint32_t)buf2[2]) << 16;
                tmp = alpha_blend24(gc, color | srcbuf[i], tmp);
                buf2[0] = tmp & 0xff;
                buf2[1] = (tmp >> 8) & 0xff;
                buf2[2] = (tmp >> 16) & 0xff;
                buf2 += 3;

                /*
                alpha = srcbuf[i];

                if(alpha == 0)
                {
                    // foreground is transparent - nothing to do here
                }
                else if(alpha == 0xff)
                {
                    // foreground is opaque - copy to buffer
                    buf2[0] = b0;
                    buf2[1] = b1;
                    buf2[2] = b2;
                }
                else
                {
                    compalpha = 0x100 - alpha;
                    tmp = (uint32_t)buf2[0] |
                           ((uint32_t)buf2[1]) << 8 |
                           ((uint32_t)buf2[2]) << 16;
                    r = ((R(color) * alpha) >> 8) + 
                        ((gc_red_component24(gc, tmp) * compalpha) >> 8);
                    g = ((G(color) * alpha) >> 8) + 
                        ((gc_green_component24(gc, tmp) * compalpha) >> 8);
                    b = ((B(color) * alpha) >> 8) + 
                        ((gc_blue_component24(gc, tmp) * compalpha) >> 8);

                    tmp = gc_comp_to_rgb24(gc, r, g, b);
                    buf2[0] = tmp & 0xff;
                    buf2[1] = (tmp >> 8) & 0xff;
                    buf2[2] = (tmp >> 16) & 0xff;
                }

                buf2 += gc->pixel_width;
                */
            }

            buf += gc->pitch;
            srcbuf += bit->bitmap.pitch;
        }

        // draw underline if needed
        if(glyph->underlined)
        {
            uint32_t col24 = to_rgb24(gc, color | 0xff);
            uint8_t b0 = col24 & 0xff;
            uint8_t b1 = ((col24 >> 8) & 0xff);
            uint8_t b2 = ((col24 >> 16) & 0xff);

            buf -= gc->pitch;

            for(i = srcx; i < srcx2; i++)
            {
                buf[0] = b0;
                buf[1] = b1;
                buf[2] = b2;
                buf += gc->pixel_width;
            }
        }
    }
    else
    {
        //uint32_t col32 = to_rgb32(gc, color), tmp;

        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        for(l = srcy; l < srcy2; l++)
        {
            buf2 = buf;

            for(i = srcx; i < srcx2; i++)
            {
                *(uint32_t *)buf2 = alpha_blend32(gc, color | srcbuf[i], 
                                                      *(uint32_t *)buf2);
                buf2 += 4;

                /*
                alpha = srcbuf[i];

                if(alpha == 0)
                {
                    // foreground is transparent - nothing to do here
                }
                else if(alpha == 0xff)
                {
                    // foreground is opaque - copy to buffer
                    *(uint32_t *)buf2 = col32;
                }
                else
                {
                    compalpha = 0x100 - alpha;
                    tmp = *(uint32_t *)buf2;
                    r = ((R(color) * alpha) >> 8) + 
                        ((gc_red_component32(gc, tmp) * compalpha) >> 8);
                    g = ((G(color) * alpha) >> 8) + 
                        ((gc_green_component32(gc, tmp) * compalpha) >> 8);
                    b = ((B(color) * alpha) >> 8) + 
                        ((gc_blue_component32(gc, tmp) * compalpha) >> 8);

                    *(uint32_t *)buf2 = gc_comp_to_rgb32(gc, r, g, b);
                }

                buf2 += gc->pixel_width;
                */
            }

            buf += gc->pitch;
            srcbuf += bit->bitmap.pitch;
        }

        // draw underline if needed
        if(glyph->underlined)
        {
            uint32_t col32 = to_rgb32(gc, color | 0xff);

            buf -= gc->pitch;

            for(i = srcx; i < srcx2; i++)
            {
                *(uint32_t *)buf = col32;
                buf += gc->pixel_width;
            }
        }
    }

    //FT_Done_Glyph(image);
}

#undef R
#undef G
#undef B
#undef A


void gc_draw_char_clipped_ttf(struct gc_t *gc, struct clipping_t *clipping,
                                              struct TGlyph *glyph,
                                              int x, int y, uint32_t color)
{
    Rect *clip_area;
    Rect screen_area;

    // If there are clipping rects, draw the character clipped to
    // each of them. Otherwise, draw unclipped (clipped to the screen)
    if(clipping->clip_rects && clipping->clip_rects->root)
    {
        for(clip_area = clipping->clip_rects->root; 
            clip_area != NULL; 
            clip_area = clip_area->next)
        {
            __gc_draw_char_clipped_ttf(gc, glyph, x, y, color, clip_area);
        }
    }
    else
    {
        if(!clipping->clipping_on)
        {
            screen_area.top = 0;
            screen_area.left = 0;
            screen_area.bottom = gc->h - 1;
            screen_area.right = gc->w - 1;
            __gc_draw_char_clipped_ttf(gc, glyph, x, y, color, &screen_area);
        }
    }
}


/*
void compute_string_bbox(FT_BBox *abbox, struct TGlyph *glyphs, 
                         FT_UInt num_glyphs)
{
    FT_BBox bbox, glyph_bbox;
    FT_UInt i;

    // initialize string bbox to "empty" values
    bbox.xMin = 32000;
    bbox.yMin = 32000;
    bbox.xMax = -32000;
    bbox.yMax = -32000;

    // for each glyph image, compute its bounding box, translate it,
    // and grow the string bbox
    for(i = 0; i < num_glyphs; i++)
    {
        FT_Glyph_Get_CBox(glyphs[i].image->image, ft_glyph_bbox_pixels, &glyph_bbox);
        glyph_bbox.xMin += glyphs[i].pos.x;
        glyph_bbox.xMax += glyphs[i].pos.x;
        glyph_bbox.yMin += glyphs[i].pos.y;
        glyph_bbox.yMax += glyphs[i].pos.y;

        if(glyph_bbox.xMin < bbox.xMin)
        {
            bbox.xMin = glyph_bbox.xMin;
        }

        if(glyph_bbox.yMin < bbox.yMin)
        {
            bbox.yMin = glyph_bbox.yMin;
        }

        if(glyph_bbox.xMax > bbox.xMax)
        {
            bbox.xMax = glyph_bbox.xMax;
        }

        if(glyph_bbox.yMax > bbox.yMax)
        {
            bbox.yMax = glyph_bbox.yMax;
        }
    }

    // check that we really grew the string bbox
    if(bbox.xMin > bbox.xMax)
    {
        bbox.xMin = 0;
        bbox.xMax = 0;
        bbox.yMin = 0;
        bbox.yMax = 0;
    }

    // return string bbox
    *abbox = bbox;
}
*/


void free_tglyph_cache(struct font_t *font)
{
    struct font_cache_t *cache = font->glyph_caches, *next;
    FT_UInt i;

    mutex_lock(&font->lock);

    while(cache)
    {
        if(cache->glyphs)
        {
            for(i = 0; i < cache->glyph_count; i++)
            {
                if(cache->glyphs[i].image)
                {
                    FT_Done_Glyph(cache->glyphs[i].image);
                    cache->glyphs[i].image = 0;
                }
            }

            free(cache->glyphs);
            cache->glyphs = NULL;
        }

        next = cache->next;
        free(cache);
        cache = next;
    }

    font->glyph_caches = NULL;

    mutex_unlock(&font->lock);
}


struct Cached_TGlyph *get_tglyph(struct font_t *font, FT_UInt index)
{
    FT_GlyphSlot slot = font->ft_face->glyph;
    struct font_cache_t *cache;

    // find the glyph cache for the given font size
    for(cache = font->glyph_caches; cache != NULL; cache = cache->next)
    {
        if(cache->ptsz == font->ptsz)
        {
            break;
        }
    }

    // no cache -- create one
    if(!cache)
    {
        // find the number of glyph indices for this font
        FT_ULong charcode;
        FT_UInt gindex, count = 0;

        charcode = FT_Get_First_Char(font->ft_face, &gindex);

        while(gindex != 0)
        {
            if(gindex > count)
            {
                count = gindex;
            }

            charcode = FT_Get_Next_Char(font->ft_face, charcode, &gindex);
        }

        // font has no glyphs??
        if(!count)
        {
            return NULL;
        }

        // create a new cache
        if(!(cache = malloc(sizeof(struct font_cache_t))))
        {
            return NULL;
        }

        A_memset(cache, 0, sizeof(struct font_cache_t));
        count++;

        if(!(cache->glyphs = malloc(sizeof(struct Cached_TGlyph) * count)))
        {
            free(cache);
            return NULL;
        }

        A_memset(cache->glyphs, 0, sizeof(struct Cached_TGlyph) * count);
        cache->glyph_count = count;
        cache->ptsz = font->ptsz;
        //cache->size = font->ft_face->size;
        cache->next = font->glyph_caches;
        font->glyph_caches = cache;
    }

    if(cache->glyphs[index].image)
    {
        return &cache->glyphs[index];
    }

    // load glyph image into the slot without rendering
    if(FT_Load_Glyph(font->ft_face, index, FT_LOAD_DEFAULT) != 0)
    {
        return NULL;
    }

    // extract glyph image and store it in our table
    if(FT_Get_Glyph(font->ft_face->glyph, &cache->glyphs[index].image) != 0)
    {
        return NULL;
    }

    cache->glyphs[index].advance_x = slot->advance.x >> 6;

    if(FT_Glyph_To_Bitmap(&cache->glyphs[index].image,
                          FT_RENDER_MODE_NORMAL, NULL, 1) != 0)
    {
        FT_Done_Glyph(cache->glyphs[index].image);
        cache->glyphs[index].image = 0;
        return NULL;
    }

    cache->glyphs[index].ptsz = font->ptsz;
    //cache->glyphs[index].size = font->ft_face->size;
    cache->glyphs[index].index = index;

    return &cache->glyphs[index];
}


/*
 * Draw a line of text with the specified font color at the specified
 * coordinates, using the given clipping info.
 *
 * The caller must ensure that gc->font is a valid pointer to a TrueType
 * or similar font, no checks are performed here for this.
 */
void gc_draw_text_clipped_ttf(struct gc_t *gc, struct clipping_t *clipping,
                                               char *string, int x, int y,
                                               uint32_t color,
                                               char accelerator)
{
    size_t len, i;
    struct TGlyph *glyph;
    struct Cached_TGlyph *cached_glyph;
    FT_Bool use_kerning;
    FT_UInt prev, num_glyphs;
    int penx, peny;

    if(!string || !*string)
    {
        return;
    }

    len = strlen(string);
    penx = 0;
    peny = 0;
    num_glyphs = 0;
    use_kerning = FT_HAS_KERNING(gc->font->ft_face);
    prev = 0;

    struct TGlyph glyphs[len];

    for(glyph = glyphs, i = 0; i < len; i++)
    {
        if(accelerator && string[i] == '&' && string[i + 1] != '\0')
        {
            glyph->underlined = 1;
            continue;
        }
        else
        {
            glyph->underlined = 0;
        }

        // convert character code to glyph index
        glyph->index = FT_Get_Char_Index(gc->font->ft_face, string[i]);

        // retrieve kerning distance and move pen position
        if(use_kerning && prev && glyph->index)
        {
            FT_Vector delta;

            FT_Get_Kerning(gc->font->ft_face, prev, glyph->index, 
                                    FT_KERNING_DEFAULT, &delta);
            penx += delta.x >> 6;
        }

        // store current pen position
        glyph->pos.x = penx;
        glyph->pos.y = peny;

        if(!(cached_glyph = get_tglyph(gc->font, glyph->index)))
        {
            // ignore errors, jump to next glyph
            continue;
        }

        // increment pen position
        penx += cached_glyph->advance_x;

        glyph->image = cached_glyph;

        // record current glyph index
        prev = glyph->index;

        // increment number of glyphs
        glyph++;
    }

    num_glyphs = glyph - glyphs;
    //compute_string_bbox(&string_bbox, glyphs, num_glyphs);

    // compute string dimensions in integer pixels
    //string_width = string_bbox.xMax - string_bbox.xMin;
    //string_height = string_bbox.yMax - string_bbox.yMin;

    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    y += char_ascender(gc->font, ' ');

    for(i = 0; i < num_glyphs; i++)
    {
        gc_draw_char_clipped_ttf(gc, clipping, &glyphs[i],
                                     x + glyphs[i].pos.x,
                                     y + glyphs[i].pos.y, color);
    }
}


int string_width_ttf(struct font_t *font, char *string)
{
    FT_GlyphSlot slot = font->ft_face->glyph;
    FT_Bool use_kerning;
    FT_UInt prev, glyph_index;
    int penx;
    size_t len, i;

    len = strlen(string);
    penx = 0;
    use_kerning = FT_HAS_KERNING(font->ft_face);
    prev = 0;

    for(i = 0; i < len; i++)
    {
        // convert character code to glyph index
        glyph_index = FT_Get_Char_Index(font->ft_face, string[i]);

        // retrieve kerning distance and move pen position
        if(use_kerning && prev && glyph_index)
        {
            FT_Vector delta;

            FT_Get_Kerning(font->ft_face, prev, glyph_index, 
                                    FT_KERNING_DEFAULT, &delta);
            penx += delta.x >> 6;
        }

        // load glyph image into the slot without rendering
        if(FT_Load_Glyph(font->ft_face, glyph_index, FT_LOAD_DEFAULT) != 0)
        {
            // ignore errors, jump to next glyph
            continue;
        }

        // increment pen position
        penx += slot->advance.x >> 6;

        // record current glyph index
        prev = glyph_index;
    }

    return penx;
}


int string_width_ttf_no_kerning(struct font_t *font, char *string)
{
    FT_GlyphSlot slot = font->ft_face->glyph;
    FT_UInt glyph_index;
    int penx;
    size_t len, i;

    len = strlen(string);
    penx = 0;

    for(i = 0; i < len; i++)
    {
        // convert character code to glyph index
        glyph_index = FT_Get_Char_Index(font->ft_face, string[i]);

        // load glyph image into the slot without rendering
        if(FT_Load_Glyph(font->ft_face, glyph_index, FT_LOAD_DEFAULT) != 0)
        {
            // ignore errors, jump to next glyph
            continue;
        }

        // increment pen position
        penx += slot->advance.x >> 6;
    }

    return penx;
}


int char_width_ttf(struct font_t *font, char c)
{
    FT_GlyphSlot slot = font->ft_face->glyph;
    FT_UInt glyph_index;

    // convert character code to glyph index
    glyph_index = FT_Get_Char_Index(font->ft_face, c);

    // load glyph image into the slot without rendering
    if(FT_Load_Glyph(font->ft_face, glyph_index, FT_LOAD_DEFAULT) != 0)
    {
        // ignore errors
        return 0;
    }

    // get pen position
    return slot->advance.x >> 6;
}

