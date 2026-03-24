/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb_ioctl.c
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
 *  \file usb_ioctl.c
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

/*
 * This ioctl() implementation is based on OpenBSD semantics, but shares none 
 * of its code.
 *
 * See: https://github.com/openbsd/src/blob/master/sys/dev/usb/usb.c
 */

//#define __DEBUG

#include <errno.h>
#include <dev/usb/usb.h>
#include <kernel/pci.h>
#include <kernel/asm.h>
#include <kernel/usb.h>
#include <kernel/usb_hub.h>
#include <kernel/usb_uhci.h>
#include <kernel/usb_ohci.h>
#include <kernel/usb_ehci.h>
#include <kernel/user.h>
#include <mm/kheap.h>


// defined in usb.c
extern struct pci_dev_t *usbbus[];


#define USB_IOCTL_COPY_RESULT(dest, src, sz)    \
    if(kernel) A_memcpy(dest, src, sz);         \
    else COPY_TO_USER(dest, src, sz);


static struct usb_dev_t *get_dev_on_bus(int busnum, uint8_t devaddr)
{
    struct pci_dev_t *bus;
    struct usb_dev_t *usb = NULL;

    if(busnum < 0 || busnum >= MAX_USB_BUSES)
    {
        return NULL;
    }

    if(devaddr < 1 || devaddr >= MAX_DEV_PER_HC)
    {
        return NULL;
    }

    bus = usbbus[busnum];

    switch(bus->prog_if)
    {
        case USB_TYPE_EHCI:
            usb = ehci_get_dev_struct(bus, devaddr);
            break;

        case USB_TYPE_OHCI:
            usb = ohci_get_dev_struct(bus, devaddr);
            break;

        case USB_TYPE_UHCI:
            usb = uhci_get_dev_struct(bus, devaddr);
            break;
    }

    return usb;
}


static long get_devinfo(dev_t dev_id, void *arg, int kernel, int devaddr_from_arg)
{
    int i, busaddr = USB_DEVID_BUS(dev_id);
    uint8_t devaddr;
    struct usb_dev_t *usb;
    struct usb_device_info info;

    COPY_FROM_USER(&info, arg, sizeof(struct usb_device_info));
    devaddr = devaddr_from_arg ? info.udi_addr : USB_DEVID_DEVADDR(dev_id);

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    info.udi_bus = busaddr;
    info.udi_addr = usb->num;
    info.udi_port = usb->port;
    strcpy(info.udi_product, usb->product_name);
    strcpy(info.udi_serial, usb->serial);

    // for now, give one device name that is indexed by bus & device numbers
    ksprintf(info.udi_devnames[0], USB_MAX_DEVNAMELEN, "usb%d.%d", busaddr, usb->num);

    for(i = 1; i < USB_MAX_DEVNAMES; i++)
    {
        info.udi_devnames[i][0] = '\0';
    }

    info.udi_vendor[0] = '\0';  // TODO: ???
    info.udi_release[0] = '\0'; // TODO: ???
    info.udi_power = 0;         // TODO: ???

    info.udi_productNo = usb->product;
    info.udi_vendorNo = usb->vendor;
    info.udi_releaseNo = usb->release;
    info.udi_class = usb->class;
    info.udi_subclass = usb->subclass;
    info.udi_protocol = usb->protocol;
    info.udi_config = usb->cur_config;
    info.udi_speed = usb->speed;

    if(usb->class == 0x09)
    {
        struct usb_hub_t *hub;

        if(!(hub = get_hub_struct(usb)))
        {
            return -ENXIO;
        }

        info.udi_nports = hub->desc.ports;

        for(i = 1; i <= MIN(16, hub->desc.ports); i++)
        {
            if(hub->ports[i].usb)
            {
                info.udi_ports[i - 1] = usb->num;
            }
            else if(hub->ports[i].flags & HUB_PORT_FLAG_ENABLED)
            {
                info.udi_ports[i - 1] = USB_PORT_ENABLED;
            }
            else if(hub->ports[i].flags & HUB_PORT_FLAG_SUSPENDED)
            {
                info.udi_ports[i - 1] = USB_PORT_SUSPENDED;
            }
            else if(hub->ports[i].flags & HUB_PORT_FLAG_POWERED)
            {
                info.udi_ports[i - 1] = USB_PORT_POWERED;
            }
            else
            {
                info.udi_ports[i - 1] = USB_PORT_DISABLED;
            }
        }

        for( ; i <= 16; i++)
        {
            info.udi_ports[i - 1] = USB_PORT_DISABLED;
        }
    }

    USB_IOCTL_COPY_RESULT(arg, &info, sizeof(struct usb_device_info));

    return 0;
}


static long get_devdesc(dev_t dev_id, void *arg, int kernel, int devaddr_from_arg)
{
    int busaddr = USB_DEVID_BUS(dev_id);
    uint8_t devaddr;
    struct usb_dev_t *usb;
    struct usb_device_ddesc ddesc;

    COPY_FROM_USER(&ddesc, arg, sizeof(struct usb_device_ddesc));
    devaddr = devaddr_from_arg ? ddesc.udd_addr : USB_DEVID_DEVADDR(dev_id);

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    if(usb_ctrl_in(usb, &ddesc.udd_desc, 0x80, 6, 1, 0, 0, 18))
    {
        ddesc.udd_bus = busaddr;
        ddesc.udd_addr = usb->num;
        USB_IOCTL_COPY_RESULT(arg, &ddesc, sizeof(struct usb_device_ddesc));
        return 0;
    }

    return -EIO;
}


static long get_configdesc(dev_t dev_id, void *arg, int kernel, int devaddr_from_arg)
{
    int busaddr = USB_DEVID_BUS(dev_id);
    int config;
    uint8_t devaddr;
    struct usb_dev_t *usb;
    struct usb_device_cdesc cdesc;

    COPY_FROM_USER(&cdesc, arg, sizeof(struct usb_device_cdesc));
    devaddr = devaddr_from_arg ? cdesc.udc_addr : USB_DEVID_DEVADDR(dev_id);

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    config = cdesc.udc_config_index;

    if(config == USB_CURRENT_CONFIG_INDEX)
    {
        config = usb->cur_config;
    }

    if(usb_ctrl_in(usb, &cdesc.udc_desc, 0x80, 6, 2, 0, config, 9))
    {
        cdesc.udc_bus = busaddr;
        cdesc.udc_addr = usb->num;
        USB_IOCTL_COPY_RESULT(arg, &cdesc, sizeof(struct usb_device_cdesc));
        return 0;
    }

    return -EIO;
}


static long get_fulldesc(dev_t dev_id, void *arg, int kernel, int devaddr_from_arg)
{
    int busaddr = USB_DEVID_BUS(dev_id);
    int config;
    uint8_t devaddr;
    long res;
    char *buf;
    struct usb_dev_t *usb;
    struct usb_device_fdesc fdesc;
    struct usb_config_descriptor_t base;

    COPY_FROM_USER(&fdesc, arg, sizeof(struct usb_device_fdesc));
    devaddr = devaddr_from_arg ? fdesc.udf_addr : USB_DEVID_DEVADDR(dev_id);

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    config = fdesc.udf_config_index;

    if(config == USB_CURRENT_CONFIG_INDEX)
    {
        config = usb->cur_config;
    }

    // first, get the config descriptor to determine how big the buffer is
    // going to be
    if(!usb_ctrl_in(usb, &base, 0x80, 6, 2, 0, config, 9))
    {
        return -EIO;
    }

    if(!base.totlen || !fdesc.udf_data || !fdesc.udf_size)
    {
        return -EINVAL;
    }

    // now get a buffer and read all descriptors
    if(!(buf = kmalloc(base.totlen)))
    {
        return -ENOMEM;
    }

    if(!usb_ctrl_in(usb, buf, 0x80, 6, 2, 0, config, base.totlen))
    {
        kfree(buf);
        return -EIO;
    }

    fdesc.udf_addr = usb->num;
    res = MIN(fdesc.udf_size, base.totlen);

    if(kernel)
    {
        A_memcpy(fdesc.udf_data, buf, res);
        res = 0;
    }
    else
    {
        if(copy_to_user(fdesc.udf_data, buf, res) != 0)
        {
            res = -EFAULT;
        }
        else
        {
            res = 0;
        }
    }

    kfree(buf);

    return res;
}


static long request(dev_t dev_id, void *arg, int kernel, int devaddr_from_arg)
{
    int success, busaddr = USB_DEVID_BUS(dev_id);
    uint8_t devaddr;
    uint8_t type, req;
    uint16_t val, index;
    size_t len;
    struct usb_dev_t *usb;
    struct usb_ctl_request creq;

    UNUSED(kernel);

    COPY_FROM_USER(&creq, arg, sizeof(struct usb_ctl_request));
    devaddr = devaddr_from_arg ? creq.ucr_addr : (uint8_t)USB_DEVID_DEVADDR(dev_id);

    len = UGETW(creq.ucr_request.wLength);
    val = UGETW(creq.ucr_request.wValue);
    index = UGETW(creq.ucr_request.wIndex);
    type = creq.ucr_request.bmRequestType;
    req = creq.ucr_request.bRequest;

    if(len > 32767)
    {
        return -EINVAL;
    }

    // guard against harmful requests
    if((type == UT_WRITE_DEVICE && req == UR_SET_ADDRESS) ||
       (type == UT_WRITE_DEVICE && req == UR_SET_CONFIG) ||
       (type == UT_WRITE_INTERFACE && req == UR_SET_INTERFACE))
    {
        return -EINVAL;
    }

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    if(type & 0x80)             // read request
    {
        // TODO: check the sanity of the buf pointer
        success = usb_ctrl_in(usb, creq.ucr_data, type, req,
                              (val >> 8), (val & 0xff),
                              index, len);
    }
    else
    {
        if(creq.ucr_data && creq.ucr_request.wLength)
        {
            // TODO: check the sanity of the buf pointer
            success = usb_ctrl_out(usb, creq.ucr_data, type, req,
                                   (val >> 8), (val & 0xff),
                                   index, len);
        }
        else
        {
            USETW(creq.ucr_request.wLength, 0);
            success = usb_ctrl_set(usb, type, req,
                                   (val >> 8), (val & 0xff),
                                   index);
        }
    }

    // TODO: check for short transfers and whether they should be accepted
    //       (only if the USBD_SHORT_XFER_OK flag is set)
    if(success)
    {
        creq.ucr_actlen = len;
    }

    return success ? 0 : -EIO;
}


static long get_config(dev_t dev_id, void *arg)
{
    int busaddr = USB_DEVID_BUS(dev_id);
    uint8_t devaddr = USB_DEVID_DEVADDR(dev_id);
    int cur_config;
    struct usb_dev_t *usb;

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    cur_config = usb->cur_config;

    return copy_to_user(arg, &cur_config, sizeof(int));
}


static long set_config(dev_t dev_id, void *arg)
{
    int busaddr = USB_DEVID_BUS(dev_id);
    uint8_t devaddr = USB_DEVID_DEVADDR(dev_id);
    int config;
    struct usb_dev_t *usb;

    COPY_FROM_USER(&config, arg, sizeof(int));

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    return usb_set_config(usb, config);
}


static long get_iface(dev_t dev_id, void *arg)
{
    int busaddr = USB_DEVID_BUS(dev_id);
    uint8_t devaddr = USB_DEVID_DEVADDR(dev_id);
    struct usb_alt_interface iface;
    struct usb_dev_t *usb;

    COPY_FROM_USER(&iface, arg, sizeof(struct usb_alt_interface));

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    iface.uai_alt_no = usb_get_iface(usb, iface.uai_interface_index);

    return copy_to_user(arg, &iface, sizeof(struct usb_alt_interface));
}


static long set_iface(dev_t dev_id, void *arg)
{
    int busaddr = USB_DEVID_BUS(dev_id);
    uint8_t devaddr = USB_DEVID_DEVADDR(dev_id);
    struct usb_alt_interface iface;
    struct usb_dev_t *usb;

    COPY_FROM_USER(&iface, arg, sizeof(struct usb_alt_interface));

    if(!(usb = get_dev_on_bus(busaddr, devaddr)))
    {
        return -ENXIO;
    }

    usb_set_iface(usb, iface.uai_interface_index, iface.uai_alt_no);

    return 0;
}


/*
 * General USB controller device (/dev/usbN) control function
 */
long usb_bus_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel)
{
    switch(cmd)
    {
        case USB_DEVICEINFO:
            return get_devinfo(dev_id, arg, kernel, 1);

        case USB_DEVICE_GET_DDESC:
            return get_devdesc(dev_id, arg, kernel, 1);

        case USB_DEVICE_GET_CDESC:
            return get_configdesc(dev_id, arg, kernel, 1);

        case USB_DEVICE_GET_FDESC:
            return get_fulldesc(dev_id, arg, kernel, 1);

        case USB_REQUEST:
            return request(dev_id, arg, kernel, 1);

        case USB_DEVICESTATS:
            return -ENOSYS;         // TODO

        case USB_SET_SHORT_XFER:
            return 0;               // TODO

        case USB_SET_TIMEOUT:
            return 0;               // TODO
    }

    return -EINVAL;
}


/*
 * General USB generic device (/dev/usbN.DD.EE) control function
 */
long usb_dev_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel)
{
    /*
     * Currently we only support ioctl() operations on the control endpoint
     */
    if(USB_DEVID_EPADDR(dev_id) != 0)
    {
        return -EINVAL;
    }

    switch(cmd)
    {
        case USB_GET_DEVICEINFO:
            return get_devinfo(dev_id, arg, kernel, 0);

        case USB_GET_DEVICE_DESC:
            return get_devdesc(dev_id, arg, kernel, 0);

        case USB_GET_CONFIG_DESC:
            return get_configdesc(dev_id, arg, kernel, 0);

        case USB_GET_FULL_DESC:
            return get_fulldesc(dev_id, arg, kernel, 0);

        case USB_DO_REQUEST:
            return request(dev_id, arg, kernel, 0);

        case USB_GET_CONFIG:
            return get_config(dev_id, arg);

        /*
         * XXX: This operation can only be performed when the control endpoint is
         *      the sole open endpoint.
         * See: https://man.netbsd.org/ugen.4
         */
        case USB_SET_CONFIG:
            return set_config(dev_id, arg);

        case USB_GET_ALTINTERFACE:
            return get_iface(dev_id, arg);

        /*
         * XXX: This operation can only be performed when no endpoints for the
         *      interface are open.
         * See: https://man.netbsd.org/ugen.4
         */
        case USB_SET_ALTINTERFACE:
            return set_iface(dev_id, arg);

        case USB_SET_SHORT_XFER:
            return 0;               // TODO

        case USB_SET_TIMEOUT:
            return 0;               // TODO
    }

    return -EINVAL;
}


/*
 * General USB control function
 */
long usb_ctrl_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel)
{
    if(USB_DEVID_DEVADDR(dev_id))
    {
        return usb_dev_ioctl(dev_id, cmd, arg, kernel);
    }
    else
    {
        return usb_bus_ioctl(dev_id, cmd, arg, kernel);
    }
}


/*
 * Read from a generic USB device.
 */
ssize_t usb_ctrl_read(struct file_t *f, off_t *pos,
                      unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    dev_t dev = f->node->blocks[0];
    volatile struct usb_endpoint_t *endpoint;
    struct usb_dev_t *usb;
    struct usb_transfer_t transfer;

    if(!buf || !count)
    {
        return -EINVAL;
    }

    if(!(usb = get_dev_on_bus(USB_DEVID_BUS(dev), USB_DEVID_DEVADDR(dev))))
    {
        return -ENXIO;
    }

    // find the IN endpoint
    for(endpoint = usb->endpoints; endpoint != NULL; endpoint = endpoint->next)
    {
        if(/* endpoint->type == USB_ENDPOINT_BULK && */
           endpoint->direction == USB_ENDPOINT_IN &&
           endpoint->addr == USB_DEVID_EPADDR(dev))
        {
            break;
        }
    }

    if(!endpoint)
    {
        return -EINVAL;
    }

    // XXX: same locking code in usb_msd.c
    while(kernel_mutex_trylock(&usb->lock) != 0)
    {
        set_task_waking_signal(this_core->cur_task, 0);
        __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_SELECT_EVENT);
        block_task_timeout(this_core->cur_task, 3);
        //__asm__ __volatile__("pause":::);
    }
    //kernel_mutex_lock(&usb->lock);

    usb_setup_transfer(usb, (struct usb_endpoint_t *)endpoint, &transfer, USB_TRANSFER_BULK);
    usb_in_transaction(&transfer, 0, buf, count);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    kernel_mutex_unlock(&usb->lock);

    // TODO: check the actual amount of data transferred
    return transfer.success ? (ssize_t)count : -EIO;
}


/*
 * Write to a generic USB device.
 */
ssize_t usb_ctrl_write(struct file_t *f, off_t *pos,
                       unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    dev_t dev = f->node->blocks[0];
    volatile struct usb_endpoint_t *endpoint;
    struct usb_dev_t *usb;
    struct usb_transfer_t transfer;

    if(!buf || !count)
    {
        return -EINVAL;
    }

    if(!(usb = get_dev_on_bus(USB_DEVID_BUS(dev), USB_DEVID_DEVADDR(dev))))
    {
        return -ENXIO;
    }

    // find the IN endpoint
    for(endpoint = usb->endpoints; endpoint != NULL; endpoint = endpoint->next)
    {
        if(/* endpoint->type == USB_ENDPOINT_BULK && */
           endpoint->direction == USB_ENDPOINT_OUT &&
           endpoint->addr == USB_DEVID_EPADDR(dev))
        {
            break;
        }
    }

    if(!endpoint)
    {
        return -EINVAL;
    }

    // XXX: same locking code in usb_msd.c
    while(kernel_mutex_trylock(&usb->lock) != 0)
    {
        set_task_waking_signal(this_core->cur_task, 0);
        __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_SELECT_EVENT);
        block_task_timeout(this_core->cur_task, 3);
        //__asm__ __volatile__("pause":::);
    }
    //kernel_mutex_lock(&usb->lock);

    usb_setup_transfer(usb, (struct usb_endpoint_t *)endpoint, &transfer, USB_TRANSFER_BULK);
    usb_out_transaction(&transfer, 0, buf, count);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    kernel_mutex_unlock(&usb->lock);

    // TODO: check the actual amount of data transferred
    return transfer.success ? (ssize_t)count : -EIO;
}

