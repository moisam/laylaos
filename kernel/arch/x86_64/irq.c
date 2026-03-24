/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
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
#include <gui/vbe.h>            // screen_refresh()

struct handler_t *interrupt_handlers[MAX_INTERRUPTS];

struct irq_redir_t irq_redir[16] =
{
    { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 },
    { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 },
    { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 },
    { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 }, { 0xFFFFFFFF, 0 },
};


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
 * Allocate an IRQ handler.
 */
struct handler_t *irq_handler_alloc(int (*func)(struct regs *, void *), 
                                    void *arg, char *name)
{
    char *p;
    struct handler_t *h = kmalloc(sizeof(struct handler_t));
    
    if(!h)
    {
        kpanic("insufficient memory for IRQ handler\n");
    }

    h->handler = func;
    h->handler_arg = arg;
    //h->hits = 0;
    //h->ticks = 0;
    h->next = NULL;

    p = h->short_name;

    while((*p++ = *name++))
    {
        ;
    }
    
    return h;
}


/*
uint32_t irq_remap(uint32_t irq)
{
    if(irq_redir[irq] != 0xFFFFFFFF)
    {
        return irq_redir[irq];
    }

    return irq;
}
*/


void reserve_irq_range(unsigned int start, unsigned int end)
{
    uintptr_t s = int_off();
    volatile uint32_t *irqs = processor_local_data[0].irq_map;
    volatile unsigned int i;

    for(i = start; i <= end; i++)
    {
        irqs[i / 32] |= (1 << (i % 32));
    }

    int_on(s);
}


unsigned int alloc_irq_vector(void)
{
    uintptr_t s = int_off();
    volatile uint32_t *irqs = processor_local_data[0].irq_map;
    volatile unsigned int i, j;

    for(i = 0; i < 8; i++)
    {
        for(j = 0; j < 32; j++)
        {
            if((irqs[i] & (1 << j)) == 0)
            {
                irqs[i] |= (1 << j);
                int_on(s);

                return (i * 32) + j;
            }
        }
    }

    int_on(s);

    return -1;
}


/*
 * Initialise IRQs.
 */
void irq_init(void)
{
    memset(interrupt_handlers, 0, sizeof(interrupt_handlers));

    pic_init(0x20, 0x28);

    timer_init();
    ps2_init();
}

