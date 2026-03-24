/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
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
#include <kernel/net/i8254x.h>
#include <kernel/laylaos.h>
#include <kernel/io.h>
#include <kernel/pci.h>
#include <kernel/usb.h>
#include <kernel/pic.h>
#include <kernel/ata.h>
#include <kernel/dev.h>
#include <kernel/hda.h>
#include <kernel/vbox.h>
#include <kernel/ahci.h>
#include <kernel/acpi.h>
#include <kernel/ksymtab.h>
#include <kernel/asm.h>
#include <mm/kheap.h>


// offsets of base addresses
static unsigned int offsets[] = { 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24 };

// device IRQ assignments found on ACPI/MP parsing
struct irq_redir_t pci_acpi_irq[256][32] = { 0, };

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

// pointer to ACPI MCFG table (for PCIe support)
static struct MCFG *mcfg = NULL;


static void pci_check_bus(uint16_t segment, uint16_t bus);
static void pci_check_func(uint16_t segment, uint16_t bus, uint8_t dev, 
                           uint8_t function, uintptr_t config_space);
static void pci_check_dev(uint16_t segment, uint16_t bus, uint8_t dev);


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

    value = pci_config_read_long(pci, 0x04);
    value |= 0x00000004;
    pci_config_write_long(pci, 0x04, value);
}


void pci_enable_interrupts(struct pci_dev_t *pci)
{
    uint32_t value;

    value = pci_config_read_long(pci, 0x04);
    value &= ~(1 << 10);
    pci_config_write_long(pci, 0x04, value);
}


void pci_enable_memoryspace(struct pci_dev_t *pci)
{
    uint32_t value;

    value = pci_config_read_long(pci, 0x04);
    value |= 0x00000002;
    pci_config_write_long(pci, 0x04, value);
}


void pci_enable_iospace(struct pci_dev_t *pci)
{
    uint32_t value;

    value = pci_config_read_long(pci, 0x04);
    value |= 0x00000001;
    pci_config_write_long(pci, 0x04, value);
}


/*
 * Helper functions adopted from:
 * https://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/tree/arch/x86/pci/early.c?id=refs/tags/v3.12.7
 */
STATIC_INLINE
void pci_config_write_legacy(uint8_t bus, uint8_t slot, uint8_t func, 
                             uint8_t offset, uint16_t val)
{
    uint32_t addr = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | 
                                                   offset | 0x80000000);
    outl(0xCF8, addr);
    outw(0xCFC + (offset & 2), val);
}


STATIC_INLINE
void pci_config_write_long_legacy(uint8_t bus, uint8_t slot, uint8_t func, 
                                  uint8_t offset, uint32_t val)
{
    uint32_t addr = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | 
                                                   offset | 0x80000000);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}


STATIC_INLINE
void pci_config_write_byte_legacy(uint8_t bus, uint8_t slot, uint8_t func, 
                                  uint8_t offset, uint8_t val)
{
    uint32_t addr = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | 
                                                   offset | 0x80000000);
    outl(0xCF8, addr);
    outb(0xCFC + (offset & 3), val);
}


STATIC_INLINE
uint32_t pci_config_read_long_legacy(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
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


STATIC_INLINE
uint16_t pci_config_read_legacy(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
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


STATIC_INLINE
uint8_t pci_config_read_byte_legacy(uint16_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint16_t tmp = 0;
    uint32_t addr = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | 
                                                   offset | 0x80000000);

    outl(0xCF8, addr);
    tmp = inb(0xCFC + (offset & 3));

    return tmp;
}


uint32_t pci_config_read_long(struct pci_dev_t *pci, uint8_t offset)
{
    if(pci->config_space)
    {
        return *(volatile uint32_t *)(pci->config_space + offset);
    }
    else
    {
        return pci_config_read_long_legacy(pci->bus, pci->dev, pci->function, offset);
    }
}


uint16_t pci_config_read(struct pci_dev_t *pci, uint8_t offset)
{
    if(pci->config_space)
    {
        return *(volatile uint16_t *)(pci->config_space + offset);
    }
    else
    {
        return pci_config_read_legacy(pci->bus, pci->dev, pci->function, offset);
    }
}


uint8_t pci_config_read_byte(struct pci_dev_t *pci, uint8_t offset)
{
    if(pci->config_space)
    {
        return *(volatile uint8_t *)(pci->config_space + offset);
    }
    else
    {
        return pci_config_read_byte_legacy(pci->bus, pci->dev, pci->function, offset);
    }
}


void pci_config_write_long(struct pci_dev_t *pci, uint8_t offset, uint32_t val)
{
    if(pci->config_space)
    {
        *(volatile uint32_t *)(pci->config_space + offset) = val;
    }
    else
    {
        pci_config_write_long_legacy(pci->bus, pci->dev, pci->function, offset, val);
    }
}


void pci_config_write(struct pci_dev_t *pci, uint8_t offset, uint16_t val)
{
    if(pci->config_space)
    {
        *(volatile uint16_t *)(pci->config_space + offset) = val;
    }
    else
    {
        pci_config_write_legacy(pci->bus, pci->dev, pci->function, offset, val);
    }
}


void pci_config_write_byte(struct pci_dev_t *pci, uint8_t offset, uint8_t val)
{
    if(pci->config_space)
    {
        *(volatile uint8_t *)(pci->config_space + offset) = val;
    }
    else
    {
        pci_config_write_byte_legacy(pci->bus, pci->dev, pci->function, offset, val);
    }
}


static uintptr_t config_space_addr(uint16_t segment, uint16_t bus, uint8_t dev, uint8_t function)
{
    uintptr_t config_space_phys;

    if(mcfg)
    {
        uintptr_t mcfgend = ((uintptr_t)mcfg) + mcfg->h.Length;
        uintptr_t alloc = (((uintptr_t)mcfg) + sizeof(struct MCFG));
        struct ecam_t *ecam;

        for( ; alloc < mcfgend; alloc += sizeof(struct ecam_t))
        {
            ecam = (struct ecam_t *)alloc;

            if(ecam->segment == segment && ecam->bus_start <= bus && bus <= ecam->bus_end)
            {
                config_space_phys = ecam->baseaddr + 
                                        ((bus << 20) | (dev << 15) | (function << 12));
                return mmio_map(config_space_phys, config_space_phys + PAGE_SIZE);
            }
        }
    }

    return 0;
}


static uint16_t pci_check_vendor(uint16_t segment, uint16_t bus, uint8_t dev, 
                                 uint8_t function, uintptr_t config_space)
{
    uint16_t vendor;

    if(!config_space)
    {
        config_space = config_space_addr(segment, bus, dev, function);
    }

    if(config_space)
    {
        vendor = *(volatile uint16_t *)(config_space);
    }
    else
    {
        vendor = pci_config_read_legacy(bus, dev, function, 0);
    }

    return vendor;
}


static uint8_t pci_get_hdrtype(uint16_t segment, uint16_t bus, uint8_t dev, 
                               uint8_t function, uintptr_t config_space)
{
    uint8_t hdrtype;

    if(!config_space)
    {
        config_space = config_space_addr(segment, bus, dev, function);
    }

    if(config_space)
    {
        hdrtype = *(volatile uint8_t *)(config_space + 0x0E);
    }
    else
    {
        hdrtype = pci_config_read_legacy(bus, dev, function, 0x0E);
        hdrtype &= 0xFF;
    }

    return hdrtype;
}


/*
 * Check the given bus to see the attached devs.
 */
static void pci_check_bus(uint16_t segment, uint16_t bus)
{
    uint8_t dev;

    if(bus >= 256)
    {
        kpanic("pci: system with too many PCI buses (> 256)!\n");
    }

    for(dev = 0; dev < 32; dev++)
    {
        pci_check_dev(segment, bus, dev);
    }
}


static void pci_check_dev(uint16_t segment, uint16_t bus, uint8_t dev)
{
    uintptr_t config_space = config_space_addr(segment, bus, dev, 0);
    uint16_t vendor;
    uint8_t function = 0;

    vendor = pci_check_vendor(segment, bus, dev, function, config_space);

    if(vendor != 0xFFFF)
    {
        pci_check_func(segment, bus, dev, function, config_space);

        unsigned int header = pci_get_hdrtype(segment, bus, dev, function, config_space);
        //unsigned int header = pci_config_read(segment, bus, dev, function, 0x0E) & 0xFF;
        //printk("pci_check_dev: header_type 0x%x\n", header);

        if((header & 0x80) != 0)
        {
	        /* multifunction dev */
	        for(function = 1; function < 8; function++)
	        {
                config_space = config_space_addr(segment, bus, dev, function);

	            if(pci_check_vendor(segment, bus, dev, function, config_space) != 0xFFFF)
	            {
	                pci_check_func(segment, bus, dev, function, config_space);
	            }
                else
                {
                    if(config_space)
                    {
                        vmmngr_unmap_page(config_space);
                    }
                }
	        }
        }
    }
    else
    {
        if(config_space)
        {
            vmmngr_unmap_page(config_space);
        }
    }
}


static void pci_check_func(uint16_t segment, 
                           uint16_t bus, uint8_t dev, uint8_t function,
                           uintptr_t config_space)
{
    uint8_t base_class;
    uint8_t sub_class;
    uint8_t secondary_bus;
    uint16_t vendor, dev_id;
    uint16_t i;
    uint8_t rev;
    struct pci_dev_t *pci;
    uint8_t prog_if;		/* Programming Interface */

    /* add dev to the list */
    pci = kmalloc(sizeof(struct pci_dev_t));

    if(!pci)
    {
    	printk("Error allocating memory for PCI dev\n");
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
    pci->segment = segment;
    pci->bus = bus;
    pci->dev = dev;
    pci->function = function;
    pci->config_space = config_space;

    vendor = pci_check_vendor(segment, bus, dev, function, config_space);
    dev_id = pci_config_read(pci, 2);
    i = pci_config_read(pci, 10);
    base_class = (i >> 8) & 0xff;
    sub_class = i & 0xff;

    i = pci_config_read(pci, 8);
    prog_if = (i >> 8) & 0xff;
    rev     = i & 0xff;
    
    pci->base_class = base_class;
    pci->sub_class = sub_class;
    pci->vendor = vendor;
    pci->dev_id = dev_id;
    pci->prog_if = prog_if;
    pci->rev = rev;

    pci_bus_bitmap[bus / 8] |= (1 << (bus % 8));

    /* check if this dev needs an IRQ assignment */
    /*
    i = pci_config_read(bus, dev, function, 0x3c);
    uint16_t header_type;
    uint8_t irq = i & 0xff;
    uint8_t irqpin = (i >> 8) & 0xff;
    header_type = pci_config_read(bus, dev, function, 0x0E);
    printk("header %u, IRQ %u (pin %u), ", header_type, irq, irqpin);
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
    
    for(i = 0; i < 6; i++)
    {
        pci->bar[i] = pci_config_read_long(pci, offsets[i]);
    }

    pci->irq[0] = pci_config_read(pci, 0x3c) & 0xff;

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
        else if((pci->vendor == 0x8086) && (pci->dev_id == 0x100e))
        {
            i8254x_init(pci);
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
            secondary_bus = pci_config_read(pci, 0x18) >> 8;

            if(secondary_bus)
            {
                pci_check_bus(segment, secondary_bus);
            }
        }
    }
    // USB Host controller
    else if((base_class == 0x0C) && (sub_class == 0x03))
    {
        usb_init(pci);
    }
    else if((vendor == VBOX_VENDOR_ID) && (dev_id == VBOX_DEVICE_ID))
    {
        vbox_init(pci);
    }
}


void pci_check_bus_range(uint16_t segment, uint16_t bus_start /* , uint16_t bus_end */)
{
    uint8_t func;
    unsigned int header_type;
    //header_type = pci_config_read(0, 0, 0, 0x0E);

    header_type = pci_get_hdrtype(segment, bus_start, 0, 0, 0);

    if((header_type & 0x80) == 0)
    {
        /* single PCI host controller */
        printk("pci: checking bus #%d (single, hdr 0x%x)\n", bus_start, header_type);
        pci_check_bus(0, 0);
    }
    else
    {
        /* multiple PCI host controllers */
        for(func = 0; func < 8; func++)
        {
            if(pci_check_vendor(segment, bus_start, 0, func, 0) != 0xFFFF)
            //if(pci_check_vendor(segment, 0, 0, func) != 0xFFFF)
            {
                break;
            }

            printk("pci: checking bus #%d (multi, hdr 0x%x)\n", bus_start, header_type);
            pci_check_bus(bus_start, func);
        }
    }
}


/*
 * Check PCI buses.
 */
void pci_check_all_buses(void)
{
    char *buses;
    int i, bus_count;
    struct pci_dev_t *pci, *tmp;

    /*
     * Force gcc to ignore the "void * to function pointer cast" warning
     */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

    void *(*acpifunc)();
    if((acpifunc = ksym_value("acpi_get_table")))
    {
        mcfg = acpifunc("MCFG");
        printk("pci: found MCFG table at " _XPTR_ "\n", mcfg);
    }

#pragma GCC diagnostic pop

    if(mcfg)
    {
        uintptr_t mcfgend = ((uintptr_t)mcfg) + mcfg->h.Length;
        uintptr_t alloc = (((uintptr_t)mcfg) + sizeof(struct MCFG));
        struct ecam_t *ecam;

        for( ; alloc < mcfgend; alloc += sizeof(struct ecam_t))
        {
            ecam = (struct ecam_t *)alloc;
            pci_check_bus_range(ecam->segment, ecam->bus_start /* , ecam->bus_end */);
        }
    }
    else
    {
        pci_check_bus_range(0, 0 /* , 255 */);
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


void pci_get_bar_info(struct pci_dev_t *pci, struct pci_bar_t *out)
{
    uint16_t i;
    unsigned int header = pci_get_hdrtype(pci->segment, pci->bus, 
                                          pci->dev, pci->function, 
                                          pci->config_space);
    //unsigned int header = pci_config_read(pci->bus, pci->dev, pci->function, 0x0E) & 0xFF;

    for(i = 0; i < 6; i++)
    {
        // devices with hdr type 0x00 have 6 bars
        if(i < 2 || !(header & 0x01))
        {
            // get base address
            out[i].base = pci_config_read_long(pci, offsets[i]);
            //printk("%d - 0x%x\n", i, out[i].base);

            if(out[i].base)
            {
                // get memory type if this is a valid bar
                out[i].iotype = (out[i].base & 0x01) + 1;

                if(out[i].iotype == PCI_IOTYPE_MMIO)
                {
                    out[i].base &= ~0xf;
                }
                else
                {
                    out[i].base &= ~0x3;
                }

                // get memory size
                cli();
                pci_config_write_long(pci, offsets[i], 0xffffffff);
                out[i].iosize = pci_config_read_long(pci, offsets[i]);
                out[i].iosize &= ~0xf;    // mask the lower 4 bits
                out[i].iosize = (~(out[i].iosize) & 0xffffffff) + 1;     // invert and add 1
                pci_config_write_long(pci, offsets[i], out[i].base);
                pci_config_read_long(pci, offsets[i]);
                sti();
            }
            else
            {
                out[i].iotype = PCI_IOTYPE_INVALID;
            }
        }
        else
        {
            out[i].iotype = PCI_IOTYPE_INVALID;
        }
    }
}


uintptr_t get_msi_addr(uint64_t *msidata, unsigned int vector,
                       int32_t cpuid, int edge, int deassert)
{
    *msidata = (vector & 0xFF) |
               (edge ? 0 : (1 << 15)) |
               (deassert ? 0 : (1 << 14));

    return (0xFEE00000 | (cpuid << 12));
}


#define CAPID_MSI       0x05
#define CAPID_MSIX      0x11

int enable_msi(struct pci_dev_t *pci)
{
    uint32_t stat, capptr, capreg, msireg = 0;
    uint64_t msidata = 0;
    uintptr_t msiaddr;

    // read the status register
    stat = pci_config_read_long(pci, 0x04);
    stat >>= 16;
    printk("enable_msi: stat %x\n", stat);

    // check if we have a capabilities pointer
    if((stat & (1 << 4)))
    {
        capptr = pci_config_read_long(pci, 0x34);
        capptr &= 0xFF;
        printk("enable_msi: capptr %x\n", capptr);

        while(capptr)
        {
            capreg = pci_config_read_long(pci, capptr);
            printk("enable_msi: capreg %x\n", capreg);

            if((capreg & 0xFF) == CAPID_MSIX)
            {
                msireg = capptr;
                break;
            }
            else if((capreg & 0xFF) == CAPID_MSI)
            {
                msireg = capptr;
                break;
            }

            capptr = ((capreg >> 8) & 0xFF);
        }

        printk("enable_msi: msireg %x\n", msireg);

        if(!msireg)
        {
            printk("pci: device does not support MSI/MSI-X\n");
            return -ENOENT;
        }

        uintptr_t s = int_off();

        pci->irq[0] = alloc_irq_vector() - 32;
        printk("pci: registering handler for MSI IRQ %d\n", pci->irq[0]);
        register_interrupt_handler(pci->irq[0] + 32, &pci->irq_handler);

        int_on(s);

        // get MSI address
        msiaddr = get_msi_addr(&msidata, pci->irq[0] + 32, lapic_cur_cpu(), 1, 0);

        capreg = pci_config_read_long(pci, msireg);
        printk("enable_msi: msiaddr %lx, msidata %lx, capreg %x\n", msiaddr, msidata, capreg);

        if((capreg & 0xFF) == CAPID_MSIX)
        {
            struct pci_bar_t bar[6];
            uint32_t tabbir, pendingbir, bir, off;
            uint32_t tabcnt, i;
            uintptr_t mappedbar;
            uint32_t *msitab;

            printk("pci: enabling MSI-X\n");
            pci_get_bar_info(pci, &bar[0]);

            // Cap + 0x4 = table offset (31:3) | BIR (2:0)
            // Cap + 0x8 = Pending bit offset (31:3) | pending bit BIR (2:0)
            tabbir = pci_config_read_long(pci, msireg + 4);
            pendingbir = pci_config_read_long(pci, msireg + 8);
            bir = tabbir & 0x07;
            off = tabbir & ~0x07;
            tabcnt = (((capreg >> 16) & 0x07FF) + 1);   // entries in table

            printk("enable_msi: tabbir %x, pendingbir %x, bir %x, off %x\n", tabbir, pendingbir, bir, off);

            mappedbar = mmio_map(bar[bir].base, bar[bir].base + bar[bir].iosize);
            msitab = (uint32_t *)(mappedbar + off);

            // write upper bits of addr
            pci_config_write_long(pci, msireg + 4, msiaddr >> 32);

            // fill the table
            for(i = 0; i < tabcnt; i++)
            {
                msitab[(i * 4) + 0] = msiaddr & 0xFFFFFFFC; // lower bits, 4-byte aligned
                msitab[(i * 4) + 1] = (msiaddr >> 32);  // upper bits
                msitab[(i * 4) + 2] = msidata;          // message data
                msitab[(i * 4) + 3] = 0;                // unmask irq in vector control
            }

            // enable MSI-X
            capreg &= ~(1 << 30);
            capreg |= (1 << 31);
            pci_config_write_long(pci, msireg, capreg);
        }
        else if((capreg & 0xFF) == CAPID_MSI)
        {
            uint32_t msgctrl = (capreg >> 16);
            uint32_t masking = !!(msgctrl & (1 << 8));  // per vector masking
            uint32_t bits64 = !!(msgctrl & (1 << 7));   // 64 bits
            uint32_t off = 8;

            printk("pci: enabling MSI\n");

            // write address low bits
            pci_config_write_long(pci, msireg + 4, msiaddr & 0xFFFFFFFF);

            if(bits64)
            {
                // write address high bits
                pci_config_write_long(pci, msireg + 8, msiaddr >> 32);
                off += 4;
            }

            // write message data
            pci_config_write_long(pci, msireg + off, msidata);

            // unmask interrupts
            if(masking)
            {
                pci_config_write_long(pci, msireg + 16, 0);
            }

            // enable MSI
            msgctrl |= 1;
            capreg = (capreg & 0xFFFF) | (msgctrl << 16);
            pci_config_write_long(pci, msireg, capreg);
        }
        else
        {
            printk("pci: invalid capability register value (0x%x)\n", (capreg & 0xFF));
            return -ENOENT;
        }

        return 0;
    }

    printk("pci: cannot find capabilities register\n");
    return -ENOENT;
}


uint16_t pci_get_devirq(struct pci_dev_t *pci)
{
    //printk("pci %u:%u, irq %u:%u\n", pci->bus, pci->dev, pci->irq[0], pci_acpi_irq[pci->bus][pci->dev].gsi);

    if(pci_acpi_irq[pci->bus][pci->dev].gsi != 0)
    {
        pci->irq[0] = pci_acpi_irq[pci->bus][pci->dev].gsi;
        return pci_acpi_irq[pci->bus][pci->dev].flags;
    }

    return 0;
}


/*
 * Register PCI IRQ handler.
 */
void pci_register_irq_handler(struct pci_dev_t *pci, 
                              int (*handler)(struct regs *, void *),
                              char *name)
{
    pci->irq_handler.handler = handler;
    pci->irq_handler.handler_arg = pci;
    //pci->irq_handler.hits = 0;
    //pci->irq_handler.ticks = 0;

    char *p = pci->irq_handler.short_name;

    while((*p++ = *name++))
    {
        ;
    }

    if(enable_msi(pci) < 0)
    {
        uint16_t apic_flags = pci_get_devirq(pci);

        if(pci->irq[0] != 0xff)
        {
            printk("pci: registering handler for IRQ %d\n", pci->irq[0]);
            register_interrupt_handler(pci->irq[0] + 32, &pci->irq_handler);
            enable_irq(pci->irq[0], apic_flags);
            //empty_loop();
        }
    }
}

