/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: asm.h
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
 *  \file asm.h
 *
 *  General assembler routines and macros.
 */

#ifndef __KERNEL_ASM_H__
#define __KERNEL_ASM_H__

#include <stdint.h>
#include <kernel/task.h>

/**
 * \def sti
 * set interrupts
 */
#define sti()           __asm__ __volatile__ ("sti")

/**
 * \def cli
 * clear interrupts
 */
#define cli()           __asm__ __volatile__ ("cli")

/**
 * \def hlt
 * halt the processor
 */
#define hlt()           __asm__ __volatile__ ("hlt")


#ifdef __x86_64__

/**
 * @brief Clear interrupts.
 *
 * Clear interrupts, returning the flags register value.
 *
 * @return  flags register before disabling interrupts.
 */
static inline uintptr_t int_off(void)
{
    uintptr_t flags;
    __asm__ __volatile__("pushfq\npopq %0\ncli":"=a"(flags));
    return flags;
}

/**
 * @brief Set interrupts.
 *
 * Set interrupts, restoring the flags register to the given value.
 *
 * @return  nothing.
 */
static inline void int_on(uintptr_t flags)
{
    __asm__ __volatile__("pushq %0\npopfq"::"a"(flags));
}

/**
 * @brief Get RIP register value.
 *
 * Get the value of the current instruction pointer (RIP) register.
 *
 * @return  RIP register value.
 */
extern uintptr_t get_rip(void);

/**
 * @brief Get RSP register value.
 *
 * Get the value of the current stack pointer (RSP) register.
 *
 * @return  RSP register value.
 */
extern uintptr_t get_rsp(void);

#else       /* !__x86_64__ */

/**
 * @brief Clear interrupts.
 *
 * Clear interrupts, returning the flags register value.
 *
 * @return  flags register before disabling interrupts.
 */
static inline uintptr_t int_off(void)
{
    uintptr_t flags;
    __asm__ __volatile__("pushfl\npopl %0\ncli":"=a"(flags));
    return flags;
}

/**
 * @brief Set interrupts.
 *
 * Set interrupts, restoring the flags register to the given value.
 *
 * @return  nothing.
 */
static inline void int_on(uintptr_t flags)
{
    __asm__ __volatile__("pushl %0\npopfl"::"a"(flags));
}

/**
 * @brief Get EIP register value.
 *
 * Get the value of the current instruction pointer (EIP) register.
 *
 * @return  EIP register value.
 */
extern uintptr_t get_eip(void);

/**
 * @brief Get ESP register value.
 *
 * Get the value of the current stack pointer (ESP) register.
 *
 * @return  ESP register value.
 */
extern uintptr_t get_esp(void);

#endif      /* !__x86_64__ */

#endif      /* __KERNEL_ASM_H__ */
