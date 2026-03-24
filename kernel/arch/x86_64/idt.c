/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
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
#include <kernel/user.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kstack.h>
#include <mm/memregion.h>
#include <mm/mmap.h>
#include <gui/vbe.h>

#include <fs/dentry.h>


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
    int intr(struct regs *r, void *arg);        \
    struct handler_t intr##_handler = { .handler = intr, };

INT_HANDLER(page_fault)
INT_HANDLER(singlestep)
INT_HANDLER(gpf)
INT_HANDLER(division_by_zero)
//INT_HANDLER(ill_opcode)


/*
 * Default handler.
 */
void isr_handler(struct regs *r)
{
    char *s;
    unsigned long long oticks = ticks;
    uint8_t int_no = (r->int_no & 0xFF);
    struct handler_t *h;

    for(h = interrupt_handlers[int_no]; h; h = h->next)
    {
        if(h->handler(r, h->handler_arg))
        {
            cli();
            this_core->irq_count[int_no]++;
            this_core->irq_ticks[int_no] += (ticks - oticks);

            return;
        }
    }

    if(int_no >= 32)
    {
        printk("Unhandled IRQ %d\n", int_no - 32);
        pic_send_eoi(int_no - 32);
        return;
    }

    if(int_no <= 18)
    {
        s = intstr[int_no];
    }
    else if(int_no <= 31)
    {
        s = intstr[19];
    }
    else
    {
        s = intstr[20];
    }

    switch_tty(1);

    __asm__ __volatile__("xchg %%bx, %%bx"::);
    printk("\nUnhandled Interrupt: int %d (%s) - err 0x%x\n", int_no, s, r->err_code);

    if(this_core->cur_task)
    {
        printk("Current task (%d - %s)\n", 
                this_core->cur_task->pid, 
                this_core->cur_task->command);
    }

    dump_regs(r);

    screen_refresh(NULL);
    __asm__ __volatile__("xchg %%bx, %%bx"::);

    empty_loop();
}


/*
 * Divison by zero interrupt handler
 */
int division_by_zero(struct regs *r, void *arg)
{
    UNUSED(arg);
    
    if(!this_core->cur_task || !this_core->cur_task->user)
    {
        kpanic("Divison by zero in kernel space!");
    }
    
    // user task
    // kill the task and force signal dispatch
    __asm__ __volatile__("xchg %%bx, %%bx"::);

#ifdef __x86_64__
    add_task_fpe_signal(this_core->cur_task, FPE_INTDIV, (void *)r->rip);
#else
    add_task_fpe_signal(this_core->cur_task, FPE_INTDIV, (void *)r->eip);
#endif      /* __x86_64__ */

    check_pending_signals(r);
    return 1;
}


/*
 * Single-step debug interrupt handler
 */
int singlestep(struct regs *r, void *arg)
{
    UNUSED(arg);

#ifdef __x86_64__
    r->rflags |= 0x100;
#else
    r->eflags |= 0x100;
#endif      /* __x86_64__ */

    __asm__ __volatile__("xchg %%bx, %%bx"::);

    if(this_core->cur_task->properties & PROPERTY_TRACE_SIGNALS)
    {
        ptrace_signal(SIGTRAP, PTRACE_EVENT_STOP /* PTRACE_EVENT_SINGLESTEP */);
    }
    
    return 1;
}


/*
 * General Protection Fault interrupt handler
 */
int gpf(struct regs *r, void *arg)
{
    UNUSED(arg);
    
    if(!this_core->cur_task || !this_core->cur_task->user)
    {
        switch_tty(1);

        if(this_core->cur_task)
        {
            printk("Current task (%d - %s)\n", this_core->cur_task->pid, this_core->cur_task->command);
        }

        dump_regs(r);
        screen_refresh(NULL);
        kpanic("General protection fault in kernel space!");
    }
    
    // user task
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    // if the GPF happened in the pagefault handler, the task would die
    // holding its own memory lock, so we need to unlock it here otherwise
    // terminate_task() will panic
    if(this_core->cur_task->mem->mutex.holder &&
       this_core->cur_task->mem->mutex.holder == this_core->cur_task)
    {
        kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));
    }

    // kill the task and force signal dispatch

#ifdef __x86_64__
    add_task_segv_signal(this_core->cur_task, SEGV_ACCERR, (void *)r->rip);
#else
    add_task_segv_signal(this_core->cur_task, SEGV_ACCERR, (void *)r->eip);
#endif      /* __x86_64__ */



    switch_tty(1);
    printk("\nGPF: int %d  err 0x%x\n", r->int_no, r->err_code);

    if(this_core->cur_task)
    {
        printk("Current task (%d - %s)\n", this_core->cur_task->pid, this_core->cur_task->command);
    }

    char *p = (char *)r->rip;
    int i;

    printk("\nbytes before: ");
    for(i = 8; i > 0; i--) printk("0x%02x ", p[-i]);
    printk("\nbytes after : ");
    for(i = 0; i < 8; i++) printk("0x%02x ", p[i]);
    printk("\n\n");

    for(struct memregion_t *tmp = this_core->cur_task->mem->first_region; tmp != NULL; tmp = tmp->next)
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
            printk("           path '%s'\n", path);
            break;
        }
    }

    dump_regs(r);
    screen_refresh(NULL);
    __asm__ __volatile__("xchg %%bx, %%bx"::);
    kpanic("_______-----------\n");

    empty_loop();



    check_pending_signals(r);
    __asm__ __volatile__("xchg %%bx, %%bx"::);
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

    // register default ISR handlers
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

    // register default IRQ handlers
    install_isr(32, 0x8E, 0x08, isr32);
    install_isr(33, 0x8E, 0x08, isr33);
    install_isr(34, 0x8E, 0x08, isr34);
    install_isr(35, 0x8E, 0x08, isr35);
    install_isr(36, 0x8E, 0x08, isr36);
    install_isr(37, 0x8E, 0x08, isr37);
    install_isr(38, 0x8E, 0x08, isr38);
    install_isr(39, 0x8E, 0x08, isr39);
    install_isr(40, 0x8E, 0x08, isr40);
    install_isr(41, 0x8E, 0x08, isr41);
    install_isr(42, 0x8E, 0x08, isr42);
    install_isr(43, 0x8E, 0x08, isr43);
    install_isr(44, 0x8E, 0x08, isr44);
    install_isr(45, 0x8E, 0x08, isr45);
    install_isr(46, 0x8E, 0x08, isr46);
    install_isr(47, 0x8E, 0x08, isr47);
    install_isr(48, 0x8E, 0x08, isr48);
    install_isr(49, 0x8E, 0x08, isr49);
    install_isr(50, 0x8E, 0x08, isr50);
    install_isr(51, 0x8E, 0x08, isr51);
    install_isr(52, 0x8E, 0x08, isr52);
    install_isr(53, 0x8E, 0x08, isr53);
    install_isr(54, 0x8E, 0x08, isr54);
    install_isr(55, 0x8E, 0x08, isr55);
    install_isr(56, 0x8E, 0x08, isr56);
    install_isr(57, 0x8E, 0x08, isr57);
    install_isr(58, 0x8E, 0x08, isr58);
    install_isr(59, 0x8E, 0x08, isr59);
    install_isr(60, 0x8E, 0x08, isr60);
    install_isr(61, 0x8E, 0x08, isr61);
    install_isr(62, 0x8E, 0x08, isr62);
    install_isr(63, 0x8E, 0x08, isr63);
    install_isr(64, 0x8E, 0x08, isr64);
    install_isr(65, 0x8E, 0x08, isr65);
    install_isr(66, 0x8E, 0x08, isr66);
    install_isr(67, 0x8E, 0x08, isr67);
    install_isr(68, 0x8E, 0x08, isr68);
    install_isr(69, 0x8E, 0x08, isr69);
    install_isr(70, 0x8E, 0x08, isr70);
    install_isr(71, 0x8E, 0x08, isr71);
    install_isr(72, 0x8E, 0x08, isr72);
    install_isr(73, 0x8E, 0x08, isr73);
    install_isr(74, 0x8E, 0x08, isr74);
    install_isr(75, 0x8E, 0x08, isr75);
    install_isr(76, 0x8E, 0x08, isr76);
    install_isr(77, 0x8E, 0x08, isr77);
    install_isr(78, 0x8E, 0x08, isr78);
    install_isr(79, 0x8E, 0x08, isr79);
    install_isr(80, 0x8E, 0x08, isr80);
    install_isr(81, 0x8E, 0x08, isr81);
    install_isr(82, 0x8E, 0x08, isr82);
    install_isr(83, 0x8E, 0x08, isr83);
    install_isr(84, 0x8E, 0x08, isr84);
    install_isr(85, 0x8E, 0x08, isr85);
    install_isr(86, 0x8E, 0x08, isr86);
    install_isr(87, 0x8E, 0x08, isr87);
    install_isr(88, 0x8E, 0x08, isr88);
    install_isr(89, 0x8E, 0x08, isr89);
    install_isr(90, 0x8E, 0x08, isr90);
    install_isr(91, 0x8E, 0x08, isr91);
    install_isr(92, 0x8E, 0x08, isr92);
    install_isr(93, 0x8E, 0x08, isr93);
    install_isr(94, 0x8E, 0x08, isr94);
    install_isr(95, 0x8E, 0x08, isr95);
    install_isr(96, 0x8E, 0x08, isr96);
    install_isr(97, 0x8E, 0x08, isr97);
    install_isr(98, 0x8E, 0x08, isr98);
    install_isr(99, 0x8E, 0x08, isr99);
    install_isr(100, 0x8E, 0x08, isr100);
    install_isr(101, 0x8E, 0x08, isr101);
    install_isr(102, 0x8E, 0x08, isr102);
    install_isr(103, 0x8E, 0x08, isr103);
    install_isr(104, 0x8E, 0x08, isr104);
    install_isr(105, 0x8E, 0x08, isr105);
    install_isr(106, 0x8E, 0x08, isr106);
    install_isr(107, 0x8E, 0x08, isr107);
    install_isr(108, 0x8E, 0x08, isr108);
    install_isr(109, 0x8E, 0x08, isr109);
    install_isr(110, 0x8E, 0x08, isr110);
    install_isr(111, 0x8E, 0x08, isr111);
    install_isr(112, 0x8E, 0x08, isr112);
    install_isr(113, 0x8E, 0x08, isr113);
    install_isr(114, 0x8E, 0x08, isr114);
    install_isr(115, 0x8E, 0x08, isr115);
    install_isr(116, 0x8E, 0x08, isr116);
    install_isr(117, 0x8E, 0x08, isr117);
    install_isr(118, 0x8E, 0x08, isr118);
    install_isr(119, 0x8E, 0x08, isr119);
    install_isr(120, 0x8E, 0x08, isr120);
    install_isr(121, 0x8E, 0x08, isr121);
    install_isr(122, 0x8E, 0x08, isr122);

    install_isr(127, 0x8E, 0x08, isr127);
    install_isr(128, 0x8E, 0x08, isr128);
    install_isr(129, 0x8E, 0x08, isr129);
    install_isr(130, 0x8E, 0x08, isr130);
    install_isr(131, 0x8E, 0x08, isr131);
    install_isr(132, 0x8E, 0x08, isr132);
    install_isr(133, 0x8E, 0x08, isr133);
    install_isr(134, 0x8E, 0x08, isr134);
    install_isr(135, 0x8E, 0x08, isr135);
    install_isr(136, 0x8E, 0x08, isr136);
    install_isr(137, 0x8E, 0x08, isr137);
    install_isr(138, 0x8E, 0x08, isr138);
    install_isr(139, 0x8E, 0x08, isr139);
    install_isr(140, 0x8E, 0x08, isr140);
    install_isr(141, 0x8E, 0x08, isr141);
    install_isr(142, 0x8E, 0x08, isr142);
    install_isr(143, 0x8E, 0x08, isr143);
    install_isr(144, 0x8E, 0x08, isr144);
    install_isr(145, 0x8E, 0x08, isr145);
    install_isr(146, 0x8E, 0x08, isr146);
    install_isr(147, 0x8E, 0x08, isr147);
    install_isr(148, 0x8E, 0x08, isr148);
    install_isr(149, 0x8E, 0x08, isr149);
    install_isr(150, 0x8E, 0x08, isr150);
    install_isr(151, 0x8E, 0x08, isr151);
    install_isr(152, 0x8E, 0x08, isr152);
    install_isr(153, 0x8E, 0x08, isr153);
    install_isr(154, 0x8E, 0x08, isr154);
    install_isr(155, 0x8E, 0x08, isr155);
    install_isr(156, 0x8E, 0x08, isr156);
    install_isr(157, 0x8E, 0x08, isr157);
    install_isr(158, 0x8E, 0x08, isr158);
    install_isr(159, 0x8E, 0x08, isr159);
    install_isr(160, 0x8E, 0x08, isr160);
    install_isr(161, 0x8E, 0x08, isr161);
    install_isr(162, 0x8E, 0x08, isr162);
    install_isr(163, 0x8E, 0x08, isr163);
    install_isr(164, 0x8E, 0x08, isr164);
    install_isr(165, 0x8E, 0x08, isr165);
    install_isr(166, 0x8E, 0x08, isr166);
    install_isr(167, 0x8E, 0x08, isr167);
    install_isr(168, 0x8E, 0x08, isr168);
    install_isr(169, 0x8E, 0x08, isr169);
    install_isr(170, 0x8E, 0x08, isr170);
    install_isr(171, 0x8E, 0x08, isr171);
    install_isr(172, 0x8E, 0x08, isr172);
    install_isr(173, 0x8E, 0x08, isr173);
    install_isr(174, 0x8E, 0x08, isr174);
    install_isr(175, 0x8E, 0x08, isr175);
    install_isr(176, 0x8E, 0x08, isr176);
    install_isr(177, 0x8E, 0x08, isr177);
    install_isr(178, 0x8E, 0x08, isr178);
    install_isr(179, 0x8E, 0x08, isr179);
    install_isr(180, 0x8E, 0x08, isr180);
    install_isr(181, 0x8E, 0x08, isr181);
    install_isr(182, 0x8E, 0x08, isr182);
    install_isr(183, 0x8E, 0x08, isr183);
    install_isr(184, 0x8E, 0x08, isr184);
    install_isr(185, 0x8E, 0x08, isr185);
    install_isr(186, 0x8E, 0x08, isr186);
    install_isr(187, 0x8E, 0x08, isr187);
    install_isr(188, 0x8E, 0x08, isr188);
    install_isr(189, 0x8E, 0x08, isr189);
    install_isr(190, 0x8E, 0x08, isr190);
    install_isr(191, 0x8E, 0x08, isr191);
    install_isr(192, 0x8E, 0x08, isr192);
    install_isr(193, 0x8E, 0x08, isr193);
    install_isr(194, 0x8E, 0x08, isr194);
    install_isr(195, 0x8E, 0x08, isr195);
    install_isr(196, 0x8E, 0x08, isr196);
    install_isr(197, 0x8E, 0x08, isr197);
    install_isr(198, 0x8E, 0x08, isr198);
    install_isr(199, 0x8E, 0x08, isr199);
    install_isr(200, 0x8E, 0x08, isr200);
    install_isr(201, 0x8E, 0x08, isr201);
    install_isr(202, 0x8E, 0x08, isr202);
    install_isr(203, 0x8E, 0x08, isr203);
    install_isr(204, 0x8E, 0x08, isr204);
    install_isr(205, 0x8E, 0x08, isr205);
    install_isr(206, 0x8E, 0x08, isr206);
    install_isr(207, 0x8E, 0x08, isr207);
    install_isr(208, 0x8E, 0x08, isr208);
    install_isr(209, 0x8E, 0x08, isr209);
    install_isr(210, 0x8E, 0x08, isr210);
    install_isr(211, 0x8E, 0x08, isr211);
    install_isr(212, 0x8E, 0x08, isr212);
    install_isr(213, 0x8E, 0x08, isr213);
    install_isr(214, 0x8E, 0x08, isr214);
    install_isr(215, 0x8E, 0x08, isr215);
    install_isr(216, 0x8E, 0x08, isr216);
    install_isr(217, 0x8E, 0x08, isr217);
    install_isr(218, 0x8E, 0x08, isr218);
    install_isr(219, 0x8E, 0x08, isr219);
    install_isr(220, 0x8E, 0x08, isr220);
    install_isr(221, 0x8E, 0x08, isr221);
    install_isr(222, 0x8E, 0x08, isr222);
    install_isr(223, 0x8E, 0x08, isr223);
    install_isr(224, 0x8E, 0x08, isr224);
    install_isr(225, 0x8E, 0x08, isr225);
    install_isr(226, 0x8E, 0x08, isr226);
    install_isr(227, 0x8E, 0x08, isr227);
    install_isr(228, 0x8E, 0x08, isr228);
    install_isr(229, 0x8E, 0x08, isr229);
    install_isr(230, 0x8E, 0x08, isr230);
    install_isr(231, 0x8E, 0x08, isr231);
    install_isr(232, 0x8E, 0x08, isr232);
    install_isr(233, 0x8E, 0x08, isr233);
    install_isr(234, 0x8E, 0x08, isr234);
    install_isr(235, 0x8E, 0x08, isr235);
    install_isr(236, 0x8E, 0x08, isr236);
    install_isr(237, 0x8E, 0x08, isr237);
    install_isr(238, 0x8E, 0x08, isr238);
    install_isr(239, 0x8E, 0x08, isr239);
    install_isr(240, 0x8E, 0x08, isr240);
    install_isr(241, 0x8E, 0x08, isr241);
    install_isr(242, 0x8E, 0x08, isr242);
    install_isr(243, 0x8E, 0x08, isr243);
    install_isr(244, 0x8E, 0x08, isr244);
    install_isr(245, 0x8E, 0x08, isr245);
    install_isr(246, 0x8E, 0x08, isr246);
    install_isr(247, 0x8E, 0x08, isr247);
    install_isr(248, 0x8E, 0x08, isr248);
    install_isr(249, 0x8E, 0x08, isr249);
    install_isr(250, 0x8E, 0x08, isr250);
    install_isr(251, 0x8E, 0x08, isr251);
    install_isr(252, 0x8E, 0x08, isr252);
    install_isr(253, 0x8E, 0x08, isr253);
    install_isr(254, 0x8E, 0x08, isr254);

    // clock IRQ for other processors (not the boot processor)
    install_isr(123, 0x8E, 0x08, isr123);

    // bad TLB shootdown
    install_isr(124, 0x8E, 0x08, isr124);

    // halt everybody
    install_isr(125, 0x8E, 0x08, isr125);

    // wakeup from idle task
    install_isr(126, 0x8E, 0x08, isr126);

    // spurious interrupt
    install_isr(0xFF, 0x8E, 0x08, isr255);

    /* install IDTR */
    idt_install();

    irq_init();

    register_interrupt_handler(0, &division_by_zero_handler);
    register_interrupt_handler(1, &singlestep_handler);

#ifndef __x86_64__
    register_interrupt_handler(7, &fpu_handler);
#endif

    register_interrupt_handler(13, &gpf_handler);
    register_interrupt_handler(14, &page_fault_handler);
}

