#include "../../SDL_internal.h"

#ifndef _SDL_laylaosvideo_h
#define _SDL_laylaosvideo_h

#include "../SDL_sysvideo.h"

#include "SDL_laylaoswindow.h"

/* Private display data */

typedef struct SDL_VideoData
{
    /*
    struct screen_t screen;
    int serverfd;
    pid_t serverpid, mypid;
    size_t evbufsz;
    struct event_t *evbuf_internal;
    */

    int numwindows;
    SDL_WindowData **windowlist;
    int windowlistlength;
} SDL_VideoData;

#endif /* _SDL_laylaosvideo_h */

/* vi: set ts=4 sw=4 expandtab: */
