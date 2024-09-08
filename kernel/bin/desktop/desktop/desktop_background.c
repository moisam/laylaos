/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: desktop_background.c
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
 *  \file desktop_background.c
 *
 *  The desktop background routines to fill the desktop with a given color, or
 *  load an image as the desktop background.
 */

#include <sys/stat.h>
#include "../include/client/window.h"
#include "../include/resources.h"
#include "desktop.h"

#define GLOB                    __global_gui_data

int background_is_image = 0;
uint32_t background_color = 0x16A085FF;
char *background_image_path = NULL;
int background_image_aspect = 0;

struct gc_t background_bitmap_gc;


void load_desktop_background(void)
{
    char *ext;
    uint8_t *rendered_image;
    unsigned int x, y, w, h;
    unsigned int left_margin, top_margin;
    struct bitmap32_t new_bitmap;
    struct stat stats;

    if(!background_image_path || !*background_image_path)
    {
        return;
    }

    if(stat(background_image_path, &stats) == -1)
    {
        return;
    }

    ext = file_extension(background_image_path);
    
    if(strcasecmp(ext, ".png") == 0)
    {
        if(!png_load(background_image_path, &new_bitmap))
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            return;
        }
    }
    else if(strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".jpg") == 0)
    {
        if(!jpeg_load(background_image_path, &new_bitmap))
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            return;
        }
    }
    else
    {
        // unsupported image format
        return;
    }

    w = desktop_window->gc->w;
    h = desktop_window->gc->h;

    // render the bitmap with the requested scaling, so later we only need
    // to copy it (or parts of it, as needed)
    if(!(rendered_image = malloc(desktop_window->gc->buffer_size)))
    {
        free(new_bitmap.data);
        return;
    }

    // free the old bitmap
    if(background_bitmap_gc.buffer)
    {
        free(background_bitmap_gc.buffer);
    }

    // fill the background graphic context so we can use the image 
    // copying/stretching functions
    background_bitmap_gc.clipping.clip_rects = NULL;
    background_bitmap_gc.clipping.clipping_on = 0;
    background_bitmap_gc.w = w; 
    background_bitmap_gc.h = h;
    background_bitmap_gc.buffer = rendered_image;
    background_bitmap_gc.buffer_size = desktop_window->gc->buffer_size;
    background_bitmap_gc.pitch = desktop_window->gc->pitch;
    background_bitmap_gc.pixel_width = desktop_window->gc->pixel_width;
    background_bitmap_gc.screen = desktop_window->gc->screen;

    // calculate potential margins that can happen if the selected scaling
    // is either scaled (fit-to-screen) or centered (fit-image)
    left_margin = 0;
    top_margin = 0;

    if(background_image_aspect == DESKTOP_BACKGROUND_SCALED)
    {
        float vaspect = (float)new_bitmap.width / new_bitmap.height;
        unsigned int dw = w;
        unsigned int dh = w / vaspect;

        if(dh > h)
        {
            dh = h;
            dw = h * vaspect;
        }

        left_margin = (w - dw) / 2;
        top_margin = (h - dh) / 2;
    }
    else if(background_image_aspect == DESKTOP_BACKGROUND_CENTERED)
    {
        left_margin = (w > new_bitmap.width) ? (w - new_bitmap.width) / 2 : 0;
        top_margin = (h > new_bitmap.height) ? (h - new_bitmap.height) / 2 : 0;
    }

    // black out the left & right margins if present
    if(left_margin)
    {
        gc_fill_rect(&background_bitmap_gc, 0, 0, left_margin, h, 0x000000FF);
        gc_fill_rect(&background_bitmap_gc, w - left_margin, 0, left_margin, h, 0x000000FF);
    }

    // black out the top & bottom margins if present
    if(top_margin)
    {
        gc_fill_rect(&background_bitmap_gc, 0, 0, w, top_margin, 0x000000FF);
        gc_fill_rect(&background_bitmap_gc, 0, h - top_margin, w, top_margin, 0x000000FF);
    }

    // now render according to the given scaling
    switch(background_image_aspect)
    {
        case DESKTOP_BACKGROUND_TILES:
            for(y = 0; y < h; y += new_bitmap.height)
            {
                for(x = 0; x < w; x += new_bitmap.width)
                {
                    gc_blit_bitmap(&background_bitmap_gc, &new_bitmap,
                                   x, y,
                                   0, 0,
                                   new_bitmap.width, new_bitmap.height);
                }
            }
            break;

        case DESKTOP_BACKGROUND_SCALED:
            gc_stretch_bitmap(&background_bitmap_gc, &new_bitmap,
                              left_margin, top_margin,
                              w - (left_margin * 2), h - (top_margin * 2),
                              0, 0,
                              new_bitmap.width, new_bitmap.height);
            break;

        case DESKTOP_BACKGROUND_STRETCHED:
            gc_stretch_bitmap(&background_bitmap_gc, &new_bitmap,
                              0, 0,
                              w, h,
                              0, 0,
                              new_bitmap.width, new_bitmap.height);
            break;

        case DESKTOP_BACKGROUND_ZOOMED:
        {
            float vaspect = (float)new_bitmap.width / new_bitmap.height;
            unsigned int dw = w;
            unsigned int dh = w / vaspect;

            if(dh < h)
            {
                dh = h;
                dw = h * vaspect;
            }

            x = ((int)w - (int)dw) / 2;
            y = ((int)h - (int)dh) / 2;
            gc_stretch_bitmap(&background_bitmap_gc, &new_bitmap,
                              x, y,
                              dw, dh,
                              0, 0,
                              new_bitmap.width, new_bitmap.height);
            break;
        }

        case DESKTOP_BACKGROUND_CENTERED:
        default:
            x = ((int)w - (int)new_bitmap.width) / 2;
            y = ((int)h - (int)new_bitmap.height) / 2;
            gc_blit_bitmap(&background_bitmap_gc, &new_bitmap,
                           x, y,
                           0, 0,
                           new_bitmap.width, new_bitmap.height);
            break;
    }

    // free the loaded bitmap
    free(new_bitmap.data);
}


void draw_desktop_background(void)
{
    // Background is color, or it is image but no image is present
    if(!background_is_image || !background_bitmap_gc.buffer)
    {
        gc_fill_rect(desktop_window->gc, 0, 0,
                     desktop_window->w, desktop_window->h, background_color);
        return;
    }

    // Background is image - copy the rendered bitmap
    gc_blit(desktop_window->gc, &background_bitmap_gc, 0, 0);
}


void redraw_desktop_background(int x, int y, int w, int h)
{
    // Background is color, or it is image but no image is present
    if(!background_is_image || !background_bitmap_gc.buffer)
    {
        gc_fill_rect(desktop_window->gc, x, y, w, h, background_color);
        return;
    }

    // Background is image - copy from the rendered bitmap
    int i;
    size_t off = (y * desktop_window->gc->pitch) +
                 (x * desktop_window->gc->pixel_width);
    size_t bytes = w * desktop_window->gc->pixel_width;
    uint8_t *src = background_bitmap_gc.buffer + off;
    uint8_t *dest = desktop_window->gc->buffer + off;

    for(i = 0; i < h; i++)
    {
        A_memcpy(dest, src, bytes);
        dest += desktop_window->gc->pitch;
        src += desktop_window->gc->pitch;
    }
}

