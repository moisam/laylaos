/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ata_irq.c
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
 *  \file ata_irq.c
 *
 *  This file impelements the functions used by the kernel to send ATA
 *  (Advanced Technology Attachment) device requests and wait for IRQs.
 *  It also contains the ATA disk I/O function, disk_task_func(), which
 *  handles all ATA read/write requests.
 *  The rest of the ATA group of functions can be found in ata2.c and
 *  ata_rw.c.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/asm.h>
#include <kernel/syscall.h>
#include <kernel/ata.h>
//#include <kernel/bio.h>
#include <kernel/pic.h>
#include <kernel/io.h>

#define NR_REQUESTS         32

struct ata_request_t
{
    int write, err;
    struct ata_dev_s *dev;
    size_t lba;
    unsigned char numsects, irq, active;
    virtual_addr buf;
    //struct IO_buffer_s *buf;
    struct ata_request_t *next;
    int wait_channel;
    int res;
    int (*func)(struct ata_dev_s *, virtual_addr);
                                      /**< if this request is being served by 
                                           a special function, this is a 
                                           pointer to the function */
};

struct ata_request_t requests[NR_REQUESTS];
volatile struct ata_request_t *cur_request = NULL;
struct kernel_mutex_t request_lock;

volatile int serving = 0;
int request_wait_channel;
int irq_wait_channel;
struct task_t *disk_task = NULL;

//volatile unsigned char ide_irq_invoked = 0;

static void ata_do_request(void);


static inline void init_bufs(void)
{
    static int inited = 0;
    
    if(!inited)
    {
        inited = 1;
        memset(requests, 0, sizeof(struct ata_request_t) * NR_REQUESTS);
        init_kernel_mutex(&request_lock);
    }
}


/*
 * Request an ATA I/O operation.
 */
int ata_add_req(struct ata_dev_s *dev,
                size_t lba, unsigned char numsects,
                virtual_addr buf, int write,
                int (*func)(struct ata_dev_s *, virtual_addr))
                //struct IO_buffer_s *buf)
{
    KDEBUG("ata_add_req:\n");
    struct ata_request_t *tmp, *req, *lreq = &requests[NR_REQUESTS];

    init_bufs();
    
    if(!dev || !numsects /* || !buf */)
    {
        return -EINVAL;
    }

    // you have to either read into a buffer, or provide a function that
    // will handle the request
    if(!buf && !func)
    {
        return -EINVAL;
    }

    elevated_priority_lock(&request_lock);

loop:

    for(req = requests; req < lreq; req++)
    {
        if(req->dev == NULL && req->active == 0)
        {
            req->dev = dev;
            break;
        }
    }

    elevated_priority_unlock(&request_lock);
    
    if(req == lreq)
    {
        //block_task(&request_wait_channel, 0);
        block_task2(&request_wait_channel, 1000);
        elevated_priority_relock(&request_lock);
        goto loop;
    }

    KDEBUG("ata_add_req: req @ 0x%x\n", req);

    req->active = 1;
    req->buf = buf;
    req->lba = lba;
    req->numsects = numsects;
    req->next = NULL;
    req->irq = 0;
    req->res = 0;
    req->write = write;
    req->err = 0;
    req->func = func;
    
    //uintptr_t s = int_off();
    elevated_priority_relock(&request_lock);
    
    if(cur_request == NULL)
    {
        cur_request = req;
    }
    else if(cur_request->next == NULL)
    {
        cur_request->next = req;
    }
    else
    {
        for(tmp = cur_request->next; tmp->next; tmp = tmp->next)
        {
            if(tmp->lba <= req->lba)
            {
                break;
            }
        }
        
        req->next = tmp->next;
        tmp->next = req;
    }

    //int_on(s);
    elevated_priority_unlock(&request_lock);

    KDEBUG("ata_add_req: serving %d, disk_task 0x%lx\n", serving, disk_task);
    
    /*
    if(buf->flags & IOBUF_FLAG_READ)
    {
        req->numsects++;
    }
    */
    
    if(!serving)
    {
        serving = 1;
        KDEBUG("ata_add_req: unblocking disk task @ 0x%lx\n", disk_task);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        unblock_task(disk_task);
    }

    KDEBUG("ata_add_req: 3 - req->wait_channel @ 0x%x\n", &req->wait_channel);
    
    // The disk daemon might have run before us, as it has lower priority than
    // user tasks. Sleep for some time and then wake up and check if the
    // I/O operation was performed.

    volatile unsigned char active = req->active;
    int res;

    while(active)
    {
        block_task2(&req->wait_channel, 500);
        active = req->active;
        KDEBUG("ata_add_req: active %d\n", active);
    }
    
    res = req->res;
    req->dev = NULL;
    req->numsects = 0;

    KDEBUG("ata_add_req: done\n");
    
    //return req->res ? -EIO : (int)buf->block_size;
    return res ? -EIO : (int)(numsects * dev->bytes_per_sector);
}


/*
 * Kernel disk task function.
 */
void disk_task_func(void *arg)
{
    UNUSED(arg);
    
    init_bufs();

    while(1)
    {
        while(!serving)
        {
            block_task2(&disk_task, 6000);
            //block_task(&disk_task, 0);
        }
        
        ata_do_request();
    }
}


static void ata_do_request(void)
{
    KDEBUG("ata_do_request:\n");
    
    if(cur_request == NULL)
    //if(cur_request == NULL || cur_request->buf == 0 /* NULL */)
    {
        serving = 0;
        return;
    }
    
    if(cur_request->func)
    {
        cur_request->res = cur_request->func(cur_request->dev,
                                             cur_request->buf);
    }
    else
    {
        if(!cur_request->write)
        //if(cur_request->buf->flags & IOBUF_FLAG_READ)
        {
            cur_request->res = ata_read_sectors(cur_request->dev,
                                                cur_request->numsects,
                                                cur_request->lba,
                                                cur_request->buf);
        }
        else
        {
            cur_request->res = ata_write_sectors(cur_request->dev,
                                                 cur_request->numsects,
                                                 cur_request->lba,
                                                 cur_request->buf);
        }
    }

    volatile struct ata_request_t *tmp = cur_request;
    cur_request->active = 0;
    //cur_request->dev = NULL;
    elevated_priority_lock(&request_lock);
    cur_request = cur_request->next;
    elevated_priority_unlock(&request_lock);
    unblock_tasks((void *)&tmp->wait_channel);
    unblock_tasks(&request_wait_channel);
}


/*
 * Wait for a disk IRQ.
 */
int ide_wait_irq(void)
{
    KDEBUG("Waiting for IRQ\n");
    
    if(!cur_request)
    {
        kpanic("Waiting for IRQ but cur_request == NULL");
        empty_loop();
    }

    volatile unsigned char irq = cur_request->irq;
    volatile int timeout = 500000;
    
    /*
     * There is a small window of time between checking ide_irq_invoked and 
     * the task sleeping, during which the IRQ can occur and we sleep
     * indefinitely. So instead of blocking the task, just wait for the IRQ
     * to happen, which should be soon anyway as the actual read/write is
     * being done in ide_access().
     */
    
    while(1)
    {
        sti();
        
        if(irq)
        {
            KDEBUG("no need to wait for IRQ\n");
            break;
        }
        
        if(--timeout == 0)
        {
            break;
        }
        
        KDEBUG("Still waiting!\n");

        lock_scheduler();
        scheduler();
        unlock_scheduler();

        irq = cur_request->irq;
    }
    
    if(cur_request->irq == 0)
    {
        volatile uint8_t status = inb(cur_request->dev->bmide +
                                      ATA_BUS_MASTER_REG_STATUS);
        uint8_t missed_irq = (status & ATA_IRQ_PENDING);
        printk("!!! status = 0x%x\n", status);
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        status = (ATA_DMA_ERROR | ATA_IRQ_PENDING);
        outb(cur_request->dev->bmide + ATA_BUS_MASTER_REG_STATUS, status);

        //delay for 400 nanoseconds
        ata_delay(cur_request->dev->ctrl + ATA_REG_ALT_STATUS);

        // read the device status register
        status = inb(cur_request->dev->base + ATA_REG_STATUS);

        //return -EAGAIN;
        return missed_irq ? 0 : -EAGAIN;
    }

    cur_request->irq--;
    KDEBUG("cur_request->irq %d\n", cur_request->irq);

    //return 0;
    return cur_request->err;
}


/*
 * Disk IRQ callback function.
 */
int ide_irq_callback(struct regs *r, int arg)
{
    UNUSED(arg);
    volatile uint8_t int_no = (r->int_no & 0xFF);
    
    //ide_irq_invoked = 1;
    
    if(!cur_request)
    {
        printk("Unexpected IRQ %d\n", int_no - 32);
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        return 0;
    }
    
    // check if the IRQ came from this device
    volatile uint8_t status = inb(cur_request->dev->bmide +
                                  ATA_BUS_MASTER_REG_STATUS);
    int_no = cur_request->dev->irq;
    
    if(!(status & ATA_IRQ_PENDING))
    {
        //printk("IRQ %d not from this device\n", int_no - 32);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        // IRQ not coming from this device
        return 0;
    }

    KDEBUG("IRQ %d, status 0x%x\n", int_no, status);
    
    // reset the Start/Stop bit if using DMA
    if(cur_request->dev->uses_dma)
    {
        uint8_t cmd = inb(cur_request->dev->bmide +
                          ATA_BUS_MASTER_REG_COMMAND);

        cmd &= ~ATA_DMA_START;
        outb(cur_request->dev->bmide + ATA_BUS_MASTER_REG_COMMAND, cmd);
        
        if(status & ATA_DMA_ERROR)
        {
            //cur_request->buf->flags |= IOBUF_FLAG_ERROR;
            cur_request->err = -EIO;
        }
        else
        {
            //cur_request->buf->flags &= ~IOBUF_FLAG_ERROR;
            cur_request->err = 0;
        }

        // clear the ERR flags
        status |= ATA_DMA_ERROR;
    }

    // clear the IRQ flag
    //status |= ATA_REQ_PENDING;
    outb(cur_request->dev->bmide + ATA_BUS_MASTER_REG_STATUS, status);

    //delay for 400 nanoseconds
    ata_delay(cur_request->dev->ctrl + ATA_REG_ALT_STATUS);

    // read the device status register
    status = inb(cur_request->dev->base + ATA_REG_STATUS);

    cur_request->numsects--;
    cur_request->err = 0;
    
    cli();

    //ide_irq_invoked = 1;
    volatile unsigned char irq = cur_request->irq;
    //cur_request->irq++;
    cur_request->irq = irq + 1;
    
    pic_send_eoi(int_no);
    
    sti();
    
    return 1;
}

