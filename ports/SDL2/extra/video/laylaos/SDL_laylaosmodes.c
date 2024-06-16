#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include "SDL_pixels.h"
#include "../SDL_sysvideo.h"

#include <gui/gui.h>

//#define GUIDEV_PATH             "/dev/guiev"

#define GLOB                    __global_gui_data


int
LAYLAOS_GetDisplayBounds(_THIS, SDL_VideoDisplay * sdl_display, SDL_Rect * rect)
{
    rect->x = 0;
    rect->y = 0;
    rect->w = GLOB.screen.w;
    rect->h = GLOB.screen.h;

    return 0;
}

int
LAYLAOS_GetDisplayUsableBounds(_THIS, SDL_VideoDisplay * display, SDL_Rect * rect)
{
    rect->x = 0;
    rect->y = 0;
    rect->w = GLOB.screen.w;
    rect->h = GLOB.screen.h;

    return 0;
}

Uint32
LAYLAOS_GetPixelFormat(void)
{
    if(GLOB.screen.red_pos == 24 && GLOB.screen.green_pos == 16 && GLOB.screen.blue_pos == 8)
    {
        return SDL_PIXELFORMAT_RGBA8888;
    }
    else if(GLOB.screen.blue_pos == 24 && GLOB.screen.green_pos == 16 && GLOB.screen.red_pos == 8)
    {
        return SDL_PIXELFORMAT_BGRA8888;
    }
    else if(GLOB.screen.red_pos == 16 && GLOB.screen.green_pos == 8 && GLOB.screen.blue_pos == 0)
    {
        return SDL_PIXELFORMAT_ARGB8888;
    }
    else if(GLOB.screen.blue_pos == 16 && GLOB.screen.green_pos == 8 && GLOB.screen.red_pos == 0)
    {
        return SDL_PIXELFORMAT_ABGR8888;
    }
    else
    {
        return SDL_PIXELFORMAT_UNKNOWN;
    }
}

int
LAYLAOS_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    /*
     * NOTE: We don't support changing video mode (yet).
     */
    return 0;
}

void
LAYLAOS_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    /*
     * NOTE: We have only one display so far.
     */
    SDL_DisplayMode mode;

    mode.driverdata = NULL;
    mode.w = GLOB.screen.w;
    mode.h = GLOB.screen.h;
    mode.refresh_rate = 0;

    SDL_AddDisplayMode(display, &mode);
}

int
LAYLAOS_InitModes(_THIS)
{
    //SDL_VideoData *viddata = (SDL_VideoData *) _this->driverdata;
    SDL_VideoDisplay display;
    //SDL_DisplayData *displaydata;
    //SDL_DisplayModeData *data;
    SDL_DisplayMode mode;

    if((mode.format = LAYLAOS_GetPixelFormat()) == SDL_PIXELFORMAT_UNKNOWN)
    {
        printf("SDL: unknown RGB format\n");
        return -1;
    }

    mode.driverdata = NULL;
    mode.w = GLOB.screen.w;
    mode.h = GLOB.screen.h;
    mode.refresh_rate = 0;

    SDL_zero(display);
    //display.name = strdup(GUIDEV_PATH);
    display.name = strdup("Default display");
    display.desktop_mode = mode;
    display.current_mode = mode;
    //display.driverdata = displaydata;
    SDL_AddVideoDisplay(&display, SDL_FALSE);

    return 0;
}

void
LAYLAOS_QuitModes(_THIS)
{
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
