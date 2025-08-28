/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: tss.h
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
 *  \file tss.h
 *
 *  Helper functions for working with the Task State Segment (TSS).
 */

#ifndef __TSS_H__
#define __TSS_H__

#include <stdint.h>
#include <string.h>

/*
 * Code for x86 was obtained from OSDev WiKi -- wiki.osdev.org
 */

#ifdef __x86_64__

/**
 * @struct tss_entry_struct
 * @brief The tss_entry_struct structure.
 *
 * A structure describing a Task State Segment.
 */
struct tss_entry_struct
{
    uint32_t res0;          /**< Reserved */
    uint64_t sp0;           /**< RSP0 */
    uint64_t sp1;           /**< RSP1 */
    uint64_t sp2;           /**< RSP2 */
    uint64_t res1;          /**< Reserved */
    uint64_t ist0;          /**< Interrupt Stack Table (IST) #0 */
    uint64_t ist1;          /**< Interrupt Stack Table (IST) #1 */
    uint64_t ist2;          /**< Interrupt Stack Table (IST) #2 */
    uint64_t ist3;          /**< Interrupt Stack Table (IST) #3 */
    uint64_t ist4;          /**< Interrupt Stack Table (IST) #4 */
    uint64_t ist5;          /**< Interrupt Stack Table (IST) #5 */
    uint64_t ist6;          /**< Interrupt Stack Table (IST) #6 */
    uint64_t res2;          /**< Reserved */
    uint16_t res3;          /**< Reserved */
    uint16_t iomap_base;    /**< I/O map base address */
} __attribute__((packed));

#else

/**
 * @struct tss_entry_struct
 * @brief The tss_entry_struct structure.
 *
 * A structure describing a Task State Segment.
 */
struct tss_entry_struct
{
    uint32_t prev_tss;      /**< The previous TSS */
    uint32_t sp0;           /**< The stack pointer to load when we change to 
                                   kernel mode */
    uint32_t ss0;           /**< The stack segment to load when we change to 
                                   kernel mode */

    /* everything below here is unusued now.. */
    uint32_t sp1;           /**< ESP1 */
    uint32_t ss1;           /**< SS1 */
    uint32_t sp2;           /**< ESP2 */
    uint32_t ss2;           /**< SS2 */
    uint32_t cr3;           /**< CR3 */
    uint32_t eip;           /**< EIP */
    uint32_t eflags;        /**< EFLAGS */
    uint32_t eax;           /**< EAX */
    uint32_t ecx;           /**< ECX */
    uint32_t edx;           /**< EDX */
    uint32_t ebx;           /**< EBX */
    uint32_t esp;           /**< ESP */
    uint32_t ebp;           /**< EBP */
    uint32_t esi;           /**< ESI */
    uint32_t edi;           /**< EDI */
    uint32_t es;            /**< ES */
    uint32_t cs;            /**< CS */
    uint32_t ss;            /**< SS */
    uint32_t ds;            /**< DS */
    uint32_t fs;            /**< FS */
    uint32_t gs;            /**< GS */
    uint32_t ldt;           /**< LDTR */
    uint16_t trap;          /**< Unused */
    uint16_t iomap_base;    /**< I/O map base address */
} __attribute__((packed));

#endif      /* __x86_64__ */
 
typedef struct tss_entry_struct tss_entry_t;

extern tss_entry_t tss_entry[];


/************************
 * Function prototypes
 ************************/

/**
 * @brief Initialise and install the TSS.
 *
 * Called during boot to setup and install the TSS.
 *
 * @param   i           index of tss struct to use
 * @param   kernel_ss   kernel stack segment
 * @param   kernel_esp  kernel stack pointer
 *
 * @return  nothing.
 */
void tss_install(int i, uint32_t kernel_ss, uintptr_t kernel_esp);

/**
 * @brief Flush the TSS.
 *
 * Called during boot to flush the TSS.
 *
 * @return  nothing.
 */
void tss_flush(void);

#endif      /* __TSS_H__ */
