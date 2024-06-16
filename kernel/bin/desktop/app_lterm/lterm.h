/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: lterm.h
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
 *  \file lterm.h
 *
 *  Header file for the terminal application.
 */

#ifndef LTERM_H
#define LTERM_H

#define UNUSED(a)                   (void)a

#include <termios.h>
#define __WANT_TTY_DEFCHARS_ARRAY__
#include <kernel/ttydefaults.h>

#include "../include/mouse.h"


#define STANDARD_VGA_WIDTH	    80      /**< standard VGA screen width */
#define STANDARD_VGA_HEIGHT	    25      /**< standard VGA screen height */


#define LF                      10      /* \n */
#define VT                      11      /* \v */
#define FF                      12      /* \f */
#define CR                      13      /* \r */

#define TTY_FLAG_EXCLUSIVE        0x01      /**< exclusive opening 
                                                   (open fails with EBUSY) */
#define TTY_FLAG_REVERSE_VIDEO    0x02      /**< reverse video mode */
#define TTY_FLAG_AUTOWRAP         0x04      /**< autowrap mode */
#define TTY_FLAG_CURSOR_RELATIVE  0x08      /**< cursor addressing relative to
                                                   scroll region */
#define TTY_FLAG_LFNL             0x10      /**< follow each LF/VT/FF with 
                                                   a CR */
#define TTY_FLAG_BOLD             0x20
#define TTY_FLAG_UNDERLINED       0x40
#define TTY_FLAG_BLINK            0x80
#define TTY_FLAG_BRIGHT           0x100


#define COLOR_BLACK		        0
#define COLOR_BLUE		        1
#define COLOR_GREEN		        2
#define COLOR_CYAN		        3
#define COLOR_RED		        4
#define COLOR_MAGENTA		    5
#define COLOR_BROWN		        6
#define COLOR_LIGHT_GREY	    7
#define COLOR_DARK_GREY	        8
#define COLOR_LIGHT_BLUE	    9
#define COLOR_LIGHT_GREEN	    10
#define COLOR_LIGHT_CYAN	    11
#define COLOR_LIGHT_RED	        12
#define COLOR_LIGHT_MAGENTA	    13
#define COLOR_LIGHT_BROWN	    14
#define COLOR_WHITE		        15

extern struct window_t *main_window;
extern uint32_t line_height;

extern uint8_t state;

extern int pending_refresh;

// term.c
extern int fd_master, fd_slave;
extern uint32_t terminal_width;
extern uint32_t terminal_height;
extern uint32_t terminal_row;
extern uint32_t terminal_col;
extern uint32_t first_text_row, first_visible_row, mouse_scroll_top;
extern struct winsize windowsz;

int init_terminal(char *myname, uint32_t w, uint32_t h);
void console_write(char c);
void erase_display(uint32_t width, uint32_t height, unsigned long cmd);
void repaint_cursor(void);
void repaint_dirty(void);
void repaint_all(void);
void process_mouse(int x, int y, mouse_buttons_t buttons);

// term_input.c
void process_key(char c, char modifiers);

#endif      /* LTERM_H */
