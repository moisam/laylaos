#include "../../SDL_internal.h"

#ifndef _SDL_laylaoskeyboard_h
#define _SDL_laylaoskeyboard_h

#include "SDL_scancode.h"

extern void LAYLAOS_InitKeyboard(_THIS);
extern void LAYLAOS_QuitKeyboard(_THIS);

extern void LAYLAOS_StartTextInput(_THIS);
extern void LAYLAOS_StopTextInput(_THIS);
extern void LAYLAOS_SetTextInputRect(_THIS, const SDL_Rect *rect);

extern SDL_Scancode LaylaOS_Keycodes[];
extern const int LaylaOS_KeycodeCount;

#endif /* _SDL_laylaoskeyboard_h */

/* vi: set ts=4 sw=4 expandtab: */
