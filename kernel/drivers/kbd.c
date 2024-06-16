/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kbd.c
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
 *  \file kbd.c
 *
 *  PS/2 Keyboard device driver implementation.
 */

//#define __DEBUG

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             KEY_BUF_SIZE

#include <kernel/laylaos.h>
#include <kernel/kbd.h>
#include <kernel/kqueue.h>
#include <kernel/mouse.h>
#include <kernel/io.h>
#include <kernel/irq.h>
#include <kernel/asm.h>
#include <kernel/tty.h>
#include <kernel/pic.h>
#include <kernel/keycodes.h>
#include <kernel/mutex.h>
#include <string.h>

uint16_t keybuf[KEY_BUF_SIZE];
struct kqueue_t kbd_queue;

struct task_t *kbd_task = NULL;

struct kernel_mutex_t kbd_lock = { 0, };



/*
 * Code adopted from BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 */

// lock keys
volatile int _numlock, _scrolllock, _capslock;

// shift, alt, and ctrl keys current state
volatile int _shift, _alt, _ctrl;

extern char xt_keycodes[];
extern char xt_esc_keycodes[];

static inline
void scancode_to_keycode(int scancode, int ext)
{
    // test if this is a break code (Original XT Scan Code Set specific)
    volatile int brk = (scancode & 0x80 /* KEYCODE_BREAK_MASK */) << 8;
    char code;
    
    scancode &= ~KEYCODE_BREAK_MASK;

    code = ext ? xt_esc_keycodes[scancode] : xt_keycodes[scancode];
    
    if(code == 0)
    {
        return;
    }

    kbdbuf_enqueue(&kbd_queue, code | brk);
}


void kbd_handle_code(int code)
{
    static int  _extended = 0;
    volatile int unblock = 0;
    
    // is this an extended code? If so, set it and return
    if(code == 0xE0 || code == 0xE1)
    {
        _extended = 1;
    }
    else
    {
        scancode_to_keycode((code & 0xff), _extended);
        _extended = 0;
        unblock = 1;
    }

    pic_send_eoi(IRQ_KBD);

    if(unblock)
    {
        unblock_kernel_task(kbd_task);
    }
}


/*
 * Keyboard task function.
 */
void kbd_task_func(void *arg)
{
    UNUSED(arg);

    volatile int code;
    volatile int has_keys;

    for(;;)
    {
        volatile struct tty_t *tty = &ttytab[cur_tty];
        has_keys = 0;

        //KDEBUG("kbd_task_func: checking\n");

        while(!kbdbuf_is_empty(&kbd_queue))
        {
            code = kbdbuf_dequeue(&kbd_queue) & 0xffff;

            tty->process_key((struct tty_t *)tty, code);

            has_keys = 1;
        }

        if(has_keys)
        {
            tty->copy_to_buf((struct tty_t *)tty);
        }

        // check no new keys since we spent some time copying keys and waking
        // up tasks in copy_to_buf() above
        if(kbdbuf_is_empty(&kbd_queue))
        {
            //block_task(&kbd_queue, 0);
            block_task2(&kbd_queue, 1000);
        }
    }
}

