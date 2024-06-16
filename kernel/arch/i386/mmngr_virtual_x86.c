/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: mmngr_virtual_x86.c
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
 *  \file mmngr_virtual_x86.c
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
#include <kernel/mutex.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <gui/vbe.h>


pt_entry *get_page_entry_pd(pdirectory *page_directory, void *virt)
{
    if(!page_directory)
    {
        return NULL;
    }
   
    // get page table
    size_t pd_index = PD_INDEX((uintptr_t)virt);
    pd_entry *e = &page_directory->m_entries_virt[pd_index];

    if(!PDE_PRESENT(*e))
    {
        // page table not present, allocate it
        physical_addr ptable_phys = 0;
        virtual_addr ptable_virt = 0;

        if(get_next_addr(&ptable_phys, &ptable_virt,
                         PTE_FLAGS_PW, REGION_PAGETABLE) != 0)
        {
            // nothing found
            return 0;
        }
      
      // init the page directory entry for virt to point to our new table
      //KDEBUG("pd = 0x%x, e = %x, p = %x, v = %x\n", page_directory, e, ptable_phys, ptable_virt);
      int userflag = (pd_index > 0 && pd_index < 0x300) ? I86_PDE_USER : 0;
      init_pd_entry(page_directory, pd_index, (ptable *)ptable_phys, ptable_virt, userflag);
      
      //KDEBUG("new table phys addr 0x%x\n", get_phys_addr(ptable_virt));
      //__asm__ __volatile__("xchg %%bx, %%bx"::);

      // clear page table
      A_memset((void *)ptable_virt, 0, sizeof(ptable));
   }

   // get table
   ptable *table = (ptable *)PDE_FRAME(*e);

   // get page
   return &table->m_entries[PT_INDEX((uintptr_t)virt)];
}


void vmmngr_initialize(void)
{

#define NUM_TABLES      3

    // allocate default page table
    ptable *table[NUM_TABLES];
    virtual_addr vtable[NUM_TABLES];
    int j;

    /*
     * allocate 5 page tables:
     *    - 1 table for kernel heap
     *    - 1 table for kernel code & dynamic structs (4mb)
     *    - 1 table for our page tables/directory
     */
    for(j = 0; j < NUM_TABLES; j++)
    {
        table[j] = (ptable *) pmmngr_alloc_block ();
        
        if(!table[j])
        {
            return;
        }
        
        // clear page table
        A_memset(table[j], 0, sizeof (ptable));

        vtable[j] = last_table_addr;
        last_table_addr += PAGE_SIZE;
        //KDEBUG("table[%d] = 0x%x, vtable[%d] = 0x%x\n", j, table[j], j, vtable[j]);
    }
   
    // create default directory table
    pdirectory* dir = (pdirectory*) pmmngr_alloc_blocks (PDIRECTORY_FRAMES);

    if (!dir) return;
   
    virtual_addr vdir = last_table_addr;
    _cur_directory_virt = (pdirectory*)last_table_addr;
    last_table_addr += (PAGE_SIZE * PDIRECTORY_FRAMES);

    //KDEBUG("vt1 = %x, vt2 = %x, vpd = %x\n", vtable[0], vtable[1], _cur_directory_virt);
    //KDEBUG("pt1 = %x, pt2 = %x, ppd = %x\n", table[0], table[1], dir);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    virtual_addr start = (virtual_addr)&kernel_ro_start;
    virtual_addr end = (virtual_addr)&kernel_ro_end;
    
    /*
     * setup a page table for our kernel code and dynamic structs
     * map 1mb to 3gb (where we are at)
     */
    for (virtual_addr i = 0, frame = 0x0, virt = 0xc0000000;
         i < 1024;
         i++, frame += PAGE_SIZE, virt += PAGE_SIZE)
    {
        // create a new page
        pt_entry page = 0;
        PTE_ADD_ATTRIB(&page, I86_PTE_PRESENT);
      
        // map kernel's text, rodata and data sections as read-only, and
        // everything else as writeable.
        if(virt < start || virt > end)
        {
            PTE_ADD_ATTRIB(&page, I86_PTE_WRITABLE);
        }
      
        PTE_SET_FRAME(&page, frame);

        // ...and add it to the page table
        table[0]->m_entries [PT_INDEX(virt)] = page;
    }

#if 0
    // Map VGA video memory to 0xC03FF000 as "present, writable".
    pt_entry page = 0;
    pt_entry_add_attrib (&page, I86_PTE_PRESENT);
    pt_entry_add_attrib (&page, I86_PTE_WRITABLE);
    pt_entry_set_frame (&page, VGA_MEMORY_PHYSICAL /* 0x000B8000 */);
    table[0]->m_entries [PT_INDEX (VGA_MEMORY_VIRTUAL /* 0xC03FF000 */)] = page;
#endif

    /*
     * setup a page table for our page tables/directory
     */
    for(j = 0; j < NUM_TABLES; j++)
    {
        pt_entry page = 0;
        PTE_ADD_ATTRIB(&page, PTE_FLAGS_PW);
        PTE_SET_FRAME(&page, (physical_addr)table[j]);
        table[2]->m_entries[PT_INDEX(vtable[j])] = page;
    }

    for (virtual_addr i = 0, frame = (virtual_addr)dir, virt = vdir;
         i < PDIRECTORY_FRAMES;
         i++, frame += PAGE_SIZE, virt += PAGE_SIZE)
    {
        pt_entry page = 0;
        PTE_ADD_ATTRIB(&page, PTE_FLAGS_PW);
        PTE_SET_FRAME(&page, frame);
        //KDEBUG("--- 0x%x, 0x%x\n", virt, frame);
        table[2]->m_entries[PT_INDEX(virt)] = page;
    }

    // clear directory table and set it as current
    A_memset(dir, 0, sizeof (pdirectory));

    init_pd_entry(dir, PD_INDEX(0xC0000000), table[0], vtable[0], 0);
    init_pd_entry(dir, PD_INDEX(KHEAP_START), table[1], vtable[1], 0);
    init_pd_entry(dir, PD_INDEX(PAGE_TABLE_START), table[2], vtable[2], 0);
   
    // switch to our page directory
    vmmngr_switch_pdirectory (dir, _cur_directory_virt);

    //printk("Initializing DMA..\n");
    //dma_init();

    printk("Initializing kernel heap..\n");
    kheap_init();

    /* all frames have 0 sharing by default (until we have user processes) */
    size_t frames = pmmngr_get_block_count();
    frame_shares = (unsigned char *)kmalloc(frames);
    A_memset((void *)frame_shares, 0, frames);
   
    printk("Initializing VESA BIOS Extensions (VBE)..\n");
    vbe_init();
}


int clone_task_pd(struct task_t *parent, struct task_t *child, int cow)
{
    if(!parent || !parent->mem || !child)
	{
		return 1;
    }

    physical_addr dirp;
    pdirectory *dirv;
    pdirectory *srcv = (pdirectory *)parent->pd_virt;

    // try and get 2 consecutive virtual address pages
    dirv = (pdirectory *)vmmngr_alloc_and_map(PAGE_SIZE * PDIRECTORY_FRAMES, 1,
                                              PTE_FLAGS_PW, &dirp, REGION_PAGETABLE);

    if(!dirv)
    {
        printk("vmm: insufficient memory for page directory\n");
        return 1;
    }

    A_memset(dirv, 0, (PAGE_SIZE * PDIRECTORY_FRAMES) /* sizeof (pdirectory) */);
    kernel_mutex_lock(&(parent->mem->mutex));

    struct memregion_t *memregion;
    
    for(memregion = parent->mem->first_region;
        memregion != NULL;
        memregion = memregion->next)
    {
        size_t sz = memregion->size * PAGE_SIZE;
        virtual_addr start = align_down(memregion->addr);
        virtual_addr end = align_up(start + sz);
        int private = (memregion->flags & MEMREGION_FLAG_PRIVATE);

        size_t pd_index;
        pd_entry *e, *le = NULL;
        ptable *table;
        pt_entry *pt;

        for( ; start < end; start += PAGE_SIZE)
        {
            // get page dir index
            pd_index = PD_INDEX((uintptr_t)start);

            // copy only entries that are present
            if(!(srcv->m_entries_phys[pd_index] & I86_PDE_PRESENT))
            {
                continue;
            }

            // copy page dir entry
        	dirv->m_entries_phys[pd_index] = srcv->m_entries_phys[pd_index];
        	dirv->m_entries_virt[pd_index] = srcv->m_entries_virt[pd_index];

            // do nothing else for kernel memory pages
            if(memregion->type == MEMREGION_TYPE_KERNEL)
            {
                continue;
            }

            // everything should have been done in shmat_internal() during
            // the fork
            /*
            if(memregion->type == MEMREGION_TYPE_SHMEM)
            {
                continue;
            }
            */

            // get page table
            e = &dirv->m_entries_virt[pd_index];
            table = (ptable *)PDE_FRAME(*e);
            pt = &table->m_entries[PT_INDEX((uintptr_t)start)];

            if(PTE_FRAME(*pt))
            //if(PTE_PRESENT(*pt))
            {
                // mark as copy-on-write if it is a private mapping, or if we
                // are forking or cloning (but not if we are vforking)
                if(private || (PTE_WRITABLE(*pt) && cow))
                {
                    if(memregion->type != MEMREGION_TYPE_SHMEM)
                    {
                        PTE_ADD_ATTRIB(pt, I86_PTE_COW);
                        PTE_DEL_ATTRIB(pt, I86_PTE_WRITABLE);
                    }
                }

                inc_frame_shares(PTE_FRAME(*pt));
                vmmngr_flush_tlb_entry(start);
            }
            
            // don't repeat the upcoming code for the same page table
            if(e == le)
            {
                continue;
            }
            
            // remember the last page table
            le = e;

            if(private || cow)
            {
                // make the table's virt address as COW
                PDE_ADD_ATTRIB(e, I86_PDE_COW);
                PDE_DEL_ATTRIB(e, I86_PDE_WRITABLE);
                
                // and its phys address as well
                e = &dirv->m_entries_phys[pd_index];
                PDE_ADD_ATTRIB(e, I86_PDE_COW);
                PDE_DEL_ATTRIB(e, I86_PDE_WRITABLE);
                inc_frame_shares(PDE_FRAME(*e));

                e = &srcv->m_entries_virt[pd_index];
                PDE_ADD_ATTRIB(e, I86_PDE_COW);
                PDE_DEL_ATTRIB(e, I86_PDE_WRITABLE);

                e = &srcv->m_entries_phys[pd_index];
                PDE_ADD_ATTRIB(e, I86_PDE_COW);
                PDE_DEL_ATTRIB(e, I86_PDE_WRITABLE);
                //vmmngr_flush_tlb_entry((virtual_addr)table);
            }
        }
    }

    kernel_mutex_unlock(&(parent->mem->mutex));
    KDEBUG("New page dir at %x (virt %x)\n", (uint32_t)dirp, (uint32_t)dirv);
    child->pd_virt = (virtual_addr)dirv;
    child->pd_phys = dirp;
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    /*
    for(int i = 0; i < 1024; i++)
    {
       	if(dirv->m_entries_phys[i] != srcv->m_entries_phys[i])
       	{
            printk("p[%d] 0x%x, 0x%x", i, dirv->m_entries_phys[i], srcv->m_entries_phys[i]);
            empty_loop();
       	}
       	
       	if(dirv->m_entries_virt[i] != srcv->m_entries_virt[i])
       	{
            printk("v[%d] 0x%x, 0x%x", i, dirv->m_entries_virt[i], srcv->m_entries_virt[i]);
            empty_loop();
       	}
    }
    */
    
    return 0;
}


void free_user_pages(virtual_addr src_addr)
{
    int i;
    //size_t v;
    volatile pdirectory *srcv = (pdirectory *)src_addr;

    /* free memory used by task PTs */
    for(i = 0; i < 1024; i++)
    {
        if(srcv->m_entries_virt[i] & I86_PDE_PRESENT)
        {
            if(i > 0 && i < 0x300)
            {
                volatile pd_entry *e = &srcv->m_entries_virt[i];
                ptable *table = (ptable *)PDE_FRAME(*e);
                pt_entry *pt;
                pt_entry *lpt = &table->m_entries[1024];
                physical_addr addr;
                virtual_addr vaddr;

                // remember the first virtual address represented by this pd entry
                vaddr = (i << 22);


                /*
                 * I selected VBE_BACKBUF_START to be 0x06800000 so it falls
                 * on the beginning of a pagetable address to ease this
                 * comparison.
                 */
                if(vaddr >= VBE_BACKBUF_START && vaddr < VBE_BACKBUF_END)
                {
                    continue;
                }

                /*
                if(vaddr >= VBE_FRONTBUF_START && vaddr < VBE_FRONTBUF_END)
                {
                    continue;
                }
                */


                for(pt = table->m_entries; pt < lpt; pt++, vaddr += PAGE_SIZE)
                {
                    if(!PTE_PRESENT(*pt))
                    {
                        continue;
                    }

                    addr = PTE_FRAME(*pt);

                    pmmngr_free_block((void *)addr);
                    vmmngr_flush_tlb_entry(vaddr);
                }

                addr = PDE_FRAME(srcv->m_entries_phys[i]);
                vaddr = PDE_FRAME(srcv->m_entries_virt[i]);
                
                /* */
                //if(get_frame_shares(addr) == 0)
                {
                    srcv->m_entries_phys[i] = 0;
                    srcv->m_entries_virt[i] = 0;
                }
                /* */

                pmmngr_free_block((void *)addr);
                vmmngr_flush_tlb_entry(vaddr);
            }
        }
    }
}


size_t get_task_pagecount(struct task_t *task)
{
    if(!task || !task->pd_virt)
    {
        return 0;
    }
    
    size_t i, count = 0;
    volatile pdirectory *dirv = (pdirectory *)task->pd_virt;
    
    for(i = 0; i < 1024; i++)
    {
        //KDEBUG("%d,", i);
        if(dirv->m_entries_phys[i] & I86_PDE_PRESENT)
        {
            if(i > 0 && i < 0x300)
            {
                volatile pd_entry *e = &dirv->m_entries_virt[i];
                ptable *table = (ptable *)PDE_FRAME(*e);
                pt_entry *pt;
                pt_entry *lpt = &table->m_entries[1024];

                for(pt = table->m_entries; pt < lpt; pt++)
                {
                    if(!PTE_PRESENT(*pt))
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

