/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: vmmngr_pte.h
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
 *  \file vmmngr_pte.h
 *
 *  Functions and macros for working with Page Table Entries (PTE).
 */

#ifndef __MMNGR_VIRT_PTE_H__
#define __MMNGR_VIRT_PTE_H__

/*
 * Code adopted from BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 */

#include <mm/mmngr_phys.h>  // physical_addr


// i86 architecture defines this format so be careful if you modify it
#define I86_PTE_PRESENT			        0x01    /**< PTE present flag */
#define I86_PTE_WRITABLE		        0x02    /**< PTE writable flag */
#define I86_PTE_USER			        0x04    /**< PTE user flag */
#define I86_PTE_WRITETHOUGH		        0x08    /**< PTE writethrough flag */
#define I86_PTE_NOT_CACHEABLE	        0x10    /**< PTE not cachable flag */
#define I86_PTE_ACCESSED		        0x20    /**< PTE access flag */
#define I86_PTE_DIRTY			        0x40    /**< PTE dirty flag */
#define I86_PTE_PAT				        0x80    /**< PTE PAT flag */
#define I86_PTE_CPU_GLOBAL		        0x100   /**< PTE CPU flag */
#define I86_PTE_LV4_GLOBAL		        0x200   /**< PTE Level4 flag */
/* LaylaOS extension */
#define I86_PTE_COW                     0x400   /**< PTE Copy-on-Write flag */
#define I86_PTE_PRIVATE                 0x800   /**< PTE private page flag */

/**
 * \def PTE_FLAGS_PWU
 * Page table entry (PTE) present, writable and user flags
 */
#define PTE_FLAGS_PWU           \
        (I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER)

/**
 * \def PTE_FLAGS_PU
 * Page table entry (PTE) present and user flags
 */
#define PTE_FLAGS_PU            (I86_PTE_PRESENT | I86_PTE_USER)

/**
 * \def PTE_FLAGS_PW
 * Page table entry (PTE) present and writable flags
 */
#define PTE_FLAGS_PW            (I86_PTE_PRESENT | I86_PTE_WRITABLE)

/**
 * \def PTE_PRESENT
 * Check if a page table entry (PTE) is present
 */
#define PTE_PRESENT(e)          (((e) & I86_PTE_PRESENT) == I86_PTE_PRESENT)

/**
 * \def PTE_WRITABLE
 * Check if a page table entry (PTE) is writable
 */
#define PTE_WRITABLE(e)         (((e) & I86_PTE_WRITABLE) == I86_PTE_WRITABLE)

/**
 * \def PTE_DIRTY
 * Check if a page table entry (PTE) is dirty
 */
#define PTE_DIRTY(e)            (((e) & I86_PTE_DIRTY) == I86_PTE_DIRTY)

/**
 * \def PTE_PRIVATE
 * Check if a page table entry (PTE) is marked private
 */
#define PTE_PRIVATE(e)          (((e) & I86_PTE_PRIVATE) == I86_PTE_PRIVATE)

/**
 * \def PTE_FRAME
 * Get page table entry's (PTE) physical frame
 */
#define PTE_FRAME(e)            ((e) & I86_PTE_FRAME)

/**
 * \def PTE_ADD_ATTRIB
 * Add an attribute to a page table entry (PTE)
 */
#define PTE_ADD_ATTRIB(eptr, attrib)    *eptr |= attrib

/**
 * \def PTE_DEL_ATTRIB
 * Remove an attribute from a page table entry (PTE)
 */
#define PTE_DEL_ATTRIB(eptr, attrib)    *eptr &= ~attrib

/**
 * \def PTE_CLEAR_ATTRIBS
 * Clear the attributes of a page table entry (PTE)
 */
#define PTE_CLEAR_ATTRIBS(eptr)         *eptr &= I86_PTE_FRAME

/**
 * \def PTE_SET_FRAME
 * Set a page table entry's (PTE) physical frame
 */
#define PTE_SET_FRAME(eptr, addr)       *eptr = (*eptr & ~I86_PTE_FRAME) | addr

/**
 * \def PTE_MAKE_COW
 * Mark a page table entry (PTE) as Copy-on-Write (CoW)
 */
#define PTE_MAKE_COW(eptr)                  \
    PTE_ADD_ATTRIB(eptr, I86_PTE_COW);      \
    PTE_DEL_ATTRIB(eptr, I86_PTE_WRITABLE);

/**
 * \def PTE_REMOVE_COW
 * Remove the Copy-on-Write flag from a page table entry (PTE)
 */
#define PTE_REMOVE_COW(eptr)                \
    PTE_ADD_ATTRIB(eptr, I86_PTE_WRITABLE); \
    PTE_DEL_ATTRIB(eptr, I86_PTE_COW);


#ifdef __x86_64__

typedef uint64_t pt_entry;              /**< 64-bit page table entry */

/**
 * \def I86_PTE_FRAME
 * Page Table Entry (PTE) physical frame mask
 */
#define I86_PTE_FRAME                   0x000ffffffffff000

/**
 * \def PT_INDEX
 * Get PT index for the given virtual address
 */
#define PT_INDEX(x)                     (((x) >> 12) & 0x1ff)

#else       /* !__x86_64__ */

typedef uint32_t pt_entry;              /**< 32-bit page table entry */

/**
 * \def I86_PTE_FRAME
 * Page Table Entry (PTE) physical frame mask
 */
#define I86_PTE_FRAME                   0xfffff000

/**
 * \def PT_INDEX
 * Get PT index for the given virtual address
 */
#define PT_INDEX(x)                     (((x) >> 12) & 0x3ff)

#endif      /* !__x86_64__ */

#endif      /* __MMNGR_VIRT_PTE_H__ */
