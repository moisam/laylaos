#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include "SDL_laylaosshape.h"

SDL_WindowShaper*
LAYLAOS_CreateShaper(SDL_Window * window)
{
    return NULL;
}

int
LAYLAOS_SetWindowShape(SDL_WindowShaper *shaper,SDL_Surface *shape,SDL_WindowShapeMode *shape_mode)
{
    return -1;
}

int
LAYLAOS_ResizeWindowShape(SDL_Window *window)
{
    return -1;
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
