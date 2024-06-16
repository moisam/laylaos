#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <kernel/keycodes.h>

#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"
#include "SDL_laylaoskeyboard.h"

void
LAYLAOS_InitKeyboard(_THIS)
{
    SDL_Keycode keymap[SDL_NUM_SCANCODES];

    /* Add default scancode to key mapping */
    SDL_GetDefaultKeymap(keymap);
    SDL_SetKeymap(0, keymap, SDL_NUM_SCANCODES, SDL_FALSE);
}

void
LAYLAOS_QuitKeyboard(_THIS)
{
}

SDL_Scancode LaylaOS_Keycodes[256] =
{
    [0] = SDL_SCANCODE_UNKNOWN,
    [KEYCODE_A] = SDL_SCANCODE_A,
    [KEYCODE_B] = SDL_SCANCODE_B,
    [KEYCODE_C] = SDL_SCANCODE_C,
    [KEYCODE_D] = SDL_SCANCODE_D,
    [KEYCODE_E] = SDL_SCANCODE_E,
    [KEYCODE_F] = SDL_SCANCODE_F,
    [KEYCODE_G] = SDL_SCANCODE_G,
    [KEYCODE_H] = SDL_SCANCODE_H,
    [KEYCODE_I] = SDL_SCANCODE_I,
    [KEYCODE_J] = SDL_SCANCODE_J,
    [KEYCODE_K] = SDL_SCANCODE_K,
    [KEYCODE_L] = SDL_SCANCODE_L,
    [KEYCODE_M] = SDL_SCANCODE_M,
    [KEYCODE_N] = SDL_SCANCODE_N,
    [KEYCODE_O] = SDL_SCANCODE_O,
    [KEYCODE_P] = SDL_SCANCODE_P,
    [KEYCODE_Q] = SDL_SCANCODE_Q,
    [KEYCODE_R] = SDL_SCANCODE_R,
    [KEYCODE_S] = SDL_SCANCODE_S,
    [KEYCODE_T] = SDL_SCANCODE_T,
    [KEYCODE_U] = SDL_SCANCODE_U,
    [KEYCODE_V] = SDL_SCANCODE_V,
    [KEYCODE_W] = SDL_SCANCODE_W,
    [KEYCODE_X] = SDL_SCANCODE_X,
    [KEYCODE_Y] = SDL_SCANCODE_Y,
    [KEYCODE_Z] = SDL_SCANCODE_Z,
    [KEYCODE_0] = SDL_SCANCODE_0,
    [KEYCODE_1] = SDL_SCANCODE_1,
    [KEYCODE_2] = SDL_SCANCODE_2,
    [KEYCODE_3] = SDL_SCANCODE_3,
    [KEYCODE_4] = SDL_SCANCODE_4,
    [KEYCODE_5] = SDL_SCANCODE_5,
    [KEYCODE_6] = SDL_SCANCODE_6,
    [KEYCODE_7] = SDL_SCANCODE_7,
    [KEYCODE_8] = SDL_SCANCODE_8,
    [KEYCODE_9] = SDL_SCANCODE_9,
    [KEYCODE_BACKTICK] = SDL_SCANCODE_GRAVE,
    [KEYCODE_MINUS] = SDL_SCANCODE_MINUS,
    [KEYCODE_EQUAL] = SDL_SCANCODE_EQUALS,
    [KEYCODE_BACKSLASH] = SDL_SCANCODE_BACKSLASH,
    [KEYCODE_BACKSPACE] = SDL_SCANCODE_BACKSPACE,
    [KEYCODE_SPACE] = SDL_SCANCODE_SPACE,
    [KEYCODE_TAB] = SDL_SCANCODE_TAB,
    [KEYCODE_ENTER] = SDL_SCANCODE_RETURN,
    [KEYCODE_ESC] = SDL_SCANCODE_ESCAPE,
    [KEYCODE_LBRACKET] = SDL_SCANCODE_LEFTBRACKET,
    [KEYCODE_RBRACKET] = SDL_SCANCODE_RIGHTBRACKET,
    [KEYCODE_SEMICOLON] = SDL_SCANCODE_SEMICOLON,
    [KEYCODE_QUOTE] = SDL_SCANCODE_APOSTROPHE,
    [KEYCODE_COMMA] = SDL_SCANCODE_COMMA,
    [KEYCODE_DOT] = SDL_SCANCODE_PERIOD,
    [KEYCODE_SLASH] = SDL_SCANCODE_SLASH,

    [KEYCODE_CAPS] = SDL_SCANCODE_CAPSLOCK,
    [KEYCODE_LSHIFT] = SDL_SCANCODE_LSHIFT,
    [KEYCODE_LCTRL] = SDL_SCANCODE_LCTRL,
    [KEYCODE_LGUI] = SDL_SCANCODE_LGUI,
    [KEYCODE_LALT] = SDL_SCANCODE_LALT,
    [KEYCODE_RSHIFT] = SDL_SCANCODE_RSHIFT,
    [KEYCODE_RCTRL] = SDL_SCANCODE_RCTRL,
    [KEYCODE_RGUI] = SDL_SCANCODE_RGUI,
    [KEYCODE_RALT] = SDL_SCANCODE_RALT,
    [KEYCODE_APPS] = SDL_SCANCODE_APPLICATION,
    [KEYCODE_F1] = SDL_SCANCODE_F1,
    [KEYCODE_F2] = SDL_SCANCODE_F2,
    [KEYCODE_F3] = SDL_SCANCODE_F3,
    [KEYCODE_F4] = SDL_SCANCODE_F4,
    [KEYCODE_F5] = SDL_SCANCODE_F5,
    [KEYCODE_F6] = SDL_SCANCODE_F6,
    [KEYCODE_F7] = SDL_SCANCODE_F7,
    [KEYCODE_F8] = SDL_SCANCODE_F8,
    [KEYCODE_F9] = SDL_SCANCODE_F9,
    [KEYCODE_F10] = SDL_SCANCODE_F10,
    [KEYCODE_F11] = SDL_SCANCODE_F11,
    [KEYCODE_F12] = SDL_SCANCODE_F12,
    [KEYCODE_PRINT_SCREEN] = SDL_SCANCODE_PRINTSCREEN,
    [KEYCODE_SCROLL] = SDL_SCANCODE_SCROLLLOCK,
    [KEYCODE_PAUSE] = SDL_SCANCODE_PAUSE,
    [KEYCODE_INSERT] = SDL_SCANCODE_INSERT,
    [KEYCODE_HOME] = SDL_SCANCODE_HOME,
    [KEYCODE_PGUP] = SDL_SCANCODE_PAGEUP,
    [KEYCODE_DELETE] = SDL_SCANCODE_DELETE,
    [KEYCODE_END] = SDL_SCANCODE_END,
    [KEYCODE_PGDN] = SDL_SCANCODE_PAGEDOWN,
    [KEYCODE_UP] = SDL_SCANCODE_UP,
    [KEYCODE_LEFT] = SDL_SCANCODE_LEFT,
    [KEYCODE_DOWN] = SDL_SCANCODE_DOWN,
    [KEYCODE_RIGHT] = SDL_SCANCODE_RIGHT,
    [KEYCODE_NUM] = SDL_SCANCODE_NUMLOCKCLEAR,
    [KEYCODE_KP_DIV] = SDL_SCANCODE_KP_DIVIDE,
    [KEYCODE_KP_MULT] = SDL_SCANCODE_KP_MULTIPLY,
    [KEYCODE_KP_ENTER] = SDL_SCANCODE_KP_ENTER,

    [KEYCODE_KP_7] = SDL_SCANCODE_KP_7,
    [KEYCODE_KP_8] = SDL_SCANCODE_KP_8,
    [KEYCODE_KP_9] = SDL_SCANCODE_KP_9,
    [KEYCODE_KP_MINUS] = SDL_SCANCODE_KP_MINUS,
    [KEYCODE_KP_4] = SDL_SCANCODE_KP_4,
    [KEYCODE_KP_5] = SDL_SCANCODE_KP_5,
    [KEYCODE_KP_6] = SDL_SCANCODE_KP_6,
    [KEYCODE_KP_PLUS] = SDL_SCANCODE_KP_PLUS,
    [KEYCODE_KP_1] = SDL_SCANCODE_KP_1,
    [KEYCODE_KP_2] = SDL_SCANCODE_KP_2,
    [KEYCODE_KP_3] = SDL_SCANCODE_KP_3,
    [KEYCODE_KP_0] = SDL_SCANCODE_KP_0,
    [KEYCODE_KP_DOT] = SDL_SCANCODE_KP_PERIOD,

    [KEYCODE_POWER] = SDL_SCANCODE_POWER,
    [KEYCODE_SLEEP] = SDL_SCANCODE_SLEEP,
    [KEYCODE_WAKEUP] = SDL_SCANCODE_UNKNOWN,

    [KEYCODE_AUD_NEXT] = SDL_SCANCODE_AUDIONEXT,
    [KEYCODE_AUD_PREV] = SDL_SCANCODE_AUDIOPREV,
    [KEYCODE_AUD_STOP] = SDL_SCANCODE_AUDIOSTOP,
    [KEYCODE_AUD_PLAY] = SDL_SCANCODE_AUDIOPLAY,
    [KEYCODE_AUD_MUTE] = SDL_SCANCODE_MUTE,
    [KEYCODE_VOLUP] = SDL_SCANCODE_VOLUMEUP,
    [KEYCODE_VOLDN] = SDL_SCANCODE_VOLUMEDOWN,
    [KEYCODE_SELECT] = SDL_SCANCODE_MEDIASELECT,
    [KEYCODE_EMAIL] = SDL_SCANCODE_MAIL,
    [KEYCODE_CALC] = SDL_SCANCODE_CALCULATOR,
    [KEYCODE_COMPUTER] = SDL_SCANCODE_COMPUTER,
    [KEYCODE_WWW_SEARCH] = SDL_SCANCODE_AC_SEARCH,
    [KEYCODE_WWW_HOME] = SDL_SCANCODE_AC_HOME,
    [KEYCODE_WWW_BACK] = SDL_SCANCODE_AC_BACK,
    [KEYCODE_WWW_FWD] = SDL_SCANCODE_AC_FORWARD,
    [KEYCODE_WWW_STOP] = SDL_SCANCODE_STOP,
    [KEYCODE_WWW_REFRESH] = SDL_SCANCODE_AC_REFRESH,
    [KEYCODE_WWW_FAV] = SDL_SCANCODE_AC_BOOKMARKS,
};

const int LaylaOS_KeycodeCount = sizeof(LaylaOS_Keycodes) / sizeof(LaylaOS_Keycodes[0]);


void
LAYLAOS_StartTextInput(_THIS)
{
    /*
     * TODO:
     */
}

void
LAYLAOS_StopTextInput(_THIS)
{
    /*
     * TODO:
     */
}

void
LAYLAOS_SetTextInputRect(_THIS, const SDL_Rect *rect)
{
    /*
     * TODO:
     */
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
