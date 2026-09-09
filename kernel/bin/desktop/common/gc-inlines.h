/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: gc-inlines.h
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
 *  \file gc-inlines.h
 *
 *  Common inlined functions used by gc drawing routines.
 */

static inline void __fill_pixel(struct gc_t *gc, int i, int j, uint32_t color, uint8_t alpha)
{
    unsigned where = j * gc->pixel_width + i * gc->pitch;
    uint8_t *buf = (uint8_t *)(gc->buffer + where);

    if(gc->pixel_width == 1)
    {
        *buf = alpha_blend8(gc, color | alpha, *buf);
    }
    else if(gc->pixel_width == 2)
    {
        *(uint16_t *)buf = alpha_blend16(gc, color | alpha, *(uint16_t *)buf);
    }
    else if(gc->pixel_width == 3)
    {
        uint32_t tmp = (uint32_t)buf[0] |
                       ((uint32_t)buf[1]) << 8 |
                       ((uint32_t)buf[2]) << 16;

        tmp = alpha_blend24(gc, color | alpha, tmp);
        buf[0] = tmp & 0xff;
        buf[1] = (tmp >> 8) & 0xff;
        buf[2] = (tmp >> 16) & 0xff;
    }
    else
    {
        *(uint32_t *)buf = alpha_blend32(gc, color | alpha, *(uint32_t *)buf);
    }
}


static inline void __prep_clipping_internal(struct gc_t *gc,
                                            struct clipping_t **clipping_out, 
                                            struct clipping_t *__clipping_in, 
                                            struct clipping_t *tmp_clipping, 
                                            RectList *tmp_clip_rects, Rect *tmp_screen_area)
{
    if(__clipping_in && __clipping_in->clip_rects && __clipping_in->clip_rects->root)
    {
        *clipping_out = __clipping_in;
    }
    else
    {
        *clipping_out = tmp_clipping;
        tmp_clipping->clipping_on = 0;
        tmp_clipping->clip_rects = tmp_clip_rects;

        if(!__clipping_in || !__clipping_in->clipping_on)
        {
            tmp_clip_rects->root = tmp_screen_area;
            tmp_screen_area->top = 0;
            tmp_screen_area->left = 0;
            tmp_screen_area->bottom = gc->h - 1;
            tmp_screen_area->right = gc->w - 1;
            tmp_screen_area->next = NULL;
        }
        else
        {
            tmp_clip_rects->root = NULL;
        }
    }
}

