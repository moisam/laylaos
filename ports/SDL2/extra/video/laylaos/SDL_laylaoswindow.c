#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <gui/mouse.h>
#include <gui/kbd.h>
#include <gui/client/window.h>
#include "SDL_laylaoswindow.h"
#include "SDL_laylaosvideo.h"
#include "SDL_assert.h"

#include "SDL_syswm.h"
#include "../../events/SDL_keyboard_c.h"

#define STYLE_FULLSCREEN    (WINDOW_NODECORATION | WINDOW_NOCONTROLBOX | WINDOW_NOICON)
#define STYLE_BORDERLESS    (WINDOW_NODECORATION | WINDOW_NOCONTROLBOX | WINDOW_NOICON)


static uint32_t
GetWindowStyle(SDL_Window * window)
{
    uint32_t style = 0;

    if(window->flags & SDL_WINDOW_FULLSCREEN)
    {
        style |= STYLE_FULLSCREEN;
    }
    else if(window->flags & SDL_WINDOW_BORDERLESS)
    {
        style |= STYLE_BORDERLESS;
    }

    if(window->flags & SDL_WINDOW_ALWAYS_ON_TOP)
    {
        style |= WINDOW_ALWAYSONTOP;
    }

    if(window->flags & SDL_WINDOW_SKIP_TASKBAR)
    {
        style |= WINDOW_SKIPTASKBAR;
    }

    return style;
}


static int
SetupWindowData(_THIS, SDL_Window *window, struct window_t *w, SDL_bool created)
{
    SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;
    SDL_WindowData *data;
    int numwindows = videodata->numwindows;
    int windowlistlength = videodata->windowlistlength;
    SDL_WindowData **windowlist = videodata->windowlist;

    /* Allocate the window data */
    data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));

    if(!data)
    {
        return SDL_OutOfMemory();
    }

    data->window = window;
    data->xwindow = w;
    data->created = created;
    data->videodata = videodata;

    /* Associate the data with the window */

    if(numwindows < windowlistlength)
    {
        windowlist[numwindows] = data;
        videodata->numwindows++;
    }
    else
    {
        windowlist =
            (SDL_WindowData **) SDL_realloc(windowlist,
                                            (numwindows +
                                             1) * sizeof(*windowlist));
        if(!windowlist)
        {
            SDL_free(data);
            return SDL_OutOfMemory();
        }
        windowlist[numwindows] = data;
        videodata->numwindows++;
        videodata->windowlistlength++;
        videodata->windowlist = windowlist;
    }

    /* Fill in the SDL window with the window data */
    window->x = w->x;
    window->y = w->y;
    window->w = w->w;
    window->h = w->h;
    
    if(w->flags & WINDOW_HIDDEN)
    {
        window->flags &= ~SDL_WINDOW_SHOWN;
        window->flags |= SDL_WINDOW_HIDDEN;
    }
    else
    {
        window->flags |= SDL_WINDOW_SHOWN;
        window->flags &= ~SDL_WINDOW_HIDDEN;
    }
    
    /*
    switch(w->state)
    {
        case WINDOW_STATE_NORMAL:
            break;

        case WINDOW_STATE_MAXIMIZED:
            window->flags |= SDL_WINDOW_MINIMIZED;
            break;

        case WINDOW_STATE_MINIMIZED:
            window->flags |= SDL_WINDOW_MINIMIZED;
            break;

        case WINDOW_STATE_FULLSCREEN:
            window->flags |= SDL_WINDOW_FULLSCREEN;
            break;
    }
    */

    window->driverdata = data;

    /* TODO: check if we actually have input focus */
    if(get_input_focus() == w->winid)
    {
        window->flags |= SDL_WINDOW_INPUT_FOCUS;
        SDL_SetKeyboardFocus(data->window);

        /*
        if(window->flags & SDL_WINDOW_INPUT_GRABBED)
        {
            mouse_grab(w);
            //cursor_clip(w->y, w->x, w->y + w->h - 1, w->x + w->w - 1);
        }
        */
    }

    /*
    if(window->flags & SDL_WINDOW_INPUT_FOCUS)
    {
        SDL_SetMouseFocus(window);
        SDL_SetKeyboardFocus(window);
    }
    */

    /* All done! */
    return 0;
}


static void dummy_repaint(struct window_t *window, int is_active_child)
{
    (void)window;
    (void)is_active_child;
}


int
LAYLAOS_CreateWindow(_THIS, SDL_Window *window)
{
    struct window_attribs_t attribs;
    struct window_t *w;
    
    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = window->x;
    attribs.y = window->y;
    attribs.w = window->w;
    attribs.h = window->h;
    attribs.flags = GetWindowStyle(window);
    
    if(!(w = window_create(&attribs)))
    {
        return SDL_SetError("Couldn't create window");
    }
    
    /*
    window_show(w);
    w->flags &= ~WINDOW_HIDDEN;
    //w->state = WINDOW_STATE_NORMAL;
    */
    w->repaint = dummy_repaint;

    if(SetupWindowData(_this, window, w, SDL_TRUE) < 0)
    {
        LAYLAOS_DestroyWindow(_this, window);
        return -1;
    }

    return 0;
}


int
LAYLAOS_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    struct window_t *w = (struct window_t *)data;

    window->title = w->title ? SDL_strdup(w->title) : SDL_strdup("");

    if(SetupWindowData(_this, window, w, SDL_FALSE) < 0)
    {
        return -1;
    }

    return 0;
}


void
LAYLAOS_SetWindowTitle(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_set_title(w, window->title);
}


void
LAYLAOS_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;

    if(icon)
    {
        Uint32 *sp, *dp;
        Uint32 *data;
        int x, y;
        unsigned int width_bytes = icon->w * 4;

        data = SDL_calloc(1, icon->h * width_bytes);

        if(!data)
        {
            SDL_OutOfMemory();
            return;
        }

        /* Code below assumes ARGB pixel format */
        SDL_assert(icon->format->format == SDL_PIXELFORMAT_ARGB8888);
        
        for(y = 0; y < icon->h; ++y)
        {
            sp = (Uint32 *)((Uint8 *)icon->pixels + y * icon->pitch);
            dp = (Uint32 *)((Uint8 *)data + y * width_bytes);

            for(x = 0; x < icon->w; ++x)
            {
                /* The server expects cursor data in the RGBA format */
                *dp = ((*sp & 0xffffff) << 8) | ((*sp) >> 24);
                dp++;
                sp++;
            }
        }

        window_load_icon(w, icon->w, icon->h, data);
        SDL_free(data);
    }
    else
    {
        window_load_icon(w, 0, 0, NULL);
    }
}


void
LAYLAOS_SetWindowPosition(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;

    if(window->x != w->x || window->y != w->y)
    {
        window_set_pos(w, window->x, window->y);
    }
}


void
LAYLAOS_SetWindowSize(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    
    if(window->x != w->x || window->y != w->y || window->w != w->w || window->h != w->h)
    {
        window_set_size(w, window->x, window->y, window->w, window->h);
        window->x = w->x;
        window->y = w->y;
        window->w = w->w;
        window->h = w->h;
    }
}


int
LAYLAOS_GetWindowBordersSize(_THIS, SDL_Window * window, int *top, int *left, int *bottom, int *right)
{
    /*
     * TODO: do this properly.
     */

    *top = WINDOW_BORDERWIDTH;
    *left = WINDOW_BORDERWIDTH;
    *bottom = WINDOW_BORDERWIDTH;
    *right = WINDOW_BORDERWIDTH;

    return 0;
}


void
LAYLAOS_GetWindowSizeInPixels(_THIS, SDL_Window * window, int *w, int *h)
{
    struct window_t *win = ((SDL_WindowData *)(window->driverdata))->xwindow;
    struct window_attribs_t attribs;
    
    // get the window's attributes
    if(!get_win_attribs(win->winid, &attribs))
    {
        *w = window->w;
        *h = window->h;
    }
    else
    {
        *w = attribs.w;
        *h = attribs.h;
    }
}


int
LAYLAOS_SetWindowOpacity(_THIS, SDL_Window * window, float opacity)
{
    /*
     * TODO:
     */

    return -1;
}


void
LAYLAOS_ShowWindow(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_show(w);
    //window->flags |= SDL_WINDOW_SHOWN;
    //window->flags &= ~SDL_WINDOW_HIDDEN;
}


void
LAYLAOS_HideWindow(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_hide(w);
    //window->flags &= ~SDL_WINDOW_SHOWN;
    //window->flags |= SDL_WINDOW_HIDDEN;

    /***
    if(window->flags & SDL_WINDOW_INPUT_FOCUS)
    {
        SDL_SetMouseFocus(NULL);
        SDL_SetKeyboardFocus(NULL);
        window->flags &= ~SDL_WINDOW_INPUT_FOCUS;
    }
    ***/
}


void
LAYLAOS_RaiseWindow(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_raise(w);
}


void
LAYLAOS_MaximizeWindow(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_maximize(w);
    //window->flags |= SDL_WINDOW_MAXIMIZED;
    //window->flags &= ~SDL_WINDOW_MINIMIZED;
}


void
LAYLAOS_MinimizeWindow(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_minimize(w);
    //window->flags |= SDL_WINDOW_MINIMIZED;
    //window->flags &= ~SDL_WINDOW_MAXIMIZED;
}


void
LAYLAOS_RestoreWindow(_THIS, SDL_Window *window)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_restore(w);
    //window->flags &= ~(SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED);
}


void
LAYLAOS_SetWindowBordered(_THIS, SDL_Window *window, SDL_bool bordered)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_set_bordered(w, bordered ? 1 : 0);
    
    /*
    if(bordered)
    {
        window->flags &= ~SDL_WINDOW_BORDERLESS;
    }
    else
    {
        window->flags |= SDL_WINDOW_BORDERLESS;
    }
    */
}


void
LAYLAOS_SetWindowResizable(_THIS, SDL_Window *window, SDL_bool resizable)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_set_resizable(w, resizable ? 1 : 0);

    /*
    if(resizable)
    {
        window->flags |= SDL_WINDOW_RESIZABLE;
    }
    else
    {
        window->flags &= ~SDL_WINDOW_RESIZABLE;
    }
    */
}


void
LAYLAOS_SetWindowAlwaysOnTop(_THIS, SDL_Window * window, SDL_bool on_top)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    window_set_ontop(w, on_top ? 1 : 0);

    /*
    if(on_top)
    {
        window->flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }
    else
    {
        window->flags &= ~SDL_WINDOW_ALWAYS_ON_TOP;
    }
    */
}


void
LAYLAOS_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    
    // make sure it is resizable or the window manager won't do it
    if(w->flags & WINDOW_NORESIZE)
    {
        window_set_resizable(w, 1);
    }

    if(fullscreen)
    {
        window_enter_fullscreen(w);
        //window->flags |= SDL_WINDOW_FULLSCREEN;
    }
    else
    {
        window_exit_fullscreen(w);
        //window->flags &= ~SDL_WINDOW_FULLSCREEN;
    }
    
    /*
    //LAYLAOS_SetWindowGrab(_this, window, fullscreen);
    LAYLAOS_SetWindowMouseGrab(_this, window, fullscreen);
    LAYLAOS_SetWindowKeyboardGrab(_this, window, fullscreen);
    */
}


int
LAYLAOS_SetWindowGammaRamp(_THIS, SDL_Window *window, const Uint16 *ramp)
{
    /*
     * TODO:
     */

    return -1;
}


int
LAYLAOS_GetWindowGammaRamp(_THIS, SDL_Window *window, Uint16 *ramp)
{
    /*
     * TODO:
     */

    return -1;
}


#if 0

void
LAYLAOS_SetWindowGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;
    
    if(grabbed)
    {
        if(mouse_grab(w))
        {
            /* Raise the window if we grab the mouse */
            window_raise(w);
            //window->flags |= SDL_WINDOW_MOUSE_CAPTURE;
            //SDL_SetMouseFocus(window);
        }

        if(keyboard_grab(w))
        {
            //window->flags |= SDL_WINDOW_INPUT_GRABBED;
            //SDL_SetKeyboardFocus(window);
        }
    }
    else
    {
        mouse_ungrab();
        keyboard_ungrab();
        //window->flags &= ~(SDL_WINDOW_MOUSE_CAPTURE | SDL_WINDOW_INPUT_GRABBED);
        //SDL_SetMouseFocus(NULL);
        //SDL_SetKeyboardFocus(NULL);
    }
}

#endif


void
LAYLAOS_SetWindowMouseGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (data == NULL) {
        return;
    }

    data->mouse_grabbed = SDL_FALSE;

    if(grabbed)
    {
        struct window_t *w = data->xwindow;

        if(window->flags & SDL_WINDOW_HIDDEN)
        {
            return;
        }

        if(mouse_grab(w, 1))
        {
            data->mouse_grabbed = SDL_TRUE;

            /* Raise the window if we grab the mouse */
            window_raise(w);
            //window->flags |= SDL_WINDOW_MOUSE_CAPTURE;
            //SDL_SetMouseFocus(window);
        }
    }
    else
    {
        mouse_ungrab();
    }
}


void
LAYLAOS_SetWindowKeyboardGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{
    struct window_t *w = ((SDL_WindowData *)(window->driverdata))->xwindow;

    if(grabbed)
    {
        keyboard_grab(w);
    }
    else
    {
        keyboard_ungrab();
    }
}


void
LAYLAOS_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

    if(data)
    {
        SDL_VideoData *videodata = (SDL_VideoData *)data->videodata;
        int numwindows = videodata->numwindows;
        SDL_WindowData **windowlist = videodata->windowlist;
        int i;

        if(windowlist)
        {
            for(i = 0; i < numwindows; ++i)
            {
                if(windowlist[i] && (windowlist[i]->window == window))
                {
                    windowlist[i] = windowlist[numwindows - 1];
                    windowlist[numwindows - 1] = NULL;
                    videodata->numwindows--;
                    break;
                }
            }
        }

        if(data->created)
        {
            window_destroy(data->xwindow);
        }

        SDL_free(data);
    }

    window->driverdata = NULL;
}


SDL_bool
LAYLAOS_GetWindowWMInfo(_THIS, SDL_Window *window, SDL_SysWMinfo *info)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

    if(info->version.major == SDL_MAJOR_VERSION &&
       info->version.minor == SDL_MINOR_VERSION)
    {
        info->subsystem = SDL_SYSWM_LAYLAOS;
        info->info.laylaos.window = data->xwindow;
        return SDL_TRUE;
    }
    else
    {
        SDL_SetError("Application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}


int
LAYLAOS_SetWindowHitTest(SDL_Window *window, SDL_bool enabled)
{
    return 0;  /* just succeed, the real work is done elsewhere. */
}


void
LAYLAOS_SetWindowMouseRect(_THIS, SDL_Window * window)
{
    /*
     * TODO:
     */
}


void LAYLAOS_OnWindowEnter(_THIS, SDL_Window * window)
{
    /*
     * TODO:
     */
}


void
LAYLAOS_AcceptDragAndDrop(SDL_Window * window, SDL_bool accept)
{
    /*
     * TODO:
     */
}


int
LAYLAOS_FlashWindow(_THIS, SDL_Window * window, SDL_FlashOperation operation)
{
    /*
     * TODO:
     */

    return SDL_Unsupported();
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
