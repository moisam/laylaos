/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: tty.c
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
 *  \file tty.c
 *
 *  The terminal (tty) device driver implementation.
 *  The driver's code is split between these files:
 *    - tty.c => device initialisation, general interface, and read/write 
 *               functions
 *    - tty_input.c => handling terminal input
 *    - tty_ioctl.c => terminal device control (ioctl)
 *    - tty_state.c => saving and restoring device state
 */

//#define __DEBUG

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#define __WANT_TTY_DEFCHARS_ARRAY__
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <kernel/laylaos.h>
#include <kernel/tty.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/timer.h>
#include <kernel/ksignal.h>
#include <kernel/vga.h>
#include <kernel/syscall.h>
#include <kernel/fcntl.h>
#include <mm/kheap.h>
#include <fs/devpts.h>
#include <gui/vbe.h>
#include <gui/fb.h>

#include "tty_inlines.h"

#define toupper(c)	((c) - 0x20 * (((c) >= 'a') && ((c) <= 'z')))
#define tolower(c)	((c) + 0x20 * (((c) >= 'A') && ((c) <= 'Z')))


/*
 * Master table that holds information about all terminal devices.
 * The first entry is /dev/tty0 (id 4, 0), a dummy entry that refers to the
 * current terminal device.
 * The second and above entries refer to terminal devices with minor id 1
 * through 63 (currently we only have 1).
 */
struct tty_t ttytab[NTTYS] = { 0, };

// tty buffers
char tty_readbuf[NTTYS - 1][TTY_BUF_SIZE];
char tty_writebuf[NTTYS - 1][TTY_BUF_SIZE];
char tty_secondarybuf[NTTYS - 1][TTY_BUF_SIZE];

// table index of the current (active) terminal device
int cur_tty = 1;

// total number of ttys (including dummy tty0)
int total_ttys = NTTYS;


/*
 * Dummy tty write.
 */
void dummy_write(struct tty_t *tty)
{
    UNUSED(tty);
}


void tty_init_queues(int i)
{
    ttybuf_init(&ttytab[i].read_q, &tty_readbuf[i - 1]);
    ttybuf_init(&ttytab[i].write_q, &tty_writebuf[i - 1]);
    ttybuf_init(&ttytab[i].secondary, &tty_secondarybuf[i - 1]);
}


/*
 * Initialize terminal device queues and init console.
 */
void tty_init(void)
{
    int i;

    for(i = 2; i < NTTYS; i++)
    {
        // init queues
        tty_init_queues(i);

        ttytab[i].vga_width = ttytab[1].vga_width;
        ttytab[i].vga_height = ttytab[1].vga_height;

    	ttytab[i].cursor_enabled = 1;
        ttytab[i].cursor_shown = 0;

        ttytab[i].write = console_write;
        ttytab[i].process_key = process_key;
        ttytab[i].copy_to_buf = copy_to_buf;

        if(!(ttytab[i].cellattribs =
                kmalloc(ttytab[i].vga_width * ttytab[i].vga_height)))
        {
            kpanic("tty: failed to alloc internal buffer\n");
        }

        A_memset(ttytab[i].cellattribs, 0, ttytab[i].vga_width * ttytab[i].vga_height);

        fb_reset_charsets(&ttytab[i]);
        fb_reset_colors(&ttytab[i]);
        tty_set_defaults(&ttytab[i]);
    }
}


/*
 * Set tty defaults.
 */
void tty_set_defaults(struct tty_t *tty)
{
    // init struct termios control chars
    memcpy(tty->termios.c_cc, ttydefchars, sizeof(ttydefchars));
    
    // init window size
    tty->window.ws_row = tty->vga_height;
    tty->window.ws_col = tty->vga_width;
    tty->window.ws_xpixel = 0;
    tty->window.ws_ypixel = 0;
    
    tty->scroll_top = 1;
    tty->scroll_bottom = tty->vga_height;

    tty->row = 0;
    tty->col = 0;
    tty->default_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    tty->color = tty->default_color;
    tty->fb_fgcolor = fb_default_fgcolor;
    tty->fb_bgcolor = fb_default_bgcolor;

    tty->attribs = 0;
    tty->flags &= ~(TTY_FLAG_EXCLUSIVE | TTY_FLAG_REVERSE_VIDEO |
                    TTY_FLAG_CURSOR_RELATIVE | TTY_FLAG_LFNL);
    tty->flags |= TTY_FLAG_AUTOWRAP;

    tty->waiting_task = NULL;

    /* input modes */
    tty->termios.c_iflag = TTYDEF_IFLAG;

    /* output modes: change outgoing NL to CRNL */
    tty->termios.c_oflag = TTYDEF_OFLAG;

    /* control modes */
    tty->termios.c_cflag = TTYDEF_CFLAG;

    /* local modes */
    tty->termios.c_lflag = TTYDEF_LFLAG;

    /* input speed */
    tty->termios.c_ispeed = TTYDEF_SPEED;

    /* output speed */
    tty->termios.c_ospeed = TTYDEF_SPEED;

    save_tty_state(tty);
}


/*
 * Perform a select operation on a tty device.
 */
int tty_select(struct file_t *f, int which)
{
    struct tty_t *tty;
    int canon;
    dev_t dev;

    if(!f || !f->node)
    {
        return 0;
    }
    
	if(!S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];
	
	if(!(tty = get_struct_tty(dev)))
	{
	    return 0;
	}

    canon = (tty->termios.c_lflag & ICANON);
    
	switch(which)
	{
    	case FREAD:
            if(ttybuf_is_empty(&tty->secondary) ||
               (canon && !tty->secondary.extra))
            {
        		selrecord(&tty->ssel);
                return 0;
            }
            return 1;

    	case FWRITE:
    	    if(ttybuf_is_full(&tty->write_q))
    	    {
        		selrecord(&tty->wsel);
                return 0;
            }
            return 1;
    	    
    	case 0:
    	    // TODO: (we should be handling exceptions)
   			break;
	}

	return 0;
}


/*
 * Perform a poll operation on a tty device.
 */
int tty_poll(struct file_t *f, struct pollfd *pfd)
{
    int res = 0;
    struct tty_t *tty;
    int canon;
    dev_t dev;

    if(!f || !f->node || !S_ISCHR(f->node->mode))
	{
        pfd->revents |= POLLNVAL;
	    return 0;
	}
	
	dev = f->node->blocks[0];

	if(!(tty = get_struct_tty(dev)))
	{
        pfd->revents |= POLLERR;
	    return 0;
	}

    canon = (tty->termios.c_lflag & ICANON);

    if(pfd->events & POLLIN)
    {
        if(ttybuf_is_empty(&tty->secondary) ||
           (canon && !tty->secondary.extra))
        {
            selrecord(&tty->ssel);
        }
        else
        {
            pfd->revents |= POLLIN;
            res = 1;
        }
    }

    if(pfd->events & POLLOUT)
    {
        if(ttybuf_is_full(&tty->write_q))
        {
            selrecord(&tty->wsel);
        }
        else
        {
            pfd->revents |= POLLOUT;
            res = 1;
        }
    }
    
    return res;
}


/*
 * Send a signal to a terminal device's foreground/background process group.
 *
 * Inputs:
 *    pgid => the process group id
 *    signal => number of signal to send
 *
 * Returns:
 *    none
 */
void tty_send_signal(pid_t pgid, int signal)
{
    if(pgid <= 0)
    {
        return;
    }

    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(*t && (*t)->pgid == pgid)
        {
            add_task_signal(*t, signal, NULL, 1);
        }
    }

    elevated_priority_unlock(&task_table_lock);
}


/*
 * Handler for syscall vhangup().
 *
 * Perform a "virtual" terminal hangup.
 *
 * Returns:
 *    0 on success, -errno on failure
 *
 * For details see: 
 *      https://docs.oracle.com/cd/E88353_01/html/E37841/vhangup-2.html
 */
int syscall_vhangup(void)
{
    struct task_t *ct = cur_task;
    
    if(!suser(ct))
    {
        return -EPERM;
    }
    
    return set_controlling_tty(ct->ctty, get_struct_tty(ct->ctty), 0);
}


/*
 * Read data from a terminal device.
 *
 * Inputs:
 *    dev => device number of the terminal device to read from.
 *           minor 0 is the current (tty0) device, minors 1 through 63
 *           are the possible terminal devices (tty1 - tty63)
 *    buf => input buffer where data is copied
 *    count => number of characters to read
 *
 * Output:
 *    buf => data is copied to this buffer
 *
 * Returns:
 *    number of characters read on success, -errno on failure
 */
ssize_t ttyx_read(struct file_t *f, off_t *pos,
                  unsigned char *buf, size_t _count, int kernel)
{
    UNUSED(pos);

    dev_t dev = f->node->blocks[0];
    struct tty_t *tty;
    unsigned char c, *p = buf;
    size_t min;
    int time;
    volatile size_t count = _count;
    volatile int has_signal = 0;
    struct task_t *ct = cur_task;
    
    /*
    if(!buf)
    {
        return -EINVAL;
    }
    */
    
    if(!(tty = get_struct_tty(dev)))
    {
        return -EINVAL;
    }

    // check the given user address is valid
    if(!kernel)
    {
        if(valid_addr(ct, (virtual_addr)p, (virtual_addr)p + count - 1) != 0)
        {
            add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)p);
            return -EFAULT;
        }
    }
    
    /*
     * The setpgid (2) manpage says:
     *    Only the foreground process group may read(2) from the terminal;
     *    if a background process group tries to read(2) from the terminal,
     *    then the group is sent a SIGTTIN signal, which suspends it.
     */
    if(tty->pgid != ct->pgid)
    {
        tty_send_signal(ct->pgid, SIGTTIN);
        return -EINVAL;
    }
    
    // time out is in 1/10th of a second
    time = tty->termios.c_cc[VTIME] * 10 * PIT_FREQUENCY;
    
    // minimum chars to read
    min = tty->termios.c_cc[VMIN];
    
    // lower the minimum count if the caller requested less chars than min
    if(min > count)
    {
        min = count;
    }
    
    ct->woke_by_signal = 0;
    
    // read input
    while(count > 0)
    {
        has_signal = ct->woke_by_signal;
        
        // stop if we receive a signal
        if(has_signal)
        {
            break;
        }
        
        int canon = (tty->termios.c_lflag & ICANON);
        
        // sleep if the secondary queue is empty, or the tty is in canonical
        // mode and there are no complete line(s) of input yet
        if(ttybuf_is_empty(&tty->secondary) ||
           (canon && !tty->secondary.extra))
        {
            if(sleep_if_empty(tty, &tty->secondary, time) != 0)
            {
                // timeout has expired
                break;
            }
        }
        
        // get next char
        do
        {
            c = ttybuf_dequeue(&tty->secondary);
            
            if(c == LF || c == (unsigned char)tty->termios.c_cc[VEOF])
            {
                // decrement input line counter
                tty->secondary.extra--;
            }
            
            // return if we reach EOF and tty is in canonical mode
            if((c == (unsigned char)tty->termios.c_cc[VEOF]) && canon)
            {
                return (ssize_t)(p - buf);
            }
            
            // copy next char to user buf
            *p++ = c;
            
            // stop if we got our count
            if(--count == 0)
            {
                break;
            }

            // stop if we got LF in canonical mode
            if(c == LF && canon)
            {
                break;
            }

            // loop until we get our count chars or queue is empty
        } while(count > 0 && !ttybuf_is_empty(&tty->secondary));
        
        if(canon)
        {
            if((p - buf) != 0)
            {
                // stop if canonical mode and we've got any chars
                break;
            }
        }
        else if((size_t)(p - buf) >= min)
        {
            // stop if we've got our min chars
            break;
        }
    }
    
    // check if we were interrupted by a signal and no chars read
    if(ct->woke_by_signal && (p - buf) == 0)
    {
        return -ERESTARTSYS;
    }
    
    // return count of bytes read
    return (ssize_t)(p - buf);
}


/*
 * Write data to a terminal device.
 *
 * Inputs:
 *    dev => device number of the terminal device to write to.
 *           minor 0 is the current (tty0) device, minors 1 through 63
 *           are the possible terminal devices (tty1 - tty63)
 *    buf => output buffer from which data is copied
 *    count => number of characters to write
 *
 * Output:
 *    none
 *
 * Returns:
 *    number of characters written on success, -errno on failure
 */
ssize_t ttyx_write(struct file_t *f, off_t *pos,
                   unsigned char *buf, size_t _count, int kernel)
{
    UNUSED(pos);

    dev_t dev = f->node->blocks[0];
    struct tty_t *tty;
    unsigned char c, *p = buf;
    volatile size_t count = _count;
    volatile int has_signal = 0;
    struct task_t *ct = cur_task;
    
    /*
    if(!buf)
    {
        return -EINVAL;
    }
    */

    if(!(tty = get_struct_tty(dev)))
    {
        return -EINVAL;
    }

    // check the given user address is valid
    if(!kernel)
    {
        if(valid_addr(ct, (virtual_addr)p, (virtual_addr)p + count - 1) != 0)
        {
            add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)p);
            return -EFAULT;
        }
    }
    
    if(!tty->write_q.buf)
    {
        return 0;
    }

    ct->woke_by_signal = 0;

    // write output
    while(count > 0)
    {
        // wait until output buffer has space
        sleep_if_full(&tty->write_q);

        has_signal = ct->woke_by_signal;
        
        // stop if we receive a signal
        if(has_signal)
        {
            break;
        }
        
        // copy output as long as there is space in the output buffer
        while(count > 0 && !ttybuf_is_full(&tty->write_q))
        {
            c = *p;
            
            // do post (output) processing
            if(tty->termios.c_oflag & OPOST)
            {
                if(c == CR)
                {
                    if(tty->termios.c_oflag & OCRNL)
                    {
                        // convert CR to LF
                        c = LF;
                    }
                    else if(tty->termios.c_oflag & ONLRET)
                    {
                        // don't output CR
                        continue;
                    }
                    else if(tty->termios.c_oflag & ONOCR)
                    {
                        // don't output CR at column 0
                        // TODO:
                        ;
                    }
                }
                else if(c == LF && (tty->termios.c_oflag & ONLCR))
                {
                    // convert NL to CR-NL
                    ttybuf_enqueue(&tty->write_q, CR);
                    
                    if(ttybuf_is_full(&tty->write_q))
                    {
                        // we will sleep until there is space in the queue
                        break;
                    }
                }
                
                if(tty->termios.c_oflag & OLCUC)
                {
                    // map lowercase to uppercase chars (non-POSIX)
                    c = toupper(c);
                }
            }
            
            p++;
            count--;
            ttybuf_enqueue(&tty->write_q, c);
        }
        
        // write output to terminal
        if(tty->write)
        {
            tty->write(tty);
        }

        // wake up waiting tasks
        unblock_tasks(&tty->write_q);
    
        // wake up select() waiting tasks
        selwakeup(&tty->wsel);
    }
    
    // check if we were interrupted by a signal and no chars written
    if(ct->woke_by_signal && (p - buf) == 0)
    {
        return -ERESTARTSYS;
    }
    
    // return count of bytes written
    return (ssize_t)(p - buf);
}

