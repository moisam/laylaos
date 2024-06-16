/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: vmmngr_pde.h
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
 *  \file vmmngr_pde.h
 *
 *  Functions and macros for working with Page Directory Entries (PDE).
 */

#ifndef __MMNGR_VIRT_PDE_H__
#define __MMNGR_VIRT_PDE_H__

/*
 * Code adopted from BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 */

#include <stdint.h>
#include <mm/mmngr_phys.h>  // physical_addr


// this format is defined by the i86 architecture--be careful if you modify it

#define I86_PDE_PRESENT                 0x01    /**< PDE present flag */
#define I86_PDE_WRITABLE                0x02    /**< PDE writable flag */
#define I86_PDE_USER                    0x04    /**< PDE user flag */
#define I86_PDE_PWT                     0x08    /**< PDE wriethrough flag */
#define I86_PDE_PCD                     0x10    /**< PDE not cacheable flag */
#define I86_PDE_ACCESSED                0x20    /**< PDE access flag */
#define I86_PDE_COW                     0x400   /**< PDE Copy-on-Write flag */

/**
 * \def PDE_PRESENT
 * Check if a page directory entry (PDE) is present
 */
#define PDE_PRESENT(e)          (((e) & I86_PDE_PRESENT) == I86_PDE_PRESENT)

/**
 * \def PDE_WRITABLE
 * Check if a page directory entry (PDE) is writable
 */
#define PDE_WRITABLE(e)         (((e) & I86_PDE_WRITABLE) == I86_PDE_WRITABLE)

/**
 * \def PDE_COW
 * Check if a page directory entry (PDE) is Copy-on-Write (CoW)
 */
#define PDE_COW(e)              (((e) & I86_PDE_COW) == I86_PDE_COW)

/**
 * \def PDE_FRAME
 * Get page directory entry's (PDE) physical frame
 */
#define PDE_FRAME(e)            ((e) & I86_PDE_FRAME)

/**
 * \def PDE_VIRT_FRAME
 * Get page directory entry's (PDE) virtual frame
 */
#define PDE_VIRT_FRAME(e)       ((e) & I86_PDE_VIRT_FRAME)

/**
 * \def PDE_ADD_ATTRIB
 * Add an attribute to a page directory entry (PDE)
 */
#define PDE_ADD_ATTRIB(eptr, attrib)    *(eptr) |= attrib

/**
 * \def PDE_DEL_ATTRIB
 * Remove an attribute from a page directory entry (PDE)
 */
#define PDE_DEL_ATTRIB(eptr, attrib)    *(eptr) &= ~attrib

/**
 * \def PDE_SET_FRAME
 * Set a page directory entry's (PDE) physical frame
 */
#define PDE_SET_FRAME(eptr, addr)       \
        *(eptr) = (*(eptr) & ~I86_PDE_FRAME) | addr

/**
 * \def PDE_SET_VIRT_FRAME
 * Set a page directory entry's (PDE) virtual frame
 */
#define PDE_SET_VIRT_FRAME(eptr, addr)  \
        *(eptr) = (*(eptr) & ~I86_PDE_VIRT_FRAME) | addr

/**
 * \def PDE_MAKE_COW
 * Mark a page directory entry (PDE) as Copy-on-Write (CoW)
 */
#define PDE_MAKE_COW(eptr)                  \
    PDE_ADD_ATTRIB(eptr, I86_PDE_COW);      \
    PDE_DEL_ATTRIB(eptr, I86_PDE_WRITABLE);

/**
 * \def PDE_REMOVE_COW
 * Remove the Copy-on-Write flag from a page directory entry (PDE)
 */
#define PDE_REMOVE_COW(eptr)                \
    PDE_ADD_ATTRIB(eptr, I86_PDE_WRITABLE); \
    PDE_DEL_ATTRIB(eptr, I86_PDE_COW);

/**
 * \def PDIRECTORY_FRAMES
 * How many pages per page directory
 */
#define PDIRECTORY_FRAMES               sizeof(pdirectory)/PAGE_SIZE

/**
 * \def PD_BYTES
 * How many bytes per page directory
 */
#define PD_BYTES                        (PAGE_SIZE * PDIRECTORY_FRAMES)

// a page directery entry

#ifdef __x86_64__

typedef uint64_t pd_entry;              /**< 64-bit page directory entry */

/**
 * \def I86_PDE_FRAME
 * Page Directory Entry (PDE) physical frame mask
 */
#define I86_PDE_FRAME                   0x000ffffffffff000

/**
 * \def I86_PDE_VIRT_FRAME
 * Page Directory Entry (PDE) virtual frame mask
 */
#define I86_PDE_VIRT_FRAME              0xfffffffffffff000

/**
 * \def PML4_INDEX
 * Get PML4 index for the given virtual address
 */
#define PML4_INDEX(x)                   (((x) >> 39) & 0x1ff)

/**
 * \def PDP_INDEX
 * Get PDP index for the given virtual address
 */
#define PDP_INDEX(x)                    (((x) >> 30) & 0x1ff)

/**
 * \def PD_INDEX
 * Get PD index for the given virtual address
 */
#define PD_INDEX(x)                     (((x) >> 21) & 0x1ff)

#else       /* !__x86_64__ */

typedef uint32_t pd_entry;              /**< 32-bit page directory entry */

#define I86_PDE_DIRTY                   0x40    /**< PDE dirty flag */
#define I86_PDE_4MB                     0x80    /**< PDE large page flag */
#define I86_PDE_CPU_GLOBAL              0x100   /**< PDE CPU flag */
#define I86_PDE_LV4_GLOBAL              0x200   /**< PDE Level4 flag */

/**
 * \def I86_PDE_FRAME
 * Page Directory Entry (PDE) physical frame mask
 */
#define I86_PDE_FRAME                   0xfffff000

/**
 * \def PD_INDEX
 * Get PD index for the given virtual address
 */
#define PD_INDEX(x)                     (((x) >> 22) & 0x3ff)

#endif      /* !__x86_64__ */

#endif      /* __MMNGR_VIRT_PDE_H__ */
