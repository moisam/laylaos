/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: lsusb.c
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
 *  \file lsusb.c
 *
 *  A simple program to list USB devices on the system.
 *
 *  This aims to mirror Linux lsusb functionality as close as possible. 
 *  However, it only offers a basic subset of what Linux lsusb can do, i.e.
 *  list USB devices and print basic info about each device.
 *
 *  More functionality will hopefully be implemented in the future, such as:
 *    - print more detailed info about each device
 *    - show devices as a tree (the -t option)
 *    - compile lsusb-lib.c as a standalone .so library and implement device
 *      class and subclass name caching to speed lookups
 *
 *  @See: https://github.com/gregkh/usbutils/blob/master/lsusb.c
 */

#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <libusb.h>
#include "lsusb-lib.h"

char *ver = "1.0";

int verbose = 0;
//int show_tree = 0;
int bus = -1;
int devnum = -1;
int vendor = -1;
int product = -1;
char *devarg = NULL;


// Endpoint types
static char *ep_attr_type[] = { "Control", "Isochronous", "Bulk", "Interrupt" };

// Endpoint sync types
static char *ep_attr_sync[] = { "None", "Async", "Adaptive", "Sync" };

// Endpoint usage types
static char *ep_attr_usage[] = { "Data", "Feedback", "Implicit Feedback Data", "Reserved" };


void parse_line_args(int argc, char **argv) 
{
    int c;
    char *p;

    static struct option long_options[] =
    {
        {"device",  required_argument, 0, 'D'},
        {"help",    no_argument,       0, 'h'},
        {"id",      required_argument, 0, 'd'},
        {"show",    required_argument, 0, 's'},
        //{"tree",    no_argument,       0, 't'},
        {"version", no_argument,       0, 'V'},
        {"verbose", no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    while((c = getopt_long(argc, argv, "D:Vd:hs:v", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'D':
                devarg = optarg;
                break;

            case 'd':
                p = strchr(optarg, ':');

                // there must be a vendor and a colon, with optional product id
                if(!p)
                {
                    fprintf(stderr, "%s: invalid argument: %s\n", argv[0], optarg);
                    exit(EXIT_FAILURE);
                }

                *p++ = '\0';

                if(*optarg)
                {
                    vendor = strtoul(optarg, NULL, 16);
                }

                if(*p)
                {
                    product = strtoul(p, NULL, 16);
                }
                break;

            case 's':
                p = strchr(optarg, ':');

                if(p)
                {
                    *p++ = '\0';

                    // both parts before and after the colon are optional
                    if(*optarg)
                    {
                        bus = strtoul(optarg, NULL, 10);
                    }

                    if(*p)
                    {
                        devnum = strtoul(p, NULL, 10);
                    }
                }
                else
                {
                    if(*optarg)
                    {
                        bus = strtoul(optarg, NULL, 10);
                    }
                }
                break;

            /*
            case 't':
                show_tree = 1;
                break;
            */

            case 'v':
                verbose++;
                break;

            case 'V':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                printf("lsusb utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options]\n\n"
                       "Options:\n"
                       "  -d, --id vendor:[product]     Show only devices with the given\n"
                       "                                  vendor & product id (hexadecimal)\n"
                       "  -D, --device [[/dev/]usb]bus.addr\n"
                       "                                Print only the given device\n"
                       "  -h, --help                    Show this help and exit\n"
                       "  -s, --show [[bus]:][devnum]   Show only devices with the given\n"
                       "                                  bus & device numbers (decimal)\n"
                       "  -t, --tree                    Print device hierarchy as a tree\n"
                       "  -v, --verbose                 Print verbose output\n"
                       "  -V, --version                 Print version and exit\n"
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


static void get_vendor_and_product(uint16_t vendorid, uint16_t productid, 
                                   struct usb_vendor_t **usbvendor,
                                   struct usb_device_t **usbdev)
{
    // try to get device & vendor ids
    *usbdev = get_usb_device(vendorid, productid);

    // if we fail to find the device, at least try to find the vendor
    if(strcmp((*usbdev)->name, "Unknown") == 0)
    {
        *usbvendor = get_usb_vendor(vendorid);
    }
    else
    {
        *usbvendor = (*usbdev)->vendor;
    }
}


static void get_proto_class_subclass(uint8_t c, uint8_t sc, uint8_t p,
                                     struct usb_class_t **class, 
                                     struct usb_subclass_t **subclass, 
                                     struct usb_protocol_t **proto)
{
    *proto = get_usb_protocol(c, sc, p);

    // if we fail to find the protocol, at least try to find the class/subclass
    if(strcmp((*proto)->name, "Unknown") == 0)
    {
        *subclass = get_usb_subclass(c, sc);
    }
    else
    {
        *subclass = (*proto)->subclass;
    }

    if(strcmp((*subclass)->name, "Unknown") == 0)
    {
        *class = get_usb_class(c);
    }
    else
    {
        *class = (*subclass)->class;
    }
}


static void __print_endpoint(const struct libusb_endpoint_descriptor *ep)
{
    static char *bytes[] = { "1x", "2x", "3x", "??" };

    printf("      Endpoint Descriptor:\n");
    printf("        bLength:            %5u\n", ep->bLength);
    printf("        bDescriptorType:    %5u\n", ep->bDescriptorType);

    printf("        bEndpointAddress:   0x%02x  EP %u %s\n", 
            ep->bEndpointAddress, 
            (ep->bEndpointAddress & 0x0f),
            (ep->bEndpointAddress & 0x80) ? "IN" : "OUT");

    printf("        bmAttributes:       %5u\n", ep->bmAttributes);
    printf("          Transfer Type:      %s\n", ep_attr_type[ep->bmAttributes & 3]);
    printf("          Sync Type:          %s\n", ep_attr_sync[(ep->bmAttributes >> 2) & 3]);
    printf("          Usage Type:         %s\n", ep_attr_usage[(ep->bmAttributes >> 4) & 3]);

    printf("        wMaxPacketSize:     0x%04x  %s %d bytes\n", 
            ep->wMaxPacketSize,
            bytes[(ep->wMaxPacketSize >> 11) & 3],
            ep->wMaxPacketSize & 0x7ff);

    printf("        bInterval:          %5u\n", ep->bInterval);
}


static void __print_interface(const struct libusb_interface *iface)
{
    int i, j;
    const struct libusb_interface_descriptor *idesc;
    struct usb_class_t *class;
    struct usb_subclass_t *subclass;
    struct usb_protocol_t *proto;

    for(i = 0; i < iface->num_altsetting; i++)
    {
        idesc = &iface->altsetting[i];

        get_proto_class_subclass(idesc->bInterfaceClass, 
                                 idesc->bInterfaceSubClass, 
                                 idesc->bInterfaceProtocol,
                                 &class, &subclass, &proto);

        printf("    Interface Descriptor:\n");
        printf("      bLength:            %5u\n", idesc->bLength);
        printf("      bDescriptorType:    %5u\n", idesc->bDescriptorType);
        printf("      bInterfaceNumber:   %5u\n", idesc->bInterfaceNumber);
        printf("      bAlternateSetting:  %5u\n", idesc->bAlternateSetting);
        printf("      bNumEndpoints:      %5u\n", idesc->bNumEndpoints);
        printf("      bInterfaceClass:    %5u %s\n", idesc->bInterfaceClass, class->name);
        printf("      bInterfaceSubClass: %5u %s\n", idesc->bInterfaceSubClass, subclass->name);
        printf("      bInterfaceProtocol: %5u %s\n", idesc->bInterfaceProtocol, proto->name);
        printf("      iInterface:         %5u\n", idesc->iInterface);

        for(j = 0; j < idesc->bNumEndpoints; j++)
        {
            __print_endpoint(&idesc->endpoint[j]);
        }
    }
}


static void __print_config(libusb_device_handle *udev, 
                           struct libusb_config_descriptor *config,
                           unsigned int speed)
{
    int i;

    printf("  Configuration Descriptor:\n");
    printf("    bLength:              %5u\n", config->bLength);
    printf("    bDescriptorType:      %5u\n", config->bDescriptorType);
    printf("    wTotalLength:         %5u\n", config->wTotalLength);
    printf("    bNumInterfaces:       %5u\n", config->bNumInterfaces);
    printf("    bConfigurationValue:  %5u\n", config->bConfigurationValue);
    printf("    iConfiguration:       %5u\n", config->iConfiguration);
    printf("    bmAttributes:          0x%02x\n", config->bmAttributes);
    printf("      %s Powered\n", (config->bmAttributes & 0x40) ? "Self" : "Bus");

    if(config->bmAttributes & 0x20)
    {
        printf("      Remote Wakeup\n");
    }

    if(config->bmAttributes & 0x10)
    {
        printf("      Battery Powered\n");
    }

    printf("    MaxPower:             %5umA\n", config->MaxPower * (speed >= 0x0300 ? 8 : 2));

    for(i = 0; i < config->bNumInterfaces; i++)
    {
        __print_interface(&config->interface[i]);
    }
}


static void __print_dev(char *myname, libusb_device *dev,
                        struct usb_vendor_t *__usbvendor,
                        struct usb_device_t *__usbdev)
{
    libusb_device_handle *udev = NULL;
    struct usb_device_t *usbdev;
    struct usb_vendor_t *usbvendor;
    struct usb_protocol_t *proto;
    struct usb_class_t *class;
    struct usb_subclass_t *subclass;
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config = NULL;
    int res;

    if(libusb_open(dev, &udev))
    {
        fprintf(stderr, "%s: libusb failed to open device\n", myname);
    }

    libusb_get_device_descriptor(dev, &desc);

    if(__usbvendor && __usbdev)
    {
        usbdev = __usbdev;
        usbvendor = __usbvendor;
    }
    else
    {
        get_vendor_and_product(desc.idVendor, desc.idProduct, &usbvendor, &usbdev);
    }

    get_proto_class_subclass(desc.bDeviceClass, desc.bDeviceSubClass, 
                             desc.bDeviceProtocol,
                             &class, &subclass, &proto);

    res = libusb_get_device_speed(dev);

    if(res == LIBUSB_SPEED_LOW)
    {
        printf("Negotitated speed: %s\n", "Low Speed (1Mbps)");
    }
    else if(res == LIBUSB_SPEED_FULL)
    {
        printf("Negotitated speed: %s\n", "Full Speed (12Mbps)");
    }
    else if(res == LIBUSB_SPEED_HIGH)
    {
        printf("Negotitated speed: %s\n", "High Speed (480Mbps)");
    }
    else if(res == LIBUSB_SPEED_SUPER)
    {
        printf("Negotitated speed: %s\n", "SuperSpeed (5Gbps)");
    }
    else if(res == LIBUSB_SPEED_SUPER_PLUS)
    {
        printf("Negotitated speed: %s\n", "SuperSpeed+ (10Gbps)");
    }
    else if(res == LIBUSB_SPEED_SUPER_PLUS_X2)
    {
        printf("Negotitated speed: %s\n", "SuperSpeed++ (20Gbps)");
    }
    else
    {
        printf("Negotitated speed: %s\n", "Unknown");
    }

    printf("Device Descriptor:\n");
    printf("  bLength:            %5u\n", desc.bLength);
    printf("  bDescriptorType:    %5u\n", desc.bDescriptorType);
    printf("  bcdUSB:             %2x.%02x\n", desc.bcdUSB >> 8, desc.bcdUSB & 0xff);
    printf("  bDeviceClass:       %5u %s\n", desc.bDeviceClass, class->name);
    printf("  bDeviceSubClass:    %5u %s\n", desc.bDeviceSubClass, subclass->name);
    printf("  bDeviceProtocol:    %5u %s\n", desc.bDeviceProtocol, proto->name);
    printf("  bMaxPacketSize0:    %5u\n", desc.bMaxPacketSize0);
    printf("  idVendor:           0x%04x %s\n", desc.idVendor, usbvendor->name);
    printf("  idProduct:          0x%04x %s\n", desc.idProduct, usbdev->name);
    printf("  bcdDevice:          %2x.%02x\n", desc.bcdDevice >> 8, desc.bcdDevice & 0xff);
    printf("  iManufacturer:      %5u\n", desc.iManufacturer);
    printf("  iProduct:           %5u\n", desc.iProduct);
    printf("  iSerial:            %5u\n", desc.iSerialNumber);
    printf("  bNumConfigurations: %5u\n", desc.bNumConfigurations);

    if(desc.bNumConfigurations)
    {
        for(res = 0; res < desc.bNumConfigurations; res++)
        {
            if(libusb_get_config_descriptor(dev, res, &config))
            {
                fprintf(stderr, "%s: libusb failed to get config descriptor %d\n", myname, res);
            }
            else if(config)
            {
                __print_config(udev, config, desc.bcdUSB);
                libusb_free_config_descriptor(config);
            }

            config = NULL;
        }
    }

    if(udev)
    {
        /*
         * TODO: dump USB hubs
         *       print device debug and status desc
         */
        libusb_close(udev);
    }
}


int print_dev(char *myname, libusb_context *ctx, char *path)
{
    libusb_device **devlist = NULL;
    libusb_device *dev = NULL;
    struct libusb_device_descriptor desc;
    struct usb_device_t *usbdev;
    struct usb_vendor_t *usbvendor;
    char devpath[PATH_MAX], tmp[PATH_MAX];
    ssize_t n, i;
    uint8_t bus, devnum;

    if(*path == '/')
    {
        strcpy(devpath, path);
    }
    else if(strncmp(path, "usb", 3) == 0)
    {
        sprintf(devpath, "/dev/%s", path);
    }
    else
    {
        sprintf(devpath, "/dev/usb%s", path);
    }

    n = libusb_get_device_list(ctx, &devlist);

    for(i = 0; i < n; i++)
    {
        bus = libusb_get_bus_number(devlist[i]);
        devnum = libusb_get_device_address(devlist[i]);
        sprintf(tmp, "/dev/usb%d.%d", bus, devnum);

        if(strcmp(tmp, devpath) == 0)
        {
            dev = devlist[i];
            break;
        }
    }

    libusb_free_device_list(devlist, 1);

    if(!dev)
    {
        fprintf(stderr, "%s: failed to open device: %s\n", myname, path);
        return EXIT_FAILURE;
    }

    libusb_get_device_descriptor(dev, &desc);
    get_vendor_and_product(desc.idVendor, desc.idProduct, &usbvendor, &usbdev);

    printf("Device: ID %04x:%04x %s %s\n",
            desc.idVendor, desc.idProduct, usbvendor->name, usbdev->name);

    __print_dev(myname, dev, usbvendor, usbdev);

    return EXIT_SUCCESS;
}


int print_all(char *myname, libusb_context *ctx, 
              int bus, int devnum, int vendor, int product)
{
    libusb_device **devlist = NULL;
    libusb_device *dev;
    struct libusb_device_descriptor desc;
    struct usb_device_t *usbdev;
    struct usb_vendor_t *usbvendor;
    ssize_t n, i;
    uint8_t dbus, ddev;

    if((n = libusb_get_device_list(ctx, &devlist)) < 0)
    {
        return EXIT_FAILURE;
    }

    for(i = 0; i < n; i++)
    {
        dev = devlist[i];
        dbus = libusb_get_bus_number(dev);
        ddev = libusb_get_device_address(dev);

        // search for specific device and/or bus if given on the commandline
        if((bus != -1 && bus != dbus) || (devnum != -1 && devnum != ddev))
        {
            continue;
        }

        libusb_get_device_descriptor(dev, &desc);

        // search for specific vendor and/or product if given on the commandline
        if((vendor != -1 && vendor != desc.idVendor) || 
           (product != -1 && product != desc.idProduct))
        {
            continue;
        }

        get_vendor_and_product(desc.idVendor, desc.idProduct, &usbvendor, &usbdev);

        if(verbose)
        {
            printf("\n");
        }

        printf("Bus %03u Device %03u: ID %04x:%04x %s %s\n",
                dbus, ddev, desc.idVendor, desc.idProduct, usbvendor->name, usbdev->name);

        if(verbose)
        {
            __print_dev(myname, dev, usbvendor, usbdev);
        }
    }

    libusb_free_device_list(devlist, 1);

    return EXIT_SUCCESS;
}


int main(int argc, char **argv)
{
    int res = 0;
    libusb_context *ctx;

    parse_line_args(argc, argv);

    if(usblib_init() != 0)
    {
        fprintf(stderr, "%s: failed to init usb database: %s\n",
                argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }

    if((res = libusb_init(&ctx)))
    {
        fprintf(stderr, "%s: failed to init libusb (err %d)\n", argv[0], res);
        exit(EXIT_FAILURE);
    }

    res = devarg ? print_dev(argv[0], ctx, devarg) : 
                   print_all(argv[0], ctx, bus, devnum, vendor, product);

    libusb_exit(ctx);

    return res;
}

