/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: irq.c
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
 *  \file irq.c
 *
 *  Functions to initialise, register and unregister IRQ handlers.
 */

#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/pic.h>
#include <kernel/idt.h>
#include <kernel/irq.h>
#include <kernel/timer.h>
#include <kernel/kbd.h>
#include <kernel/mouse.h>
//#include <kernel/fpu.h>
#include <kernel/asm.h>
#include <mm/kheap.h>

volatile int nested_irqs = 0;
struct handler_t *interrupt_handlers[256];


/*
 * IRQ handler switch function.
 */
void irq_handler(struct regs *r)
{
    struct handler_t *h;
    uint8_t int_no = (r->int_no & 0xFF);
    //int done = 0;
    unsigned long long oticks = ticks;
    
    nested_irqs++;
    
    for(h = interrupt_handlers[int_no]; h; h = h->next)
    {
        if(h->handler(r, h->handler_arg))
        {
            //if(int_no == 43) printk("Handled IRQ %d (%s)\n", int_no - 32, h->short_name);
            h->hits++;
            // TODO: this does not account for round over
            h->ticks += (ticks - oticks);
            //done = 1;
            //break;
            nested_irqs--;
            return;
        }
    }
    
    //if(!done)
    {
        printk("Unhandled IRQ %d\n", int_no - 32);
        //screen_refresh(NULL);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        pic_send_eoi(int_no - 32);
    }
    
    nested_irqs--;
}


static inline
void register_interrupt_handler(int n, struct handler_t *handler)
{
    struct handler_t *h;
    
    if(interrupt_handlers[n] == NULL)
    {
        interrupt_handlers[n] = handler;
    }
    else
    {
        h = interrupt_handlers[n];
        
        while(h->next)
        {
            h = h->next;
        }
        
        h->next = handler;
    }
    
    handler->next = NULL;
}


/*
 * Register IRQ handler.
 */
void register_irq_handler(int n, struct handler_t *handler)
{
    register_interrupt_handler(n + 32, handler);
}


/*
 * Register ISR handler.
 */
void register_isr_handler(int n, struct handler_t *handler)
{
    register_interrupt_handler(n, handler);
}


static inline
void unregister_interrupt_handler(int n, struct handler_t *handler)
{
    if(interrupt_handlers[n] == NULL)
    {
        return;
    }
    
    int s = int_off();
    struct handler_t *h;
    
    if(interrupt_handlers[n] == handler)
    {
        interrupt_handlers[n] = handler->next;
    }
    else
    {
        h = interrupt_handlers[n];

        while(h->next)
        {
            if(h->next == handler)
            {
                h->next = handler->next;
                handler->next = NULL;
                int_on(s);
                return;
            }
            
            h = h->next;
        }
    }
    
    int_on(s);
}


/*
 * Unregister IRQ handler.
 */
void unregister_irq_handler(int n, struct handler_t *handler)
{
    unregister_interrupt_handler(n + 32, handler);
}


/*
 * Unregister ISR handler.
 */
void unregister_isr_handler(int n, struct handler_t *handler)
{
    unregister_interrupt_handler(n, handler);
}


/*
 * Allocate an IRQ handler.
 */
struct handler_t *irq_handler_alloc(int (*func)(struct regs *, int), 
                                    int arg, char *name)
{
    char *p;
    struct handler_t *h = kmalloc(sizeof(struct handler_t));
    
    if(!h)
    {
        kpanic("insufficient memory for IRQ handler\n");
    }

    h->handler = func;
    h->handler_arg = arg;
    h->hits = 0;
    h->ticks = 0;
    h->next = NULL;
    
    p = h->short_name;

    while((*p++ = *name++))
    {
        ;
    }
    
    return h;
}


/*
 * Initialise IRQs.
 */
void irq_init(void)
{
    memset(interrupt_handlers, 0, sizeof(interrupt_handlers));

    pic_init(0x20, 0x28);

    install_isr(32, 0x8E, 0x08, irq0);	//IRQ 0
    install_isr(33, 0x8E, 0x08, irq1);
    install_isr(34, 0x8E, 0x08, irq2);
    install_isr(35, 0x8E, 0x08, irq3);
    install_isr(36, 0x8E, 0x08, irq4);
    install_isr(37, 0x8E, 0x08, irq5);
    install_isr(38, 0x8E, 0x08, irq6);
    install_isr(39, 0x8E, 0x08, irq7);
    install_isr(40, 0x8E, 0x08, irq8);
    install_isr(41, 0x8E, 0x08, irq9);
    install_isr(42, 0x8E, 0x08, irq10);
    install_isr(43, 0x8E, 0x08, irq11);
    install_isr(44, 0x8E, 0x08, irq12);
    install_isr(45, 0x8E, 0x08, irq13);
    install_isr(46, 0x8E, 0x08, irq14);
    install_isr(47, 0x8E, 0x08, irq15);
    
    timer_init();
    ps2_init();
}

