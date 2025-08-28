/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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
#include <kernel/user.h>
#include <kernel/asm.h>


#include "task_funcs.c"
#include "../../vdso/vdso.h"


unsigned long long ticks = 0;
unsigned long long prev_ticks = 0;
unsigned long avenrun[3];


/* define some interrupt handlers and map them to their functions */
#define TIMER_HANDLER(which)                            \
    int which##_timer_callback(struct regs *r, int arg);\
    struct handler_t which##_timer_handler =            \
    {                                                   \
        .handler = which##_timer_callback,              \
        .handler_arg = 0,                               \
        .short_name = "timer",                          \
    };

// the "proper" timer handler we use once tasking is enabled on the Bootstrap
// Processor (BSP)
TIMER_HANDLER(bsp)

// the "early" timer handler we use before tasking is enabled on the BSP
TIMER_HANDLER(early)

// the timer handler we use on the Application Processors (APs)
TIMER_HANDLER(ap)


/*
 * Code to calculate system load average, taken from Linux sources.
 * The code and a commentary can be found at:
 *   https://en.wikipedia.org/wiki/Load_(computing)
 */
INLINE void calc_load(void)
{
    static int count = 0;

    if(++count >= LOAD_FREQ)
    {
        int running = get_running_task_count() +
                      get_blocked_task_count();

        CALC_LOAD(avenrun[0], EXP_1, running);
        CALC_LOAD(avenrun[1], EXP_5, running);
        CALC_LOAD(avenrun[2], EXP_15, running);
        count = 0;
    }
}


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


STATIC_INLINE int need_schedule(void)
{
    volatile struct task_t *cur_task = this_core->cur_task;

    /* decrement task's virtual timer, if any */
    if(cur_task->itimer_virt.rel_ticks)
    {
        /* only count ticks when running in user mode */
        if(cur_task->user && !cur_task->user_in_kernel_mode)
        {
            if(--cur_task->itimer_virt.rel_ticks == 0)
            {
                siginfo_t itimer_siginfo = { .si_code = SI_TIMER };

                /* reset timer if needed */
                cur_task->itimer_virt.rel_ticks = cur_task->itimer_virt.interval;
                //add_task_timer_signal(cur_task, SIGVTALRM, ITIMER_VIRT_ID);
                add_task_signal((struct task_t *)cur_task, SIGVTALRM, &itimer_siginfo, 1);
            }
        }
    }

    if(cur_task->sched_policy != SCHED_FIFO)
    {
        /* NOTE: don't interrupt realtime FIFO tasks.
         *       those kind of tasks will not have a valid 'time_left' 
         *       field.
         */
        if(--cur_task->time_left <= 0)
        {
            return 1;
        }
    }

    return 0;
}


STATIC_INLINE void fix_limits_and_schedule(void)
{
    volatile struct task_t *cur_task = this_core->cur_task;
    rlim_t limit = cur_task->task_rlimits[RLIMIT_CPU].rlim_cur;

    if(cur_task->sched_policy == SCHED_OTHER &&
       limit != RLIM_INFINITY &&
       (cur_task->user_time + cur_task->sys_time) >= (limit * PIT_FREQUENCY))
    {
        user_add_task_signal(cur_task, SIGXCPU, 1);
    }

    //printk("cpu[%d]: scheduling\n", this_core->cpuid);
    if(!(this_core->flags & SMP_FLAG_SCHEDULER_BUSY))
    {
        scheduler();
    }
    //printk("cpu[%d]: done scheduling\n", this_core->cpuid);
}


int bsp_timer_callback(struct regs *r, int arg)
{
    UNUSED(r);
    UNUSED(arg);

	int schedule;

    ticks++;

    monotonic_time.tv_nsec += NSECS_PER_TICK;

    /* calculate the load average (every 5 seconds) */
    calc_load();

    /* wakeup nanosleepers */
    clock_check_waiters();
    
    /* decrement virtual timers */
    //dec_itimers();

    schedule = need_schedule();
    pic_send_eoi(IRQ_TIMER);

    if(schedule)
    {
        FIX_MONOTONIC();
        fix_limits_and_schedule();
    }

    FIX_VDSO_MONOTONIC();

    return 1;
}


int ap_timer_callback(struct regs *r, int arg)
{
    UNUSED(r);
    UNUSED(arg);

    //printk("cpu[%d]: timer callback\n", this_core->cpuid);

	int schedule = need_schedule();
    //printk("cpu[%d]: schedule %d\n", this_core->cpuid, schedule);

    pic_send_eoi(123);

    if(schedule)
    {
        fix_limits_and_schedule();
    }

    //printk("cpu[%d]: schedule done\n", this_core->cpuid);

    return 1;
}


/*
 * Initialise system clock.
 */
void timer_init(void)
{
    printk("Initializing clock..\n");
    register_irq_handler(IRQ_TIMER, &early_timer_handler);
    enable_irq(IRQ_TIMER);
    
    uint32_t divisor = 1193180 / PIT_FREQUENCY;
    outb(0x43, 0x36);
    
    uint8_t l = (uint8_t)(divisor & 0xff);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xff);
    
    outb(0x40, l);
    outb(0x40, h);

    avenrun[0] = 0;
    avenrun[1] = 0;
    avenrun[2] = 0;
}


void switch_timer(void)
{
    cli();

    unregister_irq_handler(IRQ_TIMER, &early_timer_handler);
    register_irq_handler(IRQ_TIMER, &bsp_timer_handler);

    register_isr_handler(123, &ap_timer_handler);
    //enable_irq(123);

    sti();
}

