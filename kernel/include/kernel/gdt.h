/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: gdt.h
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
 *  \file gdt.h
 *
 *  Helper functions for working with the Global Descriptor Table (GDT).
 */

#ifndef __GDT_H__
#define __GDT_H__

#include <stdint.h>

/**
 * \def MAX_DESCRIPTORS
 *
 * Maximum GDT descriptors
 */
#define MAX_DESCRIPTORS			         32    // MAX = 8192


#ifdef __x86_64__
# define GDT_TLS_DESCRIPTOR              7
#else
# define GDT_TLS_DESCRIPTOR              6
#endif      /* __x86_64__ */


/**
 * @struct gdt_descriptor_s
 * @brief The gdt_descriptor_s structure.
 *
 * A structure to represent a GDT descriptor.
 */
struct gdt_descriptor_s
{
    uint16_t limit;     /**< bits 0-15 of segment limit */
    uint16_t base_low;  /**< bits 0-15 of base addr */
    uint8_t  base_mid;  /**< bits 16-23 of base addr */
    uint8_t access;     /**< descriptor access */
    uint8_t flags;      /**< descriptor flags */
    uint8_t base_hi;    /**< bits 24-32 of base address */
} __attribute__((packed));


/**
 * @struct gdt_descriptor64_s
 * @brief The gdt_descriptor64_s structure.
 *
 * A structure to represent a 64-bit GDT descriptor.
 */
struct gdt_descriptor64_s
{
    struct gdt_descriptor_s gdt32;  /**< descriptor bits - same as in
                                         struct gdt_descriptor_s */
    uint32_t base_very_hi;      /**< high bits of base address */
    uint32_t reserved;          /**< reserved bits */
} __attribute__((packed));


/**
 * @struct gdtr
 * @brief The GDT register.
 *
 * Set the pointers needed to use the GDTR register [which points
 * to our GDT].
 */
struct gdtr
{
    uint16_t limit;     /**< size of GDT */
    uintptr_t base;     /**< base address of GDT */
} __attribute__((packed));


/**
 * @brief Set a GDT descriptor.
 *
 * Set the values of an entry in the GDT descriptor table.
 *
 * @param   gdt_index   index in GDT array (i.e. current processor id)
 * @param   no          index of table entry
 * @param   base        descriptor base address
 * @param   limit       descriptor limit
 * @param   type        descriptor type
 *
 * @return  nothing.
 */
void gdt_add_descriptor(int gdt_index, uint32_t no, uint32_t base,
                        uint32_t limit, uint8_t type);

//void gdt_clear_descriptor(uint32_t no);

/**
 * @brief Initialise the GDT.
 *
 * Set proper values for ring 0 and ring 3 code and data descriptors, and
 * load the GDT register.
 *
 * @return  nothing.
 */
void gdt_init(void);

/**
 * @brief Copy GDT pointer to trampoline.
 *
 * Called during SMP startup to copy processor-specific GDT pointer to the
 * bootstrap code.
 *
 * @param   i           index in GDT array (i.e. current processor id)
 * @param   p           pointer in trampoline code
 *
 * @return  nothing.
 */
void gdt_copy_to_trampoline(int i, char *p);

/**
 * @brief Set GS base.
 *
 * Set GS and Kernel GS base pointers.
 *
 * @param   base        virtual address
 *
 * @return  nothing.
 */
void set_gs_base(uintptr_t base);


/**
 * @struct user_desc
 * @brief The user_desc structure.
 *
 * Used to get and set thread local storage (TLS) information.
 */
struct user_desc
{
    unsigned int    entry_number;   /**< GDT table entry index */
    unsigned long   base_addr;      /**< TLS base address */
    unsigned int    limit;          /**< TLS limit */
    unsigned int    seg_32bit:1;    /**< Segment is 32 bits */
    unsigned int    contents:2;
    unsigned int    read_exec_only:1;   /**< Segment is executable */
    unsigned int    limit_in_pages:1;   /**< Limit is in pages, not bytes */
    unsigned int    seg_not_present:1;  /**< Segment is not memory present */
    unsigned int    useable:1;
    unsigned int    empty:25;           /**< unused */
};


/**
 * @brief Handler for syscall set_thread_area().
 *
 * Set thread local storage (TLS) information.
 *
 * @param   u_info      struct containing TLS info
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_set_thread_area(struct user_desc *u_info);


/**
 * @brief Handler for syscall get_thread_area().
 *
 * Get thread local storage (TLS) information.
 *
 * @param   u_info      struct where TLS info is returned
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_get_thread_area(struct user_desc *u_info);

#endif      /* __GDT_H__ */
