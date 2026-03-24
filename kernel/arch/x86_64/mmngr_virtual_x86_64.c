/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
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

/* defined in boot.S */
extern pdirectory BootPageDirectory[3];

pdirectory himem_pdp __attribute__((aligned(4096))) = { 0, };
pdirectory himem_pd[64] __attribute__((aligned(4096))) = { 0, };
pdirectory kheap_pdp __attribute__((aligned(4096))) = { 0, };
pdirectory kheap_pd __attribute__((aligned(4096))) = { 0, };
pdirectory mmio_pdp __attribute__((aligned(4096))) = { 0, };
pdirectory kmod_pdp __attribute__((aligned(4096))) = { 0, };
pdirectory kmem_pdp __attribute__((aligned(4096))) = { 0, };
pdirectory kmem_pd __attribute__((aligned(4096))) = { 0, };
ptable kheap_pt[4] __attribute__((aligned(4096))) = { 0, };
ptable kmem_pt[4] __attribute__((aligned(4096))) = { 0, };


/*
 * Get page entry.
 */
pt_entry *__get_page_entry_pd(pdirectory *pml4, virtual_addr virt, int __flags)
{
    int create = (__flags & FLAG_GETPDE_CREATE);
    int userflag = (virt <= USER_MEM_END) ? I86_PDE_USER : 0;
    int access = PTE_FLAGS_PW | userflag;
    size_t pml4i = PML4_INDEX(virt);
    size_t pdpi = PDP_INDEX(virt);
    size_t pdi = PD_INDEX(virt);
    void *tmp;
    pdirectory *pdp, *pd;
    ptable *pt;

    if(!pml4)
    {
        return NULL;
    }

    if(!PDE_PRESENT(pml4->m_entries_phys[pml4i]))
    {
        if(!create)
        {
            return NULL;
        }

        if(!(tmp = pmmngr_alloc_block()))
        {
            kpanic("Insufficient memory (in __get_page_entry_pd())!\n");
            return NULL;
        }

        A_memset((void *)PHYS_TO_HIMEM(tmp), 0, PAGE_SIZE);
        pml4->m_entries_phys[pml4i] = (uintptr_t)tmp | access;
        __atomic_fetch_add(&pagetable_count, 1, __ATOMIC_SEQ_CST);
    }

    pdp = (pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(pml4->m_entries_phys[pml4i]));

    if(!PDE_PRESENT(pdp->m_entries_phys[pdpi]))
    {
        if(!create)
        {
            return NULL;
        }

        if(!(tmp = pmmngr_alloc_block()))
        {
            kpanic("Insufficient memory (in __get_page_entry_pd())!\n");
            return NULL;
        }

        A_memset((void *)PHYS_TO_HIMEM(tmp), 0, PAGE_SIZE);
        pdp->m_entries_phys[pdpi] = (uintptr_t)tmp | access;
        __atomic_fetch_add(&pagetable_count, 1, __ATOMIC_SEQ_CST);
    }

    pd = (pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(pdp->m_entries_phys[pdpi]));

    if(!PDE_PRESENT(pd->m_entries_phys[pdi]))
    {
        if(!create)
        {
            return NULL;
        }

        if(!(tmp = pmmngr_alloc_block()))
        {
            kpanic("Insufficient memory (in __get_page_entry_pd())!\n");
            return NULL;
        }

        A_memset((void *)PHYS_TO_HIMEM(tmp), 0, PAGE_SIZE);
        pd->m_entries_phys[pdi] = (uintptr_t)tmp | access;
        __atomic_fetch_add(&pagetable_count, 1, __ATOMIC_SEQ_CST);
    }

    pt = (ptable *)PHYS_TO_HIMEM(PDE_FRAME(pd->m_entries_phys[pdi]));

    return &pt->m_entries[PT_INDEX(virt)];
}


pt_entry *get_page_entry_pd(pdirectory *pml4, virtual_addr virt)
{
    return __get_page_entry_pd(pml4, virt, FLAG_GETPDE_CREATE);
}


/*
 * Get physical address.
 */
physical_addr get_phys_addr(virtual_addr virt)
{
    pdirectory *pml4 = this_core->cur_task ? 
                            (pdirectory *)this_core->cur_task->pd_virt : 
                            vmmngr_get_directory_virt();
    pdirectory *pdp, *pd;
    ptable *pt;
    size_t pml4i = PML4_INDEX(virt);
    size_t pdpi = PDP_INDEX(virt);
    size_t pdi = PD_INDEX(virt);
    size_t pti = PT_INDEX(virt);

    if(!pml4)
    {
        return 0;
    }

    if(!PDE_PRESENT(pml4->m_entries_phys[pml4i]))
    {
        return 0;
    }

    pdp = (pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(pml4->m_entries_phys[pml4i]));

    if(!PDE_PRESENT(pdp->m_entries_phys[pdpi]))
    {
        return 0;
    }

    if((pdp->m_entries_phys[pdpi] & 0x80) == 0x80)
    {
        return PDE_FRAME(pdp->m_entries_phys[pdpi]) | (virt & 0x3fffffff);
    }

    pd = (pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(pdp->m_entries_phys[pdpi]));

    if(!PDE_PRESENT(pd->m_entries_phys[pdi]))
    {
        return 0;
    }

    if((pd->m_entries_phys[pdi] & 0x80) == 0x80)
    {
        return PDE_FRAME(pd->m_entries_phys[pdi]) | (virt & 0x1fffff);
    }

    pt = (ptable *)PHYS_TO_HIMEM(PDE_FRAME(pd->m_entries_phys[pdi]));

    if(!PTE_PRESENT(pt->m_entries[pti]))
    {
        return 0;
    }

    return PTE_FRAME(pt->m_entries[pti]) | (virt & 0xfff);
}


#define MAKE_PHYS(v)        ((uintptr_t)(v) - KERNEL_MEM_START)
#define KERNEL_PML_ACCESS   (I86_PDE_PRESENT | I86_PDE_WRITABLE)

/*
 * Initialize the virtual memory manager.
 */
void vmmngr_initialize(unsigned long addr)
{
    volatile size_t i, j, k, num_pds;
    volatile size_t mmap_bytes, mmap_pages;
    physical_addr first_available;
    virtual_addr ro_start = (virtual_addr)&kernel_ro_start - KERNEL_MEM_START;
    virtual_addr ro_end = (virtual_addr)&kernel_ro_end - KERNEL_MEM_START;
    virtual_addr p;

    // don't call vmmngr_switch_pdirectory() as the boot code has already
    // loaded PML4 into CR3
    this_core->_cur_directory_phys = (void *)MAKE_PHYS(BootPageDirectory);
    this_core->_cur_directory_virt = BootPageDirectory;

    // find out the size needed for our memory bitmap and the physical
    // start address of the memory area where we can store the bitmap
    pmmngr_early_init(addr, &mmap_bytes, &first_available);
    mmap_pages = mmap_bytes / PAGE_SIZE;

    if(mmap_bytes % PAGE_SIZE)
    {
        mmap_pages++;
    }

    // set up initial PDPs
    BootPageDirectory[0].m_entries_phys[PML4_INDEX(HIMEM_START)] =
                    MAKE_PHYS(&himem_pdp) | KERNEL_PML_ACCESS;
    BootPageDirectory[0].m_entries_phys[PML4_INDEX(KHEAP_START)] =
                    MAKE_PHYS(&kheap_pdp) | KERNEL_PML_ACCESS;
    BootPageDirectory[0].m_entries_phys[PML4_INDEX(MMIO_START)] =
                    MAKE_PHYS(&mmio_pdp) | KERNEL_PML_ACCESS;
    BootPageDirectory[0].m_entries_phys[PML4_INDEX(KMODULE_START)] =
                    MAKE_PHYS(&kmod_pdp) | KERNEL_PML_ACCESS;

    // identity map from -128GiB using 2MiB pages
    // this way we can later alloc a physical page and directly write to
    // it by mapping it to our himem region
    for(i = 0; i < 64; i++)
    {
        himem_pdp.m_entries_phys[i] = 
                MAKE_PHYS(&himem_pd[i]) | KERNEL_PML_ACCESS;

        for(j = 0; j < 512; j++)
        {
            himem_pd[i].m_entries_phys[j] = 
                    (i << 30) |     // PDP
                    (j << 21) |     // PD
                    0x80      |     // large page
                    KERNEL_PML_ACCESS;
        }
    }

    // calculate the number of PD entries the kernel needs
    num_pds = (0x100000 + kernel_size) / 0x200000;
    
    if(((0x100000 + kernel_size) % 0x200000))
    {
        num_pds++;
    }

    // map the kernel's PDP
    kmem_pdp.m_entries_phys[0] = MAKE_PHYS(&kmem_pd) | KERNEL_PML_ACCESS;

    // map the kernel
    for(i = 0; i < num_pds; i++)
    {
        kmem_pd.m_entries_phys[i] = MAKE_PHYS(&kmem_pt[i]) | KERNEL_PML_ACCESS;

        for(j = 0; j < 512; j++)
        {
            // map kernel's text, rodata and data sections as read-only, and
            // everything else as writeable.
            p = (0x200000 * i) + (PAGE_SIZE * j);

            if(p < ro_start || p > ro_end)
            {
                kmem_pt[i].m_entries[j] = p | KERNEL_PML_ACCESS;
            }
            else
            {
                kmem_pt[i].m_entries[j] = p | I86_PDE_PRESENT;
            }
        }
    }

    // map the new kernel PDP into our boot PML4
    BootPageDirectory[0].m_entries_phys[PML4_INDEX(KERNEL_MEM_START)] =
                    MAKE_PHYS(&kmem_pdp) | KERNEL_PML_ACCESS;

    // set up the kernel heap
    kheap_pdp.m_entries_phys[0] = MAKE_PHYS(&kheap_pd) | KERNEL_PML_ACCESS;
    kheap_pd.m_entries_phys[0] = MAKE_PHYS(&kheap_pt[0]) | KERNEL_PML_ACCESS;
    kheap_pd.m_entries_phys[1] = MAKE_PHYS(&kheap_pt[1]) | KERNEL_PML_ACCESS;
    kheap_pd.m_entries_phys[2] = MAKE_PHYS(&kheap_pt[2]) | KERNEL_PML_ACCESS;
    kheap_pd.m_entries_phys[3] = MAKE_PHYS(&kheap_pt[3]) | KERNEL_PML_ACCESS;

    for(i = 0, j = 0, k = 0; i < mmap_pages; i++)
    {
        kheap_pt[j].m_entries[k++] = (first_available + (i << 12)) | KERNEL_PML_ACCESS;

        if(k >= 512)
        {
            k = 0;
            j++;
        }
    }

    // now set up the page frame bitmap and mark used memory
    pmmngr_init(addr, PHYS_TO_HIMEM(first_available));

    // and ensure we mark our bitmap pages as used
    pmmngr_deinit_region(first_available, mmap_pages * PAGE_SIZE);

    // unmap page zero
    //kmem_pt[0].m_entries[0] = 0;
    BootPageDirectory[0].m_entries_phys[0] = 0;

    // now we are ready for the heap
    printk("Initializing kernel heap..\n");

    kheap_init((void *)(KHEAP_START + (mmap_pages * PAGE_SIZE)));

    // all frames have 0 sharing by default (until we have user processes)
    size_t frames = pmmngr_get_block_count();
    frame_shares = (uint8_t *)kmalloc(frames);
    A_memset((void *)frame_shares, 0, frames);

    if(!using_ega())
    {
        printk("Initializing VESA BIOS Extensions (VBE)..\n");
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        vbe_init();
    }
}

#undef MAKE_PHYS
#undef KERNEL_PML_ACCESS


static inline void *alloc_pd(physical_addr *phys)
{
    if(!(*phys = (physical_addr)pmmngr_alloc_block()))
    {
        return NULL;
    }

    void *pd = (void *)PHYS_TO_HIMEM(*phys);

    A_memset(pd, 0, PAGE_SIZE);
    __atomic_fetch_add(&pagetable_count, 1, __ATOMIC_SEQ_CST);

    return pd;
}


/*
 * Clone task page directory.
 */
int clone_task_pd(struct task_t *parent, struct task_t *child)
{
    physical_addr dest_pml4_phys;
    physical_addr dest_pdp_phys, dest_pd_phys, pt_phys;
    ptable *pt_virt;
    volatile virtual_addr v;
    pdirectory *dest_pml4v;
    volatile pdirectory *src_pml4v;
    pdirectory *src_pdp, *src_pd;
    pdirectory *dest_pdp, *dest_pd;
    volatile ptable *src_pt;
    volatile size_t i, j, k, l;

    if(!parent || !parent->mem || !child)
    {
        return 1;
    }

    if(!(dest_pml4v = alloc_pd(&dest_pml4_phys)))
    {
        return 1;
    }

    src_pml4v = (pdirectory *)parent->pd_virt;

    kernel_mutex_lock(&(parent->mem->mutex));

    if(!(parent->properties & PROPERTY_IDLE))
    {
        // read the PML4
        for(i = 0; i < 256; i++)
        {
            /* copy only pages that are present */
            if(!PDE_PRESENT(src_pml4v->m_entries_phys[i]))
            {
                continue;
            }

            if(!(dest_pdp = alloc_pd(&dest_pdp_phys)))
            {
                goto bailout;
            }

            __atomic_store_n(&dest_pml4v->m_entries_phys[i], 
                             dest_pdp_phys | PTE_FLAGS_PWU, __ATOMIC_SEQ_CST);

            src_pdp = (pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(src_pml4v->m_entries_phys[i]));

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
                if(!PDE_PRESENT(src_pdp->m_entries_phys[j]))
                {
                    continue;
                }

                if(!(dest_pd = alloc_pd(&dest_pd_phys)))
                {
                    goto bailout;
                }

                __atomic_store_n(&dest_pdp->m_entries_phys[j], 
                                 dest_pd_phys | PTE_FLAGS_PWU, __ATOMIC_SEQ_CST);

                src_pd = (pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(src_pdp->m_entries_phys[j]));

                // read the PD
                for(k = 0; k < 512; k++)
                {
                    /* copy only pages that are present */
                    if(!PDE_PRESENT(src_pd->m_entries_phys[k]))
                    {
                        continue;
                    }

                    if(!(pt_virt = alloc_pd(&pt_phys)))
                    {
                        goto bailout;
                    }

                    __atomic_store_n(&dest_pd->m_entries_phys[k], 
                                     pt_phys | PTE_FLAGS_PWU, __ATOMIC_SEQ_CST);

                    src_pt = (ptable *)PHYS_TO_HIMEM(PDE_FRAME(src_pd->m_entries_phys[k]));

                    // read the PT
                    for(l = 0; l < 512; l++)
                    {
                        /* copy only pages that are present */
                        if(!PTE_PRESENT(src_pt->m_entries[l]))
                        {
                            continue;
                        }

                        // mark as copy-on-write if it is a private mapping,
                        // or if we are forking or cloning (but not if we are 
                        // vforking)
                        if(PTE_PRIVATE(src_pt->m_entries[l]) && 
                           PTE_WRITABLE(src_pt->m_entries[l]))
                        {
                            PTE_MAKE_COW(&src_pt->m_entries[l]);
                        }

                        inc_frame_shares(PTE_FRAME(src_pt->m_entries[l]));
                        pt_virt->m_entries[l] = src_pt->m_entries[l];
                        __asm__ __volatile__("":::"memory");

                        v = (i << 39) | (j << 30) | (k << 21) | (l << 12);
                        vmmngr_flush_tlb_entry(v);
                    }
                }
            }
        }
    }

    // now copy kernel page dir entries
    A_memcpy((void *)&dest_pml4v->m_entries_phys[256],
                (void *)&src_pml4v->m_entries_phys[256], 256 * sizeof(pd_entry));
    __asm__ __volatile__("":::"memory");

    kernel_mutex_unlock(&(parent->mem->mutex));

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


static inline void __free_user_page(volatile pdirectory *pd, int i)
{
    physical_addr phys = PDE_FRAME(pd->m_entries_phys[i]);
    
    if(get_frame_shares(phys) == 0)
    {
        __atomic_fetch_sub(&pagetable_count, 1, __ATOMIC_SEQ_CST);
    }

    pmmngr_free_block((void *)phys);
}


/*
 * Free user pages.
 */
void free_user_pages(virtual_addr src_addr)
{
    volatile virtual_addr v;
    volatile pdirectory *src_pml4v = (volatile pdirectory *)src_addr;
    volatile pdirectory *src_pdp, *src_pd;
    volatile ptable *src_pt;
    volatile size_t i, j, k, l;

    // read the PML4
    for(v = 0, i = 0; i < 256; i++)
    {
        if(!PDE_PRESENT(src_pml4v->m_entries_phys[i]))
        {
            continue;
        }

        src_pdp = (volatile pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(src_pml4v->m_entries_phys[i]));

        // read the PDP
        for(j = 0; j < 512; j++)
        {
            /* free only pages that are present */
            if(!PDE_PRESENT(src_pdp->m_entries_phys[j]))
            {
                continue;
            }

            src_pd = (volatile pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(src_pdp->m_entries_phys[j]));

            // read the PD
            for(k = 0; k < 512; k++)
            {
                /* free only pages that are present */
                if(!PDE_PRESENT(src_pd->m_entries_phys[k]))
                {
                    continue;
                }

                src_pt = (volatile ptable *)PHYS_TO_HIMEM(PDE_FRAME(src_pd->m_entries_phys[k]));

                // read the PT
                for(l = 0; l < 512; l++)
                {
                    /* free only pages that are present */
                    if(!PTE_PRESENT(src_pt->m_entries[l]))
                    {
                        continue;
                    }

                    pmmngr_free_block((void *)PTE_FRAME(src_pt->m_entries[l]));

                    v = (i << 39) | (j << 30) | (k << 21) | (l << 12);
                    vmmngr_flush_tlb_entry(v);

                    __atomic_store_n(&(src_pt->m_entries[l]), 0, __ATOMIC_SEQ_CST);
                    __asm__ __volatile__("":::"memory");
                }

                __free_user_page(src_pd, k);
                __atomic_store_n(&(src_pd->m_entries_phys[k]), 0, __ATOMIC_SEQ_CST);
                __asm__ __volatile__("":::"memory");
            }

            __free_user_page(src_pdp, j);
            __atomic_store_n(&(src_pdp->m_entries_phys[j]), 0, __ATOMIC_SEQ_CST);
            __asm__ __volatile__("":::"memory");
        }

        __free_user_page(src_pml4v, i);
        __atomic_store_n(&(src_pml4v->m_entries_phys[i]), 0, __ATOMIC_SEQ_CST);
        __asm__ __volatile__("":::"memory");
    }
}


/*
 * Get task page count.
 */
size_t get_task_pagecount(struct task_t *task)
{
    size_t count = 0;
    volatile pdirectory *src_pml4v;
    pdirectory *src_pdp, *src_pd;
    ptable *src_pt;
    int i, j, k, l;

    if(!task || !task->pd_virt)
    {
        return 0;
    }

    src_pml4v = (pdirectory *)task->pd_virt;

    // read the PML4
    for(i = 0; i < 256; i++)
    {
        if(!PDE_PRESENT(src_pml4v->m_entries_phys[i]))
        {
            continue;
        }
        
        src_pdp = (pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(src_pml4v->m_entries_phys[i]));

        // read the PDP
        for(j = 0; j < 512; j++)
        {
            /* count only pages that are present */
            if(!PDE_PRESENT(src_pdp->m_entries_phys[j]))
            {
                continue;
            }

            src_pd = (pdirectory *)PHYS_TO_HIMEM(PDE_FRAME(src_pdp->m_entries_phys[j]));

            // read the PD
            for(k = 0; k < 512; k++)
            {
                /* count only pages that are present */
                if(!PDE_PRESENT(src_pd->m_entries_phys[k]))
                {
                    continue;
                }

                src_pt = (ptable *)PHYS_TO_HIMEM(PDE_FRAME(src_pd->m_entries_phys[k]));

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

