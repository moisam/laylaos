#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <gui/client/window.h>

#include "SDL_laylaosvideo.h"
#include "SDL_laylaosframebuffer.h"
#include "SDL_laylaosmodes.h"

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

        /* Create back buffer if it does not exist */
        if(!data->backbuffer) data->backbuffer = malloc(w->canvas_size);

        *pitch = w->canvas_pitch;
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

    /* Create the back buffer for double buffering.
     * If we fail, no problem, we just draw without double buffering (the result
     * might not pretty though with all the flickering!).
     */
    data->backbuffer = malloc(w->canvas_size);

    /* Get the pitch */
    *pitch = w->canvas_pitch;
    
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
    int i, j;
    int x, y, w, h;
    int bpp = win->gc->pixel_width;

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
        
        //memset(win->canvas, 0xff, win->canvas_size);

        // If there is a back buffer, copy from it to the cavas
        if(data->backbuffer)
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

        window_invalidate_rect(win, y, x, y + h - 1, x + w - 1);
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
