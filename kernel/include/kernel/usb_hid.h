/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: usb_hid.h
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
 *  \file usb_hid.h
 *
 *  The Universal Serial Bus (USB) driver definitions.
 */

#ifndef KERNEL_USB_HID_H
#define KERNEL_USB_HID_H

struct usb_hid_descriptor_t
{
    uint8_t  len;
    uint8_t  type;
    uint16_t hid_bcd;
    uint8_t  country_code;
    uint8_t  descriptor_count;

    struct
    {
        uint8_t  type;
        uint16_t len;
    } descriptors[1];
} __attribute__((packed));

struct usb_hid_dev_t
{
    struct usb_transfer_t transfer;
    struct usb_interface_t *iface;  /**< pointer to the USB device struct */
    uint8_t keystate[256];      /**< USB keyboard key state */
    uint8_t last_packet[8];     /**< last packet sent by USB keyboard */
    uint8_t buf[8];
    uint8_t leds;
    uint8_t last_key_pressed;
    int last_key_counter;
    struct usb_hid_dev_t *next; /**< pointer to next HID device */
};


int init_hid(struct usb_interface_t *iface);
void usb_hid_remove(struct usb_interface_t *iface);

#endif      /* KERNEL_USB_HID_H */
