/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024, 2025 (c)
 * 
 *    file: nettimer.c
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
 *  \file nettimer.c
 *
 *  Network timer implementation.
 */

#include <kernel/laylaos.h>
#include <kernel/mutex.h>
#include <kernel/task.h>
#include <kernel/net/nettimer.h>
#include <mm/kheap.h>

struct nettimer_t timers_head = { 0, };
volatile struct kernel_mutex_t nettimer_lock;
volatile struct task_t *nettimer_task = NULL;

static void nettimer_func(void *arg);


/*
 * Initialize network timers.
 */
void nettimer_init(void)
{
    init_kernel_mutex(&nettimer_lock);
    (void)start_kernel_task("nettimer", nettimer_func, NULL, &nettimer_task, 0);
}


STATIC_INLINE void nettimer_free(volatile struct nettimer_t *t)
{
    kfree((void *)t);
}


STATIC_INLINE struct nettimer_t *nettimer_alloc(void)
{
    struct nettimer_t *t;

    if((t = kmalloc(sizeof(struct nettimer_t))))
    {
        A_memset(t, 0, sizeof(struct nettimer_t));
    }

    return t;
}


static void nettimer_func(void *arg)
{
    volatile struct nettimer_t *t, *prev;

    UNUSED(arg);

    while(1)
    {
        block_task2(&nettimer_task, PIT_FREQUENCY / 5);

        kernel_mutex_lock(&nettimer_lock);

        for(prev = &timers_head, t = timers_head.next; 
            t != NULL; 
            t = prev->next)
        {
            if(!t->cancelled && t->expires < ticks)
            {
                t->cancelled = 1;
                kernel_mutex_unlock(&nettimer_lock);
                t->handler(t->arg);
                kernel_mutex_lock(&nettimer_lock);
            }

            if(t->cancelled && t->refs == 0)
            {
                prev->next = t->next;
                nettimer_free(t);
            }
            else
            {
                prev = t;
            }
        }

        kernel_mutex_unlock(&nettimer_lock);
    }
}


struct nettimer_t *nettimer_add(uint32_t expire, void (*handler)(void *), void *arg)
{
    struct nettimer_t *t, *nt = nettimer_alloc();

    if(!nt)
    {
        return NULL;
    }

    nt->refs = 1;
    nt->expires = ticks + expire;
    nt->handler = handler;
    nt->arg = arg;

    kernel_mutex_lock(&nettimer_lock);

    for(t = &timers_head; t->next != NULL; t = t->next)
    {
        ;
    }

    t->next = nt;
    kernel_mutex_unlock(&nettimer_lock);

    return nt;
}


void nettimer_oneshot(uint32_t expire, void (*handler)(void *), void *arg)
{
    struct nettimer_t *t, *nt = nettimer_alloc();

    if(!nt)
    {
        return;
    }

    nt->expires = ticks + expire;
    nt->handler = handler;
    nt->arg = arg;

    kernel_mutex_lock(&nettimer_lock);

    for(t = &timers_head; t->next != NULL; t = t->next)
    {
        ;
    }

    t->next = nt;
    kernel_mutex_unlock(&nettimer_lock);
}


void nettimer_release(struct nettimer_t *t)
{
    if(!t)
    {
        return;
    }

    kernel_mutex_lock(&nettimer_lock);
    t->refs--;
    t->cancelled = 1;
    kernel_mutex_unlock(&nettimer_lock);
}

