/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: ahci.c
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
 *  \file ahci.c
 *
 *  This file impelements the functions used by the kernel to identify SATA
 *  devices, initialize them, identify disk partitions, as well as functions
 *  to enable us to perform general I/O. This file handles both SATA and 
 *  SATAPI (DVD or CD-ROM) devices.
 */

//#define __DEBUG

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/ata.h>
#include <kernel/ahci.h>
#include <kernel/pci.h>
#include <kernel/pic.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/task.h>
#include <mm/kheap.h>
#include <mm/kstack.h>

#define PCI_COMMAND             0x04


#define SATA_SIG_ATA            0x00000101    // SATA drive
#define SATA_SIG_ATAPI          0xEB140101    // SATAPI drive
#define SATA_SIG_SEMB           0xC33C0101    // Enclosure management bridge
#define SATA_SIG_PM             0x96690101    // Port multiplier
 
#define AHCI_DEV_NULL           0
#define AHCI_DEV_SATA           1
#define AHCI_DEV_SEMB           2
#define AHCI_DEV_PM             3
#define AHCI_DEV_SATAPI         4

#define HBA_PORT_IPM_ACTIVE     1
#define HBA_PORT_DET_PRESENT    3

#define PX_SCTL_IPM_MASK        (0xf << 8)
#define PX_SCTL_IPM_ACTIVE      (0x1 << 8)
#define PX_SCTL_IPM_NONE        (0x3 << 8)

#define HBA_PORT_CMD_ST         0x0001
#define HBA_PORT_CMD_FRE        0x0010
#define HBA_PORT_CMD_FR         0x4000
#define HBA_PORT_CMD_CR         0x8000


/* Port Command Bits */
#define PORT_CMD_START          1
#define PORT_CMD_POD            2
#define PORT_CMD_SUD            4

#define HBA_PORT_IS_TFES        (1 << 30)
#define HBA_PORT_CMD_ICC        (0xf << 28)
#define HBA_PORT_CMD_ICC_ACTIVE (1 << 28)

#define CMD_SLOTS(hba)          (((hba)->cap >> 8) & 0xff)


struct ahci_dev_t
{
    dev_t devid;
    uintptr_t iobase, iosize;
    uintptr_t port_clb[32];     // virtual addresses for us to write to each
                                // port's command list base address
    uintptr_t port_fb[32];      // same for port's FIS base address
    uintptr_t port_ctba[32];    // same for port's command list buffer
    struct kernel_mutex_t port_lock[32];
    struct pci_dev_t *pci;
    struct task_t *task;
    struct ahci_dev_t *next;
};


static int last_unit = 0;
struct ahci_dev_t *first_ahci = NULL;

/* Our master table for AHCI disks and their partitions */
struct ata_dev_s *ahci_disk_dev[MAX_AHCI_DEVICES];
struct parttab_s *ahci_disk_part[MAX_AHCI_DEVICES];

int ahci_intr(struct regs *r, int unit);
int ahci_sata_read(struct ata_dev_s *dev, size_t lba, int __sectors,
                                          uintptr_t phys_buf);
int ahci_sata_write(struct ata_dev_s *dev, size_t lba, int __sectors,
                                           uintptr_t phys_buf);
int ahci_satapi_read(struct ata_dev_s *dev, size_t lba, int __sectors,
                                            uintptr_t phys_buf);
int ahci_satapi_write(struct ata_dev_s *dev, size_t lba, int __sectors,
                                             uintptr_t phys_buf);


/*
 * General AHCI Block Read/Write Operations
 */
int ahci_strategy(struct disk_req_t *req)
{
    uint32_t block;
    int res;
    int sectors_per_block, sectors_to_read;
    int min = MINOR(req->dev);
    struct ata_dev_s *dev = ahci_disk_dev[min];
    struct parttab_s *part = ahci_disk_part[min];
    
    if(MAJOR(req->dev) != AHCI_DEV_MAJ || !dev)
    {
        printk("ahci_strategy: invalid device 0x%x\n", req->dev);
        return -ENODEV;
    }

    KDEBUG("ahci_strategy - dev 0x%x, lba 0x%x, block_no 0x%x, bps 0x%x\n", req->dev, part ? part->lba : 0, req->blockno, dev->bytes_per_sector);

    sectors_to_read = req->datasz / dev->bytes_per_sector;
    sectors_per_block = req->fs_blocksz / dev->bytes_per_sector;
    block = req->blockno * sectors_per_block;

    block += part ? part->lba : 0;

    KDEBUG("ata_strategy - sectors 0x%x, block 0x%x, lba 0x%x, block_no 0x%x, linear_addr 0x%x\n", sectors, block, part ? part->lba : 0, req->blockno, block * dev->bytes_per_sector);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(!req->write)
    {
        if(dev->type == IDE_SATA)
        {
            res = ahci_sata_read(dev, block, sectors_to_read,
                                 get_phys_addr(req->data));
        }
        else
        {
            res = ahci_satapi_read(dev, block, sectors_to_read,
                                   get_phys_addr(req->data));
        }
    }
    else
    {
        if(dev->type == IDE_SATA)
        {
            res = ahci_sata_write(dev, block, sectors_to_read,
                                  get_phys_addr(req->data));
        }
        else
        {
            res = ahci_satapi_write(dev, block, sectors_to_read,
                                    get_phys_addr(req->data));
        }
    }
    
    if(res != 0)
    {
        res = -EIO;
    }

    KDEBUG("ata_strategy: done\n");
    return res;
}


/*
 * General AHCI block device control function
 */
int ahci_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel)
{
    UNUSED(arg);
    UNUSED(kernel);
    
    int i, min = MINOR(dev_id);
    struct ata_dev_s *dev = ahci_disk_dev[min];
    //printk("ahci_ioctl: dev 0x%x\n", dev_id);
    
    if(!dev)
    {
        return -EINVAL;
    }
    
    i = (int)dev->bytes_per_sector;

    switch(cmd)
    {
        case DEV_IOCTL_GET_BLOCKSIZE:
            // get the block size in bytes
            return i;
    }
    
    return -EINVAL;
}


static inline void ahci_wait(int msecs)
{
    volatile unsigned long long last_ticks = ticks;

    while(msecs)
    {
        if(ticks != last_ticks)
        {
            msecs--;
            last_ticks = ticks;
        }

        __asm__ __volatile__("nop" ::: "memory");
    }
}


/*
 * Start command engine
 */
void ahci_start_cmd(HBA_PORT *port)
{
    // Wait until CR (bit15) is cleared
    while(port->cmd & HBA_PORT_CMD_CR)
    {
        ;
    }
 
    // Set FRE (bit4) and ST (bit0)
    port->cmd |= HBA_PORT_CMD_FRE;
    port->cmd |= HBA_PORT_CMD_ST; 
}
 

/*
 * Stop command engine
 */
void ahci_stop_cmd(HBA_PORT *port)
{
    // Clear ST (bit0)
    port->cmd &= ~HBA_PORT_CMD_ST;
 
    // Clear FRE (bit4)
    port->cmd &= ~HBA_PORT_CMD_FRE;
 
    // Wait until FR (bit14), CR (bit15) are cleared
    while(1)
    {
        if(port->cmd & HBA_PORT_CMD_FR)
        {
            continue;
        }
        
        if(port->cmd & HBA_PORT_CMD_CR)
        {
            continue;
        }

        break;
    }
}


/*
 * Find a free command list slot
 */
int find_cmdslot(HBA_PORT *port)
{
    // If not set in SACT and CI, the slot is free
    uint32_t slots = (port->sact | port->ci);
    int i;

    for(i = 0; i < 32 /* cmdslots */; i++)
    {
        if((slots & 1) == 0)
        {
            return i;
        }
        
        slots >>= 1;
    }

    printk("ahci: cannot find free command list entry\n");
    return -1;
}


static inline void setup_fis(FIS_REG_H2D *fis, uint8_t command,
                                               size_t lba, int sectors)
{
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = command;
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = (1 << 6);         // LBA mode
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = (sectors & 0xff);
    fis->counth = (sectors >> 8) & 0xff;
}


int wait_for_port(HBA_PORT *port, int slot, struct kernel_mutex_t *mutex)
{
    int spin = 0;       // Spin lock timeout counter

    while((port->tfd & (ATA_SR_BUSY  | ATA_SR_DRQ)) && spin < 1000000)
    {
        spin++;
    }
    
    if(spin == 1000000)
    {
        kernel_mutex_unlock(mutex);
        printk("ahci: port hung\n");
        return -1;
    }
    
    // issue command
    port->ci = (1 << slot);

    kernel_mutex_unlock(mutex);
    
    while(1)
    {
        if((port->ci & (1 << slot)) == 0)
        {
            //printk("slot done %d\n", slot);
            break;
        }
        
        if(port->is & HBA_PORT_IS_TFES)    // task file error
        {
            printk("ahci: disk read error\n");
            return -1;
        }
    }
    
    if(port->is & HBA_PORT_IS_TFES)    // task file error
    {
        printk("ahci: disk read error\n");
        return -1;
    }

    // wakeup any sleepers
    unblock_tasks((void *)port);
    
    return 0;
}


/*
 * Read sectors from a SATA disk
 */
int ahci_sata_read(struct ata_dev_s *dev, size_t lba, int __sectors,
                                          uintptr_t phys_buf)
{
    //int spin = 0;       // Spin lock timeout counter
    int slot, i;
    int sectors = __sectors;
    struct ahci_dev_t *ahci = dev->ahci;
    int port_index = dev->port_index;
    HBA_MEM *hba = (HBA_MEM *)ahci->iobase;
    HBA_PORT *port = &hba->ports[port_index];
    HBA_CMD_HEADER *cmd_hdr = (HBA_CMD_HEADER *)ahci->port_clb[port_index];
    HBA_CMD_TBL *table;
    //FIS_REG_H2D *fis;

    // Clear pending interrupt bits
    ////port->is = (uint32_t)-1;
    
    kernel_mutex_lock(&ahci->port_lock[port_index]);
    
    while((slot = find_cmdslot(port)) == -1)
    {
        kernel_mutex_unlock(&ahci->port_lock[port_index]);
        block_task2((void *)port, 50000);
        kernel_mutex_lock(&ahci->port_lock[port_index]);
    }
    
    cmd_hdr += slot;

    // Command FIS size
    cmd_hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    
    // Read from device
    cmd_hdr->w = 0;
    
    // PRDT entries count
    cmd_hdr->prdtl = (uint16_t)((sectors - 1) >> 4) + 1;
    
    // Set up the PRDT
    // for each entry (except the last), we can read upto 8kb (or 16 sectors)
    table = (HBA_CMD_TBL *)(ahci->port_ctba[port_index] + (256 * slot));

    for(i = 0; i < cmd_hdr->prdtl - 1; i++)
    {
        table->prdt_entry[i].dba = (phys_buf & 0xffffffff);
        table->prdt_entry[i].dbau = (phys_buf >> 32);
        table->prdt_entry[i].dbc = 0x2000 - 1;  // 8kb - 1
        table->prdt_entry[i].i = 1;
        phys_buf += 0x2000;      // 8kb
        sectors -= 16;      // 16 sectors
    }

    // set up the last entry
    table->prdt_entry[i].dba = (phys_buf & 0xffffffff);
    table->prdt_entry[i].dbau = (phys_buf >> 32);
    table->prdt_entry[i].dbc = (sectors << 9) - 1;    // 512 bytes per sectors
    table->prdt_entry[i].i = 1;

    // set up the command
    setup_fis((FIS_REG_H2D *)table->cfis, ATA_CMD_READ_DMA_EXT,
                                          lba, __sectors);
    
    return wait_for_port(port, slot, &ahci->port_lock[port_index]);
}


/*
 * Read sectors from a SATAPI disk
 */
int ahci_satapi_read_capacity(struct ata_dev_s *dev)
{
    int slot;
    struct ahci_dev_t *ahci = dev->ahci;
    int port_index = dev->port_index;
    HBA_MEM *hba = (HBA_MEM *)ahci->iobase;
    HBA_PORT *port = &hba->ports[port_index];
    HBA_CMD_HEADER *cmd_hdr = (HBA_CMD_HEADER *)ahci->port_clb[port_index];
    HBA_CMD_TBL *table;
    uintptr_t tmp_phys, tmp_virt;

    // Clear pending interrupt bits
    //port->is = (uint32_t)-1;


    if(get_next_addr(&tmp_phys, &tmp_virt, PTE_FLAGS_PW, REGION_DMA) != 0)
    {
        printk("ahci: insufficient memory to read disk capcity\n");
        return -1;
    }

    kernel_mutex_lock(&ahci->port_lock[port_index]);
    
    while((slot = find_cmdslot(port)) == -1)
    {
        kernel_mutex_unlock(&ahci->port_lock[port_index]);
        block_task2((void *)port, 50000);
        kernel_mutex_lock(&ahci->port_lock[port_index]);
    }
    
    cmd_hdr += slot;

    // Command FIS size
    cmd_hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    
    // Read from device
    cmd_hdr->w = 0;

    // ATAPI
    cmd_hdr->a = 1;
    
    // PRDT entries count
    cmd_hdr->prdtl = (uint16_t)1;
    
    // Set up the PRDT
    table = (HBA_CMD_TBL *)(ahci->port_ctba[port_index] + (256 * slot));
    table->prdt_entry[0].dba = (tmp_phys & 0xffffffff);
    table->prdt_entry[0].dbau = (tmp_phys >> 32);
    table->prdt_entry[0].dbc = 8;
    table->prdt_entry[0].i = 1;

    setup_fis((FIS_REG_H2D *)table->cfis, ATA_CMD_PACKET, 0, 1);

    // set up the command
    table->acmd[0 ] = 0x25;    /* READ CAPACITY */
    table->acmd[1 ] = 0;
    table->acmd[2 ] = 0;
    table->acmd[3 ] = 0;
    table->acmd[4 ] = 0;
    table->acmd[5 ] = 0;
    table->acmd[6 ] = 0;
    table->acmd[7 ] = 0;
    table->acmd[8 ] = 0;
    table->acmd[9 ] = 0;
    table->acmd[10] = 0;
    table->acmd[11] = 0;
    
    if(wait_for_port(port, slot, &ahci->port_lock[port_index]) != 0)
    {
        dev->size = 0;
        dev->bytes_per_sector = ATAPI_SECTOR_SIZE;
        vmmngr_unmap_page((void *)tmp_virt);
        return -EIO;
    }

    uint8_t *ide_buf = (uint8_t *)tmp_virt;
    long last_lba = ide_buf[3] | (ide_buf[2] << 8) |
                    (ide_buf[1] << 16) | (ide_buf[0] << 24);

    long block_len = ide_buf[7] | (ide_buf[6] << 8) |
                     (ide_buf[5] << 16) | (ide_buf[4] << 24);

    KDEBUG("atapi_read_capacity: last_lba 0x%x, block_len 0x%x\n", last_lba, block_len);

    dev->size = ((last_lba + 1) * block_len);
    dev->bytes_per_sector = block_len;

    vmmngr_unmap_page((void *)tmp_virt);
    
    return 0;
}


/*
 * Read sectors from a SATAPI disk
 */
int ahci_satapi_read(struct ata_dev_s *dev, size_t lba, int __sectors,
                                            uintptr_t phys_buf)
{
    //int spin = 0;       // Spin lock timeout counter
    int slot, i;
    int sectors = __sectors;
    struct ahci_dev_t *ahci = dev->ahci;
    int port_index = dev->port_index;
    HBA_MEM *hba = (HBA_MEM *)ahci->iobase;
    HBA_PORT *port = &hba->ports[port_index];
    HBA_CMD_HEADER *cmd_hdr = (HBA_CMD_HEADER *)ahci->port_clb[port_index];
    HBA_CMD_TBL *table;

    // Clear pending interrupt bits
    //port->is = (uint32_t)-1;

    // make sure we have the device capacity
    if(dev->size == 0)
    {
        if(ahci_satapi_read_capacity(dev) != 0)
        {
            printk("ahci: failed to read SATAPI device capacity\n");
        }
    }
    
    kernel_mutex_lock(&ahci->port_lock[port_index]);
    
    while((slot = find_cmdslot(port)) == -1)
    {
        kernel_mutex_unlock(&ahci->port_lock[port_index]);
        block_task2((void *)port, 50000);
        kernel_mutex_lock(&ahci->port_lock[port_index]);
    }
    
    cmd_hdr += slot;

    // Command FIS size
    cmd_hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    
    // Read from device
    cmd_hdr->w = 0;

    // ATAPI
    cmd_hdr->a = 1;
    
    // PRDT entries count
    cmd_hdr->prdtl = (uint16_t)((sectors - 1) >> 2) + 1;
    
    // Set up the PRDT
    // for each entry (except the last), we can read upto 8kb (or 16 sectors)
    table = (HBA_CMD_TBL *)(ahci->port_ctba[port_index] + (256 * slot));

    for(i = 0; i < cmd_hdr->prdtl - 1; i++)
    {
        table->prdt_entry[i].dba = (phys_buf & 0xffffffff);
        table->prdt_entry[i].dbau = (phys_buf >> 32);
        table->prdt_entry[i].dbc = 0x2000 - 1;  // 8kb - 1
        table->prdt_entry[i].i = 1;
        phys_buf += 0x2000;      // 8kb
        sectors -= 4;       // 4 sectors * 2048 bytes = 8kb
    }

    // set up the last entry
    table->prdt_entry[i].dba = (phys_buf & 0xffffffff);
    table->prdt_entry[i].dbau = (phys_buf >> 32);
    table->prdt_entry[i].dbc = (sectors << 11) - 1;   // 2048 bytes per sectors
    table->prdt_entry[i].i = 1;

    setup_fis((FIS_REG_H2D *)table->cfis, ATA_CMD_PACKET, lba, __sectors);

    // set up the command
    table->acmd[0 ] = ATAPI_CMD_READ;
    table->acmd[1 ] = 0;
    table->acmd[2 ] = (lba >> 24) & 0xFF;
    table->acmd[3 ] = (lba >> 16) & 0xFF;
    table->acmd[4 ] = (lba >>  8) & 0xFF;
    table->acmd[5 ] = (lba >>  0) & 0xFF;
    table->acmd[6 ] = 0;
    table->acmd[7 ] = 0;
    table->acmd[8 ] = 0;
    table->acmd[9 ] = __sectors;
    table->acmd[10] = 0;
    table->acmd[11] = 0;
    
    return wait_for_port(port, slot, &ahci->port_lock[port_index]);
}


/*
 * Write sectors to a SATA disk
 */
int ahci_sata_write(struct ata_dev_s *dev, size_t lba, int __sectors,
                                           uintptr_t phys_buf)
{
    //int spin = 0;       // Spin lock timeout counter
    int slot, i;
    int sectors = __sectors;
    struct ahci_dev_t *ahci = dev->ahci;
    int port_index = dev->port_index;
    HBA_MEM *hba = (HBA_MEM *)ahci->iobase;
    HBA_PORT *port = &hba->ports[port_index];
    HBA_CMD_HEADER *cmd_hdr = (HBA_CMD_HEADER *)ahci->port_clb[port_index];
    HBA_CMD_TBL *table;
    //FIS_REG_H2D *fis;

    // Clear pending interrupt bits
    ////port->is = (uint32_t)-1;
    
    kernel_mutex_lock(&ahci->port_lock[port_index]);
    
    while((slot = find_cmdslot(port)) == -1)
    {
        kernel_mutex_unlock(&ahci->port_lock[port_index]);
        block_task2((void *)port, 50000);
        kernel_mutex_lock(&ahci->port_lock[port_index]);
    }
    
    cmd_hdr += slot;

    // Command FIS size
    cmd_hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    
    // Write to device
    cmd_hdr->w = 1;
    
    // PRDT entries count
    cmd_hdr->prdtl = (uint16_t)((sectors - 1) >> 4) + 1;
    
    // Set up the PRDT
    // for each entry (except the last), we can read upto 8kb (or 16 sectors)
    table = (HBA_CMD_TBL *)(ahci->port_ctba[port_index] + (256 * slot));

    for(i = 0; i < cmd_hdr->prdtl - 1; i++)
    {
        table->prdt_entry[i].dba = (phys_buf & 0xffffffff);
        table->prdt_entry[i].dbau = (phys_buf >> 32);
        table->prdt_entry[i].dbc = 0x2000 - 1;  // 8kb - 1
        table->prdt_entry[i].i = 1;
        phys_buf += 0x2000;      // 8kb
        sectors -= 16;      // 16 sectors
    }

    // set up the last entry
    table->prdt_entry[i].dba = (phys_buf & 0xffffffff);
    table->prdt_entry[i].dbau = (phys_buf >> 32);
    table->prdt_entry[i].dbc = (sectors << 9) - 1;    // 512 bytes per sectors
    table->prdt_entry[i].i = 1;

    // set up the command
    setup_fis((FIS_REG_H2D *)table->cfis, ATA_CMD_WRITE_DMA_EXT,
                                          lba, __sectors);

    return wait_for_port(port, slot, &ahci->port_lock[port_index]);
}


/*
 * Write sectors to a SATAPI disk
 */
int ahci_satapi_write(struct ata_dev_s *dev, size_t lba, int __sectors,
                                             uintptr_t phys_buf)
{
    UNUSED(dev);
    UNUSED(__sectors);
    UNUSED(lba);
    UNUSED(phys_buf);
    
    return -EROFS;
}


void ahci_register_dev(struct ata_dev_s *dev, struct parttab_s *part, int n)
{
    /*
     * We name SATA devices following Linux's method of naming SCSI:
     *    0 = /dev/sda          First SCSI disk - whole disk
     *   16 = /dev/sdb          Second SCSI disk - whole disk
     *   32 = /dev/sdc          Third SCSI disk - whole disk
     *      ...
     *  240 = /dev/sdp          Sixteenth SCSI disk - whole disk
     *
     * Partitions are handled as for IDE disks.
     * See: https://www.kernel.org/doc/Documentation/admin-guide/devices.txt
     */
    static int disk = -1;
    char name[] = { 's', 'd', '?', '\0', '\0', '\0' };
    int min;
    //int basenum = (16 * dev->ahci->pci->unit);
    int basenum;
    
    // new disk?
    if(!part)
    {
        disk++;
    }
    
    basenum = 16 * disk;
    
    if(basenum > 240)
    {
        printk("ahci: maximum number of disks reached (16) - skipping disk\n");
        return;
    }
    
    min = basenum + n;
    name[2] = 'a' + disk;
    //name[2] = 'a' + dev->ahci->pci->unit;
    
    // add partition number if needed
    if(part)
    {
        int j = 3;

        if(n >= 10)
        {
            name[j++] = '0' + (int)(n / 10);
        }

        name[j] = '0' + (int)(n % 10);
    }

    KDEBUG("ahci_register_dev: %s, 0x%x\n", name, TO_DEVID(AHCI_DEV_MAJ, min));
    //empty_loop();
    
    add_dev_node(name, TO_DEVID(AHCI_DEV_MAJ, min), (S_IFBLK | 0664));
    ahci_disk_dev[min] = dev;
    ahci_disk_part[min] = part;

    // if SATAPI, add a cdrom device node
    if(dev->type & 1)
    {
        add_cdrom_device(TO_DEVID(AHCI_DEV_MAJ, min), (S_IFBLK | 0664));
        //empty_loop();
    }
}


/* partition table offsets */
static int mbr_offset[] = { 0x1be, 0x1ce, 0x1de, 0x1ee };
static uint8_t gpt_hdr_magic[] = "EFI PART";


/*
 * Read the given device's GUID Partition Table (GPT).
 *
 * For details on GPT partition table format, see:
 *    https://wiki.osdev.org/GPT
 */
void ahci_read_gpt(struct ata_dev_s *dev, uintptr_t phys_buf,
                                          uintptr_t virt_buf)
{
    uint8_t *ide_buf = (uint8_t *)virt_buf;
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
    if(ahci_sata_read(dev, gpthdr_lba, 1, phys_buf) != 0)
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
    if(ahci_sata_read(dev, gptent_lba, 1, phys_buf) != 0)
    {
        printk("  Skipping disk with invalid GPT entries\n");
        return;
    }

    while(gptent_count--)
    {
        if(off >= dev->bytes_per_sector)
        {
            // Read the next set of partition entries
            if(ahci_sata_read(dev, ++gptent_lba, 1, phys_buf) != 0)
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
        
        ahci_register_dev(dev, part, dev_index);
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
void ahci_read_mbr(struct ata_dev_s *dev, uintptr_t phys_buf,
                                          uintptr_t virt_buf)
{
    int i;
    uint8_t *ide_buf = (uint8_t *)virt_buf;

    A_memset((void *)virt_buf, 0, 512);

    /* Read the MBR */
    //KDEBUG("  Reading the MBR..\n");
    //__asm__ ("xchg %%bx, %%bx"::);

    if(ahci_sata_read(dev, 0, 1, phys_buf) != 0)
    {
        printk("  Failed to read disk MBR - skipping\n");
        return;
    }

    /* verify the boot signature */
    /*
    if(ide_buf[0x1fe] != 0x55 || ide_buf[0x1ff] != 0xaa)
    {
        printk("  Skipping disk with invalid boot signature (%x %x)\n",
                ide_buf[0x1fe], ide_buf[0x1ff]);
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
            ahci_read_gpt(dev, phys_buf, virt_buf);
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
        
        ahci_register_dev(dev, part, i + 1);
    }

    //KDEBUG("  Finished reading the MBR..\n");
    //__asm__ ("xchg %%bx, %%bx"::);
}


/*
 * Identify SATA disk
 */
int ahci_sata_identify(struct ahci_dev_t *ahci, int port_index,
                                                uintptr_t phys_buf)
{
    int spin = 0;       // Spin lock timeout counter
    int slot;
    HBA_MEM *hba = (HBA_MEM *)ahci->iobase;
    HBA_PORT *port = &hba->ports[port_index];
    HBA_CMD_HEADER *cmd_hdr = (HBA_CMD_HEADER *)ahci->port_clb[port_index];
    HBA_CMD_TBL *table;
    //FIS_REG_H2D *fis;

    // Clear pending interrupt bits
    port->is = (uint32_t)-1;
    
    if((slot = find_cmdslot(port)) == -1)
    {
        return -1;
    }
    
    //printk("buf 0x%lx, port %d, using slot %d\n", phys_buf, port_index, slot);
    //printk("cmd_hdr @ 0x%lx\n", cmd_hdr);
    
    cmd_hdr += slot;

    // Command FIS size
    cmd_hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    
    // Read from device
    cmd_hdr->w = 0;
    
    // PRDT entries count
    cmd_hdr->prdtl = (uint16_t)1;
    
    // Set up the PRDT
    table = (HBA_CMD_TBL *)(ahci->port_ctba[port_index] + (256 * slot));
    table->prdt_entry[0].dba = (phys_buf & 0xffffffff);
    table->prdt_entry[0].dbau = (phys_buf >> 32);
    table->prdt_entry[0].dbc = 511;
    table->prdt_entry[0].i = 1;

    //printk("table @ 0x%lx\n", table);
    
    setup_fis((FIS_REG_H2D *)table->cfis, ATA_CMD_IDENTIFY, 0, 1);
    
    //printk("fis @ 0x%lx\n", fis);
    
    while((port->tfd & (ATA_SR_BUSY  | ATA_SR_DRQ)) && spin < 1000000)
    {
        spin++;
    }
    
    if(spin == 1000000)
    {
        printk("ahci: port hung\n");
        return -1;
    }
    
    //printk("spin %d\n", spin);
    
    port->ci = (1 << slot);
    
    while(1)
    {
        if((port->ci & (1 << slot)) == 0)
        {
            //printk("slot done %d\n", slot);
            break;
        }
    }
    
    return 0;
}


/*
 * Identify SATAPI disk
 */
int ahci_satapi_identify(struct ahci_dev_t *ahci, int port_index,
                                                  uintptr_t phys_buf)
{
    int spin = 0;       // Spin lock timeout counter
    int slot;
    HBA_MEM *hba = (HBA_MEM *)ahci->iobase;
    HBA_PORT *port = &hba->ports[port_index];
    HBA_CMD_HEADER *cmd_hdr = (HBA_CMD_HEADER *)ahci->port_clb[port_index];
    HBA_CMD_TBL *table;
    //FIS_REG_H2D *fis;

    // Clear pending interrupt bits
    port->is = (uint32_t)-1;
    
    if((slot = find_cmdslot(port)) == -1)
    {
        return -1;
    }
    
    cmd_hdr += slot;

    // Command FIS size
    cmd_hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    
    // Read from device
    cmd_hdr->w = 0;

    // not ATAPI for this command
    cmd_hdr->a = 0;
    
    // PRDT entries count
    cmd_hdr->prdtl = (uint16_t)1;
    
    // Set up the PRDT
    table = (HBA_CMD_TBL *)(ahci->port_ctba[port_index] + (256 * slot));
    table->prdt_entry[0].dba = (phys_buf & 0xffffffff);
    table->prdt_entry[0].dbau = (phys_buf >> 32);
    table->prdt_entry[0].dbc = 511;
    table->prdt_entry[0].i = 1;

    setup_fis((FIS_REG_H2D *)table->cfis, ATA_CMD_IDENTIFY_PACKET, 0, 0);
    
    while((port->tfd & (ATA_SR_BUSY  | ATA_SR_DRQ)) && spin < 1000000)
    {
        spin++;
    }
    
    if(spin == 1000000)
    {
        printk("ahci: port hung\n");
        return -1;
    }
    
    port->ci = (1 << slot);
    
    while(1)
    {
        if((port->ci & (1 << slot)) == 0)
        {
            break;
        }
    }
    
    return 0;
}


/*
 * Initialize SATA disk
 */
void ahci_sata_init(struct ahci_dev_t *ahci, int port_index, int type)
{
    int i, l;
    uintptr_t ctb_phys, clb_phys, ctb_virt, clb_virt;
    uintptr_t tmp_phys, tmp_virt;
    HBA_MEM *hba = (HBA_MEM *)ahci->iobase;
    HBA_PORT *port = &hba->ports[port_index];
    HBA_CMD_HEADER *cmd_list;
    //HBA_FIS *fis_dev;
    struct ata_dev_s *dev;

    if(!(dev = kmalloc(sizeof(struct ata_dev_s))))
    {
        printk("ahci: insufficient memory to init SATA device\n");
        return;
    }
    
    A_memset(dev, 0, sizeof(struct ata_dev_s));

    /* stop the DMA engine */
    ahci_stop_cmd(port);
    
#define PAGE_FLAGS      (PTE_FLAGS_PW | I86_PTE_NOT_CACHEABLE)

    /*
     * We allocate 2 pages for the command table
     * See below for details on the command table size calculation
     */
    ctb_virt = vmmngr_alloc_and_map(PAGE_SIZE * 2, 1, PAGE_FLAGS,
                                    &ctb_phys, REGION_DMA);

    if(!ctb_virt)
    {
        printk("ahci: insufficient memory for the command table\n");
        kfree(dev);
        return;
    }
    
    /* alloc memory for the command list and FIS */
    if(get_next_addr(&clb_phys, &clb_virt, PAGE_FLAGS, REGION_DMA) != 0)
    {
        printk("ahci: insufficient memory for the command list and the FIS\n");
        kfree(dev);
        return;
    }

    A_memset((void *)clb_virt, 0, PAGE_SIZE);
    A_memset((void *)ctb_virt, 0, PAGE_SIZE * 2);

#undef PAGE_FLAGS
    
    /*
     * Command list entry size = 32
     * Command list entry maxim count = 32
     * Command list maximum size = 32 * 32 = 1K per port
     * We allocate one page for both the command list and the FIS
     */
    ahci->port_clb[port_index] = clb_virt;
    port->clb = clb_phys & 0xffffffff;
    port->clbu = clb_phys >> 32;
    cmd_list = (HBA_CMD_HEADER *)clb_virt;

    /*
     * FIS entry size = 256 bytes per port
     */
    ahci->port_fb[port_index] = clb_virt + 1024;
    port->fb = (clb_phys + 1024) & 0xffffffff;
    port->fbu = (clb_phys + 1024) >> 32;
    //fis_dev = (HBA_FIS *)(clb_virt + 1024);

    /*
     * 256 bytes per command table
     * 32 entries per command table
     * Command table size = 256 * 32 = 8K per port
     */
    ahci->port_ctba[port_index] = ctb_virt;

    for(i = 0; i < 32; i++)
    {
        // 8 prdt entries per command table
        cmd_list[i].prdtl = 8;
        cmd_list[i].ctba = (ctb_phys & 0xffffffff);
        cmd_list[i].ctbau = (ctb_phys >> 32);
        cmd_list[i].p = 1;
        cmd_list[i].cfl = 0x10;
        ctb_phys += 256;
    }

    port->serr = 0xffffffff;
    port->cmd &= ~HBA_PORT_CMD_ICC;
    // power-up, spin-up, activate link
    port->cmd |= PORT_CMD_POD | PORT_CMD_SUD | HBA_PORT_CMD_ICC_ACTIVE;
    port->ie = 0xfdc000ff;
    
    /* start the command DMA engine */
    ahci_start_cmd(port);

    /*
     * Send the identify command. We will use a temporary page for this
     */
    uint8_t *ide_buf;
    
    if(get_next_addr(&tmp_phys, &tmp_virt, PTE_FLAGS_PW, REGION_DMA) != 0)
    {
        printk("ahci: insufficient memory to read device info\n");
        kfree(dev);
        return;
    }

    A_memset((void *)tmp_virt, 0, PAGE_SIZE);
    
    if(type == IDE_SATA)
    {
        l = ahci_sata_identify(ahci, port_index, tmp_phys);
    }
    else
    {
        l = ahci_satapi_identify(ahci, port_index, tmp_phys);
    }
    
    if(l < 0)
    //if(ahci_sata_identify(ahci, port_index, tmp_phys) < 0)
    {
        vmmngr_unmap_page((void *)tmp_virt);
        kfree(dev);
        return;
    }
    
    ide_buf = (uint8_t *)tmp_virt;
    
    // read device parameters
    dev->type = type;
    //dev->type = IDE_SATA;
    dev->irq = ahci->pci->irq[0];
    dev->base = ahci->iobase;
    dev->ahci = ahci;
    dev->port_index = port_index;

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

    if(type == IDE_SATA)
    {
        /* read ATA device capacity */
        dev->heads = U32(ide_buf, ATA_IDENT_HEADS);
        dev->cylinders = U32(ide_buf, ATA_IDENT_CYLINDERS);
        dev->sectors = U32(ide_buf, ATA_IDENT_SECTORS);
        dev->bytes_per_sector = U16(ide_buf, ATA_IDENT_BYTES_PER_SECTOR);
        //KDEBUG("block_len = %lu\n", dev->bytes_per_sector);

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
    else
    {
        /*
        if(ahci_satapi_read_capacity(dev) != 0)
        {
            printk("ahci: failed to read SATAPI device capacity\n");
        }
        */

        dev->size = 0;
        dev->bytes_per_sector = ATAPI_SECTOR_SIZE;
    }
    
    printk("  %s disk:\n", (type == IDE_SATA) ? "SATA" : "SATAPI");
    printk("    Model = %s\n", dev->model);
    printk("    Serial = %s, ", dev->serial);
    printk("Firmware = %s\n", dev->firmware);

    if(type == IDE_SATA)
    {
        printk("    Capacity = %luMB\n", dev->size / 1024 / 1024);

        // add the new SATA device and read the MBR
        ahci_register_dev(dev, NULL, 0);
        ahci_read_mbr(dev, tmp_phys, tmp_virt);
    }
    else
    {
        // add the new SATAPI device
        ahci_register_dev(dev, NULL, 0);
    }
    
    vmmngr_unmap_page((void *)tmp_virt);
}


/*
 * Check device type
 */
static inline int ahci_check_type(HBA_PORT *port)
{
    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;
 
    if(det != HBA_PORT_DET_PRESENT)    // Check drive status
    {
        return AHCI_DEV_NULL;
    }
    
    if(ipm != HBA_PORT_IPM_ACTIVE)
    {
        return AHCI_DEV_NULL;
    }
 
    switch(port->sig)
    {
        case SATA_SIG_ATAPI:
            return AHCI_DEV_SATAPI;

        case SATA_SIG_SEMB:
            return AHCI_DEV_SEMB;

        case SATA_SIG_PM:
            return AHCI_DEV_PM;

        default:
            return AHCI_DEV_SATA;
    }
}


/*
 * Initialise AHCI disk devices.
 */
void ahci_init(struct pci_dev_t *pci)
{
    /*
     * See: https://wiki.osdev.org/AHCI
     *      https://wiki.osdev.org/PCI
     */
    int i;
    //char buf[8];
    uint16_t cmd;
    uint32_t ports;
    uintptr_t bar5;
    struct ahci_dev_t *ahci;
    HBA_MEM *hba;
    
    cmd = pci_config_read(pci->bus, pci->dev, pci->function, PCI_COMMAND);
    cmd |= (1 << 1) | (1 << 10);
    pci_config_write(pci->bus, pci->dev, pci->function, PCI_COMMAND, cmd);

    if(!(ahci = kmalloc(sizeof(struct ahci_dev_t))))
    {
        printk("ahci: insufficient memory to init device\n");
        return;
    }
    
    A_memset(ahci, 0, sizeof(struct ahci_dev_t));
    
    printk("ahci: found an AHCI device controller\n");

#define BAR5_OFFSET             0x24

    // determine the size of the BAR
    bar5 = pci->bar[5];
    pci_config_write_long(pci->bus, pci->dev, pci->function,
                          BAR5_OFFSET, 0xffffffff);
    ahci->iosize = pci_config_read_long(pci->bus, pci->dev, pci->function,
                                        BAR5_OFFSET);
    ahci->iosize &= ~0xf;    // mask the lower 4 bits
    ahci->iosize = (~(ahci->iosize) & 0xffffffff) + 1;     // invert and add 1
    pci_config_write_long(pci->bus, pci->dev, pci->function,
                          BAR5_OFFSET, bar5);
    bar5 = pci_config_read_long(pci->bus, pci->dev, pci->function,
                                BAR5_OFFSET);
    
    //printk("bar0 0x%lx, hda->iosize 0x%lx\n", bar0, hda->iosize);

#undef BAR5_OFFSET
    
    // check whether I/O is memory-mapped or normal I/O
    if((pci->bar[5] & 0x1))
    {
        // I/O - should be MMIO
        printk("achi: ignoring device with port-based IO\n");
        kfree(ahci);
        return;
    }
    else
    {
        // MMIO
        bar5 = pci->bar[5] & ~0xf;
        ahci->iobase = mmio_map(bar5, bar5 + ahci->iosize);
    }

    pci->unit = last_unit++;
    ahci->pci = pci;
    
    printk("ahci: bar5 0x%lx, ahci->iobase 0x%lx, ahci->iosize 0x%lx\n",
           bar5, ahci->iobase, ahci->iosize);
    //printk("IRQ %d, cmd 0x%x\n", pci->irq[0], cmd);

    if(!first_ahci)
    {
        first_ahci = ahci;
    }
    else
    {
        struct ahci_dev_t *tmp = first_ahci;

        while(tmp->next)
        {
            tmp = tmp->next;
        }

        tmp->next = ahci;
    }

    pci_enable_busmastering(pci);
    pci_enable_interrupts(pci);
    pci_enable_memoryspace(pci);

    // register IRQ handler
    /*
    ksprintf(buf, 8, "ahci%d", pci->unit);
    (void)start_kernel_task(buf, ahci_task_func, ahci, &ahci->task, KERNEL_TASK_ELEVATED_PRIORITY);
    */
    pci_register_irq_handler(pci, ahci_intr, "ahci");
    
    // start the controller and tell it we know it exists
    hba = (HBA_MEM *)ahci->iobase;
    hba->ghc = 1;
    ahci_wait(50);
    hba->ghc = hba->ghc | (1 << 31);
    ahci_wait(50);
    
    printk("ahci: ver %d.%d, cap 0x%x, cmdslots %d, ports 0x%x\n",
           (hba->vs >> 16), (hba->vs & 0xff),
           hba->cap, CMD_SLOTS(hba), hba->pi);
    
    ports = hba->pi;
    
    for(i = 0; i < 32; i++)
    {
        if(ports & 1)
        {
            int dt = ahci_check_type(&hba->ports[i]);
            
            if (dt == AHCI_DEV_SATA)
            {
                printk("ahci: SATA drive found at port %d\n", i);
                hba->ports[i].sctl &= ~PX_SCTL_IPM_MASK;
                hba->ports[i].sctl |= PX_SCTL_IPM_NONE;
                ahci_sata_init(ahci, i, IDE_SATA);
            }
            else if (dt == AHCI_DEV_SATAPI)
            {
                printk("ahci: SATAPI drive found at port %d\n", i);
                hba->ports[i].sctl &= ~PX_SCTL_IPM_MASK;
                hba->ports[i].sctl |= PX_SCTL_IPM_NONE;
                ahci_sata_init(ahci, i, IDE_SATAPI);
            }
            else if (dt == AHCI_DEV_SEMB)
            {
                printk("ahci: SEMB drive found at port %d\n", i);
            }
            else if (dt == AHCI_DEV_PM)
            {
                printk("ahci: PM drive found at port %d\n", i);
            }
            else
            {
                printk("ahci: No drive found at port %d\n", i);
            }
        }
        
        ports >>= 1;
    }
    
    //screen_refresh(NULL);
    //empty_loop();
}


/*
 * AHCI interrupt handler.
 */
int ahci_intr(struct regs *r, int unit)
{
    UNUSED(r);

    KDEBUG("ahci_intr:\n");
    //screen_refresh(NULL);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    struct ahci_dev_t *ahci = first_ahci;
    uint32_t isr;
    int i;
    HBA_MEM *hba;

    while(ahci)
    {
        if(ahci->pci->unit == unit)
        {
            break;
        }

        ahci = ahci->next;
    }

    // device not found
    if(!ahci)
    {
        return 0;
    }

    hba = (HBA_MEM *)ahci->iobase;
    
    // not our IRQ
    if((isr = hba->is) == 0)
    {
        return 0;
    }

    for(i = 0; i < 32; i++)
    {
        if((hba->is & hba->pi & (1 << i)))
        {
            uint32_t pisr = hba->ports[i].is;
            
            KDEBUG("ahci: IRQ from port %d: status 0x%x\n", i, pisr);
            
            hba->ports[i].is = pisr;
            //break;
        }
    }
    
    hba->is = isr;
    pic_send_eoi(ahci->pci->irq[0]);

    return 1;
}

