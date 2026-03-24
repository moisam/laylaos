/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: usb_keytable.c
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
 *  \file usb_keytable.c
 *
 *  Scancode-to-keycode table for USB keyboards.
 *
 *  See: https://aeb.win.tue.nl/linux/kbd/scancodes-14.html
 */

#include <kernel/keycodes.h>

// USB keycodes to kernel scancodes
char usb_keycodes[256] =
{
    0,                  // 0  no scancodes
    0,                  // 1  phantom condition
    0,                  // 2  self test failed
    0,                  // 3  undefined error
    KEYCODE_A,          // 4
    KEYCODE_B,          // 5
    KEYCODE_C,          // 6
    KEYCODE_D,          // 7
    KEYCODE_E,          // 8
    KEYCODE_F,          // 9
    KEYCODE_G,          // 10
    KEYCODE_H,          // 11
    KEYCODE_I,          // 12
    KEYCODE_J,          // 13
    KEYCODE_K,          // 14
    KEYCODE_L,          // 15
    KEYCODE_M,          // 16
    KEYCODE_N,          // 17
    KEYCODE_O,          // 18
    KEYCODE_P,          // 19
    KEYCODE_Q,          // 20
    KEYCODE_R,          // 21
    KEYCODE_S,          // 22
    KEYCODE_T,          // 23
    KEYCODE_U,          // 24
    KEYCODE_V,          // 25
    KEYCODE_W,          // 26
    KEYCODE_X,          // 27
    KEYCODE_Y,          // 28
    KEYCODE_Z,          // 29
    KEYCODE_1,          // 30
    KEYCODE_2,          // 31
    KEYCODE_3,          // 32
    KEYCODE_4,          // 33
    KEYCODE_5,          // 34
    KEYCODE_6,          // 35
    KEYCODE_7,          // 36
    KEYCODE_8,          // 37
    KEYCODE_9,          // 38
    KEYCODE_0,          // 39
    KEYCODE_ENTER,      // 40
    KEYCODE_ESC,        // 41
    KEYCODE_BACKSPACE,  // 42
    KEYCODE_TAB,        // 43
    KEYCODE_SPACE,      // 44
    KEYCODE_MINUS,      // 45
    KEYCODE_EQUAL,      // 46
    KEYCODE_LBRACKET,   // 47
    KEYCODE_RBRACKET,   // 48
    KEYCODE_BACKSLASH,  // 49
    0,                  // 50  XXX: ???
    KEYCODE_SEMICOLON,  // 51
    KEYCODE_QUOTE,      // 52
    KEYCODE_BACKTICK,   // 53
    KEYCODE_COMMA,      // 54
    KEYCODE_DOT,        // 55
    KEYCODE_SLASH,      // 56
    KEYCODE_CAPS,       // 57
    KEYCODE_F1,         // 58
    KEYCODE_F2,         // 59
    KEYCODE_F3,         // 60
    KEYCODE_F4,         // 61
    KEYCODE_F5,         // 62
    KEYCODE_F6,         // 63
    KEYCODE_F7,         // 64
    KEYCODE_F8,         // 65
    KEYCODE_F9,         // 66
    KEYCODE_F10,        // 67
    KEYCODE_F11,        // 68
    KEYCODE_F12,        // 69
    0,                  // 70  XXX: PRNTSCR
    KEYCODE_SCROLL,     // 71
    0,                  // 72  XXX: Pause
    KEYCODE_INSERT,     // 73
    KEYCODE_HOME,       // 74
    KEYCODE_PGUP,       // 75
    KEYCODE_DELETE,     // 76
    KEYCODE_END,        // 77
    KEYCODE_PGDN,       // 78
    KEYCODE_RIGHT,      // 79
    KEYCODE_LEFT,       // 80
    KEYCODE_DOWN,       // 81
    KEYCODE_UP,         // 82
    KEYCODE_NUM,        // 83
    KEYCODE_KP_DIV,     // 84
    KEYCODE_KP_MULT,    // 85
    KEYCODE_KP_MINUS,   // 86
    KEYCODE_KP_PLUS,    // 87
    KEYCODE_KP_ENTER,   // 88
    KEYCODE_KP_1,       // 89
    KEYCODE_KP_2,       // 90
    KEYCODE_KP_3,       // 91
    KEYCODE_KP_4,       // 92
    KEYCODE_KP_5,       // 93
    KEYCODE_KP_6,       // 94
    KEYCODE_KP_7,       // 95
    KEYCODE_KP_8,       // 96
    KEYCODE_KP_9,       // 97
    KEYCODE_KP_0,       // 98
    KEYCODE_KP_DOT,     // 99
    0,                  // 100 XXX: ???
    KEYCODE_APPS,       // 101
    KEYCODE_POWER,      // 102
    KEYCODE_EQUAL,      // 103 XXX: KP =
    0,                  // 104 XXX: F13
    0,                  // 105 XXX: F14
    0,                  // 106 XXX: F15
    0,                  // 107 XXX: F16
    0,                  // 108 XXX: F17
    0,                  // 109 XXX: F18
    0,                  // 110 XXX: F19
    0,                  // 111 XXX: F20
    0,                  // 112 XXX: F21
    0,                  // 113 XXX: F22
    0,                  // 114 XXX: F23
    0,                  // 115 XXX: F24
    0,                  // 116 XXX: Execute
    0,                  // 117 XXX: Help
    0,                  // 118 XXX: Menu
    KEYCODE_SELECT,     // 119
    KEYCODE_AUD_STOP,   // 120
    0,                  // 121 XXX: Again
    0,                  // 122 XXX: Undo
    0,                  // 123 XXX: Cut
    0,                  // 124 XXX: Copy
    0,                  // 125 XXX: Paste
    0,                  // 126 XXX: Find
    KEYCODE_AUD_MUTE,   // 127
    KEYCODE_VOLUP,      // 128
    KEYCODE_VOLDN,      // 129
    KEYCODE_CAPS,       // 130
    KEYCODE_NUM,        // 131
    KEYCODE_SCROLL,     // 132
    KEYCODE_COMMA,      // 133 XXX: KP ,
    KEYCODE_EQUAL,      // 134 XXX: KP =
    0,                  // 135 XXX: ???
    0,                  // 136 XXX: ???
    0,                  // 137 XXX: ???
    0,                  // 138 XXX: ???
    0,                  // 139 XXX: ???
    0,                  // 140 XXX: ???
    0,                  // 141 XXX: ???
    0,                  // 142 XXX: ???
    0,                  // 143 XXX: ???
    0,                  // 144 XXX: ???
    0,                  // 145 XXX: ???
    0,                  // 146 XXX: ???
    0,                  // 147 XXX: ???
    0,                  // 148 XXX: ???
    0,                  // 149 XXX: ???
    0,                  // 150 XXX: ???
    0,                  // 151 XXX: ???
    0,                  // 152 XXX: ???
    0,                  // 153 XXX: Alt Erase
    0,                  // 154 XXX: SysRq
    0,                  // 155 XXX: Cancel
    0,                  // 156 XXX: Clear
    0,                  // 157 XXX: Prior
    0,                  // 158 XXX: Return
    0,                  // 159 XXX: Separ
    0,                  // 160 XXX: Out
    0,                  // 161 XXX: Oper
    0,                  // 162 XXX: Clear/Again
    0,                  // 163 XXX: CrSel/Props
    0,                  // 164 XXX: ExSel
};

