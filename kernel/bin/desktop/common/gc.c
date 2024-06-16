/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gc.c
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
 *  \file gc.c
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

#include "../include/memops.h"
#include "../include/gui.h"
#include "../include/gc.h"
//#include "../include/font-array.h"
#include "../include/font.h"
#include "../include/rgb.h"

#ifdef GUI_SERVER
#include "../include/server/window.h"
#endif


struct gc_t *gc_new(uint16_t width, uint16_t height, uint8_t pixel_width,
                    uint8_t *buffer, uint32_t buffer_size, uint32_t pitch,
                    struct screen_t *screen)
{
    struct gc_t *gc;
    
    if(!screen)
    {
        return NULL;
    }

    if(!(gc = (struct gc_t *)malloc(sizeof(struct gc_t))))
    {
        return NULL; 
    }

    gc->clipping.clip_rects = NULL;
    gc->clipping.clipping_on = 0;

    gc->w = width; 
    gc->h = height; 
    gc->pixel_width = pixel_width;
    gc->buffer = buffer;
    gc->buffer_size = buffer_size;
    gc->pitch = pitch;
    gc->screen = screen;

    mutex_init(&gc->lock);

    return gc;
}


int gc_alloc_backbuf(struct gc_t *orig_gc, struct gc_t *backbuf_gc,
                     int w, int h)
{
    Rect *rect;
    uint8_t *backbuf;
    size_t backbufsz = w * h * orig_gc->screen->pixel_width;

    if(!(backbuf = malloc(backbufsz)))
    {
        return -1;
    }

    backbuf_gc->w = w; 
    backbuf_gc->h = h; 
    backbuf_gc->pixel_width = orig_gc->screen->pixel_width;
    backbuf_gc->buffer = backbuf;
    backbuf_gc->buffer_size = backbufsz;
    backbuf_gc->pitch = w * orig_gc->screen->pixel_width;
    backbuf_gc->screen = orig_gc->screen;

    if(!(backbuf_gc->clipping.clip_rects = RectList_new()))
    {
        free(backbuf);
        backbuf_gc->buffer = NULL;
        return -1;
    }

    if(!(rect = Rect_new(0, 0, h - 1,  w - 1)))
    {
        RectList_free(backbuf_gc->clipping.clip_rects);
        free(backbuf);
        backbuf_gc->buffer = NULL;
        return -1;
    }

    RectList_add(backbuf_gc->clipping.clip_rects, rect);
    backbuf_gc->clipping.clipping_on = 1;

    return 0;
}


int gc_realloc_backbuf(struct gc_t *orig_gc, struct gc_t *backbuf_gc,
                       int w, int h)
{
    Rect *rect;
    uint8_t *backbuf;
    size_t backbufsz = w * h * orig_gc->screen->pixel_width;

    if(!(backbuf = malloc(backbufsz)))
    {
        return -1;
    }

    if(backbuf_gc->buffer)
    {
        free(backbuf_gc->buffer);
    }

    backbuf_gc->w = w;
    backbuf_gc->h = h;
    backbuf_gc->buffer = backbuf;
    backbuf_gc->buffer_size = backbufsz;
    backbuf_gc->pitch = w * orig_gc->screen->pixel_width;

    rect = backbuf_gc->clipping.clip_rects->root;
    rect->top = 0;
    rect->left = 0;
    rect->bottom = h - 1;
    rect->right = w - 1;

    return 0;
}


//typedef uint32_t u32vect_t __attribute__ ((vector_size(16)));


static inline void fill_line_32(uint8_t *buf, uint32_t color, int cnt)
{
    uint32_t *buf32 = (uint32_t *)buf;

    while((uintptr_t)buf32 & 0x0f)
    {
        if(cnt--)
        {
            *buf32++ = color;
        }
        else
        {
            return;
        }
    }

    __m128i m0 = _mm_setr_epi32(color, color, color, color);

    while(cnt >= 8)
    {
        _mm_storeu_si128((__m128i *)buf32, m0);
        _mm_storeu_si128((__m128i *)(buf32 + 4), m0);

        buf32 += 8;
        cnt -= 8;
    }
    /*
    if(cnt >= 4)
    {
        u32vect_t valvec = { color, color, color, color };
        u32vect_t *destvec = (u32vect_t *)buf32;

        while(cnt >= 4)
        {
            *destvec++ = valvec;
            cnt -= 4;
        }

        buf32 = (uint32_t *)destvec;
    }
    */

    while(cnt--)
    {
        *buf32++ = color;
    }
}

static inline void fill_rect_32(uint8_t *buf, uint32_t pitch, uint32_t color,
                                int x, int max_x, int y, int max_y)
{
    int cnt = max_x - x;

    for( ; y < max_y; y++)
    {
        fill_line_32(buf, color, cnt);
        //memset32(buf, color, cnt);
        buf += pitch;
    }
}


static inline void fill_rect_24(uint8_t *buf, uint32_t pitch, uint32_t color,
                                int x, int max_x, int y, int max_y)
{
    uint8_t b0 = color & 0xff;
    uint8_t b1 = ((color >> 8) & 0xff);
    uint8_t b2 = ((color >> 16) & 0xff);
    uint8_t *buf2;
    int i;

    for(; y < max_y; y++)
    {
        buf2 = buf;

        for(i = x; i < max_x; i++)
        {
            buf2[0] = b0;
            buf2[1] = b1;
            buf2[2] = b2;
            buf2 += 3;
        }

        buf += pitch;
    }
}


static inline void fill_rect_16(uint8_t *buf, uint32_t pitch, uint32_t color,
                                int x, int max_x, int y, int max_y)
{
    //uint16_t col16 = to_rgb16(color);
    int cnt = max_x - x;

    for(; y < max_y; y++)
    {
        memset16(buf, color, cnt);
        buf += pitch;
    }
}


static inline void fill_rect_8(uint8_t *buf, uint32_t pitch, uint32_t color,
                               int x, int max_x, int y, int max_y)
{
    int cnt = max_x - x;

    for(; y < max_y; y++)
    {
        memset(buf, color, cnt);
        buf += pitch;
    }
}


void gc_clipped_rect(struct gc_t *gc, int x, int y,
                                      int max_x, int max_y,
                                      Rect *clip_area,
                                      uint32_t color)
{
    // Make sure we don't go outside of the clip region:
    if(x < clip_area->left)
    {
        x = clip_area->left;
    }

    if(y < clip_area->top)
    {
        y = clip_area->top;
    }

    if(max_x > clip_area->right + 1)
    {
        max_x = clip_area->right + 1;
    }

    if(max_y > clip_area->bottom + 1)
    {
        max_y = clip_area->bottom + 1;
    }

    if(x > max_x)
    {
        x = max_x;
    }

    if(y > max_y)
    {
        y = max_y;
    }
    
    // Draw the rectangle into the framebuffer line-by line
    unsigned where = x * gc->pixel_width + y * gc->pitch;
    uint8_t *buf = (uint8_t *)(gc->buffer + where);

    if(gc->pixel_width == 1)
    {
        if((color & 0xff) == 0xff)
        {
            // opaque color
            color = to_rgb8(gc, color);
            fill_rect_8(buf, gc->pitch, color, x, max_x, y, max_y);
        }
        else
        {
            // color with some transparency
            int x2;
            uint8_t *buf8;

            for( ; y < max_y; y++)
            {
                buf8 = (uint8_t *)buf;

                for(x2 = x; x2 < max_x; x2++)
                {
                    *buf8 = alpha_blend8(gc, color, *buf8);
                    buf8++;
                }

                buf += gc->pitch;
            }
        }
    }
    else if(gc->pixel_width == 2)
    {
        if((color & 0xff) == 0xff)
        {
            // opaque color
            color = to_rgb16(gc, color);
            fill_rect_16(buf, gc->pitch, color, x, max_x, y, max_y);
        }
        else
        {
            // color with some transparency
            int x2;
            uint16_t *buf16;

            for( ; y < max_y; y++)
            {
                buf16 = (uint16_t *)buf;

                for(x2 = x; x2 < max_x; x2++)
                {
                    *buf16 = alpha_blend16(gc, color, *buf16);
                    buf16++;
                }

                buf += gc->pitch;
            }
        }
    }
    else if(gc->pixel_width == 3)
    {
        if((color & 0xff) == 0xff)
        {
            // opaque color
            color = to_rgb24(gc, color);
            fill_rect_24(buf, gc->pitch, color, x, max_x, y, max_y);
        }
        else
        {
            // color with some transparency
            int x2;
            uint8_t *buf8, tmp;

            for( ; y < max_y; y++)
            {
                buf8 = (uint8_t *)buf;

                for(x2 = x; x2 < max_x; x2++)
                {
                    tmp = buf8[0] | (buf8[1] << 8) | (buf8[2] << 16);
                    tmp = alpha_blend24(gc, color, tmp);
                    buf8[0] = (tmp & 0xff);
                    buf8[1] = (tmp >> 8) & 0xff;
                    buf8[2] = (tmp >> 16) & 0xff;
                    buf8 += 3;
                }

                buf += gc->pitch;
            }
        }
    }
    else
    {
        if((color & 0xff) == 0xff)
        {
            // opaque color
            color = to_rgb32(gc, color);
            fill_rect_32(buf, gc->pitch, color, x, max_x, y, max_y);
        }
        else
        {
            // color with some transparency
            int x2;
            uint32_t *buf32;

            for( ; y < max_y; y++)
            {
                buf32 = (uint32_t *)buf;

                for(x2 = x; x2 < max_x; x2++)
                {
                    *buf32 = alpha_blend32(gc, color, *buf32);
                    buf32++;
                }

                buf += gc->pitch;
            }
        }
    }
}


// Simple drawing a rectangle into a context
void gc_fill_rect_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                           int x, int y,  
                                           unsigned int width, 
                                           unsigned int height,
                                           uint32_t color)
{
    Rect *clip_area;
    Rect screen_area;

    // If there are clipping rects, draw the rect clipped to
    // each of them. Otherwise, draw unclipped (clipped to the screen)
    if(clipping->clip_rects && clipping->clip_rects->root)
    {
        for(clip_area = clipping->clip_rects->root;
            clip_area != NULL;
            clip_area = clip_area->next)
        {
            gc_clipped_rect(gc, x, y, x + width, y + height, clip_area, color);
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
            gc_clipped_rect(gc, x, y, x + width, y + height, 
                                &screen_area, color);
        }
    }
}

// A horizontal line as a filled rect of height 1
void gc_horizontal_line_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                                 int x, int y,
                                                 unsigned int length, 
                                                 uint32_t color)
{
    gc_fill_rect_clipped(gc, clipping, x, y, length, 1, color);
}

// A vertical line as a filled rect of width 1
void gc_vertical_line_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                               int x, int y,
                                               unsigned int length, 
                                               uint32_t color)
{
    gc_fill_rect_clipped(gc, clipping, x, y, 1, length, color);
}

// Rectangle drawing using our horizontal and vertical lines
void gc_draw_rect_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                           int x, int y, 
                                           unsigned int width, 
                                           unsigned int height,
                                           uint32_t color)
{
    gc_fill_rect_clipped(gc, clipping, x, y, width, 1, color);
    gc_fill_rect_clipped(gc, clipping, x, y + 1, 1, height - 2, color);
    gc_fill_rect_clipped(gc, clipping, x, y + height - 1, width, 1, color);
    gc_fill_rect_clipped(gc, clipping, x + width - 1, y + 1, 1, height - 2, color);
}


/*
 * Draw a single character with the specified font color at the 
 * specified coordinates.
 */
void __gc_draw_char_clipped(struct gc_t *gc, char character, int x, int y,
                            uint32_t color, Rect *bound_rect)
{
    struct font_t *font = gc->font ? gc->font : &__global_gui_data.mono;
    int destx = x;
    int desty = y;
    int srcx = 0;
    int srcy = 0;

    int charw = char_width(font, character);
    int charh = char_height(font, character);
    int destx2 = x + charw;
    int desty2 = y + charh;
    int srcx2 = charw;
    int srcy2 = charh;

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

    /*
     * NOTE: this calculation only works for monospace fonts.
     */
    uint8_t *chr = &font->data[character * charh];
    unsigned where = destx * gc->pixel_width + desty * gc->pitch;

    uint8_t *buf = (uint8_t *)(gc->buffer + where);
    uint8_t *buf2;
    int i, k, l;
    int k_init = (1 << (charw - 1 - srcx));

    if(gc->pixel_width == 1)
    {
        uint8_t col8 = to_rgb8(gc, color);

        for(l = srcy; l < srcy2; l++)
        {
            k = k_init;
            buf2 = buf;

            for(i = srcx; i < srcx2; i++)
            {
                if(chr[l] & k)
                {
                    *buf2 = col8;
                }

                buf2++;
                k >>= 1;
            }

            buf += gc->pitch;
        }
    }
    else if(gc->pixel_width == 2)
    {
        uint16_t col16 = to_rgb16(gc, color);

        for(l = srcy; l < srcy2; l++)
        {
            k = k_init;
            buf2 = buf;

            for(i = srcx; i < srcx2; i++)
            {
                if(chr[l] & k)
                {
                    *(uint16_t *)buf2 = col16;
                }

                buf2 += 2;
                k >>= 1;
            }

            buf += gc->pitch;
        }
    }
    else if(gc->pixel_width == 3)
    {
        uint32_t col24 = to_rgb24(gc, color);
        uint8_t b0 = col24 & 0xff;
        uint8_t b1 = ((col24 >> 8) & 0xff);
        uint8_t b2 = ((col24 >> 16) & 0xff);
        uint8_t *buf2;

        for(l = srcy; l < srcy2; l++)
        {
            k = k_init;
            buf2 = buf;

            for(i = srcx; i < srcx2; i++)
            {
                if(chr[l] & k)
                {
                    buf2[0] = b0;
                    buf2[1] = b1;
                    buf2[2] = b2;
                }

                buf2 += 3;
                k >>= 1;
            }

            buf += gc->pitch;
        }
    }
    else
    {
        uint32_t col32 = to_rgb32(gc, color);

        for(l = srcy; l < srcy2; l++)
        {
            k = k_init;
            buf2 = buf;

            for(i = srcx; i < srcx2; i++)
            {
                if(chr[l] & k)
                {
                    *(uint32_t *)buf2 = col32;
                }

                buf2 += 4;
                k >>= 1;
            }

            buf += gc->pitch;
        }
    }
}


/*
 * This will be a lot like context_fill_rect, but on a bitmap font character
 */
void gc_draw_char_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                           char character, int x, int y, 
                                           uint32_t color)
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
            __gc_draw_char_clipped(gc, character, x, y, color, clip_area);
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
            __gc_draw_char_clipped(gc, character, x, y, color, &screen_area);
        }
    }
}

/*
 * Draw a line of text with the specified font color at the specified
 * coordinates, using the given clipping info.
 */
void gc_draw_text_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                           char *string, int x, int y,
                                           uint32_t color, char accelerator)
{
    struct font_t *font = gc->font ? gc->font : &__global_gui_data.mono;

    if(!string || !*string)
    {
        return;
    }

    if(!accelerator)
    {
        for( ; *string; )
        {
            gc_draw_char_clipped(gc, clipping, *string, x, y, color);
            x += char_width(font, *string);
            string++;
        }
    }
    else
    {
        for( ; *string; )
        {
            if(string[0] == '&' && string[1] != '\0')
            {
                gc_horizontal_line_clipped(gc, clipping, x, 
                                               y + char_height(font, ' '),
                                               char_width(font, ' '), color);
                string++;
            }

            gc_draw_char_clipped(gc, clipping, *string, x, y, color);
            x += char_width(font, *string);
            string++;
        }
    }
}


void gc_blit(struct gc_t *dest_gc, struct gc_t *src_gc, int destx, int desty)
{
    uint8_t *dest = &dest_gc->buffer[(desty * dest_gc->pitch) + 
                                     (destx * dest_gc->pixel_width)];
    uint8_t *src = src_gc->buffer;
    int i;

    for(i = 0; i < src_gc->h; i++)
    {
        A_memcpy(dest, src, src_gc->pitch);
        dest += dest_gc->pitch;
        src += src_gc->pitch;
    }
}


/***********************************
 *
 * Functions for the world to use.
 *
 ***********************************/

void gc_fill_rect(struct gc_t *gc, int x, int y,  
                                   unsigned int width, unsigned int height,
                                   uint32_t color)
{
    gc_fill_rect_clipped(gc, &gc->clipping, x, y, width, height, color);
}


void gc_draw_rect(struct gc_t *gc, int x, int y, 
                                   unsigned int width, unsigned int height,
                                   uint32_t color)
{
    gc_draw_rect_clipped(gc, &gc->clipping, x, y, width, height, color);
}


void gc_horizontal_line(struct gc_t *gc, int x, int y,
                                         unsigned int length, uint32_t color)
{
    gc_horizontal_line_clipped(gc, &gc->clipping, x, y, length, color);
}


void gc_vertical_line(struct gc_t *gc, int x, int y,
                                       unsigned int length, uint32_t color)
{
    gc_vertical_line_clipped(gc, &gc->clipping, x, y, length, color);
}


void gc_draw_text(struct gc_t *gc, char *string, int x, int y,
                                   uint32_t color, char accelerator)
{
    if(gc->font && (gc->font->flags & FONT_FLAG_TRUE_TYPE))
    {
        gc_draw_text_clipped_ttf(gc, &gc->clipping, string, 
                                     x, y, color, accelerator);
    }
    else
    {
        gc_draw_text_clipped(gc, &gc->clipping, string, 
                                 x, y, color, accelerator);
    }
}


void gc_set_fontsize(struct gc_t *gc, int sz)
{
    // changing fontsize only makes sense if the font is NOT fixed width
    if(gc->font && gc->font->ft_face && (gc->font->flags & FONT_FLAG_TRUE_TYPE))
    {
        if(sz == gc->font->ptsz)
        {
            return;
        }

        FT_Size ftsize;

        if(FT_New_Size(gc->font->ft_face, &ftsize) == 0)
        {
            gc->font->ptsz = sz;

            if(gc->font->ftsize)
            {
                FT_Done_Size(gc->font->ftsize);
            }

            gc->font->ftsize = ftsize;
            FT_Activate_Size(ftsize);

            // arguments to FT_Set_Char_Size():
            //   font face,
            //   0 => char width in 1/64 of points, 0 means same as height,
            //   10 * 64 => char height in 1/64 of points,
            //   0 => horizontal device resolution, 0 means same as vertical,
            //   0 => vertical device resolution, 0 means default (72 dpi),

            FT_Set_Char_Size(gc->font->ft_face, 0, gc->font->ptsz * 64, 0, 0);
        }
    }
}


int gc_get_fontsize(struct gc_t *gc)
{
    return gc->font ? gc->font->ptsz : 0;
}


void gc_set_font(struct gc_t *gc, struct font_t *font)
{
    gc->font = font;
    //mutex_init(&gc->font->lock);
}


/*
void gc_get_font(struct gc_t *gc, struct font_t **font)
{
    *font = gc->font;
}
*/

