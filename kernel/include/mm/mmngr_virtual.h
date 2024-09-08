/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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

/*
 * Types of kernel memory regions
 */
#define REGION_PAGETABLE        1       /**< pagetable memory region */
#define REGION_KSTACK           2       /**< kernel stack memory region */
#define REGION_PIPE             3       /**< pipefs memory region */
#define REGION_VBE_BACKBUF      4       /**< VBE back buffer memory region */
#define REGION_VBE_FRONTBUF     5       /**< VBE front buffer memory region */
#define REGION_KMODULE          6       /**< kernel modules memory region */
#define REGION_PCACHE           7       /**< page cache memory region */
#define REGION_DMA              8       /**< DMA memory region */
#define REGION_ACPI             9       /**< ACPI memory region */
#define REGION_MMIO             10      /**< Memory-mapped IO memory region */


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
//#include <kernel/task.h>


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
	pd_entry m_entries_virt[PAGES_PER_DIR];   /**< virtual ptable entries */
};

typedef struct pdirectory pdirectory;


/*
 * Flush TLB entry.
 */
static inline void vmmngr_flush_tlb_entry(virtual_addr addr)
{
    __asm__ __volatile__("invlpg (%0)"
                         ::"r"(addr):"memory");
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
 * @brief Initialise a page directory entry.
 *
 * Helper function called by vmmngr_initialize() and other VMM functions to
 * init pd table entries.
 *
 * @param   dir         page directory
 * @param   index       the index of the pdtable entry we want to map
 * @param   ptable      physical address of the table we want to map
 * @param   vtable      virtual address of the table
 * @param   userflag    if non-zero, map the page as user-accessible
 *
 * @return  nothing.
 */
void init_pd_entry(pdirectory *dir, int index, physical_addr ptable,
                   virtual_addr vtable, int userflag);

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
void vmmngr_map_page(void *phys, void *virt, int flags);

/**
 * @brief Unmap a page.
 *
 * Unmap the given virtual address, detaching it from its physical page.
 *
 * @param   virt    virtual address
 *
 * @return  nothing.
 */
void vmmngr_unmap_page(void *virt);

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
 * @brief Get a temporary virtual address.
 *
 * Allocate a physical page and map it to a virtual address and return the
 * mapped virtual address.
 *
 * @param   __addr      the virtual address is returned here
 * @param   tmp         the page entry is returned here
 * @param   flags       page flags
 *
 * @return  nothing.
 */
void get_tmp_virt_addr(virtual_addr *__addr, pt_entry **tmp, int flags);

/**
 * @brief Get page table count.
 *
 * Get the number of pages used to map page tables and page directories.
 *
 * @return  page table count.
 */
size_t used_pagetable_count(void);

/**
 * @brief Get virtual address.
 *
 * Convert a physical address to a virtual address. The address is chosen
 * based on the requested memory \a region.
 *
 * @param   phys    physical address
 * @param   flags   page flags
 * @param   region  kernel memory region
 *
 * @return  virtual address.
 */
virtual_addr phys_to_virt(physical_addr phys, int flags, int region);

/**
 * @brief Get virtual address with offset.
 *
 * Convert a physical address range to a virtual address range. The address
 * range is chosen based on the requested memory \a region.
 *
 * If the passed physical address (\a pstart) is not page-aligned, the 
 * returned address has the same offset as \a pstart (that is, \a pstart - 
 * align_down(\a pstart)).
 *
 * @param   pstart  start of physical address region
 * @param   pend    end of physical address region
 * @param   flags   page flags
 * @param   region  kernel memory region
 *
 * @return  virtual address.
 */
virtual_addr phys_to_virt_off(physical_addr pstart, physical_addr pend,
                              int flags, int region);

/**
 * @brief Get virtual address.
 *
 * Get the next valid address from a range of addresses. For example, when
 * allocating kernel stacks, addresses can range between USER_KSTACK_START and
 * USER_KSTACK_END. When the system starts running, allocation happens in a
 * linear way, but as the system keeps running, we'll eventually reach the 
 * point where we hit the USER_KSTACK_END address, at which point we need
 * to restart allocating from USER_KSTACK_START or somewhere in the middle.
 * This function does this. It returns the first available address within
 * the given range of addresses, allocating a physical memory page in the
 * process. We use it to get addresses for other things, like when we allocate
 * page tables, for example.
 *
 * @param   phys    the physical address of the alloc'd page frame is 
 *                    returned here
 * @param   virt    similar to the above, except the virtual address is 
 *                    returned here
 * @param   flags   the flags to set on the alloc'd physical memory frame
 * @param   region  kernel memory region
 *
 * @return  0 on success, -1 on failure.
 */
int get_next_addr(physical_addr *phys, virtual_addr *virt, 
                  int flags, int region);

/**
 * @brief Get virtual address range.
 *
 * Allocate physical memory frames and map them to continuous virtual addresses
 * in the kernel's memory space. The virtual addresses depend on the given
 * \a region, which segregates kernel memory into different sections (see 
 * the memmap.txt file for a description of these sections).
 *
 * This function works similar to get_next_addr(), except the
 * latter allocates and maps a single page in the given address region.
 *
 * @param   sz              size of desired memory to allocate in bytes
 *                            (rounded up to the nearest page)
 * @param   pcontiguous     if set, the physical pages are allocated 
 *                            contiguously, i.e. each physical frame follows 
 *                            the other (this is used to reserve page 
 *                            directories, where the directory must fall on
 *                            two sequential physical frames)
 * @param   flags           the flags to set on the alloc'd physical memory 
 *                            frame
 * @param   phys            if not NULL, the physical address of the first
 *                            mapped page is stored here (this is only useful
 *                            for contiguous allocations)
 * @param   region          kernel memory region
 *
 * @return  the first virtual address in the reserved memory range, 
 *          0 on failure.
 */
virtual_addr vmmngr_alloc_and_map(size_t sz, int pcontiguous,
                                  int flags, physical_addr *phys, 
                                  int region);

/**
 * @brief Map an MMIO address space.
 *
 * For devices that use Memory-Mapped I/O, this function maps the device's
 * physical MMIO address space to the kernel's virtual address space.
 * The mapped virtual addresses will be in the \ref REGION_MMIO region.
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
pt_entry *get_page_entry_pd(pdirectory *page_directory, void *virt);

/**
 * @brief Initialize the virtual memory manager.
 *
 * This function is called early during boot to initialize internal structs,
 * set the first page directories and page tables, start the memory manager,
 * initialize the kernel heap and the VBE driver.
 *
 * @return  nothing.
 */
void vmmngr_initialize(multiboot_info_t *mbd);

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

/*
 * Helper function.
 */
int page_fault_check_table(pdirectory *pd, 
                           volatile virtual_addr faulting_address);


#ifdef __x86_64__

#define FLAG_GETPDE_CREATE          1   /**< create page directory */
#define FLAG_GETPDE_USER            2   /**< userspace page directory */
#define FLAG_GETPDE_ISPD            4   /**< requested entry is a 
                                             page directory */
#define FLAG_GETPDE_ISPDP           8   /**< requested entry is a 
                                             page directory pointer */

/**
 * @brief Get page directory entry.
 *
 * Get the page directory at index \a pd_index, creating a new page directory
 * if needed.
 *
 * @param   pd          page directory
 * @param   pd_index    index of entry in \a pd
 * @param   flags       zero or a combination of FLAG_GETPDE_CREATE,
 *                        FLAG_GETPDE_USER, FLAG_GETPDE_ISPD and 
 *                        FLAG_GETPDE_ISPD
 *
 * @return  page directory entry.
 */
pdirectory *get_pde(pdirectory *pd, size_t pd_index, int flags);

/**
 * @var pagetable_count
 * @brief pagetable count.
 *
 * The number of mapped page tables.
 */
extern volatile size_t pagetable_count;

#endif      /* !__x86_64__ */

#endif      /* __MMNGR_VIRT_H__ */
