/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: pcilib.c
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
 *  \file pcilib.c
 *
 *  Utility functions to decode the names and types of PCI devices on the
 *  system. Currently used solely by lspci, but in the future should be built
 *  as a shared library for common use.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pcilib.h"

#undef isspace
#define isspace(c)      (c == ' ' || c == '\r' || c == '\n' || c == '\t')

static int inited = 0;

struct pci_vendor_t *vendor_table = NULL;
struct pci_device_t *device_table = NULL;

struct pci_vendor_t unknown_vendor = { 0, "Unknown", NULL };
struct pci_device_t unknown_device = { 0, "Unknown", &unknown_vendor, NULL };

struct pci_class_t *class_table = NULL;
struct pci_subclass_t *subclass_table = NULL;

struct pci_class_t unknown_class = { 0, "Unknown", NULL };
struct pci_subclass_t unknown_subclass = { 0, "Unknown", &unknown_class, NULL };


struct pci_subclass_t *get_pci_subclass(uint8_t base_class, uint8_t sub_class)
{
    struct pci_subclass_t *subclass;
    
    for(subclass = subclass_table; subclass != NULL; subclass = subclass->next)
    {
        if(subclass->id == sub_class &&
           subclass->class && subclass->class->id == base_class)
        {
            return subclass;
        }
    }
    
    return &unknown_subclass;
}


struct pci_device_t *get_pci_device(uint16_t vendor, uint16_t device_id)
{
    struct pci_device_t *dev;
    
    for(dev = device_table; dev != NULL; dev = dev->next)
    {
        if(dev->id == device_id && dev->vendor && dev->vendor->id == vendor)
        {
            return dev;
        }
    }
    
    return &unknown_device;
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


static struct pci_vendor_t *get_vendor(char *str)
{
    struct pci_vendor_t *vendor;
    
    if(!(vendor = (struct pci_vendor_t *)malloc(sizeof(struct pci_vendor_t))))
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


static struct pci_device_t *get_device(char *str, struct pci_vendor_t *vendor)
{
    struct pci_device_t *dev;
    
    if(!(dev = (struct pci_device_t *)malloc(sizeof(struct pci_device_t))))
    {
        return NULL;
    }

    if(id_and_name(str, 4, &(dev->id), &(dev->name)) != 0)
    {
        free(dev);
        return NULL;
    }
    
    dev->vendor = vendor;
    dev->next = device_table;
    device_table = dev;
    
    return dev;
}


static struct pci_class_t *get_class(char *str)
{
    uint16_t id;
    struct pci_class_t *class;
    
    if(!(class = (struct pci_class_t *)malloc(sizeof(struct pci_class_t))))
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


static struct pci_subclass_t *get_subclass(char *str, struct pci_class_t *class)
{
    uint16_t id;
    struct pci_subclass_t *subclass;
    
    if(!(subclass = (struct pci_subclass_t *)malloc(sizeof(struct pci_subclass_t))))
    {
        return NULL;
    }

    if(id_and_name(str, 2, &id, &(subclass->name)) != 0)
    {
        free(subclass);
        return NULL;
    }
    
    subclass->id = (uint8_t)id;
    subclass->class = class;
    subclass->next = subclass_table;
    subclass_table = subclass;
    
    return subclass;
}


int pcilib_init(void)
{
    FILE *f;
    char buf[2048];
    int last_is_vendor = 0;
    struct pci_vendor_t *last_vendor = NULL;
    struct pci_class_t *last_class = NULL;
    
    if(inited)
    {
        return 0;
    }

    if(!(f = fopen(_PATH_PCI_IDS, "r")))
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

        //printf("pcilib_init: buf '%s'\n", buf);
        //__asm__("xchg %%bx, %%bx"::);
        
        /*
         * In the pci.ids file, device and vendor entries are formatted
         * as follows:
         *
         *   vendor vendor_name
         *   TAB device device_name
         *   TAB TAB subvendor subdevice subsystem_name
         *
         * For simplicity, we use only the first two types, i.e. we ignore
         * lines with two leading tabs.
         *
         * In addition, device classes and subclasses are formatted as:
         *
         *   C class class_name
         *   TAB subclass subclass_name
         *   TAB TAB prof-if prof-if_name
         *
         * Again, we only the first two.
         *
         * For details, see: http://pci-ids.ucw.cz/v2.2/pci.ids
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

            //printf("pcilib_init: last_vendor %x:%s\n", last_vendor->id, last_vendor->name);
            //__asm__("xchg %%bx, %%bx"::);
        }
        else if(*buf == '\t')
        {
            /*
             * TAB TAB subvendor subdevice subsystem_name
             *   --or--
             * TAB TAB prof-if prof-if_name
             */
            if(buf[1] == '\t')
            {
                continue;
            }

            if(last_is_vendor)
            {
                /* TAB device device_name */
                get_device(&buf[1], last_vendor);

                /*
                struct pci_device_t *d = get_device(&buf[1], last_vendor);
                printf("pcilib_init: dev %x:%s\n", d->id, d->name);
                __asm__("xchg %%bx, %%bx"::);
                */
            }
            else
            {
                /* TAB subclass subclass_name */
                get_subclass(&buf[1], last_class);
            }
        }
        else if(*buf == 'C')
        {
            /* class or subclass name */
            last_class = get_class(buf);
            last_is_vendor = 0;
        }
    }
    
    inited = 1;
    return 0;
}

