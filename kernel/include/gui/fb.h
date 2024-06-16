/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: fb.h
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
 *  \file fb.h
 *
 *  Functions and macros for working with the framebuffer device.
 */

#ifndef __FRAMEBUFFER_H__
#define __FRAMEBUFFER_H__

/*
 * Framebuffer deivce ioctl() commands.
 */
#define FB_SWITCH_TTY           0x01    /**< ioctl() command to switch tty */
#define FB_GET_VBE_BUF          0x02    /**< ioctl() command to get VBE 
                                             buffer information */
#define FB_GET_SCREEN_INFO      0x03    /**< ioctl() command to get screen
                                             information */
#define FB_INVALIDATE_SCREEN    0x04    /**< ioctl() command to enable or
                                             disable automatic screen update */
#define FB_INVALIDATE_AREA      0x05    /**< ioctl() command to invalidate an
                                             area of the screen */
#define FB_SET_CURSOR           0x06    /**< ioctl() command to enable or
                                             disable the software cursor */
#define FB_MAP_VBE_BACKBUF      0x07    /**< ioctl() command to map VBE back
                                             buffer into calling task's 
                                             address space */
#define FB_GET_VBE_PALETTE      0x08    /**< ioctl() command to get the palette
                                             in palette-indexed mode */

#define fb_default_fgcolor      0xC8C8C8FF
#define fb_default_bgcolor      0x000000FF


#ifdef KERNEL

/*****************************
 * Functions defined in fb.c
 *****************************/

/**
 * @brief Reset framebuffer colors.
 *
 * This function resets the framebuffer device colors to their builtin
 * defaults (see fb.c for the actual color values).
 *
 * @param   tty     pointer to terminal device
 *
 * @return  nothing.
 */
void fb_reset_colors(struct tty_t *tty);

/**
 * @brief Initialize the framebuffer device.
 *
 * This function initializes the framebuffer, sets video size, assigns the
 * appropriate functions to handle the low-level interface with the video
 * device, and then resets the framebuffer device. This is different to 
 * what fb_init_screen() does.
 * This function should be called once (early) during boot.
 *
 * @return  nothing.
 *
 * @see fb_init_screen
 */
void fb_init(void);

/**
 * @brief Initialize the framebuffer screen.
 *
 * While fb_init() is called early during boot to save VGA/VBE
 * info and initialize low-level video support, this function simply clears
 * the screen and starts a kernel task to periodically update the screen.
 * This function should only be called once during boot.
 *
 * @return  nothing.
 *
 * @see fb_init
 */
void fb_init_screen(void);

/**
 * @brief General block device control function.
 *
 * Perform ioctl operations on the framebuffer device.
 *
 * @param   dev     device id referring to the framebuffer device
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int fb_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel);

/**
 * @var framebuf_mem
 * @brief VBE front buffer.
 *
 * This variable holds the address of the video screen's front buffer (i.e.
 * whatever is actually shown on the screen).
 */
//extern virtual_addr framebuf_mem;
extern uint8_t *fb_backbuf_text, *fb_backbuf_gui, *fb_cur_backbuf;

#endif      /* KERNEL */

#endif      /* __FRAMEBUFFER_H__ */
