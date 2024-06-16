/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ata2.c
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
 *  \file ata2.c
 *
 *  This file impelements the functions used by the kernel to identify ATA
 *  (Advanced Technology Attachment) devices, initialize them, and identify 
 *  disk partitions.
 *  This file handles both PATA and PATAPI (DVD or CD-ROM) devices.
 *  The rest of the ATA group of functions can be found in ata_irq.c and
 *  ata_rw.c.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/ata.h>
#include <kernel/pci.h>
#include <kernel/pic.h>
#include <kernel/io.h>
#include <kernel/dev.h>
#include <kernel/ahci.h>
#include <mm/kheap.h>
#include <mm/dma.h>
#include <fs/procfs.h>
#include <fs/devfs.h>

unsigned char ide_buf[2048] = { 0, };

char *psstr[] = { "Primary", "Secondary" };
char *msstr[] = { "master", "slave" };

struct ata_devtab_s tab1 = {0,};     // for devices with maj == 3
struct ata_devtab_s tab2 = {0,};     // for devices with maj == 22

char *dev_type_str[] =
{
    "PATA",
    "PATAPI",
    "SATA",
    "SATAPI",
    "UNKNOWN",
};

struct handler_t ide_irq_handler =
{
    .handler = ide_irq_callback,
    .handler_arg = 0,
};


// function prototypes

int reset_finished(uint16_t iobase, uint8_t drv);
void ata_setup_controller(uint16_t iobase, uint16_t ctrl, uint16_t bmide,
                          uint8_t irq, uint8_t ps,
                          uint8_t *master, uint8_t *slave);
int ata_identify(struct ata_dev_s *dev);
void ata_setup_device(uint16_t iobase,
                      uint16_t ctrl, uint16_t bmide, uint8_t irq,
                      uint8_t ps, uint8_t ms);
int ata_cmd(struct ata_dev_s *dev, unsigned int cmd,
            unsigned int feat, unsigned int sects);
void ata_register_dev(struct ata_dev_s *dev, struct parttab_s *part, int n);
void ata_read_mbr(struct ata_dev_s *dev);


/*
 * Initialise disk devices.
 */
void ata_init(struct pci_dev_t *pci)
{
	/*
	 * See: https://wiki.osdev.org/PCI_IDE_Controller
	 */

    int i;
    unsigned int bar[6];
    uint8_t master, slave;
    uint8_t irq = pci_config_read(pci->bus, pci->dev,
                                    pci->function, 0x3c) & 0xff;

    for(i = 0; i < 6; i++)
    {
        bar[i] = pci->bar[i] & 0xFFFFFFFC;
    }

	if((pci->prog_if & 1) == 0)  /* Primary operating in legacy ISA mode */
	{
	    bar[0] = 0x1F0;
	    bar[1] = 0x3F6;
	    pci->irq[0] = 14;
	}
	else                         /* Native PCI mode */
    {
        pci->irq[0] = irq;
    }

	if((pci->prog_if & 4) == 0)  /* Secondary operating in legacy ISA mode */
	{
	    KDEBUG("1 pci->prog_if = 0x%x\n", pci->prog_if);
        bar[2] = 0x170;
        bar[3] = 0x376;
        pci->irq[1] = 15;
	}
	/*
	 * Native mode uses same IRQ for both 1ry & 2ry channels.
	 */
	else                         /* Native PCI mode */
    {
	    KDEBUG("2 pci->prog_if = 0x%x\n", pci->prog_if);
        pci->irq[1] = irq;
    }
    
    pci_enable_busmastering(pci);


    // setup primary IDE controller
    ata_setup_controller(bar[0], bar[1], bar[4], pci->irq[0], 1,
                         &master, &slave);
    
    // setup secondary IDE controller
    ata_setup_controller(bar[2], bar[3], bar[4] + 8, pci->irq[1], 0,
                         &master, &slave);
}


int reset_finished(uint16_t iobase, uint8_t drv)
{
    int i = 50000;
    volatile uint8_t status;
    
    outb(iobase + ATA_REG_DRVHD, drv);
    ata_delay(iobase + ATA_REG_STATUS);
    
    while(--i)
    {
        status = inb(iobase + ATA_REG_STATUS);
        
        if((status & ATA_SR_BUSY) == 0)
        {
            return 1;
        }
    }
    
    // timed out
    return 0;
}


void ata_setup_controller(uint16_t iobase, uint16_t ctrl, uint16_t bmide,
                          uint8_t irq, uint8_t ps,
                          uint8_t *master, uint8_t *slave)
{
	*master = IDE_UNKNOWN;
	*slave = IDE_UNKNOWN;
	
	// disable IRQs
	outb(ctrl + ATA_REG_CONTROL, NIEN);
	
	// reset controller
	outb(ctrl + ATA_REG_CONTROL, SRST | NIEN);
    ata_delay(iobase + ATA_REG_STATUS);
	outb(ctrl + ATA_REG_CONTROL, NIEN);
    ata_delay(iobase + ATA_REG_STATUS);

    // wait for devices to finish
    if(!reset_finished(iobase, 0x00))
    {
        printk("ata: timed out waiting for master hdd to reset\n");
        *master = 0;
    }

    if(!reset_finished(iobase, 0x10))
    {
        printk("ata: timed out waiting for slave hdd to reset\n");
        *slave = 0;
    }
    
    // identify devices
    if(*master)
    {
        ata_setup_device(iobase, ctrl, bmide, irq, ps, 1);
    }

    if(*slave)
    {
        ata_setup_device(iobase, ctrl, bmide, irq, ps, 0);
    }


    // enable IRQs
    struct handler_t *h = irq_handler_alloc(ide_irq_callback, 0, "ide");
    register_irq_handler(irq, h);
    enable_irq(irq);
	outb(ctrl + ATA_REG_CONTROL, 0x00);
    ata_delay(iobase + ATA_REG_STATUS);
    KDEBUG("ata_setup_controller: enabled IRQ %d\n", irq);
    //__asm__("xchg %%bx, %%bx"::);
}


int ata_identify(struct ata_dev_s *dev)
{
    unsigned int slavebit = MS(dev);
    uint8_t ch, cl;
    int type = IDE_UNKNOWN;
    uint16_t valid, udma;
    int res, l, err;

    // select device
    outb(dev->base + ATA_REG_FEATURE, 0);
    outb(dev->base + ATA_REG_DRVHD, (slavebit << 4));

    volatile uint8_t status;
    int i;
    
    ata_delay(dev->base + ATA_REG_STATUS);

    //delay for 400 nanoseconds
    /*
    for(i = 0; i < 16; i++)
    {
        status = inb(dev->base + ATA_REG_STATUS);
    }
    */

    // send ATA IDENTIFY command
    outb(dev->base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    // wait for drive to be ready
    //ata_wait(dev, ATA_SR_DRQ, TIMEOUT_DRQ);
    
    i = 0;
    
    while(((status = inb(dev->base + ATA_REG_STATUS)) & ATA_SR_BUSY) &&
          (i < TIMEOUT_DRQ))
    {
        i++;
    }
    
    res = inb(dev->base + ATA_REG_STATUS);
    err = (res & ATA_SR_ERR);
    
    /*
    if((res = ata_wait(dev, ATA_SR_DRQ, TIMEOUT_DRQ)) != 0)
    {
        KDEBUG("ata_identify: 1 - res %d\n", res);
        return res;
    }
    */

    cl = inb(dev->base + ATA_REG_TRACKLSB);
    ch = inb(dev->base + ATA_REG_TRACKMSB);

    //printk("ata_identify: 2 - cl 0x%x, ch 0x%x, res 0x%x, err 0x%x\n", cl, ch, res, err);
    //__asm__ ("xchg %%bx, %%bx"::);

    if(cl == 0x14 && ch == 0xEB)
    {
        err = 0;
        type = IDE_PATAPI;
    }
    else if(cl == 0x69 && ch == 0x96)
    {
        err = 0;
        type = IDE_SATAPI;
    }
    else if(cl == 0x00 && ch == 0x00 && res != 0x00)
    {
        type = IDE_PATA;
    }
    else if(cl == 0x3C && ch == 0xC3)
    {
        type = IDE_SATA;
    }
    else
    {
        return -EIO;    // unknown type
    }

    if(err)
    {
        return -EIO;    // unknown type
    }

    if(type & 1)    // ATAPI
    {
        outb(dev->base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
        ata_delay(dev->base + ATA_REG_STATUS);
    }

    // read the identification space
    insl(dev->base + ATA_REG_DATA, ide_buf, 128);

    // read device parameters
    dev->type = type;
    dev->sign = U16(ide_buf, ATA_IDENT_DEVICETYPE);
    dev->capabilities = U16(ide_buf, ATA_IDENT_CAPABILITIES);
    dev->commandsets =  U32(ide_buf, ATA_IDENT_COMMANDSETS);

    // string indicating device model
    for(l = ATA_IDENT_MODEL; l < (ATA_IDENT_MODEL + 40); l += 2)
    {
	    dev->model[l - ATA_IDENT_MODEL] = ide_buf[l + 1];
	    dev->model[(l + 1) - ATA_IDENT_MODEL] = ide_buf[l];
    }
    
    dev->model[40] = 0;

    for(l = ATA_IDENT_SERIAL; l < (ATA_IDENT_SERIAL + 20); l += 2)
    {
	    dev->serial[l - ATA_IDENT_SERIAL] = ide_buf[l + 1];
	    dev->serial[(l + 1) - ATA_IDENT_SERIAL] = ide_buf[l];
    }
    
    dev->serial[20] = 0;

    for(l = 46; l < 54; l += 2)
    {
	    dev->firmware[l - 46] = ide_buf[l + 1];
	    dev->firmware[(l + 1) - 46] = ide_buf[l];
    }
    
    dev->firmware[8] = 0;


	/*
	 * Determine UDMA mode
	 */

    valid = U16(ide_buf, ATA_IDENT_FIELDVALID);
    udma = U16(ide_buf, ATA_IDENT_UDMA_MODE);
    
    if((valid & 4) && (udma & (udma >> 8) & 0x3F))
    {
        if((udma >> 13) & 1)
        {
            dev->uses_dma = 6;      // mode 5 -- UDMA 100
        }
        else if((udma >> 12) & 1)
        {
            dev->uses_dma = 5;      // mode 4 -- UDMA 66
        }
        else if((udma >> 11) & 1)
        {
            dev->uses_dma = 4;      // mode 3 -- UDMA 44
        }
        else if((udma >> 10) & 1)
        {
            dev->uses_dma = 3;      // mode 2 -- UDMA 33
        }
        else if((udma >> 9) & 1)
        {
            dev->uses_dma = 2;      // mode 1 -- UDMA 25
        }
        else
        {
            dev->uses_dma = 1;      // mode 0 -- UDMA 16
        }
    }
    /*
    else
    {
        dev->uses_dma = (pci->prog_if & (1 << 7)) ? 1 : 0;
    }
    */

    if(dev->uses_dma && ata_dma_init(dev) != 0)
    {
        dev->uses_dma = 0;
    }
    
    //dev->uses_dma = 0;


    if(type & 1)    // ATAPI
    {
        dev->size = 0;
        dev->bytes_per_sector = ATAPI_SECTOR_SIZE;

        /*
        uint16_t commandsets =  U32(ide_buf, 166);
        printk("    Capabilities = 0x%x, Commandsets = 0x%x:0x%x\n",
                dev->capabilities, dev->commandsets, commandsets);
        screen_refresh(NULL);
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        for(;;);
        */
    }
    /* read ATA device capacity */
    else
    {
        dev->heads = U32(ide_buf, ATA_IDENT_HEADS);
        dev->cylinders = U32(ide_buf, ATA_IDENT_CYLINDERS);
        dev->sectors = U32(ide_buf, ATA_IDENT_SECTORS);
        dev->bytes_per_sector = U16(ide_buf, ATA_IDENT_BYTES_PER_SECTOR);

        if(dev->bytes_per_sector == 0)
        {
            dev->bytes_per_sector = 512;
        }

        KDEBUG("block_len = %lu\n", dev->bytes_per_sector);

        if(dev->commandsets & (1 << 26))
        {
    	    // device uses 48bit addressing
    	    dev->size = U32(ide_buf, ATA_IDENT_MAX_LBA_EXT);
    	    //printk("    Uses 48bit LBA, size = %luMB\n", dev->size / 1024 / 2);
        }
        else
        {
    	    // device uses CHS or 28bit addressing
    	    dev->size = U32(ide_buf, ATA_IDENT_MAX_LBA);
    	    //printk("    Uses 28bit LBA/CHS, size = %luMB\n", dev->size / 1024 / 2);
        }
        
        dev->size *= dev->bytes_per_sector;
    }

   	printk("  %s %s exists and is %s\n", psstr[PS(dev)], msstr[MS(dev)],
   	                dev_type_str[type == 0xFF ? 4 : type]);
    printk("    Model = %s\n", dev->model);
    printk("    Serial = %s, ", dev->serial);
    printk("Firmware = %s\n", dev->firmware);

    if(!(type & 1))    // ATA
    {
        printk("    Capacity = %luMB\n", dev->size / 1024 / 1024);
    }
    
    return 0;
}


void ata_setup_device(uint16_t iobase,
                      uint16_t ctrl, uint16_t bmide, uint8_t irq,
                      uint8_t ps, uint8_t ms)
{
    struct ata_dev_s *dev;
    uint8_t status;
    
    if(!(dev = kmalloc(sizeof(struct ata_dev_s))))
    {
        kpanic("Insufficient memory to initialise IDE disk\n");
        empty_loop();
    }

	memset(dev, 0, sizeof(struct ata_dev_s));
	dev->base = iobase;
	dev->ctrl = ctrl;
	dev->bmide = bmide;
	dev->nien = 0;
	dev->irq = irq;
    dev->masterslave = ((!ps) * 2) + !ms;
    //dev->type = type;
	
	// identify
	if(ata_identify(dev) != 0)
	{
	    printk("ata: cannot identify %s %s - skipping\n",
	           psstr[PS(dev)], msstr[MS(dev)]);
	    kfree(dev);
	    return;
	}
	
    // add the new device
    ata_register_dev(dev, NULL, 0);
    
    // if PATA or SATA, read the MBR
    if((dev->type & 1) == 0)
    {
        ata_read_mbr(dev);
    }

	// enable UDMA if the device supports it
	// do this AFTER reading the MBR, to simplify the MBR-reading logic
	if(dev->uses_dma)
	{
	    // enable UDMA in bus master status register
	    status = inb(dev->bmide + ATA_BUS_MASTER_REG_STATUS);
	    outb(dev->bmide + ATA_BUS_MASTER_REG_STATUS,
	                status | (ms ? 0x20 : 0x40));

	    // enable UDMA in IDE controller
	    if(ata_cmd(dev, ATA_CMD_SET_FEATURES, ATA_FEAT_XFER_MODE,
	               (ATA_XFER_MODE_UDMA | (dev->uses_dma - 1))) != 0)
        {
            printk("ata: failed to set UDMA mode\n");
            dev->uses_dma = 0;
        }
	}
}


int ata_cmd(struct ata_dev_s *dev, unsigned int cmd,
            unsigned int feat, unsigned int sects)
{
    unsigned int slavebit = MS(dev);
    int res;

    // send command
    outb(dev->base + ATA_REG_FEATURE, feat);
    outb(dev->base + ATA_REG_SECTORCNT, sects);
    outb(dev->base + ATA_REG_DRVHD, (slavebit << 4));
    outb(dev->base + ATA_REG_COMMAND, cmd);

    // wait for data
    if((res = ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY)) != 0)
    {
        printk("  Skipping disk with error status\n");
        return res;
    }
    
    return 0;
}


static void add_ata_dev(struct ata_dev_s *tmp, struct parttab_s *part,
                        int maj, int min)
{
    struct ata_devtab_s *tab = (maj == 3) ? &tab1 : &tab2;
    tab->dev[min] = tmp;
    tab->part[min] = part;
}


void ata_register_dev(struct ata_dev_s *dev, struct parttab_s *part, int n)
{
    char name[] = { 'h', 'd', '?', '\0', '\0', '\0' };
    static char d[] = { 'a', 'b', 'c', 'd' };   // 3rd letter in name
    static int majs[] = { 3, 3, 22, 22 };       // maj for primary/secondary
                                                // master/slave
    static int mins[] = { 0, 64, 0, 64 };       // min for primary/secondary
                                                // master/slave
    int i = dev->masterslave;
    int min;
    
    if(i < 0 || i > 3)
    {
        kpanic("invalid IDE device id\n");
    }
    
    min = mins[i];
    name[2] = d[i];
    
    // add partition number if needed
    if(part)
    {
        int j = 3;

        if(n >= 10)
        {
            name[j++] = '0' + (int)(n / 10);
        }

        name[j] = '0' + (int)(n % 10);
        min += n;
    }

    KDEBUG("ata_register_dev: %s, 0x%x\n", name, TO_DEVID(majs[i], min));
    //empty_loop();
    
    add_dev_node(name, TO_DEVID(majs[i], min), (S_IFBLK | 0664));
    add_ata_dev(dev, part, majs[i], min);

    // if PATAPI, add a cdrom device node
    if(dev->type & 1)
    {
        add_cdrom_device(TO_DEVID(majs[i], min), (S_IFBLK | 0664));
    }
}


/* partition table offsets */
static int mbr_offset[] = { 0x1be, 0x1ce, 0x1de, 0x1ee };
static uint8_t gpt_hdr_magic[] = "EFI PART";


static int read_sector_direct(struct ata_dev_s *dev, uint32_t lba)
{
    outb(dev->base + ATA_REG_FEATURE, 0x00);
    outb(dev->base + ATA_REG_SECTORCNT, (unsigned char)1);
    outb(dev->base + ATA_REG_SECTOR, (lba & 0xff));
    outb(dev->base + ATA_REG_TRACKLSB, ((lba >> 8) & 0xff));
    outb(dev->base + ATA_REG_TRACKMSB, ((lba >> 16) & 0xff));
    outb(dev->base + ATA_REG_DRVHD, 0xE0 | (MS(dev) << 4) | 
                                            ((lba >> 24) & 0xff));
    outb(dev->base + ATA_REG_COMMAND, 0x20);

    if(ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY) != 0)
    {
        return -EIO;
    }
	
    insw(dev->base, ide_buf, dev->bytes_per_sector / 2 /* 256 */);
    return 0;
}


/*
 * Read the given device's GUID Partition Table (GPT).
 *
 * For details on GPT partition table format, see:
 *    https://wiki.osdev.org/GPT
 */
void ata_read_gpt(struct ata_dev_s *dev)
{
    int i, dev_index;
    uint32_t gpthdr_lba = 0, off;
    uint32_t gptent_lba = 0, gptent_count = 0, gptent_sz = 0;
    struct gpt_part_entry_t *ent;
    struct parttab_s *part;

    // Sector 0 has already been read for us.
    for(i = 0; i < 4; i++)
    {
        // Check for GPT partition table signature
        if(ide_buf[mbr_offset[i] + 4] == 0xEE)
        {
            // The LBA of the GPT Partition Table Header is found at offset 8,
            // and is 4 bytes long (ideally should be 0x00000001).
            gpthdr_lba = get_dword(ide_buf + mbr_offset[i] + 8);
            break;
        }
    }

    if(gpthdr_lba == 0)
    {
        // This shouldn't happen
        return;
    }

    // Read the Partition Table Header
    if(read_sector_direct(dev, gpthdr_lba) != 0)
    {
        printk("  Skipping disk with error status\n");
        return;
    }

    // Verify GPT signature
    for(i = 0; i < 8; i++)
    {
        if(ide_buf[i] != gpt_hdr_magic[i])
        {
            printk("  Skipping disk with invalid GPT signature: '");
            printk("%c%c%c%c%c%c%c%c'\n", ide_buf[0], ide_buf[1], ide_buf[2],
                                          ide_buf[3], ide_buf[4], ide_buf[5],
                                          ide_buf[6], ide_buf[7]);
            return;
        }
    }

    // Get partition entry starting lba, entry size and count
    gptent_lba = get_dword(ide_buf + 0x48);
    gptent_count = get_dword(ide_buf + 0x50);
    gptent_sz = get_dword(ide_buf + 0x54);
    off = 0;
    dev_index = 1;

    printk("  Found GPT with %u entries (sz %u)\n", gptent_count, gptent_sz);

    // Read the first set of partition entries
    if(read_sector_direct(dev, gptent_lba) != 0)
    {
        printk("  Skipping disk with invalid GPT entries\n");
        return;
    }

    while(gptent_count--)
    {
        if(off >= dev->bytes_per_sector)
        {
            // Read the next set of partition entries
            if(read_sector_direct(dev, ++gptent_lba) != 0)
            {
                printk("  Skipping disk with invalid GPT entries\n");
                return;
            }

            off = 0;
        }

        ent = (struct gpt_part_entry_t *)(ide_buf + off);

        // Check for unused entries
        for(i = 0; i < 16; i++)
        {
            if(ent->guid[i] != 0)
            {
                break;
            }
        }

        if(i == 16)
        {
            KDEBUG("  Skipping unused GPT entry\n");
            off += gptent_sz;
            continue;
        }

        if(!(part = kmalloc(sizeof(struct parttab_s))))
        {
            return;
        }
        
        /*
         * NOTE: We do not process the attributes correctly here.
         *       Of note, the attribs field is 8 bytes long and we only
         *       store the first byte here.
         */
        part->attribs        = ent->attribs & 0xff;
        part->start_head     = 0;
        part->start_sector   = 0;
        part->start_cylinder = 0;
        part->system_id      = 0;
        part->end_head       = 0;
        part->end_sector     = 0;
        part->end_cylinder   = 0;
        part->lba            = ent->lba_start;
        part->total_sectors  = ent->lba_end - ent->lba_start;
        part->dev = dev;

        /*
        printk("    Partition %d\n", dev_index);
        printk("      attribs 0x%x\n", part->attribs);
        printk("      lba %u\n", part->lba);
        printk("      total_sectors %u\n", part->total_sectors);
        */
        
        ata_register_dev(dev, part, dev_index);
        dev_index++;
        off += gptent_sz;
    }
}


/*
 * Read the given device's master boot record (MBR).
 *
 * For details on MBR and partition table format, see:
 *    https://wiki.osdev.org/MBR_(x86)
 */
void ata_read_mbr(struct ata_dev_s *dev)
{
    int i;

    /* check it is ATA */
    if(dev->type & 1)
    {
        return;
    }

    memset(ide_buf, 0, sizeof(ide_buf) /* 512 */);

    /* Read the MBR */
    KDEBUG("  Reading the MBR..\n");
    //__asm__ ("xchg %%bx, %%bx"::);

    outb(dev->base + ATA_REG_DRVHD,
                0xE0 | (MS(dev) << 4) | ((0 >> 24) & 0x0F));
    outb(dev->base + ATA_REG_SECTORCNT, (unsigned char)1);
    outb(dev->base + ATA_REG_SECTOR, (unsigned char)0);
    outb(dev->base + ATA_REG_TRACKLSB, (unsigned char)0);
    outb(dev->base + ATA_REG_TRACKMSB, (unsigned char)0);
    outb(dev->base + ATA_REG_COMMAND, 0x20);
    
    if(ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY) != 0)
    {
        printk("  Skipping disk with error status\n");
        return;
    }
	
    insw(dev->base, ide_buf, dev->bytes_per_sector / 2 /* 256 */);
    
    /* verify the boot signature */
    /*
    if(ide_buf[0x1fe] != 0x55 || ide_buf[0x1ff] != 0xaa)
    {
        printk("  Skipping disk with invalid boot signature\n");
        return;
    }
    */

    /* add the partitions */
    for(i = 0; i < 4; i++)
    {
        //KDEBUG("i %d..\n", i);

        // Check for unused entries
        if(ide_buf[mbr_offset[i] + 4] == 0)
        {
            continue;
        }

        // Check for GPT partition table
        if(ide_buf[mbr_offset[i] + 4] == 0xEE)
        {
            ata_read_gpt(dev);
            return;
        }

        // Check partition start sector is legal
        if((ide_buf[mbr_offset[i] + 2] & 0x3f) == 0)
        {
            continue;
        }
        
        struct parttab_s *part = kmalloc(sizeof(struct parttab_s));

        if(!part)
        {
            return;
        }
        
        part->attribs        = ide_buf[mbr_offset[i] + 0] & 0xff;
        //part->bootable       = ide_buf[mbr_offset[i] + 0] & 0x80;
        part->start_head     = ide_buf[mbr_offset[i] + 1] & 0xff;
        part->start_sector   = ide_buf[mbr_offset[i] + 2] & 0x3f;
        part->start_cylinder = ide_buf[mbr_offset[i] + 3] |
                                ((ide_buf[mbr_offset[i] + 2] & 0xc0)<<8);
        part->system_id      = ide_buf[mbr_offset[i] + 4] & 0xff;
        part->end_head       = ide_buf[mbr_offset[i] + 5] & 0xff;
        part->end_sector     = ide_buf[mbr_offset[i] + 6] & 0x3f;
        part->end_cylinder   = ide_buf[mbr_offset[i] + 7] |
                                ((ide_buf[mbr_offset[i] + 6] & 0xc0)<<8);
        part->lba            = get_dword(ide_buf + mbr_offset[i] + 8);
        part->total_sectors  = get_dword(ide_buf + mbr_offset[i] + 12);
        part->dev = dev;

        /*
        printk("    Partition %d\n", i+1);
        printk("      lba bytes 0x%x 0x%x 0x%x 0x%x\n",
                ide_buf[offset[i] + 8], ide_buf[offset[i] + 9],
                ide_buf[offset[i] + 10], ide_buf[offset[i] + 11]);
        printk("      attribs 0x%x (bootable %d)\n",
                part->attribs, (part->attribs & 0x80));
        printk("      start_head %u\n", part->start_head);
        printk("      start_sector %u\n", part->start_sector);
        printk("      start_cylinder %u\n", part->start_cylinder);
        printk("      system_id 0x%x\n", part->system_id);
        printk("      end_head %u\n", part->end_head);
        printk("      end_sector %u\n", part->end_sector);
        printk("      end_cylinder %u\n", part->end_cylinder);
        printk("      lba %u\n", part->lba);
        printk("      total_sectors %u\n", part->total_sectors);
        */
        
        ata_register_dev(dev, part, i + 1);
    }

    KDEBUG("  Finished reading the MBR..\n");
    //__asm__ ("xchg %%bx, %%bx"::);
}


/*
 * General block device control function.
 */
int ata_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel)
{
    UNUSED(arg);
    UNUSED(kernel);
    
    int i, min = MINOR(dev_id);
    struct ata_devtab_s *tab = (MAJOR(dev_id) == 3) ? &tab1 : &tab2;
    struct ata_dev_s *dev = tab->dev[min];
    //printk("ata_ioctl: dev 0x%x\n", dev_id);
    
    if(!dev)
    {
        /*
        for(i = 0; i < 128; i++) printk("[%d:0x%lx] ", i, tab->dev[i]);
        printk("\n");
        */

        return -EINVAL;
    }
    
    i = (int)dev->bytes_per_sector;

    switch(cmd)
    {
        case DEV_IOCTL_GET_BLOCKSIZE:
            // get the block size in bytes
            return i;
            //return (int)dev->bytes_per_sector;
    }
    
    return -EINVAL;
}


/*
 * Read /proc/partitions.
 */
size_t get_partitions(char **buf)
{
    struct ata_devtab_s *tab;
    struct dirent *entry;
    int maj, i, j;
    size_t kb, len, count = 0, bufsz = 1024;
    char tmp[64];
    char *p;

    PR_MALLOC(*buf, bufsz);
    p = *buf;
    *p = '\0';

    ksprintf(p, 64, "major minor  1k blocks   name\n\n");
    len = strlen(p);
    count += len;
    p += len;

    /*
     * First check IDE devices
     */
    for(j = 0; j < 2; j++)
    {
        maj = j ? 22 : 3;
        tab = j ? &tab2 : &tab1;

        for(i = 0; i < MAX_ATA_DEVICES; i++)
        {
            if(tab->part[i] == NULL)
            {
                continue;
            }

            if(devfs_find_deventry(TO_DEVID(maj, i), 1, &entry) == 0)
            {
                // get partition size in kilobytes
                kb = (tab->part[i]->total_sectors *
                      tab->part[i]->dev->bytes_per_sector) / 1024;

                ksprintf(tmp, 64, "%4d  %4d  %10u   %s\n",
                              maj, i, kb, entry->d_name);
                kfree(entry);
                len = strlen(tmp);
                
                if(count + len >= bufsz)
                {
                    PR_REALLOC(*buf, bufsz, count);
                    p = *buf + count;
                }

                count += len;
                strcpy(p, tmp);
                p += len;
            }
        }
    }

    /*
     * Next check AHCI devices
     */
    for(i = 0; i < MAX_AHCI_DEVICES; i++)
    {
        if(ahci_disk_part[i] == NULL)
        {
            continue;
        }

        if(devfs_find_deventry(TO_DEVID(AHCI_DEV_MAJ, i), 1, &entry) == 0)
        {
            // get partition size in kilobytes
            kb = (ahci_disk_part[i]->total_sectors *
                  ahci_disk_part[i]->dev->bytes_per_sector) / 1024;

            ksprintf(tmp, 64, "%4d  %4d  %10u   %s\n",
                          AHCI_DEV_MAJ, i, kb, entry->d_name);
            kfree(entry);
            len = strlen(tmp);

            if(count + len >= bufsz)
            {
                PR_REALLOC(*buf, bufsz, count);
                p = *buf + count;
            }

            count += len;
            strcpy(p, tmp);
            p += len;
        }
    }

    return count;
}

