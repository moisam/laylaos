/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb.c
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
 *  \file usb.c
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

#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/dev.h>
#include <kernel/pci.h>
#include <kernel/usb.h>
#include <kernel/usb_uhci.h>
#include <kernel/usb_ohci.h>
#include <kernel/usb_ehci.h>
#include <kernel/usb_hid.h>
#include <kernel/usb_hub.h>
#include <mm/kheap.h>


static volatile int last_bus = 0;
struct pci_dev_t *usbbus[MAX_USB_BUSES + 1];

static volatile struct task_t *usb_task = NULL;
static struct usb_transfer_t inttransfer_head;
static struct kernel_mutex_t inttransfer_list_lock;


static void print_device(struct usb_dev_t *dev)
{
    printk("usb: USB v%x.%x ", BYTE2(dev->spec), BYTE1(dev->spec));

    if(dev->spec != 0x0100 && dev->spec != 0x0110 &&
       dev->spec != 0x0200 && dev->spec != 0x0300)
    {
        printk("(INVALID) ");
    }

    if(dev->class == 0x09)
    {
        if(dev->protocol == 0)
        {
            printk("- full speed USB hub");
        }
        else if(dev->protocol == 1)
        {
            printk("- Hi-speed USB hub with single TT");
        }
        else if(dev->protocol == 2)
        {
            printk("- Hi-speed USB hub with multiple TTs");
        }
    }

    printk("\n     endpoint 0:    mps %u bytes\n", dev->endpoints->mps);
    printk(  "     class:         0x%x\n", dev->class);
    printk(  "     subclass:      0x%x\n", dev->subclass);
    printk(  "     vendor:        0x%x\n", dev->vendor);
    printk(  "     product:       0x%x\n", dev->product);
    printk(  "     release:       %u.%u\n", BYTE2(dev->release), BYTE1(dev->release));
    printk(  "     manufacturer:  0x%x\n", dev->manufacturerid);
    printk(  "     productid:     0x%x\n", dev->productid);
    printk(  "     serial:        0x%x\n", dev->serialid);
    printk(  "     configs:       %u\n", dev->configs);
}


#ifdef __DEBUG

static void print_config_descriptor(struct usb_config_descriptor_t *desc)
{
    if(desc->len)
    {
        printk("usb: config descriptor:\n");
        printk("     len:             %u\n", desc->len);
        printk("     desc type:       %u\n", desc->type);
        printk("     total len:       %u\n", desc->totlen);
        printk("     interfaces:      %u\n", desc->interfaces);
        printk("     config id:       0x%x\n", desc->configval);
        printk("     config name id:  0x%x\n", desc->config);
        printk("     attribs:         0x%x\n", desc->attribs);
        printk("     max power (mA):  %u\n", desc->maxpower);
    }
}


static void print_endpoint_descriptor(struct usb_endpoint_descriptor_t *desc)
{
    if(desc->len)
    {
        printk("usb: endpoint descriptor:\n");
        printk("     len:             %u\n", desc->len);
        printk("     desc type:       %u\n", desc->type);
        printk("     endpoint %u:     %s\n", 
                (desc->addr & 0xF),
                (desc->addr & 0x80) ? "IN" : "OUT");
        printk("     attribs:         0x%x%s\n", 
                desc->attribs,
                (desc->attribs == 2) ? "(bulk data)" : " ");
        printk("     mps:             %u bytes\n", desc->mps);
        printk("     interval:        %u\n", desc->interval);
    }
}


static void print_interface_descriptor(struct usb_interface_descriptor_t *desc)
{
    if(desc->len)
    {
        printk("usb: interface descriptor:\n");
        printk("     len:             %u\n", desc->len);
        printk("     desc type:       %u\n", desc->type);
        printk("     interface num:   %u\n", desc->interfacenum);
        printk("     endpoints:       %u\n", desc->endpoints);
        printk("     altsetting:      %u\n", desc->altsetting);
        printk("     class:           0x%x\n", desc->class);
        printk("     subclass:        0x%x\n", desc->subclass);
        printk("     protocol:        0x%x\n", desc->protocol);
        printk("     interface:       0x%x\n", desc->interface);
    }
}


static void print_hid_descriptor(struct usb_hid_descriptor_t *desc)
{
    int i;
    uint16_t desclen;
    uint8_t *descriptors;

    if(desc->len)
    {
        printk("usb: Human Interface Device (HID) descriptor:\n");
        printk("     len:               %u\n", desc->len);
        printk("     desc type:         %u\n", desc->type);
        printk("     class spec:        %u.%u\n", BYTE2(desc->hid_bcd), BYTE1(desc->hid_bcd));
        printk("     country code:      %u\n", desc->country_code);
        printk("     descriptor count:  %u\n", desc->descriptor_count);

        descriptors = (uint8_t *)desc->descriptors;

        for(i = 0; i < desc->descriptor_count; i++)
        {
            desclen = descriptors[1] | (descriptors[2] << 8);
            printk("       [%d] type 0x%x, len %u\n", i, descriptors[0], desclen);
            descriptors += 3;
        }
    }
}


static void print_string_descriptor(struct usb_string_descriptor_t *desc)
{
    int i;

    if(desc->len)
    {
        printk("usb: string descriptor:\n");
        printk("     len:             %u\n", desc->len);
        printk("     desc type:       %u\n", desc->type);
        printk("     languages:       ");

        for(i = 0; i < 10; i++)
        {
            if(desc->langid[i] >= 0x0400 && desc->langid[i] <= 0x0465)
            {
                printk("0x%x ", desc->langid[i]);
            }
        }

        printk("\n");
    }
}


static void print_unistring_descriptor(struct usb_dev_t *dev,
                                       struct usb_unistring_descriptor_t *desc,
                                       uint32_t strindex)
{
    int i;
    char ascii[32] = { 0, };

    if(desc->len)
    {
        printk("usb: Unicode string descriptor:\n");
        printk("     len:             %u\n", desc->len);
        printk("     desc type:       %u\n", desc->type);

        for(i = 0; i < MIN(64, (desc->len - 2)); i += 2)
        {
            if(desc->wch[i])
            {
                ascii[i / 2] = desc->wch[i];
            }
        }

        ascii[31] = '\0';

        if(strindex == 2)
        {
            A_memcpy(dev->product_name, ascii, 32);
            printk("     product name:    %s\n", dev->product_name);
        }
        else if(strindex == 3)
        {
            A_memcpy(dev->serial, ascii, 32);
            printk("     serial:          %s\n", dev->serial);
        }
        else
        {
            printk("     strindex:        %u\n", strindex);
        }
    }
}

#endif      /* __DEBUG */


struct usb_dev_t *usb_create_dev(uint8_t bus, unsigned int port, uint8_t speed)
{
    struct usb_dev_t *dev;

    if(!(dev = kmalloc(sizeof(struct usb_dev_t))))
    {
        return NULL;
    }

    A_memset(dev, 0, sizeof(struct usb_dev_t));

    dev->speed = speed;
    dev->port = port;
    dev->bus = bus;

    // alloc the first endpoint
    if(!(dev->endpoints = kmalloc(sizeof(struct usb_endpoint_t))))
    {
        kfree(dev);
        return NULL;
    }

    A_memset(dev->endpoints, 0, sizeof(struct usb_endpoint_t));

    if(speed == USB_SPEED_LOW || speed == USB_SPEED_FULL)
    {
        dev->endpoints->mps = 8;
    }
    else if(speed == USB_SPEED_HIGH)
    {
        dev->endpoints->mps = 64;
    }
    else if(speed == USB_SPEED_SUPER)
    {
        dev->endpoints->mps = 512;
    }

    dev->endpoints->direction = USB_ENDPOINT_BI;
    dev->endpoints->type = USB_ENDPOINT_CONTROL;

    return dev;
}


void usb_destroy_dev(struct usb_dev_t *dev)
{
    volatile struct usb_interface_t *iface, *niface;
    volatile struct usb_endpoint_t *endpoint, *nendpoint;

    if(!dev)
    {
        return;
    }

    // remove USB hub
    if(dev->class == 0x09)
    {
        usb_hub_remove_dev(dev);
    }

    // remove the interfaces
    for(iface = dev->interfaces; iface != NULL; iface = iface->next)
    {
        // if this is a mass storage device, remove it from the device list and
        // delete it from /dev tree
        if(iface->desc.class == 0x08 && iface->desc.subclass == 0x06)
        {
            usb_msd_remove((struct usb_interface_t *)iface);
        }
        // remove HID
        else if(iface->desc.class == 0x03)
        {
            usb_hid_remove((struct usb_interface_t *)iface);
        }
    }

    // free interfaces
    for(iface = dev->interfaces; iface != NULL; )
    {
        niface = iface->next;
        kfree((void *)iface);
        iface = niface;
    }

    // remove endpoint devices from /dev
    for(endpoint = dev->endpoints; endpoint != NULL; endpoint = endpoint->next)
    {
        remove_dev_node(USB_MAKE_DEVID(dev->bus, dev->num, endpoint->addr));
    }

    // free endpoints
    for(endpoint = dev->endpoints; endpoint != NULL; )
    {
        nendpoint = endpoint->next;
        kfree((void *)endpoint);
        endpoint = nendpoint;
    }

    dev->interfaces = NULL;
    dev->endpoints = NULL;

    kfree(dev);
}


void remove_interrupt_transfer(struct usb_transfer_t *transfer)
{
    volatile struct usb_transfer_t *t, *prev = &inttransfer_head;

    kernel_mutex_lock(&inttransfer_list_lock);

    for(t = inttransfer_head.next_inttransfer; t != NULL; t = t->next_inttransfer)
    {
        if(t == transfer)
        {
            prev->next_inttransfer = t->next_inttransfer;
            break;
        }

        prev = t;
    }

    kernel_mutex_unlock(&inttransfer_list_lock);
}


void usb_schedule_inttransfer(struct usb_dev_t *usb, struct usb_endpoint_t *endpoint,
                              struct usb_transfer_t *transfer, 
                              void *buf, size_t bufsz,
                              void (*callback)(void *), void *callback_arg,
                              uint8_t freq)
{
    usb_setup_transfer(usb, endpoint, transfer, USB_TRANSFER_INTERRUPT);

    transfer->callback = callback;
    transfer->callback_arg = callback_arg;
    transfer->freq = freq;

    usb_in_transaction(transfer, 0, buf, bufsz);
    usb_schedule_transfer(transfer);

    kernel_mutex_lock(&inttransfer_list_lock);
    transfer->next_inttransfer = inttransfer_head.next_inttransfer;
    inttransfer_head.next_inttransfer = transfer;
    kernel_mutex_unlock(&inttransfer_list_lock);
}


void usb_setup_transfer(struct usb_dev_t *dev, struct usb_endpoint_t *endpoint,
                        struct usb_transfer_t *transfer, uint8_t type)
{
    A_memset(transfer, 0, sizeof(struct usb_transfer_t));
    transfer->dev = dev;
    transfer->endpoint = endpoint;
    transfer->type = type;
    transfer->pktsz = endpoint->mps;

    if(!dev->ops || !dev->ops->setup_transfer)
    {
        printk("usb: device with NULL setup_transfer() function\n");
    }
    else
    {
        dev->ops->setup_transfer(transfer);
    }
}


void usb_schedule_transfer(struct usb_transfer_t *transfer)
{
    if(!transfer->dev || !transfer->dev->ops || !transfer->dev->ops->schedule_transfer)
    {
        printk("usb: device with NULL schedule_transfer() function\n");
    }
    else
    {
        transfer->dev->ops->schedule_transfer(transfer);
    }
}


int usb_poll_transfer(struct usb_transfer_t *transfer)
{
    if(!transfer->dev || !transfer->dev->ops || !transfer->dev->ops->poll_transfer)
    {
        printk("usb: device with NULL poll_transfer() function\n");
        return 0;
    }
    else
    {
        return transfer->dev->ops->poll_transfer(transfer);
    }
}


void usb_wait_transfer(struct usb_transfer_t *transfer)
{
    if(!transfer->dev || !transfer->dev->ops || !transfer->dev->ops->wait_transfer)
    {
        printk("usb: device with NULL wait_transfer() function\n");
    }
    else
    {
        transfer->dev->ops->wait_transfer(transfer);
    }
}


void usb_delete_transfer(struct usb_transfer_t *transfer)
{
    struct usb_dev_t *dev = transfer->dev;
    volatile struct usb_transaction_t *trans, *next;

    if(!dev || !dev->ops || !dev->ops->delete_transfer)
    {
        printk("usb: device with NULL delete_transfer() function\n");
    }
    else
    {
        dev->ops->delete_transfer(transfer);
    }

    if(transfer->type == USB_TRANSFER_INTERRUPT)
    {
        remove_interrupt_transfer(transfer);
    }

    for(trans = transfer->trans_head; trans != NULL; )
    {
        next = trans->next;

        if(dev && trans->data)
        {
            dev->ops->free_transaction_data(trans);
            trans->data = NULL;
        }

        kfree((void *)trans);
        trans = next;
    }

    transfer->trans_head = NULL;
    transfer->trans_tail = NULL;
}


#define APPEND_TRANSACTION(transfer, transaction)   \
    if(transfer->trans_head == NULL) {              \
        transfer->trans_head = transaction;         \
        transfer->trans_tail = transaction;         \
    } else {                                        \
        transfer->trans_tail->next = transaction;   \
        transfer->trans_tail = transaction;         \
    }


uint8_t usb_setup_transaction(struct usb_transfer_t *transfer, 
                              uint8_t type, uint8_t req,
                              uint8_t hival, uint8_t loval,
                              uint16_t index, uint16_t len)
{
    struct usb_transaction_t *transaction;
    uint8_t res = 0;

    if(!(transaction = kmalloc(sizeof(struct usb_transaction_t))))
    {
        return 0;
    }

    A_memset(transaction, 0, sizeof(struct usb_transaction_t));

    transaction->dev = transfer->dev;
    transaction->type = USB_TRANS_SETUP;
    transaction->transfer = transfer;
    transaction->type = type;
    transaction->req = req;
    transaction->hival = hival;
    transaction->loval = loval;
    transaction->index = index;
    transaction->len = len;
    transaction->toggle = 0;

    if(!transfer->dev || !transfer->dev->ops || !transfer->dev->ops->setup_transaction)
    {
        printk("usb: device with NULL setup_transaction() function\n");
    }
    else
    {
        if(transfer->dev->ops->setup_transaction(transaction) == 0)
        {
            res = loval;
        }

        APPEND_TRANSACTION(transfer, transaction);
        transfer->endpoint->toggle = 1;
    }

    KDEBUG("usb_setup_transaction: res %d\n", res);

    return res;
}


void usb_in_transaction(struct usb_transfer_t *transfer, 
                        int ctrl_handshake, void *buf, size_t len)
{
    struct usb_transaction_t *transaction;
    size_t minlen;
    int remaining;

    if(transfer->type == USB_TRANSFER_ISOCHRONOUS)
    {
        minlen = len;
        len = 0;
        remaining = 0;
    }
    else
    {
        minlen = MIN(transfer->pktsz, len);
        len -= minlen;
        remaining = len / transfer->pktsz;

        if(len % transfer->pktsz)
        {
            remaining++;
        }
    }

    if(!(transaction = kmalloc(sizeof(struct usb_transaction_t))))
    {
        return;
    }

    A_memset(transaction, 0, sizeof(struct usb_transaction_t));

    transaction->dev = transfer->dev;
    transaction->type = USB_TRANS_IN;
    transaction->transfer = transfer;
    transaction->buf = buf;
    transaction->len = minlen;

    if(ctrl_handshake)
    {
        transfer->endpoint->toggle = 1;
    }

    transaction->toggle = transfer->endpoint->toggle;

    if(!transfer->dev || !transfer->dev->ops || !transfer->dev->ops->in_transaction)
    {
        printk("usb: device with NULL in_transaction() function\n");
    }
    else
    {
        transfer->dev->ops->in_transaction(transaction);
        APPEND_TRANSACTION(transfer, transaction);
        transfer->endpoint->toggle = !(transfer->endpoint->toggle);

        if(remaining)
        {
            //printk("usb_in_transaction: remaining %d, pktsz %d, len %ld, minlen %ld\n", remaining, transfer->pktsz, len, minlen);
            usb_in_transaction(transfer, transfer->endpoint->toggle,
                                            (char *)buf + minlen, len);
        }
    }
}


void usb_out_transaction(struct usb_transfer_t *transfer, 
                         int ctrl_handshake, void *buf, size_t len)
{
    struct usb_dev_t *dev = transfer->dev;
    struct usb_transaction_t *transaction;
    size_t minlen = MIN(transfer->pktsz, len);
    int remaining;

    len -= minlen;
    remaining = len / transfer->pktsz;

    if(len % transfer->pktsz)
    {
        remaining++;
    }

    if(!(transaction = kmalloc(sizeof(struct usb_transaction_t))))
    {
        return;
    }

    A_memset(transaction, 0, sizeof(struct usb_transaction_t));

    transaction->dev = transfer->dev;
    transaction->type = USB_TRANS_OUT;
    transaction->transfer = transfer;
    transaction->buf = buf;
    transaction->len = minlen;

    if(ctrl_handshake)
    {
        transfer->endpoint->toggle = 1;
    }

    transaction->toggle = transfer->endpoint->toggle;

    if(!dev || !dev->ops || !dev->ops->out_transaction)
    {
        printk("usb: device with NULL out_transaction() function\n");
    }
    else
    {
        dev->ops->out_transaction(transaction);
        APPEND_TRANSACTION(transfer, transaction);
        transfer->endpoint->toggle = !(transfer->endpoint->toggle);

        if(remaining)
        {
            //printk("usb_out_transaction: remaining %d\n", remaining);
            usb_out_transaction(transfer, transfer->endpoint->toggle,
                                            (char *)buf + minlen, len);
        }
    }
}

#undef APPEND_TRANSACTION


int usb_ctrl_in(struct usb_dev_t *dev, void *buf,
                uint8_t type, uint8_t req,
                uint8_t hival, uint8_t loval,
                uint16_t index, uint16_t len)
{
    struct usb_transfer_t transfer;

    usb_setup_transfer(dev, dev->endpoints, &transfer, USB_TRANSFER_CTRL);
    usb_setup_transaction(&transfer, type, req, hival, loval, index, len);
    usb_in_transaction(&transfer, 0, buf, len);
    usb_out_transaction(&transfer, 1, 0, 0);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    return transfer.success;
}


int usb_ctrl_out(struct usb_dev_t *dev, void *buf,
                 uint8_t type, uint8_t req,
                 uint8_t hival, uint8_t loval,
                 uint16_t index, uint16_t len)
{
    struct usb_transfer_t transfer;

    usb_setup_transfer(dev, dev->endpoints, &transfer, USB_TRANSFER_CTRL);
    usb_setup_transaction(&transfer, type, req, hival, loval, index, len);
    usb_out_transaction(&transfer, 0, buf, len);
    usb_in_transaction(&transfer, 1, 0, 0);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    return transfer.success;
}


int usb_ctrl_set(struct usb_dev_t *dev,
                 uint8_t type, uint8_t req,
                 uint8_t hival, uint8_t loval, uint16_t index)
{
    struct usb_transfer_t transfer;

    usb_setup_transfer(dev, dev->endpoints, &transfer, USB_TRANSFER_CTRL);
    usb_setup_transaction(&transfer, type, req, hival, loval, index, 0);
    usb_in_transaction(&transfer, 1, 0, 0);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    return transfer.success;
}


int usb_get_device_descriptor(struct usb_dev_t *dev, uint8_t len)
{
    struct usb_device_descriptor_t desc;
    int res = -EIO;

    if(usb_ctrl_in(dev, &desc, 0x80, 6, 1, 0, 0, len))
    {
        dev->spec = desc.bcd_usb;
        dev->class = desc.class;
        dev->subclass = desc.subclass;
        dev->protocol = desc.protocol;

        if(dev->endpoints->mps != desc.mps)
        {
            dev->endpoints->mps = desc.mps;
            // XXX: update endpoint info for XHCI devices
        }

        if(len > 8)
        {
            dev->vendor = desc.vendorid;
            dev->product = desc.productid;
            dev->release = desc.bcd_dev;
            dev->manufacturerid = desc.manufacturer;
            dev->productid = desc.product;
            dev->serialid = desc.serial;
            dev->configs = desc.configs;
        }

        print_device(dev);
        res = 0;
    }

    return res;
}


int usb_get_config_descriptor(struct usb_dev_t *dev)
{
    struct usb_config_descriptor_t desc;

    if(!usb_ctrl_in(dev, &desc, 0x80, 6, 2, 0, 0, sizeof(struct usb_config_descriptor_t)))
    {
        return -EIO;
    }

    uint16_t bufsz = desc.totlen;
    char buf[bufsz];

    if(!usb_ctrl_in(dev, buf, 0x80, 6, 2, 0, 0, bufsz))
    {
        return -EIO;
    }

    uint16_t confsz = MIN(bufsz, *(uint16_t *)(buf + 2));
    uintptr_t addr = (uintptr_t)buf;
    uintptr_t laddr = addr + confsz;

    // read the descriptors
    while(addr < laddr)
    {
        uint8_t type = *(uint8_t *)(addr + 1);
        uint8_t len = *(uint8_t *)(addr);

        if(len == 9 && type == 2)           // config descriptor
        {
#ifdef __DEBUG
            struct usb_config_descriptor_t *desc = 
                            (struct usb_config_descriptor_t *)addr;

            print_config_descriptor(desc);
#endif
        }
        else if(len == 9 && type == 4)      // interface descriptor
        {
            struct usb_interface_t *iface, *tmp;
            struct usb_interface_descriptor_t *desc = 
                            (struct usb_interface_descriptor_t *)addr;

#ifdef __DEBUG
            print_interface_descriptor(desc);
#endif

            if(!(iface = kmalloc(sizeof(struct usb_interface_t))))
            {
                printk("usb: failed to alloc interface struct\n");
            }
            else
            {
                A_memcpy(&iface->desc, desc, sizeof(struct usb_interface_descriptor_t));
                iface->bytes_per_sector = 512;      // for MSDs
                iface->usb = dev;
                //iface->data = NULL;
                iface->next = NULL;

                if(dev->interfaces == NULL)
                {
                    dev->interfaces = iface;
                }
                else
                {
                    for(tmp = dev->interfaces; tmp->next != NULL; tmp = tmp->next)
                    {
                        ;
                    }

                    tmp->next = iface;
                }
            }
        }
        else if(len == 7 && type == 5)  // endpoint descriptor
        {
            struct usb_endpoint_t *endpoint, *tmp;
            struct usb_endpoint_descriptor_t *desc = 
                            (struct usb_endpoint_descriptor_t *)addr;

#ifdef __DEBUG
            print_endpoint_descriptor(desc);
#endif

            if(!(endpoint = kmalloc(sizeof(struct usb_endpoint_t))))
            {
                printk("usb: failed to alloc endpoint struct\n");
            }
            else
            {
                endpoint->addr = (desc->addr & 0xF);
                endpoint->mps = desc->mps;
                endpoint->interval = desc->interval;
                endpoint->toggle = 0;
                endpoint->type = (desc->attribs & 0x03);
                endpoint->direction = (desc->addr & 0x80) ? 
                                        USB_ENDPOINT_IN : USB_ENDPOINT_OUT;
                endpoint->next = NULL;

                if(dev->endpoints == NULL)
                {
                    dev->endpoints = endpoint;
                }
                else
                {
                    for(tmp = dev->endpoints; tmp->next != NULL; tmp = tmp->next)
                    {
                        ;
                    }

                    tmp->next = endpoint;
                }
            }
        }
        else if(type == 33)
        {
#ifdef __DEBUG
            struct usb_hid_descriptor_t *desc =
                            (struct usb_hid_descriptor_t *)addr;

            print_hid_descriptor(desc);
#endif
        }
        else
        {
            printk("usb: unknown descriptor: type %u, len %u\n", type, len);
        }

        addr += len;
    }

    // XXX: update endpoint info for XHCI devices

    return 0;
}


#ifdef __DEBUG

int usb_get_string_descriptor(struct usb_dev_t *dev)
{
    struct usb_string_descriptor_t desc;

    if(!usb_ctrl_in(dev, &desc, 0x80, 6, 3, 0, 0, sizeof(struct usb_string_descriptor_t)))
    {
        return -EIO;
    }

    print_string_descriptor(&desc);

    return 0;
}


int usb_get_unistring_descriptor(struct usb_dev_t *dev, uint32_t strindex)
{
    char buf[64];

    if(!usb_ctrl_in(dev, buf, 0x80, 6, 3, strindex, 0x0409, sizeof(buf)))
    {
        return -EIO;
    }

    print_unistring_descriptor(dev, (struct usb_unistring_descriptor_t *)buf, strindex);

    return 0;
}

#endif


uint8_t usb_get_iface(struct usb_dev_t *dev, uint16_t iface)
{
    uint8_t alt_iface = 0;

    usb_ctrl_in(dev, &alt_iface, 0x81, 10, 0, 0, iface, 1);

    return alt_iface;
}


void usb_set_iface(struct usb_dev_t *dev, uint16_t iface, uint8_t alt_iface)
{
    usb_ctrl_set(dev, 0x01, 11, 0, alt_iface, iface);
}


uint8_t usb_get_config(struct usb_dev_t *dev)
{
    uint8_t config = 0;

    usb_ctrl_in(dev, &config, 0x80, 8, 0, 0, 0, 1);

    return config;
}


unsigned int usb_set_device_addr(struct usb_dev_t *dev, unsigned int addr)
{
    struct usb_transfer_t transfer;
    unsigned int addr2;

    usb_setup_transfer(dev, dev->endpoints, &transfer, USB_TRANSFER_CTRL);
    addr2 = usb_setup_transaction(&transfer, 0x00, 5, 0, addr, 0, 0);
    usb_in_transaction(&transfer, 1, 0, 0);

    // XXX: how to handle errors here?
    usb_schedule_transfer(&transfer);

    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    return addr2;
}


int usb_set_config(struct usb_dev_t *dev, uint32_t config)
{
    if(usb_ctrl_set(dev, 0x00, 9, 0, config, 0))
    {
        dev->cur_config = config;
        return 0;
    }

    return -EIO;
}


int usb_setup_device(struct usb_dev_t *dev, unsigned int addr)
{
    int res;
    unsigned int i;
    struct usb_endpoint_t *endpoint;

    dev->num = 0;
    dev->configs = 0;

    // start with 8 bytes as we don't know the mps yet
    if((res = usb_get_device_descriptor(dev, (dev->speed == USB_SPEED_FULL ? 8 : 18))) < 0)
    {
        res = usb_get_device_descriptor(dev, (dev->speed == USB_SPEED_FULL ? 8 : 18));
    }

    if(res < 0)
    {
        printk("usb: failed to get device descriptor (err %d)\n", res);
        return res;
    }

    dev->num = usb_set_device_addr(dev, addr);

    // now get full desc
    if(dev->speed == USB_SPEED_FULL)
    {
        usb_get_device_descriptor(dev, 18);
    }

    if((res = usb_get_config_descriptor(dev)) < 0)
    {
        res = usb_get_config_descriptor(dev);
    }

    if(res < 0)
    {
        printk("usb: failed to get config descriptor (err %d)\n", res);
        return res;
    }

#ifdef __DEBUG

    if((res = usb_get_string_descriptor(dev)) < 0)
    {
        printk("usb: failed to get string descriptor (err %d)\n", res);
        return res;
    }

    for(i = 1; i < 4; i++)
    {
        usb_get_unistring_descriptor(dev, i);
    }

#endif

    // set first config
    if((res = usb_set_config(dev, 1)) < 0)
    {
        printk("usb: failed to set config (err %d)\n", res);
        return res;
    }

    if((i = usb_get_config(dev)) != 1)
    {
        printk("usb: failed to get config (expected 1, got %d)\n", i);
        return -EIO;
    }

    if(dev->class == 0x09)
    {
        printk("usb: setting up USB hub\n");
        init_hub(dev);
    }
    else
    {
        struct usb_interface_t *iface;
        int found = 0;

        for(iface = dev->interfaces; iface != NULL; iface = iface->next)
        {
            if(iface->desc.class == 0x08 && iface->desc.subclass == 0x06)   // MSD
            {
                found = 1;
                printk("usb: setting up Mass Storage Device (MSD)\n");
                init_msd(iface);
            }
            else if(iface->desc.class == 0x03)
            {
                found = 1;
                printk("usb: setting up Human Interface Device (HID)\n");
                init_hid(iface);
            }
        }

        if(!found)
        {
            printk("usb: finished unknown device setup\n");
        }
    }

    // now create /dev nodes for the device's endpoints
    for(endpoint = dev->endpoints; endpoint != NULL; endpoint = endpoint->next)
    {
        char buf[16];

        ksprintf(buf, 16, "usb%d.%d.%02d", dev->bus, dev->num, endpoint->addr);
        add_dev_node(buf, 
                     USB_MAKE_DEVID(dev->bus, dev->num, endpoint->addr), 
                     (S_IFCHR | 0666)); // crw-rw-rw-
    }

    return 0;
}


void usb_clear_feature_halt(struct usb_dev_t *dev, struct usb_endpoint_t *endpoint)
{
    usb_ctrl_set(dev, 0x02, 1, 0, 0, endpoint->addr);
}


/*
 * Kernel disk task function.
 */
void usb_task_func(void *arg)
{
    static volatile unsigned long long last_tick = 0;
    volatile struct usb_transfer_t *t;

    UNUSED(arg);
    last_tick = ticks;

    while(1)
    {
        set_task_waitchan(this_core->cur_task, &usb_task);
        block_task_timeout(this_core->cur_task, 10);

        kernel_mutex_lock(&inttransfer_list_lock);

        for(t = inttransfer_head.next_inttransfer; t != NULL; t = t->next_inttransfer)
        {
            if(usb_poll_transfer((struct usb_transfer_t *)t) && t->callback)
            {
                t->callback(t->callback_arg);
            }
        }

        kernel_mutex_unlock(&inttransfer_list_lock);

        if(ticks >= (last_tick + PIT_FREQUENCY))
        {
            last_tick = ticks;
            uhci_poll();
            ehci_poll();
            usb_hub_poll();
        }
    }
}


static inline char *usbtype(uint8_t prog_if)
{
    switch(prog_if)
    {
        case USB_TYPE_UHCI: return "UHCI";
        case USB_TYPE_OHCI: return "OHCI";
        case USB_TYPE_EHCI: return "EHCI";
        case USB_TYPE_XHCI: return "XHCI";
        case USB_TYPE_NOHCI: return "No HCI";
        case USB_TYPE_ANYHCI: return "Any HCI";
        default: return "Unknown";
    }
}


static void fork_poller(void)
{
    static volatile int forked = 0;

    if(forked)
    {
        return;
    }

    (void)start_kernel_task("usbpoll", usb_task_func, NULL, &usb_task,
                                            KERNEL_TASK_ELEVATED_PRIORITY);
    forked = 1;
}


int usb_init(struct pci_dev_t *pci)
{
    int res = -EINVAL;
    uint16_t i;
    struct pci_bar_t bar[6];

    pci_get_bar_info(pci, &bar[0]);
    fork_poller();

    for(i = 0; i < 6; i++)
    {
        if(bar[i].iotype == PCI_IOTYPE_INVALID)
        {
            continue;
        }

        printk("usb: device %x:%x:%x.%x type 0x%x (%s):\n", 
               pci->bus, pci->dev, pci->function, i, pci->prog_if, usbtype(pci->prog_if));

        printk("usb:    BAR " _XPTR_ ", iosize " _XPTR_ " (%s)\n", 
               bar[i].base, bar[i].iosize,
               (bar[i].iotype == PCI_IOTYPE_MMIO) ? "MMIO" : "I/O");

        if(last_bus >= MAX_USB_BUSES)
        {
            printk("usb: too many buses -- skipping\n");
            return -ENOMEM;
        }

        usbbus[last_bus] = pci;
        pci->unit = last_bus++;

        switch(pci->prog_if)
        {
            case USB_TYPE_EHCI:
                res = ehci_install(pci, &bar[i]);
                break;

            case USB_TYPE_OHCI:
                res = ohci_install(pci, &bar[i]);
                break;

            case USB_TYPE_UHCI:
                res = uhci_install(pci, &bar[i]);
                break;

            default:
                printk("usb: unsupported device type -- skipping\n");
                break;
        }
    }

    // if the bus was set up properly, add a controller device
    if(res == 0)
    {
        char buf[16];

        ksprintf(buf, 16, "usb%d", pci->unit);
        add_dev_node(buf, USB_MAKE_DEVID(pci->unit, 0, 0), (S_IFCHR | 0666)); // crw-rw-rw-
    }

    return 0;
}

