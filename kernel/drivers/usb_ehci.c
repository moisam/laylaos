/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb_ehci.c
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
 *  \file usb_ehci.c
 *
 *  The Universal Serial Bus (USB) driver code is split into several files:
 *    - usb.c       => main entry point and general functions
 *    - usb_msd.c   => functions to handle Mass Storage Devices (MSD)
 *    - usb_hid.c   => functions to handle Human Interaction Devices (HID)
 *    - usb_hub.c   => functions to handle USB hubs
 *    - usb_ioctl.c => functions to handle ioctl() calls
 *    - usb_ohci.c  => OHCI layer
 *    - usb_uhci.c  => UHCI layer
 *    - usb_ehci.c  => EHCI layer
 */

//#define __DEBUG

#include <errno.h>
#include <kernel/pci.h>
#include <kernel/pciio.h>
#include <kernel/asm.h>
#include <kernel/usb.h>
#include <kernel/usb_ehci.h>
#include <kernel/pic.h>
#include <mm/kheap.h>


static volatile struct ehci_dev_t *first_ehci = NULL;
static struct usb_ops_t ehci_ops;

//int ehci_intr(struct regs *r, void *arg);
static int ehci_setup_device(volatile struct ehci_dev_t *ehci, unsigned int port, uint8_t speed);


struct usb_dev_t *ehci_get_dev_struct(struct pci_dev_t *bus, uint8_t num)
{
    volatile struct ehci_dev_t *ehci = first_ehci;

    if(!bus || num < 1)
    {
        return NULL;
    }

    while(ehci)
    {
        if(ehci->pci == bus)
        {
            if(num > ehci->port_count)
            {
                return NULL;
            }

            return (struct usb_dev_t *)(ehci->ports[num - 1].usb);
        }

        ehci = ehci->next;
    }

    return NULL;
}


static unsigned int ehci_get_next_addr(void *__ehci)
{
    struct ehci_dev_t *ehci = __ehci;
    volatile unsigned int i, j;

    for(i = 0; i < (MAX_DEV_PER_HC / sizeof(uint32_t)); i++)
    {
        for(j = 0; j < 32; j++)
        {
            if(!(ehci->addr_bitmap[i] & (1 << j)))
            {
                ehci->addr_bitmap[i] |= (1 << j);
                return (i * 32) + j;
            }
        }
    }

    return 0;
}


static void ehci_free_addr(void *__ehci, unsigned int i)
{
    struct ehci_dev_t *ehci = __ehci;

    if(i < 1 || i >= MAX_DEV_PER_HC)
    {
        return;
    }

    ehci->addr_bitmap[i / 32] &= ~(1 << (i % 32));
}


static void init_qhpool(struct ehci_dev_t *ehci)
{
    volatile size_t i;
    volatile struct ehci_qh_t *qh = (struct ehci_qh_t *)ehci->qhpool;
    uintptr_t qhphys = (uintptr_t)ehci->qhpool_phys;

    A_memset((void *)ehci->qhpool, 0, PAGE_SIZE);
    A_memset(ehci->qh_used, 0, EHCI_MAX_QH);

    for(i = 0; i < EHCI_MAX_QH; i++)
    {
        qh[i].self_phys = qhphys;
        qhphys += sizeof(struct ehci_qh_t);
    }
}


static void init_tdpool(struct ehci_dev_t *ehci)
{
    A_memset(ehci->td_used, 0, EHCI_MAX_TD);
    A_memset((void *)ehci->tdpool, 0, PAGE_SIZE);
}


static void free_qh(struct ehci_dev_t *ehci, uintptr_t qh)
{
    volatile int i = (qh - ehci->qhpool) / sizeof(struct ehci_qh_t);

    __sync_bool_compare_and_swap(&ehci->qh_used[i], 1, 0);
}


static struct ehci_qh_t *alloc_qh(volatile struct ehci_dev_t *ehci)
{
    volatile size_t i;

    for(i = 0; i < EHCI_MAX_QH; i++)
    {
        if(__sync_bool_compare_and_swap(&ehci->qh_used[i], 0, 1))
        {
            return (struct ehci_qh_t *)(ehci->qhpool + (i * sizeof(struct ehci_qh_t)));
        }
    }

    printk("%s: failed to alloc QH\n", "ehci");

    return 0;
}


static void free_td(struct ehci_dev_t *ehci, uintptr_t td)
{
    volatile int i = (td - ehci->tdpool) / sizeof(struct ehci_td_t);

    __sync_bool_compare_and_swap(&ehci->td_used[i], 1, 0);
}


static struct ehci_td_t *alloc_td(struct ehci_dev_t *ehci, uintptr_t *tdphys)
{
    volatile size_t i;

    *tdphys = 0;

    for(i = 0; i < EHCI_MAX_TD; i++)
    {
        if(__sync_bool_compare_and_swap(&ehci->td_used[i], 0, 1))
        {
            *tdphys = ehci->tdpool_phys + (i * sizeof(struct ehci_td_t));
            return (struct ehci_td_t *)(ehci->tdpool + (i * sizeof(struct ehci_td_t)));
        }
    }

    printk("%s: failed to alloc TD\n", "uhci");

    return NULL;
}


static void free_tdbuf(struct ehci_dev_t *ehci, uintptr_t tdbuf)
{
    volatile int i = (tdbuf - ehci->tdbufpool) / EHCI_TDBUF_SIZE;

    __sync_bool_compare_and_swap(&ehci->tdbuf_used[i], 1, 0);
}


static uintptr_t alloc_tdbuf(struct ehci_dev_t *ehci, uintptr_t *tdbufphys)
{
    volatile int i;

    *tdbufphys = 0;

    for(i = 0; i < EHCI_MAX_TDBUF; i++)
    {
        if(__sync_bool_compare_and_swap(&ehci->tdbuf_used[i], 0, 1))
        {
            uintptr_t v = ehci->tdbufpool + (i * EHCI_TDBUF_SIZE);

            *tdbufphys = ehci->tdbufpool_phys + (i * EHCI_TDBUF_SIZE);
            A_memset((void *)v, 0, EHCI_TDBUF_SIZE);

            return v;
        }
    }

    printk("%s: failed to alloc TD buffer\n", "ehci");

    return 0;
}


STATIC_INLINE void enable_async_sched(volatile struct ehci_dev_t *ehci)
{
    volatile uint32_t dword;
    volatile int timeout;

    //printk("%s: enabling async scheduling\n", "ehci");

    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_ASYNCLISTADDR, ehci->async_qh->self_phys);

    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
    dword |= ECHI_USBCMD_ASYNCEN;
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

    timeout = 20;

    while(!(pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS) & ECHI_USBSTS_ASYNCSTS) && timeout--)
    {
        tick_delay(2);
    }

    if(timeout <= 0)
    {
        printk("%s: async enable timeout (status 0x%x)\n", 
                "ehci", pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS));
    }
}


STATIC_INLINE void disable_async_sched(volatile struct ehci_dev_t *ehci)
{
    volatile uint32_t dword;
    volatile int timeout;

    //printk("%s: disabling async scheduling\n", "ehci");

    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
    dword &= ~ECHI_USBCMD_ASYNCEN;
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

    timeout = 20;

    while((pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS) & ECHI_USBSTS_ASYNCSTS) && timeout--)
    {
        tick_delay(2);
    }

    if(timeout <= 0)
    {
        printk("%s: async disable timeout (status 0x%x)\n", 
                "ehci", pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS));
    }
}


STATIC_INLINE void enable_periodic_sched(volatile struct ehci_dev_t *ehci)
{
    volatile uint32_t dword;
    volatile int timeout;

    printk("%s: enabling periodic scheduling\n", "ehci");

    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
    dword |= ECHI_USBCMD_PERIODICEN;
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

    timeout = 20;

    while(!(pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS) & ECHI_USBSTS_PERIODSTS) && timeout--)
    {
        tick_delay(2);
    }

    if(timeout <= 0)
    {
        printk("%s: periodic enable timeout (status 0x%x)\n", 
                "ehci", pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS));
    }
}


static void print_port_status(uint32_t status)
{
    int first = 1;

    printk("%s: status 0x%x (", "ehci", status);

    if(status & (1 << 0))
    {
        printk(" connected");
        first = 0;
    }

    if(status & (1 << 1))
    {
        printk("%sconn change", first ? " " : ", ");
        first = 0;
    }

    if(status & (1 << 2))
    {
        printk("%senabled", first ? " " : ", ");
        first = 0;
    }

    if(status & (1 << 3))
    {
        printk("%senabled change", first ? " " : ", ");
        first = 0;
    }

    if(status & (1 << 4))
    {
        printk("%sovercur", first ? " " : ", ");
        first = 0;
    }

    if(status & (1 << 5))
    {
        printk("%sovercur change", first ? " " : ", ");
        first = 0;
    }

    if(status & (1 << 6))
    {
        printk("%sresumed", first ? " " : ", ");
        first = 0;
    }

    if(status & (1 << 7))
    {
        printk("%ssuspended", first ? " " : ", ");
        first = 0;
    }

    if(status & (1 << 8))
    {
        printk("%sreset", first ? " " : ", ");
        first = 0;
    }

    printk(" )\n");
}


static char *states[] = { "SE0", "K-state", "J-state", "Undefined" };


static void ehci_reset_port(volatile struct ehci_dev_t *ehci, unsigned int port)
{
    uint32_t dword, state;
    volatile int timeout;
    int reg = EHCI_REG_HCOP + HCOP_PORTSC + (4 * port);

    dword = pcidev_inl(ehci, reg);
    dword |= EHCI_PORTSC_POWER;
    pcidev_outl(ehci, reg, dword);

    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);

    if(dword & ECHI_USBSTS_HCHALTED)
    {
        printk("ehci: port is halted during reset\n");
    }

    // start reset sequence
    dword = pcidev_inl(ehci, reg);
    dword &= ~EHCI_PORTSC_EN;
    dword |= EHCI_PORTSC_RESET;
    pcidev_outl(ehci, reg, dword);

    // wait
    tick_delay(50);

    // stop reset sequence
    dword = pcidev_inl(ehci, reg);
    dword &= ~EHCI_PORTSC_RESET;
    pcidev_outl(ehci, reg, dword);

    // wait for reset bit to clear
    timeout = 50;

    while((pcidev_inl(ehci, reg) & EHCI_PORTSC_RESET) && timeout--)
    {
        tick_delay(5);
    }

    if(timeout <= 0)
    {
        printk("%s: reset timeout for port %u\n", "ehci", port);
        //return;
    }

    tick_delay(10);
    dword = pcidev_inl(ehci, reg);
    state = (dword >> 10) & 0x03;
    printk("ehci: port %d after reset: status %x - state %d (%s)\n", port, dword, state, states[state]);

    if(/* (ehci->flags & EHCI_FLAG_PORTENABLED) && */ (dword & EHCI_PORTSC_POWER))
    {
        // only enable if we have a high speed device
        if(dword & EHCI_PORTSC_EN)
        {
            ehci_setup_device(ehci, port, USB_SPEED_HIGH);
        }
        else
        {
            // if a low speed device is attached, release ownership of this port
            dword = pcidev_inl(ehci, HCCAP_HCSPARAMS);

            if(dword & 0xF000)
            {
                dword = pcidev_inl(ehci, reg);
                dword |= EHCI_PORTSC_OWNER;
                pcidev_outl(ehci, reg, dword);
            }
        }
    }

    printk("%s: port %u reset complete\n", "ehci", port);
    print_port_status(pcidev_inl(ehci, reg));
}


static void ehci_check_port_state(volatile struct ehci_dev_t *ehci, unsigned int port)
{
    uint32_t dword, state;
    int reg = EHCI_REG_HCOP + HCOP_PORTSC + (4 * port);

    dword = pcidev_inl(ehci, reg);

    if(!(dword & EHCI_PORTSC_CONN))
    {
        printk("ehci: port %d: no device connected\n", port);
        return;
    }

    // wait 100ms until port power is stable
    tick_delay(20);

    // get line status from bits 10 & 11
    dword = pcidev_inl(ehci, reg);
    state = (dword >> 10) & 0x03;
    printk("ehci: port %d: status %x - state %d (%s)\n", port, dword, state, states[state]);

    if(state == 0 /* SE0 */ || state == 2 /* J-state */ || state == 3 /* undefined */)
    {
        ehci_reset_port(ehci, port);
    }
    else if(state == 1 /* K-state */)
    {
        // if a low speed device is attached, release ownership of this port
        dword = pcidev_inl(ehci, HCCAP_HCSPARAMS);

        if(dword & 0xF000)
        {
            dword = pcidev_inl(ehci, reg);
            dword |= EHCI_PORTSC_OWNER;
            pcidev_outl(ehci, reg, dword);
        }
    }
}


static int ehci_enable_ports(volatile struct ehci_dev_t *ehci)
{
    volatile unsigned int i;

    if(!(ehci->ports = kmalloc(sizeof(struct ehci_port_t) * ehci->port_count)))
    {
        printk("%s: insufficienct memory to enable ports\n", "ehci");
        return -ENOMEM;
    }

    A_memset((void *)ehci->ports, 0, sizeof(struct ehci_port_t) * ehci->port_count);

    for(i = 0; i < ehci->port_count; i++)
    {
        ehci_check_port_state(ehci, i);
        ehci->ports[i].port = i;
        ehci->ports[i].ehci = ehci;
    }

    ehci->flags |= EHCI_FLAG_PORTENABLED;

    return 0;
}


static void ehci_setup_transfer(struct usb_transfer_t *transfer)
{
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;

    if(transfer->type == USB_TRANSFER_ISOCHRONOUS)
    {
        kpanic("ehci: isochronous transfers not implemented yet!\n\n");
    }
    else
    {
        transfer->data = alloc_qh(ehci);
    }
}


static void link_async_qh(struct usb_transfer_t *transfer)
{
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;
    volatile struct ehci_qh_t *prev, *qh = transfer->data;

    kernel_mutex_lock_infinite_wait(&ehci->qh_lock);

    prev = ehci->async_qh;
    __atomic_store_n(&qh->next, prev->next, __ATOMIC_SEQ_CST);
    __atomic_store_n(&qh->nextvirt, prev->nextvirt, __ATOMIC_SEQ_CST);
    __atomic_store_n(&prev->next, qh->self_phys | (1 << 1), __ATOMIC_SEQ_CST);
    __atomic_store_n(&prev->nextvirt, qh, __ATOMIC_SEQ_CST);

    kernel_mutex_unlock(&ehci->qh_lock);
}


static void unlink_async_qh(struct usb_transfer_t *transfer)
{
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;
    volatile struct ehci_qh_t *prev, *qh = transfer->data;
    volatile uint32_t dword;
    volatile int timeout;

    if(!qh)
    {
        return;
    }

    kernel_mutex_lock_infinite_wait(&ehci->qh_lock);

    qh->next_qtd = 1;       // Terminate list
    qh->next_qtd_alt = 1;   // Terminate list
    prev = ehci->async_qh;

    while(prev)
    {
        if(prev->nextvirt == qh)
        {
            __atomic_store_n(&prev->next, qh->next, __ATOMIC_SEQ_CST);
            __atomic_store_n(&prev->nextvirt, qh->nextvirt, __ATOMIC_SEQ_CST);
            break;
        }

        prev = prev->nextvirt;
    }

    // We must ensure QH coherency. The host controller will probably have
    // cached copies of the QH in question. We need to let it know we have
    // changed the queue by setting the Interrupt on Async Advance Doorbell bit
    // int the USBCMD register. We then wait until the relevant bit in the
    // USBSTS register is set.
    // See EHCI specification section 4.8.2 Removing Queue Heads from Asynchronous Schedule

    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
    dword |= ECHI_USBCMD_ASYNCDOORBELL;
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

    timeout = 50;
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);

    while(!(dword & ECHI_USBSTS_ASYNCINT) && timeout--)
    {
        tick_delay(1);
        dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);
    }

    //printk("unlink_async_qh: status %x, qh %lx\n", dword, qh);
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBSTS, dword | ECHI_USBSTS_ASYNCINT);

    kernel_mutex_unlock(&ehci->qh_lock);

    if(timeout <= 0)
    {
        printk("%s: timeout waiting for async advance doorbell\n", "ehci");
    }

    //printk("unlink_async_qh: status %x\n", pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS));
}


static void ehci_delete_transfer(struct usb_transfer_t *transfer)
{
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;
    struct ehci_qh_t *qh = transfer->data;
    volatile int i;
    uint32_t dword;

    if(!qh)
    {
        return;
    }

    if(transfer->type == USB_TRANSFER_ISOCHRONOUS)
    {
        kpanic("ehci: isochronous transfers not implemented yet!\n\n");
    }
    else if(transfer->type == USB_TRANSFER_CTRL || transfer->type == USB_TRANSFER_BULK)
    {
        unlink_async_qh(transfer);
    }
    else if(transfer->type == USB_TRANSFER_INTERRUPT)
    {
        kernel_mutex_lock_infinite_wait(&ehci->qh_lock);

        dword = (uint32_t)qh->self_phys | (1 << 1);

        for(i = 0; i < 1024; i++)
        {
            if(ehci->framelist[i] == dword)
            {
                ehci->framelist[i] = 0x01;
                break;
            }
        }

        kernel_mutex_unlock(&ehci->qh_lock);

        if(i == 1024)
        {
            printk("ehci: transfer not found in framelist\n");
        }
    }

    free_qh(ehci, (uintptr_t)transfer->data);
    transfer->data = NULL;
}


static void __wait_transfer(struct usb_transfer_t *transfer, volatile int timeout)
{
    volatile struct usb_transaction_t *usbtrans = transfer->trans_head;

    while(timeout--)
    {
        struct ehci_transaction_t *ehcitrans = usbtrans->data;
        struct ehci_td_t *td = ehcitrans->tdvirt;

        //printk("__wait_transfer: to %d, st 0x%x, buf0 0x%lx\n", timeout, td->token.bits.status, td->buf0);

        // while not active
        while(!(td->token.bits.status & (1 << 7)))
        {
            if((usbtrans = usbtrans->next) == NULL)
            {
                break;
            }

            ehcitrans = usbtrans->data;
            td = ehcitrans->tdvirt;
        }

        if(usbtrans == NULL)
        {
            break;
        }

        __asm__ __volatile__("pause":::);
        /*
        set_task_waking_signal(this_core->cur_task, 0);
        __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_SELECT_EVENT);
        block_task_timeout(this_core->cur_task, 1);
        */
    }

    if(timeout <= 0)
    {
        struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;

        switch_tty(1);
        printk("%s: async transfer timed out\n", "ehci");
        printk("ehci_add_async: 1 status %x, head %lx\n", 
                pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS), 
                pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_ASYNCLISTADDR));
        kpanic("ehci: async transfer timed out\n");
    }
}


static void ehci_add_async(struct usb_transfer_t *transfer)
{
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;
    volatile uint32_t dword;

    // start the async scheduler if it is not running
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);

    if(!(dword & ECHI_USBSTS_ASYNCSTS))
    {
        enable_async_sched(ehci);
    }

    link_async_qh(transfer);
}


static void ehci_add_periodic(struct usb_transfer_t *transfer)
{
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;
    struct ehci_qh_t *qh = transfer->data;
    volatile uint32_t dword;
    volatile int i;

    // start the periodic scheduler if it is not running
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);

    if(!(dword & ECHI_USBSTS_PERIODSTS))
    {
        enable_periodic_sched(ehci);
    }

    kernel_mutex_lock_infinite_wait(&ehci->qh_lock);

    for(i = 0; i < 1024; i++)
    {
        if(ehci->framelist[i] == 0x01)
        {
            break;
        }
    }

    if(i == 1024)
    {
        kernel_mutex_unlock(&ehci->qh_lock);
        printk("ehci: periodic table is full\n");
        return;
    }

    if(transfer->type == USB_TRANSFER_INTERRUPT)
    {
        ehci->framelist[i] = qh->self_phys | (1 << 1);     // QH
    }
    else if(transfer->type == USB_TRANSFER_ISOCHRONOUS)
    {
        kpanic("ehci: isochronous transfers not implemented yet!\n\n");
    }

    kernel_mutex_unlock(&ehci->qh_lock);
}


static void ehci_schedule_transfer(struct usb_transfer_t *transfer)
{
    struct usb_dev_t *usb = transfer->dev;
    struct ehci_transaction_t *first = transfer->trans_head->data;
    struct ehci_qh_t *qh = transfer->data;
    //struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;
    uint8_t smask = 0;

    if(transfer->type == USB_TRANSFER_INTERRUPT)
    {
        smask = (1 << 7);
    }

    if(transfer->type != USB_TRANSFER_ISOCHRONOUS)
    {
        uintptr_t qhphys = qh->self_phys;

        A_memset(qh, 0, sizeof(struct ehci_qh_t));
        qh->self_phys = qhphys;

        qh->next = 1 | 2;   // Terminate | QH
        qh->devaddr = usb->num;
        qh->endpoint = transfer->endpoint->addr;
        qh->endpoint_speed = 2;     // High speed
        qh->toggle = 1;             // Get data toggle from qTD
        qh->mps = transfer->pktsz;
        qh->int_sched_mask = smask;
        qh->mult = 1;               // High bandwidth multiplier
        qh->next_qtd_alt = 1;       // Terminate list

        if(first->tdphys)
        {
            qh->next_qtd = (uintptr_t)first->tdphys;
        }
        else
        {
            qh->next_qtd = 1;       // Terminate list
        }
    }

    if(transfer->type == USB_TRANSFER_CTRL)
    {
        ehci_add_async(transfer);
    }
    else if(transfer->type == USB_TRANSFER_BULK)
    {
        ehci_add_async(transfer);
    }
    else if(transfer->type == USB_TRANSFER_INTERRUPT)
    {
        ehci_add_periodic(transfer);
    }
    else if(transfer->type == USB_TRANSFER_ISOCHRONOUS)
    {
        kpanic("ehci: isochronous transfers not implemented yet!\n\n");
    }

    transfer->success = 1;
}


static inline int transaction_success(struct ehci_td_t *td)
{
    return (td->token.bits.status == 0 || td->token.bits.status == 1);
}


static int ehci_poll_transfer(struct usb_transfer_t *transfer)
{
    struct ehci_qh_t *qh = transfer->data;
    struct ehci_transaction_t *first;
    volatile struct usb_transaction_t *usbtrans;
    int done = 1;

    transfer->success = 1;

    // check for completion
    for(usbtrans = transfer->trans_head; usbtrans != NULL; usbtrans = usbtrans->next)
    {
        struct ehci_transaction_t *ehcitrans = usbtrans->data;
        transfer->success = (transfer->success && transaction_success(ehcitrans->tdvirt));
        done = done && !(ehcitrans->tdvirt->token.bits.status & (1 << 7));

        if(transfer->success)
        {
            if(ehcitrans->inbuf && ehcitrans->inlen)
            {
                A_memcpy(ehcitrans->inbuf, ehcitrans->tdbuf, ehcitrans->inlen);
            }
        }
    }

    if(!done)
    {
        return 0;
    }

    // re-schedule
    for(usbtrans = transfer->trans_head; usbtrans != NULL; usbtrans = usbtrans->next)
    {
        struct ehci_transaction_t *ehcitrans = usbtrans->data;

        ehcitrans->tdvirt->token.bits.toggle = transfer->endpoint->toggle;
        transfer->endpoint->toggle = !(transfer->endpoint->toggle);
        ehcitrans->tdvirt->token.bits.status = (1 << 7);
    }

    first = transfer->trans_head->data;
    qh->next_qtd = ((uintptr_t)first->tdphys & 0xFFFFFFF0);
    qh->next_qtd_alt = 1;       // Terminate list

    return transfer->success;
}


static void ehci_wait_transfer(struct usb_transfer_t *transfer)
{
    volatile struct usb_transaction_t *usbtrans;

    __wait_transfer(transfer, 550000000);

    // check results
    transfer->success = 1;

    if(transfer->type == USB_TRANSFER_ISOCHRONOUS)
    {
        kpanic("ehci: isochronous transfers not implemented yet!\n\n");
    }

    for(usbtrans = transfer->trans_head; usbtrans != NULL; usbtrans = usbtrans->next)
    {
        struct ehci_transaction_t *uhcitrans = usbtrans->data;

        transfer->success = (transfer->success && transaction_success(uhcitrans->tdvirt));

        if(transfer->success)
        {
            if(uhcitrans->inbuf && uhcitrans->inlen)
            {
                A_memcpy(uhcitrans->inbuf, uhcitrans->tdbuf, uhcitrans->inlen);
            }
        }
    }
}


static struct ehci_td_t *ehci_alloc_td(struct usb_dev_t *usb, uintptr_t *tdphys)
{
    struct ehci_td_t *td;

    if(!(td = alloc_td(usb->priv, tdphys)))
    {
        return NULL;
    }

    A_memset(td, 0, sizeof(struct ehci_td_t));
    td->next_qtd = 0x01;
    td->next_qtd_alt = 0x01;

    td->token.bits.status = (1 << 7);   // set active bit
    td->token.bits.errcnt = 3;

    return td;
}


static int ehci_alloc_tdbuf(struct usb_dev_t *usb, struct ehci_td_t *td, 
                            uintptr_t *virt, size_t len)
{
    /*
     * TODO: handle larger buffer sizes
     */
    if(len > EHCI_TDBUF_SIZE)
    {
        kpanic("ehci: TD buffer length > 4096\n");
    }

    if(len)
    {
        uintptr_t tdvirt, tdphys;

        if(!(tdvirt = alloc_tdbuf(usb->priv, &tdphys)))
        {
            return -ENOMEM;
        }

        td->buf0 = tdphys;
        *virt = tdvirt;
    }
    else
    {
        td->buf0 = 0;
        *virt = 0;
    }

    return 0;
}


static int ehci_td_setup(struct usb_transaction_t *transaction, 
                         struct ehci_transaction_t *et)
{
    struct usb_dev_t *usb = transaction->dev;
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)usb->priv;
    struct ehci_td_t *td;
    volatile struct usb_request_t *req;
    uintptr_t bufvirt, tdphys;

    if(!(td = ehci_alloc_td(usb, &tdphys)))
    {
        printk("%s: failed to set up transaction descriptor\n", "ehci");
        return -ENOMEM;
    }

    td->token.bits.pktid = USB_TRANS_SETUP;
    td->token.bits.bytes = 8;
    td->token.bits.toggle = transaction->toggle;

    if(ehci_alloc_tdbuf(usb, td, &bufvirt, sizeof(struct usb_request_t)) < 0)
    {
        printk("%s: failed to alloc TD buffer\n", "ehci");
        free_td(ehci, (uintptr_t)td);
        return -ENOMEM;
    }

    req = (void *)bufvirt;
    req->type = transaction->type;
    req->req = transaction->req;
    req->hival = transaction->hival;
    req->loval = transaction->loval;
    req->index = transaction->index;
    req->len = transaction->len;

    et->tdbuf = (void *)bufvirt;
    et->tdvirt = td;
    et->tdphys = (void *)tdphys;

    return 0;
}


static int ehci_td_setup_io(struct usb_transaction_t *transaction, 
                            struct ehci_transaction_t *et, 
                            uint8_t pktid)
{
    struct usb_dev_t *usb = transaction->dev;
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)usb->priv;
    struct ehci_td_t *td;
    uintptr_t bufvirt, tdphys;

    if(!(td = ehci_alloc_td(usb, &tdphys)))
    {
        printk("%s: failed to set up transaction descriptor\n", "ehci");
        return -ENOMEM;
    }

    td->token.bits.pktid = pktid;
    td->token.bits.bytes = transaction->len;
    td->token.bits.toggle = transaction->toggle;

    if(ehci_alloc_tdbuf(usb, td, &bufvirt, transaction->len) < 0)
    {
        printk("%s: failed to alloc TD buffer\n", "ehci");
        free_td(ehci, (uintptr_t)td);
        return -ENOMEM;
    }

    et->tdbuf = (void *)bufvirt;
    et->tdvirt = td;
    et->tdphys = (void *)tdphys;

    return 0;
}


#define APPEND_TD(tranfer, et)                                          \
    if(transfer && transfer->trans_tail) {                              \
        struct ehci_transaction_t *last = transfer->trans_tail->data;   \
        last->tdvirt->next_qtd = ((uintptr_t)et->tdphys);               \
        last->tdvirt->next_qtd_alt = 1;                                 \
    }


static int ehci_setup_transaction(struct usb_transaction_t *transaction)
{
    struct ehci_transaction_t *et;
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)transaction->transfer;

    if(!(et = kmalloc(sizeof(struct ehci_transaction_t))))
    {
        printk("%s: failed to set up transaction\n", "ehci");
        return -ENOMEM;
    }

    A_memset(et, 0, sizeof(struct ehci_transaction_t));
    transaction->data = et;
    et->inbuf = NULL;
    et->inlen = 0;

    if(ehci_td_setup(transaction, et) < 0)
    {
        kfree(et);
        return -ENOMEM;
    }

    APPEND_TD(tranfer, et);

    return 0;
}


static void ehci_in_transaction(struct usb_transaction_t *transaction)
{
    struct ehci_transaction_t *et;
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)transaction->transfer;

    if(transfer->type == USB_TRANSFER_ISOCHRONOUS)
    {
        kpanic("ehci: isochronous transfers not implemented yet!\n\n");
    }

    if(!(et = kmalloc(sizeof(struct ehci_transaction_t))))
    {
        printk("%s: failed to set up IN transaction\n", "ehci");
        return;
    }

    A_memset(et, 0, sizeof(struct ehci_transaction_t));
    transaction->data = et;
    et->inbuf = transaction->buf;
    et->inlen = transaction->len;

    if(ehci_td_setup_io(transaction, et, USB_TRANS_IN) < 0)
    {
        kfree(et);
        return;
    }

    APPEND_TD(tranfer, et);
}


static void ehci_out_transaction(struct usb_transaction_t *transaction)
{
    struct ehci_transaction_t *et;
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)transaction->transfer;

    if(transfer->type == USB_TRANSFER_ISOCHRONOUS)
    {
        kpanic("ehci: isochronous transfers not implemented yet!\n\n");
    }

    if(!(et = kmalloc(sizeof(struct ehci_transaction_t))))
    {
        printk("%s: failed to set up OUT transaction\n", "ehci");
        return;
    }

    A_memset(et, 0, sizeof(struct ehci_transaction_t));
    transaction->data = et;
    et->inbuf = NULL;
    et->inlen = 0;

    if(ehci_td_setup_io(transaction, et, USB_TRANS_OUT) < 0)
    {
        kfree(et);
        return;
    }

    if(transaction->buf && transaction->len)
    {
        A_memcpy(et->tdbuf, transaction->buf, transaction->len);
    }

    APPEND_TD(tranfer, et);
}

#undef APPEND_TD


static void ehci_free_usb_transaction_data(volatile struct usb_transaction_t *usbtrans)
{
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)usbtrans->transfer;
    struct ehci_dev_t *ehci = (struct ehci_dev_t *)transfer->dev->priv;
    struct ehci_transaction_t *et = usbtrans->data;

    if(!et)
    {
        return;
    }

    if(et->tdvirt)
    {
        free_td(ehci, (uintptr_t)et->tdvirt);
    }

    if(et->tdbuf)
    {
        free_tdbuf(ehci, (uintptr_t)et->tdbuf);
    }

    et->tdvirt = 0;
    et->tdphys = 0;
    et->tdbuf = 0;

    kfree(et);
    usbtrans->data = NULL;
}


static int ehci_setup_device(volatile struct ehci_dev_t *ehci, unsigned int port, uint8_t speed)
{
    struct usb_dev_t *usb;
    int res;

    printk("%s: setting up high-speed USB device on port %d\n", "ehci", port);

    if(!(usb = usb_create_dev(ehci->pci->unit, port, speed)))
    {
        printk("%s: failed to create USB device\n", "ehci");
        return -ENOMEM;
    }

    usb->type = USB_TYPE_EHCI;
    usb->priv = (void *)ehci;
    usb->ops = &ehci_ops;

    // TODO: check the result of ehci_get_next_addr() != 0
    if((res = usb_setup_device(usb, ehci_get_next_addr((void *)ehci) /* port + 1 */)) < 0)
    {
        printk("%s: failed to set up USB device\n", "ehci");
        ehci_free_addr((void *)ehci, usb->num);
        usb_destroy_dev(usb);
        return res;
    }

    ehci->ports[port].usb = usb;
    ehci->ports[port].flags |= EHCI_PORT_FLAG_CONNECTED;

    return 0;
}


static int ehci_start(volatile struct ehci_dev_t *ehci)
{
    struct pci_dev_t *pci = ehci->pci;
    volatile uint8_t byte;
    volatile uint32_t dword;
    volatile int timeout;

    // start by deactivating legacy support

    // first get the EHCI Extended Capabilities Pointer (EECP), which is
    // the second byte of the HCCPARAMS register
    dword = pcidev_inl(ehci, EHCI_REG_HCCAP + HCCAP_HCCPARAMS);
    byte = BYTE2(dword);
    //printk("1 dword %x, byte %x\n", dword, byte);

    // 0x00 means no extended capabilities
    // >= 0x40 is an offset in the PCI config space
    if(byte >= 0x40)
    {
        dword = pci_config_read_long(pci, byte);
        //printk("2 dword %x, byte %x\n", dword, byte);

        if((dword & 0xFF) == 1)
        {
            // set the OS semaphore
            dword |= (1 << 24);
            pci_config_write_long(pci, byte, dword);
            timeout = 300;
            //printk("3 dword %x, byte %x\n", dword, byte);

            // wait for the BIOS semaphore to clear
            while((pci_config_read_long(pci, byte) & (1 << 16)) && timeout--)
            {
                tick_delay(1);
            }

            if(timeout <= 0)
            {
                /*
                 * TODO: should this be a fatal error?
                 */
                printk("%s: BIOS semaphore timeout\n", "ehci");
            }

            // wait for the OS semaphore to set
            timeout = 300;

            while(!(pci_config_read_long(pci, byte) & (1 << 24)) && timeout--)
            {
                tick_delay(1);
            }

            if(timeout <= 0)
            {
                /*
                 * TODO: should this be a fatal error?
                 */
                printk("%s: OS semaphore timeout\n", "ehci");
            }

            // disable USB SMI
            pci_config_write_long(pci, byte + 4, 0x00);
            tick_delay(50);
            printk("%s: finished BIOS handover\n", "ehci");
        }
        else
        {
            printk("%s: could not find legacy support capability\n", "ehci");
        }
    }
    else
    {
        printk("%s: failed to find EECP\n", "ehci");
    }

    // get the capabilities register length
    ehci->caplen = pcidev_inl(ehci, EHCI_REG_HCCAP + HCCAP_CAPLENGTH) & 0xFF;

    // the operations register is right after it
    // use it to get port count
    dword = pcidev_inl(ehci, EHCI_REG_HCCAP + HCCAP_HCSPARAMS);
    ehci->port_count = dword & 0xf;

    printk("%s: root ports %u\n", "ehci", ehci->port_count);

    // stop the host controller
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
    dword &= ~(1 << 0);     // clear the run/stop bit
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

    // wait for host controller to signal it stopped
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);

    while(!(dword & ECHI_USBSTS_HCHALTED))
    {
        tick_delay(2);
        dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);
    }

    // clear the status register
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBSTS, 0x3F);

    // clear the interrupt register
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBINTR, 0);

    // now program the 4G segment selector register
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_CTRLDSSEGMENT, 0);

    // reset the host controller
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
    dword |= (1 << 1);
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

    // wait for host controller to signal it reset
    timeout = 50;

    while((pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD) & (1 << 1)) && timeout--)
    {
        tick_delay(1);
    }

    if(timeout <= 0)
    {
        printk("%s: reset timeout\n", "ehci");
        return -ETIMEDOUT;
    }

    /*
    // set the interrupt threshold
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
    dword |= EHCI_USBCMD_INTTHRSHLD_8;
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

    // set the interrupt register
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBINTR, 0x07);
    */

    // set the frame index
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_FRINDEX, 0x00);

    // init the async list
    if(!ehci->async_qh)
    {
        ehci->async_qh = alloc_qh(ehci);
        ehci->tail_qh = ehci->async_qh;

        uintptr_t qhphys = ehci->async_qh->self_phys;

        A_memset((void *)ehci->async_qh, 0, sizeof(struct ehci_qh_t));
        ehci->async_qh->self_phys = qhphys;

        ehci->async_qh->next = ehci->async_qh->self_phys | (1 << 1);   // QH
        ehci->async_qh->nextvirt = ehci->async_qh;
        ehci->async_qh->endpoint_speed = 2;     // High speed
        ehci->async_qh->toggle = 1;             // Get data toggle from qTD
        ehci->async_qh->head_flag = 1;          // Mark as head of reclamation list
        ehci->async_qh->mult = 1;               // High bandwidth multiplier
        ehci->async_qh->next_qtd = 1;           // Terminate list
        ehci->async_qh->next_qtd_alt = 1;       // Terminate list
    }

    // init the periodic list
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
    dword &= ECHI_USBCMD_FRLIST_MASK;
    dword |= EHCI_USBCMD_FRLIST_1024;
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_PERIODICLISTBASE, (uintptr_t)ehci->framelist_phys);
    enable_periodic_sched(ehci);

    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_ASYNCLISTADDR, ehci->async_qh->self_phys);
    enable_async_sched(ehci);

    // clear the status register
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBSTS, 0x3F);

    // start the controller
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);

    if(dword & ECHI_USBSTS_HCHALTED)
    {
        printk("%s: starting controller\n", "ehci");
        dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBCMD);
        dword |= (1 << 0);      // set the run/stop bit
        pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBCMD, dword);

        tick_delay(50);
        dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);
        printk("ehci: controller status 0x%x\n", dword);
    }

    /*
    for(volatile int i = 0; i < ehci->port_count; i++)
    {
        int reg = EHCI_REG_HCOP + HCOP_PORTSC + (4 * i);
        dword = pcidev_inl(ehci, reg);
        printk("1 port[%d] status %x\n", i, dword);

        if(!(dword & EHCI_PORTSC_POWER))
        {
            dword |= EHCI_PORTSC_POWER;
            pcidev_outl(ehci, reg, dword);
            tick_delay(50);
        }
        printk("2 port[%d] status %x\n", i, dword);
    }
    */

    // set the config flag register
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_CONFIGFLAG, 0x01);

    // enable ports
    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);

    if(!(dword & ECHI_USBSTS_HCHALTED))
    {
        printk("%s: controller running, enabling ports\n", "ehci");
        return ehci_enable_ports(ehci);
    }
    else
    {
        printk("ehci: controller halted (status 0x%x)\n", dword);
        return -EINVAL;
    }
}


int ehci_install(struct pci_dev_t *pci, struct pci_bar_t *bar)
{
    struct ehci_dev_t *ehci;
    //char buf[8];
    int res = 0;
    uintptr_t base = bar->base;
    volatile unsigned int i;

    if(!(ehci = kmalloc(sizeof(struct ehci_dev_t))))
    {
        printk("%s: insufficient memory to init device\n", "echi");
        return -ENOMEM;
    }
    
    A_memset(ehci, 0, sizeof(struct ehci_dev_t));

    // entry 0 in the device address bitmap is always used so that we
    // don't use device address 0 by mistake, as it is reserved
    ehci->addr_bitmap[0] = 1;

    ehci->pci = pci;
    ehci->iosize = bar->iosize;

    // check whether I/O is memory-mapped or normal I/O
    if(bar->iotype == PCI_IOTYPE_MMIO)
    {
        // MMIO
        base &= ~0xf;
        ehci->iobase = mmio_map(base, base + bar->iosize);
        ehci->mmio = 1;
    }
    else
    {
        // I/O
        ehci->iobase = base & ~0x3;
    }

    printk("%s: base " _XPTR_ ", iobase " _XPTR_ ", iosize " _XPTR_ " (%s)\n", 
           "ehci", base, ehci->iobase, ehci->iosize, ehci->mmio ? "MMIO" : "I/O");

    if(!first_ehci)
    {
        first_ehci = ehci;
    }
    else
    {
        volatile struct ehci_dev_t *tmp = first_ehci;

        while(tmp->next)
        {
            tmp = tmp->next;
        }

        tmp->next = ehci;
    }

    /*
    // register IRQ handler
    ksprintf(buf, 8, "ehci%d", pci->unit);
    pci_register_irq_handler(pci, ehci_intr, buf);
    */

    pci_enable_busmastering(pci);
    //pci_enable_interrupts(pci);
    pci_enable_memoryspace(pci);

    // alloc memory for frame list
   	if(!(ehci->framelist_phys = pmmngr_alloc_block()))
    {
        res = -ENOMEM;
        goto err;
    }

    ehci->framelist = (uint32_t *)mmio_map((physical_addr)ehci->framelist_phys, 
                                           (physical_addr)ehci->framelist_phys + PAGE_SIZE);

    // init the frame list
    for(i = 0; i < 1024; i++)
    {
        ehci->framelist[i] = (uint32_t)0x01;
    }

    // alloc memory for transfer descriptor pool
   	if(!(ehci->tdpool_phys = (uintptr_t)pmmngr_alloc_block()))
    {
        res = -ENOMEM;
        goto err;
    }

    ehci->tdpool = mmio_map(ehci->tdpool_phys, ehci->tdpool_phys + PAGE_SIZE);
    init_tdpool(ehci);

    printk("%s: tdpool virt " _XPTR_ ", phys " _XPTR_ "\n", 
                "ehci", ehci->tdpool, ehci->tdpool_phys);

    // alloc memory for transfer descriptor buffer pool
    if(!(ehci->tdbufpool_phys = 
            (uintptr_t)pmmngr_alloc_blocks(EHCI_TDBUF_POOL_SIZE / PAGE_SIZE)))
    {
        res = -ENOMEM;
        goto err;
    }

    ehci->tdbufpool = mmio_map(ehci->tdbufpool_phys, ehci->tdbufpool_phys + EHCI_TDBUF_POOL_SIZE);

    printk("%s: tdbufpool virt " _XPTR_ ", phys " _XPTR_ "\n", 
                "ehci", ehci->tdbufpool, ehci->tdbufpool_phys);

    // alloc memory for queue header pool
   	if(!(ehci->qhpool_phys = (uintptr_t)pmmngr_alloc_block()))
    {
        res = -ENOMEM;
        goto err;
    }

    ehci->qhpool = mmio_map(ehci->qhpool_phys, ehci->qhpool_phys + PAGE_SIZE);
    init_qhpool(ehci);

    printk("%s: qhpool virt " _XPTR_ ", phys " _XPTR_ "\n", 
                "ehci", ehci->qhpool, ehci->qhpool_phys);

    if((uintptr_t)ehci->framelist_phys > 0xffffffff ||
       ehci->tdpool_phys > 0xffffffff ||
       ehci->tdbufpool_phys > 0xffffffff ||
       ehci->qhpool_phys > 0xffffffff)
    {
        printk("ehci: fr %lx, tdp %lx, td %lx, qh %lx\n", 
                ehci->framelist_phys, ehci->tdpool_phys, 
                ehci->tdbufpool_phys, ehci->qhpool_phys);
        kpanic("ehci: could not get physical frames < 0xffffffff\n");
        for(;;);
    }

    if((res = ehci_start(ehci)) < 0)
    {
        goto err;
    }

    ehci->flags |= EHCI_FLAG_RUN;

    printk("%s: setup done\n", "ehci");

    return 0;


err:

    if(ehci->mmio)
    {
        vmmngr_free_pages(ehci->iobase, ehci->iobase + ehci->iosize);
        ehci->iobase = 0;
    }

    if(ehci->framelist_phys)
    {
        pmmngr_free_block(ehci->framelist_phys);
        ehci->framelist = 0;
        ehci->framelist_phys = 0;
    }

    if(ehci->tdpool_phys)
    {
        pmmngr_free_block((void *)ehci->tdpool_phys);
        ehci->tdpool_phys = 0;
        ehci->tdpool = 0;
    }

    if(ehci->tdbufpool_phys)
    {
        pmmngr_free_blocks((void *)ehci->tdbufpool_phys, EHCI_TDBUF_POOL_SIZE / PAGE_SIZE);
        ehci->tdbufpool_phys = 0;
        ehci->tdbufpool = 0;
    }

    if(ehci->qhpool_phys)
    {
        pmmngr_free_block((void *)ehci->qhpool_phys);
        ehci->qhpool_phys = 0;
        ehci->qhpool = 0;
    }

    return res;
}


void ehci_poll(void)
{
    volatile struct ehci_dev_t *ehci = first_ehci;
    volatile struct usb_dev_t *usb;
    volatile unsigned int i;
    uint32_t dword;
    int reg;

    /*
    printk("ehci_poll:\n");
    kpanic("*******\n");
    for(;;);
    */

    while(ehci)
    {
        if(!(ehci->flags & EHCI_FLAG_RUN) ||
           !(ehci->flags & EHCI_FLAG_PORTENABLED))
        {
            ehci = ehci->next;
            continue;
        }

        // for each EHCI port
        for(i = 0; i < ehci->port_count; i++)
        {
            // check port status
            reg = EHCI_REG_HCOP + HCOP_PORTSC + (i * 4);
            dword = pcidev_inl(ehci, reg);
            //printk("ehci_poll: reg %x, status %x\n", reg, dword);

            // and if there is a device change
            if(dword & EHCI_PORTSC_CONNCHG)
            {
                // acknowledge that
                pcidev_outl(ehci, reg, dword);

                // and if there is a new device attached, set it up
                if(dword & EHCI_PORTSC_CONN)
                {
                    if(!(ehci->ports[i].flags & EHCI_PORT_FLAG_CONNECTED))
                    {
                        printk("ehci: device connected to port %d\n", i);
                        ehci_check_port_state(ehci, i);
                        ehci->ports[i].port = i;
                        ehci->ports[i].ehci = ehci;
                    }
                }
                // and if a device was removed, delete it
                else
                {
                    printk("ehci: device removed from port %d\n", i);
                    usb = ehci->ports[i].usb;

                    if(usb && usb->type == USB_TYPE_EHCI)
                    {
                        ehci->ports[i].usb = NULL;
                        ehci_free_addr((void *)ehci, usb->num);
                        usb_destroy_dev((struct usb_dev_t *)usb);
                        ehci->ports[i].flags &= ~EHCI_PORT_FLAG_CONNECTED;
                    }
                }
            }
        }

        ehci = ehci->next;
    }
}


#if 0

/*
 * IRQ callback function.
 */
int ehci_intr(struct regs *r, void *arg)
{
    UNUSED(r);

    printk("ehci_intr:\n");
    //screen_refresh(NULL);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    kpanic("!!!!!\n");
    //for(;;);

    struct pci_dev_t *pci = arg;
    volatile struct ehci_dev_t *ehci = first_ehci;
    volatile uint32_t dword;

    while(ehci)
    {
        if(ehci->pci == pci)
        {
            break;
        }

        ehci = ehci->next;
    }

    if(!ehci)
    {
        printk("ehci_intr: not from here\n");
        return 0;
    }

    dword = pcidev_inl(ehci, EHCI_REG_HCOP + HCOP_USBSTS);

    if(!dword)
    {
        // IRQ not from this device
        printk("ehci_intr: word 0\n");
        return 0;
    }

    //printk("ehci_intr[%d:%d:%d]: status 0x%x\n", pci->bus, pci->dev, pci->function, dword);

    // acknowledge interrupt
    pcidev_outl(ehci, EHCI_REG_HCOP + HCOP_USBSTS, dword);

    if(dword & ECHI_USBSTS_HSERR)
    {
        printk("ehci_intr[%d:%d:%d]: host system error (status 0x%x)\n", pci->bus, pci->dev, pci->function, dword);
        kpanic("@@@@@@@@\n");
        for(;;);
        ehci_start(ehci);
    }

    pic_send_eoi(ehci->pci->irq[0]);
    printk("ehci_intr: irq %d\n", ehci->pci->irq[0]);

    return 1;
}

#endif


static struct usb_ops_t ehci_ops =
{
    .setup_transfer = ehci_setup_transfer,
    .schedule_transfer = ehci_schedule_transfer,
    .wait_transfer = ehci_wait_transfer,
    .poll_transfer = ehci_poll_transfer,
    .delete_transfer = ehci_delete_transfer,
    .setup_transaction = ehci_setup_transaction,
    .in_transaction = ehci_in_transaction,
    .out_transaction = ehci_out_transaction,
    .free_transaction_data = ehci_free_usb_transaction_data,
    .get_next_addr = ehci_get_next_addr,
    .free_addr = ehci_free_addr,
};

