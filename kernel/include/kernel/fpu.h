/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: fpu.h
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
 *  \file fpu.h
 *
 *  Helper functions for working with the Floating Point Unit (FPU).
 */

#ifndef __KERNEL_FPU_H__
#define __KERNEL_FPU_H__

#include <kernel/laylaos.h>
#include <kernel/task.h>


/**
 * @brief Initialise the FPU.
 *
 * Called during boot to initialise the FPU.
 *
 * @return  nothing.
 */
extern void fpu_init(void);

/**
 * @var fpu_handler
 * @brief Pointer to the FPU IRQ handler.
 *
 * Contains a pointer to the FPU IRQ handler function.
 */
extern struct handler_t fpu_handler;


#ifdef __x86_64__

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

INLINE void fpu_state_save(volatile struct task_t *task)
{
    __asm__ __volatile__("fxsave (%0)" : : "r"(&task->fpregs));
}

INLINE void fpu_state_restore(volatile struct task_t *task)
{
    __asm__ __volatile__("fxrstor (%0)" : : "r"(&task->fpregs));
}

#else       /* !__x86_64__ */

static inline void clts(void)
{
    // clear TS so that we can use math
	__asm__ __volatile__ ("clts");
}

void fpu_state_restore(void);
int fpu_callback(struct regs *r, int args);
void fpu_emulate(void);
void forget_fpu(struct task_t *task);

#endif      /* !__x86_64__ */

#endif      /* __KERNEL_FPU_H__ */
