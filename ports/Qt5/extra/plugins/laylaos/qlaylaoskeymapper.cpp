/***************************************************************************
**
** Copyright (C) 2024 Mohammed Isam <mohammed_isam1984@yahoo.com>
** Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qlaylaoskeymapper.h"

#include <kernel/keycodes.h>

QT_BEGIN_NAMESPACE

int LaylaOS_ScanCodes[] = {
    Qt::Key_A,              KEYCODE_A,
    Qt::Key_B,              KEYCODE_B,
    Qt::Key_C,              KEYCODE_C,
    Qt::Key_D,              KEYCODE_D,
    Qt::Key_E,              KEYCODE_E,
    Qt::Key_F,              KEYCODE_F,
    Qt::Key_G,              KEYCODE_G,
    Qt::Key_H,              KEYCODE_H,
    Qt::Key_I,              KEYCODE_I,
    Qt::Key_J,              KEYCODE_J,
    Qt::Key_K,              KEYCODE_K,
    Qt::Key_L,              KEYCODE_L,
    Qt::Key_M,              KEYCODE_M,
    Qt::Key_N,              KEYCODE_N,
    Qt::Key_O,              KEYCODE_O,
    Qt::Key_P,              KEYCODE_P,
    Qt::Key_Q,              KEYCODE_Q,
    Qt::Key_R,              KEYCODE_R,
    Qt::Key_S,              KEYCODE_S,
    Qt::Key_T,              KEYCODE_T,
    Qt::Key_U,              KEYCODE_U,
    Qt::Key_V,              KEYCODE_V,
    Qt::Key_W,              KEYCODE_W,
    Qt::Key_X,              KEYCODE_X,
    Qt::Key_Y,              KEYCODE_Y,
    Qt::Key_Z,              KEYCODE_Z,
    Qt::Key_0,              KEYCODE_0,
    Qt::Key_1,              KEYCODE_1,
    Qt::Key_2,              KEYCODE_2,
    Qt::Key_3,              KEYCODE_3,
    Qt::Key_4,              KEYCODE_4,
    Qt::Key_5,              KEYCODE_5,
    Qt::Key_6,              KEYCODE_6,
    Qt::Key_7,              KEYCODE_7,
    Qt::Key_8,              KEYCODE_8,
    Qt::Key_9,              KEYCODE_9,
    Qt::Key_AsciiTilde,     KEYCODE_BACKTICK,
    Qt::Key_Minus,          KEYCODE_MINUS,
    Qt::Key_Equal,          KEYCODE_EQUAL,
    Qt::Key_Backslash,      KEYCODE_BACKSLASH,
    Qt::Key_Backspace,      KEYCODE_BACKSPACE,
    Qt::Key_Space,          KEYCODE_SPACE,
    Qt::Key_Tab,            KEYCODE_TAB,
    Qt::Key_Enter,          KEYCODE_ENTER,
    Qt::Key_Escape,         KEYCODE_ESC,
    Qt::Key_BracketLeft,    KEYCODE_LBRACKET,
    Qt::Key_BracketRight,   KEYCODE_RBRACKET,
    Qt::Key_Semicolon,      KEYCODE_SEMICOLON,
    Qt::Key_Apostrophe,     KEYCODE_QUOTE,
    Qt::Key_Comma,          KEYCODE_COMMA,
    Qt::Key_Period,         KEYCODE_DOT,
    Qt::Key_Slash,          KEYCODE_SLASH,

    Qt::Key_F1,             KEYCODE_F1,
    Qt::Key_F2,             KEYCODE_F2,
    Qt::Key_F3,             KEYCODE_F3,
    Qt::Key_F4,             KEYCODE_F4,
    Qt::Key_F5,             KEYCODE_F5,
    Qt::Key_F6,             KEYCODE_F6,
    Qt::Key_F7,             KEYCODE_F7,
    Qt::Key_F8,             KEYCODE_F8,
    Qt::Key_F9,             KEYCODE_F9,
    Qt::Key_F10,            KEYCODE_F10,
    Qt::Key_F11,            KEYCODE_F11,
    Qt::Key_F12,            KEYCODE_F12,
    Qt::Key_Print,          KEYCODE_PRINT_SCREEN,
    Qt::Key_Pause,          KEYCODE_PAUSE,
    Qt::Key_Insert,         KEYCODE_INSERT,
    Qt::Key_Home,           KEYCODE_HOME,
    Qt::Key_PageUp,         KEYCODE_PGUP,
    Qt::Key_Delete,         KEYCODE_DELETE,
    Qt::Key_End,            KEYCODE_END,
    Qt::Key_PageDown,       KEYCODE_PGDN,
    Qt::Key_Up,             KEYCODE_UP,
    Qt::Key_Left,           KEYCODE_LEFT,
    Qt::Key_Down,           KEYCODE_DOWN,
    Qt::Key_Right,          KEYCODE_RIGHT,

    // keypad keys
    Qt::Key_Slash,          KEYCODE_KP_DIV,         /* ?? */
    Qt::Key_Asterisk,       KEYCODE_KP_MULT,
    Qt::Key_Enter,          KEYCODE_KP_ENTER,
    Qt::Key_Home,           KEYCODE_KP_7,
    Qt::Key_Up,             KEYCODE_KP_8,
    Qt::Key_PageUp,         KEYCODE_KP_9,
    Qt::Key_Minus,          KEYCODE_KP_MINUS,
    Qt::Key_Left,           KEYCODE_KP_4,
    Qt::Key_5,              KEYCODE_KP_5,
    Qt::Key_Right,          KEYCODE_KP_6,
    Qt::Key_Plus,           KEYCODE_KP_PLUS,
    Qt::Key_End,            KEYCODE_KP_1,
    Qt::Key_Down,           KEYCODE_KP_2,
    Qt::Key_PageDown,       KEYCODE_KP_3,
    Qt::Key_Insert,         KEYCODE_KP_0,
    Qt::Key_Delete,         KEYCODE_KP_DOT,

    // modifier keys
    Qt::Key_NumLock,        KEYCODE_NUM,
    Qt::Key_CapsLock,       KEYCODE_CAPS,
    Qt::Key_ScrollLock,     KEYCODE_SCROLL,
    Qt::Key_Shift,          KEYCODE_LSHIFT,
    Qt::Key_Control,        KEYCODE_LCTRL,
    Qt::Key_Super_L,        KEYCODE_LGUI,           /**< Left GUI */
    Qt::Key_Alt,            KEYCODE_LALT,
    Qt::Key_Shift,          KEYCODE_RSHIFT,
    Qt::Key_Control,        KEYCODE_RCTRL,
    Qt::Key_Super_R,        KEYCODE_RGUI,           /**< Right GUI */
    Qt::Key_Alt,            KEYCODE_RALT,
    Qt::Key_Menu,           KEYCODE_APPS,           /**< Applications */
    
    // ACPI key codes
    Qt::Key_PowerOff,       KEYCODE_POWER,
    Qt::Key_Sleep,          KEYCODE_SLEEP,
    Qt::Key_WakeUp,         KEYCODE_WAKEUP,
    
    // Windows multimedia key codes
    Qt::Key_MediaNext,      KEYCODE_AUD_NEXT,
    Qt::Key_MediaPrevious,  KEYCODE_AUD_PREV,
    Qt::Key_MediaStop,      KEYCODE_AUD_STOP,
    Qt::Key_MediaPlay,      KEYCODE_AUD_PLAY,
    Qt::Key_VolumeMute,     KEYCODE_AUD_MUTE,
    Qt::Key_VolumeUp,       KEYCODE_VOLUP,
    Qt::Key_VolumeDown,     KEYCODE_VOLDN,
    Qt::Key_Select,         KEYCODE_SELECT,
    Qt::Key_LaunchMail,     KEYCODE_EMAIL,
    Qt::Key_Calculator,     KEYCODE_CALC,
    Qt::Key_Explorer,       KEYCODE_COMPUTER,
    Qt::Key_Search,         KEYCODE_WWW_SEARCH,
    Qt::Key_HomePage,       KEYCODE_WWW_HOME,
    Qt::Key_Back,           KEYCODE_WWW_BACK,
    Qt::Key_Forward,        KEYCODE_WWW_FWD,
    Qt::Key_Stop,           KEYCODE_WWW_STOP,
    Qt::Key_Refresh,        KEYCODE_WWW_REFRESH,
    Qt::Key_Favorites,      KEYCODE_WWW_FAV,
    0,                    0x00
};

int LaylaOS_ScanCodes_Numlock[] = {
    Qt::Key_1,     KEYCODE_KP_1,
    Qt::Key_2,     KEYCODE_KP_2,
    Qt::Key_3,     KEYCODE_KP_3,
    Qt::Key_4,     KEYCODE_KP_4,
    Qt::Key_5,     KEYCODE_KP_5,
    Qt::Key_6,     KEYCODE_KP_6,
    Qt::Key_7,     KEYCODE_KP_7,
    Qt::Key_8,     KEYCODE_KP_8,
    Qt::Key_9,     KEYCODE_KP_9,
    Qt::Key_Plus,  KEYCODE_KP_PLUS,
    Qt::Key_Minus,  KEYCODE_KP_MINUS,
    Qt::Key_Enter, KEYCODE_KP_ENTER,
    Qt::Key_Comma, KEYCODE_KP_DOT,
    0,             0x00
};

int QLaylaOSKeyMapper::translateKeyCode(char key, bool numlockActive)
{
    int code = 0;
    int i = 0;

    if (numlockActive) {
        while (LaylaOS_ScanCodes_Numlock[i]) {
            if (key == LaylaOS_ScanCodes_Numlock[i + 1]) {
                code = LaylaOS_ScanCodes_Numlock[i];
                break;
            }
            i += 2;
        }

        if (code > 0)
            return code;
    }

    i = 0;
    while (LaylaOS_ScanCodes[i]) {
        if (key == LaylaOS_ScanCodes[i + 1]) {
            code = LaylaOS_ScanCodes[i];
            break;
        }
        i += 2;
    }

    return code;
}

QT_END_NAMESPACE
