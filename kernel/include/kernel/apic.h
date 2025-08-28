/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: apic.h
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
 *  \file apic.h
 *
 *  Macros and function declarations for working with Advanced Programmable
 *  Interrup Controllers (APIC).
 */

#ifndef KERNEL_APIC_H
#define KERNEL_APIC_H

/*
 * Local APIC register offsets
 */
#define LAPIC_REG_ID                    0x020
#define LAPIC_REG_VERSION               0x030
#define LAPIC_REG_TASK_PRIO             0x080
#define LAPIC_REG_ARBITRATION_PRIO      0x090
#define LAPIC_REG_PROCESSOR_PRIO        0x0A0
#define LAPIC_REG_EOI                   0x0B0
#define LAPIC_REG_REMOTE_READ           0x0C0
#define LAPIC_REG_LOGICAL_DEST          0x0D0
#define LAPIC_REG_DEST_FORMAT           0x0E0
#define LAPIC_REG_SPURIOUS_INT_VECT     0x0F0
#define LAPIC_REG_ERR_STATUS            0x280
#define LAPIC_REG_CMCI                  0x2F0
#define LAPIC_REG_ICRL                  0x300
#define LAPIC_REG_ICRH                  0x310
#define LAPIC_REG_LVT_TIMER             0x320
#define LAPIC_REG_THERM_SENSOR          0x330
#define LAPIC_REG_LVT_PERF_MONITOR      0x340
#define LAPIC_REG_LVT_LINT0             0x350
#define LAPIC_REG_LVT_LINT1             0x360
#define LAPIC_REG_LVT_ERR               0x370
#define LAPIC_REG_INIT_COUNT            0x380
#define LAPIC_REG_CUR_COUNT             0x390
#define LAPIC_REG_DIVIDE_CONFIG         0x3E0


#define APIC_DISABLE                    0x10000
#define APIC_SW_ENABLE                  0x100
#define APIC_MSR_ENABLE                 0x800
#define APIC_CPU_FOCUS                  0x200
#define APIC_NMI                        (4 << 8)

#define APIC_PERIODIC                   0x20000


extern uintptr_t lapic_virt;
extern uintptr_t lapic_phys;
extern volatile int apic_running;


/***********************
 * Function prototypes
 ***********************/

void apic_init(void);
int lapic_cur_cpu(void);
void lapic_timer_init(int timer_irq);

#endif      /* KERNEL_APIC_H */
