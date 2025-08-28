/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: idt.h
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
 *  \file idt.h
 *
 *  Helper functions for working with the Interrupt Descriptor Table (IDT).
 */

#ifndef __IDT_H__
#define __IDT_H__

#include <stdint.h>

/**
 * \def MAX_INTERRUPTS
 *
 * Maximum interrupts
 */
#define MAX_INTERRUPTS			256


#ifdef __x86_64__

/**
 * @struct idt_descriptor_s
 * @brief The idt_descriptor_s structure.
 *
 * A structure to represent an IDT descriptor.
 */
struct idt_descriptor_s
{
    uint16_t base_low;  /**< bits 0-16 of IR [interrupt routine] address */
    uint16_t selector;  /**< code selector in GDT */
    uint8_t ist;        /**< Reserved -- Should be zero */
    uint8_t flags;      /**< flags */
    uint16_t base_hi;   /**< bits 16-32 of IR [interrupt routine] address */
    uint32_t base_very_hi;  /**< high bits of IR [interrupt routine] address */
    uint32_t reserved;  /**< Reserved -- Should be zero */
} __attribute__((packed));

#else       /* !__x86_64__ */

/**
 * @struct idt_descriptor_s
 * @brief The idt_descriptor_s structure.
 *
 * A structure to represent an IDT descriptor.
 */
struct idt_descriptor_s
{
    uint16_t base_low;  /**< bits 0-16 of IR [interrupt routine] address */
    uint16_t selector;  /**< code selector in GDT */
    uint8_t reserved;   /**< Reserved -- Should be zero */
    uint8_t flags;      /**< flags */
    uint16_t base_hi;   /**< bits 16-32 of IR [interrupt routine] address */
} __attribute__((packed));

#endif      /* !__x86_64__ */


/**
 * @struct idtr
 * @brief The IDT register.
 *
 * Set the pointers needed to use the IDTR register [which points
 * to our IDT].
 */
struct idtr
{
    uint16_t limit;     /**< size of IDT */
    uintptr_t base;     /**< base address of IDT */
} __attribute__((packed));


/************************
 * Functions
 ************************/

/**
 * @brief Initialise the IDT.
 *
 * Set interrupt service routine pointers and load the IDT register.
 *
 * @return  nothing.
 */
void idt_init(void);

/**
 * @brief Set an IDT descriptor.
 *
 * Set the values of an entry in the IDT descriptor table.
 *
 * @param   no              index of table entry
 * @param   flags           descriptor flags
 * @param   selector        code selector in GDT
 * @param   isr_function    address of ISR handler
 *
 * @return  nothing.
 */
void install_isr(uint32_t no, uint8_t flags, uint16_t selector,
                 void (*isr_function)());

/**
 * @brief Install IDT.
 *
 * Load the IDT register.
 *
 * @return  nothing.
 */
void idt_install(void);

#endif      /* __IDT_H__ */
