#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <gui/client/window.h>

#include "SDL_laylaosvideo.h"
#include "SDL_laylaosframebuffer.h"
#include "SDL_laylaosmodes.h"

/*
 * Well behaved applications have no problem running regardless of the 
 * framebuffer format, in which case we can simply copy data directly to 
 * the front buffer. Others, however, assume the default ARGB32 format
 * and draw directly to the backbuffer. In the best case scenario, this
 * will lead to fudged images on the screen. In the worst case scenario,
 * this will lead to a SIGSEGV. To avoid this, we create an ARGB32 backbuffer
 * in all cases, then direct copy or blit to the front buffer as required.
 */
static int alloc_backbuffer(SDL_WindowData *data, struct window_t *w, int *pitch)
{
    size_t sz = w->w * w->h * 4;

    /* Free old back buffer if it exists */
    if(data->backbuffer)
    {
        free(data->backbuffer);
        data->backbuffer = NULL;
    }

    /* Create new back buffer */
    data->backbuffer = malloc(sz);
    *pitch = w->w * 4;

    return !!(data->backbuffer);
}

int
LAYLAOS_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format,
                                void **pixels, int *pitch)
{
    SDL_WindowData *data = (SDL_WindowData *)(window->driverdata);
    struct window_t *w = data->xwindow;
    
    if(w->canvas && w->canvas_pitch && w->w == window->w && w->h == window->h)
    {
        if((*format = LAYLAOS_GetPixelFormat()) == SDL_PIXELFORMAT_UNKNOWN)
        {
            return SDL_SetError("Unknown window pixel format");
        }

        /* Create new back buffer */
        if(!alloc_backbuffer(data, w, pitch))
        {
            return SDL_SetError("Couldn't create back buffer");
        }

        *pixels = data->backbuffer ? data->backbuffer : w->canvas;
        return 0;
    }

    /* Free the old framebuffer surface */
    LAYLAOS_DestroyWindowFramebuffer(_this, window);
    
    /* Create the canvas for drawing */
    if(!window_new_canvas(w))
    {
        return SDL_SetError("Couldn't create new canvas");
    }
    
    /* Find out the pixel format and depth */
    if((*format = LAYLAOS_GetPixelFormat()) == SDL_PIXELFORMAT_UNKNOWN)
    {
        return SDL_SetError("Unknown window pixel format");
    }

    /* Create the back buffer for double buffering */
    if(!alloc_backbuffer(data, w, pitch))
    {
        return SDL_SetError("Couldn't create back buffer");
    }
    
    /* And the canvas */
    *pixels = data->backbuffer ? data->backbuffer : w->canvas;
    
    return 0;
}

int
LAYLAOS_UpdateWindowFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects,
                                int numrects)
{
    SDL_WindowData *data = (SDL_WindowData *)(window->driverdata);
    struct window_t *win = data->xwindow;
    struct bitmap32_t bitmap = { 0, };
    int i, j, k;
    int x, y, w, h;
    int bpp = win->gc->pixel_width;
    size_t tmpsz = 0;
    int direct_copy = (bpp == 4);

    if(!data->backbuffer)
    {
        window_invalidate(win);
        return 0;
    }

    for(i = 0; i < numrects; ++i)
    {
        x = rects[i].x;
        y = rects[i].y;
        w = rects[i].w;
        h = rects[i].h;

        if(w <= 0 || h <= 0 || (x + w) <= 0 || (y + h) <= 0)
        {
            /* Clipped? */
            continue;
        }

        if(x < 0)
        {
            x += w;
            w += rects[i].x;
        }

        if(y < 0)
        {
            y += h;
            h += rects[i].y;
        }

        if(x + w > window->w)
        {
            w = window->w - x;
        }

        if(y + h > window->h)
        {
            h = window->h - y;
        }

        // If the buffer is 32 bits, copy directly.
        // Otherwise we have to let libgui blit the image to the front buffer.
        if(direct_copy)
        {
            uint8_t *src = data->backbuffer + (y * win->canvas_pitch) + (x * bpp);
            uint8_t *dst = win->canvas + (y * win->canvas_pitch) + (x * bpp);
            size_t bytes = w * bpp;

            for(j = 0; j < h; j++)
            {
                __builtin_memcpy(dst, src, bytes);
                src += win->canvas_pitch;
                dst += win->canvas_pitch;
            }
        }
        else
        {
            uint32_t *d, *s;

            if(bitmap.data == NULL)
            {
                tmpsz = w * h * 4;

                if(!(bitmap.data = malloc(tmpsz)))
                {
                    return SDL_SetError("Coulnd't alloc temp memory");
                }
            }
            else if((w * h * 4) > tmpsz)
            {
                tmpsz = w * h * 4;

                if(!(bitmap.data = realloc(bitmap.data, tmpsz)))
                {
                    return SDL_SetError("Coulnd't realloc temp memory");
                }
            }

            s = (uint32_t *)data->backbuffer + (y * win->w) + x;
            d = bitmap.data;

            bitmap.width = w;
            bitmap.height = h;

            for(j = 0; j < h; j++)
            {
                for(k = 0; k < w; k++)
                {
                    *d = (*s << 8) | 0xff;
                    d++;
                    s++;
                }

                s += (win->w - w);
            }

            gc_blit_bitmap(win->gc, &bitmap, x, y, 0, 0, w, h);
        }

        window_invalidate_rect(win, y, x, y + h - 1, x + w - 1);
    }

    if(bitmap.data)
    {
        free(bitmap.data);
    }

    return 0;
}

void
LAYLAOS_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    struct window_t *w = data ? data->xwindow : NULL;

    if(!data || !w)
    {
        /* The window wasn't fully initialized */
        return;
    }

    // destroy the front buffer
    window_destroy_canvas(w);

    // and the back buffer
    if(data->backbuffer)
    {
        free(data->backbuffer);
        data->backbuffer = NULL;
    }
}


#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
