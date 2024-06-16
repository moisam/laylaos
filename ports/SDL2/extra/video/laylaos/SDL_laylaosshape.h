#include "../../SDL_internal.h"

#ifndef SDL_laylaosshape_h_
#define SDL_laylaosshape_h_

#include "SDL_video.h"
#include "SDL_shape.h"
#include "../SDL_sysvideo.h"

typedef struct
{
    void *unused;
} SDL_ShapeData;

extern SDL_WindowShaper *LAYLAOS_CreateShaper(SDL_Window* window);
extern int LAYLAOS_ResizeWindowShape(SDL_Window* window);
extern int LAYLAOS_SetWindowShape(SDL_WindowShaper *shaper,SDL_Surface *shape,SDL_WindowShapeMode *shapeMode);

#endif /* SDL_laylaosshape_h_ */
