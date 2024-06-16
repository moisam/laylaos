/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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
#include <kernel/io.h>


/*
 * Initialise the PIC.
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
}


/*
void pic_disable()
{
  __asm__ __volatile__(
    "mov $0xFF, %%al\n\
     out %%al, $0xA1\n\
     out %%al, $0x21":::);
}
*/


/*
 * Enable an IRQ.
 */
void enable_irq(unsigned char irq_line)
{
    uint16_t port;
    uint8_t val;
    
    if(irq_line < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    
    val = inb(port) & ~(1 << irq_line);
    outb(port, val);
}


/*
 * Disable an IRQ.
 */
void disable_irq(unsigned char irq_line)
{
    uint16_t port;
    uint8_t val;
    
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

