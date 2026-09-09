/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
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

    __atomic_store_n(&mutex->sig1[0], 'I', __ATOMIC_SEQ_CST);
    __atomic_store_n(&mutex->sig1[1], 'N', __ATOMIC_SEQ_CST);
}


/*
 * Try to lock a kernel mutex.
 */
uint32_t __kernel_mutex_trylock(volatile struct kernel_mutex_t *mutex, const char *func, int line)
{
    (void)func;
    (void)line;

    if(!__atomic_exchange_n(&mutex->lock, 1, __ATOMIC_ACQUIRE))
    {
        __atomic_store_n(&mutex->holder, this_core->cur_task, __ATOMIC_SEQ_CST);

        __atomic_store_n(&mutex->sig1[0], 'L', __ATOMIC_SEQ_CST);
        __atomic_store_n(&mutex->sig1[1], 'K', __ATOMIC_SEQ_CST);

        return 0;
    }

    return 1;
}


/*
 * Lock a kernel mutex.
 */
void __kernel_mutex_lock(volatile struct kernel_mutex_t *mutex, const char *func, int line)
{
    (void)func;
    (void)line;

    volatile long tries = 0;

    if(this_core->cur_task && mutex->holder && mutex->holder == this_core->cur_task)
    {
        switch_tty(1);

        /*
        printk("\nmutex: mutex " _XPTR_", holder " _XPTR_ ", lock %d, recur %d\n", mutex, mutex->holder, mutex->lock, mutex->recursive_count);
        for(size_t z = 0; z < sizeof(struct kernel_mutex_t); z++) printk("%x ", ((char *)mutex)[z]);
        printk("\n");
        for(size_t z = 0; z < sizeof(struct kernel_mutex_t); z++) printk("%c ", ((char *)mutex)[z]);
        printk("\n");
        */
        printk("\nmutex: mutex " _XPTR_ ", holder " _XPTR_ " (pid %d - %s), this pid %d\n",
                mutex, mutex->holder, mutex->holder ? mutex->holder->pid : 0, 
                mutex->holder ? mutex->holder->command : "null",
                this_core->cur_task->pid);
        printk("mutex: func %s, line %d\n", func, line);

        kpanic("mutex: self lock\n");
    }

    while(__atomic_exchange_n(&mutex->lock, 1, __ATOMIC_ACQUIRE) && ++tries < 0x7FFFFFFF)
    {
        __asm__ __volatile__("pause":::"memory");
    }

    if(tries >= 0x7FFFFFFF)
    {
        switch_tty(1);

        /*
        pt_entry *e = get_page_entry(mutex);
        uintptr_t phys = PTE_FRAME(*e);

        printk("\nmutex: mutex " _XPTR_ " (phys " _XPTR_ "), holder " _XPTR_ ", lock %d, recur %d\n", mutex, phys, mutex->holder, mutex->lock, mutex->recursive_count);
        for(size_t z = 0; z < 44; z++) printk("%x ", ((char *)mutex)[z]);
        printk("\n");
        for(size_t z = 0; z < 44; z++) printk("%c ", ((char *)mutex)[z]);
        printk("\n");
	    pmmngr_print_memmap();
	    */
        printk("\nmutex: mutex " _XPTR_ ", holder " _XPTR_ " (pid %d - %s), this pid %d\n",
                mutex, mutex->holder, mutex->holder ? mutex->holder->pid : 0, 
                mutex->holder ? mutex->holder->command : "null",
                this_core->cur_task->pid);
        printk("mutex: func %s, line %d\n", func, line);

        kpanic("mutex: waiting forever\n");
    }

    __atomic_store_n(&mutex->holder, this_core->cur_task, __ATOMIC_SEQ_CST);

    __atomic_store_n(&mutex->sig1[0], 'L', __ATOMIC_SEQ_CST);
    __atomic_store_n(&mutex->sig1[1], 'K', __ATOMIC_SEQ_CST);
}


/*
 * Unlock a kernel mutex.
 */
void kernel_mutex_unlock(volatile struct kernel_mutex_t *mutex)
{
    __atomic_store_n(&mutex->lock, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&mutex->holder, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&mutex->recursive_count, 0, __ATOMIC_SEQ_CST);

    __atomic_store_n(&mutex->sig1[0], 'U', __ATOMIC_SEQ_CST);
    __atomic_store_n(&mutex->sig1[1], 'N', __ATOMIC_SEQ_CST);
}

