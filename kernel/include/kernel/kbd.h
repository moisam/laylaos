/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: kbd.h
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
 *  \file kbd.h
 *
 *  Functions and macros for working with PS/2 keyboards.
 */

#ifndef __KBD_DRIVER_H__
#define __KBD_DRIVER_H__

#define BREAK_CODE          0x80


/*
 * Code adopted from BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 */

#include <stdint.h>

enum KBD_ENCODER_IO
{
	KBD_ENC_INPUT_BUF	=	0x60,
	KBD_ENC_CMD_REG	    =	0x60
};

enum KBD_ENC_CMDS
{
	KBD_ENC_CMD_SET_LED				    =	0xED,
	KBD_ENC_CMD_ECHO					=	0xEE,
	KBD_ENC_CMD_SCAN_CODE_SET			=	0xF0,
	KBD_ENC_CMD_ID					    =	0xF2,
	KBD_ENC_CMD_AUTODELAY				=	0xF3,
	KBD_ENC_CMD_ENABLE				    =	0xF4,
	KBD_ENC_CMD_RESETWAIT				=	0xF5,
	KBD_ENC_CMD_RESETSCAN				=	0xF6,
	KBD_ENC_CMD_ALL_AUTO				=	0xF7,
	KBD_ENC_CMD_ALL_MAKEBREAK			=	0xF8,
	KBD_ENC_CMD_ALL_MAKEONLY			=	0xF9,
	KBD_ENC_CMD_ALL_MAKEBREAK_AUTO  	=	0xFA,
	KBD_ENC_CMD_SINGLE_AUTOREPEAT		=	0xFB,
	KBD_ENC_CMD_SINGLE_MAKEBREAK		=	0xFC,
	KBD_ENC_CMD_SINGLE_BREAKONLY		=	0xFD,
	KBD_ENC_CMD_RESEND			    	=	0xFE,
	KBD_ENC_CMD_RESET					=	0xFF
};

// keyboard controller

enum KBD_CTRL_IO
{
	KBD_CTRL_STATS_REG  =	0x64,
	KBD_CTRL_CMD_REG	=	0x64
};

enum KBD_CTRL_STATS_MASK
{
	KBD_CTRL_STATS_MASK_OUT_BUF	    =	1,		//00000001
	KBD_CTRL_STATS_MASK_IN_BUF	    =	2,		//00000010
	KBD_CTRL_STATS_MASK_SYSTEM	    =	4,		//00000100
	KBD_CTRL_STATS_MASK_CMD_DATA	=	8,		//00001000
	KBD_CTRL_STATS_MASK_LOCKED	    =	0x10,	//00010000
	KBD_CTRL_STATS_MASK_AUX_BUF	    =	0x20,	//00100000
	KBD_CTRL_STATS_MASK_TIMEOUT	    =	0x40,	//01000000
	KBD_CTRL_STATS_MASK_PARITY	    =	0x80	//10000000
};

enum KBD_CTRL_CMDS
{
	KBD_CTRL_CMD_READ				=	0x20,
	KBD_CTRL_CMD_WRITE			    =	0x60,
	KBD_CTRL_CMD_SELF_TEST		    =	0xAA,
	KBD_CTRL_CMD_INTERFACE_TEST	    =	0xAB,
	KBD_CTRL_CMD_DISABLE			=	0xAD,
	KBD_CTRL_CMD_ENABLE			    =	0xAE,
	KBD_CTRL_CMD_READ_IN_PORT		=	0xC0,
	KBD_CTRL_CMD_READ_OUT_PORT	    =	0xD0,
	KBD_CTRL_CMD_WRITE_OUT_PORT	    =	0xD1,
	KBD_CTRL_CMD_READ_TEST_INPUTS	=	0xE0,
	KBD_CTRL_CMD_SYSTEM_RESET		=	0xFE,
	KBD_CTRL_CMD_MOUSE_DISABLE	    =	0xA7,
	KBD_CTRL_CMD_MOUSE_ENABLE		=	0xA8,
	KBD_CTRL_CMD_MOUSE_PORT_TEST	=	0xA9,
	KBD_CTRL_CMD_MOUSE_WRITE		=	0xD4
};

// scan error codes

enum KBD_ERROR
{
	KBD_ERR_BUF_OVERRUN			=	0,
	KBD_ERR_ID_RET				=	0x83AB,
	KBD_ERR_BAT					=	0xAA,	//note: can also be L. shift key 
	                                        //      make code
	KBD_ERR_ECHO_RET			=	0xEE,
	KBD_ERR_ACK					=	0xFA,
	KBD_ERR_BAT_FAILED			=	0xFC,
	KBD_ERR_DIAG_FAILED			=	0xFD,
	KBD_ERR_RESEND_CMD			=	0xFE,
	KBD_ERR_KEY					=	0xFF
};


/**
 * \def KEY_BUF_SIZE
 *
 * Keyboard buffer size
 */
#define KEY_BUF_SIZE            1024


extern volatile int _numlock, _scrolllock, _capslock;
extern volatile int _shift, _alt, _ctrl;
extern uint16_t keybuf[KEY_BUF_SIZE];
extern struct kqueue_t kbd_queue;

/**
 * @var kbd_task
 * @brief kernel kbd task.
 *
 * The kernel task that handles incoming keyboard input.
 */
extern volatile struct task_t *kbd_task;


/***************************************
 * Functions defined in drivers/kbd.c.
 ***************************************/

/**
 * @brief Handle keyboard scancode.
 *
 * Convert a raw keyboard scancode to a kernel keycode. The converted keycode
 * is stored in the keyboard buffer and the keyboard kernel task is woken up
 * if it is sleeping.
 *
 * @param   code        scancode
 *
 * @return  nothing.
 */
void kbd_handle_code(int code);

/**
 * @brief Keyboard task function.
 *
 * Check if there are any keys in the keyboard input buffer, copy those to
 * the terminal's input buffer and wake up any waiting tasks. This is the
 * function that is run by \ref kbd_task.
 *
 * @param   arg     unused (all kernel callback functions require a single
 *                          argument of type void pointer)
 *
 * @return  nothing.
 */
void kbd_task_func(void *arg);


/***************************************
 * Functions defined in drivers/ps2.c.
 ***************************************/

/**
 * @brief Update LEDs.
 *
 * Update keyboard LED lights.
 *
 * @param   num     non-zero to set NumLock light
 * @param   caps    non-zero to set CapsLock light
 * @param   scroll  non-zero to set ScrollLock light
 *
 * @return  nothing.
 */
void kbd_set_leds(int num, int caps, int scroll);

/**
 * @brief Reset the system.
 *
 * This function attempts to restart the system via the keyboard controller.
 *
 * @return  nothing.
 */
void kbd_reset_system(void);

/**
 * @brief PS/2 IRQ callback function.
 *
 * Called when a PS/2 keyboard or mouse raise an IRQ.
 *
 * @param   r       current CPU registers
 * @param   arg     unused (other IRQ callback functions use this to identify
 *                    the unit that raised the IRQ)
 *
 * @return  1 if the IRQ was handled, 0 if not.
 */
int sharedps2_callback(struct regs *r, int arg);

/**
 * @brief Initialise PS/2 devices.
 *
 * Initialise PS/2 keyboard and mouse.
 *
 * @return  nothing.
 */
void ps2_init(void);

#endif      /* __KBD_DRIVER_H__ */
