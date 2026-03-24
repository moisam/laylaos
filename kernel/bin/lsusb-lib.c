/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: lsusb-lib.c
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
 *  \file lsusb-lib.c
 *
 *  Utility functions to decode the names and types of USB devices on the
 *  system. Currently used solely by lsusb, but in the future should be built
 *  as a shared library for common use.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lsusb-lib.h"

#undef isspace
#define isspace(c)      (c == ' ' || c == '\r' || c == '\n' || c == '\t')

static int inited = 0;

struct usb_vendor_t *vendor_table = NULL;
struct usb_device_t *device_table = NULL;

struct usb_vendor_t unknown_vendor = { 0, "Unknown", NULL };
struct usb_device_t unknown_device = { 0, "Unknown", &unknown_vendor, NULL };

struct usb_class_t *class_table = NULL;
struct usb_subclass_t *subclass_table = NULL;
struct usb_protocol_t *protocol_table = NULL;

struct usb_class_t unknown_class = { 0, "Unknown", NULL };
struct usb_subclass_t unknown_subclass = { 0, "Unknown", &unknown_class, NULL };
struct usb_protocol_t unknown_protocol = { 0, "Unknown", &unknown_class, &unknown_subclass, NULL };


struct usb_class_t *get_usb_class(uint8_t base_class)
{
    struct usb_class_t *class;

    for(class = class_table; class != NULL; class = class->next)
    {
        if(class->id == base_class)
        {
            return class;
        }
    }

    return &unknown_class;
}


struct usb_subclass_t *get_usb_subclass(uint8_t base_class, uint8_t sub_class)
{
    struct usb_subclass_t *subclass;
    
    for(subclass = subclass_table; subclass != NULL; subclass = subclass->next)
    {
        if(subclass->id == sub_class &&
           subclass->class->id == base_class)
        {
            return subclass;
        }
    }
    
    return &unknown_subclass;
}


struct usb_protocol_t *get_usb_protocol(uint8_t class, uint8_t subclass, uint8_t proto)
{
    struct usb_protocol_t *protocol;

    for(protocol = protocol_table; protocol != NULL; protocol = protocol->next)
    {
        if(protocol->id == proto &&
           protocol->class->id == class &&
           protocol->subclass->id == subclass)
        {
            return protocol;
        }
    }
    
    return &unknown_protocol;
}


struct usb_device_t *get_usb_device(uint16_t vendor, uint16_t device_id)
{
    struct usb_device_t *dev;

    for(dev = device_table; dev != NULL; dev = dev->next)
    {
        if(dev->id == device_id && dev->vendor->id == vendor)
        {
            return dev;
        }
    }

    return &unknown_device;
}


struct usb_vendor_t *get_usb_vendor(uint16_t vendor)
{
    struct usb_vendor_t *vend;

    for(vend = vendor_table; vend != NULL; vend = vend->next)
    {
        if(vend->id == vendor)
        {
            return vend;
        }
    }

    return &unknown_vendor;
}


static int get_hex(char *str, int digits)
{
    int i, j;
    int res = 0;
    
    for(i = 0; i < digits; i++, str++)
    {
        if(*str >= '0' && *str <= '9')
        {
            j = *str - '0';
        }
        else if(*str >= 'a' && *str <= 'f')
        {
            j = *str - 'a' + 10;
        }
        else
        {
            j = 0;
        }

        res = (res * 16) + j;
    }
    
    return res;
}


static int id_and_name(char *str, int id_digits, uint16_t *id, char **name)
{
    size_t len;

    *id = get_hex(str, id_digits);
    str += id_digits;
    
    while(isspace(*str))
    {
        str++;
    }
    
    len = strlen(str);

    if(str[len - 1] == '\n' || str[len - 1] == '\r')
    {
        str[len - 1] = '\0';
    }

    if(!*str || !(*name = strdup(str)))
    {
        return -ENOMEM;
    }
    
    return 0;
}


static struct usb_vendor_t *get_vendor(char *str)
{
    struct usb_vendor_t *vendor;
    
    if(!(vendor = (struct usb_vendor_t *)malloc(sizeof(struct usb_vendor_t))))
    {
        return NULL;
    }

    if(id_and_name(str, 4, &(vendor->id), &(vendor->name)) != 0)
    {
        free(vendor);
        return NULL;
    }
    
    vendor->next = vendor_table;
    vendor_table = vendor;
    
    return vendor;
}


static struct usb_device_t *get_device(char *str, struct usb_vendor_t *vendor)
{
    struct usb_device_t *dev;
    
    if(!(dev = (struct usb_device_t *)malloc(sizeof(struct usb_device_t))))
    {
        return NULL;
    }

    if(id_and_name(str, 4, &(dev->id), &(dev->name)) != 0)
    {
        free(dev);
        return NULL;
    }

    dev->vendor = vendor ? vendor : &unknown_vendor;
    dev->next = device_table;
    device_table = dev;
    
    return dev;
}


static struct usb_class_t *get_class(char *str)
{
    uint16_t id;
    struct usb_class_t *class;

    if(!(class = (struct usb_class_t *)malloc(sizeof(struct usb_class_t))))
    {
        return NULL;
    }
    
    // skip the leading 'C' and spaces on the line
    str++;

    while(isspace(*str))
    {
        str++;
    }

    if(id_and_name(str, 2, &id, &(class->name)) != 0)
    {
        free(class);
        return NULL;
    }
    
    class->id = (uint8_t)id;
    class->next = class_table;
    class_table = class;
    
    return class;
}


static struct usb_subclass_t *get_subclass(char *str, struct usb_class_t *class)
{
    uint16_t id;
    struct usb_subclass_t *subclass;
    
    if(!(subclass = (struct usb_subclass_t *)malloc(sizeof(struct usb_subclass_t))))
    {
        return NULL;
    }

    if(id_and_name(str, 2, &id, &(subclass->name)) != 0)
    {
        free(subclass);
        return NULL;
    }
    
    subclass->id = (uint8_t)id;
    subclass->class = class ? class : &unknown_class;
    subclass->next = subclass_table;
    subclass_table = subclass;
    
    return subclass;
}


static struct usb_protocol_t *get_protocol(char *str, 
                                           struct usb_class_t *class,
                                           struct usb_subclass_t *subclass)
{
    uint16_t id;
    struct usb_protocol_t *proto;
    
    if(!(proto = (struct usb_protocol_t *)malloc(sizeof(struct usb_protocol_t))))
    {
        return NULL;
    }

    if(id_and_name(str, 2, &id, &(proto->name)) != 0)
    {
        free(proto);
        return NULL;
    }
    
    proto->id = (uint8_t)id;
    proto->class = class ? class : &unknown_class;
    proto->subclass = subclass ? subclass : &unknown_subclass;
    proto->next = protocol_table;
    protocol_table = proto;
    
    return proto;
}


int usblib_init(void)
{
    FILE *f;
    char buf[2048];
    int last_is_vendor = 0;
    int last_is_class = 0;
    struct usb_vendor_t *last_vendor = NULL;
    struct usb_class_t *last_class = NULL;
    struct usb_subclass_t *last_subclass = NULL;

    if(inited)
    {
        return 0;
    }

    if(!(f = fopen(_PATH_USB_IDS, "r")))
    {
        return errno;
    }

    while(!feof(f))
    {
        if(!fgets(buf, sizeof(buf), f))
        {
            // EOF reached
            break;
        }
        
        if(!*buf)
        {
            continue;
        }

        //printf("usblib_init: buf '%s'\n", buf);
        //__asm__("xchg %%bx, %%bx"::);
        
        /*
         * In the usb.ids file, device and vendor entries are formatted
         * as follows:
         *
         *   vendor vendor_name
         *   TAB device device_name
         *   TAB TAB interface interface_name
         *
         * For simplicity, we use only the first two types, i.e. we ignore
         * lines with two leading tabs.
         *
         * In addition, device classes and subclasses are formatted as:
         *
         *   C class class_name
         *   TAB subclass subclass_name
         *   TAB TAB protocol protocol_name
         *
         * There are other definitions (e.g. HID classes) which we are not
         * using for now for simplicity.
         *
         * For details, see: http://www.linux-usb.org/usb.ids
         */
        
        //if(*buf == '#' || isspace(*buf))
        if(*buf == '#' || *buf == ' ' || *buf == '\r' || *buf == '\n')
        {
            continue;
        }
        
        /* vendor vendor_name */
        if((*buf >= '0' && *buf <= '9') || (*buf >= 'a' && *buf <= 'f'))
        {
            last_vendor = get_vendor(buf);
            last_is_vendor = 1;
            last_is_class = 0;

            //printf("usblib_init: last_vendor %x:%s\n", last_vendor->id, last_vendor->name);
            //__asm__("xchg %%bx, %%bx"::);
        }
        else if(*buf == '\t')
        {
            /*
             * TAB TAB interface interface_name
             *   --or--
             * TAB TAB protocol protocol_name
             *   --or--
             * TAB TAB one of the other entry types we don't use
             */
            if(buf[1] == '\t')
            {
                if(last_is_class)
                {
                    /* TAB TAB protocol protocol_name */
                    get_protocol(&buf[1], last_class, last_subclass);
                }

                continue;
            }

            if(last_is_vendor)
            {
                /* TAB device device_name */
                get_device(&buf[1], last_vendor);

                /*
                struct usb_device_t *d = get_device(&buf[1], last_vendor);
                printf("usblib_init: dev %x:%s\n", d->id, d->name);
                __asm__("xchg %%bx, %%bx"::);
                */
            }
            else if(last_is_class)
            {
                /* TAB subclass subclass_name */
                last_subclass = get_subclass(&buf[1], last_class);
            }
        }
        else if(*buf == 'C')
        {
            /* class or subclass name */
            last_class = get_class(buf);
            last_is_class = 1;
            last_is_vendor = 0;
        }
        else
        {
            /* one of the other entry types we don't use */
            last_is_class = 0;
            last_is_vendor = 0;
        }
    }
    
    inited = 1;
    return 0;
}

