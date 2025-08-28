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


/**
 * \def rdtsc
 * read the timestamp counter
 */
static inline unsigned long long rdtsc(void)
{
    unsigned int low, high;
    unsigned long long val = 0;

    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));

    val |= high;
    val <<= 32;
    val |= low;

    return val;
}


static inline void __lock_xchg_byte(volatile void *p, uint8_t v)
{
    uint8_t res;
    uint8_t *p2 = (uint8_t *)p;
    __asm__ __volatile__("lock xchgb %0, %1":"+m"(*p2),"=a"(res):"1"(v):"cc","memory");
}


static inline void __lock_xchg_word(volatile void *p, uint16_t v)
{
    uint16_t res;
    uint16_t *p2 = (uint16_t *)p;
    __asm__ __volatile__("lock xchgw %0, %1":"+m"(*p2),"=a"(res):"1"(v):"cc","memory");
}


static inline void __lock_xchg_int(volatile void *p, unsigned int v)
{
    unsigned int res;
    unsigned int *p2 = (unsigned int *)p;
    __asm__ __volatile__("lock xchgl %0, %1":"+m"(*p2),"=a"(res):"1"(v):"cc","memory");
}


static inline void __lock_xchg_ptr(volatile void *p, uintptr_t v)
{
    void *res;
    uintptr_t *p2 = (uintptr_t *)p;
    __asm__ __volatile__("lock xchgq %0, %1":"+m"(*p2),"=a"(res):"1"(v):"cc","memory");
}


static inline unsigned int __lock_xchg_int_res(volatile void *p, unsigned int v)
{
    unsigned int res;
    unsigned int *p2 = (unsigned int *)p;
    __asm__ __volatile__("lock xchgl %0, %1":"+m"(*p2),"=a"(res):"1"(v):"cc","memory");
    return res;
}


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

/**
 * @brief Query cpuid.
 *
 * Find out whether the cpuid instruction is supported by the current cpu.
 *
 * @return  non-zero if cpuid is supported, zero otherwise.
 */
extern int has_cpuid(void);

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
