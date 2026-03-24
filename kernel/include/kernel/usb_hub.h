/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: usb_hub.h
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
 *  \file usb_hub.h
 *
 *  The Universal Serial Bus (USB) driver definitions.
 */

#ifndef KERNEL_USB_HUB_H
#define KERNEL_USB_HUB_H

struct usb_hub_desc_t
{
    uint8_t len;
    uint8_t type;
    uint8_t ports;
    uint16_t power_switch_mode : 2;
    uint16_t comp_dev          : 1;
    uint16_t over_cur_prot_mode: 2;
    uint16_t tt_think_time     : 2;
    uint16_t port_indicators   : 1;
    uint8_t res;
    uint8_t poweron;
    uint8_t hc_current;
    // bit 0 => reserved
    // bit 1 => port 1
    // bit 2 => port 2
    // ...
    // bit N => port N (as indicated in the ports field)
    uint8_t devrem[8];
} __attribute__((packed));

struct usb_hub_port_status_t
{
    // Status
    uint32_t curstat : 1;
    uint32_t enabled : 1;
    uint32_t suspend : 1;
    uint32_t overcur : 1;
    uint32_t reset   : 1;
    uint32_t res1    : 3;
    uint32_t power   : 1;
    uint32_t lospeed : 1;
    uint32_t hispeed : 1;
    uint32_t testmode: 1;
    uint32_t control : 1;
    uint32_t res2    : 3;

    // Change
    uint32_t state_change   : 1;
    uint32_t enabled_change : 1;
    uint32_t suspend_change : 1;
    uint32_t overcur_change : 1;
    uint32_t reset_change   : 1;
    uint32_t res3           : 11;
} __attribute__((packed));

struct usb_hub_status_t
{
    uint32_t local_power        : 1;
    uint32_t overcur            : 1;
    uint32_t res1               : 14;
    uint32_t local_power_change : 1;
    uint32_t overcur_change     : 1;
    uint32_t res2               : 14;
} __attribute__((packed));


struct usb_hub_port_t
{
#define HUB_PORT_FLAG_CONNECTED         0x01
#define HUB_PORT_FLAG_ENABLED           0x02
#define HUB_PORT_FLAG_SUSPENDED         0x04
#define HUB_PORT_FLAG_POWERED           0x08
    volatile int flags;         /**< port flags */

    struct usb_dev_t *usb;      /**< if non-NULL, pointer to the USB device
                                     currently connected on this port */
};

struct usb_hub_t
{
    struct usb_dev_t *usb;  /**< pointer to the USB device struct */
    volatile struct usb_hub_port_t *ports;  /**< root ports */
    struct usb_hub_desc_t desc;
    struct usb_hub_status_t status;
    struct usb_hub_t *next; /**< pointer to next hub */
};


int init_hub(struct usb_dev_t *usb);
void usb_hub_remove_dev(struct usb_dev_t *usb);
void usb_hub_poll(void);
struct usb_hub_t *get_hub_struct(struct usb_dev_t *usb);

#endif      /* KERNEL_USB_HUB_H */
