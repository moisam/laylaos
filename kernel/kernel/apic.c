/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: apic.c
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
 *  \file apic.c
 *
 *  Code to support Advanced Programmable Interrupt Controllers (APIC) in the kernel.
 */

#include <kernel/laylaos.h>
#include <kernel/kbd.h>
#include <kernel/mouse.h>
#include <kernel/apic.h>
#include <kernel/irq.h>
#include <kernel/msr.h>
#include <kernel/asm.h>
#include <kernel/io.h>
#include <kernel/pic.h>
#include <kernel/ioapic.h>
#include <gui/vbe.h>            // screen_refresh()

uintptr_t lapic_phys = 0;
uintptr_t lapic_virt = 0;
volatile int apic_running = 0;

int spurious_callback(struct regs *r, int arg);


struct handler_t spurious_handler =
{
    .handler = spurious_callback,
    .handler_arg = 0,
    .short_name = "spurious",
};


int spurious_callback(struct regs *r, int arg)
{
    UNUSED(r);
    UNUSED(arg);

    return 1;
}


void lapic_timer_init(int timer_irq)
{
    uint32_t i;
    uint64_t j;
    volatile uint8_t k;

    // Initialize LAPIC to a well known state
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_DEST_FORMAT)) = 0xFFFFFFFF;
    i = *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LOGICAL_DEST));
    i &= 0x00FFFFFF;
    i |= 1;
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LOGICAL_DEST)) = i;
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LVT_TIMER)) = APIC_DISABLE;
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LVT_PERF_MONITOR)) = APIC_NMI;
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LVT_LINT0)) = APIC_DISABLE;
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LVT_LINT1)) = APIC_DISABLE;
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_TASK_PRIO)) = 0;

    // Enable LAPIC
    // Global enable
    j = rdmsr(IA32_APIC_BASE_MSR);
    j |= (1 << 11);
    wrmsr(IA32_APIC_BASE_MSR, j);

    // Software enable, map spurious interrupt to dummy isr
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_SPURIOUS_INT_VECT)) = 
                                                        APIC_SW_ENABLE + 0xFF;

    // Map APIC timer to an interrupt, and by that enable it in one-shot mode
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LVT_TIMER)) = timer_irq;

    // Set up divide value to 16
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_DIVIDE_CONFIG)) = 3;

    // Initialize PIT Channel 2 in one-shot mode.
    // Waiting 1 sec could slow down boot time considerably,
    // so we'll wait 1/100 sec, and multiply the counted ticks.
    k = inb(0x61);
    k &= 0xdd;
    k |= 1;
    outb(0x61, k);

    // Write to the PIT Mode/Command register. The value 0xb2 means:
    //   Select Channel 2 (bits 6 & 7)
    //   Access mode: lobyte/hibyte (bits 4 & 5)
    //   Mode 1 (hardware re-triggerable one-shot) (bits 1 to 3)
    //   BCD/Binary mode (bit 0)
    outb(0x43, 0xb2);

    // 1193180/100 Hz = 11931 = 2e9bh
    outb(0x42, 0x9b);   // MSB to Channel 2 data port
    k = inb(0x60);      // short delay
    outb(0x42, 0x2e);   // LSB to Channel 2 data port
    k = inb(0x60);      // short delay

    // Reset PIT one-shot counter (start counting)
    k = inb(0x61);
    k &= 0xde;
    outb(0x61, k);      // gate low
    k |= 1;
    outb(0x61, k);      // gate high

    k = inb(0x61);

    // Reset APIC timer (set counter to -1)
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_INIT_COUNT)) = 0xFFFFFFFF;

    // Now wait until PIT counter reaches zero
    //printk("apic: loop -- k 0x%x\n", k);

    if((k & 0x20) == 0)
    {
        while((k & 0x20) == 0)
        {
            //i = *((volatile uint32_t *)(lapic_virt + LAPIC_REG_CUR_COUNT));
            k = inb(0x61);
            //printk("apic: loop -- counter value %u, k 0x%x\n", i, k);
        }
    }
    else
    {
        while((k & 0x20) != 0)
        {
            //i = *((volatile uint32_t *)(lapic_virt + LAPIC_REG_CUR_COUNT));
            k = inb(0x61);
            //printk("apic: loop -- counter value %u, k 0x%x\n", i, k);
        }
    }

    // Stop APIC timer
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LVT_TIMER)) = APIC_DISABLE;

    // Now do the math...
    // Get current counter value
    i = *((volatile uint32_t *)(lapic_virt + LAPIC_REG_CUR_COUNT));
    printk("apic: counter value %u\n", i);

    // It is counted down from -1, make it positive
    i = (0xFFFFFFFF - i) + 1;
    printk("apic: counter value +ve %u\n", i);

    // We used divide value different than 1, so now we have to multiply 
    // the result by 16
    i <<= 4;
    printk("apic: counter value << 4 = %u\n", i);

    // Moreover, PIT did not wait a whole sec, only a fraction, so multiply 
    // by that too
    i *= 100;
    printk("apic: counter value * 100 = %u\n", i);

    // Now calculate timer counter value of your choice.
    // This means that tasks will be preempted PIT_FREQUENCY times in a second
    // (currently defined as 100).
    i /= PIT_FREQUENCY;
    printk("apic: counter value / 100 = %u\n", i);

    // Again, we did not use divide value of 1
    i >>= 4;
    printk("apic: counter value >> 4 =  %u\n", i);

    // Sanity check, min 16
    if(i < 1600)
    {
        printk("apic: calculated frequency %u is too low.. fixing\n", i);
        i = 1600;
    }

    printk("apic: frequency %u ticks\n", i);
    screen_refresh(NULL);
    //for(;;);

    // Now i holds appropriate number of ticks, use it as APIC timer 
    // counter initializer
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_INIT_COUNT)) = i;

    // Finally re-enable timer in periodic mode
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_LVT_TIMER)) = timer_irq | APIC_PERIODIC;

    // Set divide value register again
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_DIVIDE_CONFIG)) = 3;

    apic_running = 1;
}


int lapic_cur_cpu(void)
{
    if(!lapic_virt)
    {
        return 0;
    }

    return (*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ID))) >> 24;
}


/*
void lapic_enable(void)
{
    uint64_t msr = rdmsr(IA32_APIC_BASE_MSR);
    uint32_t i;

    msr |= APIC_MSR_ENABLE;
    msr &= ~(1 << 10);
    wrmsr(IA32_APIC_BASE_MSR, msr);

    // Software enable, map spurious interrupt to dummy isr
    i = *((volatile uint32_t *)(lapic_virt + LAPIC_REG_SPURIOUS_INT_VECT));
    i |= (APIC_SW_ENABLE | 0xFF);
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_SPURIOUS_INT_VECT)) = i;
}
*/


void apic_init(void)
{
    if(!lapic_phys)
    {
        printk("apic: cannot find LAPIC base address..\n");
        return;
    }

    if(!(lapic_virt = mmio_map(lapic_phys, lapic_phys + PAGE_SIZE)))
    {
        printk("apic: failed to map base registers\n");
        return;
    }

    printk("apic: LAPIC at phys " _XPTR_ ", virt " _XPTR_ "\n", 
            lapic_phys, lapic_virt);

    // Register spurious IRQ handler
    register_isr_handler(0xFF, &spurious_handler);

    cli();

    pic_disable();
    //lapic_enable();
    //ioapic_redirect_legacy_irqs();
    lapic_timer_init(32);

    // we enabled these early on via the PIC, but now that we have APIC 
    // running, we need to redirect and enable them
    ioapic_enable_irq(IRQ_TIMER);
    ioapic_enable_irq(IRQ_MOUSE);
    ioapic_enable_irq(IRQ_KBD);
    ioapic_enable_irq(9);
    ioapic_enable_irq(11);
    ioapic_enable_irq(14);
    ioapic_enable_irq(15);

    sti();
}

