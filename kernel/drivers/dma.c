/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: dma.c
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
 *  \file dma.c
 *
 *  The Direct Memory Access (DMA) device driver.
 */

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <mm/dma.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kstack.h>
#include <kernel/laylaos.h>
#include <kernel/mutex.h>


struct PRD_entry_t
{
    uint32_t addr;
    uint16_t count;
    uint16_t reserved;
    //physical_addr PRD_phys;
} __attribute__((packed));


int ata_dma_init(struct ata_dev_s *dev)
{
    if(!(dev->PRDT_phys = (physical_addr)pmmngr_alloc_dma_blocks(1)))
    {
        printk("dma: failed to allocate memory\n");
        return -ENOMEM;
    }
    
    dev->PRDT_virt = phys_to_virt(dev->PRDT_phys,
                                  PTE_FLAGS_PW, REGION_DMA);
    memset((void *)dev->PRDT_virt, 0, PAGE_SIZE);

    if(!(dev->dma_buf_phys = (physical_addr)pmmngr_alloc_dma_blocks(1)))
    {
        printk("dma: failed to allocate memory\n");
        return -ENOMEM;
    }

    dev->dma_buf_virt = phys_to_virt(dev->dma_buf_phys,
                                     PTE_FLAGS_PW, REGION_DMA);
    
    dev->dma_buf_size = 1 * PAGE_SIZE;

    return 0;
}


int ata_dma_prepare(struct ata_dev_s *dev, size_t sz)
{
    if(!dev)
    {
        kpanic("dma: invalid dev");
        return -EINVAL;
    }
    
    // we only support 4kb (i.e. one memory page) transfers for now
    // also, the LSBit must be 0
    if(!sz || (sz & 1) || (sz > 0x1000))
    {
        kpanic("dma: invalid byte count");
        return -EINVAL;
    }
    
    struct PRD_entry_t *PRDT = (struct PRD_entry_t *)dev->PRDT_virt;
    PRDT[0].addr = (uint32_t)dev->dma_buf_phys;

    if(sz == 0xFFFF)
    {
        PRDT[0].count = (uint16_t)0;
    }
    else
    {
        PRDT[0].count = (uint16_t)sz;
    }

    PRDT[0].reserved = (uint16_t)0x8000;
    
    KDEBUG("ata_dma_prepare: PRDT_phys 0x%x, PRDT_virt 0x%x\n", dev->PRDT_phys, dev->PRDT_virt);
    KDEBUG("ata_dma_prepare: buf_phys 0x%x, buf_virt 0x%x\n", dev->dma_buf_phys, dev->dma_buf_virt);
    KDEBUG("ata_dma_prepare: addr 0x%x, sz 0x%x, res 0x%x\n", PRDT[0].addr, PRDT[0].count, PRDT[0].reserved);
    
    return 0;
}

