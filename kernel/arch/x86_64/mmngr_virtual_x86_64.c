/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mmngr_virtual_x86_64.c
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
 *  \file mmngr_virtual_x86_64.c
 *
 *  The Virtual Memory Manager (VMM) implementation.
 *
 *  The driver's code is split between these files:
 *    - mmngr_virtual.c => general VMM functions
 *    - arch/xxx/mmngr_virtual_xxx.c => arch-specific VMM functions
 *    - arch/xxx/page_fault.c => arch-specific page fault handler
 */

//#define __DEBUG

#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/mutex.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kheap.h>
#include <mm/kstack.h>
#include <gui/vbe.h>


/* defined in mmngr_virtual.c */
extern volatile virtual_addr last_table_addr;


//physical_addr zeropage_phys = 0;
//virtual_addr zeropage_virt = 0;


/*
 * Get page directory entry.
 */
pdirectory *get_pde(pdirectory *pd, size_t pd_index, int flags)
{
    int userflag = (flags & FLAG_GETPDE_USER) ? I86_PDE_USER : 0;
    int is_pd = (flags & (FLAG_GETPDE_ISPD | FLAG_GETPDE_ISPDP));
    int create = (flags & FLAG_GETPDE_CREATE);
    pd_entry *e = &pd->m_entries_virt[pd_index];

    if(!PDE_PRESENT(*e))
    {
        // page dir not present, allocate it
        physical_addr pd_phys = 0;
        virtual_addr pd_virt = 0;
        size_t sz;
        
        if(!create)
        {
            return 0;
        }
        
        if(is_pd)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            sz = PAGE_SIZE * PDIRECTORY_FRAMES;
            pd_virt = vmmngr_alloc_and_map(sz, 1, PTE_FLAGS_PW,
                                           &pd_phys, REGION_PAGETABLE);
        }
        else
        {
            sz = PAGE_SIZE;
            get_next_addr(&pd_phys, &pd_virt, PTE_FLAGS_PW,
                          REGION_PAGETABLE);
        }

        if(!pd_virt)
        {
            // nothing found
            kpanic("Insufficient memory for page table (in get_pde())!\n");
            return 0;
        }
      
        // init the page directory entry for virt to point to our new pd
        init_pd_entry(pd, pd_index, pd_phys, pd_virt, userflag);

        // clear page dir
        A_memset((void *)pd_virt, 0, sz);
    }

    return (pdirectory *)PDE_VIRT_FRAME(*e);
}


/*
 * Get page entry.
 */
pt_entry *get_page_entry_pd(pdirectory *pml4, void *virt)
{
    int flags = FLAG_GETPDE_CREATE |
                  (((uintptr_t)virt <= USER_MEM_END) ? FLAG_GETPDE_USER : 0);
    pdirectory *pdp, *pd;
    ptable *pt;

    if(!pml4)
    {
        return NULL;
    }
    
    if(!(pdp = get_pde(pml4, PML4_INDEX((uintptr_t)virt), 
                       flags | FLAG_GETPDE_ISPDP)))
    {
        return NULL;
    }

    if(!(pd = get_pde(pdp, PDP_INDEX((uintptr_t)virt), 
                      flags | FLAG_GETPDE_ISPD)))
    {
        return NULL;
    }

    if(!(pt = (ptable *)get_pde(pd, PD_INDEX((uintptr_t)virt), flags)))
    {
        return NULL;
    }

    return &pt->m_entries[PT_INDEX((uintptr_t)virt)];
}


/*
 * Initialize the virtual memory manager.
 */
void vmmngr_initialize(/* multiboot_info_t *mbd */)
{
    virtual_addr pml4v, pdpv[4], pdv[4];
    pdirectory *pml4, *pdp[4], *pd[4];
    virtual_addr ptv, frame, v;
    ptable *pt, *tmp;
    volatile int j, num_tables;
    //int has_vbe = BIT_SET(mbd->flags, 11);
    
    num_tables = (0x100000 + kernel_size) / 0x200000;

    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    if(((0x100000 + kernel_size) % 0x200000))
    {
        num_tables++;
    }

    ptable *table[num_tables];
    virtual_addr vtable[num_tables];
    
    pagetable_count = num_tables;

    for(j = 0; j < num_tables; j++)
    {
        if(!(table[j] = (ptable *)pmmngr_alloc_block()))
        {
            kpanic("Insufficient memory for VM init\n");
            return;
        }
        
        // clear page table
        A_memset(table[j], 0, sizeof (ptable));

        vtable[j] = last_table_addr;
        last_table_addr += PAGE_SIZE;
    }

    virtual_addr ro_start = (virtual_addr)&kernel_ro_start;
    virtual_addr ro_end = (virtual_addr)&kernel_ro_end;
    
    /*
     * setup a page table for our kernel code and dynamic structs
     * map 1mb to 3gb (where we are at)
     */
    for(frame = 0x0, v = KERNEL_MEM_START;
        frame < (0x100000 + kernel_size);
        frame += PAGE_SIZE, v += PAGE_SIZE)
    {
        // create a new page
        pt_entry page = 0;
        PTE_ADD_ATTRIB(&page, I86_PTE_PRESENT);
        
        // map kernel's text, rodata and data sections as read-only, and
        // everything else as writeable.
        if(v < ro_start || v > ro_end)
        {
            PTE_ADD_ATTRIB(&page, I86_PTE_WRITABLE);
        }
        
        PTE_SET_FRAME(&page, frame);

        // ...and add it to the page table
        table[PD_INDEX(v)]->m_entries[PT_INDEX(v)] = page;
    }

#define ALLOC_PD(p)                                     \
    if(!(p = (pdirectory *)                             \
                pmmngr_alloc_blocks(PDIRECTORY_FRAMES)))\
    {                                                   \
        kpanic("Insufficient memory for VM init\n");    \
        return;                                         \
    }                                                   \
    A_memset(p, 0, PD_BYTES);                           \
    pagetable_count += PDIRECTORY_FRAMES;

    // Create the default directory table. To kick things off, we need to do
    // the following:
    //   - alloc kernel's level 4 page directory (pml4)
    //   - alloc the following page directory pages (pdp), each of which 
    //     covers 512 GiB worth of kernel address space:
    //       * the first part of the kernel (starting at 0xFFFF800000000000)
    //       * the kernel heap (starting at KHEAP_START)
    //       * temporary filesystems (starting at TMPFS_START)
    //       * the page cache (starting at PCACHE_MEM_START)
    //   - page directories for each of the above pdps (each page directory
    //     will cover the 1st 1 GiB of the pdp's addresses)
    //   - a page table so we can address resolve the other tables
    //
    // Doing the above here at the beginning ensures that every task that we
    // fork later on will have pointers to the same kernel memory addresses.

    ALLOC_PD(pml4);
    ALLOC_PD(pdp[0]);
    ALLOC_PD(pdp[1]);
    ALLOC_PD(pdp[2]);
    ALLOC_PD(pdp[3]);
    ALLOC_PD(pd[0]);
    ALLOC_PD(pd[1]);
    ALLOC_PD(pd[2]);
    ALLOC_PD(pd[3]);

    pml4v = last_table_addr;
    pdpv[0] = pml4v + PD_BYTES;
    pdpv[1] = pdpv[0] + PD_BYTES;
    pdpv[2] = pdpv[1] + PD_BYTES;
    pdpv[3] = pdpv[2] + PD_BYTES;
    pdv[0] = pdpv[3] + PD_BYTES;
    pdv[1] = pdv[0] + PD_BYTES;
    pdv[2] = pdv[1] + PD_BYTES;
    pdv[3] = pdv[2] + PD_BYTES;
    last_table_addr += (9 * PD_BYTES);

    init_pd_entry(pml4, PML4_INDEX(KERNEL_MEM_START), 
                                    (physical_addr)pdp[0], pdpv[0], 0);
    init_pd_entry(pml4, PML4_INDEX(PAGE_TABLE_START), 
                                    (physical_addr)pdp[1], pdpv[1], 0);
    init_pd_entry(pml4, PML4_INDEX(PCACHE_MEM_START), 
                                    (physical_addr)pdp[2], pdpv[2], 0);
    init_pd_entry(pml4, PML4_INDEX(TMPFS_START), 
                                    (physical_addr)pdp[3], pdpv[3], 0);

    init_pd_entry(pdp[0], PDP_INDEX(KERNEL_MEM_START), 
                                    (physical_addr)pd[0], pdv[0], 0);
    init_pd_entry(pdp[1], PDP_INDEX(PAGE_TABLE_START), 
                                    (physical_addr)pd[1], pdv[1], 0);
    init_pd_entry(pdp[2], PDP_INDEX(PCACHE_MEM_START), 
                                    (physical_addr)pd[2], pdv[2], 0);
    init_pd_entry(pdp[3], PDP_INDEX(TMPFS_START), 
                                    (physical_addr)pd[3], pdv[3], 0);

    for(j = 0; j < num_tables; j++)
    {
        init_pd_entry(pd[0], j, (physical_addr)(table[j]), vtable[j], 0);
    }

#define ALLOC_PT(pt)                                    \
    if(!(pt = (ptable *)pmmngr_alloc_block()))          \
    {                                                   \
        kpanic("Insufficient memory for VM init\n");    \
        return;                                         \
    }                                                   \
    A_memset(pt, 0, sizeof(ptable));                    \
    pagetable_count++;


    ALLOC_PT(pt);
    ptv = last_table_addr;
    last_table_addr += PAGE_SIZE;

    pt->m_entries[PT_INDEX(pml4v)    ] = (uintptr_t)pml4 | PTE_FLAGS_PW;
    pt->m_entries[PT_INDEX(pml4v) + 1] = 
                            ((uintptr_t)pml4 + PAGE_SIZE) | PTE_FLAGS_PW;

    for(j = 0; j < 4; j++)
    {
        pt->m_entries[PT_INDEX(pdpv[j])    ] = (uintptr_t)pdp[j] | PTE_FLAGS_PW;
        pt->m_entries[PT_INDEX(pdpv[j]) + 1] = 
                                ((uintptr_t)pdp[j] + PAGE_SIZE) | PTE_FLAGS_PW;

        pt->m_entries[PT_INDEX(pdv[j])     ] = (uintptr_t)pd[j] | PTE_FLAGS_PW;
        pt->m_entries[PT_INDEX(pdv[j])  + 1] = 
                                ((uintptr_t)pd[j] + PAGE_SIZE) | PTE_FLAGS_PW;
    }

    pt->m_entries[PT_INDEX(ptv)      ] = (uintptr_t)pt | PTE_FLAGS_PW;

    for(j = 0; j < num_tables; j++)
    {
        pt->m_entries[PT_INDEX(vtable[j])] = 
                                (uintptr_t)(table[j]) | PTE_FLAGS_PW;
    }

    init_pd_entry(pd[1], 0, (physical_addr)pt, ptv, 0);

    for(j = 1; j < PAGES_PER_DIR - 20 /* 24 */; j++)
    {
        ALLOC_PT(tmp);
        pt->m_entries[PT_INDEX(last_table_addr)] = 
                                        (uintptr_t)tmp | PTE_FLAGS_PW;
        init_pd_entry(pd[1], j, (physical_addr)tmp, last_table_addr, 0);
        last_table_addr += PAGE_SIZE;
    }

    // switch to our page directory
    vmmngr_switch_pdirectory(pml4, (pdirectory *)pml4v);

    printk("Initializing kernel heap..\n");
    kheap_init();

    /* all frames have 0 sharing by default (until we have user processes) */
    size_t frames = pmmngr_get_block_count();
    frame_shares = (unsigned char *)kmalloc(frames);
    A_memset((void *)frame_shares, 0, frames);

    if(!using_ega())
    //if(has_vbe)
    {
        printk("Initializing VESA BIOS Extensions (VBE)..\n");
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        vbe_init();
    }

    // allocate a zero page after all memory is mapped so we do not overwrite
    // something important
    /*
    get_next_addr(&zeropage_phys, &zeropage_virt, 
                  I86_PTE_PRESENT, REGION_PIPE);
    A_memset((void *)zeropage_virt, 0, PAGE_SIZE);
    inc_frame_shares(zeropage_phys);
    */
}


static inline pdirectory *alloc_pd(physical_addr *phys)
{
    pdirectory *pd;
    
    // try and get 2 consecutive virtual address pages
    if(!(pd = (pdirectory *)vmmngr_alloc_and_map(PD_BYTES, 1, PTE_FLAGS_PW,
                                                 phys, REGION_PAGETABLE)))
    {
        printk("vmm: insufficient memory for page directory\n");
        return NULL;
    }

    A_memset(pd, 0, PD_BYTES);
    
    return pd;
}


#pragma GCC push_options
#pragma GCC optimize("O0")

/*
 * Clone task page directory.
 */
int clone_task_pd(struct task_t *parent, struct task_t *child)
{
    if(!parent || !parent->mem || !child)
    {
        return 1;
    }

    physical_addr dest_pml4_phys;
    physical_addr dest_pdp_phys, dest_pd_phys;
    virtual_addr v;
    pdirectory *dest_pml4v;
    pdirectory *src_pml4v = (pdirectory *)parent->pd_virt;
    pdirectory *src_pdp, *src_pd;
    pdirectory *dest_pdp, *dest_pd;
    ptable *src_pt;
    volatile int i, j, k, l;

    if(!(dest_pml4v = alloc_pd(&dest_pml4_phys)))
    {
        return 1;
    }

    kernel_mutex_lock(&(parent->mem->mutex));

    // read the PML4
    for(v = 0, i = 0; i < 512; i++)
    {
        /* copy only pages that are present */
        if(!PDE_PRESENT(src_pml4v->m_entries_virt[i]))
        {
            v += ((virtual_addr)PAGE_SIZE * 512 * 512 * 512);
            continue;
        }

        /*
         * first 4-mb & kernel space & kheap will be linked as-is, while user 
         * pages will be marked COW and read-only if they are writable.
         */
        if(i >= 256)    /* first 4-mb & kernel space & kheap */
        {
            dest_pml4v->m_entries_phys[i] = src_pml4v->m_entries_phys[i];
            dest_pml4v->m_entries_virt[i] = src_pml4v->m_entries_virt[i];
            continue;
        }

        if(parent->properties & PROPERTY_IDLE)
        //if(parent == idle_task)
        {
            continue;
        }

        if(!(dest_pdp = alloc_pd(&dest_pdp_phys)))
        {
            goto bailout;
        }
        
        init_pd_entry(dest_pml4v, i, dest_pdp_phys, 
                        (virtual_addr)dest_pdp, I86_PDE_USER);
        src_pdp = (pdirectory *)PDE_VIRT_FRAME(src_pml4v->m_entries_virt[i]);
        
        /*
         * When to mark user pages Copy-on-Write (CoW):
         *
         *                  Forking     Cloning     Vforking
         * ---------------------------------------------------
         * MAP_PRIVATE      YES         NO          NO
         * MAP_SHARED       NO          NO          NO
         * ---------------------------------------------------
         */

        // read the PDP
        for(j = 0; j < 512; j++)
        {
            /* copy only pages that are present */
            if(!PDE_PRESENT(src_pdp->m_entries_virt[j]))
            {
                v += (PAGE_SIZE * 512 * 512);
                continue;
            }

            if(!(dest_pd = alloc_pd(&dest_pd_phys)))
            {
                goto bailout;
            }
            
            init_pd_entry(dest_pdp, j, dest_pd_phys, 
                                (virtual_addr)dest_pd, I86_PDE_USER);
            src_pd = (pdirectory *)PDE_VIRT_FRAME(src_pdp->m_entries_virt[j]);

            // read the PD
            for(k = 0; k < 512; k++)
            {
                /* copy only pages that are present */
                if(!PDE_PRESENT(src_pd->m_entries_virt[k]))
                {
                    v += (PAGE_SIZE * 512);
                    continue;
                }

                physical_addr pt_phys = 0;
                virtual_addr pt_virt = 0;

                if(get_next_addr(&pt_phys, &pt_virt, PTE_FLAGS_PWU,
                                 REGION_PAGETABLE) != 0)
                {
                    goto bailout;
                }
                
                A_memset((void *)pt_virt, 0, PAGE_SIZE);
                init_pd_entry(dest_pd, k, pt_phys, pt_virt, I86_PDE_USER);

                src_pt = (ptable *)PDE_VIRT_FRAME(src_pd->m_entries_virt[k]);

                // read the PT
                for(l = 0; l < 512; l++)
                {
                    /* copy only pages that are present */
                    if(!PTE_PRESENT(src_pt->m_entries[l]))
                    {
                        v += PAGE_SIZE;
                        continue;
                    }

                    // mark as copy-on-write if it is a private mapping,
                    // or if we are forking or cloning (but not if we are 
                    // vforking)
                    if(PTE_PRIVATE(src_pt->m_entries[l]) && 
                       PTE_WRITABLE(src_pt->m_entries[l]))
                    {
                        //if(!((v >= USER_SHM_START && v < USER_SHM_END) ||
                        //     (v >= VBE_BACKBUF_START && v < VBE_BACKBUF_END)))
                        {
                            PTE_MAKE_COW(&src_pt->m_entries[l]);
                        }
                    }

                    inc_frame_shares(PTE_FRAME(src_pt->m_entries[l]));
                    ((ptable *)pt_virt)->m_entries[l] = src_pt->m_entries[l];
                    vmmngr_flush_tlb_entry(v);
                    v += PAGE_SIZE;
                }
            }
        }
    }



    kernel_mutex_unlock(&(parent->mem->mutex));

    //KDEBUG("Old page dir at 0x%lx (virt 0x%lx)\n", (uintptr_t)parent->pd_phys, (uintptr_t)src_pml4v);
    //KDEBUG("New page dir at 0x%lx (virt 0x%lx)\n", (uintptr_t)dest_pml4p, (uintptr_t)dest_pml4v);

    child->pd_virt = (virtual_addr)dest_pml4v;
    child->pd_phys = dest_pml4_phys;
    
    return 0;

bailout:

    /*
     * TODO: release used pages.
     */

    kernel_mutex_unlock(&(parent->mem->mutex));
    return 1;
}

#pragma GCC pop_options


static inline void __free_page_table(virtual_addr virt)
{
    volatile pt_entry *pt = get_page_entry((void *)virt);

    if(pt)
    {
        __atomic_store_n(pt, 0, __ATOMIC_SEQ_CST);
    }
}


static inline void __free_user_page(volatile pdirectory *pd, int i, int is_pd)
{
    physical_addr phys = PDE_FRAME(pd->m_entries_phys[i]);
    virtual_addr virt = PDE_VIRT_FRAME(pd->m_entries_virt[i]);
    
    if(get_frame_shares(phys) == 0)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        __atomic_fetch_sub(&pagetable_count, 1, __ATOMIC_SEQ_CST);
        __free_page_table(virt);
    }

    pmmngr_free_block((void *)phys);
    vmmngr_flush_tlb_entry(virt);

    if(is_pd)
    {
        if(get_frame_shares(phys + PAGE_SIZE) == 0)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            __atomic_fetch_sub(&pagetable_count, 1, __ATOMIC_SEQ_CST);
            __free_page_table(virt + PAGE_SIZE);
        }

        pmmngr_free_block((void *)(phys + PAGE_SIZE));
        vmmngr_flush_tlb_entry(virt + PAGE_SIZE);
    }
}


#pragma GCC push_options
#pragma GCC optimize("O0")

/*
 * Free user pages.
 */
void free_user_pages(virtual_addr src_addr)
{
    virtual_addr v;
    volatile pdirectory *src_pml4v = (pdirectory *)src_addr;
    volatile pdirectory *src_pdp, *src_pd;
    ptable *src_pt;
    volatile int i, j, k, l;
    struct kernel_region_t *r = &kernel_regions[REGION_PAGETABLE];

    elevated_priority_lock_recursive(r->mutex, r->lock_count);

    // read the PML4
    for(v = 0, i = 0; i < 512; i++)
    {
        if(!PDE_PRESENT(src_pml4v->m_entries_virt[i]))
        {
            v += ((virtual_addr)PAGE_SIZE * 512 * 512 * 512);
            continue;
        }

        if(i >= 256)
        {
            break;
        }
        
        src_pdp = (pdirectory *)PDE_VIRT_FRAME(src_pml4v->m_entries_virt[i]);
        
        // read the PDP
        for(j = 0; j < 512; j++)
        {
            /* free only pages that are present */
            if(!PDE_PRESENT(src_pdp->m_entries_virt[j]))
            {
                v += (PAGE_SIZE * 512 * 512);
                continue;
            }

            src_pd = (pdirectory *)PDE_VIRT_FRAME(src_pdp->m_entries_virt[j]);

            // read the PD
            for(k = 0; k < 512; k++)
            {
                /* free only pages that are present */
                if(!PDE_PRESENT(src_pd->m_entries_virt[k]))
                {
                    v += (PAGE_SIZE * 512);
                    continue;
                }

                src_pt = (ptable *)PDE_VIRT_FRAME(src_pd->m_entries_virt[k]);

                // read the PT
                for(l = 0; l < 512; l++)
                {
                    /* free only pages that are present */
                    if(!PTE_PRESENT(src_pt->m_entries[l]))
                    {
                        v += PAGE_SIZE;
                        continue;
                    }

                    /*
                    printk("free_user_pages: virt 0x%lx, phys 0x%lx, shares %d\n", v, PTE_FRAME(src_pt->m_entries[l]), get_frame_shares(PTE_FRAME(src_pt->m_entries[l])));
                    screen_refresh(NULL);
                    __asm__ __volatile__("xchg %%bx, %%bx"::);
                    */

                    pmmngr_free_block((void *)PTE_FRAME(src_pt->m_entries[l]));
                    vmmngr_flush_tlb_entry(v);
                    __atomic_store_n(&(src_pt->m_entries[l]), 0, __ATOMIC_SEQ_CST);

                    v += PAGE_SIZE;
                }

                __free_user_page(src_pd, k, 0);
                __atomic_store_n(&(src_pd->m_entries_virt[k]), 0, __ATOMIC_SEQ_CST);
                __atomic_store_n(&(src_pd->m_entries_phys[k]), 0, __ATOMIC_SEQ_CST);
            }

            __free_user_page(src_pdp, j, 1);
            __atomic_store_n(&(src_pdp->m_entries_virt[j]), 0, __ATOMIC_SEQ_CST);
            __atomic_store_n(&(src_pdp->m_entries_phys[j]), 0, __ATOMIC_SEQ_CST);
        }

        __free_user_page(src_pml4v, i, 1);
        __atomic_store_n(&(src_pml4v->m_entries_virt[i]), 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(&(src_pml4v->m_entries_phys[i]), 0, __ATOMIC_SEQ_CST);
    }

    elevated_priority_unlock_recursive(r->mutex, r->lock_count);
}

#pragma GCC pop_options


/*
 * Get task page count.
 */
size_t get_task_pagecount(struct task_t *task)
{
    if(!task || !task->pd_virt)
    {
        return 0;
    }
    
    size_t count = 0;
    volatile pdirectory *src_pml4v = (pdirectory *)task->pd_virt;
    pdirectory *src_pdp, *src_pd;
    ptable *src_pt;
    int i, j, k, l;

    // read the PML4
    for(i = 0; i < 512; i++)
    {
        if(!PDE_PRESENT(src_pml4v->m_entries_virt[i]))
        {
            continue;
        }

        if(i >= 256)
        {
            break;
        }
        
        src_pdp = (pdirectory *)PDE_VIRT_FRAME(src_pml4v->m_entries_virt[i]);
        
        // read the PDP
        for(j = 0; j < 512; j++)
        {
            /* count only pages that are present */
            if(!PDE_PRESENT(src_pdp->m_entries_virt[j]))
            {
                continue;
            }

            src_pd = (pdirectory *)PDE_VIRT_FRAME(src_pdp->m_entries_virt[j]);

            // read the PD
            for(k = 0; k < 512; k++)
            {
                /* count only pages that are present */
                if(!PDE_PRESENT(src_pd->m_entries_virt[k]))
                {
                    continue;
                }

                src_pt = (ptable *)PDE_VIRT_FRAME(src_pd->m_entries_virt[k]);

                // read the PT
                for(l = 0; l < 512; l++)
                {
                    /* count only pages that are present */
                    if(!PTE_PRESENT(src_pt->m_entries[l]))
                    {
                        continue;
                    }

                    count++;
                }
            }
        }
    }
    
    return count;
}


#define MAKE_VIRT_ADDR(pml4i, pdpi, pdi, pti)   \
    (((virtual_addr)(0xFFFF)) << 48)|           \
    (((virtual_addr)(pml4i)) << 39) |           \
    (((virtual_addr)(pdpi)) << 30)  |           \
    (((virtual_addr)(pdi)) << 21)   |           \
    (((virtual_addr)(pti)) << 12);

#define MAY_RETURN_ADDR(pml4i, pdpi, pdi, pti)      \
    {                                               \
        virtual_addr res =                          \
            MAKE_VIRT_ADDR(pml4i, pdpi, pdi, pti);  \
        if(res >= max) return 0;                    \
        if(res >= min) return res;                  \
        continue;                                   \
    }


virtual_addr __get_next_addr(virtual_addr min, virtual_addr max)
{
    volatile pdirectory *src_pml4v = 
                    this_core->cur_task ? 
                        (pdirectory *)this_core->cur_task->pd_virt : 
                                      vmmngr_get_directory_virt();
    pdirectory *src_pdp, *src_pd;
    ptable *src_pt;
    volatile int i, j, k, l;

    // read the PML4
    for(i = (int)PML4_INDEX(min); i <= (int)PML4_INDEX(max); i++)
    //for(i = 256; i < 512; i++)
    {
        if(!PDE_PRESENT(src_pml4v->m_entries_virt[i]))
        {
            MAY_RETURN_ADDR(i, 0, 0, 0);
        }
        
        src_pdp = (pdirectory *)PDE_VIRT_FRAME(src_pml4v->m_entries_virt[i]);
        
        // read the PDP
        for(j = 0; j < 512; j++)
        {
            if(!PDE_PRESENT(src_pdp->m_entries_virt[j]))
            {
                MAY_RETURN_ADDR(i, j, 0, 0);
            }

            src_pd = (pdirectory *)PDE_VIRT_FRAME(src_pdp->m_entries_virt[j]);

            // read the PD
            for(k = 0; k < 512; k++)
            {
                if(!PDE_PRESENT(src_pd->m_entries_virt[k]))
                {
                    MAY_RETURN_ADDR(i, j, k, 0);
                }

                src_pt = (ptable *)PDE_VIRT_FRAME(src_pd->m_entries_virt[k]);

                // read the PT
                for(l = 0; l < 512; l++)
                {
                    if(!PTE_PRESENT(src_pt->m_entries[l]))
                    {
                        MAY_RETURN_ADDR(i, j, k, l);
                    }
                }
            }
        }
    }

    return 0;
}


int get_next_addr(physical_addr *phys, virtual_addr *virt, 
                  int flags, int region)
{
    volatile virtual_addr res;
    struct kernel_region_t *r = &kernel_regions[region];
    volatile int tries = 0;

    // for safety
    *virt = 0;
    *phys = 0;
    res = 0;

    elevated_priority_lock_recursive(r->mutex, r->lock_count);

retry:

    res = __get_next_addr(r->min, r->max);

    if(res)
    {
        pt_entry *pt = get_page_entry((void *)res);

        if(PTE_FRAME(*pt) == 0)
        {
            // reserve the addr temporarily so we can unlock the mutex
            PTE_SET_FRAME(pt, 1);

            elevated_priority_unlock_recursive(r->mutex, r->lock_count);

            if(!vmmngr_alloc_page(pt, flags))
            {
                // this means no physical memory available, so bail out
                PTE_SET_FRAME(pt, 0);
                return -1;
            }

            *virt = res;
            *phys = PTE_FRAME(*pt);

            vmmngr_flush_tlb_entry(res);
            
            if(region == REGION_PAGETABLE)
            {
                __atomic_fetch_add(&pagetable_count, 1, __ATOMIC_SEQ_CST);
            }

            //KDEBUG("get_next_addr: res 0x%x\n", res);
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            return 0;
        }
    }

    if(++tries < 5)
    {
        goto retry;
    }

    elevated_priority_unlock_recursive(r->mutex, r->lock_count);

    // nothing found
    return -1;
}

