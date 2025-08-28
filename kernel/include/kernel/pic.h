/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: pic.h
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
 *  \file pic.h
 *
 *  Functions to work with the 8259 Programmable Interrupt Controller (PIC).
 */

#ifndef __PIC_H__
#define __PIC_H__

#include <stdint.h>
#include <kernel/io.h>
#include <kernel/apic.h>

/* controller registers */
/* PIC1&2 register port addresses */
#define PIC1_COMMAND        0X20
#define PIC1_DATA           0X21
#define PIC2_COMMAND        0XA0
#define PIC2_DATA           0XA1

/* End of Interrupt command code */
#define PIC_EOI             0x20

/* Interrupt Command Words */
/* ICW1 bit masks */
#define PIC_ICW1_ICW4       0X01	//ICW4 needed
#define PIC_ICW1_SINGLE     0X02	//Single mode
#define PIC_ICW1_INTERVAL4  0X04	//Call addr interval 4
#define PIC_ICW1_LEVEL      0X08	//level triggered
#define PIC_ICW1_INIT       0X10	//init

/* ICW4 bit masks */
#define PIC_ICW4_8086       0X01	//8086/88 mode
#define PIC_ICW4_AUTO       0X02	//auto EOI
#define PIC_ICW4_BUF_SLAVE  0X08	//buffered mode/slave
#define PIC_ICW4_BUF_MASTER 0X0C	//buffered mode/master
#define PIC_ICW4_SFNM       0X10	//special fully nested

#define PIC_WAIT()                  \
    __asm__ __volatile__("jmp 1f\n" \
                         "1:\n"     \
                         "jmp 2f\n" \
                         "2:"::);

static inline void pic_send_eoi(unsigned char irq)
{
    if(apic_running)
    {
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_EOI)) = 0;
        return;
    }

    if(irq >= 8)
    {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    
    outb(PIC1_COMMAND, PIC_EOI);
}


/***********************
 * Function prototypes
 ***********************/

/**
 * @brief Initialize the PIC.
 *
 * Initialize the 8259 Programmable Interrupt Controller (PIC).
 *
 * @param   offset0     first offset to pass to the PIC
 * @param   offset1     second offset to pass to the PIC
 *
 * @return  nothing.
 */
void pic_init(int offset0, int offset1);

/**
 * @brief Disable the PIC.
 *
 * Disable the 8259 Programmable Interrupt Controller (PIC).
 *
 * @return  nothing.
 */
void pic_disable(void);

/**
 * @brief Enable an IRQ.
 *
 * Enable the given IRQ.
 *
 * @param   irq_line    IRQ to enable
 *
 * @return  nothing.
 */
void enable_irq(unsigned char irq_line);

/**
 * @brief Disable an IRQ.
 *
 * Disable the given IRQ.
 *
 * @param   irq_line    IRQ to disable
 *
 * @return  nothing.
 */
void disable_irq(unsigned char irq_line);

//void pic_disable();

#endif      /* __PIC_H__ */
