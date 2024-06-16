/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: tty_state.c
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
 *  \file tty_state.c
 *
 *  The terminal (tty) device driver implementation.
 *  The driver's code is split between these files:
 *    - tty.c => device initialisation, general interface, and read/write 
 *               functions
 *    - tty_input.c => handling terminal input
 *    - tty_ioctl.c => terminal device control (ioctl)
 *    - tty_state.c => saving and restoring device state
 */

#include <errno.h>
#include <signal.h>
#include <kernel/vga.h>
#include <kernel/tty.h>
#include <gui/vbe.h>


void save_tty_cursor_state(struct tty_t *tty)
{
    tty->saved_state.cursor_shown = tty->cursor_shown;
    tty->saved_state.cursor_enabled = tty->cursor_enabled;
    tty->saved_state.row = tty->row;
    tty->saved_state.col = tty->col;
}


void restore_tty_cursor_state(struct tty_t *tty)
{
    tty->cursor_shown = tty->saved_state.cursor_shown;
    tty->cursor_enabled = tty->saved_state.cursor_enabled;
    tty->row = tty->saved_state.row;
    tty->col = tty->saved_state.col;
}


/*
 * Save current terminal state. This should include cursor coordinates,
 * attributes, and character sets pointed at by G0 and G1.
 *
 * FIXME: this is not fully implemented yet!
 */
void save_tty_state(struct tty_t *tty)
{
    save_tty_cursor_state(tty);

    tty->saved_state.vga_width = tty->vga_width;
    tty->saved_state.vga_height = tty->vga_height;
    tty->saved_state.attribs = tty->attribs;

    // EGA tty    
    tty->saved_state.color = tty->color;

    // VGA tty (the framebuffer device)
    tty->saved_state.fb_fgcolor = tty->fb_fgcolor;
    tty->saved_state.fb_bgcolor = tty->fb_bgcolor;
    
    //save_screen(tty);
}


/*
 * Restore terminal state that was most recently saved. This should include
 * cursor coordinates, attributes, and character sets pointed at by G0 and G1.
 *
 * FIXME: this is not fully implemented yet!
 */
void restore_tty_state(struct tty_t *tty)
{
    restore_tty_cursor_state(tty);

    tty->vga_width = tty->saved_state.vga_width;
    tty->vga_height = tty->saved_state.vga_height;
    tty->attribs = tty->saved_state.attribs;

    // EGA tty
    tty->color = tty->saved_state.color;

    // VGA tty (the framebuffer device)
    tty->fb_fgcolor = tty->saved_state.fb_fgcolor;
    tty->fb_bgcolor = tty->saved_state.fb_bgcolor;
    
    restore_screen(tty);
}


int switch_tty(int which)
{
    if(which < 1 || which >= total_ttys)
    {
        return -EINVAL;
    }
    
    if(cur_tty == which)
    {
        return 0;
    }
    
    printk("Switching to tty %d\n", which);

    hide_cur(&ttytab[cur_tty]);
    ttytab[cur_tty].flags &= ~TTY_FLAG_ACTIVE;
    //tty_send_signal(ttytab[cur_tty].pgid, SIGSTOP);
    cur_tty = which;
    ttytab[cur_tty].flags |= TTY_FLAG_ACTIVE;
    repaint_screen = 1;
    restore_screen(&ttytab[cur_tty]);
    //tty_send_signal(ttytab[cur_tty].pgid, SIGCONT);

    return 0;
}

