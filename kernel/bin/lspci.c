/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: lspci.c
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
 *  \file lspci.c
 *
 *  A simple program to list PCI devices on the system.
 */

#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "pcilib.h"

#define _PATH_PCI_DEVICES       "/proc/bus/pci/devices"

int numeric = 0;
char *ver = "1.0";

struct pci_t
{
    uint8_t base_class;
    uint8_t sub_class;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor;
    uint16_t device_id;
    uint8_t rev;
};


void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",    no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"numeric", no_argument, 0, 'n'},
        {0, 0, 0, 0}
    };

    while((c = getopt_long(argc, argv, "hvn", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'n':
                numeric = 1;
                break;

            case 'v':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                printf("lspci utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options]\n\n"
                       "Options:\n"
                       "  -h, --help        Show this help and exit\n"
                       "  -n, --numeric     "
                            "Show vendor and device codes instead of names\n"
                       "  -v, --version     Print version and exit\n"
                       "\n", argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case '?':
                break;

            default:
                abort();
        }
    }

    if(optind < argc)
    {
        fprintf(stderr, "%s: ignoring excess arguments\n", argv[0]);
    }
}


#define NEXT_TAB(p2, p, err)            \
    if(!(p2 = strchr(p, '\t')))         \
    {                                   \
        fprintf(stderr, "%s: ignoring device with missing %s\n",    \
                argv[0], err);          \
        continue;                       \
    }                                   \
    *p2 = '\0';


#define NEXT_FIELD(p, p2, err)          \
    p = p2 + 1;                         \
    if(!*p)                             \
    {                                   \
        fprintf(stderr, "%s: ignoring device with missing %s\n",    \
                argv[0], err);          \
        continue;                       \
    }



int main(int argc, char **argv)
{
    FILE *f;
    char buf[2048];
    char *p, *p2;
    
    struct pci_t pci;
    struct pci_subclass_t *pci_subclass;
    struct pci_device_t *pci_device;

    parse_line_args(argc, argv);
    
    if(pcilib_init() != 0)
    {
        fprintf(stderr, "%s: failed to init pcilib: %s\n",
                argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(!(f = fopen(_PATH_PCI_DEVICES, "r")))
    {
        fprintf(stderr, "%s: failed to open %s: %s\n",
                argv[0], _PATH_PCI_DEVICES, strerror(errno));
        exit(EXIT_FAILURE);
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
        
        //printf("lspci: buf '%s'\n", buf);
        
        // get base class
        p = buf;
        //printf("lspci: 1 p '%s' (%lu)\n", p, strlen(buf));
        NEXT_TAB(p2, p, "subclass");
        //printf("lspci: 2 p '%s'\n", p);
        pci.base_class = strtoul(p, NULL, 16);
        NEXT_FIELD(p, p2, "subclass");

        // get subclass
        NEXT_TAB(p2, p, "bus");
        pci.sub_class = strtoul(p, NULL, 16);
        NEXT_FIELD(p, p2, "bus");

        // get bus
        NEXT_TAB(p2, p, "device");
        pci.bus = strtoul(p, NULL, 16);
        NEXT_FIELD(p, p2, "device");

        // get device
        NEXT_TAB(p2, p, "function");
        pci.device = strtoul(p, NULL, 16);
        NEXT_FIELD(p, p2, "function");

        // get function
        NEXT_TAB(p2, p, "vendor");
        pci.function = strtoul(p, NULL, 16);
        NEXT_FIELD(p, p2, "vendor");

        // get vendor
        NEXT_TAB(p2, p, "device id");
        pci.vendor = strtoul(p, NULL, 16);
        NEXT_FIELD(p, p2, "device id");

        // get device id
        NEXT_TAB(p2, p, "revision");
        pci.device_id = strtoul(p, NULL, 16);
        NEXT_FIELD(p, p2, "revision");

        // get revision
        pci.rev = strtoul(p, NULL, 16);

        /* now print out the info */
        printf("%02x:%02x.%x ", pci.bus, pci.device, pci.function);

        /* show only codes if requested */
        if(numeric)
        {
            printf("%04x: %04x:%04x (rev %02x)\n",
                    pci.base_class, pci.vendor, pci.device_id, pci.rev);
            //__asm__("xchg %%bx, %%bx"::);
            continue;
        }

        /* not numeric - lookup device name, etc. */
        
        pci_subclass = get_pci_subclass(pci.base_class, pci.sub_class);
        printf("%s (%s): ", pci_subclass->class->name, pci_subclass->name);

        pci_device = get_pci_device(pci.vendor, pci.device_id);
        printf("%s %s ", pci_device->vendor->name, pci_device->name);

        printf(" (rev %02u)\n", pci.rev);
        //__asm__("xchg %%bx, %%bx"::);
    }

    fclose(f);

    return 0;
}

