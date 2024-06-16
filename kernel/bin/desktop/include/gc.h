/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gc.h
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
 *  \file gc.h
 *
 *  Declarations and struct definitions for working with graphical contexts
 *  on both client and server sides.
 */

#ifndef GUI_GC_H
#define GUI_GC_H

#include <inttypes.h>
#include <pthread.h>
#include "list.h"
#include "screen.h"
#include "bitmap.h"

#ifdef __cplusplus
extern "C" {
#endif

// prototypes for optimized memory functions from asmlib.a
// Library source can be downloaded from:
//     https://www.agner.org/optimize/#asmlib
void * A_memcpy(void * dest, const void * src, size_t count);
void * A_memset(void * dest, int c, size_t count);


/**********************
 * Struct definitions
 **********************/

struct clipping_t
{
    //List *clip_rects;
    RectList *clip_rects;
    uint8_t clipping_on;
};


struct gc_t
{
    uint8_t *buffer;
    uint32_t buffer_size;
    uint32_t pitch;
    uint32_t w;
    uint32_t h;
    uint8_t pixel_width;
    struct screen_t *screen;

//#ifndef GUI_SERVER
    //List *clip_rects;
    //uint8_t clipping_on;
    struct clipping_t clipping;
//#endif      /* !GUI_SERVER */
    
    mutex_t lock;

    struct font_t *font;
};


/**********************
 * Inlines
 **********************/

static inline void gc_get_clipping(struct gc_t *gc, struct clipping_t *clipping)
{
    clipping->clip_rects = gc->clipping.clip_rects;
    clipping->clipping_on = gc->clipping.clipping_on;
}

static inline void gc_set_clipping(struct gc_t *gc, struct clipping_t *clipping)
{
    gc->clipping.clip_rects = clipping->clip_rects;
    gc->clipping.clipping_on = clipping->clipping_on;
}


/**********************
 * Internal functions
 **********************/

void gc_draw_char_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                           char character, int x, int y,
                                           uint32_t color);
void gc_draw_text_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                           char *string, int x, int y,
                                           uint32_t color, char accelerator);
void gc_draw_text_clipped_ttf(struct gc_t *gc, struct clipping_t *clipping,
                                               char *string, int x, int y,
                                               uint32_t color,
                                               char accelerator);

/**********************
 * Public functions
 **********************/

#include "rect.h"

struct gc_t *gc_new(uint16_t width, uint16_t height, uint8_t pixel_width,
                    uint8_t *buffer, uint32_t buffer_size, uint32_t pitch,
                    struct screen_t *screen);

void gc_fill_rect(struct gc_t *gc, int x, int y,  
                                   unsigned int w, unsigned int h,
                                   uint32_t color);
void gc_draw_rect(struct gc_t *gc, int x, int y, 
                                   unsigned int width, unsigned int height,
                                   uint32_t color);
void gc_horizontal_line(struct gc_t *gc, int x, int y,
                                         unsigned int length, uint32_t color);
void gc_vertical_line(struct gc_t *gc, int x, int y,
                                       unsigned int length, uint32_t color);

void gc_draw_text(struct gc_t *gc, char *string, int x, int y,
                                   uint32_t color, char accelerator);

void gc_blit_bitmap_highlighted(struct gc_t *gc, 
                                struct bitmap32_t *bitmap,
                                int destx, int desty,
                                int offsetx, int offsety,
                                unsigned int width, unsigned int height,
                                uint32_t hicolor);

void gc_blit_bitmap(struct gc_t *gc, struct bitmap32_t *bitmap,
                                     int destx, int desty,
                                     int offsetx, int offsety,
                                     unsigned int width, unsigned int height);

void gc_blit_icon_highlighted(struct gc_t *gc, struct bitmap32_array_t *ba,
                                               int destx, int desty,
                                               int offsetx, int offsety,
                                               unsigned int width, unsigned int height,
                                               uint32_t hicolor);

void gc_blit_icon(struct gc_t *gc, struct bitmap32_array_t *ba,
                                   int destx, int desty,
                                   int offsetx, int offsety,
                                   unsigned int width, unsigned int height);

void gc_stretch_bitmap_highlighted(struct gc_t *gc, 
                                   struct bitmap32_t *bitmap,
                                   int destx, int desty,
                                   unsigned int destw, unsigned int desth,
                                   int isrcx, int isrcy,
                                   unsigned int srcw, unsigned int srch,
                                   uint32_t hicolor);

void gc_stretch_bitmap(struct gc_t *gc, 
                       struct bitmap32_t *bitmap,
                       int destx, int desty,
                       unsigned int destw, unsigned int desth,
                       int isrcx, int isrcy,
                       unsigned int srcw, unsigned int srch);

void gc_blit(struct gc_t *dest_gc, struct gc_t *src_gc, int destx, int desty);

void gc_set_font(struct gc_t *gc, struct font_t *font);
//void gc_get_font(struct gc_t *gc, struct font_t **font);

void gc_set_fontsize(struct gc_t *gc, int sz);
int gc_get_fontsize(struct gc_t *gc);


#ifdef GUI_SERVER

/*************************
 * Server-side functions
 *************************/

#include "server/window.h"

void gc_clipped_window(struct gc_t *gc, struct server_window_t *window,
                                        int x, int y, int max_x, int max_y,
                                        Rect *clip_area);
void gc_copy_window(struct gc_t *gc, struct server_window_t *window);

#else       /* !GUI_SERVER */

/**********************************
 * Internal client-side functions
 **********************************/

void gc_circle_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                       int xc, int yc, int radius, int thickness, 
                       uint32_t color);
void gc_circle_filled_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                              int xc, int yc, int radius, uint32_t color);
void gc_arc_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                    int xc, int yc, int radius, int angle1, int angle2,
                    int thickness, uint32_t color);
void gc_line_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                     int x1, int y1, int x2, int y2,
                     int __thickness, uint32_t color);
void gc_draw_rect_thick_clipped(struct gc_t *gc, struct clipping_t *clipping,
                                int x, int y, 
                                unsigned int width, unsigned int height,
                                int thickness, uint32_t color);
void gc_polygon_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                        int *vertices, int nvertex,
                        int thickness, uint32_t color);
void gc_polygon_fill_clipped(struct gc_t *gc, struct clipping_t *__clipping,
                             int *vertices, int nvertex, uint32_t color);

/********************************
 * Public client-side functions
 ********************************/

int gc_alloc_backbuf(struct gc_t *orig_gc, struct gc_t *backbuf_gc,
                     int w, int h);
int gc_realloc_backbuf(struct gc_t *orig_gc, struct gc_t *backbuf_gc,
                       int w, int h);

void gc_circle(struct gc_t *gc, int xc, int yc,
                                int radius, int thickness, uint32_t color);
void gc_circle_filled(struct gc_t *gc, int xc, int yc,
                                       int radius, uint32_t color);

void gc_arc(struct gc_t *gc, int xc, int yc,
                             int radius, int angle1, int angle2,
                             int thickness, uint32_t color);

void gc_polygon(struct gc_t *gc, int *vertices, int nvertex,
                                 int thickness, uint32_t color);
void gc_polygon_fill(struct gc_t *gc, int *vertices, int nvertex,
                                      uint32_t color);

void gc_line(struct gc_t *gc, int x1, int y1, int x2, int y2,
                              int __thickness, uint32_t color);
void gc_draw_rect_thick(struct gc_t *gc, int x, int y, 
                                         unsigned int width,
                                         unsigned int height,
                                         int thickness, uint32_t color);

#endif      /* GUI_SERVER */

#ifdef __cplusplus
}
#endif

#endif      /* GUI_GC_H */
