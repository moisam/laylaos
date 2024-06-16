/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: timer.c
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
 *  \file timer.c
 *
 *  Timer IRQ callback function.
 */

//#define __DEBUG

#define _POSIX_MONOTONIC_CLOCK

//#include <sys/sched.h>
#include <sched.h>
#include <kernel/laylaos.h>
#include <kernel/pic.h>
#include <kernel/irq.h>
#include <kernel/io.h>
#include <kernel/task.h>
#include <kernel/clock.h>
#include <kernel/timer.h>
#include <kernel/ksignal.h>
#include <kernel/softint.h>


#include "task_funcs.c"
#include "../../vdso/vdso.h"


unsigned long long ticks = 0;
unsigned long long prev_ticks = 0;

int early_timer_callback(struct regs *r, int arg);
int timer_callback(struct regs *r, int arg);

// the "proper" timer handler we use after tasking is enabled
struct handler_t timer_handler =
{
    .handler = timer_callback,
    .handler_arg = 0,
    .short_name = "timer",
};

// the "early" timer handler we use before tasking is enabled
struct handler_t early_timer_handler =
{
    .handler = early_timer_callback,
    .handler_arg = 0,
    .short_name = "timer",
};


#define FIX_MONOTONIC()                             \
    while(monotonic_time.tv_nsec >= 1000000000)     \
    {                                               \
        monotonic_time.tv_sec++;                    \
        monotonic_time.tv_nsec -= 1000000000;       \
    }

#define FIX_VDSO_MONOTONIC()                        \
    vdso_monotonic->tv_sec = monotonic_time.tv_sec; \
    vdso_monotonic->tv_nsec = monotonic_time.tv_nsec;


int early_timer_callback(struct regs *r, int arg)
{
    UNUSED(r);
    UNUSED(arg);

    ticks++;

    monotonic_time.tv_nsec += NSECS_PER_TICK;
    
    pic_send_eoi(IRQ_TIMER);
    FIX_MONOTONIC();
    FIX_VDSO_MONOTONIC();
    return 1;
}


int timer_callback(struct regs *r, int arg)
{
    UNUSED(r);
    UNUSED(arg);

	int schedule = 0;

    ticks++;

    monotonic_time.tv_nsec += NSECS_PER_TICK;
    
    /* wakeup nanosleepers */
    clock_check_waiters();
    
    /* decrement virtual timers */
    dec_itimers();

    if(--cur_task->time_left <= 0)
    {
        /* NOTE: don't interrupt realtime FIFO tasks.
         *       those kind of tasks will not have a valid 'time_left' 
         *       field.
         */
        if(cur_task->sched_policy != SCHED_FIFO)
        {
            schedule = 1;
        }
    }
    
    pic_send_eoi(IRQ_TIMER);

    if(schedule)
    {
        FIX_MONOTONIC();

        rlim_t limit = cur_task->task_rlimits[RLIMIT_CPU].rlim_cur;

        if(cur_task->sched_policy == SCHED_OTHER &&
            limit != RLIM_INFINITY &&
            (cur_task->user_time + cur_task->sys_time) >= (limit * PIT_FREQUENCY))
        {
            user_add_task_signal(cur_task, SIGXCPU, 1);
        }

        lock_scheduler();
        scheduler();
        unlock_scheduler();
    }

    FIX_VDSO_MONOTONIC();

    return 1;
}


/*
 * Initialise system clock.
 */
void timer_init(void)
{
    printk("Initialising clock..\n");
    register_irq_handler(IRQ_TIMER, &early_timer_handler);
    enable_irq(IRQ_TIMER);
    
    uint32_t divisor = 1193180 / PIT_FREQUENCY;
    outb(0x43, 0x36);
    
    uint8_t l = (uint8_t)(divisor & 0xff);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xff);
    
    outb(0x40, l);
    outb(0x40, h);
}


void switch_timer(void)
{
    unregister_irq_handler(IRQ_TIMER, &early_timer_handler);
    register_irq_handler(IRQ_TIMER, &timer_handler);
}

