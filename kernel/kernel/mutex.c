/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mutex.c
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
 *  \file mutex.c
 *
 *  Kernel mutex implementation.
 */


//#define __DEBUG

#include <kernel/mutex.h>
#include <kernel/task.h>
#include <kernel/tty.h>
#include <kernel/asm.h>
#include <gui/vbe.h>

#include "task_funcs.c"


/*
 * Initialise a kernel mutex.
 */
void init_kernel_mutex(volatile struct kernel_mutex_t *mutex)
{
    mutex->lock = 0;
    mutex->recursive_count = 0;
    mutex->holder = 0;
}


/*
 * Try to lock a kernel mutex.
 */
uint32_t __kernel_mutex_trylock(volatile struct kernel_mutex_t *mutex, const char *func, int line)
{
    //if(__sync_lock_test_and_set(&mutex->lock, 1) == 0)
    if(__sync_bool_compare_and_swap(&mutex->lock, 0, 1))
    {
        if(this_core->cur_task)
        {
            //this_core->cur_task->lock_held = mutex;
            __lock_xchg_ptr(&this_core->cur_task->lock_held, (uintptr_t)mutex);
        }

        __lock_xchg_ptr(&mutex->holder, (uintptr_t)this_core->cur_task);
        __lock_xchg_ptr(&mutex->from_func, (uintptr_t)func);
        __lock_xchg_int(&mutex->from_line, line);
        /*
        mutex->holder = this_core->cur_task;
        mutex->from_func = func;
        mutex->from_line = line;
        */
        __asm__ __volatile__("":::"memory");

        return 0;
    }

    __asm__ __volatile__("":::"memory");
    return 1;
}


/*
 * Lock a kernel mutex.
 */
void __kernel_mutex_lock(volatile struct kernel_mutex_t *mutex, const char *func, int line)
{
    volatile long tries = 0;

    //while(__sync_lock_test_and_set(&mutex->lock, 1))
    while(!__sync_bool_compare_and_swap(&mutex->lock, 0, 1))
    {
        // check if a task with higher priority is holding the lock
        if(this_core->cur_task)
        {
            //struct task_t *holder = get_task_by_id(mutex->holder);
            volatile struct task_t *holder = mutex->holder;

            //this_core->cur_task->lock_waited = mutex;

            if(holder)
            {
                if(this_core->cur_task == holder)
                {
                    switch_tty(1);
                    printk("mutex: infinite wait on lock 0x%lx (pid %d (%s), prio 0x%x, policy 0x%x, user %d)\n", mutex, this_core->cur_task->pid, this_core->cur_task->command, this_core->cur_task->priority, this_core->cur_task->sched_policy, this_core->cur_task->user);

                    printk("mutex: called from %s():%d\n", func, line);
                    printk("mutex: holder called from %s():%d\n", mutex->from_func, mutex->from_line);
                    screen_refresh(NULL);
                    kpanic("mutex: task locked itself -- waiting forever\n");
                }

                /*
                if(this_core->cur_task->sched_policy != holder->sched_policy)
                {
                    switch_tty(1);
                    printk("mutex: inverted policy -- cpu %d, lock 0x%lx ", this_core->cpuid, mutex);
                    printk("(pid %d (%s), ", this_core->cur_task->pid, this_core->cur_task->command);
                    printk("prio 0x%x, ", this_core->cur_task->priority);
                    printk("policy 0x%x, ", this_core->cur_task->sched_policy);
                    printk("state %d, ", this_core->cur_task->state);
                    printk("user %d)\n", this_core->cur_task->user);

                    printk("(cpu %d, pid %d (%s), ", holder->cpuid, holder->pid, holder->command);
                    printk("prio 0x%x, ", holder->priority);
                    printk("policy 0x%x, ", holder->sched_policy);
                    printk("state %d, ", holder->state);
                    printk("user %d, ", holder->user);
                    printk("lock 0x%lx)\n", holder->lock_held);

                    printk("mutex: called from %s():%d\n", func, line);
                    printk("mutex: holder called from %s():%d\n", mutex->from_func, mutex->from_line);
                    screen_refresh(NULL);
                    kpanic("mutex: inverted priority deadlock\n");
                }

                // check for potential deadlock
                if(this_core->cur_task->lock_held &&
                   holder->lock_waited == this_core->cur_task->lock_held)
                {
                    switch_tty(1);
                    printk("mutex: cur_task: lock 0x%lx, wanted 0x%lx\n", 
                                    this_core->cur_task->lock_held, 
                                    this_core->cur_task->lock_waited);
                    printk("mutex: holder  : lock 0x%lx, wanted 0x%lx\n", 
                                    holder->lock_held, holder->lock_waited);
                    screen_refresh(NULL);
                    kpanic("mutex: deadlock detected\n");
                }
                */
            }

            if(++tries >= 50000000 /* && cur_task->user */)
            {
                switch_tty(1);
                printk("mutex: infinite wait on lock 0x%lx (pid %d (%s), prio 0x%x, policy 0x%x, user %d)\n", mutex, this_core->cur_task->pid, this_core->cur_task->command, this_core->cur_task->priority, this_core->cur_task->sched_policy, this_core->cur_task->user);

                if(holder)
                {
                    printk("mutex: lock holder pid %d (%s), ", holder->pid, holder->command);
                    printk("cpuid 0x%x, ", holder->cpuid);
                    printk("prio 0x%x, ", holder->priority);
                    printk("policy 0x%x, ", holder->sched_policy);
                    printk("user %d, ", holder->user);
                    printk("state 0x%x, ", holder->state);
                    printk("lock 0x%lx\n", holder->lock_held);
                }

                printk("mutex: called from %s():%d\n", func, line);
                printk("mutex: holder called from %s():%d\n", mutex->from_func, mutex->from_line);
                screen_refresh(NULL);
                kpanic("mutex: waiting forever\n");
            }
        }

        scheduler();
    }

    if(this_core->cur_task)
    {
        /*
        if((this_core->cur_task->pid == 1 && mutex >= 0xffff8080002a0000) ||
           (this_core->cur_task->pid == 33 && mutex >= 0xffff808000440000))
        {
            __asm__ __volatile__("xchg %%bx, %%bx"::);
        }
        */

        //this_core->cur_task->lock_waited = NULL;
        __lock_xchg_ptr(&this_core->cur_task->lock_held, (uintptr_t)mutex);
        //this_core->cur_task->lock_held = mutex;
    }

    __lock_xchg_ptr(&mutex->holder, (uintptr_t)this_core->cur_task);
    __lock_xchg_ptr(&mutex->from_func, (uintptr_t)func);
    __lock_xchg_int(&mutex->from_line, line);
    /*
    mutex->holder = this_core->cur_task;
    mutex->from_func = func;
    mutex->from_line = line;
    */
    __asm__ __volatile__("":::"memory");
}


/*
 * Unlock a kernel mutex.
 */
void kernel_mutex_unlock(volatile struct kernel_mutex_t *mutex)
{
    uintptr_t s = int_off();

    __lock_xchg_ptr(&mutex->holder, 0);
    __lock_xchg_ptr(&mutex->from_func, 0);
    __lock_xchg_int(&mutex->from_line, 0);
    __lock_xchg_int(&mutex->recursive_count, 0);
    /*
    mutex->holder = 0;
    mutex->from_func = NULL;
    mutex->from_line = 0;
    mutex->recursive_count = 0;
    */

    if(this_core->cur_task)
    {
        //this_core->cur_task->lock_held = NULL;
        __lock_xchg_ptr(&this_core->cur_task->lock_held, 0);
    }

    //__sync_lock_release(&mutex->lock);
    __sync_bool_compare_and_swap(&mutex->lock, 1, 0);
    __asm__ __volatile__("":::"memory");
    int_on(s);
}

