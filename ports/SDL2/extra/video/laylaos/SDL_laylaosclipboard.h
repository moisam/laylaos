#include "../../SDL_internal.h"

#ifndef _SDL_laylaosclipboard_h
#define _SDL_laylaosclipboard_h

extern int LAYLAOS_SetClipboardText(_THIS, const char *text);
extern char *LAYLAOS_GetClipboardText(_THIS);
extern SDL_bool LAYLAOS_HasClipboardText(_THIS);

#endif /* _SDL_laylaosclipboard_h */

/* vi: set ts=4 sw=4 expandtab: */
