/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: tty_input.c
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
 *  \file tty_input.c
 *
 *  The terminal (tty) device driver implementation.
 *  The driver's code is split between these files:
 *    - tty.c => device initialisation, general interface, and read/write 
 *               functions
 *    - tty_input.c => handling terminal input
 *    - tty_ioctl.c => terminal device control (ioctl)
 *    - tty_state.c => saving and restoring device state
 */

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#define KBDUS_DEFINE_KEYCODES
#include <ctype.h>
#include <kernel/kbdus.h>
#include <kernel/tty.h>
#include <kernel/kbd.h>
#include <kernel/task.h>

#undef tolower
#define tolower(c)	((c) + 0x20 * (((c) >= 'A') && ((c) <= 'Z')))

// Number of possible signals the tty can send during input processing
#define TTY_ISIGS           3

// The following array helps us when we encounter control characters that 
// result in signal production while processing input.
// The first struct member holds an index into the termios's c_cc[] array, 
// while the second member contains the number of the signal to be sent.
struct
{
    int c_cc;
    int signal;
}
tty_isig[] =
{
    { VINTR, SIGINT },
    { VQUIT, SIGQUIT },
    { VSUSP, SIGTSTP },
};


static inline void emit_codes(struct kqueue_t *q, char *codes, int count)
{
    char *c = codes;
    
    while(count--)
    {
        ttybuf_enqueue(q, *c);
        c++;
    }
}


/*
 * Process tty input.
 */
void process_key(struct tty_t *tty, int c)
{
    char codes[8];
    int count;
	
	// test if this is a break code
    int brk = (c & 0x8000) ? 1 : 0;
    int index;
    uint32_t scancode;
    
    c &= 0xff;
    
    index = _capslock ? 6 :
            _numlock ? 4:
            _ctrl ? 2 : 0;

    if(_shift)
    {
        index++;
    }
    
    scancode = kbdus.key[c][index];
    
    // handle special keys first
    if(scancode == 0xff)
    {
        switch(c)
        {
            case KEYCODE_LCTRL:
            case KEYCODE_RCTRL:
    			_ctrl = !brk;
    			return;

            case KEYCODE_LSHIFT:
            case KEYCODE_RSHIFT:
    			_shift = !brk;
                //printk("process_key: _shift %d\n", _shift);
    			return;

            case KEYCODE_LALT:
            case KEYCODE_RALT:
    			_alt = !brk;
    			return;

    		case KEYCODE_CAPS:
    		    if(!brk)
    		    {
    				_capslock = !_capslock;
    				kbd_set_leds(_numlock, _capslock, _scrolllock);
    			}
    			return;

    		case KEYCODE_NUM:
    		    if(!brk)
    		    {
    				_numlock = !_numlock;
    				kbd_set_leds(_numlock, _capslock, _scrolllock);
    			}
    			return;

    		case KEYCODE_SCROLL:
    		    if(!brk)
    		    {
    				_scrolllock = !_scrolllock;
    				kbd_set_leds(_numlock, _capslock, _scrolllock);
    			}
    			return;
        }
    }

    if(scancode == 0 || brk)
    {
        return;
    }

    count = 0;

    if(c >= KEYCODE_UP && c <= KEYCODE_RIGHT)
    {
        // For arrow keys, we emit the following sequences (taking arrow Right
        // as an example):
        //   - Right             =>   ^[[C
        //   - SHIFT-Right       =>   ^[[1;2C
        //   - CTRL-Right        =>   ^[[1;5C
        //   - CTRL-SHIFT-Right  =>   ^[[1;6C

        if((scancode & 0x5b00) == 0x5b00)
        {
            // In application keypad mode, cursor keys send ESC O x instead of ESC [ x.
            // Modify the code by removing the '[' and putting an 'O' in its place.
            if((tty->flags & TTY_FLAG_APP_KEYMODE))
            {
                scancode &= 0xffff00ff;
                scancode |= 0x4f00;
            }
        }
        else
        {
            uint32_t scancode2 = CTRL_ARROW_PROLOGUE;

            while(scancode2 != 0)
            {
                codes[count++] = scancode2 & 0xff;
                scancode2 >>= 8;
            }
        }
    }

    while(scancode != 0)
    {
        codes[count++] = scancode & 0xff;
        scancode >>= 8;
    }

    if(_alt)
    {
        /*
         * If the key is pressed with ALT (aka Meta key), we precede the
         * key code(s) by an extra ESC char.
         *
         * TODO: support the other possibility, which is setting the high
         *       order bit of the char (see `man setmetamode` for more).
         */
        if(ttybuf_has_space_for(&tty->read_q, count + 1))
        {
            ttybuf_enqueue(&tty->read_q, '\033');
            emit_codes(&tty->read_q, (char *)codes, count);
        }
    }
    else
    {
        if(ttybuf_has_space_for(&tty->read_q, count))
        {
            emit_codes(&tty->read_q, (char *)codes, count);
        }
    }
}


void raw_process_key(struct tty_t *tty, int code)
{
    volatile char codes[2];

    /*
     * In 'raw' mode (that is used by tty2 for the GUI environment), we
     * enqueue 2 bytes:
     * [0] flags (currently we only support 0x80, for break codes)
     * [1] key code
     */

    codes[0] = (code & 0x8000) ? KEYCODE_BREAK_MASK : 0;
    codes[1] = (code & 0xff);

    if(ttybuf_has_space_for(&tty->secondary, 2))
    {
        emit_codes(&tty->secondary, (char *)codes, 2);
    }
}


/*
 * Copy input from the terminal device's read queue to the secondary queue
 * from which reading tasks can fetch input. If the terminal device is in
 * canonical mode, it also does some input processing on the input.
 *
 * For more details, see: https://man7.org/linux/man-pages/man3/termios.3.html
 *
 * Input:
 *    tty => pointer to the terminal device struct
 */
void copy_to_buf(struct tty_t *tty)
{
    char c;
    int i;
    int strip = (tty->termios.c_iflag & ISTRIP);
    
    // otherwise, process input
    while(!ttybuf_is_empty(&tty->read_q) && !ttybuf_is_full(&tty->secondary))
    {
        c = ttybuf_dequeue(&tty->read_q);
        
        // strip 8th bit if needed
        if(strip)
        {
            c &= ~0x80;
        }
        
        if(c == CR)
        {
            if(tty->termios.c_iflag & IGNCR)
            {
                // ignore CR
                continue;
            }

            if(tty->termios.c_iflag & ICRNL)
            {
                // convert CR to LF
                c = LF;
            }
        }
        else if(c == LF && (tty->termios.c_iflag & INLCR))
        {
            // convert LF to CR
            c = CR;
        }
        
        // convert to lowercase if needed
        if(tty->termios.c_iflag & IUCLC)
        {
            c = tolower(c);
        }
        
        // process input for canonical mode
        if(tty->termios.c_lflag & ICANON)
        {
            // erase character
            if(c == (char)tty->termios.c_cc[VERASE])
            {
                // don't erase last char if any of the following is true:
                //   - queue is empty
                //   - last char is LF
                //   - last char is EOF
                if(ttybuf_is_empty(&tty->secondary) ||
                   (((char *)tty->secondary.buf)
                        [((tty->secondary.head - 1) % TTY_BUF_SIZE)]
                            == LF) ||
                   c == (char)tty->termios.c_cc[VEOF])
                {
                    continue;
                }
                
                // echo DEL char if needed
                if(tty->termios.c_lflag & ECHO)
                {
                    if(c < 32)
                    {
                        // if control char, output an extra DEL char
                        ttybuf_enqueue(&tty->write_q, 127);
                    }

                    // output DEL char
                    ttybuf_enqueue(&tty->write_q, 127);
                    
                    tty->write(tty);
                }
                
                // remove the last char from the secondary queue
                tty->secondary.head = (tty->secondary.head - 1) % 
                                            TTY_BUF_SIZE;
            }
            
            if(tty->stopped && (tty->termios.c_iflag & IXANY))
            {
                tty->stopped = 0;
                continue;
            }
            
            // stop char
            if(c == (char)tty->termios.c_cc[VSTOP] && 
               (tty->termios.c_iflag & IXOFF))
            {
                tty->stopped = 1;
                continue;
            }
            
            // start char
            if(c == (char)tty->termios.c_cc[VSTART] && 
               (tty->termios.c_iflag & IXOFF))
            {
                tty->stopped = 0;
                continue;
            }
        }

        if(c == LF || c == (char)tty->termios.c_cc[VEOF])
        {
            // flag that we have input lines (for canonical tty readers)
            tty->secondary.extra++;
        }
        
        /*
         * This bit needs revision.
         * See: https://man7.org/linux/man-pages/man3/termios.3.html
         */
        if(tty->termios.c_iflag & BRKINT)
        {
            if(!(tty->termios.c_iflag & IGNBRK) && c == '\0')
            {
                flush_queue(&tty->read_q);
                flush_queue(&tty->write_q);
                flush_queue(&tty->secondary);
                tty_send_signal(tty->pgid, SIGINT);
                continue;
            }
        }
        
        if(tty->termios.c_lflag & ECHO)
        {
            if(c == LF)
            {
                ttybuf_enqueue(&tty->write_q, 10);
                ttybuf_enqueue(&tty->write_q, 13);
            }
            else if(c < 32)
            {
                if(tty->termios.c_lflag & ECHOCTL)
                {
                    ttybuf_enqueue(&tty->write_q, '^');
                    ttybuf_enqueue(&tty->write_q, c + 64);
                }
            }
            else
            {
                ttybuf_enqueue(&tty->write_q, c);
            }

            if((c == LF || c == VT || c == FF) && 
               (tty->flags & TTY_FLAG_LFNL))
            {
                ttybuf_enqueue(&tty->write_q, CR);
            }
            
            tty->write(tty);
        }

        ttybuf_enqueue(&tty->secondary, c);

        if(tty->termios.c_lflag & ISIG)
        {
            for(i = 0; i < TTY_ISIGS; i++)
            {
                if(c == (char)tty->termios.c_cc[tty_isig[i].c_cc])
                {
                    tty_send_signal(tty->pgid, tty_isig[i].signal);
                    break;
                }
            }
            
            if(i < TTY_ISIGS)
            {
                //ttybuf_enqueue(&tty->secondary, LF);
                continue;
            }
        }
    }

    // wake up select() waiting tasks
    selwakeup(&tty->ssel);
    
    // wake up waiting tasks
    if(tty->waiting_task)
    {
        unblock_task((struct task_t *)tty->waiting_task);
    }
}


void raw_copy_to_buf(struct tty_t *tty)
{
    // wake up select() waiting tasks
    selwakeup(&tty->ssel);
    
    // wake up waiting tasks
    if(tty->waiting_task)
    {
        unblock_task((struct task_t *)tty->waiting_task);
    }
}

