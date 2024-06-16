/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: fpu.c
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
 *  \file fpu.c
 *
 *  Functions for working with the Floating Point Unit (FPU).
 */

//#define __DEBUG

#include <kernel/laylaos.h>
#include <kernel/fpu.h>
//#include <kernel/task.h>
#include <kernel/irq.h>


#ifndef __x86_64__      /* !__x86_64__ */

// Emulation Mode bit
#define CR0_EM          (1 << 2)

// Task Switched flag
#define CR0_TS          (1 << 3)

// last task who used the fpu
volatile struct task_t *last_task_used_fpu = NULL;

struct handler_t fpu_handler =
{
    .handler = fpu_callback,
};


/*
 * Save the current FPU information in the old task struct, then get the new
 * information from the current task.
 */
void fpu_state_restore(void)
{
    //__asm__("xchg %%bx, %%bx"::);
    register struct task_t *ct = get_cur_task();
    
    if(last_task_used_fpu)
    {
        __asm__ __volatile__("fnsave %0": : "m" (last_task_used_fpu->i387));
    }
    
    if(ct->properties & PROPERTY_USED_FPU)
    {
        KDEBUG("fpu_state_restore: restoring fpu (pid %d)\n", ct->pid);

        __asm__ __volatile__("frstor %0"::"m" (ct->i387));
    }
    else
    {
        KDEBUG("fpu_state_restore: init'ing fpu (pid %d)\n", ct->pid);

        fpu_init();
        //__asm__("fninit"::);
        ct->properties |= PROPERTY_USED_FPU;
    }

    last_task_used_fpu = ct;
}


void forget_fpu(struct task_t *task)
{
    task->properties &= ~PROPERTY_USED_FPU;
    
    if(task == last_task_used_fpu)
    {
        last_task_used_fpu = NULL;
    }
}


int fpu_callback(struct regs *r, int arg)
{
    UNUSED(r);
    UNUSED(arg);
    
    unsigned int cr0 = 0;

    __asm__ __volatile__("mov %%cr0, %0" : "=r" (cr0));
    
    if(cr0 & CR0_EM)
    {
        fpu_emulate();
    }
    else
    {
        clts();
        fpu_state_restore();
    }
    
    return 0;
}


void fpu_emulate(void)
{
    kpanic("fpu: math emulation is not implemented yet!\n");
}

#endif      /* !__x86_64__ */

