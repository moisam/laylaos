/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: softint.h
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
 *  \file softint.h
 *
 *  Functions and macro defines for working with soft interrupts (similar to
 *  hardware interrupts but raised by software and are of lesser urgency).
 */

#ifndef __KERNEL_SOFTINT_H__
#define __KERNEL_SOFTINT_H__

/**
 * \def MAX_SOFTINT
 *
 * Maximum number of supported soft interrupts
 */
//#define MAX_SOFTINT         8
#define MAX_SOFTINT         4

/*
 * Cookies to identify some kernel soft interrupts.
 */
//#define SOFTINT_CLOCK       1   /**< kernel callouts softint cookie */
//#define SOFTINT_NET         2   /**< network softint cookie */
#define SOFTINT_SLEEP       1   /**< POSIX timers softint cookie */
#define SOFTINT_ITIMER      2   /**< itimers softint cookie */


struct softint_t
{
    char name[8];
    void *cookie;
    void (*handler)(int);
    int arg;
    char pending;
};

extern struct softint_t softints[MAX_SOFTINT];
extern volatile int pending_bitmap;


/**
 * @var softint_task
 * @brief kernel softint task.
 *
 * The kernel task that handles soft interrupts.
 */
extern struct task_t *softint_task;

extern struct task_t *softitimer_task;

extern struct task_t *softsleep_task;

/**********************************
 * Functions defined in softint.c
 **********************************/

/**
 * @brief Initialise soft interrupts.
 *
 * Initialize the kernel's soft interrupts table and bitmaps.
 *
 * @return  nothing.
 */
void softint_init(void);

/**
 * @brief Soft interrupts task function.
 *
 * Check if there are any pending soft interrupts and call the registered
 * soft interrupt function (if there is any).
 *
 * @param   arg     unused (all kernel callback functions require a single
 *                          argument of type void pointer)
 *
 * @return  nothing.
 */
void softint_task_func(void *arg);

/**
 * @brief Register a soft interrupt.
 *
 * The given \a name should be unique and less than 8 characters long.
 * Soft interrupts are identified by the given \a cookie, which is later
 * passed to softint_schedule(), softint_block() and softint_unblock()
 * if any of these calls is needed.
 *
 * @param   name    soft interrupt name (less than 8 chars)
 * @param   cookie  soft interrupt cookie (used to refer to this softint later)
 * @param   handler the handler function to call when this softint happens
 *
 * @return  zero on success, -(errno) on failure.
 */
void softint_register(char *name, int cookie, void (*handler)(int));

/**
 * @brief Schedule a soft interrupt.
 *
 * Schedule the soft interrupt identified by the given \a cookie, passing
 * \a arg to the handler function.
 *
 * NOTE: This function MUST be called with interrupts disabled!
 *
 * @param   cookie  soft interrupt cookie
 * @param   arg     argument to pass to handler
 *
 * @return  zero on success, -(errno) on failure.
 */
void softint_schedule(int cookie, int arg);

/**
 * @brief Block a soft interrupt.
 *
 * Block the soft interrupt identified by the given \a cookie.
 *
 * @param   cookie  soft interrupt cookie
 *
 * @return  \a cookie on success, NULL on failure.
 */
void softint_block(int cookie);

/**
 * @brief Unblock a soft interrupt.
 *
 * Unblock the soft interrupt identified by the given \a cookie.
 *
 * @param   cookie  soft interrupt cookie
 *
 * @return  \a cookie on success, NULL on failure.
 */
void softint_unblock(int cookie);


/**********************************
 * Functions defined in timeout.c
 **********************************/

/**
 * @brief Callout soft interrupt function.
 *
 * Software (low priority) clock interrupt.
 * Run periodic events from timeout queue.
 * This callback function is called by the soft interrupts kernel task.
 * It handles kernel callouts and calls any kernel functions that were
 * scheduled to run.
 *
 * @param   unused      unused argument (obviously)
 *
 * @return  nothing.
 */
//void softclock(int unused);


/**********************************
 * Functions defined in network.c
 **********************************/

/**
 * @brief Network soft interrupt function.
 *
 * This callback function is called by the soft interrupts kernel task.
 * It handles network interrupts and calls network protocol functions to 
 * handle network packages.
 *
 * @param   unused      unused argument (obviously)
 *
 * @return  nothing.
 */
//void softnet(int unused);


/**********************************
 * Functions defined in itimer.c
 **********************************/

/**
 * @brief Interval timers soft interrupt function.
 *
 * This callback function is called by the soft interrupts kernel task.
 * It handles interval timers and wakes up tasks with expired timers.
 *
 * @param   unused      unused argument (obviously)
 *
 * @return  nothing.
 */
//void softitimer(int unused);


/**********************************
 * Functions defined in clock.c
 **********************************/

/**
 * @brief Timers soft interrupt function.
 *
 * This callback function is called by the soft interrupts kernel task.
 * It handles POSIX timers and wakes up tasks with expired timers.
 *
 * @param   unused      unused argument (obviously)
 *
 * @return  nothing.
 */
//void softsleep(int unused);

#endif      /* __KERNEL_SOFTINT_H__ */
