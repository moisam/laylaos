/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: pci.c
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
 *  \file pci.c
 *
 *  General functions for working with PCI (Peripheral Component 
 *  Interconnect) devices.
 *
 *  For PCI details, see: https://wiki.osdev.org/PCI
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <kernel/net/ne2000.h>
#include <kernel/laylaos.h>
#include <kernel/io.h>
#include <kernel/pci.h>
#include <kernel/pic.h>
#include <kernel/ata.h>
#include <kernel/dev.h>
#include <kernel/hda.h>
#include <kernel/vbox.h>
#include <kernel/ahci.h>
#include <mm/kheap.h>


struct pci_bus_t *first_pci_bus = NULL;
struct pci_bus_t *last_pci_bus = NULL;

struct pci_dev_t *first_pci = NULL;
struct pci_dev_t *last_pci = NULL;

int total_pci_dev = 0;
char pci_bus_bitmap[32] = { 0, };

static char *class_code_str[] =
{
    "dev built prior definition of the class code field",
    "Mass Storage Controller",
    "Network Controller",
    "Display Controller",
    "Multimedia Controller",
    "Memory Controller",
    "Bridge dev",
    "Simple Communication Controller",
    "Base System Peripheral",
    "Input dev",
    "Docking Station",
    "Processor",
    "Serial Bus Controller",
    "Wireless Controller",
    "Intelligent I/O Controller",
    "Satellite Communication Controller",
    "Encryption/Decryption Controller",
    "Data Acquisition and Signal Processing Controller",
    "Reserved",
    "dev does not fit any defined class"
};


static uint16_t pci_check_vendor(uint8_t bus, uint8_t slot, uint8_t function);
static uint16_t pci_get_vendor_id(uint8_t bus, uint8_t slot, uint8_t func);
static void pci_check_bus(uint8_t bus);
static void pci_check_func(uint8_t bus, uint8_t dev, uint8_t function);
static void pci_check_dev(uint8_t bus, uint8_t dev);


/*
 * Get device count on bus.
 */
int devices_on_bus(struct pci_bus_t *bus)
{
    int count = 0;
    struct pci_dev_t *pci;
    
    for(pci = bus->first; pci != NULL; pci = pci->next)
    {
        count++;
    }

    return count;
}


/*
 * Get bus count.
 */
int active_pci_bus_count(void)
{
    int count = 0;
    struct pci_bus_t *bus;
    
    for(bus = first_pci_bus; bus != NULL; bus = bus->next)
    {
        count++;
    }

    return count;
}


static int brute_active_pci_bus_count(void)
{
    int i, j;
    int count = 0;
    
    for(i = 0; i < (int)sizeof(pci_bus_bitmap); i++)
    {
        for(j = 1; j < (1 << 8); j <<= 1)
        {
            if(pci_bus_bitmap[i] & j)
            {
                count++;
            }
        }
    }
    
    return count;
}


/*
 * Get bus numbers and count.
 */
int active_pci_buses(char **buses, int *bus_count)
{
    size_t i, j;
    char k;
    int count = first_pci_bus ? active_pci_bus_count() : 
                                brute_active_pci_bus_count();
    char *arr = (char *)kmalloc(count * sizeof(char));
    
    *buses = arr;
    *bus_count = count;
    
    if(!arr)
    {
        return -ENOMEM;
    }
    
    for(i = 0, k = 0; i < sizeof(pci_bus_bitmap); i++)
    {
        for(j = 1; j < (1 << 8); j <<= 1, k++)
        {
            if(pci_bus_bitmap[i] & j)
            {
                //printk("active_pci_buses: i %u, j %u, k %d\n", i, j, k);
                *arr++ = k;
            }
        }
    }
    
    return 0;
}


/*
 * Helper functions to enable busmastering, IRQs, memory space, and I/O space
 * for a given PCI device.
 */

void pci_enable_busmastering(struct pci_dev_t *pci)
{
    uint32_t value;

    value = pci_config_read_long(pci->bus, pci->dev, pci->function, 0x04);
    value |= 0x00000004;
    pci_config_write_long(pci->bus, pci->dev, pci->function, 0x04, value);
}


void pci_enable_interrupts(struct pci_dev_t *pci)
{
    uint32_t value;

    value = pci_config_read_long(pci->bus, pci->dev, pci->function, 0x04);
    value &= ~(1 << 10);
    pci_config_write_long(pci->bus, pci->dev, pci->function, 0x04, value);
}


void pci_enable_memoryspace(struct pci_dev_t *pci)
{
    uint32_t value;

    value = pci_config_read_long(pci->bus, pci->dev, pci->function, 0x04);
    value |= 0x00000002;
    pci_config_write_long(pci->bus, pci->dev, pci->function, 0x04, value);
}


void pci_enable_iospace(struct pci_dev_t *pci)
{
    uint32_t value;

    value = pci_config_read_long(pci->bus, pci->dev, pci->function, 0x04);
    value |= 0x00000001;
    pci_config_write_long(pci->bus, pci->dev, pci->function, 0x04, value);
}


/*
 * Helper functions adopted from:
 * https://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/tree/arch/x86/pci/early.c?id=refs/tags/v3.12.7
 */
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, 
                      uint8_t offset, uint16_t val)
{
    uint32_t addr = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | 
                                                   offset | 0x80000000);
    outl(0xCF8, addr);
    outw(0xCFC + (offset & 2), val);
}


void pci_config_write_long(uint8_t bus, uint8_t slot, uint8_t func, 
                           uint8_t offset, uint32_t val)
{
    uint32_t addr = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | 
                                                   offset | 0x80000000);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}


void pci_config_write_byte(uint8_t bus, uint8_t slot, uint8_t func, 
                           uint8_t offset, uint8_t val)
{
    uint32_t addr = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | 
                                                   offset | 0x80000000);
    outl(0xCF8, addr);
    outb(0xCFC + (offset & 3), val);
}


uint32_t pci_config_read_long(uint8_t bus, uint8_t slot, uint8_t func, 
                              uint8_t offset)
{
    uint32_t addr;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint32_t tmp = 0;
    
    addr = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
	                  (offset & 0xfc) | ((uint32_t)0x80000000));
    
    outl(0xCF8, addr);
    tmp = (uint32_t)(inl(0xCFC));

    return tmp;
}


uint16_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, 
                         uint8_t offset)
{
    uint32_t addr;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
    
    addr = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                      (offset & 0xfc) | ((uint32_t)0x80000000));
    
    outl(0xCF8, addr);
    tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);

    return tmp;
}


uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, 
                             uint8_t offset)
{
    uint16_t tmp = 0;
    uint32_t addr = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | 
                                                   offset | 0x80000000);
    
    outl(0xCF8, addr);
    tmp = inb(0xCFC + (offset & 3));
    
    return tmp;
}


static uint16_t pci_check_vendor(uint8_t bus, uint8_t slot, uint8_t function)
{
    uint16_t vendor;//, dev;

    if((vendor = pci_config_read(bus, slot, function, 0)) != 0xFFFF)
    {
        //dev = pci_config_read(bus, slot, 0, 2);
    }

    return vendor;
}


static uint16_t pci_get_vendor_id(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint16_t vendor;//, dev;

    if((vendor = pci_config_read(bus, slot, func, 0)) != 0xFFFF)
    {
        //dev = pci_config_read(bus, slot, func, 2);
    }

    return vendor;
}


/*
 * Check the given bus to see the attached devs.
 */
static void pci_check_bus(uint8_t bus)
{
    uint8_t dev;
    
    for(dev = 0; dev < 32; dev++)
    {
        pci_check_dev(bus, dev);
    }
}


static void pci_check_dev(uint8_t bus, uint8_t dev)
{
    uint16_t vendor;
    uint8_t function = 0;

    vendor = pci_check_vendor(bus, dev, function);

    if(vendor != 0xFFFF)
    {
        pci_check_func(bus, dev, function);

        unsigned int header = pci_config_read(bus, dev, function, 0x0E) & 0xFF;
        //printk("pci_check_dev: header_type 0x%x\n", header);

        if((header & 0x80) != 0)
        {
	        /* multifunction dev */
	        for(function = 1; function < 8; function++)
	        {
	            if(pci_check_vendor(bus, dev, function) != 0xFFFF)
	            {
	                pci_check_func(bus, dev, function);
	            }
	        }
        }
    }
}


static void pci_check_func(uint8_t bus, uint8_t dev, uint8_t function)
{
    uint8_t base_class;
    uint8_t sub_class;
    uint8_t secondary_bus;
    uint16_t vendor, dev_id;
    uint16_t i;
    uint8_t rev;
    struct pci_dev_t *pci;
    uint8_t prog_if;		/* Programming Interface */

    vendor = pci_check_vendor(bus, dev, function);
    dev_id = pci_config_read(bus, dev, function, 2);
    i = pci_config_read(bus, dev, function, 10);
    base_class = (i >> 8) & 0xff;
    sub_class = i & 0xff;

    i = pci_config_read(bus, dev, function, 8);
    prog_if = (i >> 8) & 0xff;
    rev     = i & 0xff;
    
    /* add dev to the list */
    pci = kmalloc(sizeof(struct pci_dev_t));

    if(!pci)
    {
    	printk("Error allocating memory for PCI dev %u:%u\n", vendor, dev_id);
    	return;
    }

    if(last_pci)
    {
        last_pci->next = pci;
        last_pci = pci;
    }
    else
    {
        first_pci = pci;
        last_pci = pci;
    }
    
    total_pci_dev++;
    pci->next = NULL;
    pci->base_class = base_class;
    pci->sub_class = sub_class;
    pci->dev = dev;
    pci->vendor = vendor;
    pci->dev_id = dev_id;
    pci->bus = bus;
    pci->function = function;
    pci->prog_if = prog_if;
    pci->rev = rev;

    pci_bus_bitmap[bus / 8] |= (1 << (bus % 8));

    /* check if this dev needs an IRQ assignment */
    /*
    i = pci_config_read(bus, dev, function, 0x3c);
    uint8_t irq = i & 0xff;
    uint8_t irqpin = (i >> 8) & 0xff;
    printk("\n(%d)header %u, IRQ %u (pin %u), ", total_pci_devs, header_t, irq, irqpin);
    */
	  
    /* print status message */
    printk("  %u:%u.%u (%x:%x) is a ",
            (unsigned int)bus, (unsigned int)dev, (unsigned int)function, 
            base_class, sub_class);

    if(base_class == 0xFF)
    {
	    printk("%s", class_code_str[0x13]);
	}
    else if(base_class >= 0x12)
    {
	    printk("%s", class_code_str[0x12]);
	}
    else if(base_class > 0x00)
    {
	    printk("%s", class_code_str[base_class]);
	}
    else
    {
	    printk("VGA dev (%u:%u)", base_class, sub_class);
	}
      
    //printk(": %s", get_dev_str(vendor, dev_id));
    printk(" Vendor (%x) DID (%x)\n", vendor, dev_id);
      
    /* read base addresses */
    static unsigned int offsets[] = { 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24 };
    
    for(i = 0; i < 6; i++)
    {
        pci->bar[i] = pci_config_read_long(pci->bus, pci->dev, 
                                           pci->function, offsets[i]);
    }
    
    pci->irq[0] = pci_config_read(pci->bus, pci->dev, 
                                        pci->function, 0x3c) & 0xff;

    if(base_class == 0x01)
    {
        if(sub_class == 0x01)
        {
            ata_init(pci);
        }
        else if(sub_class == 0x06)
        {
            ahci_init(pci);
        }
    }
    // Ethernet controller
    else if((base_class == 0x02) && (sub_class == 0x00))
    {
        if((pci->vendor == 0x10EC) && (pci->dev_id == 0x8029))
        {
            ne2000_init(pci);
        }
    }
    // Multimedia Audio device
    else if((base_class == 0x04) && (sub_class == 0x03))
    {
        // Ensoniq ES1370 AudioPCI
        //if((pci->vendor == 0x1274) && (pci->dev_id == 0x5000))
        {
            hda_init(pci);
        }
    }
    else if(base_class == 0x06)
    {
        // PCI-to-PCI bridge
        if(sub_class == 0x04)
        {
            secondary_bus = pci_config_read(bus, dev, function, 0x19);
            pci_check_bus(secondary_bus);
        }
    }
    else if((vendor == VBOX_VENDOR_ID) && (dev_id == VBOX_DEVICE_ID))
    {
        vbox_init(pci);
    }
}


/*
 * Check PCI buses.
 */
void pci_check_all_buses(void)
{
    uint8_t func;
    uint8_t bus;
    uint16_t header_type;

    char *buses;
    int i, bus_count;
    struct pci_dev_t *pci, *tmp;

    header_type = pci_config_read(0, 0, 0, 0x0E);

    if((header_type & 0x80) == 0)
    {
        /* single PCI host controller */
        pci_check_bus(0);
    }
    else
    {
        /* multiple PCI host controllers */
        for(func = 0; func < 8; func++)
        {
            if(pci_get_vendor_id(0, 0, func) != 0xFFFF)
            {
                break;
            }

            bus = func;
            pci_check_bus(bus);
        }
    }

    printk("Total PCI devs found: %d\n", total_pci_dev);
	
	if(total_pci_dev == 0)
	{
	    return;
	}

    // sort devices by bus

    if(active_pci_buses(&buses, &bus_count) != 0)
    {
        return;
    }

    for(i = 0; i < bus_count; i++)
    {
        struct pci_bus_t *bus;
        
        if(!(bus = kmalloc(sizeof(struct pci_bus_t))))
        {
        	printk("Error allocating memory for PCI bus %d\n", buses[i]);
            kfree(buses);
        	return;
        }
        
        memset(bus, 0, sizeof(struct pci_bus_t));
        bus->bus = buses[i];

        if(last_pci_bus)
        {
            last_pci_bus->next = bus;
            last_pci_bus = bus;
        }
        else
        {
            first_pci_bus = bus;
            last_pci_bus = bus;
        }

        for(pci = first_pci; pci != NULL; pci = pci->next)
        {
            if(pci->bus != buses[i])
            {
                continue;
            }


            if(!(tmp = kmalloc(sizeof(struct pci_dev_t))))
            {
            	printk("Error allocating memory for a PCI dev on bus %d\n", 
            	        buses[i]);
                kfree(buses);
    	        return;
            }
            
            memcpy(tmp, pci, sizeof(struct pci_dev_t));

            if(bus->last)
            {
                bus->last->next = tmp;
            }
            else
            {
                bus->first = tmp;
            }

            tmp->next = NULL;
            bus->last = tmp;
            bus->count++;
        }
    }
    
    kfree(buses);
}


/*
 * Register PCI IRQ handler.
 */
void pci_register_irq_handler(struct pci_dev_t *pci, 
                              int (*handler)(struct regs *, int),
                              char *name)
{
    if(pci->irq[0] != 0xff)
    {
        pci->irq_handler.handler = handler;
        pci->irq_handler.handler_arg = pci->unit;
        pci->irq_handler.hits = 0;
        pci->irq_handler.ticks = 0;

        char *p = pci->irq_handler.short_name;

        while((*p++ = *name++))
        {
            ;
        }

        register_irq_handler(pci->irq[0], &pci->irq_handler);
        enable_irq(pci->irq[0]);
        printk("pci: registering handler for IRQ %d\n", pci->irq[0]);
        //empty_loop();
    }
}

