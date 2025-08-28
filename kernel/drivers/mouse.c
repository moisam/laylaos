/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mouse.c
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
 *  \file mouse.c
 *
 *  PS/2 Mouse device driver implementation.
 */

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <kernel/laylaos.h>
#include <kernel/kbd.h>
#include <kernel/mouse.h>
#include <kernel/io.h>
#include <kernel/vga.h>
#include <kernel/irq.h>
#include <kernel/pic.h>
#include <kernel/task.h>
#include <kernel/select.h>
#include <kernel/mutex.h>

/*
 * See: https://wiki.osdev.org/Mouse_Input
 */

char _mouse_id = -1;
int mouse_scaled = 0;

volatile unsigned char mouse_cycle = 0;
unsigned char byte_count = 0;        /* bytes in each mouse packet */

volatile struct task_t *mouse_task = NULL;
struct selinfo mouse_ssel = { 0, };
volatile struct kernel_mutex_t mouse_lock = { 0, };

mouse_buttons_t cur_button_state = 0;

// table to help convert scaled mouse movement for small deltas (< 6)
static int scaled_value[] =
{
    0, 1, 1, 3, 6, 9,
};


/*
 * Handle mouse input.
 */
void mouse_handle_code(int code)
{
    mouse_buttons_t buttons;
    static unsigned char mouse_byte[5] = { 0, };

    mouse_byte[mouse_cycle] = code;
    mouse_cycle++;

    if(mouse_cycle >= byte_count)    /* all bytes have been received */
    {
        mouse_cycle = 0;        /* restart the cycle */

        int dx = mouse_byte[1]; // - (0x100 & (mouse_byte[0] << (8-4)));
        int dy = mouse_byte[2]; // - (0x100 & (mouse_byte[0] << (8-5)));
        int state = mouse_byte[0];
        
        dx = dx - ((state << 4) & 0x100);
        dy = dy - ((state << 3) & 0x100);

        if((mouse_byte[0] & 0x80) || (mouse_byte[0] & 0x40))
        {
            // overflow
            dx = 0;
            dy = 0;
        }

        // For scaled (non-linear) movement, if the unscaled delta is < 6, we
        // convert it using the table above. For values >= 6, we double the
        // delta value.
        if(mouse_scaled)
        {
            if(dx < 6)
            {
                dx = scaled_value[dx];
            }
            else
            {
                dx <<= 1;
            }

            if(dy < 6)
            {
                dy = scaled_value[dy];
            }
            else
            {
                dy <<= 1;
            }
        }        
        
        static mouse_buttons_t b0[] =
        {
            0,
            MOUSE_LBUTTON_DOWN,
            MOUSE_RBUTTON_DOWN,
            MOUSE_LBUTTON_DOWN | MOUSE_RBUTTON_DOWN,
            MOUSE_MBUTTON_DOWN,
            MOUSE_MBUTTON_DOWN | MOUSE_LBUTTON_DOWN,
            MOUSE_MBUTTON_DOWN | MOUSE_RBUTTON_DOWN,
            MOUSE_MBUTTON_DOWN | MOUSE_RBUTTON_DOWN | MOUSE_LBUTTON_DOWN,
        };
        
        static mouse_buttons_t b3[] =
        {
            0,
            MOUSE_VSCROLL_UP,
            MOUSE_HSCROLL_RIGHT,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            MOUSE_HSCROLL_LEFT,
            MOUSE_VSCROLL_DOWN,
        };
        
        /* check button statuses */
        buttons = b0[mouse_byte[0] & 0x07];

        if(byte_count == 4 && mouse_byte[3])
        {
            if(_mouse_id == 4)
            {
                if(mouse_byte[3] & 0x20)
                {
                    buttons |= MOUSE_5BUTTON_DOWN;
                }

                if(mouse_byte[3] & 0x10)
                {
                    buttons |= MOUSE_4BUTTON_DOWN;
                }

                buttons |= b3[mouse_byte[3] & 0x0f];
            }
            else
            {
                if((int8_t)mouse_byte[3] > 0)
                {
                    buttons |= MOUSE_VSCROLL_DOWN;
                }
                else
                {
                    buttons |= MOUSE_VSCROLL_UP;
                }
            }
        }

        cur_button_state = buttons;

        add_mouse_packet(dx, dy, buttons);
        pic_send_eoi(IRQ_MOUSE);

        unblock_kernel_task(mouse_task);
    }
    else
    {
        pic_send_eoi(IRQ_MOUSE);
    }
}

