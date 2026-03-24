/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: usb_hub.c
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
 *  \file usb_hub.c
 *
 *  The Universal Serial Bus (USB) driver code is split into several files:
 *    - usb.c       => main entry point and general functions
 *    - usb_msd.c   => functions to handle Mass Storage Devices (MSD)
 *    - usb_hid.c   => functions to handle Human Interaction Devices (HID)
 *    - usb_hub.c   => functions to handle USB hubs
 *    - usb_ioctl.c => functions to handle ioctl() calls
 *    - usb_uhci.c  => UHCI layer
 */

//#define __DEBUG

#include <errno.h>
#include <kernel/pci.h>
#include <kernel/asm.h>
#include <kernel/usb.h>
#include <kernel/usb_hub.h>
#include <mm/kheap.h>


struct usb_hub_t hub_list;
struct kernel_mutex_t usb_hub_tablock;


struct usb_hub_t *get_hub_struct(struct usb_dev_t *usb)
{
    volatile struct usb_hub_t *hub;

    elevated_priority_lock(&usb_hub_tablock);

    for(hub = hub_list.next; hub != NULL; hub = hub->next)
    {
        if(hub->usb == usb)
        {
            elevated_priority_unlock(&usb_hub_tablock);
            return (struct usb_hub_t *)hub;
        }
    }

    elevated_priority_unlock(&usb_hub_tablock);

    return NULL;
}


static int hub_get_descriptor(struct usb_hub_t *hub)
{
    int res;
    struct usb_hub_desc_t *desc = &hub->desc;

    res = usb_ctrl_in(hub->usb, desc, 0xA0, 6, 0x29, 0, 
                        0, sizeof(struct usb_hub_desc_t));

    if(!res)
    {
        printk("usb-hub: failed to get hub %s\n", "descriptor");
    }

    return res;
}


static int hub_get_status(struct usb_hub_t *hub)
{
    int res;

    res = usb_ctrl_in(hub->usb, &hub->status, 0xA0, 0, 0, 0, 0, 4);

    if(!res)
    {
        printk("usb-hub: failed to get hub %s\n", "status");
    }

    return res;
}


static int hub_port_connected(struct usb_hub_t *hub, uint8_t port)
{
    struct usb_hub_port_status_t portst;

    if(usb_ctrl_in(hub->usb, &portst, 0xA3, 0, 0, 0, port, 4))
    {
        return portst.curstat;
    }

    printk("usb-hub: failed to get port %d %s\n", port, "connection status");

    return 0;
}


static int hub_setup_device(volatile struct usb_hub_t *hub, unsigned int port, uint8_t speed)
{
    struct usb_dev_t *usb;
    int res;

    if(!(usb = usb_create_dev(hub->usb->bus, port, speed)))
    {
        printk("usb-hub: failed to create USB device\n");
        return -ENOMEM;
    }

    usb->type = hub->usb->type;
    usb->priv = hub->usb->priv;
    usb->ops = hub->usb->ops;

    // TODO: check the result of usb->get_next_addr() != 0
    if((res = usb_setup_device(usb, usb->ops->get_next_addr(usb->priv) /* port + 1 */)) < 0)
    {
        printk("usb-hub: failed to set up USB device\n");
        usb->ops->free_addr(usb->priv, usb->num);
        usb_destroy_dev(usb);
        return res;
    }

    hub->ports[port].usb = usb;
    hub->ports[port].flags |= HUB_PORT_FLAG_CONNECTED;

    return 0;
}


#define TOGGLE(hub, port, portst, bit, flag)            \
    if(portst -> bit) hub -> ports[port].flags |= flag; \
    else hub -> ports[port].flags &= ~flag;

static int port_get_status(volatile struct usb_hub_t *hub, 
                           struct usb_hub_port_status_t *portst, uint8_t port)
{
    int res;

    res = usb_ctrl_in(hub->usb, portst, 0xA3, 0, 0, 0, port, 4);

    if(!res)
    {
        printk("usb-hub: failed to get port %d %s\n", port, "status");
    }
    else
    {
        TOGGLE(hub, port, portst, enabled, HUB_PORT_FLAG_ENABLED);
        TOGGLE(hub, port, portst, suspend, HUB_PORT_FLAG_SUSPENDED);
        TOGGLE(hub, port, portst, power  , HUB_PORT_FLAG_POWERED);
    }

    return res;
}

#undef TOGGLE


static int port_set_feature(volatile struct usb_hub_t *hub, uint8_t feature, 
                            uint8_t sel, uint8_t port)
{
    int res;

    res = usb_ctrl_set(hub->usb, 0x23, 3, 0, feature, (sel << 8) | port);

    if(!res)
    {
        printk("usb-hub: failed to %s feature %d on port %d\n", "set", feature, port);
    }

    return res;
}


static int port_clear_feature(volatile struct usb_hub_t *hub, uint8_t feature, uint8_t port)
{
    int res;

    res = usb_ctrl_set(hub->usb, 0x23, 1, 0, feature, port);

    if(!res)
    {
        printk("usb-hub: failed to %s feature %d on port %d\n", "clear", feature, port);
    }

    return res;
}


void usb_hub_remove_dev(struct usb_dev_t *usb)
{
    volatile struct usb_hub_t *prev, *hub = NULL;
    struct usb_dev_t *tmp;
    volatile unsigned int i;

    elevated_priority_lock(&usb_hub_tablock);

    for(prev = &hub_list; prev != NULL; prev = prev->next)
    {
        if(prev->next && prev->next->usb == usb)
        {
            hub = prev->next;
            prev->next = hub->next;
            break;
        }
    }

    elevated_priority_unlock(&usb_hub_tablock);

    if(!hub)
    {
        printk("usb-hub: cannot find USB hub in list\n");
        return;
    }

    if(hub->ports)
    {
        for(i = 1; i <= hub->desc.ports; i++)
        {
            if((tmp = hub->ports[i].usb))
            {
                hub->ports[i].usb = NULL;
                tmp->ops->free_addr(tmp->priv, tmp->num);
                usb_destroy_dev(tmp);
                hub->ports[i].flags &= ~HUB_PORT_FLAG_CONNECTED;
            }
        }

        kfree((void *)hub->ports);
        hub->ports = NULL;
    }

    kfree((void *)hub);
}


void usb_hub_poll(void)
{
    volatile struct usb_hub_t *hub;
    struct usb_hub_port_status_t portst;
    struct usb_dev_t *usb;
    volatile unsigned int i;
    volatile int restart;

    elevated_priority_lock(&usb_hub_tablock);

    for(hub = hub_list.next; hub != NULL; hub = hub->next)
    {
        restart = 0;
        elevated_priority_unlock(&usb_hub_tablock);

        // for each hub port
        for(i = 1; i <= hub->desc.ports; i++)
        {
            // check port status
            port_get_status(hub, &portst, i);

            // and if there is a device change
            if(portst.state_change)
            {
                // and if there is a new device attached, set it up
                if(portst.curstat && 
                   !(hub->ports[i].flags & HUB_PORT_FLAG_CONNECTED))
                {
                    printk("usb-hub: device connected to port %d\n", i);

                    // power on
                    port_set_feature(hub, 8, 0, i);
                    tick_delay(1);

                    // reset
                    port_set_feature(hub, 4, 0, i);
                    tick_delay(1);

                    // XXX: set the right speed
                    hub_setup_device(hub, i, USB_SPEED_HIGH);
                    restart = 1;
                }
                // and if a device was removed, delete it
                else if(!portst.curstat && 
                        (hub->ports[i].flags & HUB_PORT_FLAG_CONNECTED))
                {
                    printk("usb-hub: device removed from port %d\n", i);

                    usb = hub->ports[i].usb;
                    hub->ports[i].usb = NULL;
                    usb->ops->free_addr(usb->priv, usb->num);
                    usb_destroy_dev(usb);
                    hub->ports[i].flags &= ~HUB_PORT_FLAG_CONNECTED;
                    restart = 1;
                }

                port_clear_feature(hub, 16, i);
            }
        }

        elevated_priority_lock(&usb_hub_tablock);

        if(restart)
        {
            hub = &hub_list;
        }
    }

    elevated_priority_unlock(&usb_hub_tablock);
}


/*
static void print_port_stat(struct usb_hub_port_status_t *portst)
{
    printk("curstat %d, enabled %d, suspend %d, overcur %d, ", portst->curstat, portst->enabled, portst->suspend, portst->overcur);
    printk("reset %d, res1 %d, power %d, lospeed %d, ", portst->reset, portst->res1, portst->power, portst->lospeed);
    printk("hispeed %d, testmode %d, control %d, res2 %d\n", portst->hispeed, portst->testmode, portst->control, portst->res2);

    printk("state_change %d, enabled_change %d, suspend_change %d, overcur_change %d, reset_change %d, res3 %d\n", portst->state_change, portst->enabled_change, portst->suspend_change, portst->overcur_change, portst->reset_change, portst->res3);
}
*/


int init_hub(struct usb_dev_t *usb)
{
    struct usb_hub_t *hub;
    struct usb_hub_port_status_t portst;
    //volatile struct usb_endpoint_t *endpoint;
    volatile uint8_t i;

    if(!usb)
    {
        return -EINVAL;
    }

    if(!(hub = kmalloc(sizeof(struct usb_hub_t))))
    {
        printk("usb-hub: insufficient memory to init USB hub\n");
        return -ENOMEM;
    }
    
    A_memset(hub, 0, sizeof(struct usb_hub_t));
    hub->usb = usb;

    /*
    // find the interrupt endpoint
    for(endpoint = usb->endpoints; endpoint != NULL; endpoint = endpoint->next)
    {
        if(endpoint->type == USB_ENDPOINT_INTERRUPT)
        {
            iface->endpoint_interrupt = (struct usb_endpoint_t *)endpoint;
            break;
        }
    }

    if(!endpoint)
    {
        printk("usb-hub: USB hub has invalid interrupt endpoint\n");
        kfree(hub);
        return -EINVAL;
    }
    */

    if(!hub_get_descriptor(hub))
    {
        printk("usb-hub: failed to get device descriptor\n");
        kfree(hub);
        return -EINVAL;
    }

    hub_get_status(hub);

    printk("usb-hub: ports %d\n", hub->desc.ports);

    if(hub->desc.ports == 0)
    {
        printk("usb-hub: USB hub has invalid port count\n");
        kfree(hub);
        return -EINVAL;
    }

    if(!(hub->ports = kmalloc(sizeof(struct usb_hub_port_t) * (hub->desc.ports + 1))))
    {
        printk("usb-hub: insufficient memory to initialize USB hub ports\n");
        kfree(hub);
        return -ENOMEM;
    }

    A_memset((void *)hub->ports, 0, sizeof(struct usb_hub_port_t) * (hub->desc.ports + 1));

    for(i = 1; i <= hub->desc.ports; i++)
    {
        port_get_status(hub, &portst, i);

        printk("usb-hub: checking port %d (of %d)\n", i, hub->desc.ports);
        /*
        print_port_stat(&portst);
        */

        // power on
        port_set_feature(hub, 8, 0, i);
        tick_delay(10);

        // reset
        port_set_feature(hub, 4, 0, i);
        tick_delay(10);

        if(hub_port_connected(hub, i))
        {
            // XXX: set the right speed
            hub_setup_device(hub, i, USB_SPEED_HIGH);
            port_clear_feature(hub, 16, i);
            tick_delay(10);
        }
    }

    elevated_priority_lock(&usb_hub_tablock);
    hub->next = hub_list.next;
    hub_list.next = hub;
    elevated_priority_unlock(&usb_hub_tablock);

    printk("usb-hub: finished setting up USB hub\n");

    return 0;
}

