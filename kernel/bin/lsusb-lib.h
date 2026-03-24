/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: lsusb-lib.h
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
 *  \file lsusb-lib.h
 *
 *  Function declarations of the utility functions implemented in lsusb-lib.c.
 */

#ifndef USBLIB_H
#define USBLIB_H

#include <stdint.h>

#define _PATH_USB_IDS       "/usr/share/usb.ids"

struct usb_vendor_t
{
    uint16_t id;
    char *name;
    struct usb_vendor_t *next;
};

struct usb_device_t
{
    uint16_t id;
    char *name;
    struct usb_vendor_t *vendor;
    struct usb_device_t *next;
};

struct usb_class_t
{
    uint8_t id;
    char *name;
    struct usb_class_t *next;
};

struct usb_subclass_t
{
    uint8_t id;
    char *name;
    struct usb_class_t *class;
    struct usb_subclass_t *next;
};

struct usb_protocol_t
{
    uint8_t id;
    char *name;
    struct usb_class_t *class;
    struct usb_subclass_t *subclass;
    struct usb_protocol_t *next;
};


int usblib_init(void);
struct usb_vendor_t *get_usb_vendor(uint16_t vendor);
struct usb_device_t *get_usb_device(uint16_t vendor, uint16_t device_id);
struct usb_class_t *get_usb_class(uint8_t base_class);
struct usb_subclass_t *get_usb_subclass(uint8_t base_class, uint8_t sub_class);
struct usb_protocol_t *get_usb_protocol(uint8_t class, uint8_t subclass, uint8_t proto);

#endif      /* USBLIB_H */
