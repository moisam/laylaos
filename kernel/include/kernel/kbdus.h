/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: kbdus.h
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
 *  \file kbdus.h
 *
 *  US Keyboard tables. Of the files including this header file, only one 
 *  must define KBDUS_DEFINE_KEYCODES to define the tables. All other files
 *  not defining KBDUS_DEFINE_KEYCODES will include extern declarations of
 *  the tables.
 */

#include <kernel/keycodes.h>


#ifdef KBDUS_DEFINE_KEYCODES

char keycodes[] =
{
      0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2',
    '3', '4', '5', '6', '7', '8', '9', '`', '-', '=', '\\', '\b', ' ', '\t',
    '\n', '\033', '[', ']', ';', '\'', ',', '.', '/',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,       // caps, shift, ctrl, alt, gui, apps
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', // function keys
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // prntscrn, scroll, ...
    '/', '*', '\n',                      // keypad
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',    // keypad
};


char shift_keycodes[] =
{
      0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ')', '!', '@',
    '#', '$', '%', '^', '&', '*', '(', '~', '_', '+', '|', '\b', ' ', '\t',
    '\n', '\033', '{', '}', ':', '"', '<', '>', '?',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,       // caps, shift, ctrl, alt, gui, apps
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', // function keys
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // prntscrn, scroll, ...
    '/', '*', '\n',                      // keypad
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',    // keypad
};


int alt_keypad[] =
{
    'H', 'A', '5', '-', 'D', 'G', 'C', '+', 'Y', 'B', '6', '2', '3'
};


/*
 * Most of the keycodes in the table below are 1-4 bytes long, which fit in a
 * 4-byte var. Some (e.g. CTRL-arrow keys) need longer keycodes, for example,
 * CTRL-Right results in '^[[1;5C', which is 6 bytes. For these, we define a
 * "prologue" of bytes, which is '^[[1;'. Whenever the keyboard driver 
 * encounters such a key, it emits the prologue and then the rest (e.g. '5C' 
 * for CTRL-Right).
 */

#define CTRL_ARROW_PROLOGUE             0x3b315b1b


#include <kernel/bits/kbd-defs.h>

struct keytable_t kbdus =
{
  "us",
  {
      /* name                 normal shift ctrl  sctrl num   snum  caps  scaps */
    { /* NULL              */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_A         */ 0x61,  0x41, 0x01, 0x01, 0x61, 0x41, 0x41, 0x61, },
    { /* KEYCODE_B         */ 0x62,  0x42, 0x02, 0x02, 0x62, 0x42, 0x42, 0x62, },
    { /* KEYCODE_C         */ 0x63,  0x43, 0x03, 0x03, 0x63, 0x43, 0x43, 0x63, },
    { /* KEYCODE_D         */ 0x64,  0x44, 0x04, 0x04, 0x64, 0x44, 0x44, 0x64, },
    { /* KEYCODE_E         */ 0x65,  0x45, 0x05, 0x05, 0x65, 0x45, 0x45, 0x65, },
    { /* KEYCODE_F         */ 0x66,  0x46, 0x06, 0x06, 0x66, 0x46, 0x46, 0x66, },
    { /* KEYCODE_G         */ 0x67,  0x47, 0x07, 0x07, 0x67, 0x47, 0x47, 0x67, },
    { /* KEYCODE_H         */ 0x68,  0x48, 0x08, 0x08, 0x68, 0x48, 0x48, 0x68, },
    { /* KEYCODE_I         */ 0x69,  0x49, 0x09, 0x09, 0x69, 0x49, 0x49, 0x69, },
    { /* KEYCODE_J         */ 0x6a,  0x4a, 0x0a, 0x0a, 0x6a, 0x4a, 0x4a, 0x6a, },
    { /* KEYCODE_K         */ 0x6b,  0x4b, 0x0b, 0x0b, 0x6b, 0x4b, 0x4b, 0x6b, },
    { /* KEYCODE_L         */ 0x6c,  0x4c, 0x0c, 0x0c, 0x6c, 0x4c, 0x4c, 0x6c, },
    { /* KEYCODE_M         */ 0x6d,  0x4d, 0x0d, 0x0d, 0x6d, 0x4d, 0x4d, 0x6d, },
    { /* KEYCODE_N         */ 0x6e,  0x4e, 0x0e, 0x0e, 0x6e, 0x4e, 0x4e, 0x6e, },
    { /* KEYCODE_O         */ 0x6f,  0x4f, 0x0f, 0x0f, 0x6f, 0x4f, 0x4f, 0x6f, },
    { /* KEYCODE_P         */ 0x70,  0x50, 0x10, 0x10, 0x70, 0x50, 0x50, 0x70, },
    { /* KEYCODE_Q         */ 0x71,  0x51, 0x11, 0x11, 0x71, 0x51, 0x51, 0x71, },
    { /* KEYCODE_R         */ 0x72,  0x52, 0x12, 0x12, 0x72, 0x52, 0x52, 0x72, },
    { /* KEYCODE_S         */ 0x73,  0x53, 0x13, 0x13, 0x73, 0x53, 0x53, 0x73, },
    { /* KEYCODE_T         */ 0x74,  0x54, 0x14, 0x14, 0x74, 0x54, 0x54, 0x74, },
    { /* KEYCODE_U         */ 0x75,  0x55, 0x15, 0x15, 0x75, 0x55, 0x55, 0x75, },
    { /* KEYCODE_V         */ 0x76,  0x56, 0x16, 0x16, 0x76, 0x56, 0x56, 0x76, },
    { /* KEYCODE_W         */ 0x77,  0x57, 0x17, 0x17, 0x77, 0x57, 0x57, 0x77, },
    { /* KEYCODE_X         */ 0x78,  0x58, 0x18, 0x18, 0x78, 0x58, 0x58, 0x78, },
    { /* KEYCODE_Y         */ 0x79,  0x59, 0x19, 0x19, 0x79, 0x59, 0x59, 0x79, },
    { /* KEYCODE_Z         */ 0x7a,  0x5a, 0x1a, 0x1a, 0x7a, 0x5a, 0x5a, 0x7a, },
    { /* KEYCODE_0         */ 0x30,  0x29,    0,    0, 0x30, 0x29, 0x30, 0x29, },
    { /* KEYCODE_1         */ 0x31,  0x21,    0,    0, 0x31, 0x21, 0x31, 0x21, },
    { /* KEYCODE_2         */ 0x32,  0x40,    0,    0, 0x32, 0x40, 0x32, 0x40, },
    { /* KEYCODE_3         */ 0x33,  0x23,    0,    0, 0x33, 0x23, 0x33, 0x23, },
    { /* KEYCODE_4         */ 0x34,  0x24,    0,    0, 0x34, 0x24, 0x34, 0x24, },
    { /* KEYCODE_5         */ 0x35,  0x25,    0,    0, 0x35, 0x25, 0x35, 0x25, },
    { /* KEYCODE_6         */ 0x36,  0x5e,    0,    0, 0x36, 0x5e, 0x36, 0x5e, },
    { /* KEYCODE_7         */ 0x37,  0x26,    0,    0, 0x37, 0x26, 0x37, 0x26, },
    { /* KEYCODE_8         */ 0x38,  0x2a,    0,    0, 0x38, 0x2a, 0x38, 0x2a, },
    { /* KEYCODE_9         */ 0x39,  0x28,    0,    0, 0x39, 0x28, 0x39, 0x28, },
    { /* KEYCODE_BACKTICK  */ 0x60,  0x7e,    0,    0, 0x60, 0x7e, 0x60, 0x7e, },
    { /* KEYCODE_MINUS     */ 0x2d,  0x5f, 0x1f, 0x1f, 0x2d, 0x5f, 0x2d, 0x5f, },
    { /* KEYCODE_EQUAL     */ 0x3d,  0x2b,    0,    0, 0x3d, 0x2b, 0x3d, 0x2b, },
    { /* KEYCODE_BACKSLASH */ 0x5c,  0x7c, 0x1c, 0x1c, 0x5c, 0x7c, 0x5c, 0x7c, },
    { /* KEYCODE_BACKSPACE */ 0x08,  0x08, 0x7e335b1b, 0x7e335b1b, 0x08, 0x08, 0x08, 0x08, },
    { /* KEYCODE_SPACE     */ 0x20,  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, },
    { /* KEYCODE_TAB       */ 0x09,  0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, },
    { /* KEYCODE_ENTER     */ 0x0d,  0x0d, 0x0a, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, },
    { /* KEYCODE_ESC       */ 0x1b,  0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, },
    { /* KEYCODE_LBRACKET  */ 0x5b,  0x7b, 0x1b, 0x1b, 0x5b, 0x7b, 0x5b, 0x7b, },
    { /* KEYCODE_RBRACKET  */ 0x5d,  0x7d, 0x1d, 0x1d, 0x5d, 0x7d, 0x5d, 0x7d, },
    { /* KEYCODE_SEMICOLON */ 0x3b,  0x3a,    0,    0, 0x3b, 0x3a, 0x3b, 0x3a, },
    { /* KEYCODE_QUOTE     */ 0x27,  0x22,    0,    0, 0x27, 0x22, 0x27, 0x22, },
    { /* KEYCODE_COMMA     */ 0x2c,  0x3c,    0,    0, 0x2c, 0x3c, 0x2c, 0x3c, },
    { /* KEYCODE_DOT       */ 0x2e,  0x3e,    0,    0, 0x2e, 0x3e, 0x2e, 0x3e, },
    { /* KEYCODE_SLASH     */ 0x2f,  0x3f,    0,    0, 0x2f, 0x3f, 0x2f, 0x3f, },

    { /* KEYCODE_CAPS      */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_LSHIFT    */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_LCTRL     */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_LGUI      */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_LALT      */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_RSHIFT    */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_RCTRL     */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_RGUI      */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_RALT      */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_APPS      */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_F1        */ 0x415b5b1b, 0x415b5b1b, 0x415b5b1b, 0x415b5b1b, 0x415b5b1b, 0x415b5b1b, 0x415b5b1b, 0x415b5b1b, },
    { /* KEYCODE_F2        */ 0x425b5b1b, 0x425b5b1b, 0x425b5b1b, 0x425b5b1b, 0x425b5b1b, 0x425b5b1b, 0x425b5b1b, 0x425b5b1b, },
    { /* KEYCODE_F3        */ 0x435b5b1b, 0x435b5b1b, 0x435b5b1b, 0x435b5b1b, 0x435b5b1b, 0x435b5b1b, 0x435b5b1b, 0x435b5b1b, },
    { /* KEYCODE_F4        */ 0x445b5b1b, 0x445b5b1b, 0x445b5b1b, 0x445b5b1b, 0x445b5b1b, 0x445b5b1b, 0x445b5b1b, 0x445b5b1b, },
    { /* KEYCODE_F5        */ 0x455b5b1b, 0x455b5b1b, 0x455b5b1b, 0x455b5b1b, 0x455b5b1b, 0x455b5b1b, 0x455b5b1b, 0x455b5b1b, },
    { /* KEYCODE_F6        */ 0x465b5b1b, 0x465b5b1b, 0x465b5b1b, 0x465b5b1b, 0x465b5b1b, 0x465b5b1b, 0x465b5b1b, 0x465b5b1b, },
    { /* KEYCODE_F7        */ 0x475b5b1b, 0x475b5b1b, 0x475b5b1b, 0x475b5b1b, 0x475b5b1b, 0x475b5b1b, 0x475b5b1b, 0x475b5b1b, },
    { /* KEYCODE_F8        */ 0x485b5b1b, 0x485b5b1b, 0x485b5b1b, 0x485b5b1b, 0x485b5b1b, 0x485b5b1b, 0x485b5b1b, 0x485b5b1b, },
    { /* KEYCODE_F9        */ 0x495b5b1b, 0x495b5b1b, 0x495b5b1b, 0x495b5b1b, 0x495b5b1b, 0x495b5b1b, 0x495b5b1b, 0x495b5b1b, },
    { /* KEYCODE_F10       */ 0x4a5b5b1b, 0x4a5b5b1b, 0x4a5b5b1b, 0x4a5b5b1b, 0x4a5b5b1b, 0x4a5b5b1b, 0x4a5b5b1b, 0x4a5b5b1b, },
    { /* KEYCODE_F11       */ 0x4b5b5b1b, 0x4b5b5b1b, 0x4b5b5b1b, 0x4b5b5b1b, 0x4b5b5b1b, 0x4b5b5b1b, 0x4b5b5b1b, 0x4b5b5b1b, },
    { /* KEYCODE_F12       */ 0x4c5b5b1b, 0x4c5b5b1b, 0x4c5b5b1b, 0x4c5b5b1b, 0x4c5b5b1b, 0x4c5b5b1b, 0x4c5b5b1b, 0x4c5b5b1b, },
    { /* KEYCODE_PRINT_SCREEN */ 0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_SCROLL    */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_PAUSE     */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_INSERT    */ 0x7e325b1b, 0x7e325b1b, 0x7e325b1b, 0x7e325b1b, 0x7e325b1b, 0x7e325b1b, 0x7e325b1b, 0x7e325b1b, },
    { /* KEYCODE_HOME      */ 0x7e315b1b, 0x7e315b1b, 0x7e315b1b, 0x7e315b1b, 0x7e315b1b, 0x7e315b1b, 0x7e315b1b, 0x7e315b1b, 
                           /* 0x00485b1b, 0x00485b1b, 0x00485b1b, 0x00485b1b, 0x00485b1b, 0x00485b1b, 0x00485b1b, 0x00485b1b, */ },
    { /* KEYCODE_PGUP      */ 0x7e355b1b, 0x7e355b1b, 0x7e355b1b, 0x7e355b1b, 0x7e355b1b, 0x7e355b1b, 0x7e355b1b, 0x7e355b1b, },
    { /* KEYCODE_DELETE    */ 0x7e335b1b, 0x7e335b1b, 0x7e335b1b, 0x7e335b1b, 0x7e335b1b, 0x7e335b1b, 0x7e335b1b, 0x7e335b1b, },
    { /* KEYCODE_END       */ 0x7e345b1b, 0x7e345b1b, 0x7e345b1b, 0x7e345b1b, 0x7e345b1b, 0x7e345b1b, 0x7e345b1b, 0x7e345b1b,
                           /* 0x00595b1b, 0x00595b1b, 0x00595b1b, 0x00595b1b, 0x00595b1b, 0x00595b1b, 0x00595b1b, 0x00595b1b, */ },
    { /* KEYCODE_PGDN      */ 0x7e365b1b, 0x7e365b1b, 0x7e365b1b, 0x7e365b1b, 0x7e365b1b, 0x7e365b1b, 0x7e365b1b, 0x7e365b1b, },
    { /* KEYCODE_UP        */ 0x00415b1b, 0x00004132, 0x00004135, 0x00004136, 0x00415b1b, 0x00004132, 0x00415b1b, 0x00004132, },
    { /* KEYCODE_LEFT      */ 0x00445b1b, 0x00004432, 0x00004435, 0x00004436, 0x00445b1b, 0x00004432, 0x00445b1b, 0x00004432, },
    { /* KEYCODE_DOWN      */ 0x00425b1b, 0x00004232, 0x00004235, 0x00004236, 0x00425b1b, 0x00004232, 0x00425b1b, 0x00004232, },
    { /* KEYCODE_RIGHT     */ 0x00435b1b, 0x00004332, 0x00004335, 0x00004336, 0x00435b1b, 0x00004332, 0x00435b1b, 0x00004332, },
    { /* KEYCODE_NUM       */ 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
    { /* KEYCODE_KP_DIV    */ 0x2f,  0x3f,    0,    0, 0x2f, 0x3f, 0x2f, 0x3f, },
    { /* KEYCODE_KP_MULT   */ 0x2a,  0x2a,    0,    0, 0x2a,    0, 0x2a,    0, },
    { /* KEYCODE_KP_ENTER  */ 0x0d,  0x0d, 0x0a, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, },

    { /* KEYCODE_KP_7      */ 0x00485b1b, 0x37, 0x00485b1b, 0x00485b1b, 0x37, 0x00485b1b, 0x00485b1b, 0x37, },
    { /* KEYCODE_KP_8      */ 0x00415b1b, 0x38, 0x00415b1b, 0x00415b1b, 0x38, 0x00415b1b, 0x00415b1b, 0x38, },
    { /* KEYCODE_KP_9      */ 0x7e355b1b, 0x39, 0x7e355b1b, 0x7e355b1b, 0x39, 0x7e355b1b, 0x7e355b1b, 0x39, },
    { /* KEYCODE_KP_MINUS  */ 0x2d,  0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, },
    { /* KEYCODE_KP_4      */ 0x00445b1b, 0x34, 0x00445b1b, 0x00445b1b, 0x34, 0x00445b1b, 0x00445b1b, 0x34, },
    { /* KEYCODE_KP_5      */ 0x00475b1b, 0x35, 0x00475b1b, 0x00475b1b, 0x35, 0x00475b1b, 0x00475b1b, 0x35, },
    { /* KEYCODE_KP_6      */ 0x00435b1b, 0x36, 0x00435b1b, 0x00435b1b, 0x36, 0x00435b1b, 0x00435b1b, 0x36, },
    { /* KEYCODE_KP_PLUS   */ 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, },
    { /* KEYCODE_KP_1      */ 0x00595b1b, 0x31, 0x00595b1b, 0x00595b1b, 0x31, 0x00595b1b, 0x00595b1b, 0x31, },
    { /* KEYCODE_KP_2      */ 0x00425b1b, 0x32, 0x00425b1b, 0x00425b1b, 0x32, 0x00425b1b, 0x00425b1b, 0x32, },
    { /* KEYCODE_KP_3      */ 0x7e365b1b, 0x33, 0x7e365b1b, 0x7e365b1b, 0x33, 0x7e365b1b, 0x7e365b1b, 0x33, },
    { /* KEYCODE_KP_0      */ 0x7e325b1b, 0x30, 0x7e325b1b, 0x7e325b1b, 0x30, 0x7e325b1b, 0x7e325b1b, 0x30, },
    { /* KEYCODE_KP_DOT    */ 0x7e335b1b, 0x2e, 0x7e335b1b, 0x7e335b1b, 0x2e, 0x7e335b1b, 0x7e335b1b, 0x2e, },
    
    /* ACPI key codes */
    { /* KEYCODE_POWER     */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_SLEEP     */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_WAKEUP    */    0,     0,    0,    0,    0,    0,    0,    0, },
    
    /* Windows multimedia key codes */
    { /* KEYCODE_AUD_NEXT  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_AUD_PREV  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_AUD_STOP  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_AUD_PLAY  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_AUD_MUTE  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_VOLUP     */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_VOLDN     */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_SELECT    */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_EMAIL     */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_CALC      */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_COMPUTER  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_WWW_SEARCH */   0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_WWW_HOME  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_WWW_BACK  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_WWW_FWD   */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_WWW_STOP  */    0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_WWW_REFRESH */  0,     0,    0,    0,    0,    0,    0,    0, },
    { /* KEYCODE_WWW_FAV   */    0,     0,    0,    0,    0,    0,    0,    0, },
  },
};

#else

extern char keycodes[];
extern char shift_keycodes[];
extern int alt_keypad[];

#endif      /* KBDUS_DEFINE_KEYCODES */


/*
 * Check if the given scancode becomes a control char when pressed with CTRL,
 * e.g. CTRL-C (which should emit keycode 0x03). We use the scancode as an index
 * to the keycodes[] array, instead of the keycode, so that we return the correct
 * control char regardless of the keyboard layout used.
 */
static inline int ctrl_char(int c)
{
    // TODO: we are missing the codes for @, ^, _
    
    if(c >= KEYCODE_A && c <= KEYCODE_Z)
    {
        return keycodes[c] - keycodes[KEYCODE_A /* 'a' */] + 1;
    }
    
    if(c == KEYCODE_LBRACKET || c == KEYCODE_RBRACKET || c == KEYCODE_BACKSLASH)
    {
        return keycodes[c] - keycodes[KEYCODE_LBRACKET /* '[' */] + 27;
    }
    
    return keycodes[c];
}


static inline int is_caps_char(int c)
{
    return (c >= KEYCODE_A && c <= KEYCODE_Z);
}

