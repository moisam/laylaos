/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: mmngr_virtual.c
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
 *  \file mmngr_virtual.c
 *
 *  The Virtual Memory Manager (VMM) implementation.
 *
 *  The driver's code is split between these files:
 *    - mmngr_virtual.c => general VMM functions
 *    - arch/xxx/mmngr_virtual_xxx.c => arch-specific VMM functions
 *    - arch/xxx/page_fault.c => arch-specific page fault handler
 */

//#define __DEBUG

#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/irq.h>
#include <kernel/isr.h>
#include <kernel/mutex.h>
#include <kernel/vga.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/memregion.h>
#include <mm/kheap.h>
#include <mm/kstack.h>
#include <mm/mmap.h>
#include <mm/dma.h>
#include <fs/tmpfs.h>
#include <gui/vbe.h>


/*
 * Code adopted from BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 */

// current directory table
pdirectory *_cur_directory_virt = 0;
pdirectory *_cur_directory_phys = 0;

#ifdef __x86_64__
volatile size_t pagetable_count = 0;
#endif


/*
 * Switch to a new page directory.
 */
void vmmngr_switch_pdirectory(pdirectory *dir_phys, pdirectory *dir_virt)
{
    if(!dir_phys || !dir_virt)
    {
        return;
    }
    
    _cur_directory_phys = dir_phys;
    _cur_directory_virt = dir_virt;
    pmmngr_load_PDBR((physical_addr)&dir_phys->m_entries_phys);

    return;
}


/*
 * Get current page directory.
 */
pdirectory *vmmngr_get_directory_virt(void)
{
    return _cur_directory_virt;
}


pdirectory *vmmngr_get_directory_phys(void)
{
    return _cur_directory_phys;
}


/*
 * Allocate a physical page and map it to the given virtual address, setting
 * the flags as passed to us (sets at least the present flag even if 
 * flags == 0).
 *
 * Returns 1 on success, 0 on failure.
 */
int vmmngr_alloc_page(pt_entry *e, int flags)
{
    if(!e)
    {
        return 0;
    }
    
    // allocate a free physical frame
    void *p = pmmngr_alloc_block();
    
    if(!p)
    {
        return 0;
    }

    // map it to the page
    flags |= I86_PTE_PRESENT;
    *e = 0;
    PTE_SET_FRAME(e, (uintptr_t)p);
    PTE_ADD_ATTRIB(e, flags);

    return 1;
}


/*
 * Allocate physical memory frames and map them to the virtual addresses
 * starting from the given address. The number of alloc'd physical frames is
 * sz/PAGE_SIZE.
 *
 * NOTE: The caller MUST ensure addr is page-aligned!
 *
 * Returns 1 on success, 0 on failure.
 */
int vmmngr_alloc_pages(virtual_addr addr, size_t sz, int flags)
{
    virtual_addr laddr = addr + sz;
    virtual_addr i = addr;

    void *p;
    pt_entry *page;

	if(pmmngr_get_free_block_count() <= (sz / PAGE_SIZE))
	{
		return 0;	//out of memory
	}

    flags |= I86_PTE_PRESENT;
    
    while(i < laddr)
    {
        if((page = get_page_entry((void *)i)))
        {
            if(!(p = pmmngr_alloc_block()))
            {
                printk("vmm: failed to alloc page at 0x%x\n", i);

                // rollback everything
                i -= PAGE_SIZE;

                while(i >= addr)
                {
                    vmmngr_free_page(get_page_entry((void *)i));
                    vmmngr_flush_tlb_entry(i);
                    i -= PAGE_SIZE;
                }

                return 0;
            }

            *page = 0;
            PTE_SET_FRAME(page, (uintptr_t)p);
            PTE_ADD_ATTRIB(page, flags);

            vmmngr_flush_tlb_entry(i);
        }

        i += PAGE_SIZE;
    }
    
    return 1;
}


/*
 * Free a page in physical memory.
 */
void vmmngr_free_page(pt_entry *e)
{
    if(!e)
    {
        return;
    }

    void *p = (void *)PTE_FRAME(*e);

    if(p)
    {
        pmmngr_free_block(p);
    }

    *e = 0;
}


/*
 * Free pages in physical memory.
 */
void vmmngr_free_pages(virtual_addr addr, size_t sz)
{
    virtual_addr laddr = addr + sz;
    virtual_addr i = addr;

    pt_entry *e;
    void *p;
    
    while(i < laddr)
    {
        if((e = get_page_entry((void *)i)))
        {
            if((p = (void *)PTE_FRAME(*e)))
            {
                pmmngr_free_block(p);
            }

            *e = 0;
        }

        vmmngr_flush_tlb_entry(i);
        i += PAGE_SIZE;
    }
}


/*
 * Change page flags.
 */
void vmmngr_change_page_flags(virtual_addr addr, size_t sz, int flags)
{
    virtual_addr laddr = addr + sz;
    virtual_addr i = addr;
    
    while(i < laddr)
    {
        pt_entry *page = get_page_entry((void *)i);
  
        if(page && PTE_PRESENT(*page))
        {
            PTE_CLEAR_ATTRIBS(page);
            PTE_ADD_ATTRIB(page, flags);
            vmmngr_flush_tlb_entry(i);
        }

        i += PAGE_SIZE;
    }
}


/*
 * Helper function called by vmmngr_initialize() and other VMM functions to
 * init pd table entries.
 *
 * Inputs:
 *   index => the index of the pdtable entry we want to map
 *   ptable => physical address of the table we want to map
 *   vtable => virtual address of the table
 */
void init_pd_entry(pdirectory *dir, int index,
                   physical_addr table, virtual_addr vtable, int userflag)
{
   if(!dir)
   {
      return;
   }
   
   pd_entry *entry = &dir->m_entries_phys[index];
   *entry = 0;
   PDE_ADD_ATTRIB(entry, I86_PDE_PRESENT | I86_PDE_WRITABLE | userflag);
   PDE_SET_FRAME(entry, table);

   entry = &dir->m_entries_virt[index];
   *entry = 0;
   PDE_ADD_ATTRIB(entry, I86_PDE_PRESENT | I86_PDE_WRITABLE | userflag);
   PDE_SET_VIRT_FRAME(entry, vtable);
}


/*
 * Map a page.
 */
void vmmngr_map_page(void *phys, void *virt, int flags)
{
    pt_entry *page = get_page_entry(virt);

    if(!page)
    {
        return;
    }

    // map it in (Can also do (*page |= 3 to enable..)
    *page = 0;
    PTE_SET_FRAME(page, (uintptr_t)phys);
    PTE_ADD_ATTRIB(page, flags);
}


/*
 * Unmap a page.
 */
void vmmngr_unmap_page(void *virt)
{
    pt_entry *pt = get_page_entry(virt);
    
    if(pt)
    {
        *pt = 0;
        vmmngr_flush_tlb_entry((virtual_addr)virt);
    }
}


/*
 * Free page directory.
 */
void free_pd(virtual_addr src_addr)
{
    size_t i;
    volatile virtual_addr addr = src_addr;
    
    for(i = 0; i < PDIRECTORY_FRAMES; i++, addr += PAGE_SIZE)
    {
        vmmngr_free_page(get_page_entry((void *)addr));
        vmmngr_flush_tlb_entry(addr);
        __atomic_fetch_sub(&pagetable_count, 1, __ATOMIC_SEQ_CST);
    }
}


/*
 * Get physical address.
 */
physical_addr get_phys_addr(virtual_addr virt)
{
    pt_entry *pt = get_page_entry((void *)virt);
    
    if(!pt)
    {
        return 0;
    }
    
    return PTE_FRAME(*pt);
}


/*
 * Get a temporary virtual address.
 */
void get_tmp_virt_addr(virtual_addr *__addr, pt_entry **tmp, int flags)
{
    virtual_addr addr, end = TMPFS_END;
    
    *__addr = 0;
    *tmp = NULL;
    
    kernel_mutex_lock(&tmpfs_lock);
    
    for(addr = TMPFS_START; addr < end; addr += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)addr);

        if(PTE_FRAME(*pt) == 0)
        {
            PTE_SET_FRAME(pt, 1);
            PTE_ADD_ATTRIB(pt, flags);
            *__addr = addr;
            *tmp = pt;
            kernel_mutex_unlock(&tmpfs_lock);
            return;
        }
    }

    kernel_mutex_unlock(&tmpfs_lock);
}


/*
 * Get page table count.
 */
size_t used_pagetable_count(void)
{

#ifdef __x86_64__

    //printk("%s: pagetable_count %ld\n", __func__, pagetable_count);
    return (size_t)pagetable_count;

#else

    size_t i, count = 0;
    
    for(i = PAGE_TABLE_START; i < PAGE_TABLE_END; i += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)i);

        if(pt && PTE_FRAME(*pt) != 0)
        {
            count++;
        }
    }

    //printk("%s: count %lx\n", __func__, count);
    
    return count;

#endif

}


// last address we used
volatile virtual_addr last_table_addr = PAGE_TABLE_START;
volatile virtual_addr last_pipe_addr = PIPE_MEMORY_START;
volatile virtual_addr last_kstack_addr = USER_KSTACK_START;
volatile virtual_addr last_kmod_addr = KMODULE_START;
volatile virtual_addr last_pcache_addr = PCACHE_MEM_START;
volatile virtual_addr last_dma_addr = DMA_BUF_MEM_START;
volatile virtual_addr last_acpi_addr = ACPI_MEMORY_START;
volatile virtual_addr last_mmio_addr = MMIO_START;

// mutex to avoid clashes between tasks wanting to allocate page tables,
// pipes, ...
struct kernel_mutex_t table_mutex = { 0, };
struct kernel_mutex_t pipefs_mutex = { 0, };
struct kernel_mutex_t kstack_mutex = { 0, };
struct kernel_mutex_t kmod_mem_mutex = { 0, };
struct kernel_mutex_t pcache_mutex = { 0, };
struct kernel_mutex_t dma_mutex = { 0, };
struct kernel_mutex_t acpi_mutex = { 0, };
struct kernel_mutex_t mmio_mutex = { 0, };

// the following is included for consistency, we don't actually need them
volatile virtual_addr last_vbe_backbuf_addr = VBE_BACKBUF_START;
volatile virtual_addr last_vbe_frontbuf_addr = VBE_FRONTBUF_START;
struct kernel_mutex_t vbe_backbuf_mutex = { 0, };
struct kernel_mutex_t vbe_frontbuf_mutex = { 0, };


struct kernel_region_t kernel_regions[] =
{
    { REGION_PAGETABLE, PAGE_TABLE_START, PAGE_TABLE_END, &last_table_addr, &table_mutex },
    { REGION_KSTACK, USER_KSTACK_START, USER_KSTACK_END, &last_kstack_addr, &kstack_mutex },
    { REGION_KMODULE, KMODULE_START, KMODULE_END, &last_kmod_addr, &kmod_mem_mutex },
    { REGION_VBE_BACKBUF, VBE_BACKBUF_START, VBE_BACKBUF_END, &last_vbe_backbuf_addr, &vbe_backbuf_mutex },
    { REGION_VBE_FRONTBUF, VBE_FRONTBUF_START, VBE_FRONTBUF_END, &last_vbe_frontbuf_addr, &vbe_frontbuf_mutex },
    { REGION_PIPE, PIPE_MEMORY_START, PIPE_MEMORY_END, &last_pipe_addr, &pipefs_mutex },
    { REGION_PCACHE, PCACHE_MEM_START, PCACHE_MEM_END, &last_pcache_addr, &pcache_mutex },
    { REGION_DMA, DMA_BUF_MEM_START, DMA_BUF_MEM_END, &last_dma_addr, &dma_mutex },
    { REGION_ACPI, ACPI_MEMORY_START, ACPI_MEMORY_END, &last_acpi_addr, &acpi_mutex },
    { REGION_MMIO, MMIO_START, MMIO_END, &last_mmio_addr, &mmio_mutex },
    { 0, 0, 0, 0, 0 },
};


/*
 * Convert a physical address to a virtual address. We choose a virtual
 * address in the range addr_min <= virt < addr_max.
 */
virtual_addr phys_to_virt(physical_addr phys, int flags, int region)
{
    virtual_addr addr_min, addr_max, res, end;
    volatile virtual_addr *last_addr;
    struct kernel_mutex_t *mutex;
    
    get_region_bounds(&addr_min, &addr_max, &last_addr, 
                      &mutex, region, __func__);

    kernel_mutex_lock(mutex);

    end = addr_max;

    if(*last_addr >= addr_max)
    {
        *last_addr = addr_min;
    }
    
    res = *last_addr;

retry:

    for( ; res < end; res += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)res);

        if(PTE_FRAME(*pt) == 0)
        {
            PTE_SET_FRAME(pt, phys);
            PTE_ADD_ATTRIB(pt, flags);
            *last_addr = res + PAGE_SIZE;
            kernel_mutex_unlock(mutex);
            vmmngr_flush_tlb_entry(res);
            return res;
        }
    }

    // if we started searching from the middle, try to search from the 
    // beginning, maybe we'll find a page that someone has free'd
    if((end == addr_max) && (*last_addr != addr_min))
    {
        end = *last_addr;
        res = addr_min;
        goto retry;
    }
    
    kernel_mutex_unlock(mutex);
    
    // nothing found
    return -1;
}


/*
 * Convert a physical address range to a virtual address range. We choose
 * a virtual address in the range addr_min <= virt < addr_max. If the passed
 * physical address (pstart) is not page-aligned, the returned address has the
 * same offset as pstart (that is, pstart - align_down(pstart)).
 */
virtual_addr phys_to_virt_off(physical_addr pstart, physical_addr pend,
                              int flags, int region)
{
    virtual_addr addr_min, addr_max;
    volatile virtual_addr *last_addr;
    struct kernel_mutex_t *mutex;
    volatile virtual_addr a, addr = 0;
    size_t sz = align_up(pend - pstart);
    size_t i, j, pages = sz / PAGE_SIZE;

    get_region_bounds(&addr_min, &addr_max, &last_addr, 
                      &mutex, region, __func__);
    kernel_mutex_lock(mutex);
    
    for(i = addr_min, j = 0; i < addr_max; i += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)i);

        if(PTE_FRAME(*pt) == 0)
        {
            // we've got our pages
            if(++j == pages)
            {
                addr = i - ((pages - 1) * PAGE_SIZE);
                break;
            }
        }
        else
        {
            // reset our counter
            j = 0;
        }
    }

    if(j != pages)
    {
        kernel_mutex_unlock(mutex);

        return 0;
    }

    for(i = 0, a = addr; i < pages; i++, pstart += PAGE_SIZE, a += PAGE_SIZE)
    {
        vmmngr_map_page((void *)pstart, (void *)a, flags);
        vmmngr_flush_tlb_entry(a);
    }

    kernel_mutex_unlock(mutex);

    return addr + (pstart - align_down(pstart));
}


/*
 * Allocate physical memory frames and map them to continuous virtual addresses
 * in the kernel's memory space. The virtual addresses fall between addr_min
 * and addr_max, which segregates kernel memory into different sections (see 
 * the memmap.txt file for a description of these sections).
 * This function works similar to get_next_addr(), except the
 * latter allocates and maps a single page in the given address region.
 *
 * Inputs:
 *     sz => size of desired memory to allocate in bytes (rounded up to the
 *           nearest page)
 *     pcontiguous => if set, the physical pages are allocated contiguously,
 *                    i.e. each physical frame follows the other (this is used
 *                    to reserve page directories, where the directory must 
 *                    fall on two sequential physical frames)
 *     flags => the flags to set on the alloc'd physical memory frame
 *
 * Outputs:
 *     phys => if not NULL, the physical address of the first mapped page is
 *             stored here (this is only useful for contiguous allocations)
 *
 * Returns the first virtual address in the reserved memory range, 
 *         0 on failure.
 */
virtual_addr vmmngr_alloc_and_map(size_t sz, int pcontiguous,
                                  int flags, physical_addr *__phys, 
                                  int region)
{
    virtual_addr addr_min, addr_max;
    struct kernel_mutex_t *mutex = NULL;
    volatile virtual_addr *last_addr = NULL;


    size_t i, j, pages = sz / PAGE_SIZE;
    virtual_addr addr = 0;
    physical_addr phys = 0;
    
    if(sz % PAGE_SIZE)
    {
        pages++;
    }

    // for safety
    if(__phys)
    {
        *__phys = 0;
    }

    if(pcontiguous && !(phys = (physical_addr)pmmngr_alloc_blocks(pages)))
    {
        return 0;
    }


    get_region_bounds(&addr_min, &addr_max, &last_addr, 
                      &mutex, region, __func__);

    kernel_mutex_lock(mutex);
    
    // try and get consecutive virtual address pages
    for(i = addr_min, j = 0; i < addr_max; i += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)i);

        // we've got an unused address
        if(PTE_FRAME(*pt) == 0)
        {
            // we've got our pages
            if(++j == pages)
            {
                addr = i - ((pages - 1) * PAGE_SIZE);
                break;
            }
        }
        else
        {
            // reset our counter
            j = 0;
        }
    }

    if(j != pages)
    {
        if(pcontiguous)
        {
            pmmngr_free_blocks((void *)phys, pages);
        }

        kernel_mutex_unlock(mutex);
        
        return 0;
    }
    
    if(pcontiguous)
    {
        size_t v = (size_t)addr, p = (size_t)phys;
        
        for(i = 0; i < pages; i++, p += PAGE_SIZE, v += PAGE_SIZE)
        {
            vmmngr_map_page((void *)p, (void *)v, flags);
            vmmngr_flush_tlb_entry(v);
        }
    }
    else
    {
        if(!vmmngr_alloc_pages(addr, sz, flags))
        {
            addr = 0;
        }
    }
    
    if(__phys)
    {
        *__phys = phys;
    }


    if(addr)
    {
        *last_addr = addr + sz;
    }

    kernel_mutex_unlock(mutex);

    if(region == REGION_PAGETABLE)
    {
        __atomic_fetch_add(&pagetable_count, pages, __ATOMIC_SEQ_CST);
    }
    
    return addr;
}


/*
 * Map an MMIO address space.
 */
virtual_addr mmio_map(physical_addr pstart, physical_addr pend)
{

//#define flags   (PTE_FLAGS_PW | I86_PTE_WRITETHOUGH | I86_PTE_NOT_CACHEABLE)
#define flags   (PTE_FLAGS_PW | I86_PTE_NOT_CACHEABLE)

    physical_addr aligned_pstart = align_down(pstart);

    pmmngr_deinit_region(aligned_pstart, align_up(pend) - aligned_pstart);

    return phys_to_virt_off(pstart, pend, flags, REGION_MMIO);

#undef flags

}

