#include "../../SDL_internal.h"

#ifndef _SDL_laylaoswindow_h
#define _SDL_laylaoswindow_h

#include "../SDL_sysvideo.h"

typedef struct
{
    SDL_Window *window;
    struct window_t *xwindow;
    uint8_t *backbuffer;
    SDL_bool created;
    SDL_bool mouse_grabbed;
    struct SDL_VideoData *videodata;
} SDL_WindowData;

extern int LAYLAOS_CreateWindow(_THIS, SDL_Window * window);
extern int LAYLAOS_CreateWindowFrom(_THIS, SDL_Window * window, const void *data);
extern void LAYLAOS_SetWindowTitle(_THIS, SDL_Window * window);
extern void LAYLAOS_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon);
extern void LAYLAOS_SetWindowPosition(_THIS, SDL_Window * window);
extern void LAYLAOS_SetWindowSize(_THIS, SDL_Window * window);
extern int LAYLAOS_GetWindowBordersSize(_THIS, SDL_Window * window, int *top, int *left, int *bottom, int *right);
extern void LAYLAOS_GetWindowSizeInPixels(_THIS, SDL_Window * window, int *width, int *height);
extern int LAYLAOS_SetWindowOpacity(_THIS, SDL_Window * window, float opacity);
extern void LAYLAOS_ShowWindow(_THIS, SDL_Window * window);
extern void LAYLAOS_HideWindow(_THIS, SDL_Window * window);
extern void LAYLAOS_RaiseWindow(_THIS, SDL_Window * window);
extern void LAYLAOS_MaximizeWindow(_THIS, SDL_Window * window);
extern void LAYLAOS_MinimizeWindow(_THIS, SDL_Window * window);
extern void LAYLAOS_RestoreWindow(_THIS, SDL_Window * window);
extern void LAYLAOS_SetWindowBordered(_THIS, SDL_Window * window, SDL_bool bordered);
extern void LAYLAOS_SetWindowAlwaysOnTop(_THIS, SDL_Window * window, SDL_bool on_top);
extern void LAYLAOS_SetWindowResizable(_THIS, SDL_Window * window, SDL_bool resizable);
extern void LAYLAOS_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen);
extern int LAYLAOS_SetWindowGammaRamp(_THIS, SDL_Window * window, const Uint16 * ramp);
extern int LAYLAOS_GetWindowGammaRamp(_THIS, SDL_Window * window, Uint16 * ramp);
//extern void LAYLAOS_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed);
extern void LAYLAOS_SetWindowMouseRect(_THIS, SDL_Window * window);
extern void LAYLAOS_SetWindowMouseGrab(_THIS, SDL_Window * window, SDL_bool grabbed);
extern void LAYLAOS_SetWindowKeyboardGrab(_THIS, SDL_Window * window, SDL_bool grabbed);
extern void LAYLAOS_DestroyWindow(_THIS, SDL_Window * window);
extern SDL_bool LAYLAOS_GetWindowWMInfo(_THIS, SDL_Window * window,
                                        struct SDL_SysWMinfo *info);
extern int LAYLAOS_SetWindowHitTest(SDL_Window *window, SDL_bool enabled);

extern void LAYLAOS_OnWindowEnter(_THIS, SDL_Window * window);
extern void LAYLAOS_AcceptDragAndDrop(SDL_Window * window, SDL_bool accept);
extern int LAYLAOS_FlashWindow(_THIS, SDL_Window * window, SDL_FlashOperation operation);

#endif /* _SDL_laylaoswindow_h */

/* vi: set ts=4 sw=4 expandtab: */
