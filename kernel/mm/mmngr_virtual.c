/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
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
#include <kernel/smp.h>
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
 * Code adopted, and heavily changed, from the BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 */


volatile size_t pagetable_count = 0;


/*
 * Switch to a new page directory.
 */
void vmmngr_switch_pdirectory(pdirectory *dir_phys, pdirectory *dir_virt)
{
    if(!dir_phys || !dir_virt)
    {
        return;
    }
    
    this_core->_cur_directory_phys = dir_phys;
    this_core->_cur_directory_virt = dir_virt;
    pmmngr_load_PDBR((physical_addr)&dir_phys->m_entries_phys);

    return;
}


/*
 * Get current page directory.
 */
pdirectory *vmmngr_get_directory_virt(void)
{
    return this_core->_cur_directory_virt;
}


pdirectory *vmmngr_get_directory_phys(void)
{
    return this_core->_cur_directory_phys;
}


/**
 * @brief Get page entry.
 *
 * Get the page table entry representing the given virtual address.
 *
 * @param   virt    virtual address
 *
 * @return  page table entry.
 */
pt_entry *get_page_entry(virtual_addr virt)
{
    pdirectory *page_directory = this_core->cur_task ? 
                    (pdirectory *)this_core->cur_task->pd_virt : 
                                  vmmngr_get_directory_virt();

    return get_page_entry_pd(page_directory, virt);
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
    // allocate a free physical frame
    void *p = pmmngr_alloc_block();
    
    if(!p)
    {
        kpanic("Insufficient memory (in vmmngr_alloc_page())!\n");
        return 0;
    }

    // map it to the page
    flags |= I86_PTE_PRESENT;
    __atomic_store_n(e, (uintptr_t)p | flags, __ATOMIC_SEQ_CST);
    __asm__ __volatile__("":::"memory");

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
        if((page = get_page_entry(i)))
        {
            if(!(p = pmmngr_alloc_block()))
            {
                printk("vmm: failed to alloc page at 0x%x\n", i);
                kpanic("Insufficient memory (in vmmngr_alloc_pages())!\n");

                // rollback everything
                i -= PAGE_SIZE;

                while(i >= addr)
                {
                    vmmngr_free_page(get_page_entry(i));
                    vmmngr_flush_tlb_entry(i);
                    i -= PAGE_SIZE;
                }

                return 0;
            }

            __atomic_store_n(page, (uintptr_t)p | flags, __ATOMIC_SEQ_CST);
            __asm__ __volatile__("":::"memory");

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

    __atomic_store_n(e, 0, __ATOMIC_SEQ_CST);
    __asm__ __volatile__("":::"memory");
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
        if((e = get_page_entry(i)))
        {
            if((p = (void *)PTE_FRAME(*e)))
            {
                pmmngr_free_block(p);
            }

            __atomic_store_n(e, 0, __ATOMIC_SEQ_CST);
            __asm__ __volatile__("":::"memory");
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
        pt_entry *page = get_page_entry(i);
  
        if(page && PTE_PRESENT(*page))
        {
            PTE_CLEAR_ATTRIBS(page);
            PTE_ADD_ATTRIB(page, flags);
            __asm__ __volatile__("":::"memory");
            vmmngr_flush_tlb_entry(i);
        }

        i += PAGE_SIZE;
    }
}


/*
 * Map a page.
 */
void vmmngr_map_page(physical_addr phys, virtual_addr virt, int flags)
{
    pt_entry *page = get_page_entry(virt);

    if(!page)
    {
        return;
    }

    // map it in (Can also do (*page |= 3 to enable..)
    __atomic_store_n(page, (uintptr_t)phys | flags, __ATOMIC_SEQ_CST);
    __asm__ __volatile__("":::"memory");
}


/*
 * Unmap a page.
 */
void vmmngr_unmap_page(virtual_addr virt)
{
    pt_entry *pt = get_page_entry(virt);

    if(pt)
    {
        __atomic_store_n(pt, 0, __ATOMIC_SEQ_CST);
        __asm__ __volatile__("":::"memory");
        vmmngr_flush_tlb_entry((virtual_addr)virt);
    }
}


/*
 * Free page directory.
 */
void free_pd(virtual_addr addr)
{
    physical_addr phys;

    if((phys = get_phys_addr(addr)))
    {
        pmmngr_free_block((void *)phys);
        __atomic_fetch_sub(&pagetable_count, 1, __ATOMIC_SEQ_CST);
    }
}


/*
 * Get page table count.
 */
size_t used_pagetable_count(void)
{
    //printk("%s: pagetable_count %ld\n", __func__, pagetable_count);
    return (size_t)pagetable_count;
}


// last address we used
volatile virtual_addr last_kmod_addr = KMODULE_START;
volatile virtual_addr last_mmio_addr = MMIO_START;

// mutex to avoid clashes between tasks wanting to allocate pages
volatile struct kernel_mutex_t kmod_mem_mutex = { 0, };
volatile struct kernel_mutex_t mmio_mutex = { 0, };


/*
 * Map a kernel module.
 */
virtual_addr kmod_map(physical_addr pstart, physical_addr pend)
{
    physical_addr aligned_pstart = align_down(pstart);
    virtual_addr res = last_kmod_addr;
    size_t sz = align_up(pend - pstart);
    volatile size_t i;

    kernel_mutex_lock(&kmod_mem_mutex);

    for(i = 0; i < sz; i += PAGE_SIZE)
    {
        vmmngr_map_page(aligned_pstart + i, last_kmod_addr + i, PTE_FLAGS_PW);
        //vmmngr_flush_tlb_entry(last_mmio_addr + i);
    }

    last_kmod_addr += sz;
    kernel_mutex_unlock(&kmod_mem_mutex);

    return res + (pstart - align_down(pstart));
}


/*
 * Map an MMIO address space.
 */
virtual_addr mmio_map(physical_addr pstart, physical_addr pend)
{

#define flags   (PTE_FLAGS_PW | I86_PTE_WRITETHOUGH | I86_PTE_NOT_CACHEABLE)
//#define flags   (PTE_FLAGS_PW | I86_PTE_NOT_CACHEABLE)

    physical_addr aligned_pstart = align_down(pstart);
    virtual_addr res = last_mmio_addr;
    size_t sz = align_up(pend - pstart);
    volatile size_t i;

    pmmngr_deinit_region(aligned_pstart, align_up(pend) - aligned_pstart);

    kernel_mutex_lock(&mmio_mutex);

    for(i = 0; i < sz; i += PAGE_SIZE)
    {
        vmmngr_map_page(aligned_pstart + i, last_mmio_addr + i, flags);
        //vmmngr_flush_tlb_entry(last_mmio_addr + i);
    }

    last_mmio_addr += sz;
    kernel_mutex_unlock(&mmio_mutex);

    return res + (pstart - align_down(pstart));

#undef flags

}

