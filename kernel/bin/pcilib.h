/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: pcilib.h
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
 *  \file pcilib.h
 *
 *  Function declarations of the utility functions implemented in pcilib.c.
 */

#ifndef PCILIB_H
#define PCILIB_H

#include <stdint.h>

#define _PATH_PCI_IDS       "/usr/share/pci.ids"

#define CLASS_COUNT         (sizeof(pci_class_table) / sizeof(pci_class_t))

struct pci_vendor_t
{
    uint16_t id;
    char *name;
    struct pci_vendor_t *next;
};

struct pci_device_t
{
    uint16_t id;
    char *name;
    struct pci_vendor_t *vendor;
    struct pci_device_t *next;
};

struct pci_class_t
{
    uint8_t id;
    char *name;
    struct pci_class_t *next;
};

struct pci_subclass_t
{
    uint8_t id;
    char *name;
    struct pci_class_t *class;
    struct pci_subclass_t *next;
};


int pcilib_init(void);
struct pci_device_t *get_pci_device(uint16_t vendor, uint16_t device_id);
struct pci_subclass_t *get_pci_subclass(uint8_t base_class, uint8_t sub_class);

#endif      /* PCILIB_H */
