/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
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

//#define __DEBUG

#define __USE_XOPEN_EXTENDED
#include <errno.h>
#include <signal.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/ksignal.h>
#include <kernel/asm.h>
#include <kernel/fpu.h>
#include <kernel/tty.h>
#include <kernel/user.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kstack.h>
#include <mm/kheap.h>
#include <mm/memregion.h>
#include <mm/mmap.h>
#include <gui/vbe.h>

#include <fs/dentry.h>


static void print_err(struct regs *r, struct task_t *ct, 
                      virtual_addr faulting_address)
{
    // The error code gives us details of what happened.
    int present = r->err_code & 0x1;      // Page not present
    int rw = r->err_code & 0x2;           // Write operation?
    int us = r->err_code & 0x4;           // Processor was in user-mode?
    int reserved = r->err_code & 0x8;     // Overwritten CPU-reserved bits 
                                          //   of page entry?
    int id = r->err_code & 0x10;          // Caused by an instruction fetch?

    printk("Page fault! Error (0x%x: ", r->err_code);
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

    printk("\b) at " _XPTR_ "\n", faulting_address);

    if(ct)
    {
        printk("Current task (%d - %s) at 0x%lx\n", ct->pid, ct->command, ct);
    }

    printk("cpu: %d\n", this_core->cpuid);
    dump_regs(r);
}


static int map_stack_page(volatile struct memregion_t *memregion,
                          virtual_addr faulting_address,
                          volatile physical_addr *tmp_phys,
                          uintptr_t *private_flag)
{
    if(faulting_address >= STACK_START || faulting_address <= LIB_ADDR_END)
    {
   		return -EINVAL;
    }

    virtual_addr aligned_faulting_address = align_down(faulting_address);

    if(exceeds_rlimit(this_core->cur_task, RLIMIT_STACK, 
                          (STACK_START - aligned_faulting_address)))
    {
    	//__asm__ __volatile__("xchg %%bx, %%bx"::);
   		return -EINVAL;
    }

   	if(!(*tmp_phys = (physical_addr)pmmngr_alloc_block()))
   	{
   		return -ENOMEM;
    }

    if(memregion->addr > aligned_faulting_address)
    {
        virtual_addr end = memregion->addr + (memregion->size * PAGE_SIZE);
            
        memregion->addr = aligned_faulting_address;
        memregion->size = (end - aligned_faulting_address) / PAGE_SIZE;
        this_core->cur_task->end_stack = aligned_faulting_address;
    }

    this_core->cur_task->minflt++;

    *private_flag = (memregion->flags & MEMREGION_FLAG_PRIVATE) ? 
                                                        I86_PTE_PRIVATE : 0;
    __asm__ __volatile__("":::"memory");

    return 0;
}


static inline void __pagefault_cleanup(struct task_t *ct,
                                       uint64_t *fpregs,
                                       int recursive_pagefault)
{
    if(!recursive_pagefault)
    {
        __sync_and_and_fetch(&ct->properties, ~PROPERTY_HANDLING_PAGEFAULT);
        kernel_mutex_unlock(&(ct->mem->mutex));
    }

    cli();
    __asm__ __volatile__("fxrstor (%0)" : : "r"(fpregs));
}


static inline void hang(void)
{
    cli();
    hlt();
	empty_loop();
}


//#pragma GCC push_options
//#pragma GCC optimize("O0")

/*
 * Page fault handler
 */
int page_fault(struct regs *r, int arg)
{
    UNUSED(arg);
    
    // A page fault has occurred.
    // The faulting address is stored in the CR2 register.
    //extern virtual_addr get_cr2(void);
    volatile virtual_addr faulting_address;

    __asm__ __volatile__("movq %%cr2, %0" : "=r"(faulting_address) ::);

    KDEBUG("page_fault: faulting_address " _XPTR_ "\n", faulting_address);

    struct task_t *ct = (struct task_t *)this_core->cur_task;

	if(!ct || !ct->mem)
	{
        switch_tty(1);
        printk("page_fault: faulting_address " _XPTR_ "\n", faulting_address);
	    printk("pagefault handler cannot find current task or its mem ptr (task = " _XPTR_ ", mem " _XPTR_ ")\n", ct, ct->mem);
	    print_err(r, NULL, faulting_address);
	    screen_refresh(NULL);
	    __asm__ __volatile__("xchg %%bx, %%bx"::);
        hang();
	}

    // The error code gives us details of what happened.
    int present = r->err_code & 0x1;      // Page not present
    int rw = r->err_code & 0x2;           // Write operation?
    int us = r->err_code & 0x4;           // Processor was in user-mode?

    pdirectory *pd = (pdirectory *)ct->pd_virt;

    volatile physical_addr tmp_phys = 0;
    //volatile virtual_addr  tmp_virt = 0;
    int recursive_pagefault = (ct->properties & PROPERTY_HANDLING_PAGEFAULT);
    uint64_t *fpregs, /* __fpregs[64 + 1] */ *__fpregs = kmalloc(sizeof(uint64_t) * 66);
    
    /*
     * There is a good chance we will need to either load the page from disk,
     * or allocate a free (zeroed) page. Both of these will likely involve
     * SSE, which will corrupt the userspace fpregs. We can't save these in
     * the task struct, as any context switching will overwrite this data,
     * and returning to usermode will probably break the user application.
     * So we temporarily store fpregs on the stack here and restore them
     * upon return.
     */
    if((uintptr_t)__fpregs & 0x0f)
    {
        fpregs = (uint64_t *)((uintptr_t)__fpregs + 16 - ((uintptr_t)__fpregs & 0x0f));
    }
    else
    {
        fpregs = __fpregs;
    }
    
    __asm__ __volatile__("fxsave (%0)" : : "r"(fpregs));

	if(faulting_address > KERNEL_MEM_START)
	{
	    goto unresolved;
	}
    
    sti();
    
    if(!recursive_pagefault)
    {
        __sync_or_and_fetch(&ct->properties, PROPERTY_HANDLING_PAGEFAULT);
        kernel_mutex_lock(&(ct->mem->mutex));
    }
    
    // get the memory region containing this address.
    // if not found, it means we either are are accessing a non-mapped
    // memory (and we deserve a SIGSEGV), or we're trying to expand the stack.
    volatile struct memregion_t *memregion = NULL;
    volatile physical_addr phys = 0;
    uintptr_t private_flag = 0;
    volatile pt_entry *e1 = get_page_entry_pd(pd, faulting_address);

    if(!(memregion = memregion_containing(ct, faulting_address)))
    {
        if(!(memregion = memregion_containing(ct, ct->end_stack)))
        {
            goto unresolved;
        }
        
        if(memregion->type != MEMREGION_TYPE_STACK)
        {
            goto unresolved;
        }

        if(map_stack_page(memregion, faulting_address,
                              &tmp_phys, &private_flag) != 0)
        {
       		goto unresolved;
        }

        if(e1)
        {
            __atomic_store_n(e1, tmp_phys | PTE_FLAGS_PWU | private_flag, __ATOMIC_SEQ_CST);
        }

        vmmngr_flush_tlb_entry(faulting_address);
        __pagefault_cleanup(ct, fpregs, recursive_pagefault);
        kfree(__fpregs);
        return 1;
    }

    // trying to access kernel memory from userland?
    if(memregion->type == MEMREGION_TYPE_KERNEL && us)
    {
        goto unresolved;
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
        if(memregion->type == MEMREGION_TYPE_STACK)
        {
            if(map_stack_page(memregion, faulting_address,
                                  &tmp_phys, &private_flag) != 0)
            {
           		goto unresolved;
            }

            if(e1)
            {
                __atomic_store_n(e1, tmp_phys | PTE_FLAGS_PWU | private_flag, __ATOMIC_SEQ_CST);
            }

            vmmngr_flush_tlb_entry(faulting_address);
            __pagefault_cleanup(ct, fpregs, recursive_pagefault);
            kfree(__fpregs);
            return 1;
        }

        if(memregion_load_page((struct memregion_t *)memregion, pd,
                                faulting_address) != 0)
        {
            goto unresolved;
        }
        
        ct->majflt++;
        __pagefault_cleanup(ct, fpregs, recursive_pagefault);
        kfree(__fpregs);

        return 1;
    }

    // if page is present and not marked as CoW, or the fault is read access,
    // this is an access violation.
    if(!e1 || !*e1 || !rw)
    {
        goto unresolved;
    }
    
    ct->minflt++;
    
    // page is present and is marked CoW and we're trying to write to it

    /* get the physical frame */
    phys = PTE_FRAME(*e1);
        
    /* is this the last copy? */
    if(get_frame_shares(phys) == 0)
    {
        /* yes, mark as RW and remove the COW flag */
        PTE_REMOVE_COW(e1);
    }
    else
    {
        /* no, so make a copy of it */
        /* get a temporary virtual address so that we can copy the page */
       	if(!(tmp_phys = (physical_addr)pmmngr_alloc_block()))
       	{
            goto unresolved;
        }

        A_memcpy((void *)PHYS_TO_HIMEM(tmp_phys), 
                 (void *)(align_down(faulting_address)), PAGE_SIZE);

        dec_frame_shares(phys);

        private_flag = (memregion->flags & MEMREGION_FLAG_PRIVATE) ?
                                        I86_PTE_PRIVATE : 0;

        if(e1)
        {
            __atomic_store_n(e1, tmp_phys | PTE_FLAGS_PWU | private_flag, __ATOMIC_SEQ_CST);
        }
    }

    vmmngr_flush_tlb_entry(faulting_address);
    __pagefault_cleanup(ct, fpregs, recursive_pagefault);
    kfree(__fpregs);

    return 1;


unresolved:

    // unresolved page fault in a kernel task
    // output an error message
    //if(!ct->user)
    {
        switch_tty(1);
        printk("cpu %d (cur %d, prev %d):\n", this_core->cpuid, ct->cpuid, ct->prev_cpuid);


        volatile int i;

        for(i = 0; i < processor_count; i++)
        {
            if(processor_local_data[i].cur_task == ct && processor_local_data[i].cpuid != this_core->cpuid)
            {
                printk("task also running on cpu %d!\n", processor_local_data[i].cpuid);
            }
        }


        print_err(r, ct, faulting_address);

        printk("bytes: ");
        for(int z = 0; z < 8; z++) printk("%02x ", ((char *)r->rip)[z]);
        printk("\n");

        //kernel_stack_trace();
        printk("pid %d, prop 0x%x, threads %d, children %d, tlead %d\n", ct->pid, ct->properties, ct->threads->thread_count, ct->children, ct->threads->thread_group_leader->pid);

    	if(faulting_address > KERNEL_MEM_START)
    	{
            pt_entry *e1 = get_page_entry_pd(pd, faulting_address);
            printk("phys(1) is 0x%lx\n", PTE_FRAME(*e1));

            if(ct->parent)
            {
                e1 = get_page_entry_pd((pdirectory *)ct->parent->pd_virt, faulting_address);
                printk("phys(2) is 0x%lx\n", PTE_FRAME(*e1));
            }

            e1 = get_page_entry_pd((pdirectory *)this_core->idle_task->pd_virt, faulting_address);
            printk("phys(3) is 0x%lx\n", PTE_FRAME(*e1));
    	}
    	else
    	{
            for(struct memregion_t *tmp = ct->mem->first_region; tmp != NULL; tmp = tmp->next)
            {
                char *path;
                struct dentry_t *dent;

                if(r->rip >= tmp->addr &&
                   r->rip < (tmp->addr + (tmp->size * PAGE_SIZE)))
                {
                    path = "*";

                    if(tmp->inode && get_dentry(tmp->inode, &dent) == 0)
                    {
                        if(dent->path)
                        {
                            path = dent->path;
                        }
                    }

                    printk("memregion: addr %lx - %lx (type %d, prot %x, fl %x, %s)\n", 
                           tmp->addr, tmp->addr + (tmp->size * PAGE_SIZE), tmp->type, 
                           tmp->prot, tmp->flags, path);
                    break;
                }
    	    }
    	}

        screen_refresh(NULL);
        __asm__ __volatile__("xchg %%bx, %%bx"::);

        kpanic("page fault");
        hang();
    }

    __pagefault_cleanup(ct, fpregs, recursive_pagefault);
    kfree(__fpregs);

    // user task
    // kill the task and force signal dispatch
    add_task_segv_signal(ct, SEGV_MAPERR, (void *)faulting_address);
    check_pending_signals(r);
    return 1;
}

//#pragma GCC pop_options

