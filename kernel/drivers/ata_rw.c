/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ata_rw.c
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
 *  \file ata_rw.c
 *
 *  This file impelements the functions used by the kernel to perform ATA
 *  (Advanced Technology Attachment) read and write operations.
 *  This file handles both PATA and PATAPI (DVD or CD-ROM) devices.
 *  The rest of the ATA group of functions can be found in ata2.c and
 *  ata_irq.c.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/ata.h>
#include <kernel/io.h>
#include <kernel/task.h>
#include <mm/dma.h>

extern struct ata_devtab_s tab1;     // for devices with maj == 3
extern struct ata_devtab_s tab2;     // for devices with maj == 22

// function prototypes

static int ata_read_pio(struct ata_dev_s *dev, unsigned char numsects,
                        size_t lba, virtual_addr buf);
                        //size_t lba, struct IO_buffer_s *buf);
static int ata_write_pio(struct ata_dev_s *dev, unsigned char numsects,
                         size_t lba, virtual_addr buf);
                         //size_t lba, struct IO_buffer_s *buf);
static int ata_read_dma(struct ata_dev_s *dev, unsigned char numsects,
                        size_t lba, virtual_addr buf);
                        //size_t lba, struct IO_buffer_s *buf);
static int ata_write_dma(struct ata_dev_s *dev, unsigned char numsects,
                         size_t lba, virtual_addr buf);
                         //size_t lba, struct IO_buffer_s *buf);
static int atapi_read_pio(struct ata_dev_s *dev, unsigned char numsects,
                          size_t lba, virtual_addr buf);
                          //size_t lba, struct IO_buffer_s *buf);
static int atapi_write_pio(struct ata_dev_s *dev, unsigned char numsects,
                           size_t lba, virtual_addr buf);
                           //size_t lba, struct IO_buffer_s *buf);
static int atapi_read_capacity(struct ata_dev_s *dev);


/*
 * General Block Read/Write Operations.
 */
int ata_strategy(struct disk_req_t *req)
{
    uint32_t block;
    int sectors_per_block, sectors_to_read;
    int min = MINOR(req->dev);
    struct ata_devtab_s *tab = (MAJOR(req->dev) == 3) ? &tab1 : &tab2;
    struct ata_dev_s *dev = tab->dev[min];
    struct parttab_s *part = tab->part[min];
    
    if(!dev)
    {
        printk("ata_strategy: invalid device 0x%x\n", req->dev);
        return -ENODEV;
    }

    sectors_to_read = req->datasz / dev->bytes_per_sector;
    sectors_per_block = req->fs_blocksz / dev->bytes_per_sector;
    block = req->blockno * sectors_per_block;

    block += part ? part->lba : 0;

    //printk("ata_strategy: block %d, sectors_to_read %d\n", block, sectors_to_read);

    return ata_add_req(dev, block, sectors_to_read, req->data, req->write, NULL);
}


/*
 * Wait on a device until it is ready.
 */
void select_drive(struct ata_dev_s *dev)
{
    static uint16_t last_base = 0;
    // avoid 0 & 1 the default slavebit values in Initialization
    static unsigned int last_slavebit = -1;
    unsigned int slavebit = MS(dev);

    /* if this is not the last drive selected, select it */
    if(last_base != dev->base || last_slavebit != slavebit)
    {
        last_base = dev->base;
        last_slavebit = slavebit;

        //drive select
        outb(dev->base + ATA_REG_DRVHD, (slavebit << 4));
  
        //delay for 400 nanoseconds
        for(int i = 0; i < 4; i++)
        {
            inb(dev->ctrl + ATA_REG_ALT_STATUS);
        }
    }
}


/*
 * Wait on an ATA device.
 */
int ata_wait(struct ata_dev_s *dev, unsigned char mask, unsigned int timeout)
{
    /* wait for drive to be ready */
    volatile unsigned char res;
    volatile unsigned int wait = 0;
    
    while(1)
    {
        res = inb(dev->ctrl + ATA_REG_ALT_STATUS);
        
        // check for error or device failure
        if((res & ATA_SR_ERR) || (res & ATA_SR_DF))
        {
            printk("ata_wait: status 0x%x\n", res);

            res = inb(dev->base + ATA_REG_ERR);

            printk("ata_wait: err 0x%x\n", res);

            if(res & ATA_ER_AMNF)
            {
                return -EBADR;
            }

            if(res & ATA_ER_TK0NF)
            {
                return -ENOMEDIUM;
            }

            if(res & ATA_ER_ABRT)
            {
                return -EIO;
            }

            if(res & ATA_ER_MCR)
            {
                return -ENOMEDIUM;
            }

            if(res & ATA_ER_IDNF)
            {
                return -EBADR;
            }

            if(res & ATA_ER_MC)
            {
                return -ENOMEDIUM;
            }

            if(res & ATA_ER_UNC)
            {
                return -EIO;
            }

            if(res & ATA_ER_BBK)
            {
                return -EBADR;
            }

            return -EIO;
        }
        
        if(!(res & ATA_SR_BUSY) && ((res & mask) == mask))
        {
            return 0;
        }
        
        if(++wait >= timeout)
        {
            return -ETIMEDOUT;
        }

        lock_scheduler();
        scheduler();
        unlock_scheduler();
    }
}


int ata_wait_busy(struct ata_dev_s *dev)
{
    volatile unsigned char res;
    volatile unsigned int wait = 0;

    while(1)
    {
        res = inb(dev->ctrl + ATA_REG_ALT_STATUS);

        if(!(res & ATA_SR_BUSY))
        {
            return 0;
        }
        
        if(++wait >= TIMEOUT_BUSY)
        {
            return -ETIMEDOUT;
        }

        lock_scheduler();
        scheduler();
        unlock_scheduler();
    }
}


void calc_address(struct ata_dev_s *dev, size_t lba, unsigned char *lba_io,
                  unsigned char *lba_mode, unsigned char *head)
{
    unsigned short cyl;
    unsigned char sect;

    // select LBA28, LBA48 or CHS
    if(lba >= 0x10000000)
    {
        //LBA48
        *lba_mode = 2;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = 0;
        lba_io[5] = 0;
        *head     = 0;
    }
    else if(dev->capabilities & 0x200)
    {
        //LBA28
        *lba_mode = 1;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        *head     = (lba & 0xF000000)>>24;
    }
    else
    {
        //CHS
        *lba_mode = 0;
        sect      = (lba%63)+1;
        cyl       = (lba+1-sect)/(16*63);
        lba_io[0] = sect;
        lba_io[1] = (cyl>>0) & 0xFF;
        lba_io[2] = (cyl>>8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        *head     = (lba + 1 - sect)%(16*63)/(63);
    }
}


void ata_setup_transfer(struct ata_dev_s *dev, unsigned char numsects,
                        unsigned char *lba_io,
                        unsigned char lba_mode, unsigned char head)
{
    if(lba_mode == 2)
    {
        //LBA48
        outb(dev->base + ATA_REG_SECTORCNT, 0);
        outb(dev->base + ATA_REG_SECTOR, lba_io[3]);
        outb(dev->base + ATA_REG_TRACKLSB, lba_io[4]);
        outb(dev->base + ATA_REG_TRACKMSB, lba_io[5]);
        outb(dev->base + ATA_REG_SECTORCNT, numsects);
        outb(dev->base + ATA_REG_SECTOR, lba_io[0]);
        outb(dev->base + ATA_REG_TRACKLSB, lba_io[1]);
        outb(dev->base + ATA_REG_TRACKMSB, lba_io[2]);
        outb(dev->base + ATA_REG_DRVHD, 0x40 | (MS(dev) << 4));
    }
    else if(lba_mode == 1)
    {
        //LBA28
        outb(dev->base + ATA_REG_FEATURE, 0x00);
        outb(dev->base + ATA_REG_SECTORCNT, numsects);
        outb(dev->base + ATA_REG_SECTOR, lba_io[0]);
        outb(dev->base + ATA_REG_TRACKLSB, lba_io[1]);
        outb(dev->base + ATA_REG_TRACKMSB, lba_io[2]);
        outb(dev->base + ATA_REG_DRVHD, 0xE0 | (MS(dev) << 4) | head);
    }
    else
    {
        //CHS
        outb(dev->base + ATA_REG_FEATURE, 0x00);
        outb(dev->base + ATA_REG_SECTORCNT, numsects);
        outb(dev->base + ATA_REG_SECTOR, lba_io[0]);
        outb(dev->base + ATA_REG_TRACKLSB, lba_io[1]);
        outb(dev->base + ATA_REG_TRACKMSB, lba_io[2]);
        outb(dev->base + ATA_REG_DRVHD, 0xA0 | (MS(dev) << 4) | head);
    }
}


/*
 * Read sectors from an ATA device.
 */
int ata_read_sectors(struct ata_dev_s *dev, unsigned char numsects,
                     size_t lba, virtual_addr buf)
                     //size_t lba, struct IO_buffer_s *buf)
{
    int res = 0;
    
    if(numsects == 0)
    {
        return 0;
    }
  
    if(!dev)
    {
        res = -ENODEV;
    }
    else if(((lba+numsects) > dev->size) && (dev->type & 1) == 0)
    {
        // PATA or SATA
        res = -EBADR;
    }
    else
    {
        if((dev->type & 1) == 0)        // PATA or SATA
        {
            KDEBUG("ata_read_sectors: 1 - uses_dma %d\n", dev->uses_dma);

            if(dev->uses_dma)
            {
                res = ata_read_dma(dev, numsects, lba, buf);
            }
            else
            {
                res = ata_read_pio(dev, numsects, lba, buf);
            }
        }
        else if(dev->type & 1)          // PATAPI or SATAPI
        {
            /*
            if(dev->uses_dma)
            {
                res = atapi_read_dma(dev, numsects, lba, buf);
            }
            else
            */
            {
                res = atapi_read_pio(dev, numsects, lba, buf);
            }
        }
        else
        {
            printk("ata: reading from unknown device type (0x%x)\n",
                   dev->type);
            kpanic("halting\n");
        }
    }
    
    return res;
}


/*
 * Write sectors to an ATA device.
 */
int ata_write_sectors(struct ata_dev_s *dev, unsigned char numsects,
                      size_t lba, virtual_addr buf)
                      //size_t lba, struct IO_buffer_s *buf)
{
    int res = 0;

    if(numsects == 0)
    {
        return 0;
    }
  
    if(!dev)
    {
        res = -ENODEV;
    }
    else if(((lba+numsects) > dev->size) && (dev->type & 1) == 0)
    {
        // PATA or SATA
        res = -EBADR;
    }
    else
    {
        if((dev->type & 1) == 0)        // PATA or SATA
        {
            if(dev->uses_dma)
            {
                res = ata_write_dma(dev, numsects, lba, buf);
            }
            else
            {
                res = ata_write_pio(dev, numsects, lba, buf);
            }
        }
        else if(dev->type & 1)          // PATAPI or SATAPI
        {
            /*
            if(dev->uses_dma)
            {
                res = atapi_write_dma(dev, numsects, lba, buf);
            }
            else
            */
            {
                res = atapi_write_pio(dev, numsects, lba, buf);
            }
        }
        else
        {
            printk("ata: writing to unknown device type (0x%x)\n", dev->type);
            kpanic("halting\n");
        }
    }
    
    return res;
}


int ata_read_pio(struct ata_dev_s *dev, unsigned char numsects,
                 size_t lba, virtual_addr buf)
                 //size_t lba, struct IO_buffer_s *buf)
{
    unsigned char i, lba_mode, head;
    unsigned char cmd = 0;
    unsigned int bps = dev->bytes_per_sector;
    unsigned char lba_io[6];
    void *edi = (void *)buf /* ->data */;
    int res;

    KDEBUG("ata_read_pio: lba 0x%lx, buf 0x%lx\n", lba, buf);

    if(ata_wait_busy(dev) != 0)
    {
        return -EBUSY;
    }
    
    calc_address(dev, lba, lba_io, &lba_mode, &head);
    select_drive(dev);
    
    if((res = ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY)) != 0)
    {
        return res;
    }

    cmd = (lba_mode != 2) ? ATA_CMD_READ_PIO : ATA_CMD_READ_PIO_EXT;

    ata_setup_transfer(dev, numsects, lba_io, lba_mode, head);
    outb(dev->base + ATA_REG_COMMAND, cmd);

    //delay for 400 nanoseconds
    ata_delay(dev->ctrl + ATA_REG_ALT_STATUS);

    for(i = 0; i < numsects; i++)
    {
        res = ide_wait_irq();

        if(res != 0)
        {
            printk("ata: PIO read failed");
            return -EIO;
        }

        if((res = ata_wait(dev, ATA_SR_DRQ, TIMEOUT_DRQ)) != 0)
        {
            return res;
        }

        insw(dev->base + ATA_REG_DATA, edi, bps / 2);
        edi = (void *)((char *)edi + bps);
    }

    return 0;
}


int ata_write_pio(struct ata_dev_s *dev, unsigned char numsects,
                  size_t lba, virtual_addr buf)
                  //size_t lba, struct IO_buffer_s *buf)
{
    unsigned char i, lba_mode, head;
    unsigned char cmd = 0;
    unsigned int bps = dev->bytes_per_sector;
    unsigned char lba_io[6];
    void *edi = (void *)buf /* ->data */;
    int res;

    if(ata_wait_busy(dev) != 0)
    {
        return -EBUSY;
    }

    calc_address(dev, lba, lba_io, &lba_mode, &head);
    select_drive(dev);
    
    if((res = ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY)) != 0)
    {
        return res;
    }

    cmd = (lba_mode != 2) ? ATA_CMD_WRITE_PIO : ATA_CMD_WRITE_PIO_EXT;

    ata_setup_transfer(dev, numsects, lba_io, lba_mode, head);
    outb(dev->base + ATA_REG_COMMAND, cmd);
    
    for(i = 0; i < numsects; i++)
    {
        if((res = ata_wait(dev, ATA_SR_DRQ, TIMEOUT_DRQ)) != 0)
        {
            return res;
        }

        outsw(dev->base + ATA_REG_DATA, edi, bps / 2);

        res = ide_wait_irq();

        if(res != 0)
        {
            printk("ata: PIO write failed");
            return -EIO;
        }

        edi = (void *)((char *)edi + bps);
    }
    
    /*
     * TODO: the flush command
     */

    return 0;
}


int ata_setup_dma(struct ata_dev_s *dev, unsigned char numsects,
                  virtual_addr buf, int iswrite)
                  //struct IO_buffer_s *buf, int iswrite)
{
    size_t bytes = numsects * dev->bytes_per_sector;
    volatile uint8_t status;
    
    if(ata_dma_prepare(dev, bytes) != 0)
    {
        kpanic("ata: error setting up DMA");
        return -EIO;
    }

    if(iswrite)
    {
        A_memcpy((void *)dev->dma_buf_virt, (void *)buf /* ->data */, bytes);
    }

    // setup PRDT
    outl(dev->bmide + ATA_BUS_MASTER_REG_PRDT, dev->PRDT_phys);
    
    // set r/w command
    outb(dev->bmide + ATA_BUS_MASTER_REG_COMMAND,
            (iswrite ? 0x00 : 0x08) | ATA_DMA_END);

    // clear the INTR and ERR flags
    status = inb(dev->bmide + ATA_BUS_MASTER_REG_STATUS);
    status |= (ATA_DMA_ERROR | ATA_IRQ_PENDING);
    outb(dev->bmide + ATA_BUS_MASTER_REG_STATUS, status);
    
    return 0;
}


int ata_read_dma(struct ata_dev_s *dev, unsigned char numsects,
                 size_t lba, virtual_addr buf)
                 //size_t lba, struct IO_buffer_s *buf)
{
    unsigned char lba_mode, head;
    unsigned char cmd = 0;
    unsigned int bps = dev->bytes_per_sector;
    unsigned char lba_io[6];
    int res;
    volatile uint8_t status;

    if(ata_wait_busy(dev) != 0)
    {
        return -EBUSY;
    }
    
    calc_address(dev, lba, lba_io, &lba_mode, &head);
    select_drive(dev);
    
    if((res = ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY)) != 0)
    {
        return res;
    }

    cmd = (lba_mode != 2) ? ATA_CMD_READ_DMA : ATA_CMD_READ_DMA_EXT;

    // setup transfer and DMA
    ata_setup_transfer(dev, numsects, lba_io, lba_mode, head);
    ata_setup_dma(dev, numsects, buf, 0);
    
    // start the read
    outb(dev->base + ATA_REG_COMMAND, cmd);
    status = inb(dev->bmide + ATA_BUS_MASTER_REG_STATUS);
    outb(dev->bmide + ATA_BUS_MASTER_REG_COMMAND, status | ATA_DMA_START);
    
    // wait for IRQ
    res = ide_wait_irq();

    if(res != 0)
    {
        printk("ata: DMA read failed\n");
        //buf->flags |= IOBUF_FLAG_ERROR;
        return -EIO;
    }

    /*
    if(buf->flags & IOBUF_FLAG_ERROR)
    {
        KDEBUG("ide_ata_dma_access - dma error\n");
        return -EIO;
    }
    */
    
    A_memcpy((void *)buf /* ->data */, (void *)dev->dma_buf_virt, numsects * bps);

    /*
    printk("ata_read_dma: dev->dma_buf_virt 0x%lx\n", dev->dma_buf_virt);
    char *p = buf;
    for(unsigned int z = 0; z < 60; z++) printk("%x", p[z]);
    printk("\n");
    */

    return 0;
}


int ata_write_dma(struct ata_dev_s *dev, unsigned char numsects,
                  size_t lba, virtual_addr buf)
                  //size_t lba, struct IO_buffer_s *buf)
{
    unsigned char lba_mode, head;
    unsigned char cmd = 0;
    unsigned char lba_io[6];
    int res;
    volatile uint8_t status;

    if(ata_wait_busy(dev) != 0)
    {
        return -EBUSY;
    }
    
    calc_address(dev, lba, lba_io, &lba_mode, &head);
    select_drive(dev);
    
    if((res = ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY)) != 0)
    {
        return res;
    }

    cmd = (lba_mode != 2) ? ATA_CMD_WRITE_DMA : ATA_CMD_WRITE_DMA_EXT;

    // setup transfer and DMA
    ata_setup_transfer(dev, numsects, lba_io, lba_mode, head);
    ata_setup_dma(dev, numsects, buf, 1);
    
    // start the write
    outb(dev->base + ATA_REG_COMMAND, cmd);
    status = inb(dev->bmide + ATA_BUS_MASTER_REG_STATUS);
    outb(dev->bmide + ATA_BUS_MASTER_REG_COMMAND, status | ATA_DMA_START);
    
    // wait for IRQ
    res = ide_wait_irq();

    if(res != 0)
    {
        printk("ata: DMA write failed");
        //buf->flags |= IOBUF_FLAG_ERROR;
        return -EIO;
    }

    /*
    if(buf->flags & IOBUF_FLAG_ERROR)
    {
        KDEBUG("ide_ata_dma_access - dma error\n");
        return -EIO;
    }
    */

    return 0;
}


static int atapi_read_packet(struct ata_dev_s *dev,
                      unsigned char *packet, int packetsz,
                      void *buf, size_t bufsz, int ignore_nomedium)
{
    int res;
    size_t left = bufsz;
    
    if(ata_wait_busy(dev) != 0)
    {
        return -EBUSY;
    }

    select_drive(dev);
    
    if((res = ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY)) != 0)
    {
        if(res != -ENOMEDIUM || !ignore_nomedium)
        {
            KDEBUG("atapi_read_packet: 1 res %d\n", res);
            return res;
        }
    }
    
    // setup registers
    outb(dev->base + ATA_REG_FEATURE, 0);
    outb(dev->base + ATA_REG_SECTORCNT, 0);
    outb(dev->base + ATA_REG_SECTOR, 0);
    outb(dev->base + ATA_REG_TRACKLSB, (unsigned char)(bufsz & 0xFF));
    outb(dev->base + ATA_REG_TRACKMSB, (unsigned char)(bufsz >> 8));
    outb(dev->base + ATA_REG_DRVHD, MS(dev) << 4);

    // send packet command
    outb(dev->base + ATA_REG_COMMAND, ATA_CMD_PACKET);

    // wait for drive to be ready
    if((res = ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY)) != 0)
    {
        KDEBUG("atapi_read_packet: 2 res %d\n", res);
        return res;
    }
    
    // send packet data
    outsw(dev->base + ATA_REG_DATA, packet, packetsz / 2);

    //receive data
    while(left)
    {
        KDEBUG("atapi_read_packet: left 0x%x\n", left);
        //__asm__("xchg %%bx, %%bx"::);

        res = ide_wait_irq();

        KDEBUG("atapi_read_packet: 3 res 0x%x\n", res);
        //__asm__("xchg %%bx, %%bx"::);

        if(res != 0)
        {
            //buf->flags |= IOBUF_FLAG_ERROR;
            return -EIO;
        }
        
        // stop if device indicates end of command
        if((inb(dev->base + ATA_REG_STATUS) &
                (ATA_SR_BUSY | ATA_SR_DRQ)) == 0)
        {
            break;
        }

        // get byte count
        unsigned short bytes = inb(dev->base + ATA_REG_TRACKLSB) |
                               (inb(dev->base + ATA_REG_TRACKMSB) << 8);

        KDEBUG("atapi_read_packet: bytes 0x%x\n", bytes);
        //__asm__("xchg %%bx, %%bx"::);

        if(bytes == 0)
        {
            break;
        }
        
        if(bytes > left)
        {
            printk("atapi: buffer overrun\n");
            return -ENOBUFS;
        }

        // read data
        insw(dev->base + ATA_REG_DATA, buf, bytes / 2);

	    buf = (void *)((char *)buf + bytes);
	    left -= bytes;
    }
    
    // drive will send another IRQ - wait for it so it doesn't interfere
    // with subsequent operations
    ide_wait_irq();
    ata_wait_busy(dev);

    KDEBUG("atapi: done\n");
    
    return 0;
}


int atapi_read_pio(struct ata_dev_s *dev, unsigned char numsects,
                   size_t lba, virtual_addr buf)
                   //size_t lba, struct IO_buffer_s *buf)
{
    unsigned char packet[12];
    int res;
    
    // make sure we have the device capacity
    if(dev->size == 0)
    {
        if(atapi_read_capacity(dev) != 0)
        {
            printk("ata: failed to read ATAPI device capacity\n");
        }
    }
    
    // setup SCSI packet
    memset(packet, 0, 12);
    packet[0 ] = ATAPI_CMD_READ;
    packet[2 ] = (lba >> 24) & 0xFF;
    packet[3 ] = (lba >> 16) & 0xFF;
    packet[4 ] = (lba >>  8) & 0xFF;
    packet[5 ] = (lba >>  0) & 0xFF;
    packet[9 ] = numsects;
    
    // do the read
    res = atapi_read_packet(dev, packet, 12, (void *)buf,
                            numsects * dev->bytes_per_sector, 0);
    /*
    if((res = atapi_read_packet(dev, packet, 12, (void *)buf->data,
                                numsects * dev->bytes_per_sector)) != 0)
    {
        buf->flags |= IOBUF_FLAG_ERROR;
    }
    */
    
    return res;
}


int atapi_read_capacity(struct ata_dev_s *dev)
{
    unsigned char packet[12];
    unsigned char buf[8];
    int res;
    
    // setup SCSI packet
    memset(packet, 0, 12);
    packet[0 ] = 0x25;    /* READ CAPACITY */
    //packet[4 ] = 0x02;

    KDEBUG("atapi_read_capacity:\n");
    //__asm__("xchg %%bx, %%bx"::);
    
    if((res = atapi_read_packet(dev, packet, 12, buf, 8, 0)) != 0)
    {
        dev->size = 0;
        dev->bytes_per_sector = ATAPI_SECTOR_SIZE;
        return -EIO;
    }

    long last_lba = buf[3] | (buf[2] << 8) |
                    (buf[1] << 16) | (buf[0] << 24);

    long block_len = buf[7] | (buf[6] << 8) |
                     (buf[5] << 16) | (buf[4] << 24);

    KDEBUG("atapi_read_capacity: last_lba 0x%x, block_len 0x%x\n", last_lba, block_len);

    dev->size = ((last_lba + 1) * block_len);
    dev->bytes_per_sector = block_len;
    
    return 0;
}


int atapi_write_pio(struct ata_dev_s *dev, unsigned char numsects,
                    size_t lba, virtual_addr buf)
                    //size_t lba, struct IO_buffer_s *buf)
{
    UNUSED(dev);
    UNUSED(numsects);
    UNUSED(lba);
    UNUSED(buf);
    
    return -EROFS;
}


/*
 * Send a TEST UNIT READY (0x00) command to the ATAPI device.
 * Parameter addr should point to a 2-byte buffer. The status register
 * will be returned in the first byte and the error register will be
 * returned in the second byte.
 */
int atapi_test_unit_ready(struct ata_dev_s *dev, virtual_addr addr)
{
    unsigned char packet[12];
    int status, err, res;
    uint8_t *buf = (uint8_t *)addr;
    
    if(!dev)
    {
        KDEBUG("atapi_test_unit_ready: invalid device\n");
        return -ENODEV;
    }

    if(!(dev->type & 1))     // PATA or SATA
    {
        KDEBUG("atapi_test_unit_ready: device is not ATAPI\n");
        return -ENODEV;
    }

    // setup SCSI packet
    memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_TEST_UNIT_READY;

    res = atapi_read_packet(dev, packet, 12, NULL, 0, 1);
    /*
    if((res = atapi_read_packet(dev, packet, 12, NULL, 0)) != 0)
    {
        printk("atapi_test_unit_ready: device err %d\n", res);
        dev->size = 0;
        dev->bytes_per_sector = ATAPI_SECTOR_SIZE;
        return res;
    }
    */

    status = inb(dev->base + ATA_REG_STATUS);
    err = inb(dev->base + ATA_REG_ERR);

    KDEBUG("atapi_test_unit_ready: status 0x%x, err 0x%x\n", status, err);

    buf[0] = status;
    buf[1] = err;
    //screen_refresh(NULL);
    //for(;;);

    return res;
}


/*
 * Send a REQUEST SENSE (0x03) command to the ATAPI device.
 * Parameter addr should point to a buffer that is 18 bytes in size.
 * The sense data returned by the device will be stored there.
 */
int atapi_request_sense(struct ata_dev_s *dev, virtual_addr addr)
{
    unsigned char packet[12];
    
    // setup SCSI packet
    memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_REQUEST_SENSE;
    packet[4] = 18;

    return atapi_read_packet(dev, packet, 12, (void *)addr, 18, 1);
}


/*
int atapi_lock_media(dev_t devid, uint8_t *__status, uint8_t *__err)
{
    int status, err, res;
    int min = MINOR(devid);
    struct ata_devtab_s *tab = (MAJOR(devid) == 3) ? &tab1 : &tab2;
    struct ata_dev_s *dev = tab->dev[min];
    
    if(!dev)
    {
        printk("atapi_lock_media: invalid device 0x%x\n", devid);
        return -ENODEV;
    }

    if(!(dev->type & 1))     // PATA or SATA
    {
        printk("atapi_lock_media: device 0x%x not ATAPI\n", devid);
        return -ENODEV;
    }

    if(ata_wait_busy(dev) != 0)
    {
        return -EBUSY;
    }

    select_drive(dev);
    
    if((res = ata_wait(dev, ATA_SR_DRDY, TIMEOUT_DRDY)) != 0)
    {
        return res;
    }

    outb(dev->base + ATA_REG_COMMAND, ATAPI_CMD_MEDIA_LOCK);
    ata_delay(dev->base + ATA_REG_STATUS);
    status = inb(dev->base + ATA_REG_STATUS);
    err = inb(dev->base + ATA_REG_ERR);

    printk("atapi_lock_media: status 0x%x, err 0x%x\n", status, err);
    screen_refresh(NULL);
    for(;;);

    *__status = status;
    *__err = err;

    return 0;
}
*/

