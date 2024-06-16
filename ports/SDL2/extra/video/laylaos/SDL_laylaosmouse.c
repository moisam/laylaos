#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <gui/client/window.h>
#include <gui/mouse.h>
#include <gui/cursor.h>
#include "SDL_laylaosvideo.h"
#include "SDL_assert.h"

//#include "../SDL_sysvideo.h"
#include "../../events/SDL_mouse_c.h"

static SDL_Cursor *
LAYLAOS_CreateDefaultCursor()
{
    SDL_Cursor *cursor;

    cursor = SDL_calloc(1, sizeof(*cursor));

    if(cursor)
    {
        cursor->driverdata = (void *)(uintptr_t)CURSOR_NORMAL;
    }
    else
    {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor *
LAYLAOS_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    SDL_Cursor *cursor;

    cursor = SDL_calloc(1, sizeof(*cursor));

    if(cursor)
    {
        Uint32 *sp, *dp;
        Uint32 *data;
        int x, y;
        unsigned int width_bytes = surface->w * 4;

        data = SDL_calloc(1, surface->h * width_bytes);

        if(!data)
        {
            SDL_OutOfMemory();
            return NULL;
        }

        /* Code below assumes ARGB pixel format */
        SDL_assert(surface->format->format == SDL_PIXELFORMAT_ARGB8888);
        
        for(y = 0; y < surface->h; ++y)
        {
            sp = (Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch);
            dp = (Uint32 *)((Uint8 *)data + y * width_bytes);

            for(x = 0; x < surface->w; ++x)
            {
                /* The server expects cursor data in the RGBA format */
                *dp = ((*sp & 0xffffff) << 8) | ((*sp) >> 24);
                dp++;
                sp++;
            }
        }

        cursor->driverdata = (void *)(uintptr_t)cursor_load(surface->w, surface->h,
                                                            hot_x, hot_y, data);
        SDL_free(data);
        
        if(!cursor->driverdata)
        {
            SDL_free(cursor);
            SDL_SetError("Failed to load cursor to server");
            return NULL;
        }
    }
    else
    {
        SDL_OutOfMemory();
    }

    return cursor;
}

/*
static SDL_Cursor *
LAYLAOS_CreateBlankCursor()
{
    SDL_Cursor *cursor = NULL;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 32, SDL_PIXELFORMAT_ARGB8888);

    if(surface)
    {
        cursor = LAYLAOS_CreateCursor(surface, 0, 0);
        SDL_FreeSurface(surface);
    }
    
    return cursor;
}
*/

static SDL_Cursor *
LAYLAOS_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor *cursor;
    curid_t sysid;

    switch(id)
    {
    default:
        SDL_assert(0);
        return NULL;
    case SDL_SYSTEM_CURSOR_ARROW:     sysid = CURSOR_NORMAL; break;
    case SDL_SYSTEM_CURSOR_IBEAM:     sysid = CURSOR_IBEAM; break;
    case SDL_SYSTEM_CURSOR_WAIT:      sysid = CURSOR_WAITING; break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR: sysid = CURSOR_CROSSHAIR; break;
    case SDL_SYSTEM_CURSOR_WAITARROW: sysid = CURSOR_WAITING; break;
    case SDL_SYSTEM_CURSOR_SIZENWSE:  sysid = CURSOR_NWSE; break;
    case SDL_SYSTEM_CURSOR_SIZENESW:  sysid = CURSOR_NESW; break;
    case SDL_SYSTEM_CURSOR_SIZEWE:    sysid = CURSOR_WE; break;
    case SDL_SYSTEM_CURSOR_SIZENS:    sysid = CURSOR_NS; break;
    case SDL_SYSTEM_CURSOR_SIZEALL:   sysid = CURSOR_CROSS; break;
    case SDL_SYSTEM_CURSOR_NO:        sysid = CURSOR_X; break;
    case SDL_SYSTEM_CURSOR_HAND:      sysid = CURSOR_HAND; break;
    }

    cursor = SDL_calloc(1, sizeof(*cursor));
    
    if(cursor)
    {
        cursor->driverdata = (void *)(uintptr_t)sysid;
    }
    else
    {
        SDL_OutOfMemory();
    }

    return cursor;
}

static void
LAYLAOS_FreeCursor(SDL_Cursor *cursor)
{
    curid_t curid = (curid_t)(uintptr_t)cursor->driverdata;

    cursor_free(curid);
    SDL_free(cursor);
}

static int
LAYLAOS_ShowCursor(SDL_Cursor *cursor)
{
    SDL_VideoDevice *video;
    SDL_Window *window;
    SDL_WindowData *data;
    curid_t curid;
    
    if(cursor)
    {
        curid = (curid_t)(uintptr_t)cursor->driverdata;
    }
    else
    {
        curid = CURSOR_NONE;
    }
    
    video = SDL_GetVideoDevice();

    for(window = video->windows; window; window = window->next)
    {
        data = (SDL_WindowData *)window->driverdata;

        if(curid != CURSOR_NONE)
        {
            cursor_show(data->xwindow, curid);
        }
        else
        {
            cursor_hide(data->xwindow);
        }
    }

    return 0;
}

static void
LAYLAOS_WarpMouse(SDL_Window *window, int x, int y)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    
    x = to_child_x(data->xwindow, x);
    y = to_child_y(data->xwindow, y);
    cursor_set_pos(x, y);

    /* Send the exact mouse motion associated with this warp */
    SDL_SendMouseMotion(window, SDL_GetMouse()->mouseID, 0, x, y);
}

static int
LAYLAOS_WarpMouseGlobal(int x, int y)
{
    cursor_set_pos(x, y);
    return 0;
}

static int
LAYLAOS_SetRelativeMouseMode(SDL_bool enabled)
{
    SDL_Unsupported();
    return -1;
}

static int
LAYLAOS_CaptureMouse(SDL_Window *window)
{
    SDL_Window *mouse_focus = SDL_GetMouseFocus();

    if(window)
    {
        SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
        int confined = !!(data->mouse_grabbed);
        
        if(!mouse_grab(data->xwindow, confined))
        {
            return SDL_SetError("Server refused mouse capture");
        }
    }
    else if(mouse_focus)
    {
        SDL_UpdateWindowGrab(mouse_focus);
    }
    else
    {
        mouse_ungrab();
    }

    return 0;
}

static Uint32
LAYLAOS_GetGlobalMouseState(int *x, int *y)
{
    Uint32 retval = 0;
    struct cursor_info_t curinfo = { 0, };
    
    cursor_get_info(&curinfo);
    *x = curinfo.x;
    *y = curinfo.y;

    retval |= (curinfo.buttons & MOUSE_LBUTTON_DOWN) ? SDL_BUTTON_LMASK : 0;
    retval |= (curinfo.buttons & MOUSE_RBUTTON_DOWN) ? SDL_BUTTON_RMASK : 0;
    retval |= (curinfo.buttons & MOUSE_MBUTTON_DOWN) ? SDL_BUTTON_MMASK : 0;
    //retval |= GetAsyncKeyState(VK_XBUTTON1) & 0x8000 ? SDL_BUTTON_X1MASK : 0;
    //retval |= GetAsyncKeyState(VK_XBUTTON2) & 0x8000 ? SDL_BUTTON_X2MASK : 0;

    return retval;
}

void
LAYLAOS_InitMouse(_THIS)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = LAYLAOS_CreateCursor;
    mouse->CreateSystemCursor = LAYLAOS_CreateSystemCursor;
    mouse->ShowCursor = LAYLAOS_ShowCursor;
    mouse->FreeCursor = LAYLAOS_FreeCursor;
    mouse->WarpMouse = LAYLAOS_WarpMouse;
    mouse->WarpMouseGlobal = LAYLAOS_WarpMouseGlobal;
    mouse->SetRelativeMouseMode = LAYLAOS_SetRelativeMouseMode;
    mouse->CaptureMouse = LAYLAOS_CaptureMouse;
    mouse->GetGlobalMouseState = LAYLAOS_GetGlobalMouseState;

    SDL_SetDefaultCursor(LAYLAOS_CreateDefaultCursor());
}

void
LAYLAOS_QuitMouse(_THIS)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    
    if(mouse->def_cursor)
    {
        SDL_free(mouse->def_cursor);
        mouse->def_cursor = NULL;
        mouse->cur_cursor = NULL;
    }
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
