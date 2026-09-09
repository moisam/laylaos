/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
 * 
 *    file: pic.c
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
 *  \file pic.c
 *
 *  The 8259 Programmable Interrupt Controller (PIC) device driver 
 *  implementation.
 */

#include <kernel/pic.h>
#include <kernel/ioapic.h>
#include <kernel/io.h>
#include <kernel/irq.h>


/*
 * Initialize the PIC.
 */
void pic_init(int offset0, int offset1)
{
    /* send ICW1 */
    outb(PIC1_COMMAND, PIC_ICW1_INIT+PIC_ICW1_ICW4);
    PIC_WAIT();
    outb(PIC2_COMMAND, PIC_ICW1_INIT+PIC_ICW1_ICW4);
    PIC_WAIT();

    /* send ICW2 */
    outb(PIC1_DATA, offset0);
    PIC_WAIT();
    outb(PIC2_DATA, offset1);
    PIC_WAIT();

    /* send ICW3 */
    outb(PIC1_DATA, 4);	//tell PIC1 we have a slave PIC2 at IRQ2
    PIC_WAIT();
    outb(PIC2_DATA, 2);	//tell PIC2 its cascade id
    PIC_WAIT();

    /* send ICW4 */
    outb(PIC1_DATA, PIC_ICW4_8086);
    PIC_WAIT();
    outb(PIC2_DATA, PIC_ICW4_8086);
    PIC_WAIT();

    /* unmask both PICs */
    outb(PIC1_DATA, 0);
    outb(PIC2_DATA, 0);
}


/*
#define PIC_READ_IRR    0x0A

uint16_t get_pending_irqs(void)
{
    // Send command to read IRR
    outb(PIC1_COMMAND, PIC_READ_IRR);
    outb(PIC2_COMMAND, PIC_READ_IRR);

    // Read IRR from both PICs
    uint8_t master_irr = inb(PIC1_COMMAND);
    uint8_t slave_irr = inb(PIC2_COMMAND);

    // Combine into a single 16-bit value
    return ((uint16_t)slave_irr << 8) | master_irr;
}
*/


void pic_disable(void)
{
    //pic_init(0x20, 0x28);   // remap IRQs
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}


/*
 * Enable an IRQ.
 */
void enable_irq(unsigned char irq_line, uint16_t apic_flags)
{
    uint16_t port, elcr_port;
    uint8_t val;

    if(apic_running)
    {
        //ioapic_enable_irq(irq_remap(irq_line), 0x20 + irq_line);
        ioapic_enable_irq(irq_line, apic_flags);
        return;
    }
    
    if(irq_line < 8)
    {
        port = PIC1_DATA;
        elcr_port = 0x4D0;
    }
    else
    {
        port = PIC2_DATA;
        elcr_port = 0x4D1;
        irq_line -= 8;
    }

    val = inb(port) & ~(1 << irq_line);
    outb(port, val);

    // if the IRQ is level triggered (e.g. PCI on IRQ 9), handle this by
    // setting the ELCR (Edge/Level Control Register)
    // See: https://davmac.org/osdev/pchwpe/i8259.html
    if(apic_flags & IOAPIC_LEVEL_TRIGGER)
    {
        val = inb(elcr_port) | (1 << irq_line);
        outb(elcr_port, val);
    }
}


/*
 * Disable an IRQ.
 */
void disable_irq(unsigned char irq_line)
{
    uint16_t port;
    uint8_t val;

    if(apic_running)
    {
        //ioapic_disable_irq(irq_remap(irq_line));
        ioapic_disable_irq(irq_line);
        return;
    }
    
    if(irq_line < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    
    val = inb(port) | (1 << irq_line);
    outb(port, val);
}

