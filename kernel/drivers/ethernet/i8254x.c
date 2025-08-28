/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: i8254x.c
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
 *  \file i8254x.c
 *
 *  Intel 8254x series device driver implementation.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/pciio.h>
#include <kernel/pic.h>
#include <kernel/asm.h>
#include <kernel/net.h>
#include <kernel/net/i8254x.h>
#include <kernel/net/ether.h>
#include <kernel/net/packet.h>
#include <mm/kheap.h>

#define I8254x_DEVS                     4

// how many outgoing packets we can keep in queue
#define MAX_OUT_PACKETS                 128

#define i8254x_IN_BUFFER_COUNT          (PAGE_SIZE / sizeof(struct i8254x_rx_desc_t))
#define i8254x_IN_BUFFER_SIZE           2048
#define i8254x_IN_BUFFER_TOTALMEM       (i8254x_IN_BUFFER_SIZE * i8254x_IN_BUFFER_COUNT)

#define i8254x_OUT_BUFFER_COUNT         i8254x_IN_BUFFER_COUNT
#define i8254x_OUT_BUFFER_SIZE          2048
#define i8254x_OUT_BUFFER_TOTALMEM      (i8254x_OUT_BUFFER_SIZE * i8254x_OUT_BUFFER_COUNT)

struct i8254x_t i8254x_dev[I8254x_DEVS];

static void i8254x_func(void *arg);


static inline void device_wait(int msecs)
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


static uint16_t i8254x_eeprom_read(struct i8254x_t *dev, uint8_t i)
{
    volatile uint32_t tmp;

    pcidev_outl(dev, i8254x_REG_EERD, 1 | ((uint32_t)i << 8));
    tmp = pcidev_inl(dev, i8254x_REG_EERD);

    while(!(tmp & (1 << 4)))
    {
        device_wait(1);
        tmp = pcidev_inl(dev, i8254x_REG_EERD);
    }

	return (uint16_t)((tmp >> 16) & 0xFFFF);
}


int i8254x_init(struct pci_dev_t *pci)
{
    static int unit = 0;
    
    if(!pci)
    {
        return -EINVAL;
    }
    
    register struct i8254x_t *dev = &i8254x_dev[unit];
    uintptr_t bar0, phys, virt, phys2, virt2;
    //virtual_addr vinbuf, voutbuf;
    //physical_addr pinbuf, poutbuf;
    uint16_t word;
    uint32_t dword;
    int res = 0;

    init_kernel_mutex(&dev->outq.lock);
    dev->outq.max = MAX_OUT_PACKETS;

	/*
	 * Allocate internal buffers. Each buffer is 2048 bytes (half a page).
	 * We allocate a total of i8254x_IN_BUFFER_COUNT (256) buffers.
	 *
	 * We do this early on because once we enabled our IRQ, we might start
	 * receiving them before we have our buffers set.
	 *
	 * NOTE: is 2048 actually enough per packet?
	 */

    // we need to store these as we work with virtual addresses but the device
    // descriptors require physical ones
    if(!(dev->inbuf_virt = kmalloc(i8254x_IN_BUFFER_COUNT * sizeof(virtual_addr))))
    {
        printk("net: failed to alloc i8254x internal buffers\n");
        return -ENOMEM;
    }

    if(!(dev->outbuf_virt = kmalloc(i8254x_OUT_BUFFER_COUNT * sizeof(virtual_addr))))
    {
        printk("net: failed to alloc i8254x internal buffers\n");
        kfree(dev->inbuf_virt);
        dev->inbuf_virt = NULL;
        return -ENOMEM;
    }

    pci->unit = unit++;
    dev->dev = pci;
    init_kernel_mutex(&dev->lock);

    //printk("bar0 0x%x\n", pci->bar[0]);

#define BAR0_OFFSET             0x10

    // determine the size of the BAR
    bar0 = pci->bar[0];
    pci_config_write_long(pci->bus, pci->dev, pci->function, 
                                    BAR0_OFFSET, 0xffffffff);
    dev->iosize = pci_config_read_long(pci->bus, pci->dev, pci->function, 
                                    BAR0_OFFSET);
    dev->iosize &= ~0xf;    // mask the lower 4 bits
    dev->iosize = (~(dev->iosize) & 0xffffffff) + 1;     // invert and add 1
    pci_config_write_long(pci->bus, pci->dev, pci->function, 
                                    BAR0_OFFSET, bar0);
    bar0 = pci_config_read_long(pci->bus, pci->dev, pci->function, 
                                    BAR0_OFFSET);
    
    printk("net: BAR0 " _XPTR_ ", iosize " _XPTR_ "\n", bar0, dev->iosize);

#undef BAR0_OFFSET

    // check whether I/O is memory-mapped or normal I/O
    if((pci->bar[0] & 0x1))
    {
        // I/O
        dev->iobase = pci->bar[0] & ~0x3;
    }
    else
    {
        // MMIO
        bar0 = pci->bar[0] & ~0xf;
        dev->iobase = mmio_map(bar0, bar0 + dev->iosize);
        dev->mmio = 1;
    }

    pci_enable_busmastering(pci);
    pci_enable_interrupts(pci);
    pci_enable_memoryspace(pci);

    // read the MAC address
    word = i8254x_eeprom_read(dev, 0);
    dev->nsaddr[0] = (word & 0xff);
    dev->nsaddr[1] = ((word >> 8) & 0xff);
    word = i8254x_eeprom_read(dev, 1);
    dev->nsaddr[2] = (word & 0xff);
    dev->nsaddr[3] = ((word >> 8) & 0xff);
    word = i8254x_eeprom_read(dev, 2);
    dev->nsaddr[4] = (word & 0xff);
    dev->nsaddr[5] = ((word >> 8) & 0xff);

    // reset
    dword = pcidev_inl(dev, i8254x_REG_CTRL);
    pcidev_outl(dev, i8254x_REG_CTRL, dword | CTRL_RST);
    device_wait(1);

    // set up link
    dword = pcidev_inl(dev, i8254x_REG_CTRL);
    pcidev_outl(dev, i8254x_REG_CTRL, dword | CTRL_ASDE | CTRL_SLU);
    dword = pcidev_inl(dev, i8254x_REG_CTRL);
    pcidev_outl(dev, i8254x_REG_CTRL, dword & ~CTRL_LRST);
    dword = pcidev_inl(dev, i8254x_REG_CTRL);
    pcidev_outl(dev, i8254x_REG_CTRL, dword & ~CTRL_PHYS_RST);
    dword = pcidev_inl(dev, i8254x_REG_CTRL);
    pcidev_outl(dev, i8254x_REG_CTRL, dword & ~CTRL_ILOS);

    // init statistics registers
    for(dword = 0; dword < 64; dword++)
    {
        pcidev_outl(dev, i8254x_REG_CRCERRS + (dword * 4), 0);
    }

    // reset the Multicase Table Array
    for(dword = 0; dword < 128; dword++)
    {
        pcidev_outl(dev, i8254x_REG_MTA + (dword * 4), 0);
    }

#define PAGE_FLAGS      (PTE_FLAGS_PW | I86_PTE_NOT_CACHEABLE)

    /*
     * Alloc memory and set up the receive descriptors.
     * Each descriptor is 16 bytes, so a page gives us 256 descriptors.
     */
    if(get_next_addr(&phys, &virt, PAGE_FLAGS, REGION_DMA) != 0)
    {
        res = -ENOMEM;
        goto err;
    }

    dev->rx_desc = (volatile struct i8254x_rx_desc_t *)virt;

    for(dword = 0; dword < i8254x_IN_BUFFER_COUNT; dword += 2)
    {
        // a page will hold two buffers as bufsize is 2K
        if(get_next_addr(&phys2, &virt2, PTE_FLAGS_PW, REGION_DMA) != 0)
        {
            res = -ENOMEM;
            goto err;
        }

        dev->inbuf_virt[dword] = virt2;
        dev->rx_desc[dword].address = (volatile uint64_t)phys2;
        dev->rx_desc[dword].status = 0;

        dev->inbuf_virt[dword + 1] = virt2 + i8254x_IN_BUFFER_SIZE;
        dev->rx_desc[dword + 1].address = (volatile uint64_t)phys2 + i8254x_IN_BUFFER_SIZE;
        dev->rx_desc[dword + 1].status = 0;
    }

    // setup the receive descriptor ring buffer
    pcidev_outl(dev, i8254x_REG_RDBAH, (uint32_t)(phys >> 32));
    pcidev_outl(dev, i8254x_REG_RDBAL, (uint32_t)(phys & 0xFFFFFFFF));
    dword = pcidev_inl(dev, i8254x_REG_RDBAH);
	printk("net: RX ring desc %x", dword);
    dword = pcidev_inl(dev, i8254x_REG_RDBAL);
	printk(":%x\n", dword);

    // set up the receive buffer length
    pcidev_outl(dev, i8254x_REG_RDLEN, (uint32_t)(i8254x_IN_BUFFER_COUNT * 16));

    // set up head and tail pointers
    pcidev_outl(dev, i8254x_REG_RDH, 0);
    pcidev_outl(dev, i8254x_REG_RDT, i8254x_IN_BUFFER_COUNT - 1);
    //dev->rx_tail = 0;

    // no delay for receiving IRQs
    pcidev_outl(dev, i8254x_REG_RDTR, 0);

    // set packet size (2K)
    dword = pcidev_inl(dev, i8254x_REG_RCTL);
    dword &= ~RCTL_BSEX;
    dword &= ~((1 << 17) | (1 << 16));
    pcidev_outl(dev, i8254x_REG_RCTL, dword);

    // no loopback
    //dword = pcidev_inl(dev, i8254x_REG_RCTL);
    //pcidev_outl(dev, i8254x_REG_RCTL, dword & ~RCTL_LBM);

    // enable reception, store bad packets, promiscous, broadcast accept mode
    pcidev_outl(dev, i8254x_REG_RCTL, RCTL_SBP | RCTL_EN | RCTL_UPE | 
                                      RCTL_MPE | RCTL_BAM | RCTL_LPE | RCTL_SECRC);

    /*
     * Alloc memory and set up the transmission descriptors.
     * Each descriptor is 16 bytes, so a page gives us 256 descriptors.
     */
    if(get_next_addr(&phys, &virt, PAGE_FLAGS, REGION_DMA) != 0)
    {
        res = -ENOMEM;
        goto err;
    }

    dev->tx_desc = (volatile struct i8254x_tx_desc_t *)virt;

    for(dword = 0; dword < i8254x_OUT_BUFFER_COUNT; dword += 2)
    {
        // a page will hold two buffers as bufsize is 2K
        if(get_next_addr(&phys2, &virt2, PTE_FLAGS_PW, REGION_DMA) != 0)
        {
            res = -ENOMEM;
            goto err;
        }

        dev->outbuf_virt[dword] = virt2;
        dev->tx_desc[dword].address = (volatile uint64_t)phys2;
        dev->tx_desc[dword].cmd = 0;

        dev->outbuf_virt[dword + 1] = virt2 + i8254x_OUT_BUFFER_SIZE;
        dev->tx_desc[dword + 1].address = (volatile uint64_t)phys2 + i8254x_OUT_BUFFER_SIZE;
        dev->tx_desc[dword + 1].cmd = 0;
    }

    // setup the receive descriptor ring buffer
    pcidev_outl(dev, i8254x_REG_TDBAH, (uint32_t)(phys >> 32));
    pcidev_outl(dev, i8254x_REG_TDBAL, (uint32_t)(phys & 0xFFFFFFFF));
    dword = pcidev_inl(dev, i8254x_REG_TDBAH);
	printk("net: TX ring desc %x", dword);
    dword = pcidev_inl(dev, i8254x_REG_TDBAL);
	printk(":%x\n", dword);

    // set up the transmission buffer length
    pcidev_outl(dev, i8254x_REG_TDLEN, (uint32_t)(i8254x_OUT_BUFFER_COUNT * 16));

    // set up head and tail pointers
    pcidev_outl(dev, i8254x_REG_TDH, 0);
    pcidev_outl(dev, i8254x_REG_TDT, 0 /* i8254x_OUT_BUFFER_COUNT */);
    //dev->tx_tail = 0;

    // set the transmit control register
    pcidev_outl(dev, i8254x_REG_TCTL, (TCTL_EN | TCTL_PSP));

#undef PAGE_FLAGS

	dev->netif.unit = pci->unit;
	dev->netif.flags = IFF_UP | IFF_RUNNING | IFF_BROADCAST;
	dev->netif.transmit = i8254x_transmit;
	dev->netif.mtu = 1500;

	memcpy(&(dev->netif.hwaddr), &(dev->nsaddr), 6);

    (void)start_kernel_task("i8254x", i8254x_func, (void *)(uintptr_t)pci->unit, &dev->task, 0);

    // enable IRQs and reset any pending IRQs
    pci_register_irq_handler(pci, i8254x_intr, "i8254x");
    pcidev_outl(dev, i8254x_REG_IMS, IMS_LSC | IMS_RXO | IMS_RXT | IMS_TXQE | IMS_TXDW);
    pcidev_inl(dev, i8254x_REG_INTR);

    // read link status
    dword = pcidev_inl(dev, i8254x_REG_STATUS);

    // print out the result
    printk("net: found an i8254x series (or similar) network adapter\n");
    printk("     MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dev->nsaddr[0], dev->nsaddr[1], dev->nsaddr[2],
           dev->nsaddr[3], dev->nsaddr[4], dev->nsaddr[5]);
    printk("     IRQ: 0x%02x, IOBase: " _XPTR_ "\n",
           dev->dev->irq[0], dev->iobase);
    printk("net: link status 0x%x\n", dword);

	ethernet_attach(&dev->netif);

    return 0;

err:

    if(dev->mmio)
    {
        vmmngr_free_pages(dev->iobase, dev->iobase + dev->iosize);
        dev->iobase = 0;
    }

    if(dev->tx_desc)
    {
        vmmngr_free_page(get_page_entry((void *)dev->tx_desc));
        dev->tx_desc = 0;
    }

    if(dev->rx_desc)
    {
        vmmngr_free_page(get_page_entry((void *)dev->rx_desc));
        dev->rx_desc = 0;
    }

    return res;
}


int i8254x_intr(struct regs *r, int unit)
{
    UNUSED(r);

    register struct i8254x_t *dev = &i8254x_dev[unit];
    uint32_t i, j;

    if(!dev->iobase)
    {
        return 0;
    }

    i = pcidev_inl(dev, i8254x_REG_INTR);

    if(i == 0)
    {
        // IRQ did not come from this device
        return 0;
    }

    //printk("i8254x_intr: i %x\n", i);

    // TX descriptor complete and TX queue empty
    i &= ~3;

    // link status change
    if(i & (1 << 2))
    {
        i &= ~(1 << 2);
        j = pcidev_inl(dev, i8254x_REG_CTRL);
        pcidev_outl(dev, i8254x_REG_CTRL, j | CTRL_SLU);
    }

    // RX underrun / min threshold
    if(i & (1 << 6) || i & (1 << 4))
    {
        i &= ~((1 << 6) | (1 << 4));
    }

    // pending packet
    if(i & (1 << 7))
    {
        i &= ~(1 << 7);
    }

    // TX underrun / min threshold
    if(i & (1 << 15))
    {
        i &= ~(1 << 15);
    }

    if(i)
    {
        printk("net: i8254x: unhandled IRQ 0x%x\n", i);
    }

    // clear any pending IRQs
    pcidev_inl(dev, i8254x_REG_INTR);

    // acknowledge the interrupt
    pic_send_eoi(dev->dev->irq[0]);

    unblock_kernel_task(dev->task);

    return 1;
}


int i8254x_transmit(struct netif_t *ifp, struct packet_t *p)
{
    if(!ifp)
    {
        return -EINVAL;
    }

    cli();

    register struct i8254x_t *dev = &i8254x_dev[ifp->unit];
    volatile uint16_t tx_head = pcidev_inl(dev, i8254x_REG_TDH);
    volatile uint16_t tx_tail = pcidev_inl(dev, i8254x_REG_TDT);
    volatile uint16_t tx_next = (tx_tail + 1) % i8254x_OUT_BUFFER_COUNT;
    volatile uintptr_t data = dev->outbuf_virt[tx_tail];

    //printk("i8254x_transmit: tx_head %d, tx_tail %d, tx_next %d\n", tx_head, tx_tail, tx_next);

    if(tx_head == tx_next)
    {
        // transmit buffer is full
        sti();
        return -ENOMEM;
    }

    //dev->tx_desc[tx_tail].address = (uint64_t)p->data;
    A_memcpy((void *)data, p->data, p->count);
    dev->tx_desc[tx_tail].length = p->count;
    dev->tx_desc[tx_tail].cso = 0;
    dev->tx_desc[tx_tail].sta = 0;
    dev->tx_desc[tx_tail].css = 0;
    dev->tx_desc[tx_tail].special = 0;
    // cmd: EOP, FCS, RS
    dev->tx_desc[tx_tail].cmd = (1 | (1 << 1) | (1 << 3));

    //printk("i8254x_transmit: data p %lx, v %lx\n", dev->tx_desc[tx_tail].address, data);

    // update the tail so the hardware knows it's ready
    pcidev_outl(dev, i8254x_REG_TDT, tx_next);

    sti();

    /*
    while(!(dev->tx_desc[tx_tail].sta & 0x0f))
    {
        device_wait(1);
    }

    printk("i8254x_transmit: sent\n");
    */

    dev->netif.stats.tx_packets++;
    dev->netif.stats.tx_bytes += p->count;

    free_packet(p);

    return 0;
}


static inline void err_drop(struct i8254x_t *dev, 
                            volatile uint16_t rx_cur,
                            volatile uint16_t rx_tail, uint16_t datalen)
{
    printk("%s: packet dropped\n", dev->netif.name);
    dev->netif.stats.rx_over_errors++;
    dev->netif.stats.rx_dropped++;

    // update RX counts and the tail pointer
    dev->netif.stats.rx_packets++;
    dev->netif.stats.rx_bytes += datalen;

    dev->rx_desc[rx_cur].status = (uint16_t)(0);
    rx_tail = (rx_tail + 1) % i8254x_IN_BUFFER_COUNT;

    // write the tail to the device
    pcidev_outl(dev, i8254x_REG_RDT, rx_tail);
}


int i8254x_process_input(struct netif_t *ifp)
{
    struct packet_t *p2;
    struct i8254x_t *dev = &i8254x_dev[ifp->unit];
    volatile uint32_t rx_head = pcidev_inl(dev, i8254x_REG_RDH);
    volatile uint32_t rx_tail = pcidev_inl(dev, i8254x_REG_RDT);
    volatile uint32_t diff = (rx_head < rx_tail) ? 
                            (i8254x_IN_BUFFER_COUNT - rx_tail + rx_head - 1) :
                            (rx_head - rx_tail - 1);
    volatile uint32_t rx_cur;
    volatile uintptr_t data;
    uint16_t datalen;
    int drop;

    while(diff != 0)
    {
        rx_cur = (rx_tail + 1) % i8254x_IN_BUFFER_COUNT;
        data = dev->inbuf_virt[rx_cur];
        datalen = dev->rx_desc[rx_cur].length;
        drop = 0;

        // this should not happen
        if(!(dev->rx_desc[rx_cur].status & (1 << 0)))
        {
            printk("%s: RX packet with status done\n", dev->netif.name);
            drop = 1;
        }

        // do not handle no-EOP
        if(!(dev->rx_desc[rx_cur].status & (1 << 1)))
        {
            printk("%s: RX packet with no EOP\n", dev->netif.name);
            drop = 1;
        }

        if(dev->rx_desc[rx_cur].errors)
        {
            printk("%s: RX error (0x%x)\n", dev->netif.name, dev->rx_desc[rx_cur].errors);
            drop = 1;
        }

        if(drop)
        {
            err_drop(dev, rx_cur, rx_tail, datalen);
            goto cont;
        }

        if(!(p2 = alloc_packet(datalen)))
        {
            printk("%s: insufficient memory for new packet\n", dev->netif.name);
            err_drop(dev, rx_cur, rx_tail, datalen);
            goto cont;
        }

        p2->ifp = ifp;
        A_memcpy(p2->data, (void *)data, datalen);
        ethernet_receive(ifp, p2);

        // update RX counts and the tail pointer
        dev->netif.stats.rx_packets++;
        dev->netif.stats.rx_bytes += datalen;

        dev->rx_desc[rx_cur].status = (uint16_t)(0);
        rx_tail = (rx_tail + 1) % i8254x_IN_BUFFER_COUNT;

        // write the tail to the device
        pcidev_outl(dev, i8254x_REG_RDT, rx_tail);

cont:

        // update the pointers
        rx_head = pcidev_inl(dev, i8254x_REG_RDH);
        rx_tail = pcidev_inl(dev, i8254x_REG_RDT);
        diff = (rx_head < rx_tail) ? 
                        (i8254x_IN_BUFFER_COUNT - rx_tail + rx_head - 1) :
                        (rx_head - rx_tail - 1);
    }

    return 0;
}


static void i8254x_func(void *arg)
{
    int unit = (int)(intptr_t)arg;
    struct i8254x_t *dev = &i8254x_dev[unit];

    while(1)
    {
	    if(dev->netif.flags & IFF_UP)
	    {
            i8254x_process_input(&dev->netif);
        }

        block_task2(dev, PIT_FREQUENCY);
    }
}

