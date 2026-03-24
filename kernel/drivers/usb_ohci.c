/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb_ohci.c
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
 *  \file usb_ohci.c
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
#include <kernel/usb_ohci.h>
#include <kernel/pic.h>
#include <mm/kheap.h>


static volatile struct ohci_dev_t *first_ohci = NULL;
static struct usb_ops_t ohci_ops;

int ohci_intr(struct regs *r, void *arg);


struct usb_dev_t *ohci_get_dev_struct(struct pci_dev_t *bus, uint8_t num)
{
    volatile struct ohci_dev_t *ohci = first_ohci;

    if(!bus || num < 1)
    {
        return NULL;
    }

    while(ohci)
    {
        if(ohci->pci == bus)
        {
            if(num > ohci->port_count)
            {
                return NULL;
            }

            return ohci->ports[num - 1].usb;
        }

        ohci = ohci->next;
    }

    return NULL;
}


static unsigned int ohci_get_next_addr(void *__ohci)
{
    struct ohci_dev_t *ohci = __ohci;
    volatile unsigned int i, j;

    for(i = 0; i < (MAX_DEV_PER_HC / sizeof(uint32_t)); i++)
    {
        for(j = 0; j < 32; j++)
        {
            if(!(ohci->addr_bitmap[i] & (1 << j)))
            {
                ohci->addr_bitmap[i] |= (1 << j);
                return (i * 32) + j;
            }
        }
    }

    return 0;
}


static void ohci_free_addr(void *__ohci, unsigned int i)
{
    struct ohci_dev_t *ohci = __ohci;

    if(i < 1 || i >= MAX_DEV_PER_HC)
    {
        return;
    }

    ohci->addr_bitmap[i / 32] &= ~(1 << (i % 32));
}


static void init_tdpool(struct ohci_dev_t *ohci)
{
    struct ohci_td_t *td, *tdl = ohci->tdpool + OHCI_MAX_TD;
    uintptr_t tdpool_phys = ohci->tdpool_phys;

    for(td = ohci->tdpool; td != tdl; td++)
    {
        td->self_phys = tdpool_phys;
        tdpool_phys += sizeof(struct ohci_td_t);
    }
}


static void init_edpool(struct ohci_dev_t *ohci)
{
    struct ohci_ed_t *ed, *edl = ohci->edpool + OHCI_MAX_ED;
    uintptr_t edpool_phys = ohci->edpool_phys;

    for(ed = ohci->edpool; ed != edl; ed++)
    {
        ed->self_phys = edpool_phys;
        edpool_phys += sizeof(struct ohci_ed_t);
    }
}


static void free_td(struct ohci_td_t *td)
{
    __sync_bool_compare_and_swap(&td->alloced, 1, 0);
}


static struct ohci_td_t *alloc_td(struct ohci_dev_t *ohci)
{
    struct ohci_td_t *td, *tdl = ohci->tdpool + OHCI_MAX_TD;

    for(td = ohci->tdpool; td != tdl; td++)
    {
        if(__sync_bool_compare_and_swap(&td->alloced, 0, 1))
        {
            return td;
        }
    }

    printk("%s: failed to alloc TD\n", "ohci");

    return NULL;
}


static void free_ed(struct ohci_ed_t *ed)
{
    __sync_bool_compare_and_swap(&ed->alloced, 1, 0);
}


static struct ohci_ed_t *alloc_ed(struct ohci_dev_t *ohci)
{
    struct ohci_ed_t *ed, *edl = ohci->edpool + OHCI_MAX_ED;

    for(ed = ohci->edpool; ed != edl; ed++)
    {
        if(__sync_bool_compare_and_swap(&ed->alloced, 0, 1))
        {
            return ed;
        }
    }

    printk("%s: failed to alloc ED\n", "ohci");

    return NULL;
}


static struct ohci_ed_t *ed_from_phys(struct ohci_dev_t *ohci, uintptr_t phys)
{
    struct ohci_ed_t *ed, *edl = ohci->edpool + OHCI_MAX_ED;

    for(ed = ohci->edpool; ed != edl; ed++)
    {
        if(ed->self_phys == phys)
        {
            return ed;
        }
    }

    printk("%s: failed to find ED: " _XPTR_ "\n", "ohci", phys);

    return NULL;
}


static void free_tdbuf(struct ohci_dev_t *ohci, uintptr_t tdbuf)
{
    volatile int i = (tdbuf - ohci->tdbufpool) / OHCI_TDBUF_SIZE;

    __sync_bool_compare_and_swap(&ohci->tdbuf_used[i], 1, 0);
    //print_buf_usage(ohci, __func__);
}


static uintptr_t alloc_tdbuf(struct ohci_dev_t *ohci, uintptr_t *tdbufphys)
{
    volatile int i;

    *tdbufphys = 0;

    for(i = 0; i < MAX_TDBUF; i++)
    {
        if(__sync_bool_compare_and_swap(&ohci->tdbuf_used[i], 0, 1))
        {
            *tdbufphys = ohci->tdbufpool_phys + (i * OHCI_TDBUF_SIZE);
            return ohci->tdbufpool + (i * OHCI_TDBUF_SIZE);
        }
    }

    //print_buf_usage(ohci, __func__);
    printk("%s: failed to alloc TD buffer\n", "ohci");

    return 0;
}


static void reset_port(volatile struct ohci_dev_t *ohci, unsigned int port)
{
    int timeout;
    int reg = OHCI_REG_ROOTHUB_PRSTS + (port * 4);

    // clear reset bit
    pcidev_outl(ohci, reg, OHCI_RHP_PORTRST_STS);

    // wait for reset bit to clear
    timeout = 20;

    while((pcidev_inl(ohci, reg) & OHCI_RHP_PORTRST_STS) && timeout--)
    {
        tick_delay(1);
    }

    if(timeout <= 0)
    {
        printk("%s: reset timeout for port %u\n", "ohci", port);
        return;
    }

    // enable port
    pcidev_outl(ohci, reg, OHCI_RHP_PORTEN_STS);
    tick_delay(5);
    printk("%s: port %u reset: status 0x%x\n", "ohci", port, pcidev_inl(ohci, reg));
}


static int ohci_setup_device(volatile struct ohci_dev_t *ohci, unsigned int port, uint8_t speed)
{
    volatile uint32_t dword;
    struct usb_dev_t *usb;
    int res;

    // mark it connected now so we don't get interrupted by an.. well.. interrupt
    ohci->ports[port].flags |= OHCI_PORT_FLAG_CONNECTED;

    // wait 100ms until power is stable
    tick_delay(10);

    // reset port on device attachment
    reset_port(ohci, port);

    dword = pcidev_inl(ohci, OHCI_REG_ROOTHUB_PRSTS + (port * 4));

    // check port is enabled
    if(dword & OHCI_RHP_PORTEN_STS)
    {
        // and that it has power and a device is attached
        if((dword & OHCI_RHP_PORTPWR_STS) && (dword & OHCI_RHP_CUR_CONN_STS))
        {
            if(!(usb = usb_create_dev(ohci->pci->unit, port, speed)))
            {
                printk("%s: failed to create USB device\n", "ohci");
                return -ENOMEM;
            }

            usb->type = USB_TYPE_OHCI;
            usb->priv = (void *)ohci;
            usb->ops = &ohci_ops;

            // TODO: check the result of ohci_get_next_addr() != 0
            if((res = usb_setup_device(usb, ohci_get_next_addr((void *)ohci) /* port + 1 */)) < 0)
            {
                printk("%s: failed to set up USB device\n", "ohci");
                ohci_free_addr((void *)ohci, usb->num);
                usb_destroy_dev(usb);
                return res;
            }

            ohci->ports[port].usb = usb;
        }
    }

#define ACKNOWLEDGE_CHANGE(o, p, d, f)  \
    if(d & f) pcidev_outl(o, OHCI_REG_ROOTHUB_PRSTS + (p * 4), f);

    ACKNOWLEDGE_CHANGE(ohci, port, dword, OHCI_RHP_CONN_STS_CHG);
    ACKNOWLEDGE_CHANGE(ohci, port, dword, OHCI_RHP_PORTEN_STS_CHG);
    ACKNOWLEDGE_CHANGE(ohci, port, dword, OHCI_RHP_PORTSUSPND_STS_CHG);
    ACKNOWLEDGE_CHANGE(ohci, port, dword, OHCI_RHP_PORTOVRCUR_CHG);
    ACKNOWLEDGE_CHANGE(ohci, port, dword, OHCI_RHP_PORTRST_STS_CHG);

#undef ACKNOWLEDGE_CHANGE

    return 0;
}


static void check_port_status(volatile struct ohci_dev_t *ohci, unsigned int port)
{
    volatile uint32_t dword;

    // check status has actually changed
    dword = pcidev_inl(ohci, OHCI_REG_ROOTHUB_PRSTS + (port * 4));

    if(!!(dword & OHCI_RHP_CUR_CONN_STS) == 
       !!(ohci->ports[port].flags & OHCI_PORT_FLAG_CONNECTED))
    {
        return;
    }

    printk("ohci: port %u: %sspeed device %s\n", 
            port,
            (dword & OHCI_RHP_LOSPEED) ? "Low " : "Full",
            (dword & OHCI_RHP_CUR_CONN_STS) ? "attached" : "removed");

    if(dword & OHCI_RHP_CUR_CONN_STS)
    {
        ohci_setup_device(ohci, port, (dword & OHCI_RHP_LOSPEED) ?
                                          USB_SPEED_LOW : USB_SPEED_FULL);
    }
    else
    {
        struct usb_dev_t *usb;

        usb = ohci->ports[port].usb;

        if(usb && usb->type == USB_TYPE_OHCI)
        {
            ohci->ports[port].usb = NULL;
            ohci_free_addr((void *)ohci, usb->num);
            usb_destroy_dev(usb);
            ohci->ports[port].flags &= ~OHCI_PORT_FLAG_CONNECTED;
        }
    }
}


static int ohci_enable_ports(struct ohci_dev_t *ohci)
{
    volatile unsigned int i;
    volatile uint32_t dword;

    if(!(ohci->ports = kmalloc(sizeof(struct ohci_port_t) * ohci->port_count)))
    {
        printk("%s: insufficienct memory to enable ports\n", "ohci");
        return -ENOMEM;
    }

    A_memset((void *)ohci->ports, 0, sizeof(struct ohci_port_t) * ohci->port_count);

    for(i = 0; i < ohci->port_count; i++)
    {
        ohci->ports[i].port = i;
        ohci->ports[i].ohci = ohci;
    }

    for(i = 0; i < ohci->port_count; i++)
    {
        dword = pcidev_inl(ohci, OHCI_REG_ROOTHUB_PRSTS + (i * 4));

        if(dword & OHCI_RHP_CUR_CONN_STS)
        {
            check_port_status(ohci, i);
        }
    }

    ohci->flags |= OHCI_FLAG_PORTENABLED;

    return 0;
}


static void ohci_setup_transfer(struct usb_transfer_t *transfer)
{
    struct ohci_dev_t *ohci = (struct ohci_dev_t *)transfer->dev->priv;
    struct ohci_ed_t *ed;

    if(!(ed = alloc_ed(ohci)))
    {
        return;
    }

    transfer->data = ed;

    // zero the struct manually to avoid over-writing the 'alloced' bit
    ed->dword0 = 0;
    ed->td_qtail = 0;
    ed->td_qhead = 0;
    ed->next = 0;
    ed->td_dummy = 0;
}


static void remove_from_list(struct ohci_dev_t *ohci,
                             struct ohci_ed_t *head, struct ohci_ed_t *ed)
{
    volatile struct ohci_ed_t *tmp = head;

    // search for ED by its physical address
    while(tmp->next)
    {
        if(tmp->next == ed->self_phys)
        {
            tmp->next = ed->next;
            break;
        }

        if(!(tmp = ed_from_phys(ohci, tmp->next)))
        {
            break;
        }
    }
}


static void populate_ed(struct usb_transfer_t *transfer)
{
    struct ohci_ed_t *ed = transfer->data;
    struct usb_dev_t *usb = transfer->dev;
    struct ohci_transaction_t *first = transfer->trans_head->data;

    ed->endpoint = transfer->endpoint->addr;
    ed->dev = usb->num;
    ed->mps = transfer->endpoint->mps;
    ed->direction = 0;  // get direction from TD
    ed->speed = (usb->speed == USB_SPEED_LOW);
    ed->format = 0;
    ed->td_qhead = first->tdvirt->self_phys;
    ed->td_qtail = ed->td_dummy->self_phys;
    ed->skip = 0;
}


static int transfer_freq_index(struct usb_transfer_t *transfer)
{
    if(transfer->freq < 2)
    {
        return 5;
    }
    else if(transfer->freq < 4)
    {
        return 4;
    }
    else if(transfer->freq < 8)
    {
        return 3;
    }
    else if(transfer->freq < 16)
    {
        return 2;
    }
    else if(transfer->freq < 32)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


static void ohci_delete_transfer(struct usb_transfer_t *transfer)
{
    struct ohci_dev_t *ohci = (struct ohci_dev_t *)transfer->dev->priv;
    struct ohci_ed_t *ed = transfer->data;
    int i;

    ed->skip = 1;

    kernel_mutex_lock(&ohci->lock);

    if(transfer->type == USB_TRANSFER_BULK)
    {
        remove_from_list(ohci, ohci->ed_bulk_head, ed);
    }
    else if(transfer->type == USB_TRANSFER_CTRL)
    {
        remove_from_list(ohci, ohci->ed_ctrl_head, ed);
    }
    else if(transfer->type == USB_TRANSFER_INTERRUPT)
    {
        i = transfer_freq_index(transfer);
        remove_from_list(ohci, ohci->ed_int_head[i], ed);
    }

    kernel_mutex_unlock(&ohci->lock);

    if(ed->td_dummy)
    {
        free_td((void *)ed->td_dummy);
        ed->td_dummy = NULL;
    }

    free_ed(transfer->data);
    transfer->data = NULL;
}


static void ohci_schedule_transfer(struct usb_transfer_t *transfer)
{
    struct usb_dev_t *usb = transfer->dev;
    struct ohci_dev_t *ohci = (struct ohci_dev_t *)usb->priv;
    struct ohci_transaction_t *last = transfer->trans_tail->data;
    struct ohci_ed_t *ed = transfer->data;
    volatile uint32_t dword;
    int i;

    if(!ed)
    {
        printk("%s: transfer with NULL endpoint descriptor (in %s)\n", "ohci", __func__);
        return;
    }

    // create a dummy TD at the end
    if(!(ed->td_dummy = alloc_td(ohci)))
    {
        printk("%s: failed to set up transaction descriptor\n", "ohci");
        return;
    }

    ed->td_dummy->next = (1 << 0);
    ed->td_dummy->dword0 = 0;
    ed->td_dummy->curbuf = 0;
    ed->td_dummy->bufend = 0;

    last->tdvirt->next = ed->td_dummy->self_phys;
    populate_ed(transfer);

    kernel_mutex_lock(&ohci->lock);

    if(transfer->type == USB_TRANSFER_BULK)
    {
        ed->next = ohci->ed_bulk_head->next;
        ohci->ed_bulk_head->next = ed->self_phys;

        // signal that bulk list is filled
        dword = pcidev_inl(ohci, OHCI_REG_CMD_STS);
        dword |= OHCI_CMDSTS_BLF;
        pcidev_outl(ohci, OHCI_REG_CMD_STS, dword);
    }
    else if(transfer->type == USB_TRANSFER_CTRL)
    {
        ed->next = ohci->ed_ctrl_head->next;
        ohci->ed_ctrl_head->next = ed->self_phys;

        // signal that control list is filled
        dword = pcidev_inl(ohci, OHCI_REG_CMD_STS);
        dword |= OHCI_CMDSTS_CLF;
        pcidev_outl(ohci, OHCI_REG_CMD_STS, dword);
    }
    else if(transfer->type == USB_TRANSFER_INTERRUPT)
    {
        i = transfer_freq_index(transfer);
        ed->next = ohci->ed_int_head[i]->next;
        ohci->ed_int_head[i]->next = ed->self_phys;
    }

    kernel_mutex_unlock(&ohci->lock);

    transfer->success = 1;
}


static int ohci_poll_transfer(struct usb_transfer_t *transfer)
{
    //struct ohci_dev_t *ohci = (struct ohci_dev_t *)transfer->dev->priv;
    struct ohci_ed_t *ed = transfer->data;
    struct ohci_transaction_t *first, *last;
    volatile struct usb_transaction_t *usbtrans;
    int done = 1;

    transfer->success = 1;

    if(!ed)
    {
        printk("%s: transfer with NULL endpoint descriptor (in %s)\n", "ohci", __func__);
        return 0;
    }

    // check for completion
    for(usbtrans = transfer->trans_head; usbtrans != NULL; usbtrans = usbtrans->next)
    {
        struct ohci_transaction_t *ohcitrans = usbtrans->data;
        transfer->success = (transfer->success && (ohcitrans->tdvirt->sts == 0));
        done = done && (ohcitrans->tdvirt->sts != 15); // transfer not accessed

        if(transfer->success)
        {
            if(ohcitrans->inbuf && ohcitrans->inlen)
            {
                A_memcpy(ohcitrans->inbuf, ohcitrans->tdbuf, ohcitrans->inlen);
            }
        }
    }

    if(!done)
    {
        return 0;
    }

    // re-schedule
    first = transfer->trans_head->data;
    last = transfer->trans_tail->data;
    A_memcpy(first->tdvirt, first->tdcopyvirt, sizeof(struct ohci_td_t));

    // XXX: something went wrong and we freed the dummy TD tail
    if(!ed->td_dummy)
    {
        return transfer->success;
    }

    last->tdvirt->next = ed->td_dummy->self_phys;
    populate_ed(transfer);

    return transfer->success;
}


static void ohci_wait_transfer(struct usb_transfer_t *transfer)
{
    //struct ohci_dev_t *ohci = (struct ohci_dev_t *)transfer->dev->priv;
    volatile struct usb_transaction_t *usbtrans = transfer->trans_head;
    volatile int timeout = 150000;
    struct ohci_ed_t *ed = transfer->data;

    if(!ed)
    {
        printk("%s: transfer with NULL endpoint descriptor (in %s)\n", "ohci", __func__);
        return;
    }

    while(timeout--)
    {
        struct ohci_transaction_t *ohcitrans = usbtrans->data;
        struct ohci_td_t *td = ohcitrans->tdvirt;

        //printk("ohci_wait_transfer: td->dword0 0x%x, timeout %d\n", td->dword0, timeout);

        // while not accessed
        while(td->sts != 15)
        {
            if((usbtrans = usbtrans->next) == NULL)
            {
                break;
            }

            ohcitrans = usbtrans->data;
            td = ohcitrans->tdvirt;
        }

        if(usbtrans == NULL)
        {
            break;
        }

        /*
        __asm__ __volatile__("pause":::);
        __asm__ __volatile__("pause":::);
        __asm__ __volatile__("pause":::);
        //scheduler();
        //tick_delay(2);
        */
        set_task_waking_signal(this_core->cur_task, 0);
        __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_SELECT_EVENT);
        block_task_timeout(this_core->cur_task, 1);
    }

    if(!timeout)
    {
        printk("%s: transfer timed out\n", "ohci");
    }

    // check results
    transfer->success = 1;

    for(usbtrans = transfer->trans_head; usbtrans != NULL; usbtrans = usbtrans->next)
    {
        struct ohci_transaction_t *ohcitrans = usbtrans->data;
        transfer->success = (transfer->success && (ohcitrans->tdvirt->sts == 0));

        //printk("ohci_wait_transfer: usbtrans 0x%lx, success %d\n", usbtrans, transfer->success);

        if(transfer->success)
        {
            if(ohcitrans->inbuf && ohcitrans->inlen)
            {
                A_memcpy(ohcitrans->inbuf, ohcitrans->tdbuf, ohcitrans->inlen);
            }
        }
    }

    // mark as inactive
    if(transfer->type == USB_TRANSFER_BULK ||
       transfer->type == USB_TRANSFER_CTRL)
    {
        ed->skip = 1;
    }
}


static int ohci_alloc_tdbuf(struct ohci_dev_t *ohci, struct ohci_td_t *td, size_t len)
{
    if(len > OHCI_TDBUF_SIZE)
    {
        kpanic("ohci: TD buffer length > 1024\n");
    }

    if(len)
    {
        uintptr_t tdphys;

        if(!(td->virtbuf = (void *)alloc_tdbuf(ohci, &tdphys)))
        {
            return -ENOMEM;
        }

        td->curbuf = tdphys;
        td->bufend = td->curbuf + len - 1;
    }
    else
    {
        td->virtbuf = 0;
        td->curbuf = 0;
        td->bufend = 0;
    }

    return 0;
}


static int ohci_td_setup(struct usb_transaction_t *transaction, 
                         struct ohci_transaction_t *ot)
{
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)transaction->transfer;
    struct ohci_dev_t *ohci = (struct ohci_dev_t *)transfer->dev->priv;
    struct ohci_td_t *td;
    volatile struct usb_request_t *req;

    if(!(td = alloc_td(ohci)))
    {
        printk("%s: failed to set up transaction descriptor\n", "ohci");
        return -ENOMEM;
    }

    td->next = (1 << 0);
    td->direction = OHCI_TD_DIRECTION_SETUP;
    td->toggle = transaction->toggle;
    td->togglefromtd = 1;
    td->sts = 15;   // execute
    td->delayint = 7;
    td->errcnt = 0;
    td->smallpkt = 1;

    if(ohci_alloc_tdbuf(ohci, td, sizeof(struct usb_request_t)) < 0)
    {
        printk("%s: failed to alloc TD buffer\n", "ohci");
        free_td(td);
        return -ENOMEM;
    }

    req = td->virtbuf;
    req->type = transaction->type;
    req->req = transaction->req;
    req->hival = transaction->hival;
    req->loval = transaction->loval;
    req->index = transaction->index;
    req->len = transaction->len;

    ot->tdbuf = (void *)td->virtbuf;
    ot->tdvirt = td;

    return 0;
}


static int ohci_td_setup_io(struct usb_transaction_t *transaction, 
                            struct ohci_transaction_t *ot, 
                            uint8_t direction)
{
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)transaction->transfer;
    struct ohci_dev_t *ohci = (struct ohci_dev_t *)transfer->dev->priv;
    struct ohci_td_t *td, *tdcopy = NULL;

    // make a copy of the TD for interrupt transfers
    if(transfer->type == USB_TRANSFER_INTERRUPT)
    {
        if(!(tdcopy = kmalloc(sizeof(struct ohci_td_t))))
        {
            printk("%s: failed to set up transaction descriptor copy\n", "ohci");
            return -ENOMEM;
        }
    }

    if(!(td = alloc_td(ohci)))
    {
        if(tdcopy)
        {
            kfree(tdcopy);
        }

        printk("%s: failed to set up transaction descriptor\n", "ohci");
        return -ENOMEM;
    }

    td->next = (1 << 0);
    td->direction = direction;
    td->toggle = transaction->toggle;
    td->togglefromtd = 1;
    td->sts = 15;   // execute
    td->delayint = 7;
    td->errcnt = 0;
    td->smallpkt = 1;

    if(ohci_alloc_tdbuf(ohci, td, transaction->len) < 0)
    {
        if(tdcopy)
        {
            kfree(tdcopy);
        }

        printk("%s: failed to alloc TD buffer\n", "ohci");
        free_td(td);
        return -ENOMEM;
    }

    if(tdcopy)
    {
        A_memcpy(tdcopy, td, sizeof(struct ohci_td_t));
    }

    ot->tdbuf = (void *)td->virtbuf;
    ot->tdvirt = td;
    ot->tdcopyvirt = tdcopy;

    return 0;
}


#define APPEND_TD(tranfer, ot)                                          \
    if(transfer && transfer->trans_tail) {                              \
        struct ohci_transaction_t *last = transfer->trans_tail->data;   \
        last->tdvirt->next = ((uintptr_t)ot->tdvirt->self_phys);        \
    }


static int ohci_setup_transaction(struct usb_transaction_t *transaction)
{
    struct ohci_transaction_t *ot;
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)transaction->transfer;

    if(!(ot = kmalloc(sizeof(struct ohci_transaction_t))))
    {
        printk("%s: failed to set up transaction\n", "ohci");
        return -ENOMEM;
    }

    A_memset(ot, 0, sizeof(struct ohci_transaction_t));
    transaction->data = ot;
    ot->inbuf = NULL;
    ot->inlen = 0;

    if(ohci_td_setup(transaction, ot) < 0)
    {
        kfree(ot);
        return -ENOMEM;
    }

    APPEND_TD(tranfer, ot);

    return 0;
}


static void ohci_in_transaction(struct usb_transaction_t *transaction)
{
    struct ohci_transaction_t *ot;
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)transaction->transfer;

    if(!(ot = kmalloc(sizeof(struct ohci_transaction_t))))
    {
        printk("%s: failed to set up IN transaction\n", "ohci");
        return;
    }

    A_memset(ot, 0, sizeof(struct ohci_transaction_t));
    transaction->data = ot;
    ot->inbuf = transaction->buf;
    ot->inlen = transaction->len;

    if(ohci_td_setup_io(transaction, ot, OHCI_TD_DIRECTION_IN) < 0)
    {
        kfree(ot);
        return;
    }

    APPEND_TD(tranfer, ot);
}


static void ohci_out_transaction(struct usb_transaction_t *transaction)
{
    struct ohci_transaction_t *ot;
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)transaction->transfer;

    if(!(ot = kmalloc(sizeof(struct ohci_transaction_t))))
    {
        printk("%s: failed to set up OUT transaction\n", "ohci");
        return;
    }

    A_memset(ot, 0, sizeof(struct ohci_transaction_t));
    transaction->data = ot;
    ot->inbuf = NULL;
    ot->inlen = 0;

    if(ohci_td_setup_io(transaction, ot, OHCI_TD_DIRECTION_OUT) < 0)
    {
        kfree(ot);
        return;
    }

    if(transaction->buf && transaction->len)
    {
        A_memcpy(ot->tdbuf, transaction->buf, transaction->len);
    }

    APPEND_TD(tranfer, ot);
}

#undef APPEND_TD


static void ohci_free_usb_transaction_data(volatile struct usb_transaction_t *usbtrans)
{
    struct usb_transfer_t *transfer = (struct usb_transfer_t *)usbtrans->transfer;
    struct ohci_dev_t *ohci = (struct ohci_dev_t *)transfer->dev->priv;
    struct ohci_transaction_t *ot = usbtrans->data;

    if(!ot)
    {
        return;
    }

    if(ot->tdvirt)
    {
        free_td(ot->tdvirt);
    }

    if(ot->tdcopyvirt)
    {
        // NOTE: this is an malloc'd copy
        kfree(ot->tdcopyvirt);
    }

    if(ot->tdbuf)
    {
        free_tdbuf(ohci, (uintptr_t)ot->tdbuf);
    }

    ot->tdvirt = 0;
    ot->tdcopyvirt = 0;
    ot->tdbuf = 0;

    kfree(ot);
    usbtrans->data = NULL;
}


int ohci_install(struct pci_dev_t *pci, struct pci_bar_t *bar)
{
    struct ohci_dev_t *ohci;
    struct ohci_td_t *td;
    char buf[8];
    //uintptr_t phys, virt;
    int res = 0;
    uintptr_t base = bar->base;
    volatile uint32_t dword, fminterval;
    volatile unsigned int i;

    if(!(ohci = kmalloc(sizeof(struct ohci_dev_t))))
    {
        printk("%s: insufficient memory to init device\n", "ochi");
        return -ENOMEM;
    }
    
    A_memset(ohci, 0, sizeof(struct ohci_dev_t));

    // entry 0 in the device address bitmap is always used so that we
    // don't use device address 0 by mistake, as it is reserved
    ohci->addr_bitmap[0] = 1;

    ohci->pci = pci;
    ohci->iosize = bar->iosize;

    // check whether I/O is memory-mapped or normal I/O
    if(bar->iotype == PCI_IOTYPE_MMIO)
    {
        // MMIO
        base &= ~0xf;
        ohci->iobase = mmio_map(base, base + bar->iosize);
        ohci->mmio = 1;
    }
    else
    {
        // I/O
        ohci->iobase = base & ~0x3;
    }

    printk("%s: base " _XPTR_ ", iobase " _XPTR_ ", iosize " _XPTR_ " (%s)\n", 
           "ohci", base, ohci->iobase, ohci->iosize, ohci->mmio ? "MMIO" : "I/O");

    if(!first_ohci)
    {
        first_ohci = ohci;
    }
    else
    {
        volatile struct ohci_dev_t *tmp = first_ohci;

        while(tmp->next)
        {
            tmp = tmp->next;
        }

        tmp->next = ohci;
    }

    // register IRQ handler
    ksprintf(buf, 8, "ohci%d", pci->unit);
    pci_register_irq_handler(pci, ohci_intr, buf);

    pci_enable_busmastering(pci);
    pci_enable_interrupts(pci);
    pci_enable_memoryspace(pci);

    // take control of the host controller
    dword = pcidev_inl(ohci, OHCI_REG_CTRL);

    if(dword & OHCI_CTRL_INTREDIR)
    {
        // driver is active
        dword = pcidev_inl(ohci, OHCI_REG_CMD_STS);
        dword |= OHCI_CMDSTS_OCR;
        pcidev_outl(ohci, OHCI_REG_CMD_STS, dword);

        // wait for ownership change
        for(i = 0; i < 100; i++)
        {
            tick_delay(1);
            dword = pcidev_inl(ohci, OHCI_REG_CTRL);

            if(!(dword & OHCI_CTRL_INTREDIR))
            {
                break;
            }
        }

        if(i < 100)
        {
            printk("ohci: controller ownership changed successfully\n");
        }
        else
        {
            printk("ohci: could not change controller ownership\n");

            // reset the interrupt routing bit
            dword = pcidev_inl(ohci, OHCI_REG_CTRL);
            dword &= ~OHCI_CTRL_INTREDIR;
            pcidev_outl(ohci, OHCI_REG_CTRL, dword);

            for(i = 0; i < 20; i++)
            {
                tick_delay(1);
                dword = pcidev_inl(ohci, OHCI_REG_CTRL);

                if(!(dword & OHCI_CTRL_INTREDIR))
                {
                    break;
                }
            }

            // driver still active
            dword = pcidev_inl(ohci, OHCI_REG_CTRL);

            if(!(dword & OHCI_CTRL_INTREDIR))
            {
                printk("ohci: controller ownership changed successfully\n");
            }
            else
            {
                printk("ohci: could not change controller ownership\n");
            }
        }
    }
    else
    {
        // driver is not active
        if((dword & OHCI_CTRL_HCFS) != OHCI_HC_RESET)
        {
            printk("ohci: active BIOS driver present\n");

            if((dword & OHCI_CTRL_HCFS) != OHCI_HC_OPERATIONAL)
            {
                dword &= ~OHCI_CTRL_HCFS;
                dword |= OHCI_HC_RESUME;
                pcidev_outl(ohci, OHCI_REG_CTRL, dword);

                // should wait 20ms
                tick_delay(3);
            }
        }
        else
        {
            tick_delay(3);
        }
    }

    // now we can reset the controller
    fminterval = pcidev_inl(ohci, OHCI_REG_FRAME_INTRVL);
    dword = pcidev_inl(ohci, OHCI_REG_CMD_STS);
    dword |= OHCI_CMDSTS_RESET;
    pcidev_outl(ohci, OHCI_REG_CMD_STS, dword);
    tick_delay(1);

    // restore the saved frame interval register value
    pcidev_outl(ohci, OHCI_REG_FRAME_INTRVL, fminterval);

    // toggle the frame interval time
    dword = pcidev_inl(ohci, OHCI_REG_FRAME_INTRVL);
    dword ^= (1 << 31);
    pcidev_outl(ohci, OHCI_REG_FRAME_INTRVL, dword);

    // resume the host controller
    dword = pcidev_inl(ohci, OHCI_REG_CTRL);

    if((dword & OHCI_CTRL_HCFS) == OHCI_HC_SUSPEND)
    {
        dword &= ~OHCI_CTRL_HCFS;
        dword |= OHCI_HC_RESUME;
        pcidev_outl(ohci, OHCI_REG_CTRL, dword);

        // should wait 20ms
        tick_delay(3);
    }

#define ALLOC_MEMPAGE(s, p, v) \
   	if(!(p = (uintptr_t)pmmngr_alloc_block())) { \
        res = -ENOMEM; \
        goto err; \
    } \
    v = (s *)mmio_map(p, p + PAGE_SIZE); \
    A_memset(v, 0, PAGE_SIZE);

    // alloc memory for transfer descriptor pool (must be 256-byte aligned)
    // the upper half of the page will be used for the periodic list descriptors
    ALLOC_MEMPAGE(struct ohci_hcca_t, ohci->hcca_phys, ohci->hcca);
    printk("%s: hcca virt " _XPTR_ ", phys " _XPTR_ "\n", 
                "ohci", ohci->hcca, ohci->hcca_phys);

    // alloc memory for transfer descriptor pool
    ALLOC_MEMPAGE(struct ohci_td_t, ohci->tdpool_phys, ohci->tdpool);
    printk("%s: tdpool virt " _XPTR_ ", phys " _XPTR_ "\n", 
                "ohci", ohci->tdpool, ohci->tdpool_phys);

    // alloc memory for endpoint descriptor pool
    ALLOC_MEMPAGE(struct ohci_ed_t, ohci->edpool_phys, ohci->edpool);
    printk("%s: edpool virt " _XPTR_ ", phys " _XPTR_ "\n", 
                "ohci", ohci->edpool, ohci->edpool_phys);

    // alloc memory for transfer descriptor buffer pool
    if(!(ohci->tdbufpool_phys = 
            (uintptr_t)pmmngr_alloc_blocks(OHCI_TDBUF_POOL_SIZE / PAGE_SIZE)))
    {
        res = -ENOMEM;
        goto err;
    }

    ohci->tdbufpool = mmio_map(ohci->tdbufpool_phys, 
                               ohci->tdbufpool_phys + OHCI_TDBUF_POOL_SIZE);

    printk("%s: tdbufpool virt " _XPTR_ ", phys " _XPTR_ "\n", 
                "ohci", ohci->tdbufpool, ohci->tdbufpool_phys);

    init_tdpool(ohci);
    init_edpool(ohci);

#undef ALLOC_MEMPAGE

    // now set the host controller up

#define ED_INT_HEAD_VIRT(o, i)  \
    (((uintptr_t)(o)->hcca) + 2048 + ((i) * sizeof(struct ohci_ed_t)))

#define ED_INT_HEAD_PHYS(o, i)  \
    (((uintptr_t)(o)->hcca_phys) + 2048 + ((i) * sizeof(struct ohci_ed_t)))

#define ED_INT_HEAD_TDVIRT(o, i)\
    (((uintptr_t)(o)->hcca) + 2048 + 1024 + ((i) * sizeof(struct ohci_td_t)))

#define ED_INT_HEAD_TDPHYS(o, i)\
    (((uintptr_t)(o)->hcca_phys) + 2048 + 1024 + ((i) * sizeof(struct ohci_td_t)))

    // init the periodic list (interrupt & isochronous transfers)
    // use the upper half of the hcca allocated page
    for(i = 0; i < OHCI_MAX_INT_INDEX; i++)
    {
        ohci->ed_int_head[i] = (struct ohci_ed_t *)ED_INT_HEAD_VIRT(ohci, i);
        ohci->ed_int_head[i]->skip = 1;
        ohci->ed_int_head[i]->td_qhead = ED_INT_HEAD_TDPHYS(ohci, i);
        ohci->ed_int_head[i]->td_qtail = ED_INT_HEAD_TDPHYS(ohci, i);

        // link higher frequencies to the 1ms frequency
        if(i > 0)
        {
            ohci->ed_int_head[i]->next = ED_INT_HEAD_PHYS(ohci, 0);
        }
    }

    // init interrupt heads for frequencies 1, 2, 4, 8, 16 and 32ms
    static int freq_index[32] =
    {
        5, 4, 3, 2, 1, 0, 1, 2, 1, 0, 3, 2, 1, 0, 1, 2,
        1, 4, 3, 2, 1, 0, 1, 2, 1, 0, 3, 2, 1, 0, 1, 2,
    };

    for(i = 0; i < 32; i++)
    {
        ohci->hcca->ed_int_head[i] = ED_INT_HEAD_PHYS(ohci, freq_index[i]);
    }

    // init the non-periodic block transfer list
    td = alloc_td(ohci);
    td->next = (1 << 0);
    ohci->ed_bulk_head = alloc_ed(ohci);
    ohci->ed_bulk_head->skip = 1;
    ohci->ed_bulk_head->td_qhead = td->self_phys;
    ohci->ed_bulk_head->td_qtail = ohci->ed_bulk_head->td_qhead;
    ohci->ed_bulk_head->next = 0;     // end of list

    // init the non-periodic control transfer list
    td = alloc_td(ohci);
    td->next = (1 << 0);
    ohci->ed_ctrl_head = alloc_ed(ohci);
    ohci->ed_ctrl_head->skip = 1;
    ohci->ed_ctrl_head->td_qhead = td->self_phys;
    ohci->ed_ctrl_head->td_qtail = ohci->ed_ctrl_head->td_qhead;
    ohci->ed_ctrl_head->next = 0;     // end of list

    // set the list head registers
    pcidev_outl(ohci, OHCI_REG_CTRL_HEAD_ED, ohci->ed_ctrl_head->self_phys);
    pcidev_outl(ohci, OHCI_REG_CTRL_CUR_ED , ohci->ed_ctrl_head->self_phys);
    pcidev_outl(ohci, OHCI_REG_BULK_HEAD_ED, ohci->ed_bulk_head->self_phys);
    pcidev_outl(ohci, OHCI_REG_BULK_CUR_ED , ohci->ed_bulk_head->self_phys);

    // set the hcca register
    pcidev_outl(ohci, OHCI_REG_HCCA, ohci->hcca_phys);

    // enable all interrupts except SOF detect
    pcidev_outl(ohci, OHCI_REG_INT_DIS, OHCI_INTSTS_SOF | OHCI_INTSTS_MAST_INT_EN);
    pcidev_outl(ohci, OHCI_REG_INT_STS, 0xffffffff);
    pcidev_outl(ohci, OHCI_REG_INT_EN, OHCI_INTSTS_SCHED_OVRRN |
                                       OHCI_INTSTS_WR_DONE |
                                       OHCI_INTSTS_RESUME_DET |
                                       OHCI_INTSTS_ERR |
                                       OHCI_INTSTS_FRAME_OVRFL |
                                       OHCI_INTSTS_RHSC |
                                       OHCI_INTSTS_OWN_CHG |
                                       OHCI_INTSTS_MAST_INT_EN);

    // enable transfers
    dword = pcidev_inl(ohci, OHCI_REG_CTRL);
    dword |= OHCI_CTRL_PERIOD_EN | OHCI_CTRL_CTRL_EN | OHCI_CTRL_BULK_EN /* | OHCI_CTRL_ISOC_EN */;
    pcidev_outl(ohci, OHCI_REG_CTRL, dword);

    // enable remote wakeup
    dword = pcidev_inl(ohci, OHCI_REG_CTRL);
    dword |= OHCI_CTRL_REMWAKEUP_EN;
    pcidev_outl(ohci, OHCI_REG_CTRL, dword);

    // set the value at which the host controller switches to periodic list
    // processing.  we set this at 90% of the frame interval
    dword = pcidev_inl(ohci, OHCI_REG_FRAME_INTRVL);
    dword &= 0x3fff;
    dword *= 90/100;
    pcidev_outl(ohci, OHCI_REG_PERIOD_START, dword);

    // set the host controller status to operational
    dword = pcidev_inl(ohci, OHCI_REG_CTRL);
    dword &= ~OHCI_CTRL_HCFS;
    dword |= OHCI_HC_OPERATIONAL;
    pcidev_outl(ohci, OHCI_REG_CTRL, dword);

    // configure ports
    dword = pcidev_inl(ohci, OHCI_REG_ROOTHUB_DESCA);
    dword &= ~(OHCI_RHA_DEV_TYPE | OHCI_RHA_PORT_MASK);
    dword |= OHCI_RHA_NO_PWR_SWITCH;
    pcidev_outl(ohci, OHCI_REG_ROOTHUB_DESCA, dword);
    pcidev_outl(ohci, OHCI_REG_ROOTHUB_DESCB, 0);

    // turn power on to all ports
    pcidev_outl(ohci, OHCI_REG_ROOTHUB_STS, OHCI_RHS_PWRSTAT_CHG);
    tick_delay(2);

    dword = pcidev_inl(ohci, OHCI_REG_ROOTHUB_DESCA);
    ohci->port_count = BYTE1(dword);

    printk("%s: root ports %u\n", "ohci", ohci->port_count);
    printk("%s: controller running, enabling ports\n", "ohci");

    if((res = ohci_enable_ports(ohci)) < 0)
    {
        goto err;
    }

    ohci->flags |= OHCI_FLAG_RUN;

    printk("%s: setup done\n", "ohci");

    return 0;


err:

    if(ohci->mmio)
    {
        vmmngr_free_pages(ohci->iobase, ohci->iosize);
        ohci->iobase = 0;
    }

    if(ohci->hcca_phys)
    {
        vmmngr_free_pages((virtual_addr)ohci->hcca, PAGE_SIZE);
        ohci->hcca_phys = 0;
        ohci->hcca = 0;
    }

    if(ohci->tdpool_phys)
    {
        vmmngr_free_pages((virtual_addr)ohci->tdpool, PAGE_SIZE);
        ohci->tdpool_phys = 0;
        ohci->tdpool = 0;
    }

    if(ohci->edpool_phys)
    {
        vmmngr_free_pages((virtual_addr)ohci->edpool, PAGE_SIZE);
        ohci->edpool_phys = 0;
        ohci->edpool = 0;
    }

    if(ohci->tdbufpool_phys)
    {
        vmmngr_free_pages(ohci->tdbufpool, OHCI_TDBUF_POOL_SIZE);
        ohci->tdbufpool = 0;
    }

    return res;
}


/*
 * IRQ callback function.
 */
int ohci_intr(struct regs *r, void *arg)
{
    UNUSED(r);

    //printk("ohci_intr:\n");
    //screen_refresh(NULL);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    struct pci_dev_t *pci = arg;
    volatile struct ohci_dev_t *ohci = first_ohci;
    volatile uint32_t dword, dword2;

    while(ohci)
    {
        if(ohci->pci == pci)
        {
            break;
        }

        ohci = ohci->next;
    }

    if(!ohci)
    {
        printk("ohci_intr: not from here\n");
        return 0;
    }

    dword = pcidev_inl(ohci, OHCI_REG_INT_STS);

    if(!dword)
    {
        // IRQ not from this device
        //printk("ohci_intr: dword 0\n");
        return 0;
    }

    //printk("ohci_intr: dword 0x%x\n", dword);

    // unrecoverable error
    if(dword & OHCI_INTSTS_ERR)
    {
        dword2 = pcidev_inl(ohci, OHCI_REG_CMD_STS);
        dword2 |= OHCI_CMDSTS_RESET;
        pcidev_outl(ohci, OHCI_REG_CMD_STS, dword2);
    }

    // root hub status change
    if((dword & OHCI_INTSTS_RHSC) && (ohci->flags & OHCI_FLAG_PORTENABLED))
    {
        volatile unsigned int i;

        for(i = 0; i < ohci->port_count; i++)
        {
            dword2 = pcidev_inl(ohci, OHCI_REG_ROOTHUB_PRSTS + (i * 4));

            if(dword2 & OHCI_RHP_CONN_STS_CHG)
            {
                check_port_status(ohci, i);
            }
        }
    }

    // acknowledge interrupt
    pcidev_outl(ohci, OHCI_REG_INT_STS, dword);

    pic_send_eoi(ohci->pci->irq[0]);

    return 1;
}


static struct usb_ops_t ohci_ops =
{
    .setup_transfer = ohci_setup_transfer,
    .schedule_transfer = ohci_schedule_transfer,
    .wait_transfer = ohci_wait_transfer,
    .poll_transfer = ohci_poll_transfer,
    .delete_transfer = ohci_delete_transfer,
    .setup_transaction = ohci_setup_transaction,
    .in_transaction = ohci_in_transaction,
    .out_transaction = ohci_out_transaction,
    .free_transaction_data = ohci_free_usb_transaction_data,
    .get_next_addr = ohci_get_next_addr,
    .free_addr = ohci_free_addr,
};

