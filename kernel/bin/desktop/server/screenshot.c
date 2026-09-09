/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: screenshot.c
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
 *  \file screenshot.c
 *
 *  Functions to work with screenshots on the server side.
 */

#define GUI_SERVER
#include <emmintrin.h>
#include "../include/gui.h"
#include "../include/rgb.h"
#include "../include/server/event.h"
#include "../include/server/cursor.h"

#define GLOB        __global_gui_data

static void send_to_client(uint32_t *tmpbuf, size_t total, size_t chunksz, int clientfd, struct event_t *ev)
{
    if(total < chunksz - 4)
    {
        size_t bufsz = sizeof(struct event_buf_t) + total + 4;
        char cbuf[bufsz];
        struct event_buf_t *evbuf = (struct event_buf_t *)cbuf;

        A_memset(evbuf, 0, sizeof(struct event_buf_t));
        A_memcpy(evbuf->buf + 4, tmpbuf, total);
        evbuf->buf[0] = 'D';
        evbuf->buf[1] = 'A';
        evbuf->buf[2] = 'T';
        evbuf->buf[3] = 'A';

        evbuf->bufsz = total + 4;
        evbuf->seqid = ev->seqid;
        evbuf->type = EVENT_SCREENSHOT_DATA;
        evbuf->src = TO_WINID(GLOB.mypid, 0);
        evbuf->dest = ev->src;
        evbuf->valid_reply = 1;

        write(clientfd, evbuf, bufsz);
    }
    else
    {
        char tmpname[] = "/tmp/scrnshotXXXXXX";
        size_t bufsz = sizeof(struct event_buf_t) + sizeof(tmpname) + 4;
        char cbuf[bufsz];
        struct event_buf_t *evbuf = (struct event_buf_t *)cbuf;
        int fd = mkstemp(tmpname);

        if(fd < 0)
        {
            free(tmpbuf);
            send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
            return;
        }

        write(fd, tmpbuf, total);
        close(fd);

        A_memset(evbuf, 0, sizeof(struct event_buf_t));
        A_memcpy(evbuf->buf + 4, tmpname, sizeof(tmpname));
        evbuf->buf[0] = 'P';
        evbuf->buf[1] = 'A';
        evbuf->buf[2] = 'T';
        evbuf->buf[3] = 'H';

        evbuf->bufsz = sizeof(tmpname) + 4;
        evbuf->seqid = ev->seqid;
        evbuf->type = EVENT_SCREENSHOT_DATA;
        evbuf->src = TO_WINID(GLOB.mypid, 0);
        evbuf->dest = ev->src;
        evbuf->valid_reply = 1;

        write(clientfd, evbuf, bufsz);
    }
}

static void get_window_screenshot(struct gc_t *gc, int clientfd, struct event_t *ev)
{
    struct server_window_t *win;
    struct gc_t srcgc, destgc;
    struct screen_t destscreen;
    int x = ev->screenshot.x;
    int y = ev->screenshot.y;
    int w = ev->screenshot.w;
    int h = ev->screenshot.h;
    size_t chunksz = (ev->screenshot.chunksz) & ~3; // ensure 4-byte aligned
    size_t total, totaltosend;
    uint32_t *paintbuf, *sendbuf;

    if(x < 0 || y < 0 || chunksz == 0)
    {
        send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
        return;
    }

    if(!(win = server_window_by_winid(ev->screenshot.winid)))
    {
        send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
        return;
    }

    int hasframe = !(win->flags & WINDOW_NODECORATION);

    if(x + w > win->w)
    {
        w = win->w - x;
    }

    if(y + h > win->h)
    {
        h = win->h - y;
    }

    if(w <= 0 || h <= 0)
    {
        send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
        return;
    }

    totaltosend = w * h * 4;
    total = win->w * win->h * 4;

    if(!(paintbuf = malloc(total)))
    {
        send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
        return;
    }

    if(!(sendbuf = malloc(totaltosend)))
    {
        free(paintbuf);
        send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
        return;
    }

    srcgc.w = win->client_w;
    srcgc.h = win->client_h; 
    srcgc.screen = gc->screen;
    srcgc.pixel_width = gc->pixel_width;
    srcgc.pitch = win->canvas_pitch;
    srcgc.buffer = win->canvas;
    srcgc.buffer_size = win->canvas_size;
    srcgc.clipping.clip_rects = NULL;
    srcgc.clipping.clipping_on = 0;

    destgc.w = win->w;
    destgc.h = win->h; 
    destgc.screen = &destscreen;
    destgc.pixel_width = 4;
    destgc.pitch = (destgc.w * destgc.pixel_width);
    destgc.buffer = (uint8_t *)paintbuf;
    destgc.buffer_size = destgc.pitch * destgc.h;
    destgc.clipping.clip_rects = NULL;
    destgc.clipping.clipping_on = 0;

    destscreen.pixel_width = 4;
    destscreen.red_pos = 24;
    destscreen.green_pos = 16;
    destscreen.blue_pos = 8;
    destscreen.rgb_mode = 1;

    if(hasframe)
    {
        int saved_winx = win->x;
        int saved_winy = win->y;

        win->x = 0;
        win->y = 0;
        server_window_draw_border(&destgc, win);

        win->x = saved_winx;
        win->y = saved_winy;
    }

    if(gc_copy_part_gc(&destgc, &srcgc,
                       hasframe ? WINDOW_BORDERWIDTH : 0,  /* dx */
                       hasframe ? WINDOW_TITLEHEIGHT : 0,  /* dy */
                       0,                                  /* sx */
                       0,                                  /* sy */
                       win->client_w, win->client_h, 0) < 0)
    {
        FILE *f = fopen("/root/ttt", "a+");
        fprintf(f, "gc_copy_part_gc err\n");
        fclose(f);
    }

    // now crop the requested pixels
    size_t rowbytes = w * 4;
    char *s = (char *)paintbuf + (y * destgc.pitch) + (x * destgc.pixel_width);
    char *d = (char *)sendbuf;
    uint32_t *d32 = sendbuf;
    int i;

    for(i = 0; i < h; i++)
    {
        A_memcpy(d, s, rowbytes);
        s += destgc.pitch;
        d += rowbytes;
    }

    // add the alpha channel
    for(i = 0; i < w * h; i++)
    {
        *d32 = (*d32) | 0xff;
        d32++;
    }

    // send to the client
    send_to_client(sendbuf, totaltosend, chunksz, clientfd, ev);

    free(sendbuf);
    free(paintbuf);
}

void server_screenshot_get(struct gc_t *gc, int clientfd, struct event_t *ev)
{
    if(ev->screenshot.winid != 0)
    {
        get_window_screenshot(gc, clientfd, ev);
        return;
    }

    int x = ev->screenshot.x;
    int y = ev->screenshot.y;
    int w = ev->screenshot.w;
    int h = ev->screenshot.h;
    int loopx, loopy;
    size_t chunksz = (ev->screenshot.chunksz) & ~3; // ensure 4-byte aligned
    size_t total;
    uint32_t *dest, *tmpbuf;

    if(x < 0 || y < 0 || chunksz == 0)
    {
        send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
        return;
    }

    if(x + w > gc->w)
    {
        w = gc->w - x;
    }

    if(y + h > gc->h)
    {
        h = gc->h - y;
    }

    uint32_t r_pos = gc->screen->red_pos;
    uint32_t g_pos = gc->screen->green_pos;
    uint32_t b_pos = gc->screen->blue_pos;

    if(w <= 0 || h <= 0)
    {
        send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
        return;
    }

    total = w * h * 4;

    if(!(tmpbuf = malloc(total)))
    {
        send_err_event(clientfd, ev->src, EVENT_SCREENSHOT_DATA, EINVAL, ev->seqid);
        return;
    }

    dest = tmpbuf;

    // disable desktop updates while we grab the screenshot
    mutex_lock(&update_lock);

    // hide mouse cursor
    struct cursor_t *cur = cursor[cur_cursor];
    struct cursor_bitmap_t *curbitmap = cur ? &cur->bitmaps[cur->curframe] : NULL;

    if(curbitmap)
    {
        int my = root_mouse_y - curbitmap->hoty;
        int mx = root_mouse_x - curbitmap->hotx;

        RectList dirty_list;
        Rect mouse_rect;

        dirty_list.root = &mouse_rect;
        dirty_list.last = &mouse_rect;

        mouse_rect.top = my;
        mouse_rect.left = mx;
        mouse_rect.bottom = my + curbitmap->h - 1;
        mouse_rect.right = mx + curbitmap->w - 1;
        mouse_rect.next = NULL;

        server_window_paint(gc, root_window, NULL, 
                            &dirty_list, 
                            FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);
    }

    // convert raw framebuffer pixels to RGBA
    if(gc->pixel_width == 1)
    {
        struct rgba_color_t *palette = gc->screen->palette;

        for(loopy = y; loopy < y + h; loopy++)
        {
            uint8_t *src = (uint8_t *)(gc->buffer + (x + loopy * gc->pitch));

            for(loopx = 0; loopx < w; loopx++)
            {
                uint8_t idx = *src;
                uint32_t r = palette[idx].red;
                uint32_t g = palette[idx].green;
                uint32_t b = palette[idx].blue;

                *dest++ = (r << 24) | (g << 16) | (b << 8) | 0xFF;
                src++;
            }
        }
    }
    else if(gc->pixel_width == 2)
    {
        uint32_t r_mask = (1 << gc->screen->red_mask_size) - 1;
        uint32_t g_mask = (1 << gc->screen->green_mask_size) - 1;
        uint32_t b_mask = (1 << gc->screen->blue_mask_size) - 1;

        for(loopy = y; loopy < y + h; loopy++)
        {
            uint16_t *src = (uint16_t *)(gc->buffer + (x * 2 + loopy * gc->pitch));

            for(loopx = 0; loopx < w; loopx++)
            {
                uint16_t p = *src;
                
                uint32_t r = (((p >> r_pos) & r_mask) * 0xFF) / r_mask;
                uint32_t g = (((p >> g_pos) & g_mask) * 0xFF) / g_mask;
                uint32_t b = (((p >> b_pos) & b_mask) * 0xFF) / b_mask;

                *dest++ = (r << 24) | (g << 16) | (b << 8) | 0xFF;
                src++;
            }
        }
    }
    else if(gc->pixel_width == 3)
    {
        for(loopy = y; loopy < y + h; loopy++)
        {
            uint8_t *src = (uint8_t *)(gc->buffer + (x * 3 + loopy * gc->pitch));

            for(loopx = 0; loopx < w; loopx++)
            {
                uint32_t p = src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16);
                
                uint32_t r = (p >> r_pos) & 0xFF;
                uint32_t g = (p >> g_pos) & 0xFF;
                uint32_t b = (p >> b_pos) & 0xFF;
                
                *dest++ = (r << 24) | (g << 16) | (b << 8) | 0xFF;
                src += 3;
            }
        }
    }
    else
    {
        __m128i m_red   = _mm_set1_epi32(0xFF << r_pos);
        __m128i m_green = _mm_set1_epi32(0xFF << g_pos);
        __m128i m_blue  = _mm_set1_epi32(0xFF << b_pos);
        __m128i v_alpha = _mm_set1_epi32(0x000000FF);

        for(loopy = y; loopy < y + h; loopy++)
        {
            uint32_t *src = (uint32_t *)(gc->buffer + (x * 4 + loopy * gc->pitch));

            for(loopx = 0; loopx <= w - 4; loopx += 4)
            {
                __m128i pixels = _mm_loadu_si128((const __m128i *)&src[loopx]);

                __m128i r = _mm_srli_epi32(_mm_and_si128(pixels, m_red), r_pos);
                __m128i g = _mm_srli_epi32(_mm_and_si128(pixels, m_green), g_pos);
                __m128i b = _mm_srli_epi32(_mm_and_si128(pixels, m_blue), b_pos);

                __m128i final_rgba = _mm_or_si128(
                    _mm_or_si128(_mm_slli_epi32(r, 24), _mm_slli_epi32(g, 16)),
                    _mm_or_si128(_mm_slli_epi32(b, 8), v_alpha)
                );

                _mm_storeu_si128((__m128i *)&dest[loopx], final_rgba);
            }

            for(; loopx < w; loopx++)
            {
                uint32_t p = src[loopx];
                uint32_t r = (p >> r_pos) & 0xFF;
                uint32_t g = (p >> g_pos) & 0xFF;
                uint32_t b = (p >> b_pos) & 0xFF;
                dest[loopx] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
            }

            dest += w;
        }
    }

    // enable desktop updates
    mutex_unlock(&update_lock);

    // redraw mouse cursor
    draw_mouse_cursor(1);

    // send to the client
    send_to_client(tmpbuf, total, chunksz, clientfd, ev);

    free(tmpbuf);
}

