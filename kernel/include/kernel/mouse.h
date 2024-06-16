/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: mouse.h
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
 *  \file mouse.h
 *
 *  Functions and macros for working with mice.
 */

#ifndef __MOUSE_DRIVER_H__
#define __MOUSE_DRIVER_H__

#define MOUSE_LBUTTON_DOWN      0x01    /**< Left button down */
#define MOUSE_RBUTTON_DOWN      0x02    /**< Right button down */
#define MOUSE_MBUTTON_DOWN      0x04    /**< Middle button down */
#define MOUSE_VSCROLL_DOWN      0x08    /**< Vertical scroll down */
#define MOUSE_VSCROLL_UP        0x10    /**< Vertical scroll up */
#define MOUSE_4BUTTON_DOWN      0x20    /**< 4th button down */
#define MOUSE_5BUTTON_DOWN      0x40    /**< 5th button down */
#define MOUSE_HSCROLL_RIGHT     0x80    /**< Horizontal scroll right */
#define MOUSE_HSCROLL_LEFT      0x100   /**< Horizontal scroll left */

#include <stdint.h>

/**
 * @typedef mouse_buttons_t
 * @brief The mouse_buttons_t typedef.
 *
 * Mouse button status.
 */
typedef uint16_t mouse_buttons_t;


/**
 * @struct mouse_packet_t
 * @brief The mouse_packet_t structure.
 *
 * The kernel processes mouse input into packets, which are represented by
 * this structure.
 */
struct mouse_packet_t
{
    int type;                 /**< packet type */
    int dx,                   /**< delta x */
        dy;                   /**< delta y */
    mouse_buttons_t buttons;  /**< current button state */
};


/**
 * @var _mouse_id
 * @brief PS/2 mouse id.
 *
 * If >= zero, this is the PS/2 mouse id (0, 3, or 4 depending on whether 
 * mouse has 3 buttons, horizontal and vertical scroll wheels, etc).
 */
extern char _mouse_id;

/**
 * @var mouse_cycle
 * @brief current mouse cycle.
 *
 * Current byte in the mouse cycle (see \ref byte_count).
 */
extern volatile unsigned char mouse_cycle;

/**
 * @var byte_count
 * @brief mouse cycle byte count.
 *
 * The count of bytes in each mouse cycle (3 or 4 for PS/2 mice).
 */
extern unsigned char byte_count;

/**
 * @var mouse_task
 * @brief kernel mouse task.
 *
 * The kernel task that handles incoming mouse input.
 */
extern struct task_t *mouse_task;


extern int mouse_scaled;


/*****************************************
 * Functions defined in drivers/mouse.c.
 *****************************************/

/**
 * @brief Add a new mouse packet.
 *
 * When a mouse IRQ occurs, this function is called to add the new mouse
 * coordinates (more accurately, delta values) and button status.
 * Packets are queued until a client performs a read operation on the mouse
 * input device (major = 13, minor = 32). Packets are stored in a circular
 * buffer, when the buffer is full, old (unread) packets are lost.
 *
 * @param   dx          delta x
 * @param   dy          delta y
 * @param   buttons     mouse button state
 *
 * @return  nothing.
 */
void add_mouse_packet(int dx, int dy, mouse_buttons_t buttons);

/**
 * @brief Check for mouse packets.
 *
 * This function sleeps until one or more mouse packets are available.
 * When mouse input is available, the function wakes any sleeping tasks
 * waiting for mouse data.
 *
 * @param   arg     unused (all kernel callback functions require a single
 *                          argument of type void pointer)
 *
 * @return  nothing.
 */
void mouse_task_func(void *arg);

/**
 * @brief Handle mouse input.
 *
 * Convert a raw mouse byte to a kernel mouse packet. The packet
 * is stored in the mouse buffer and the mouse kernel task is woken up
 * if it is sleeping.
 *
 * @param   code        raw mouse byte
 *
 * @return  nothing.
 */
void mouse_handle_code(int code);

#endif      /* __MOUSE_DRIVER_H__ */
