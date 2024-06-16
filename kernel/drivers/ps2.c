/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: ps2.c
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
 *  \file ps2.c
 *
 *  Common PS/2 Keyboard and Mouse device driver implementation.
 */

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             KEY_BUF_SIZE

#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/kbd.h>
#include <kernel/kqueue.h>
#include <kernel/mouse.h>
#include <kernel/io.h>
#include <kernel/irq.h>
#include <kernel/pic.h>
#include <kernel/idt.h>
#include <kernel/asm.h>

#define MOUSE_CMD_SET_SAMPLE_RATE       0xF3
#define MOUSE_CMD_STATUS_REQUEST        0xE9

struct handler_t ps2_kbd_handler =
{
    .handler = sharedps2_callback,
    .handler_arg = 0,
    .short_name = "keyboard",
};

struct handler_t ps2_mouse_handler =
{
    .handler = sharedps2_callback,
    .handler_arg = 0,
    .short_name = "mouse",
};


static inline int wait_input(void)
{
    int timeout = 1000000;
    volatile uint8_t in;
    
    while(timeout--)
    {
        in = inb(KBD_CTRL_STATS_REG);
        
        if(!(in & 0x2))
        {
            return 0;
        }
    }
    
    return -1;
}


static inline int wait_output(void)
{
    int timeout = 1000000;
    volatile uint8_t in;
    
    while(timeout--)
    {
        in = inb(KBD_CTRL_STATS_REG);
        
        if(in & 0x1)
        {
            return 0;
        }
    }
    
    return -1;
}

static inline void send_command(uint8_t cmd)
{
    wait_input();
    outb(KBD_CTRL_CMD_REG, cmd);
}


static inline void send_command2(uint8_t cmd, uint8_t byte)
{
    wait_input();
    outb(KBD_CTRL_CMD_REG, cmd);
    wait_input();
    outb(KBD_ENC_INPUT_BUF, byte);
}


static inline uint8_t ps2_kbd_write(uint8_t byte)
{
    wait_input();
    outb(KBD_ENC_INPUT_BUF, byte);
    wait_output();
    return inb(KBD_ENC_INPUT_BUF);
}


static inline uint8_t ps2_mouse_write(uint8_t byte)
{
    send_command2(KBD_CTRL_CMD_MOUSE_WRITE, byte);
    wait_output();
    return inb(KBD_ENC_INPUT_BUF);
}


/*
 * Set LEDs.
 */
void kbd_set_leds(int num, int caps, int scroll)
{
    uint8_t data = 0;

    // set or clear the bit
    data = (scroll) ? (data | 1) : (data & 1);
    data = (num) ? (num | 2) : (num & 2);
    data = (caps) ? (num | 4) : (num & 4);

    // send the command -- update keyboard Light Emetting Diods (LEDs)
    wait_input();
    outb(KBD_ENC_CMD_REG, KBD_ENC_CMD_SET_LED);
    wait_input();
    outb(KBD_ENC_CMD_REG, data);
}


/*
 * Reset the system.
 *
 * See: https://wiki.osdev.org/Reboot
 */
void kbd_reset_system(void)
{
#if 0
    extern struct idt_descriptor_s IDT[MAX_INTERRUPTS];

    /* NULL the IDT */
    memset((void *)&IDT[0], 0, sizeof(struct idt_descriptor_s) * 
                                            MAX_INTERRUPTS - 1);

    /* keyboard reset */
    wait_input();
    outb(KBD_ENC_CMD_REG, KBD_CTRL_CMD_SYSTEM_RESET);
    empty_loop();
#endif

    uint8_t tmp;
    
    cli();
    
    do
    {
        tmp = inb(KBD_CTRL_STATS_REG);
        
        if(tmp & 1)
        {
            inb(KBD_ENC_CMD_REG);
        }
    } while(tmp & 2);
    
    outb(KBD_CTRL_STATS_REG, KBD_CTRL_CMD_SYSTEM_RESET);

loop:
    hlt();
    goto loop;
}


/*
 * Code to handle an issue with QEmu that leads to garbage data when both
 * PS/2 keyboard and mouse are used together. Code is taken from ToaruOS.
 * See: https://github.com/toaruos/misaka/blob/master/kernel/arch/x86_64/ps2hid.c
 */
int sharedps2_callback(struct regs *r, int arg)
{
    UNUSED(arg);

    volatile uint8_t status, data;

    // disable both ports
    send_command(KBD_CTRL_CMD_DISABLE);
    send_command(KBD_CTRL_CMD_MOUSE_DISABLE);
    
    // read status & data
    status = inb(KBD_CTRL_STATS_REG);
    data = inb(KBD_ENC_INPUT_BUF);

    // re-enable both ports
    send_command(KBD_CTRL_CMD_ENABLE);
    send_command(KBD_CTRL_CMD_MOUSE_ENABLE);

    if(!(status & 0x01))
    {
        //pic_send_eoi(r->int_no - 32);
        //return 1;
        return 0;
    }
    
    if(!(status & 0x20))
    {
        kbd_handle_code(data);
    }
    else if(status & 0x21)
    {
        mouse_handle_code(data);
    }
    else
    {
        pic_send_eoi(r->int_no - 32);
    }
    
    return 1;
}


void ps2_init(void)
{
    volatile uint8_t byte;
    int timeout = 1000;
    
    printk("ps2: initializing PS/2 keyboard and mouse..\n");
    
    // disable both ports
    send_command(KBD_CTRL_CMD_DISABLE);
    send_command(KBD_CTRL_CMD_MOUSE_DISABLE);
    
    // discard any data
    while((byte = inb(KBD_CTRL_STATS_REG)) & 1)
    {
        if(--timeout == 0)
        {
            break;
        }
        
        inb(KBD_ENC_INPUT_BUF);
    }
    
    if(timeout == 0)
    {
        printk("ps2: timed out during initialization\n");
        return;
    }

    // init our buffer
    kbdbuf_init(&kbd_queue, keybuf /*, KEY_BUF_SIZE */);

    // run self test

    // send command
    send_command(KBD_CTRL_CMD_SELF_TEST);

    // wait for output buffer to be full
    wait_output();

    // if output buffer == 0x55, test passed
    byte = inb(KBD_ENC_INPUT_BUF);

    if(byte != 0x55)
    {
        printk("ps2: failed self test\n");
        return;
    }
    
    // enable interrupt lines and translation
    wait_input();
    outb(KBD_CTRL_CMD_REG, KBD_CTRL_CMD_READ);
    wait_output();
    byte = inb(KBD_ENC_INPUT_BUF);
    byte |= (0x01 | 0x02 | 0x40);
    send_command2(KBD_CTRL_CMD_WRITE, byte);
    
    // enable both ports
    send_command(KBD_CTRL_CMD_ENABLE);
    send_command(KBD_CTRL_CMD_MOUSE_ENABLE);
    
    // select scanmode 2
    ps2_kbd_write(KBD_ENC_CMD_SCAN_CODE_SET);
    ps2_kbd_write(2);

    // set lock keys and led lights
    _numlock = 0;
    _scrolllock = 0;
    _capslock = 0;
    //kbd_set_leds(0, 0, 0);

    // shift, ctrl, and alt keys
    _shift = _alt = _ctrl = 0;
    
    // configure mouse
    _mouse_id = 0;
    byte_count = 3;
    mouse_cycle = 0;

    ps2_mouse_write(KBD_ENC_CMD_RESETSCAN);         // set defaults
    ps2_mouse_write(KBD_ENC_CMD_ENABLE);            // turn data on
    
    // enable mouse wheel (if available)
    ps2_mouse_write(KBD_ENC_CMD_ID);    // get mouse ID
    wait_output();
    inb(KBD_ENC_INPUT_BUF);
    ps2_mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);     // set sample rate
    ps2_mouse_write(200);
    ps2_mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    ps2_mouse_write(100);
    ps2_mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    ps2_mouse_write(80);
    ps2_mouse_write(KBD_ENC_CMD_ID);    // get mouse ID
    wait_output();
    byte = inb(KBD_ENC_INPUT_BUF);
    
    if(byte == 3)
    {
        _mouse_id = 3;
        byte_count = 4;
    }
    
    // enable 4th & 5th buttons
    ps2_mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);     // set sample rate
    ps2_mouse_write(200);
    ps2_mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    ps2_mouse_write(100);
    ps2_mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    ps2_mouse_write(80);
    ps2_mouse_write(KBD_ENC_CMD_ID);    // get mouse ID
    wait_output();
    byte = inb(KBD_ENC_INPUT_BUF);
    
    if(byte == 4)
    {
        _mouse_id = 4;
    }

    // mouse status request
    byte = ps2_mouse_write(MOUSE_CMD_STATUS_REQUEST);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(byte == KBD_ERR_ACK)
    {
        // get the status byte
        byte = inb(KBD_ENC_INPUT_BUF);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        printk("ps2: mouse status 0x%x\n", byte);

        if(byte & 0x10)
        {
            mouse_scaled = 1;
        }

        // discard the other 2 bytes
        inb(KBD_ENC_INPUT_BUF);
        inb(KBD_ENC_INPUT_BUF);
    }

    // now install our IRQ handlers
    
    // only for QEmu
    register_irq_handler(IRQ_KBD, &ps2_kbd_handler);
    register_irq_handler(IRQ_MOUSE, &ps2_mouse_handler);
    enable_irq(IRQ_MOUSE);
    enable_irq(IRQ_KBD);
}

