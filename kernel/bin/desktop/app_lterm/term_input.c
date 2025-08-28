/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: term_input.c
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
 *  \file term_input.c
 *
 *  Helper functions to handle terminal input. This file is part of the
 *  graphical terminal application.
 */

#include <unistd.h>
#include <kernel/kbdus.h>
#include "../include/event.h"
#include "../include/keys.h"
#include "lterm.h"


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
 * See:
 *    https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-PC-Style-Function-Keys
 */
static uint32_t keypad_keys_normal[] =
{
    0x20, 0x09, 0x0d, 0x2a, 0x2b, 0x2d, 0x2e, 
    0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 
    0x35, 0x36, 0x37, 0x38, 0x39,
};

static uint32_t keypad_keys_application[] =
{
    0x204f1b, 0x494f1b, 0x4d4f1b, 0x6a4f1b, 0x6b4f1b, 0x6d4f1b, 0x7e335b1b,
    0x6f4f1b, 0x7e325b1b, 0x464f1b, 0x425b1b, 0x7e365b1b, 0x445b1b,
    0x455b1b, 0x435b1b, 0x484f1b, 0x415b1b, 0x7e355b1b,
};


static inline void emit_codes(char *codes, int count)
{
    write(fd_master, codes, count);
}


static inline void ext_key(volatile char *codes, int c)
{
    codes[0] = '\033' /* '\e' */;
    codes[1] = '[';
    codes[2] = alt_keypad[c - KEYCODE_KP_7];
    codes[3] = '~';
}


static inline void may_scroll(void)
{
    // scroll down to cursor if needed
    if(mouse_scroll_top != first_visible_row)
    {
        mouse_scroll_top = first_visible_row;
        repaint_all();
    }
}


static inline int keystate_to_modifiers(char modifiers)
{
    int mod;
    int _alt = (modifiers & MODIFIER_MASK_ALT);
    int _ctrl = (modifiers & MODIFIER_MASK_CTRL);
    int _shift = (modifiers & MODIFIER_MASK_SHIFT);

    mod = (_alt && _ctrl) ? '7' : _ctrl ? '5' : _alt ? '3' : 0;
    if(_shift) mod++;
    if(mod == 1) mod++;

    return mod;
}


static inline void keypad_key(volatile char *codes, int index, int appmode)
{
    int count = 0;
    int scancode = appmode ? keypad_keys_application[index] :
                             keypad_keys_normal[index];

    while(scancode != 0)
    {
        codes[count++] = scancode & 0xff;
        scancode >>= 8;
    }

    emit_codes((char *)codes, count);
    may_scroll();
}


/*
 * Cursor keys send codes with a prefix that depends on whether the cursor
 * key mode is set (which sets the TTY_FLAG_APP_CURSORKEYS_MODE flag).
 * CSI is 'ESC [' and SS3 is 'ESC O'.
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
static inline void cur_key(volatile char *codes, int index, char modifiers)
{
    static char cursor_keys[] = { 'A', 'D', 'B', 'C', 'H', 'F' };
    int mod = keystate_to_modifiers(modifiers);
    int count = 2;
    codes[0] = '\033';
    codes[1] = (terminal_flags & TTY_FLAG_APP_CURSORKEYS_MODE) ? 'O' : '[';

    if(mod)
    {
        codes[count++] = '1';
        codes[count++] = ';';
        codes[count++] = mod;
    }

    codes[count++] = cursor_keys[index];
    emit_codes((char *)codes, count);
    may_scroll();
}

#if 0
static inline void cur_key(volatile char *codes, int c1, int c2)
{
    codes[0] = '\033' /* '\e' */;
    codes[1] = c1;
    codes[2] = alt_keypad[c2 - KEYCODE_KP_7];
}
#endif


void process_key(char c, char modifiers)
{
    static char *funckey_strs[] = { "15", "17", "18", "19", "20", "21", "23", "24" };
    static char esc = '\033';

    volatile char codes[8];
    volatile int count;
    int mod;
    
// suppress the -Wchar-subscripts compiler warning
#define INT(c)          ((int)c & 0xff)
    
    switch(c)
    {
        case KEYCODE_LCTRL:
        case KEYCODE_RCTRL:
        case KEYCODE_LSHIFT:
        case KEYCODE_RSHIFT:
        case KEYCODE_LALT:
        case KEYCODE_RALT:
		case KEYCODE_CAPS:
		case KEYCODE_NUM:
		case KEYCODE_SCROLL:
			return;

        /*
         * Function keys emit the following codes:
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
         * See: 
         *   https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-PC-Style-Function-Keys
         */
        case KEYCODE_F1:
        case KEYCODE_F2:
        case KEYCODE_F3:
        case KEYCODE_F4:
            mod = keystate_to_modifiers(modifiers);

            if(mod)
            {
                codes[0] = '\033' /* '\e' */;
                codes[1] = '[';
                codes[2] = '1';
                codes[3] = ';';
                codes[4] = mod;
                codes[5] = 'P' + (c - KEYCODE_F1);
                count = 6;
            }
            else
            {
                codes[0] = '\033' /* '\e' */;
                codes[1] = 'O';
                codes[2] = 'P' + (c - KEYCODE_F1);
                count = 3;
            }

            emit_codes((char *)codes, count);
            may_scroll();
            return;

        case KEYCODE_F5:
        case KEYCODE_F6:
        case KEYCODE_F7:
        case KEYCODE_F8:
        case KEYCODE_F9:
        case KEYCODE_F10:
        case KEYCODE_F11:
        case KEYCODE_F12:
            mod = keystate_to_modifiers(modifiers);
            codes[0] = '\033' /* '\e' */;
            codes[1] = '[';
            codes[2] = funckey_strs[c - KEYCODE_F5][0];
            codes[3] = funckey_strs[c - KEYCODE_F5][1];
            count = 4;

            if(mod)
            {
                codes[count++] = ';';
                codes[count++] = mod;
            }

            codes[count++] = '~';
            emit_codes((char *)codes, count);
            may_scroll();
            return;
        
        case KEYCODE_DELETE:
            keypad_key(codes, 6, 1);
            return;
        
        case KEYCODE_PGUP:
            ext_key(codes, KEYCODE_KP_9);
            count = 4;
            break;

        case KEYCODE_PGDN:
            ext_key(codes, KEYCODE_KP_3);
            count = 4;
            break;

        case KEYCODE_INSERT:
            ext_key(codes, KEYCODE_KP_0);
            count = 4;
            break;

        case KEYCODE_HOME:
            cur_key(codes, 4, modifiers);
            return;

        case KEYCODE_END:
            cur_key(codes, 5, modifiers);
            return;

        case KEYCODE_UP:
        case KEYCODE_LEFT:
        case KEYCODE_RIGHT:
        case KEYCODE_DOWN:
            cur_key(codes, c - KEYCODE_UP, modifiers);
            return;

#define __KEYPAD_MODE   \
    ((terminal_flags & TTY_FLAG_APP_KEYPAD_MODE) && !(modifiers & MODIFIER_MASK_NUM))

        case KEYCODE_SPACE:
            keypad_key(codes, 0, __KEYPAD_MODE);
            return;

        case KEYCODE_TAB:
            keypad_key(codes, 1, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_ENTER:
            keypad_key(codes, 2, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_MULT:
            keypad_key(codes, 3, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_PLUS:
            keypad_key(codes, 4, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_MINUS:
            keypad_key(codes, 5, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_DOT:
            keypad_key(codes, 6, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_DIV:
            keypad_key(codes, 7, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_0:
            keypad_key(codes, 8, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_1:
            keypad_key(codes, 9, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_2:
            keypad_key(codes, 10, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_3:
            keypad_key(codes, 11, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_4:
            keypad_key(codes, 12, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_5:
            keypad_key(codes, 13, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_6:
            keypad_key(codes, 14, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_7:
            keypad_key(codes, 15, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_8:
            keypad_key(codes, 16, __KEYPAD_MODE);
            return;

        case KEYCODE_KP_9:
            keypad_key(codes, 17, __KEYPAD_MODE);
            return;

#undef __KEYPAD_MODE
            
        default:
            if((modifiers & MODIFIER_MASK_CTRL))
            {
                // send ctrl code, e.g. CTRL-A => ASCII 01
                
                codes[0] = ctrl_char(c);
                count = 1;
                break;
            }

            if((modifiers & MODIFIER_MASK_CAPS))
            {
                if(is_caps_char(c))
                {
                    codes[0] = (modifiers & MODIFIER_MASK_SHIFT) ? 
                                            keycodes[INT(c)] :
                                            shift_keycodes[INT(c)];
                    count = 1;
                    break;
                }
            }
            
            if((modifiers & MODIFIER_MASK_SHIFT))
            {
                codes[0] = shift_keycodes[INT(c)];
                count = 1;
                break;
            }
            
            codes[0] = keycodes[INT(c)];
            count = 1;
            
            if(codes[0] == 0)
            {
                return;
            }
            
            break;
    }
    
    if((modifiers & MODIFIER_MASK_ALT))
    {
        /*
         * If the key is pressed with ALT (aka Meta key), we precede the
         * key code(s) by an extra ESC char.
         *
         * TODO: support the other possibility, which is setting the high
         *       order bit of the char (see `man setmetamode` for more).
         */
        emit_codes(&esc, 1);
    }

    emit_codes((char *)codes, count);
    may_scroll();
}

