/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: ioapic.c
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
 *  \file ioapic.c
 *
 *  Code to support I/O Advanced Programmable Interrupt Controllers (I/O APIC)
 *  in the kernel.
 */

#include <kernel/laylaos.h>
#include <kernel/ioapic.h>
#include <kernel/apic.h>
#include <kernel/smp.h>
#include <kernel/irq.h>
#include <mm/mmngr_virtual.h>

// base registers
#define IOAPIC_REGSEL               0x00
#define IOAPIC_REGWIN               0x10

// register offsets for read/write
#define IOAPIC_REG_IOAPIC_ID        0x00
#define IOAPIC_REG_IOAPIC_VER       0x01
#define IOAPIC_REG_IOAPIC_ARB       0x02
#define IOAPIC_REG_IOREDTBL         0x10

// delivery modes
#define IOAPIC_DELIVERY_MODE_FIX    0x00
#define IOAPIC_DELIVERY_MODE_LOW    0x01
#define IOAPIC_DELIVERY_MODE_SMI    0x02
#define IOAPIC_DELIVERY_MODE_NMI    0x04
#define IOAPIC_DELIVERY_MODE_INIT   0x05
#define IOAPIC_DELIVERY_MODE_EXTINT 0x06

// destination modes
#define IOAPIC_DEST_PHYSICAL        0x00
#define IOAPIC_DEST_LOGICAL         0x01

//uintptr_t ioapic_base = 0;

int ioapic_count = 0;

struct ioapic_t
{
    uint8_t id, max_redirect;
    uint32_t phys_base, irq_base;
    uintptr_t virt_base;
};

struct ioapic_t ioapics[MAX_IOAPIC];

/*
struct ioapic_redirect_ent_t
{
    uint64_t vector:8;
    uint64_t delivery_mode:3;
    uint64_t dest_mode:1;
    uint64_t delivery_status:1;
    uint64_t pin_polarity:1;
    uint64_t remote_irr:1;
    uint64_t trigger_mode:1;
    uint64_t mask:1;
    uint64_t resesrved:39;
    uint64_t dest:8;
} __attribute__ ((packed));
*/

union ioapic_redirect_ent_t
{
    struct
    {
        uint8_t interrupt:8;
        uint8_t delivery_mode:3;
        uint8_t dest_mode:1;
        uint8_t delivery_status:1;
        uint8_t pin_polarity:1;
        uint8_t remote_irr:1;
        uint8_t trigger_mode:1;
        uint8_t mask:1;
        uint64_t resesrved:39;
        uint8_t dest:8;
    } __attribute__ ((packed));

    struct
    {
        uint32_t low_byte;
        uint32_t high_byte;
    } __attribute__ ((packed));
};


void ioapic_reg_write(int index, uint8_t off, uint32_t val)
{
    *(volatile uint32_t *)(ioapics[index].virt_base + IOAPIC_REGSEL) = off;
    *(volatile uint32_t *)(ioapics[index].virt_base + IOAPIC_REGWIN) = val;
}


uint32_t ioapic_reg_read(int index, uint8_t off)
{
    *(volatile uint32_t *)(ioapics[index].virt_base + IOAPIC_REGSEL) = off;
    return *(volatile uint32_t *)(ioapics[index].virt_base + IOAPIC_REGWIN);
}


uint32_t ioapic_get_id(int index)
{
    return (ioapic_reg_read(index, IOAPIC_REG_IOAPIC_ID) >> 24) & 0x0F;
}


uint32_t ioapic_get_ver(int index)
{
    return ioapic_reg_read(index, IOAPIC_REG_IOAPIC_VER) & 0xFF;
}


uint32_t ioapic_get_irqs(int index)
{
    return ((ioapic_reg_read(index, IOAPIC_REG_IOAPIC_VER) >> 16) & 0xFF) + 1;
}


uint64_t ioapic_get_redirect_ent(int index, uint8_t irq)
{
    uint32_t reg = IOAPIC_REG_IOREDTBL + (2 * irq);
    uint64_t val = (uint64_t)ioapic_reg_read(index, reg) |
                   ((uint64_t)ioapic_reg_read(index, reg + 1) << 32);

    return val;
}


void ioapic_set_redirect_ent(int index, uint8_t irq, uint64_t redir)
{
    uint32_t reg = IOAPIC_REG_IOREDTBL + (2 * irq);

    *(volatile uint32_t *)(ioapics[index].virt_base + IOAPIC_REGSEL) = reg;
    *(volatile uint32_t *)(ioapics[index].virt_base + IOAPIC_REGWIN) = (redir & 0xFFFFFFFF);

    *(volatile uint32_t *)(ioapics[index].virt_base + IOAPIC_REGSEL) = reg + 1;
    *(volatile uint32_t *)(ioapics[index].virt_base + IOAPIC_REGWIN) = ((redir >> 32) & 0xFFFFFFFF);
}


/*
void ioapic_enable_irq(uint32_t i, uint8_t vect)
{
    uint64_t v = ioapic_get_redirect_ent(i);
    struct ioapic_redirect_ent_t *redir = (struct ioapic_redirect_ent_t *)&v;

    redir->vector = vect;
    redir->delivery_mode = IOAPIC_DELIVERY_MODE_FIX;
    redir->dest_mode = IOAPIC_DEST_PHYSICAL;
    redir->mask = 0;
    redir->dest = 0;

    ioapic_set_redirect_ent(i, v);
    printk("ioapic: mapped IRQ %d to interrupt %d\n", i, vect);
}


void ioapic_disable_irq(uint32_t i)
{
    uint64_t v = ioapic_get_redirect_ent(i);
    struct ioapic_redirect_ent_t *redir = (struct ioapic_redirect_ent_t *)&v;

    redir->mask = 1;

    ioapic_set_redirect_ent(i, v);
    printk("ioapic: disabled IRQ %d\n", i);
}
*/


static int ioapic_from_gsi(uint32_t irq_base)
{
    int i;

    for(i = 0; i < ioapic_count; i++)
    {
        if(ioapics[i].irq_base <= irq_base &&
           (ioapics[i].irq_base + ioapics[i].max_redirect) > irq_base)
        {
            return i;
        }
    }

    kpanic("ioapic: IRQ base not found\n");
    return 0;       // to pacify gcc
}


static void ioapic_create_redirect(uint8_t irq_id, uint32_t irq_base,
                                   uint16_t flags, int cpu, int enable)
{
    union ioapic_redirect_ent_t ent;
    int ioapic_target = ioapic_from_gsi(irq_base);
    uint32_t irq_off, target_io_off, reg;

    ent.interrupt = irq_id;

    if(flags & IOAPIC_ACTIVE_HIGH_LOW)
    {
        ent.pin_polarity = 1;
    }

    if(flags & IOAPIC_TRIGGER_EDGE_LOW)
    {
        ent.trigger_mode = 1;
    }

    ent.mask = !enable;
    ent.dest = processor_local_data[cpu].lapicid;           // XXX:

    irq_off = irq_base - ioapics[ioapic_target].irq_base;
    target_io_off = irq_off * 2;
    reg = IOAPIC_REG_IOREDTBL + target_io_off;

    *(volatile uint32_t *)(ioapics[ioapic_target].virt_base + IOAPIC_REGSEL) = reg;
    *(volatile uint32_t *)(ioapics[ioapic_target].virt_base + IOAPIC_REGWIN) = ent.low_byte;

    *(volatile uint32_t *)(ioapics[ioapic_target].virt_base + IOAPIC_REGSEL) = reg + 1;
    *(volatile uint32_t *)(ioapics[ioapic_target].virt_base + IOAPIC_REGWIN) = ent.high_byte;

    printk("ioapic: mapped IRQ %d to interrupt %d\n", irq_id, irq_base);
}


static void ioapic_redirect_irq_to_cpu(int id, uint8_t irq, int enable)
{
    if(irq_redir[irq].gsi != 0xFFFFFFFF)
    {
        ioapic_create_redirect(irq + 0x20, irq_redir[irq].gsi, 
                               irq_redir[irq].flags, id, enable);
        return;
    }

    ioapic_create_redirect(irq + 0x20, irq, 0, id, enable);
}


void ioapic_enable_irq(uint32_t i)
{
    printk("ioapic: enabling IRQ %d\n", i);
    ioapic_redirect_irq_to_cpu(lapic_cur_cpu(), i, 1);
}


void ioapic_disable_irq(uint32_t i)
{
    printk("ioapic: disabling IRQ %d\n", i);
    ioapic_redirect_irq_to_cpu(lapic_cur_cpu(), i, 0);
}


/*
void ioapic_redirect_legacy_irqs(void)
{
    int i;

    for(i = 0; i < 16; i++)
    {
        if(i != 2)
        {
            ioapic_redirect_irq_to_cpu(lapic_cur_cpu(), i, 1);
        }
    }
}
*/


void ioapic_add(uint32_t int_base, uint32_t phys_base)
{
    uint32_t i;

    if(ioapic_count >= MAX_IOAPIC)
    {
        printk("ioapic: too many I/O APICs (max %d)\n", MAX_IOAPIC);
        return;
    }

    if(!(ioapics[ioapic_count].virt_base = 
                mmio_map(phys_base, phys_base + PAGE_SIZE)))
    {
        printk("ioapic: failed to map base registers\n");
        return;
    }

    ioapics[ioapic_count].id = ioapic_get_id(ioapic_count);
    ioapics[ioapic_count].phys_base = phys_base;
    ioapics[ioapic_count].irq_base = int_base;
    ioapics[ioapic_count].max_redirect = ioapic_get_irqs(ioapic_count);

    printk("ioapic: initializing I/O APIC:\n");
    printk("ioapic:    base phys " _XPTR_", virt " _XPTR_ "\n",
            ioapics[ioapic_count].phys_base, ioapics[ioapic_count].virt_base);
    printk("ioapic:    id %d, ver %d, IRQs %d\n",
            ioapics[ioapic_count].id, 
            ioapic_get_ver(ioapic_count), 
            ioapics[ioapic_count].max_redirect);

    // disable interrupts
    for(i = 0; i < ioapics[ioapic_count].max_redirect; i++)
    {
        uint64_t v = ioapic_get_redirect_ent(ioapic_count, i);
        union ioapic_redirect_ent_t redir;

        redir.low_byte = v & 0xFFFFFFFF;
        redir.high_byte = (v >> 32) & 0xFFFFFFFF;
        redir.mask = 1;
        v = (uint64_t)redir.low_byte | ((uint64_t)redir.high_byte << 32);

        ioapic_set_redirect_ent(ioapic_count, i, v);
    }

    printk("ioapic: disabled all redirections\n");
    ioapic_count++;
}


/*
void ioapic_init(uintptr_t phys_base)
{
    uint32_t i;

    // disable interrupts
    for(i = 0; i < ioapic_get_irqs(); i++)
    {
        uint64_t v = ioapic_get_redirect_ent(i);
        struct ioapic_redirect_ent_t *redir = (struct ioapic_redirect_ent_t *)&v;

        redir->mask = 1;
        ioapic_set_redirect_ent(i, v);
    }

    printk("ioapic: disabled all redirections\n");
}
*/

