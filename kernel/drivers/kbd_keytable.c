/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kbd_keytable.c
 *    This file is part of LaylaOS.
 *
 *    LaylaOS is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    LaylaOS is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LaylaOS.  If not, see <http://www.gnu.org/licenses/>.
 */    

/**
 *  \file kbd_keytable.c
 *
 *  Scancode-to-keycode table for keyboards with the XT scancode set.
 *
 *  See: http://www.brokenthorn.com/Resources/OSDevScanCodes.html
 */

#include <kernel/keycodes.h>

// 'normal' scancodes
char xt_keycodes[256] =
{
    0,
    KEYCODE_ESC,        // 01
    KEYCODE_1,          // 02
    KEYCODE_2,          // 03
    KEYCODE_3,          // 04
    KEYCODE_4,          // 05
    KEYCODE_5,          // 06
    KEYCODE_6,          // 07
    KEYCODE_7,          // 08
    KEYCODE_8,          // 09
    KEYCODE_9,          // 0A
    KEYCODE_0,          // 0B
    KEYCODE_MINUS,      // 0C
    KEYCODE_EQUAL,      // 0D
    KEYCODE_BACKSPACE,  // 0E
    KEYCODE_TAB,        // 0F
    KEYCODE_Q,          // 10
    KEYCODE_W,          // 11
    KEYCODE_E,          // 12
    KEYCODE_R,          // 13
    KEYCODE_T,          // 14
    KEYCODE_Y,          // 15
    KEYCODE_U,          // 16
    KEYCODE_I,          // 17
    KEYCODE_O,          // 18
    KEYCODE_P,          // 19
    KEYCODE_LBRACKET,   // 1A
    KEYCODE_RBRACKET,   // 1B
    KEYCODE_ENTER,      // 1C
    KEYCODE_LCTRL,      // 1D
    KEYCODE_A,          // 1E
    KEYCODE_S,          // 1F
    KEYCODE_D,          // 20
    KEYCODE_F,          // 21
    KEYCODE_G,          // 22
    KEYCODE_H,          // 23
    KEYCODE_J,          // 24
    KEYCODE_K,          // 25
    KEYCODE_L,          // 26
    KEYCODE_SEMICOLON,  // 27
    KEYCODE_QUOTE,      // 28
    KEYCODE_BACKTICK,   // 29
    KEYCODE_LSHIFT,     // 2A
    KEYCODE_BACKSLASH,  // 2B
    KEYCODE_Z,          // 2C
    KEYCODE_X,          // 2D
    KEYCODE_C,          // 2E
    KEYCODE_V,          // 2F
    KEYCODE_B,          // 30
    KEYCODE_N,          // 31
    KEYCODE_M,          // 32
    KEYCODE_COMMA,      // 33
    KEYCODE_DOT,        // 34
    KEYCODE_SLASH,      // 35
    KEYCODE_RSHIFT,     // 36
    KEYCODE_KP_MULT,    // 37
    KEYCODE_LALT,       // 38
    KEYCODE_SPACE,      // 39
    KEYCODE_CAPS,       // 3A
    KEYCODE_F1,         // 3B
    KEYCODE_F2,         // 3C
    KEYCODE_F3,         // 3D
    KEYCODE_F4,         // 3E
    KEYCODE_F5,         // 3F
    KEYCODE_F6,         // 40
    KEYCODE_F7,         // 41
    KEYCODE_F8,         // 42
    KEYCODE_F9,         // 43
    KEYCODE_F10,        // 44
    KEYCODE_NUM,        // 45
    KEYCODE_SCROLL,     // 46
    KEYCODE_KP_7,       // 47
    KEYCODE_KP_8,       // 48
    KEYCODE_KP_9,       // 49
    KEYCODE_KP_MINUS,   // 4A
    KEYCODE_KP_4,       // 4B
    KEYCODE_KP_5,       // 4C
    KEYCODE_KP_6,       // 4D
    KEYCODE_KP_PLUS,    // 4E
    KEYCODE_KP_1,       // 4F
    KEYCODE_KP_2,       // 50
    KEYCODE_KP_3,       // 51
    KEYCODE_KP_0,       // 52
    KEYCODE_KP_DOT,     // 53
    0,
    0,
    0,
    KEYCODE_F11,        // 57
    KEYCODE_F12,        // 58
    0,
};
    

// escaped scancodes (e0 xx)
char xt_esc_keycodes[256] =
{
    /* 0x00 - 0x0f */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x10 */          KEYCODE_AUD_PREV,
    /* 0x11 - 0x18 */   0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x19 */          KEYCODE_AUD_NEXT,
    /* 0x1a - 0x1b */   0, 0,
    /* 0x1c */          KEYCODE_KP_ENTER,
    /* 0x1d */          KEYCODE_RCTRL,
    /* 0x1e - 0x1f */   0, 0,
    /* 0x20 */          KEYCODE_AUD_MUTE,
    /* 0x21 */          KEYCODE_CALC,
    /* 0x22 */          KEYCODE_AUD_PLAY,
    /* 0x23 */          0,
    /* 0x24 */          KEYCODE_AUD_STOP,
    /* 0x25 - 0x2d */   0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x2e */          KEYCODE_VOLDN,
    /* 0x2f */          0,
    /* 0x30 */          KEYCODE_VOLUP,
    /* 0x31 */          0,
    /* 0x32 */          KEYCODE_WWW_HOME,
    /* 0x33 - 0x34 */   0, 0,
    /* 0x35 */          KEYCODE_KP_DIV,
    /* 0x36 - 0x37 */   0, 0,
    /* 0x38 */          KEYCODE_RALT,
    /* 0x39 - 0x46 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x47 */          KEYCODE_HOME,
    /* 0x48 */          KEYCODE_UP,
    /* 0x49 */          KEYCODE_PGUP,
    /* 0x4a */          0,
    /* 0x4b */          KEYCODE_LEFT,
    /* 0x4c */          0,
    /* 0x4d */          KEYCODE_RIGHT,
    /* 0x4e */          0,
    /* 0x4f */          KEYCODE_END,
    /* 0x50 */          KEYCODE_DOWN,
    /* 0x51 */          KEYCODE_PGDN,
    /* 0x52 */          KEYCODE_INSERT,
    /* 0x53 */          KEYCODE_DELETE,
    /* 0x54 - 0x5a */   0, 0, 0, 0, 0, 0, 0,
    /* 0x5b */          KEYCODE_LGUI,
    /* 0x5c */          KEYCODE_RGUI,
    /* 0x5d */          KEYCODE_APPS,
    /* 0x5e */          KEYCODE_POWER,
    /* 0x5f */          KEYCODE_SLEEP,
    /* 0x60 - 0x62 */   0, 0, 0,
    /* 0x63 */          KEYCODE_WAKEUP,
    /* 0x64 */          0,
    /* 0x65 */          KEYCODE_WWW_SEARCH,
    /* 0x66 */          KEYCODE_WWW_FAV,
    /* 0x67 */          KEYCODE_WWW_REFRESH,
    /* 0x68 */          KEYCODE_WWW_STOP,
    /* 0x69 */          KEYCODE_WWW_FWD,
    /* 0x6a */          KEYCODE_WWW_BACK,
    /* 0x6b */          KEYCODE_COMPUTER,
    /* 0x6c */          KEYCODE_EMAIL,
    /* 0x6d */          KEYCODE_SELECT,
    0,
};

