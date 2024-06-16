/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023 (c)
 * 
 *    file: softint.c
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
 *  \file softint.c
 *
 *  The kernel's soft interrupts implementation.
 */

//#define __DEBUG

#include <errno.h>
#include <stddef.h>
#include <string.h>
//#include <sys/sched.h>
#include <sched.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/asm.h>
#include <kernel/softint.h>
#include <kernel/syscall.h>
#include <kernel/clock.h>

#include "task_funcs.c"

#if 0

/*
struct softint_t
{
    char name[8];
    void *cookie;
    void (*handler)(int);
    int arg;
    char pending;
};
*/

struct softint_t softints[MAX_SOFTINT] = { 0, };
// /* static */ volatile int pending_bitmap = 0;
static volatile int blocked_bitmap = 0;
//static volatile int running_softint = 0;
struct task_t *softint_task = NULL;
//struct kernel_mutex_t softint_lock;


/*
 * Initialise soft interrupts.
 */
void softint_init(void)
{
    memset(softints, 0, sizeof(softints));
    //init_kernel_mutex(&softint_lock);
    
    ////softint_register("clock", SOFTINT_CLOCK, softclock);
    //////softint_register("net", SOFTINT_NET, softnet);
    //softint_register("sleep", SOFTINT_SLEEP, softsleep);
    //softint_register("itimer", SOFTINT_ITIMER, softitimer);
}


/*
 * Soft interrupts task function.
 */
void softint_task_func(void *arg)
{
    UNUSED(arg);
    
    volatile int i, j, sleep;
    
    for(;;)
    {
        KDEBUG("softint_task_func: checking\n");

        //kernel_mutex_lock(&softint_lock);
        sleep = 1;
        
        for(i = 0, j = 1; i < MAX_SOFTINT; i++, j <<= 1)
        {
            //lock_scheduler();

            if(softints[i].pending && !(blocked_bitmap & j))
            {
                lock_scheduler();

                //pending_bitmap &= ~j;
                sleep = 0;
                softints[i].pending = 0;
                //running_softint = 1;
                KDEBUG("softint_task_func: running int %d (%s), func @ 0x%lx\n", i, softints[i].name, softints[i].handler);
                //__asm__ __volatile__("xchg %%bx, %%bx"::);

                ////kernel_mutex_unlock(&softint_lock);
                unlock_scheduler();

                softints[i].handler(softints[i].arg);
                KDEBUG("softint_task_func: finished running int %d (%s)\n", i, softints[i].name);

                //lock_scheduler();

                ////kernel_mutex_lock(&softint_lock);

                //running_softint = 0;
            }

            //unlock_scheduler();
        }

        ////kernel_mutex_unlock(&softint_lock);
        
        //if(!pending_bitmap)
        if(sleep)
        {
            KDEBUG("softint_task_func: sleeping\n");
            block_task(softints, 0);
        }
    }
}


/*
 * Register a soft interrupt.
 */
void softint_register(char *name, int i, void (*handler)(int))
//int softint_register(char *name, void *cookie, void (*handler)(int))
{
    // /* volatile */ int i;
    
    if(!name || /* !cookie || */ !handler || strlen(name) >= 8)
    {
        //return -EINVAL;
        return;
    }
    

    if(i < 1 || i >= MAX_SOFTINT)
    {
        return;
    }

    
    lock_scheduler();

    //for(i = 0; i < MAX_SOFTINT; i++)
    {
        //if(softints[i].cookie == NULL)
        {
            //softints[i].cookie = cookie;
            softints[i].handler = handler;
            strcpy(softints[i].name, name);
            softints[i].pending = 0;
            softints[i].arg = 0;
            //pending_bitmap &= ~(1 << i);
            blocked_bitmap &= ~(1 << i);

            unlock_scheduler();

            //return 0;
        }
    }

    //unlock_scheduler();

    //return -ENOMEM;
}


/*
 * Schedule a soft interrupt.
 *
 * NOTE: This function MUST be called with interrupts disabled!
 */
void softint_schedule(int i, int arg)
//int softint_schedule(void *cookie, int arg)
{
    // /* volatile */ int i;
    
    if(i < 1 || i >= MAX_SOFTINT)
    {
        return;
    }

    /*
    if(!cookie)
    {
        return -EINVAL;
    }
    */

    //for(i = 0; i < MAX_SOFTINT; i++)
    {
        //if(softints[i].cookie == cookie)
        {
            softints[i].pending = 1;
            softints[i].arg = arg;
            //pending_bitmap |= (1 << i);

            //unblock_kernel_task(softint_task);


            /*
             * This is the code from unblock_kernel_task(). We inline it
             * here for performance, as this function is called repeatedly
             * from timer_callback().
             */
            if(softint_task->state == TASK_READY || 
               softint_task->state == TASK_RUNNING ||
               softint_task->state == TASK_ZOMBIE)
            {
                // task is already unblocked
                //return 0;
                return;
            }

            softint_task->state = TASK_READY;
            //softint_task->wait_channel = NULL;

            remove_from_queue(softint_task);
            //remove_from_queue(softint_task, &blocked_queue /*, TASK_NEXT_BLOCKED */);
            append_to_ready_queue(softint_task);


            //return 0;
        }
    }

    //return -EINVAL;
}


/*
 * Block a soft interrupt.
 */
void softint_block(int i)
//void *softint_block(void *cookie)
{
    // /* volatile */ int i;

    if(i < 1 || i >= MAX_SOFTINT)
    {
        return;
    }
    
    /*
    if(!cookie)
    {
        return NULL;
    }
    */

    lock_scheduler();

    //for(i = 0; i < MAX_SOFTINT; i++)
    {
        //if(softints[i].cookie == cookie)
        {
            blocked_bitmap |= (1 << i);
            //break;
        }
    }

    unlock_scheduler();

    //return cookie;
}


/*
 * Unblock a soft interrupt.
 */
void softint_unblock(int i)
//void softint_unblock(void *cookie)
{
    // /* volatile */ int i;

    if(i < 1 || i >= MAX_SOFTINT)
    {
        return;
    }
    
    /*
    if(!cookie)
    {
        return;
    }
    */
    
    lock_scheduler();

    //for(i = 0; i < MAX_SOFTINT; i++)
    {
        //if(softints[i].cookie == cookie)
        {
            blocked_bitmap &= ~(1 << i);

            //if(pending_bitmap)
            {
                unlock_scheduler();
                unblock_kernel_task(softint_task);
                lock_scheduler();
            }
            
            //break;
        }
    }

    unlock_scheduler();
}

#endif

