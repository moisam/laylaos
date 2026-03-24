/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
 * 
 *    file: mmngr_virtual.h
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
 *  \file mmngr_virtual.h
 *
 *  Functions and macros for working with the Virtual Memory Manager (VMM).
 */

#ifndef __MMNGR_VIRT_H__
#define __MMNGR_VIRT_H__

/*
 * Code adopted from BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 */

#include <stdint.h>
//#include <sys/pagesize.h>
#include <kernel/pagesize.h>
#include <mm/mmngr_phys.h>
#include <mm/vmmngr_pte.h>
#include <mm/vmmngr_pde.h>
#include <kernel/mutex.h>

#define PHYS_TO_HIMEM(p)        (((uintptr_t)p) | HIMEM_START)


#ifdef __x86_64__

/*
 * Shorthand to x86-64 page tables (using the standard 4 KiB memory page):
 *
 * Table  Entry size  Entries  Memory addressed
 * -----  ----------  -------  ----------------
 *
 * PML4   8 bytes     512      256 TiB
 * PDP    8 bytes     512      512 GiB
 * PD     8 bytes     512      1 GiB
 * PT     8 bytes     512      2 MiB
 *
 * See also: https://stackoverflow.com/questions/11246559/how-does-linux-support-more-than-512gb-of-virtual-address-range-in-x86-64
 *
 */

typedef uint64_t virtual_addr;      /**< 64-bit virtual address */

/*
 *  63         48 47      39 38      30 29      21 20        12 11          0
 *  +------------+----------+----------+----------+------------+------------+
 *  | Sign ext   |   PML4   |   PDP    | Page dir | Page table |    Page    |
 *  |            |  offset  |  offset  |  offset  |  offset    |   offset   |
 *  +------------+----------+----------+----------+------------+------------+
 *     16 bits      9 bits     9 bits     9 bits      9 bits       12 bits
 *
 *  PML4 = Page Map Level 4
 *  PDP  = Page Directory Pointer table
 *  PD   = Page Directory table
 *  PT   = Page table
 */

// x86-64 architecture defines 512 entries per table--do not change
#define PAGES_PER_TABLE                 512
#define PAGES_PER_DIR	                512

#else       /* !__x86_64__ */

typedef uint32_t virtual_addr;      /**< 32-bit virtual address */

/*
 *  31         22 21        12 11            0
 *  +------------+------------+--------------+
 *  |  Page dir  | Page table |     Page     |
 *  |   offset   |   offset   |    offset    |
 *  +------------+------------+--------------+
 *     10 bits      10 bits       12 bits
 */

// i86 architecture defines 1024 entries per table--do not change
#define PAGES_PER_TABLE                 1024
#define PAGES_PER_DIR	                1024

#endif      /* !__x86_64__ */


// defined in kernel/task.h
struct task_t;


/**
 * @struct ptable
 * @brief The ptable structure.
 *
 * A structure to represent a page table.
 */
struct ptable
{
	pt_entry m_entries[PAGES_PER_TABLE];    /**< page entries */
};

typedef struct ptable ptable;

/**
 * @struct pdirectory
 * @brief The pdirectory structure.
 *
 * A structure to represent a page directory.
 */
struct pdirectory
{
	pd_entry m_entries_phys[PAGES_PER_DIR];   /**< physical ptable entries */
};

typedef struct pdirectory pdirectory;


/*
 * Flush TLB entry.
 */
static inline void vmmngr_flush_tlb_entry(virtual_addr addr)
{
    __asm__ __volatile__("invlpg (%0)"
                         ::"r"(addr):"memory");
    tlb_shootdown(addr);
}


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief Switch page directory.
 *
 * Switch to a new page directory.
 *
 * @param   dir_phys    page directory physical address
 * @param   dir_virt    page directory virtual address
 *
 * @return  nothing.
 */
void vmmngr_switch_pdirectory(pdirectory *dir_phys, pdirectory *dir_virt);

/**
 * @brief Get current page directory.
 *
 * Get the virtual address of the current page directory.
 *
 * @return  virtual address of current page directory.
 */
pdirectory *vmmngr_get_directory_virt(void);

/**
 * @brief Get current page directory.
 *
 * Get the physical address of the current page directory.
 *
 * @return  physical address of current page directory.
 */
pdirectory *vmmngr_get_directory_phys(void);

/**
 * @brief Allocate a page in physical memory.
 *
 * Allocate a physical page and map it to the given virtual address, setting
 * the flags as passed to us (sets at least the present flag even if 
 * \a flags == 0).
 *
 * @param   e       page entry
 * @param   flags   page flags
 *
 * @return  1 on success, 0 on failure.
 */
int vmmngr_alloc_page(pt_entry *e, int flags);

/**
 * @brief Allocate pages in physical memory.
 *
 * Allocate physical memory frames and map them to the virtual addresses
 * starting from the given address. The number of alloc'd physical frames is
 * \a sz / PAGE_SIZE.
 *
 * NOTE: The caller MUST ensure \a addr is page-aligned!
 *
 * @param   addr    virtual address
 * @param   sz      requested allocation size in bytes
 * @param   flags   page flags
 *
 * @return  1 on success, 0 on failure.
 */
int vmmngr_alloc_pages(virtual_addr addr, size_t sz, int flags);

/**
 * @brief Free a page in physical memory.
 *
 * Free the physical page corresponding to the given page entry.
 *
 * @param   e       page entry
 *
 * @return  nothing.
 */
void vmmngr_free_page(pt_entry *e);

/**
 * @brief Free pages in physical memory.
 *
 * For each page from \a addr to \a addr + \a sz, find the page entry and
 * free the corresponding physical page.
 *
 * @param   addr    virtual address
 * @param   sz      size in bytes
 *
 * @return  nothing.
 */
void vmmngr_free_pages(virtual_addr addr, size_t sz);

/**
 * @brief Change page flags.
 *
 * For each page from \a addr to \a addr + \a sz, find the page entry and
 * change its flags.
 *
 * NOTE: The caller MUST ensure \a addr is page-aligned!
 *
 * @param   addr    virtual address
 * @param   sz      size in bytes
 * @param   flags   page flags
 *
 * @return  nothing.
 */
void vmmngr_change_page_flags(virtual_addr addr, size_t sz, int flags);

/**
 * @brief Map a page.
 *
 * Map the given virtual address to the given physical address, giving the
 * page the passed \a flags.
 *
 * @param   phys    physical address
 * @param   virt    virtual address
 * @param   flags   page flags
 *
 * @return  nothing.
 */
void vmmngr_map_page(physical_addr phys, virtual_addr virt, int flags);

/**
 * @brief Unmap a page.
 *
 * Unmap the given virtual address, detaching it from its physical page.
 *
 * @param   virt    virtual address
 *
 * @return  nothing.
 */
void vmmngr_unmap_page(virtual_addr virt);

/**
 * @brief Free page directory.
 *
 * Called when reaping zombie tasks to free the memory used by the zombie
 * task's page directory.
 *
 * @param   addr    virtual address
 *
 * @return  nothing.
 */
void free_pd(virtual_addr addr);

/**
 * @brief Get physical address.
 *
 * Get the physical address to which the given virtual address is mapped.
 *
 * @param   virt    virtual address
 *
 * @return  physical address.
 */
physical_addr get_phys_addr(virtual_addr virt);

/**
 * @brief Get page table count.
 *
 * Get the number of pages used to map page tables and page directories.
 *
 * @return  page table count.
 */
size_t used_pagetable_count(void);

/**
 * @brief Map an MMIO address space.
 *
 * For devices that use Memory-Mapped I/O, this function maps the device's
 * physical MMIO address space to the kernel's virtual address space.
 *
 * If the passed physical address (\a pstart) is not page-aligned, the 
 * returned address has the same offset as \a pstart (that is, \a pstart - 
 * align_down(\a pstart)).
 *
 * @param   pstart  start of physical address region
 * @param   pend    end of physical address region
 *
 * @return  virtual address.
 */
virtual_addr mmio_map(physical_addr pstart, physical_addr pend);

/**
 * @brief Map a kernel module.
 *
 * If the passed physical address (\a pstart) is not page-aligned, the 
 * returned address has the same offset as \a pstart (that is, \a pstart - 
 * align_down(\a pstart)).
 *
 * @param   pstart  start of physical address region
 * @param   pend    end of physical address region
 *
 * @return  virtual address.
 */
virtual_addr kmod_map(physical_addr pstart, physical_addr pend);

/**
 * @brief Get page entry.
 *
 * Get the page table entry representing the given virtual address.
 * Entries are searched in the given \a page_directory.
 *
 * @param   page_directory  page directory
 * @param   virt            virtual address
 *
 * @return  page table entry.
 */
pt_entry *get_page_entry_pd(pdirectory *page_directory, virtual_addr virt);

/**
 * @brief Get page entry.
 *
 * Get the page table entry representing the given virtual address.
 *
 * @param   virt    virtual address
 *
 * @return  page table entry.
 */
pt_entry *get_page_entry(virtual_addr virt);

/**
 * @brief Initialize the virtual memory manager.
 *
 * This function is called early during boot to initialize internal structs,
 * set the first page directories and page tables, start the memory manager,
 * initialize the kernel heap and the VBE driver.
 *
 * @return  nothing.
 */
void vmmngr_initialize(/* multiboot_info_t *mbd */);

/**
 * @brief Clone task page directory.
 *
 * Create a copy of the \a parent task's page directory and page tables and
 * assign the copy to the \a child task. If \a cow is not zero, all pages
 * and page tables are marked as Copy-on-Write (CoW).
 * Called during fork().
 *
 * @param   parent      parent task
 * @param   child       child task
 *
 * @return  0 on success, 1 on failure.
 */
int clone_task_pd(struct task_t *parent, struct task_t *child);

/**
 * @brief Free user pages.
 *
 * Called during execve() and when a task terminates to free userspace pages
 * and page tables.
 *
 * @param   addr    virtual address of task's page directory
 *
 * @return  nothing.
 */
void free_user_pages(virtual_addr addr);

/**
 * @brief Get task page count.
 *
 * Return the number of page mapped into the task's address space.
 *
 * @param   task    pointer to task
 *
 * @return  page count.
 */
size_t get_task_pagecount(struct task_t *task);


/***********************************
 * Helper functions.
 ***********************************/

/**
 * @var pagetable_count
 * @brief pagetable count.
 *
 * The number of mapped page tables.
 */
extern volatile size_t pagetable_count;

#ifndef __x86_64__
int page_fault_check_table(pdirectory *pd, 
                           volatile virtual_addr faulting_address);
#endif

// defined in arch/x86_64/mmngr_virtual_x86_64.c
pt_entry *__get_page_entry_pd(pdirectory *pml4, virtual_addr virt, int __flags);


#ifdef __x86_64__

/*
 * Values to the flags param to get_pde().
 */
#define FLAG_GETPDE_CREATE          1   /**< create page directory */
#define FLAG_GETPDE_USER            2   /**< userspace page directory */
#define FLAG_GETPDE_ISPD            4   /**< requested entry is a 
                                             page directory */
#define FLAG_GETPDE_ISPDP           8   /**< requested entry is a 
                                             page directory pointer */

#endif      /* !__x86_64__ */

#endif      /* __MMNGR_VIRT_H__ */
