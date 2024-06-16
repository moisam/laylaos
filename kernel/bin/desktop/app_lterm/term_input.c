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


static inline void cur_key(volatile char *codes, int c)
{
    codes[0] = '\033' /* '\e' */;
    codes[1] = '[';
    codes[2] = alt_keypad[c - KEYCODE_KP_7];
}


void process_key(char c, char modifiers)
{
    static char esc = '\033';
    volatile char codes[8];
    volatile int count;
    
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

        // function keys emit 4 codes:
        // F1 => ESC [ [ 'A'
        // F2 => ESC [ [ 'B'
        // ...
        case KEYCODE_F1:
        case KEYCODE_F2:
        case KEYCODE_F3:
        case KEYCODE_F4:
        case KEYCODE_F5:
        case KEYCODE_F6:
        case KEYCODE_F7:
        case KEYCODE_F8:
        case KEYCODE_F9:
        case KEYCODE_F10:
        case KEYCODE_F11:
        case KEYCODE_F12:
            codes[0] = '\033' /* '\e' */;
            codes[1] = '[';
            codes[2] = '[';
            codes[3] = keycodes[INT(c)];
            count = 4;
            break;
        
        case KEYCODE_DELETE:
            ext_key(codes, KEYCODE_KP_DOT);
            count = 4;
            break;
        
        case KEYCODE_KP_0:
            /* fallthrough */

        case KEYCODE_KP_3:
            /* fallthrough */

        case KEYCODE_KP_9:
            /* fallthrough */

        case KEYCODE_KP_DOT:
            if(!(modifiers & MODIFIER_MASK_NUM) ||
                (modifiers & MODIFIER_MASK_SHIFT))     // same as KEYCODE_DELETE
            {
                ext_key(codes, c);
                count = 4;
            }
            else
            {
                codes[0] = keycodes[INT(c)];
                count = 1;
            }
            break;

        // keypad keys:
        //   extended scancodes => cursor movement
        //   without num lock => cursor movement
        //   with shift on => cursor movement
        //   otherwise, normal numeric keypad

        case KEYCODE_KP_MINUS:
            /* fallthrough */

        case KEYCODE_KP_PLUS:
            /* fallthrough */

        case KEYCODE_KP_1:
            /* fallthrough */

        case KEYCODE_KP_2:
            /* fallthrough */

        case KEYCODE_KP_4:
            /* fallthrough */

        case KEYCODE_KP_5:
            /* fallthrough */

        case KEYCODE_KP_6:
            /* fallthrough */

        case KEYCODE_KP_7:
            /* fallthrough */

        case KEYCODE_KP_8:
            if(!(modifiers & MODIFIER_MASK_NUM) ||
                (modifiers & MODIFIER_MASK_SHIFT))     // same as KEYCODE_HOME
            {
                cur_key(codes, c);
                count = 3;
            }
            else
            {
                codes[0] = keycodes[INT(c)];
                count = 1;
            }
            break;

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
            cur_key(codes, KEYCODE_KP_7);
            count = 3;
            break;

        case KEYCODE_END:
            cur_key(codes, KEYCODE_KP_1);
            count = 3;
            break;

        case KEYCODE_UP:
            cur_key(codes, KEYCODE_KP_8);
            count = 3;
            break;

        case KEYCODE_LEFT:
            cur_key(codes, KEYCODE_KP_4);
            count = 3;
            break;

        case KEYCODE_RIGHT:
            cur_key(codes, KEYCODE_KP_6);
            count = 3;
            break;

        case KEYCODE_DOWN:
            cur_key(codes, KEYCODE_KP_2);
            count = 3;
            break;
            
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

    // scroll down to cursor if needed
    if(mouse_scroll_top != first_visible_row)
    {
        mouse_scroll_top = first_visible_row;
        repaint_all();
    }
}

