#include "../../SDL_internal.h"


extern int LAYLAOS_CreateWindowFramebuffer(_THIS, SDL_Window * window,
                                           Uint32 * format,
                                           void ** pixels, int *pitch);
extern int LAYLAOS_UpdateWindowFramebuffer(_THIS, SDL_Window * window,
                                           const SDL_Rect * rects, int numrects);
extern void LAYLAOS_DestroyWindowFramebuffer(_THIS, SDL_Window * window);

/* vi: set ts=4 sw=4 expandtab: */
