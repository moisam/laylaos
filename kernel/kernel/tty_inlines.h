/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: tty_inlines.h
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
 *  \file tty_inlines.h
 *
 *  Inlined functions for the kernel terminal driver use.
 */

#include <fs/devpts.h>
#include <kernel/clock.h>

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

extern struct pty_t *pty_slaves[MAX_PTY_DEVICES];

/*
 * adjust row and column (if needed) after outputting a char.
 */
INLINE void tty_adjust_indices(struct tty_t *tty)
{
    if(tty->col >= tty->window.ws_col)
    {
        if(tty->flags & TTY_FLAG_AUTOWRAP)
        {
            tty->col = 0;
            tty->row++;
        }
        else
        {
            tty->col = tty->window.ws_col - 1;
        }
    }
  
    if(tty->row >= tty->scroll_bottom)
    {
        // scroll up
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        scroll_up(tty, tty->window.ws_col, tty->scroll_bottom, tty->scroll_top - 1);
        tty->row = tty->scroll_bottom - 1;
    }
}


/*
 * Sleep if a terminal's read queue is empty.
 *
 * Input:
 *    q => terminal device queue to sleep on
 */
INLINE int sleep_if_empty(struct tty_t *tty, struct kqueue_t *q, int timeout_ticks)
{
    struct task_t *ct = cur_task;
    volatile int sig = ct->woke_by_signal;
    volatile int empty = ttybuf_is_empty(q);
    
    // sleep until we get a signal then check if buffer is still empty
    while(!sig && empty)
    {
        tty->waiting_task = ct;

        if(timeout_ticks)
        {
            if(clock_wait(&waiter_head[0], ct->pid, timeout_ticks, 0) == 0)
            {
                tty->waiting_task = NULL;
                return -ETIMEDOUT;
            }
        }
        else
        {
            //block_task(q, 1);
            block_task2(q, 20);
        }

        tty->waiting_task = NULL;

        sig = ct->woke_by_signal;
        empty = ttybuf_is_empty(q);
    }
    
    return 0;
}


/*
 * Sleep if a terminal's write queue is full.
 *
 * Input:
 *    q => terminal device queue to sleep on
 */
INLINE void sleep_if_full(struct kqueue_t *q)
{
    // don't sleep if queue is not full
    if(!ttybuf_is_full(q))
    {
        return;
    }

    struct task_t *ct = cur_task;
    volatile int sig = ct->woke_by_signal;
    volatile int space = ttybuf_has_space_for(q, 128);
    
    // make sure we have space for at least 128 more chars (arbitrary number)
    while(!sig && !space)
    {
        //block_task(q, 1);
        block_task2(q, 20);

        sig = ct->woke_by_signal;
        space = ttybuf_has_space_for(q, 128);
    }
}


/*
 * Get a terminal device's tty struct.
 */
INLINE struct tty_t *get_struct_tty(dev_t dev)
{
    int minor = MINOR(dev);
    int major = MAJOR(dev);

    // ttyx device (major 5, minor 0)
    if(major == 5 && minor == 0)
    {
        minor = MINOR(cur_task->ctty);
        major = MAJOR(cur_task->ctty);
    }

    // tty devices (major 4)
    if(major == 4 && minor < total_ttys)
    {
        return &ttytab[(minor == 0) ? cur_tty : minor];
    }

    // pseudoterminal master/slave devices (major 2 and 136)
    if(major == PTY_MASTER_MAJ || major == PTY_SLAVE_MAJ)
    {
        if(minor >= MAX_PTY_DEVICES || !pty_slaves[minor])
        {
            return NULL;
        }
    
        // slave pty with a closed master pty
        if(pty_slaves[minor]->tty.flags & TTY_FLAG_MASTER_CLOSED)
        {
            return NULL;
        }
    
        return &pty_slaves[minor]->tty;
    }
        
    return NULL;
}

