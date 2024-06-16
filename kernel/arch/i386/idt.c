/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: idt.c
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
 *  \file idt.c
 *
 * Define Interrupt Descriptor Table [IDT] for x86 and x64 processors to
 * provide interface for managing interrupts, along with other
 * interrupt-related functions.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#define __USE_XOPEN_EXTENDED
#include <signal.h>
#include <sys/ptrace.h>
#include <kernel/laylaos.h>
#include <kernel/idt.h>
#include <kernel/isr.h>
#include <kernel/irq.h>
#include <kernel/pic.h>
#include <kernel/io.h>
#include <kernel/asm.h>
#include <kernel/task.h>
#include <kernel/ksignal.h>
#include <kernel/fpu.h>
#include <kernel/ptrace.h>
#include <kernel/tty.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kstack.h>
#include <mm/memregion.h>
#include <mm/mmap.h>
#include <gui/vbe.h>

#include <fs/dentry.h>


/* installs the IDTR */
extern void _idt_install();

/* The IDT */
struct idt_descriptor_s IDT[MAX_INTERRUPTS];

/* IDTR data */
struct idtr IDTR;

/* Interrupt description strings */
char *intstr[] =
{
    "Division by zero",
    "Single step (debugger)",
    "Non Maskable Interrupt (NMI) Pin",
    "Breakpoint (debugger)",
    "Overflow",
    "Bounds check",
    "Undefined Operation Code (OPCode) instruction",
    "No coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid Task State Segment (TSS)",
    "Segment Not Present",
    "Stack Segment Overrun",
    "General Protection Fault (GPF)",
    "Page Fault",
    "Unassigned",
    "Coprocessor error",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Unknown"
};


/* define some interrupt handlers and map them to their functions */
#define INT_HANDLER(intr)                       \
    int intr(struct regs *r, int arg);          \
    struct handler_t intr##_handler = { .handler = intr, };

INT_HANDLER(page_fault)
INT_HANDLER(singlestep)
INT_HANDLER(gpf)
//INT_HANDLER(ill_opcode)


/*
 * Default handler.
 */
void isr_handler(struct regs *r)
{
    char *s;
    //KDEBUG("isr_handler: int 0x%x\n", r->int_no);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    if(interrupt_handlers[r->int_no] != 0)
    {
        struct handler_t *h;
    
        for(h = interrupt_handlers[r->int_no]; h; h = h->next)
        {
            //KDEBUG("isr_handler: h @ 0x%x\n", h);
            if(h->handler(r, h->handler_arg))
            {
                //KDEBUG("isr_handler: h @ 0x%x - done\n", h);
                break;
            }
            //KDEBUG("isr_handler: h @ 0x%x - skipping\n", h);
        }
    }
    else
    {
        if(r->int_no <= 18)
        {
            s = intstr[r->int_no];
        }
        else if(r->int_no <= 31)
        {
            s = intstr[19];
        }
        else
        {
            s = intstr[20];
        }
        
        switch_tty(1);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);

        /*
        if(cur_task)
        {
            printk("Current process: pid %d, comm %s\n",
                   cur_task->pid, cur_task->command);

            for(struct memregion_t *tmp = cur_task->mem->first_region; tmp != NULL; tmp = tmp->next)
            {
                char *path;
                struct dentry_t *dent;
                
                path = "*";
                
                if(tmp->inode && get_dentry(tmp->inode, &dent) == 0)
                {
                    if(dent->path)
                    {
                        path = dent->path;
                    }
                }

                printk("memregion: addr %lx - %lx (type %d, prot %x, fl %x, %s)\n", tmp->addr, tmp->addr + (tmp->size * PAGE_SIZE), tmp->type, tmp->prot, tmp->flags, path);
            }
        }
        */

        printk("\nUnhandled Interrupt: int %d (%s) - err 0x%x\n",
               r->int_no, s, r->err_code);

        if(cur_task)
        {
            printk("Current task (%d - %s)\n", cur_task->pid, cur_task->command);
        }

        dump_regs(r);

        screen_refresh(NULL);
        __asm__ __volatile__("xchg %%bx, %%bx"::);

        empty_loop();
    }
}


/*
 * Single-step debug interrupt handler
 */
int singlestep(struct regs *r, int arg)
{
    UNUSED(arg);

#ifdef __x86_64__
    r->rflags |= 0x100;
#else
    r->eflags |= 0x100;
#endif      /* __x86_64__ */

    if(get_cur_task()->properties & PROPERTY_TRACE_SIGNALS)
    {
        ptrace_signal(SIGTRAP, PTRACE_EVENT_STOP /* PTRACE_EVENT_SINGLESTEP */);
    }
    
    return 1;
}


/*
 * General Protection Fault interrupt handler
 */
int gpf(struct regs *r, int arg)
{
    UNUSED(arg);
    struct task_t *ct = get_cur_task();
    
    if(!ct || !ct->user)
    {
        kpanic("General protection fault in kernel space!");
    }
    
    // user task
    // kill the task and force signal dispatch

#ifdef __x86_64__
    add_task_segv_signal(ct, SIGSEGV, SEGV_ACCERR, (void *)r->rip);
#else
    add_task_segv_signal(ct, SIGSEGV, SEGV_ACCERR, (void *)r->eip);
#endif      /* __x86_64__ */

    check_pending_signals(r);
    return 1;
}


/*
 * Install a new interrupt handler.
 */
void install_isr(uint32_t no, uint8_t flags,
                 uint16_t selector, void (*isr_function)())
{
    if(no >= MAX_INTERRUPTS)
    {
        return;
    }

    if(!isr_function)
    {
        return;
    }
  
    /* get address of interrupt handler */
    uintptr_t isr_base = (uintptr_t)isr_function;
  
    /* store base addr in IDT */
    IDT[no].base_low = (uint16_t)(isr_base & 0xFFFF);
    IDT[no].selector = selector;
    IDT[no].reserved = 0;
    IDT[no].flags = flags;
    IDT[no].base_hi = (uint16_t)((isr_base >> 16) & 0xFFFF);

#ifdef __x86_64__

    IDT[no].base_very_hi = (uint32_t)((isr_base >> 32) & 0xFFFFFFFF);
    IDT[no].ist = 0;

#endif      /* __x86_64__ */

}


/*
 * Install the IDT.
 */
void idt_install(void)
{
	//__asm__ __volatile__("lidt (IDTR)":::);
	__asm__ __volatile__("movabsq $IDTR, %%rax\nlidt (%%rax)":::"rax");
}


/*
 * Initialise the IDT.
 */
void idt_init(void)
{
    /* set IDTR */
    IDTR.limit = (sizeof(struct idt_descriptor_s) * MAX_INTERRUPTS) - 1;
    IDTR.base = (uintptr_t)&IDT[0];
    //IDTR.base = (uint32_t)&IDT[0];
  
    /* NULL the IDT */
    memset((void *)&IDT[0], 0, sizeof(struct idt_descriptor_s) * 
                                            MAX_INTERRUPTS - 1);
  
    /*
     * Flags are single byte structs:
     *   7                           0
     * +---+---+---+---+---+---+---+---+
     * | P |  DPL  | S |    GateType   |
     * +---+---+---+---+---+---+---+---+
     * P: Present
     * DPL: Descriptor Privilege Level
     * S: Storage segment
     * Type: 0xE for interrupt gate
     */

    /* register default handlers */
    install_isr( 0, 0x8E, 0x08, isr0 );
    install_isr( 1, 0x8E, 0x08, isr1 );
    install_isr( 2, 0x8E, 0x08, isr2 );
    install_isr( 3, 0x8E, 0x08, isr3 );
    install_isr( 4, 0x8E, 0x08, isr4 );
    install_isr( 5, 0x8E, 0x08, isr5 );
    install_isr( 6, 0x8E, 0x08, isr6 );
    install_isr( 7, 0x8E, 0x08, isr7 );
    install_isr( 8, 0x8E, 0x08, isr8 );
    install_isr( 9, 0x8E, 0x08, isr9 );
    install_isr(10, 0x8E, 0x08, isr10);
    install_isr(11, 0x8E, 0x08, isr11);
    install_isr(12, 0x8E, 0x08, isr12);
    install_isr(13, 0x8E, 0x08, isr13);
    install_isr(14, 0x8E, 0x08, isr14);
    install_isr(15, 0x8E, 0x08, isr15);
    install_isr(16, 0x8E, 0x08, isr16);
    install_isr(17, 0x8E, 0x08, isr17);
    install_isr(18, 0x8E, 0x08, isr18);
    install_isr(19, 0x8E, 0x08, isr19);
    install_isr(20, 0x8E, 0x08, isr20);
    install_isr(21, 0x8E, 0x08, isr21);
    install_isr(22, 0x8E, 0x08, isr22);
    install_isr(23, 0x8E, 0x08, isr23);
    install_isr(24, 0x8E, 0x08, isr24);
    install_isr(25, 0x8E, 0x08, isr25);
    install_isr(26, 0x8E, 0x08, isr26);
    install_isr(27, 0x8E, 0x08, isr27);
    install_isr(28, 0x8E, 0x08, isr28);
    install_isr(29, 0x8E, 0x08, isr29);
    install_isr(30, 0x8E, 0x08, isr30);
    install_isr(31, 0x8E, 0x08, isr31);

    for(uint16_t i = 32; i < MAX_INTERRUPTS; i++)
    {
        install_isr(i, 0x8E, 0x08, isr_handler);
    }
    
    /* install IDTR */
    idt_install();

    irq_init();

    register_isr_handler(1, &singlestep_handler);

#ifndef __x86_64__
    register_isr_handler(7, &fpu_handler);
#endif

    register_isr_handler(13, &gpf_handler);
    register_isr_handler(14, &page_fault_handler);
}

