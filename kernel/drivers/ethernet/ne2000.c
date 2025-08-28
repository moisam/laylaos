/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ne2000.c
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
 *  \file ne2000.c
 *
 *  NE2000 device driver implementation.
 */

//#define __DEBUG

#define __USE_MISC

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <net/if.h>         // IFF_* flags
#include <sys/param.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/io.h>
#include <kernel/pic.h>
#include <kernel/asm.h>
#include <kernel/net.h>
#include <kernel/net/ne2000.h>
#include <kernel/net/ether.h>
#include <kernel/net/packet.h>

#define NE2000_DEVS             1

#define PSTART                  0x46
#define PSTOP                   0x80
#define TRANSMITBUFFER          0x40

// how many outgoing packets we can keep in queue
#define MAX_OUT_PACKETS         128


static inline void OUTB(uint16_t port, uint8_t command)
{
  __asm__ __volatile__ (
   "nop; nop;\n"
   "out %%al, %%dx\n"
   "nop; nop;"
   :
   :"d"(port),"a"(command)
   :);
}

static inline uint8_t INB(uint16_t port)
{
  uint8_t res;
  __asm__ __volatile__ (
   "nop; nop;\n"
   "in %%dx, %%al\n"
   "nop; nop;"
   :"=a"(res)
   :"d"(port)
   :);
  return (uint8_t)res;
}


struct ne2000_t ne2000_dev[NE2000_DEVS];


/* Receive ring descriptor */
struct receive_ring_desc_t
{
    unsigned char rsr;                   // Receiver status
    unsigned char next_packet;           // Pointer to next packet
    unsigned short count;                // Bytes in packet (length + 4)
} __attribute__((packed));


#define NE2000_IN_BUFFER_COUNT          32
#define NE2000_IN_BUFFER_SIZE           2048
#define NE2000_IN_BUFFER_TOTALMEM       (PAGE_SIZE * 16)

uint32_t ne2000_in_buffer_use_bitmap = 0;
char *ne2000_in_buffers = NULL;
uint32_t ne2000_in_packet_bitmap = 0;

static void ne2000_func(void *arg);


static void ne2000_packet_free(struct packet_t *_p)
{
    uint32_t i;
    char *p = (char *)_p;
    
    if(p < ne2000_in_buffers ||
       p >= (ne2000_in_buffers + NE2000_IN_BUFFER_TOTALMEM))
    {
        KDEBUG("ne2000: free: ignoring packet with invalid addr\n");
        return;
    }
    
    i = (p - ne2000_in_buffers) / NE2000_IN_BUFFER_SIZE;
    ne2000_in_buffer_use_bitmap &= ~(1 << i);
}


/*
 * This function's code is similar to ne2000_alloc_packet() from 
 * network/packet.c, modified to use our static buffers instead of dynamically
 * allocating buffers on the kernel heap. This is to avoid racing conditions
 * and possible deadlocks when an IRQ interrupt happens while some other
 * task is holding the kernel heap's lock.
 */
static struct packet_t *ne2000_alloc_packet(size_t len)
{
    struct packet_t *p;
    static size_t min_size = sizeof(struct packet_t);
    volatile int i;

    if((len + min_size) > NE2000_IN_BUFFER_SIZE)
    {
        printk("ne2000: requested packet size larger than buffer size\n");
        return NULL;
    }
    
    for(i = 0; i < NE2000_IN_BUFFER_COUNT; i++)
    {
        if(!(ne2000_in_buffer_use_bitmap & (1 << i)))
        {
            break;
        }
    }
    
    if(i == NE2000_IN_BUFFER_COUNT)
    {
        printk("ne2000: full internal buffers (bitmap 0x%x)\n", 
               ne2000_in_buffer_use_bitmap);
        return NULL;
    }

    ne2000_in_buffer_use_bitmap |= (1 << i);
    //ne2000_in_packet_bitmap |= (1 << i);
    p = (struct packet_t *)(ne2000_in_buffers + (i * NE2000_IN_BUFFER_SIZE));

    A_memset(p, 0, sizeof(struct packet_t) + len);
    p->data = ((uint8_t *)p + sizeof(struct packet_t));
    p->head = p->data;
    p->end = p->data + len;
    p->refs = 1;
    p->count = len;
    p->free_packet = ne2000_packet_free;
    
    return p;
}


int ne2000_init(struct pci_dev_t *pci)
{
    int unit = 0;
    
    if(!pci)
    {
        return -EINVAL;
    }
    
    register struct ne2000_t *ne = &ne2000_dev[unit];
    register int i;

    init_kernel_mutex(&ne->outq.lock);
    ne->outq.max = MAX_OUT_PACKETS;

    volatile uint8_t val;
    uint8_t prom[32];

    pci->unit = 0;
    ne->iobase = pci->bar[0] & ~0x3;
    ne->dev = pci;
    init_kernel_mutex(&ne->lock);


	/*
	 * Allocate internal buffers. Each buffer is 2048 bytes (half a page).
	 * We allocate 8 pages for a total 16 buffers.
	 *
	 * We do this early on because once we enabled our IRQ, we might start
	 * receiving them before we have our buffers set.
	 *
	 * NOTE: is 2048 actually enough per packet?
	 *       are 16 buffers enough in case we get overrun by incoming packets?
	 */
    if(!(ne2000_in_buffers = (char *)
            vmmngr_alloc_and_map(NE2000_IN_BUFFER_TOTALMEM, 0,
                                 PTE_FLAGS_PW, NULL, REGION_DMA)))
    {
        printk("net: failed to alloc ne2000 receive buffers\n");
        return -ENOMEM;
    }
    
    ne2000_in_buffer_use_bitmap = 0;
    ne2000_in_packet_bitmap = 0;

    (void)start_kernel_task("ne2000", ne2000_func, (void *)(uintptr_t)pci->unit, &ne->task, 0);

    pci_register_irq_handler(pci, ne2000_intr, "ne2000");

    /* Reset the device */
    val = INB(ne->iobase + REG_NE_RESET);
    OUTB(ne->iobase + REG_NE_RESET, val);

    val = INB(ne->iobase + REG_INTERRUPT_STATUS);
    
    while((val & 0x80) == 0)
    {
        val = INB(ne->iobase + REG_INTERRUPT_STATUS);
    }

    OUTB(ne->iobase + REG_INTERRUPT_STATUS, 0xFF);
    OUTB(ne->iobase + REG_COMMAND, 0x21);
    OUTB(ne->iobase + REG_DATA_CONFIGURATION, 0x58);
    OUTB(ne->iobase + REG_REMOTE_BYTECOUNT0, 0x20);
    OUTB(ne->iobase + REG_REMOTE_BYTECOUNT1, 0x00);
    OUTB(ne->iobase + REG_REMOTE_STARTADDRESS0, 0x00);
    OUTB(ne->iobase + REG_REMOTE_STARTADDRESS1, 0x00);
    OUTB(ne->iobase + REG_COMMAND, CR_START | CR_RREAD);
    OUTB(ne->iobase + REG_RECEIVE_CONFIGURATION, 0x0E);
    OUTB(ne->iobase + REG_TRANSMIT_CONFIGURATION, 0x04);
    
    for(i = 0; i < 32; i++)
    {
	    prom[i] = INB(ne->iobase + REG_NE_DATA);
	}

    OUTB(ne->iobase + REG_TRANSMIT_PAGE, 0x40);
    OUTB(ne->iobase + REG_PAGESTART, 0x46);
    OUTB(ne->iobase + REG_BOUNDARY, 0x46);
    OUTB(ne->iobase + REG_PAGESTOP, 0x60);
    OUTB(ne->iobase + REG_INTERRUPTMASK, 0x1F);
    OUTB(ne->iobase + REG_COMMAND, 0x61);
    OUTB(ne->iobase + REG_COMMAND, 0x61);
    OUTB(ne->iobase + REG_COMMAND + 0x01, prom[0]);
    OUTB(ne->iobase + REG_COMMAND + 0x02, prom[2]);
    OUTB(ne->iobase + REG_COMMAND + 0x03, prom[4]);
    OUTB(ne->iobase + REG_COMMAND + 0x04, prom[6]);
    OUTB(ne->iobase + REG_COMMAND + 0x05, prom[8]);
    OUTB(ne->iobase + REG_COMMAND + 0x06, prom[10]);
    OUTB(ne->iobase + REG_COMMAND + 0x08, 0xFF);
    OUTB(ne->iobase + REG_COMMAND + 0x09, 0xFF);
    OUTB(ne->iobase + REG_COMMAND + 0x0A, 0xFF);
    OUTB(ne->iobase + REG_COMMAND + 0x0B, 0xFF);
    OUTB(ne->iobase + REG_COMMAND + 0x0C, 0xFF);
    OUTB(ne->iobase + REG_COMMAND + 0x0D, 0xFF);
    OUTB(ne->iobase + REG_COMMAND + 0x0E, 0xFF);
    OUTB(ne->iobase + REG_COMMAND + 0x0F, 0xFF);

    for(i = 0; i < 6; i++)
    {
        ne->nsaddr[i] = prom[i * 2];
	}

    OUTB(ne->iobase + REG_DATA_CONFIGURATION, 0x58);
    ne->next_packet = PSTART + 1;
    OUTB(ne->iobase + REG_P1_CURPAGE, ne->next_packet);
    OUTB(ne->iobase + REG_COMMAND, 0x22);
    OUTB(ne->iobase + REG_TRANSMIT_CONFIGURATION, 0x00);
    // 0x0E = accept broadcast, multicast, and runt packets (< 64 bytes)
    OUTB(ne->iobase + REG_RECEIVE_CONFIGURATION, 0x0E);

    printk("net: found a NE2000 (or similar) network adapter\n");
    printk("     MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           ne->nsaddr[0], ne->nsaddr[1], ne->nsaddr[2],
           ne->nsaddr[3], ne->nsaddr[4], ne->nsaddr[5]);
    printk("     IRQ: 0x%02x, IOBase: 0x%02x\n",
           ne->dev->irq[0], ne->iobase);

	ne->netif.unit = pci->unit;
	ne->netif.flags = IFF_UP | IFF_RUNNING | IFF_BROADCAST;
	ne->netif.transmit = ne2000_transmit;
	ne->netif.mtu = 1500;

	memcpy(&(ne->netif.hwaddr), &(ne->nsaddr), 6);

	ethernet_attach(&ne->netif);

    return 0;
}


int ne2000_intr(struct regs *r, int unit)
{
    UNUSED(r);

    //printk("ne2000_intr[%d]:\n", this_core->cpuid);

    if(unit != 0)
    {
        printk("ne2000_intr[%d]: unit %d\n", this_core->cpuid, unit);
        return 0;
    }

    register struct ne2000_t *ne = &ne2000_dev[unit];
    uint8_t i;

    if(!ne->iobase)
    {
        printk("ne2000_intr[%d]: base 0x%lx\n", this_core->cpuid, ne->iobase);
        return 0;
    }

    i = INB(ne->iobase + REG_INTERRUPT_STATUS);

    if(i == 0)
    {
        // IRQ did not come from this device
        printk("ne2000_intr[%d]: i 0x%x\n", this_core->cpuid, i);
        return 0;
    }

    OUTB(ne->iobase + REG_INTERRUPTMASK, 0x00);
    ne2000_do_intr(ne);
    OUTB(ne->iobase + REG_INTERRUPTMASK, 0x1F);

    // acknowledge the interrupt
    //OUTB(ne->iobase + REG_INTERRUPT_STATUS, i);

    unblock_task_no_preempt(ne->task);
    pic_send_eoi(ne->dev->irq[0]);

    return 1;
}


void ne2000_do_intr(struct ne2000_t *ne)
{
    volatile uint8_t i, j;

    // loop until no more interrupts
    while(1)
    {
        i = INB(ne->iobase + REG_INTERRUPT_STATUS);

        // packet received
        if(i & IR_RX)
        {

receive:

            if(INB(ne->iobase + REG_INTERRUPT_STATUS) & IR_ROVRN)
            {
                volatile int counter;
                volatile uint8_t val;

                // Receiver ovverun
                OUTB(ne->iobase + REG_COMMAND, CR_STOP | CR_NODMA);

                OUTB(ne->iobase + REG_REMOTE_BYTECOUNT0, 0);
                OUTB(ne->iobase + REG_REMOTE_BYTECOUNT1, 0);
                
    
                for(counter = 0x7FFF; counter > 0; counter--)
                {
                    val = INB(ne->iobase + REG_INTERRUPT_STATUS);
                    
                    if(val & IR_RESET)
                    {
                        break;
                    }
                }

                OUTB(ne->iobase + REG_TRANSMIT_CONFIGURATION, TR_LB0);
                OUTB(ne->iobase + REG_COMMAND, CR_START | CR_NODMA);

                ne2000_receive(ne);

                OUTB(ne->iobase + REG_INTERRUPT_STATUS, IR_ROVRN);
                OUTB(ne->iobase + REG_TRANSMIT_CONFIGURATION, 0);
                
                // check receiver ring
                i = INB(ne->iobase + REG_BOUNDARY);
                OUTB(ne->iobase + REG_COMMAND, CR_START | CR_NODMA | CR_PG1);
                j = INB(ne->iobase + REG_P1_CURPAGE);
                OUTB(ne->iobase + REG_COMMAND, CR_START | CR_NODMA | CR_PG0);
                
                if(i != j)
                {
                    goto receive;
                }
            }
            else
            {
                // reset PRX (transmit) bit in ISR
                OUTB(ne->iobase + REG_INTERRUPT_STATUS, IR_RX);
                ne2000_receive(ne);
            }
        }
        // packet transmitted
        else if(i & (IR_TX | IR_TXE) /* 0x0A */)
        {
            // reset PTX and TXE bits in ISR
            OUTB(ne->iobase + REG_INTERRUPT_STATUS, IR_TX | IR_TXE);
            i = INB(ne->iobase + REG_TRANSMIT_STATUS);
            
            // check if FU, CRS or ABT bits are set in TSR
            if(i & 0x38)
            {
                // bad transmission
                ne->netif.stats.tx_errors++;
            }
        }
        else
        {
            if(i & IR_RDC)
            {
                // reset the 'remote DMA complete' bit in ISR
                OUTB(ne->iobase + REG_INTERRUPT_STATUS, IR_RDC);
            }

            break;
        }
    }
}


void read_mem(struct ne2000_t *ne, uint16_t src, void *dst, size_t orig_len)
{
    size_t i;
    uint8_t *p = (uint8_t *)dst;
    size_t len = orig_len;
    uint16_t port = ne->iobase + REG_NE_DATA;
    
    // Word-align length
    if(len & 1)
    {
        len++;
    }

    // Abort any remote DMA already in progress
    OUTB(ne->iobase + REG_COMMAND, CR_START | CR_NODMA);

    // Setup DMA byte count
    OUTB(ne->iobase + REG_REMOTE_BYTECOUNT0, (uint8_t) (len & 0xFF));
    OUTB(ne->iobase + REG_REMOTE_BYTECOUNT1, (uint8_t) (len >> 8));

    // Setup NIC memory source address
    OUTB(ne->iobase + REG_REMOTE_STARTADDRESS0, (uint8_t) (src & 0xFF));
    OUTB(ne->iobase + REG_REMOTE_STARTADDRESS1, (uint8_t) (src >> 8));

    // Select remote DMA read
    OUTB(ne->iobase + REG_COMMAND, CR_START | CR_RREAD);

    // Read NIC memory
    for(i = 0; i < orig_len; i++)
    {
        // *p++ = INB(ne->iobase + REG_NE_DATA);
        *p++ = INB(port);
    }

    if(orig_len & 1)
    {
        //INB(ne->iobase + REG_NE_DATA);
        INB(port);
    }
}


void ne2000_receive(struct ne2000_t *ne)
{
    struct receive_ring_desc_t packet_hdr;
    size_t len;
    struct packet_t *p;
    uint8_t boundary, cur_page;
    int i;

    // set page 1 regs and get the current page, then back to page 0
    OUTB(ne->iobase + REG_COMMAND, CR_START | CR_NODMA | CR_PG1);
    cur_page = INB(ne->iobase + REG_P1_CURPAGE);
    OUTB(ne->iobase + REG_COMMAND, CR_START | CR_NODMA | CR_PG0);

    while(cur_page != ne->next_packet)
    {
        read_mem(ne, (ne->next_packet << 8), &packet_hdr,
                 sizeof(struct receive_ring_desc_t));
        len = packet_hdr.count - sizeof(struct receive_ring_desc_t);
        OUTB(ne->iobase + REG_INTERRUPT_STATUS, IR_RDC);
        
        // read packet and send to upper layer
        if((p = ne2000_alloc_packet(len)))
        {
            read_mem(ne, ((ne->next_packet << 8) |
                          sizeof(struct receive_ring_desc_t)),
                     p->data, len);

            ne->netif.stats.rx_packets++;
            ne->netif.stats.rx_bytes += len;
            //netstats->link.recv++;

            i = ((uintptr_t)p - 
                    (uintptr_t)ne2000_in_buffers) / NE2000_IN_BUFFER_SIZE;
            ne2000_in_packet_bitmap |= (1 << i);

            KDEBUG("ne2000_receive: bitmap %x\n", ne2000_in_buffer_use_bitmap);

            //ethernet_receive(&ne->netif, p);
        }
        else
        {
            // insufficient memory - drop packet
            printk("%s: packet dropped\n", ne->netif.name);
            ne->netif.stats.rx_over_errors++;
            ne->netif.stats.rx_dropped++;
            //netstats->link.memerr++;
            //netstats->link.drop++;
        }

        OUTB(ne->iobase + REG_INTERRUPT_STATUS, IR_RDC);
        
        if(packet_hdr.next_packet == PSTOP)
        {
            ne->next_packet = PSTART;
        }
        else
        {
            ne->next_packet = packet_hdr.next_packet;
        }

        if(ne->next_packet == PSTART)
        {
            boundary = PSTOP - 1;
        }
        else
        {
            boundary = ne->next_packet - 1;
        }

        OUTB(ne->iobase + REG_BOUNDARY, boundary);

        // set page 1 regs and get the current page, then back to page 0
        OUTB(ne->iobase + REG_COMMAND, CR_START | CR_NODMA | CR_PG1);
        cur_page = INB(ne->iobase + REG_P1_CURPAGE);
        OUTB(ne->iobase + REG_COMMAND, CR_START | CR_NODMA | CR_PG0);
    }
}


int ne2000_transmit(struct netif_t *ifp, struct packet_t *p)
{
    register struct ne2000_t *ne;
	int unit;
	volatile uint8_t i;
    volatile int old_flags;
	size_t j, len;
    uint8_t *dest = (uint8_t *)p->data;

    if(!ifp || (unit = ifp->unit) != 0)
    {
        return -EINVAL;
    }

    ne = &ne2000_dev[unit];
    old_flags = __set_cpu_flag(SMP_FLAG_SCHEDULER_BUSY);

    while(!__sync_bool_compare_and_swap(&ifp->sending, 0, 1))
    {
        ;
    }

    cli();

    i = INB(ne->iobase + REG_COMMAND);

    if(i == (CR_NODMA | CR_TRANS | CR_START))
    {
        __sync_bool_compare_and_swap(&ifp->sending, 1, 0);

        if(!(old_flags & SMP_FLAG_SCHEDULER_BUSY))
        {
            __clear_cpu_flag(SMP_FLAG_SCHEDULER_BUSY);
        }

        sti();
        return -EAGAIN;
    }

    // get a word-aligned count
    len = p->count;
    
    if(len & 1)
    {
        len++;
    }

    // Setup DMA byte count
    OUTB(ne->iobase + REG_REMOTE_BYTECOUNT0, (uint8_t) (len & 0xFF));
    OUTB(ne->iobase + REG_REMOTE_BYTECOUNT1, (uint8_t) (len >> 8));

    // Setup NIC memory destination address
    OUTB(ne->iobase + REG_REMOTE_STARTADDRESS0, (uint8_t)0);
    OUTB(ne->iobase + REG_REMOTE_STARTADDRESS1, (uint8_t)TRANSMITBUFFER);

    // set remote DMA write
    OUTB(ne->iobase + REG_COMMAND, CR_RWRITE | CR_START);

    // transfer data
    for(j = 0; j < len; j++)
    {
        OUTB(ne->iobase + REG_NE_DATA, dest[j]);
    }

    // set transmit buf start page
    OUTB(ne->iobase + REG_TRANSMIT_PAGE, TRANSMITBUFFER);
    OUTB(ne->iobase + REG_TRANSMIT_BYTECOUNT0, (uint8_t) (len & 0xFF));
    OUTB(ne->iobase + REG_TRANSMIT_BYTECOUNT1, (uint8_t) (len >> 8));

    // transmit and wait
    OUTB(ne->iobase + REG_COMMAND, CR_NODMA | CR_TRANS | CR_START);

    __sync_bool_compare_and_swap(&ifp->sending, 1, 0);

    if(!(old_flags & SMP_FLAG_SCHEDULER_BUSY))
    {
        __clear_cpu_flag(SMP_FLAG_SCHEDULER_BUSY);
    }

    sti();

    ne->netif.stats.tx_packets++;
    ne->netif.stats.tx_bytes += len;
    
    free_packet(p);

    return 0;
}


int ne2000_process_input(struct netif_t *ifp)
{
    volatile int i;

    //printk("ne2000_process_input[%d]:\n", this_core->cpuid);

    while(ne2000_in_packet_bitmap)
    {
        for(i = 0; i < NE2000_IN_BUFFER_COUNT; i++)
        {
            if((ne2000_in_packet_bitmap & (1 << i)))
            {
                struct packet_t *p = (struct packet_t *)
                                         (ne2000_in_buffers + 
                                            (i * NE2000_IN_BUFFER_SIZE));
                struct packet_t *p2;

                if(!(p2 = dup_packet(p)))
                {
                    return 1;
                }

                ne2000_in_packet_bitmap &= ~(1 << i);
                ne2000_in_buffer_use_bitmap &= ~(1 << i);
                p2->ifp = ifp;
                ethernet_receive(ifp, p2);
            }
        }
    }

    return 0;
}


static void ne2000_func(void *arg)
{
    int unit = (int)(intptr_t)arg;
    struct ne2000_t *ne = &ne2000_dev[unit];

    while(1)
    {
        //printk("ne2000_func[%d]:\n", this_core->cpuid);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);

	    if(ne->netif.flags & IFF_UP)
	    {
            ne2000_process_input(&ne->netif);
        }

        block_task(ne, 1);
    }
}

