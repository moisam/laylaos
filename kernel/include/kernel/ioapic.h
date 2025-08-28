/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: ioapic.h
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
 *  \file ioapic.h
 *
 *  Macros and function declarations for working with I/O Advanced Programmable
 *  Interrup Controllers (I/O APIC).
 */

#ifndef KERNEL_IOAPIC_H
#define KERNEL_IOAPIC_H

#define MAX_IOAPIC                      32

#define IOAPIC_ACTIVE_HIGH_LOW          (1 << 1)
#define IOAPIC_TRIGGER_EDGE_LOW         (1 << 3)


/***********************
 * Function prototypes
 ***********************/

//void ioapic_init(uintptr_t phys_base);
void ioapic_disable_irq(uint32_t i);
void ioapic_enable_irq(uint32_t i);
void ioapic_add(uint32_t int_base, uint32_t phys_base);
void ioapic_redirect_legacy_irqs(void);

#endif      /* KERNEL_IOAPIC_H */
