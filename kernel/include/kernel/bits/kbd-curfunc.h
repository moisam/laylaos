/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: kbd-curfunc.h
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
 *  \file kbd-curfunc.h
 *
 *  Definitions for cursor keys and function keys F1-F12.
 */

#ifndef KBDUS_DEFINE_KEYCODES
#error You must not include this file directly. Use kbdus.h instead.
#endif

/*
 * Cursor keys send codes with a prefix that depends on whether the cursor
 * key mode is set (which sets the TTY_FLAG_APP_CURSORKEYS_MODE flag in the
 * tty struct). CSI is 'ESC [' and SS3 is 'ESC O'.
 *
 * In the struct keytable_t, such keys have 0xfe in the least 
 * significant byte to indicate they use the cursor key table. The second 
 * least significant byte is an index to the cursor key table. The driver then
 * emits the prefix as appropriate according to the current key mode, followed
 * by the given char from the cursor key table.
 *
 *    Key            Normal     Application
 *    -------------+----------+-------------
 *    Cursor Up    | CSI A    | SS3 A
 *    Cursor Down  | CSI B    | SS3 B
 *    Cursor Right | CSI C    | SS3 C
 *    Cursor Left  | CSI D    | SS3 D
 *    Home         | CSI H    | SS3 H
 *    End          | CSI F    | SS3 F
 *    -------------+----------+-------------
 *
 * See:
 *    https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-PC-Style-Function-Keys
 */
uint32_t cursor_keys[] =
{
    'A', // Up
    'B', // Down
    'C', // Right
    'D', // Left
    'H', // Home
    'F', // End
};

/*
 * Function keys send the following codes:
 *
 *    Key        Escape Sequence
 *    ---------+-----------------
 *    F1       | SS3 P
 *    F2       | SS3 Q
 *    F3       | SS3 R
 *    F4       | SS3 S
 *    F5       | CSI 1 5 ~
 *    F6       | CSI 1 7 ~
 *    F7       | CSI 1 8 ~
 *    F8       | CSI 1 9 ~
 *    F9       | CSI 2 0 ~
 *    F10      | CSI 2 1 ~
 *    F11      | CSI 2 3 ~
 *    F12      | CSI 2 4 ~
 *    ---------+-----------------
 *
 * We define the prologue (SS3 or CSI) for the normal state (i.e. without 
 * modifiers), and another prologue for when there are modifiers. This is
 * because F1-F4 send different sequences to the rest of the function keys.
 *
 * In the struct keytable_t, such keys have 0xfd in the least 
 * significant byte to indicate they are function keys. The second 
 * least significant byte is an index to the tables below. The driver then
 * emits the prefix as appropriate according to the modifiers, followed
 * by the given char from the funckey_lastchar table below.
 *
 * See: same source as above.
 */
struct funckey_sequence_t
{
    char *seq;
    int len;
};

struct funckey_sequence_t funckey_prologue_normal[] =
{
    { "\033O", 2 }, { "\033O", 2 }, { "\033O", 2 }, { "\033O", 2 },
    { "\033[15", 4 }, { "\033[17", 4 }, { "\033[18", 4 }, { "\033[19", 4 }, 
    { "\033[20", 4 }, { "\033[21", 4 }, { "\033[23", 4 }, { "\033[24", 4 },
};

struct funckey_sequence_t funckey_prologue_modifiers[] =
{
    { "\033[1", 3 }, { "\033[1", 3 }, { "\033[1", 3 }, { "\033[1", 3 }, 
    { "\033[15", 4 }, { "\033[17", 4 }, { "\033[18", 4 }, { "\033[19", 4 }, 
    { "\033[20", 4 }, { "\033[21", 4 }, { "\033[23", 4 }, { "\033[24", 4 },
};

char funckey_lastchar[] =
{
    'P', 'Q', 'R', 'S', '~', '~', '~', '~', '~', '~', '~', '~',
};

/*
 * Keypad keys send the following codes:
 *
 *    Key              Numeric    Application
 *    ---------------+----------+-------------
 *    Space          | SP       | SS3 SP      
 *    Tab            | TAB      | SS3 I       
 *    Enter          | CR       | SS3 M       
 *    * (multiply)   | *        | SS3 j       
 *    + (add)        | +        | SS3 k       
 *    - (minus)      | -        | SS3 m       
 *    . (Delete)     | .        | CSI 3 ~     
 *    / (divide)     | /        | SS3 o       
 *    0 (Insert)     | 0        | CSI 2 ~     
 *    1 (End)        | 1        | SS3 F       
 *    2 (DownArrow)  | 2        | CSI B       
 *    3 (PageDown)   | 3        | CSI 6 ~     
 *    4 (LeftArrow)  | 4        | CSI D       
 *    5 (Begin)      | 5        | CSI E       
 *    6 (RightArrow) | 6        | CSI C       
 *    7 (Home)       | 7        | SS3 H       
 *    8 (UpArrow)    | 8        | CSI A       
 *    9 (PageUp)     | 9        | CSI 5 ~     
 *    ---------------+----------+-------------
 *
 * In the struct keytable_t, such keys have 0xfc in the least 
 * significant byte to indicate they are cursor keys. The second 
 * least significant byte is an index to the tables below. The driver then
 * emits the codes as appropriate according to the keypad key state.
 *
 * See: same source as above.
 */
uint32_t keypad_keys_normal[] =
{
    0x20, 0x09, 0x0d, 0x2a, 0x2b, 0x2d, 0x2e, 
    0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 
    0x35, 0x36, 0x37, 0x38, 0x39,
};

uint32_t keypad_keys_application[] =
{
    0x20 /* 0x204f1b */, 0x09 /* 0x494f1b */, 0x0d /* 0x4d4f1b */, 0x6a4f1b, 0x6b4f1b, 0x6d4f1b, 0x7e335b1b,
    0x6f4f1b, 0x7e325b1b, 0x464f1b, 0x425b1b, 0x7e365b1b, 0x445b1b,
    0x455b1b, 0x435b1b, 0x484f1b, 0x415b1b, 0x7e355b1b,
};

