/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifndef _SDL_laylaosaudio_h
#define _SDL_laylaosaudio_h

#include "../SDL_sysaudio.h"

#define _THIS   SDL_AudioDevice *this

struct SDL_PrivateAudioData
{
    /* The file descriptor for the audio device */
    int audio_fd;

    /* The parent process id, to detect when application quits */
    pid_t parent;

    /* Raw mixing buffer */
    Uint8 *mixbuf;
    int mixlen;

    /* Support for audio timing using a timer, in addition to select() */
    float frame_ticks;
    float next_frame;
};

#define FUDGE_TICKS 10      /* The scheduler overhead ticks per frame */

#endif /* _SDL_laylaosaudio_h */

/* vi: set ts=4 sw=4 expandtab: */
