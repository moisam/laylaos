/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: tty_ioctl.c
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
 *  \file tty_ioctl.c
 *
 *  The terminal (tty) device driver implementation.
 *  The driver's code is split between these files:
 *    - tty.c => device initialisation, general interface, and read/write 
 *               functions
 *    - tty_input.c => handling terminal input
 *    - tty_ioctl.c => terminal device control (ioctl)
 *    - tty_state.c => saving and restoring device state
 */

#define __USE_XOPEN_EXTENDED

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#include <errno.h>
#include <termios.h>
#include <kernel/laylaos.h>
#include <kernel/tty.h>
#include <kernel/asm.h>
#include <kernel/kqueue.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/timer.h>
#include <fs/devpts.h>
#include <gui/fb.h>

#include "tty_inlines.h"


/*
 * Flush the given terminal read/write queue.
 *
 * Input:
 *    q => the queue to flush
 */
void flush_queue(struct kqueue_t *q)
{
    uintptr_t i = int_off();
    ttybuf_clear(q);
    int_on(i);
}


/*
 * Block task until the given terminal output queue is empty.
 *
 * Input:
 *    tty => the terminal whose queue to flush
 *
 * Returns:
 *    none
 */
static void wait_until_sent(struct tty_t *tty)
{
    volatile int count = ttybuf_used(&tty->write_q);

    while(count)
    {
        block_task2(&tty->write_q, 20);
        count = ttybuf_used(&tty->write_q);
    }
}


/*
 * Send a break (stream of zeroes) to the given terminal.
 *
 * Input:
 *    tty => the terminal to which to send the break
 *    decisecs => length of break (in deci-seconds)
 *
 * Returns:
 *    none
 */
static void send_break(struct tty_t *tty, int decisecs)
{
    unsigned long long i = (decisecs * PIT_FREQUENCY) / 10;
    unsigned long long newticks = ticks + i;
    
    while(ticks < newticks)
    {
        ttybuf_enqueue(&tty->read_q, 0);
    }
}


/*
 * Get and set the given terminal's termios structure.
 *
 * Input:
 *    tty => the terminal whose struct termios is read/set
 *    termios => pointer to the struct to read from or write to
 *
 * Returns:
 *    0
 */
static int get_termios(struct tty_t *tty, struct termios *termios, int kernel)
{
    if(!tty || !termios)
    {
        return -EINVAL;
    }
    
    if(kernel)
    {
        A_memcpy(termios, &tty->termios, sizeof(struct termios));
    }
    else
    {
        copy_to_user(termios, &tty->termios, sizeof(struct termios));
    }

    return 0;
}


static int set_termios(struct tty_t *tty, struct termios *termios, int kernel)
{
    if(!tty || !termios)
    {
        return -EINVAL;
    }
    
    if(kernel)
    {
        A_memcpy(&tty->termios, termios, sizeof(struct termios));
    }
    else
    {
        copy_from_user(&tty->termios, termios, sizeof(struct termios));
    }

    return 0;
}


/*
 * Get and set the given terminal's termios structure. Similar to the above
 * two functions, except the following works on struct termio instead of
 * struct termios.
 */
static int get_termio(struct tty_t *tty, struct termio *termio, int kernel)
{
    int i;
    struct termio tmp;

    if(!tty || !termio)
    {
        return -EINVAL;
    }
    
    tmp.c_iflag = tty->termios.c_iflag;
    tmp.c_oflag = tty->termios.c_oflag;
    tmp.c_cflag = tty->termios.c_cflag;
    tmp.c_lflag = tty->termios.c_lflag;
    tmp.c_line = tty->termios.c_line;

    for(i = 0; i < NCC; i++)
    {
        tmp.c_cc[i] = tty->termios.c_cc[i];
    }
    
    if(kernel)
    {
        A_memcpy(termio, &tmp, sizeof(struct termio));
    }
    else
    {
        copy_to_user(termio, &tmp, sizeof(struct termio));
    }

    return 0;
}


static int set_termio(struct tty_t *tty, struct termio *termio, int kernel)
{
    int i;
    struct termio tmp;

    if(!tty || !termio)
    {
        return -EINVAL;
    }
    
    if(kernel)
    {
        A_memcpy(&tmp, termio, sizeof(struct termio));
    }
    else
    {
        copy_from_user(&tmp, termio, sizeof(struct termio));
    }

    tty->termios.c_iflag = tmp.c_iflag;
    tty->termios.c_oflag = tmp.c_oflag;
    tty->termios.c_cflag = tmp.c_cflag;
    tty->termios.c_lflag = tmp.c_lflag;
    tty->termios.c_line = tmp.c_line;

    for(i = 0; i < NCC; i++)
    {
        tty->termios.c_cc[i] = tmp.c_cc[i];
    }

    return 0;
}


/*
 * Get and set the given terminal's window size.
 *
 * Input:
 *    tty => the terminal whose window size is read/set
 *    window => pointer to the struct to read from or write to
 *
 * Returns:
 *    0
 */
static int get_winsize(struct tty_t *tty, struct winsize *window, int kernel)
{
    if(!tty || !window)
    {
        return -EINVAL;
    }
    
    if(kernel)
    {
        A_memcpy(window, &tty->window, sizeof(struct winsize));
        return 0;
    }
    else
    {
        return copy_to_user(window, &tty->window, sizeof(struct winsize));
    }
}


static int set_winsize(struct tty_t *tty, struct winsize *window, int kernel)
{
    if(!tty || !window)
    {
        return -EINVAL;
    }
    
    if(kernel)
    {
        A_memcpy(&tty->window, window, sizeof(struct winsize));
    }
    else
    {
        COPY_FROM_USER(&tty->window, window, sizeof(struct winsize));
    }

    tty_send_signal(tty->pgid, SIGWINCH);
    return 0;
}


/*
 * Set the controlling terminal of the calling process.
 *
 * Input:
 *    dev => the terminal's device id
 *    tty => pointer to the terminal's struct
 *    arg => controls what is done to the terminal (details in link below)
 *
 * Returns:
 *    0
 *
 * For details of arg, see: https://linux.die.net/man/4/tty_ioctl
 */
int set_controlling_tty(dev_t dev, struct tty_t *tty, int arg)
{
    struct task_t *ct = cur_task;
    
    if(!tty)
    {
        return -ENOTTY;
    }

    if(!arg)
    {
        // give up the terminal if its the calling task's controlling tty
        if(ct->ctty == dev)
        {
            setid(ct, ctty, 0);
        
            // if task is session leader, session members lose their 
            // controlling tty
            if(session_leader(ct))
            {
                tty_send_signal(tty->pgid, SIGHUP);
                tty_send_signal(tty->pgid, SIGCONT);

                elevated_priority_lock(&task_table_lock);

                for_each_taskptr(t)
                {
                    if(*t && (*t)->sid == ct->sid)
                    {
                        (*t)->ctty = 0;
                    }
                }

                elevated_priority_unlock(&task_table_lock);
            
                if(ct->pgid == tty->pgid)
                {
                    tty->pgid = 0;
                }
            
                if(ct->sid == tty->sid)
                {
                    tty->sid = 0;
                }
            }
            
            return 0;
        }
    
        return -EPERM;
    }
    else
    {
        // make this terminal the controlling tty of the calling task,
        // unless it is the controlling tty of another session group, in which
        // case the call fails, unless the caller is root and arg == 1, in
        // which case we steal the terminal and the other group is screwed

        if(!group_leader(ct) || ct->ctty > 0)
        {
            return -EPERM;
        }
        
        if(tty->sid && tty->sid != ct->sid)
        {
            // not root
            if(!suser(ct))
            {
                return -EPERM;
            }
            
            if(arg != 1)
            {
                return -EPERM;
            }

			// open fails if the exclusive mode is on and the caller is
			// not root
			if((tty->flags & TTY_FLAG_EXCLUSIVE) &&
			   !suser(ct))
			{
			    return -EBUSY;
			}
            
            elevated_priority_lock(&task_table_lock);

            for_each_taskptr(t)
            {
                if(*t && (*t)->ctty == dev)
                {
                    (*t)->ctty = 0;
                }
            }

            elevated_priority_unlock(&task_table_lock);
        }
        
        tty->sid = ct->sid;
        tty->pgid = ct->pgid;
        //ct->ctty = dev;
        setid(ct, ctty, dev);
        return 0;
    }
}


/*
 * Terminal ioctl function.
 *
 * Input:
 *    dev => device id of the terminal
 *    cmd => command to perform on the terminal (details in the link below)
 *    arg => optional argument, depends on what cmd is
 *    kernel => non-zero if the caller is a kernel function, zero if it is a
 *              syscall from userland
 *
 * Returns:
 *    0 on success, -errno on failure
 *
 * For details of cmd and arg, see: https://linux.die.net/man/4/tty_ioctl
 */
int tty_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel)
{
    struct tty_t *tty;
    int tmp;

    if(!(tty = get_struct_tty(dev)))
    {
        return -EINVAL;
    }

    switch(cmd)
    {
        case TCGETS:          // get tty settings
            return get_termios(tty, (struct termios *)arg, kernel);
            
        case TCSETSF:         // drain output buf, discard input, and
                              // set tty settings
            flush_queue(&tty->read_q);
            __attribute__((fallthrough));
            
        case TCSETSW:         // drain output buf and set tty settings
            wait_until_sent(tty);
            __attribute__((fallthrough));
            
        case TCSETS:          // set tty settings
            return set_termios(tty, (struct termios *)arg, kernel);

        case TCGETA:          // similar to TCGETS but works on struct termio
            return get_termio(tty, (struct termio *)arg, kernel);

        case TCSETAF:         // similar to TCSETSF but works on struct termio
            flush_queue(&tty->read_q);
            __attribute__((fallthrough));
            
        case TCSETAW:         // similar to TCSETSW but works on struct termio
            wait_until_sent(tty);
            __attribute__((fallthrough));
            
        case TCSETA:          // similar to TCSETS but works on struct termio
            return set_termio(tty, (struct termio *)arg, kernel);
        
        case TIOCGLCKTRMIOS:  // get locking status of tty's struct termios
                              // NOTE: not implemented yet
            return -EINVAL;
        
        case TIOCSLCKTRMIOS:  // set locking status of tty's struct termios
                              // NOTE: not implemented yet
            return -EINVAL;

        case TIOCGWINSZ:      // get window size
            return get_winsize(tty, (struct winsize *)arg, kernel);

        case TIOCSWINSZ:      // set window size
            return set_winsize(tty, (struct winsize *)arg, kernel);
            
        case TCSBRK:      // send a break (Linux drains output buf if arg == 0)
                          // break should be between 0.25-0.5 seconds
            if(arg)
            {
                wait_until_sent(tty);
            }
            else
            {
                send_break(tty, 5);     // 5 deci-seconds
            }
            return 0;
            
        case TCSBRKP:         // POSIX version of TCSBRK (arg is break length
                              // in deci-seconds)
            if(arg)
            {
                send_break(tty, (int)(uintptr_t)arg);
            }
            return 0;

        case TIOCSBRK:        // turn break on (start sending zeroes)
                              // NOTE: not implemented yet
            return -EINVAL;

        case TIOCCBRK:        // turn break off (stop sending zeroes)
                              // NOTE: not implemented yet
            return -EINVAL;

        case TCXONC:          // software flow control
            switch((int)(uintptr_t)arg)
            {
                case TCOOFF:    // suspend output
                    break;      // NOTE: not implemented

                case TCOON:     // restart suspended output
                    break;      // NOTE: not implemented

                case TCIOFF:    // transmit a STOP char
                    ttybuf_enqueue(&tty->read_q, tty->termios.c_cc[VSTOP]);
                    return 0;
                    
                case TCION:     // transmit a START char
                    ttybuf_enqueue(&tty->read_q, tty->termios.c_cc[VSTART]);
                    return 0;
            }

            return -EINVAL;

        case TIOCINQ:           // get number of bytes in input buffer
        //case FIONREAD:
            tmp = ttybuf_used(&tty->read_q);
            
            if(kernel)
            {
                *(int *)arg = tmp;
            }
            else
            {
                COPY_VAL_TO_USER((int *)arg, &tmp);
            }

            return 0;
        
        case TIOCOUTQ:          // get number of bytes in output buffer
            tmp = ttybuf_used(&tty->write_q);

            if(kernel)
            {
                *(int *)arg = tmp;
            }
            else
            {
                COPY_VAL_TO_USER((int *)arg, &tmp);
            }

            return 0;

        case TCFLSH:            // flush tty buffers
            tmp = (int)(uintptr_t)arg;

            if(tmp == TCIFLUSH)
            {
                flush_queue(&tty->read_q);
            }
            else if(tmp == TCOFLUSH)
            {
                flush_queue(&tty->write_q);
            }
            else if(tmp == TCIOFLUSH)
            {
                flush_queue(&tty->read_q);
                flush_queue(&tty->write_q);
            }
            else
            {
                return -EINVAL;
            }

            return 0;

        case TIOCSTI:           // insert given byte in the input queue

            if(kernel)
            {
                tmp = *(int *)arg;
            }
            else
            {
                COPY_VAL_FROM_USER(&tmp, arg);
            }

            ttybuf_enqueue(&tty->read_q, tmp);
            return 0;
        
        case TIOCCONS:          // redirect output
                                // NOTE: not implemented yet
            return -EINVAL;

        case TIOCSCTTY:         // set controlling terminal
            return set_controlling_tty(dev, tty, (int)(uintptr_t)arg);

        case TIOCGPGRP:         // get tty's foregound process group id
            if(kernel)
            {
                *(pid_t *)arg = tty->pgid;
            }
            else
            {
                COPY_VAL_TO_USER((pid_t *)arg, &tty->pgid);
            }

            return 0;

        case TIOCSPGRP:         // set tty's foregound process group id
            if(kernel)
            {
                tty->pgid = *(pid_t *)arg;
            }
            else
            {
                COPY_VAL_FROM_USER(&tty->pgid, (pid_t *)arg);
            }

            return 0;
            
        case TIOCGSID:          // get tty's session id
            if(kernel)
            {
                *(pid_t *)arg = tty->sid;
            }
            else
            {
                COPY_VAL_TO_USER((pid_t *)arg, &tty->sid);
            }

            return 0;
            
        case TIOCEXCL:          // enable exclusive mode
            tty->flags |= TTY_FLAG_EXCLUSIVE;
            return 0;

        case TIOCNXCL:          // disable exclusive mode
            tty->flags &= ~TTY_FLAG_EXCLUSIVE;
            return 0;
        
        case TIOCGETD:          // get the line discipline
            if(kernel)
            {
                *(int *)arg = tty->termios.c_line;
            }
            else
            {
                COPY_VAL_TO_USER((int *)arg, &tty->termios.c_line);
            }

            return 0;
        
        case TIOCSETD:          // set the line discipline
            if(kernel)
            {
                tty->termios.c_line = *(int *)arg;
            }
            else
            {
                COPY_VAL_FROM_USER(&tty->termios.c_line, (int *)arg);
            }

            return 0;
        
        case TIOCPKT:           // enable/disable packet mode (pseudo-ttys)
                                // NOTE: not implemented yet
            return -EINVAL;

        case TIOCMGET:          // get the status of modem bits
                                // NOTE: not implemented yet
            return -EINVAL;

        case TIOCMSET:          // set the status of modem bits
                                // NOTE: not implemented yet
            return -EINVAL;

        case TIOCMBIC:          // clear the indicated modem bits
                                // NOTE: not implemented yet
            return -EINVAL;

        case TIOCMBIS:          // set the indicated modem bits
                                // NOTE: not implemented yet
            return -EINVAL;

        case TIOCGSOFTCAR:      // get the CLOCAL flag status
            tmp = (tty->termios.c_cflag & CLOCAL) ? 1 : 0;

            if(kernel)
            {
                *(int *)arg = tmp;
            }
            else
            {
                COPY_VAL_TO_USER((int *)arg, &tmp);
            }

            return 0;

        case TIOCSSOFTCAR:      // set the CLOCAL flag status
            if(kernel)
            {
                tmp = *(int *)arg;
            }
            else
            {
                COPY_VAL_FROM_USER(&tmp, (int *)arg);
            }
            
            if(tmp)
            {
                tty->termios.c_cflag |= CLOCAL;
            }
            else
            {
                tty->termios.c_cflag &= ~CLOCAL;
            }

            return 0;

        /*
         * Pseudoterminal ioctls
         *
         * See: https://man7.org/linux/man-pages/man2/ioctl_tty.2.html
         */
        
        case TIOCSPTLCK:        // set/remove pty slave device lock

            // this operation only works on a master pty device
            if(MAJOR(dev) != PTY_MASTER_MAJ)
            {
                return -EINVAL;
            }
            
            if(kernel)
            {
                tmp = *(int *)arg;
            }
            else
            {
                COPY_FROM_USER(&tmp, (int *)arg, sizeof(int));
                //tmp = (int)(uintptr_t)arg;
            }

            if(tmp)
            {
                tty->flags |= TTY_FLAG_LOCKED;
            }
            else
            {
                tty->flags &= ~TTY_FLAG_LOCKED;
            }

            return 0;

        case TIOCGPTN:      // get pty slave device number
            if((tmp = pty_slave_index(dev)) < 0)
            {
                return tmp;
            }

            if(kernel)
            {
                *(int *)arg = tmp;
            }
            else
            {
                COPY_VAL_TO_USER((int *)arg, &tmp);
            }

            return 0;

        /*
         * Our own extensions.
         */
        case VT_SWITCH_TTY:             // switch active TTY
            tmp = (int)(uintptr_t)arg;

            // if 0 is passed as arg, use the tty device referenced by
            // the given file descriptor
            if(!tmp)
            {
                for(tmp = 0; tmp < total_ttys; tmp++)
                {
                    if(tty == &ttytab[tmp])
                    {
                        break;
                    }
                }
                
                if(tmp == total_ttys)
                {
                    return -EINVAL;
                }
            }

            return switch_tty(tmp);

        // No input processing (used by the gui server). This needs to be set
        // in addition to the "raw mode" as some processing is done even in
        // raw mode (and we check the different flags, which makes the process
        // a bit slow).
        case VT_RAW_INPUT:
            tmp = (int)(uintptr_t)arg;
            
            if(tmp)
            {
                tty->process_key = raw_process_key;
                tty->copy_to_buf = raw_copy_to_buf;
            }
            else
            {
                tty->process_key = process_key;
                tty->copy_to_buf = copy_to_buf;
            }
            
            return 0;

        case VT_GRAPHICS_MODE:
            tmp = (int)(uintptr_t)arg;
            
            if(tmp)
            {
                tty->flags |= TTY_FLAG_NO_TEXT;
                tty->write = NULL;
                fb_cur_backbuf = fb_backbuf_gui;
            }
            else
            {
                tty->flags &= ~TTY_FLAG_NO_TEXT;
                tty->write = console_write;
                fb_cur_backbuf = fb_backbuf_text;
            }
            
            return 0;

        default:
            return -EINVAL;
    }
}

