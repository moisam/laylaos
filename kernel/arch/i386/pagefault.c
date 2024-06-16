/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: pagefault.c
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
 *  \file pagefault.c
 *
 *  The Virtual Memory Manager (VMM) implementation.
 *
 *  The driver's code is split between these files:
 *    - mmngr_virtual.c => general VMM functions
 *    - arch/xxx/mmngr_virtual_xxx.c => arch-specific VMM functions
 *    - arch/xxx/page_fault.c => arch-specific page fault handler
 */

#include <errno.h>
#include <kernel/task.h>
#include <kernel/ksignal.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kstack.h>
#include <mm/memregion.h>


int page_fault_check_table(pdirectory *pd, volatile virtual_addr faulting_address)
{
    volatile physical_addr tab_phys = 0;
    volatile virtual_addr  tab_virt = 0;

    /*
     * We need to clone the page table. To do this, we will get
     * the current page table of the faulting address, then we get a new
     * page table and copy the current table into the new one, then we
     * need to allocate a new page and update the new table to be able
     * to access it.
     */
    //pdirectory *pd = vmmngr_get_directory_virt();
    size_t pd_index = PD_INDEX((uintptr_t)faulting_address);
    pd_entry *t = &pd->m_entries_virt[pd_index];
    ptable *srctab = (ptable *)PDE_FRAME(*t);
    pd_entry *pt = &pd->m_entries_phys[pd_index];
    physical_addr oldtab_phys = PDE_FRAME(*pt);
    
    // if the page table is not present, we don't need to check for frame
    // shares and CoW, as we will alloc one later.
    if(!srctab)
    {
        return 0;
    }

    KDEBUG("page_fault: pd_index 0x%x, t 0x%x, srctab 0x%x\n", pd_index, t, srctab);
    KDEBUG("page_fault: oldtab_phys 0x%x, shares 0x%x\n", oldtab_phys, get_frame_shares(oldtab_phys));
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if((get_frame_shares(oldtab_phys) != 0) /* && (*t & I86_PDE_COW) */)
    {
        /* get a fresh page table */
        if(get_next_addr((physical_addr *)&tab_phys, (virtual_addr *)&tab_virt,
                         PTE_FLAGS_PWU, REGION_PAGETABLE) != 0)
        {
            return -ENOMEM;
        }

        //KDEBUG("page_fault: tab_phys 0x%x, tab_virt 0x%x\n", tab_phys, tab_virt);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);

        /* copy the page table */
        A_memcpy((void *)tab_virt, (void *)srctab, sizeof(ptable));
        
        int userflag = (pd_index > 0 && pd_index < 0x300) ? I86_PDE_USER : 0;
        init_pd_entry(pd, pd_index, (ptable *)tab_phys, tab_virt, userflag);
        //vmmngr_map_page((void *)tab_phys, (void *)tab_virt, flags);
        vmmngr_flush_tlb_entry(tab_virt);
        
        dec_frame_shares(oldtab_phys);
    }
    else if(PDE_COW(*t))
    {
        //printk("page_fault_check_table: b\n");
        PDE_ADD_ATTRIB(t, I86_PDE_WRITABLE);
        PDE_DEL_ATTRIB(t, I86_PDE_COW);
            
        t = &pd->m_entries_phys[pd_index];
        PDE_ADD_ATTRIB(t, I86_PDE_WRITABLE);
        PDE_DEL_ATTRIB(t, I86_PDE_COW);

        vmmngr_flush_tlb_entry((virtual_addr)srctab);
    }
    
    return 0;
}


void print_err(struct regs *r, struct task_t *ct, virtual_addr faulting_address)
{
    // The error code gives us details of what happened.
    int present = r->err_code & 0x1;      // Page not present
    int rw = r->err_code & 0x2;           // Write operation?
    int us = r->err_code & 0x4;           // Processor was in user-mode?
    int reserved = r->err_code & 0x8;     // Overwritten CPU-reserved bits of page entry?
    int id = r->err_code & 0x10;          // Caused by an instruction fetch?

    printk("Page fault! task (0x%x), error (0x%x: ", ct ? ct->pid : -1, r->err_code);
    printk("%s ", present ? "present" : "not-present");
    printk("%s ", rw ? "write" : "read");
    printk("%s ", us ? "user-mode" : "kernel-mode");

    if (reserved)
    {
        printk("reserved ");
    }

    if (id)
    {
        printk("instruction ");
    }

    printk("\b) at 0x%x\n", faulting_address);
    dump_regs(r);

}

#pragma GCC push_options
#pragma GCC optimize("O0")

/*
 * Page fault handler
 */
int page_fault(struct regs *r, int arg)
{
    UNUSED(arg);
    
    // A page fault has occurred.
    // The faulting address is stored in the CR2 register.
    extern virtual_addr get_cr2(void);
    volatile virtual_addr faulting_address = get_cr2();
    //__asm__ __volatile__("mov %%cr2, %0" : "=r" (faulting_address));
    KDEBUG("page_fault: faulting_address 0x%x\n", faulting_address);
    register struct task_t *ct = get_cur_task();
	
	if(!ct || !ct->mem)
	{
        printk("page_fault: faulting_address 0x%x\n", faulting_address);
	    printk("pagefault handler cannot find current task!\n");
	    print_err(r, NULL, faulting_address);
	    //__asm__ __volatile__("xchg %%bx, %%bx"::);
	    empty_loop();
	}

    // The error code gives us details of what happened.
    int present = r->err_code & 0x1;      // Page not present
    int rw = r->err_code & 0x2;           // Write operation?

    pdirectory *pd = (pdirectory *)ct->pd_virt;

    volatile physical_addr tmp_phys = 0 /* , tab_phys = 0 */;
    volatile virtual_addr  tmp_virt = 0 /* , tab_virt = 0 */;
    
    KDEBUG("page_fault: faulting_address 0x%x, pid 0x%x\n", faulting_address, ct->pid);
        
    kernel_mutex_lock(&(ct->mem->mutex));
        
    // get the memory region containing this address.
    // if not found, it means we either are are accessing a non-mapped
    // memory (and we deserve a SIGSEGV), or we're trying to expand the stack.
    volatile struct memregion_t *memregion = NULL, *memregion_higher = NULL;
    volatile physical_addr phys = 0;
    
    if(!(memregion = memregion_containing((struct task_t *)ct,
                                    faulting_address,
                                    (struct memregion_t **)&memregion_higher)))
    {
        if(!memregion_higher)
        {
            goto unresolved;
        }
        
        if(exceeds_rlimit(ct, RLIMIT_STACK,
                            (KERNEL_MEM_START - ct->end_stack)))
        {
            goto unresolved;
        }

       	if(!(tmp_phys = (physical_addr)pmmngr_alloc_block()))
       	{
       		goto unresolved;
        }
        
        memregion_higher->addr -= PAGE_SIZE;
        memregion_higher->size++;
        
        if(align_down(ct->end_stack) == (memregion_higher->addr + PAGE_SIZE))
        {
            ct->end_stack = memregion_higher->addr;
        }
        
        ct->minflt++;

        if(page_fault_check_table(pd, faulting_address) != 0)
        {
            pmmngr_free_block((void *)tmp_phys);
            goto unresolved;
        }
        
        goto finalize;
    }
    
    // trying to write to a non-writeable page?
    if(rw && !(memregion->prot & PROT_WRITE))
    {
        goto unresolved;
    }
    
    // if page is not present in memory, we need to load it from file then
    // modify its access rights according to the mapping.
    if(!present)
    {
        if(memregion_load_page((struct memregion_t *)memregion, pd,
                                faulting_address) != 0)
        {
            goto unresolved;
        }
        
        ct->majflt++;

        kernel_mutex_unlock(&(ct->mem->mutex));

        return 1;
    }

    pt_entry *e1 = get_page_entry_pd(pd, (void *)faulting_address);
    
    // if page is present and not marked as CoW, or the fault is read access,
    // this is an access violation.
    if(!e1 || !*e1 || /* !(*e1 & I86_PTE_COW) || */ !rw)
    {
        goto unresolved;
    }
    
    ct->minflt++;
    
    // page is present and is marked CoW and we're trying to write to it

    if(page_fault_check_table(pd, faulting_address) != 0)
    {
        goto unresolved;
    }
    
    pt_entry *e = get_page_entry_pd(pd, (void *)faulting_address);

    if(!(*e & I86_PTE_COW))
    {
        vmmngr_flush_tlb_entry(faulting_address);
        kernel_mutex_unlock(&(ct->mem->mutex));
        return 1;
    }

    /* get the physical frame */
    phys = PTE_FRAME(*e);

    KDEBUG("page_fault: phys 0x%x, shares %d\n", phys, get_frame_shares(phys));
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
        
    /* is this the last copy? */
    if(get_frame_shares(phys) == 0)
    {
        /* yes, mark as RW and remove the COW flag */
        PTE_ADD_ATTRIB(e, I86_PTE_WRITABLE);
        PTE_DEL_ATTRIB(e, I86_PTE_COW);
    }
    else
    {
        /* no, so make a copy of it */
        /* get a temporary virtual address so that we can copy the page */
        if(get_next_addr((physical_addr *)&tmp_phys, (virtual_addr *)&tmp_virt,
                             PTE_FLAGS_PWU, REGION_PAGETABLE) != 0)
        {
            goto unresolved;
        }

        KDEBUG("page_fault: tmp_phys 0x%x, tmp_virt 0x%x\n", tmp_phys, tmp_virt);

        A_memcpy((void *)tmp_virt, (void *)(align_down(faulting_address)), PAGE_SIZE);

        dec_frame_shares(phys);
    }

finalize:
    /*
     * if we copied the page with the faulting address, make sure our page
     * table (whether old or fresh) points to the right address.
     */
    if(tmp_phys)
    {
        KDEBUG("page_fault: faulting_address 0x%x\n", faulting_address);
            
        /* decrement the old frame's share count and assign the new one */
        e = get_page_entry_pd(pd, (void *)faulting_address);

        /* ensure we have a clean slate, then set the frame and the flags */
        if(e)
        {
            *e = 0;
            PTE_ADD_ATTRIB(e, PTE_FLAGS_PWU);
            PTE_SET_FRAME(e, tmp_phys);
        }
            
        /* remove the temporary virtual address mapping */
        if(tmp_virt)
        {
            e = get_page_entry_pd(pd, (void *)tmp_virt);

            if(e)
            {
                *e = 0;
            }

            vmmngr_flush_tlb_entry(tmp_virt);
        }
    }

    vmmngr_flush_tlb_entry(faulting_address);
        
    KDEBUG("page_fault: finished\n");

    kernel_mutex_unlock(&(ct->mem->mutex));
        
    return 1;


unresolved:
    
    kernel_mutex_unlock(&(ct->mem->mutex));
    
    // unresolved page fault
    // output an error message
    print_err(r, ct, faulting_address);
    //screen_refresh(NULL);
    __asm__ __volatile__("xchg %%bx, %%bx"::);

    // kill the task - SIGSEGV will be dispatched on return from the IRQ
    add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)faulting_address);
    //user_add_task_signal(ct, SIGSEGV, 1);
    return 1;
}

#pragma GCC pop_options

