/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: key-names.c
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
 *  \file key-names.c
 *
 *  This file contains key names in string format.
 */

#include "../include/keys.h"

// Key names formatted as strings
char *long_key_names[] =
{
    [KEYCODE_A] = "A",
    [KEYCODE_B] = "B",
    [KEYCODE_C] = "C",
    [KEYCODE_D] = "D",
    [KEYCODE_E] = "E",
    [KEYCODE_F] = "F",
    [KEYCODE_G] = "G",
    [KEYCODE_H] = "H",
    [KEYCODE_I] = "I",
    [KEYCODE_J] = "J",
    [KEYCODE_K] = "K",
    [KEYCODE_L] = "L",
    [KEYCODE_M] = "M",
    [KEYCODE_N] = "N",
    [KEYCODE_O] = "O",
    [KEYCODE_P] = "P",
    [KEYCODE_Q] = "Q",
    [KEYCODE_R] = "R",
    [KEYCODE_S] = "S",
    [KEYCODE_T] = "T",
    [KEYCODE_U] = "U",
    [KEYCODE_V] = "V",
    [KEYCODE_W] = "W",
    [KEYCODE_X] = "X",
    [KEYCODE_Y] = "Y",
    [KEYCODE_Z] = "Z",
    [KEYCODE_0] = "0",
    [KEYCODE_1] = "1",
    [KEYCODE_2] = "2",
    [KEYCODE_3] = "3",
    [KEYCODE_4] = "4",
    [KEYCODE_5] = "5",
    [KEYCODE_6] = "6",
    [KEYCODE_7] = "7",
    [KEYCODE_8] = "8",
    [KEYCODE_9] = "9",
    [KEYCODE_BACKTICK] = "`",
    [KEYCODE_MINUS] = "-",
    [KEYCODE_EQUAL] = "=",
    [KEYCODE_BACKSLASH] = "\\",
    [KEYCODE_BACKSPACE] = "Backspace",
    [KEYCODE_SPACE] = "Space",
    [KEYCODE_TAB] = "Tab",
    [KEYCODE_ENTER] = "Enter",
    [KEYCODE_ESC] = "Esc",
    [KEYCODE_LBRACKET] = "[",
    [KEYCODE_RBRACKET] = "]",
    [KEYCODE_SEMICOLON] = ";",
    [KEYCODE_QUOTE] = "'",
    [KEYCODE_COMMA] = ",",
    [KEYCODE_DOT] = ".",
    [KEYCODE_SLASH] = "/",

    [KEYCODE_CAPS] = "CapsLock",
    [KEYCODE_LSHIFT] = "Left Shift",
    [KEYCODE_LCTRL] = "Left Ctrl",
    [KEYCODE_LGUI] = "Left GIU",
    [KEYCODE_LALT] = "Left Alt",
    [KEYCODE_RSHIFT] = "Right Shift",
    [KEYCODE_RCTRL] = "Right Ctrl",
    [KEYCODE_RGUI] = "Right GIU",
    [KEYCODE_RALT] = "Right Alt",
    [KEYCODE_APPS] = "Applications",
    [KEYCODE_F1] = "F1",
    [KEYCODE_F2] = "F2",
    [KEYCODE_F3] = "F3",
    [KEYCODE_F4] = "F4",
    [KEYCODE_F5] = "F5",
    [KEYCODE_F6] = "F6",
    [KEYCODE_F7] = "F7",
    [KEYCODE_F8] = "F8",
    [KEYCODE_F9] = "F9",
    [KEYCODE_F10] = "F10",
    [KEYCODE_F11] = "F11",
    [KEYCODE_F12] = "F12",
    [KEYCODE_PRINT_SCREEN] = "PrntScrn",
    [KEYCODE_SCROLL] = "ScrlLock",
    [KEYCODE_PAUSE] = "Pause",
    [KEYCODE_INSERT] = "Insert",
    [KEYCODE_HOME] = "Home",
    [KEYCODE_PGUP] = "PgUp",
    [KEYCODE_DELETE] = "Delete",
    [KEYCODE_END] = "End",
    [KEYCODE_PGDN] = "PgDn",
    [KEYCODE_UP] = "Up",
    [KEYCODE_LEFT] = "Left",
    [KEYCODE_DOWN] = "Down",
    [KEYCODE_RIGHT] = "Right",
    [KEYCODE_NUM] = "NumLock",
    [KEYCODE_KP_DIV] = "Keypad /",
    [KEYCODE_KP_MULT] = "Keypad *",
    [KEYCODE_KP_ENTER] = "Keypad Enter",

    [KEYCODE_KP_7] = "Keypad 7",
    [KEYCODE_KP_8] = "Keypad 8",
    [KEYCODE_KP_9] = "Keypad 9",
    [KEYCODE_KP_MINUS] = "Keypad -",
    [KEYCODE_KP_4] = "Keypad 4",
    [KEYCODE_KP_5] = "Keypad 5",
    [KEYCODE_KP_6] = "Keypad 6",
    [KEYCODE_KP_PLUS] = "Keypad +",
    [KEYCODE_KP_1] = "Keypad 1",
    [KEYCODE_KP_2] = "Keypad 2",
    [KEYCODE_KP_3] = "Keypad 3",
    [KEYCODE_KP_0] = "Keypad 0",
    [KEYCODE_KP_DOT] = "Keypad .",
    
    /* ACPI key codes */
    [KEYCODE_POWER] = "ACPI Power",
    [KEYCODE_SLEEP] = "ACPI Sleep",
    [KEYCODE_WAKEUP] = "ACPI WakeUp",
    
    /* Windows multimedia key codes */
    [KEYCODE_AUD_NEXT] = "Audio Next",
    [KEYCODE_AUD_PREV] = "Audio Prev",
    [KEYCODE_AUD_STOP] = "Audio Stop",
    [KEYCODE_AUD_PLAY] = "Audio Play",
    [KEYCODE_AUD_MUTE] = "Audio Mute",
    [KEYCODE_VOLUP] = "VolUp",
    [KEYCODE_VOLDN] = "VolDn",
    [KEYCODE_SELECT] = "Select",
    [KEYCODE_EMAIL] = "Email",
    [KEYCODE_CALC] = "Calc",
    [KEYCODE_COMPUTER] = "Computer",
    [KEYCODE_WWW_SEARCH] = "WWW Search",
    [KEYCODE_WWW_HOME] = "WWW Home",
    [KEYCODE_WWW_BACK] = "WWW Back",
    [KEYCODE_WWW_FWD] = "WWW Forward",
    [KEYCODE_WWW_STOP] = "WWW Stop",
    [KEYCODE_WWW_REFRESH] = "WWW Refresh",
    [KEYCODE_WWW_FAV] = "WWW Favorites",
};


char *get_long_key_name(int key)
{
    if(key < KEYCODE_A || key > KEYCODE_WWW_FAV)
    {
        return "Unknown";
    }
    
    return long_key_names[key];
}

