/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: usb.h
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
 *  \file usb.h
 *
 *  The Universal Serial Bus (USB) driver definitions.
 */

#ifndef KERNEL_USB_H
#define KERNEL_USB_H

/*
 * We construct USB device numbers as follows:
 *   - all devices have major devid of USB_CTRL_DEV_MAJ
 *   - bus control devices (/dev/usbN) have minor id from their bus number
 *   - endpoint devices (/dev/usbN.DD.EE) have 16-bit minor id as below
 *
 * +------------+-----------------------+---------------+
 * |   15-12    |         11-4          |      3-0      |
 * +------------+-----------------------+---------------+
 * | Bus number |      Device addr      | Endpoint addr |
 * +------------+-----------------------+---------------+
 */

#define USB_MAKE_DEVID(bus, dev, ep)    \
    TO_DEVID(USB_CTRL_DEV_MAJ,          \
             ((((bus) & 0x0f) << 12) | (((dev) & 0xff) << 4) | ((ep) & 0x0f)))

#define USB_DEVID_BUS(devid)        ((MINOR(devid) >> 12) & 0x0f)
#define USB_DEVID_DEVADDR(devid)    ((MINOR(devid) >> 4) & 0xff)
#define USB_DEVID_EPADDR(devid)     ((MINOR(devid) >> 0) & 0x0f)

/*
 * USB controller devices have a major of 189 (char)
 */
#define USB_CTRL_DEV_MAJ            189

/*
 * USB mass storage devices have a major of 180 (block)
 */
#define USB_MSD_DEV_MAJ             180

/*
 * Maximum number of USB disks we can handle:
 *     26 whole disks
 *      7 partitions per disk
 *    208 disks/partitions in total
 */
#define MAX_USB_MSD_DEVICES         208

// Max USB buses
#define MAX_USB_BUSES               8

// Max USB devices per host controller (it is 127 but use 128 to make 
// calculations easier)
#define MAX_DEV_PER_HC              128

// USB types
#define USB_TYPE_UHCI               0x00
#define USB_TYPE_OHCI               0x10
#define USB_TYPE_EHCI               0x20
#define USB_TYPE_XHCI               0x30
#define USB_TYPE_NOHCI              0x80
#define USB_TYPE_ANYHCI             0xFE

// USB speed
#define USB_SPEED_LOW               1
#define USB_SPEED_FULL              2
#define USB_SPEED_HIGH              3
#define USB_SPEED_SUPER             4

// Transfer types
#define USB_TRANSFER_BULK           0
#define USB_TRANSFER_CTRL           1
#define USB_TRANSFER_INTERRUPT      2
#define USB_TRANSFER_ISOCHRONOUS    3

// Transaction types
#define USB_TRANS_OUT               0
#define USB_TRANS_IN                1
#define USB_TRANS_SETUP             2

// Endpoint direction
#define USB_ENDPOINT_OUT            0
#define USB_ENDPOINT_IN             1
#define USB_ENDPOINT_BI             2

// Endpoint types
#define USB_ENDPOINT_CONTROL        0
#define USB_ENDPOINT_ISOCHRONOUS    1
#define USB_ENDPOINT_BULK           2
#define USB_ENDPOINT_INTERRUPT      3

#define BYTE1(x)            ((x) & 0xFF)
#define BYTE2(x)            (((x) >> 8) & 0xFF)
#define BYTE3(x)            (((x) >> 16) & 0xFF)
#define BYTE4(x)            (((x) >> 24) & 0xFF)

#ifndef MAX
#define	MAX(a, b)           (((a)>(b))?(a):(b))
#endif

#ifndef MIN
#define	MIN(a, b)           (((a)<(b))?(a):(b))
#endif

struct usb_dev_t;

struct usb_device_descriptor_t
{
    uint8_t  len;       // 18
    uint8_t  type;      // 1
    uint16_t bcd_usb;
    uint8_t  class;
    uint8_t  subclass;
    uint8_t  protocol;
    uint8_t  mps;       // max packet size -- 8/16/32/64
    uint16_t vendorid;
    uint16_t productid;
    uint16_t bcd_dev;
    uint8_t  manufacturer;
    uint8_t  product;
    uint8_t  serial;
    uint8_t  configs;   // configuration count
} __attribute__((packed));

struct usb_config_descriptor_t
{
    uint8_t  len;       // 9
    uint8_t  type;      // 2
    uint16_t totlen;
    uint8_t  interfaces;
    uint8_t  configval;
    uint8_t  config;
    uint8_t  attribs;
    uint8_t  maxpower;
} __attribute__((packed));

struct usb_interface_descriptor_t
{
    uint8_t  len;       // 9
    uint8_t  type;      // 4
    uint8_t  interfacenum;
    uint8_t  altsetting;
    uint8_t  endpoints;
    uint8_t  class;
    uint8_t  subclass;
    uint8_t  protocol;
    uint8_t  interface;
} __attribute__((packed));

struct usb_endpoint_descriptor_t
{
    uint8_t  len;       // 7
    uint8_t  type;      // 5
    uint8_t  addr;
    uint8_t  attribs;
    uint16_t mps;       // max packet size
    uint8_t  interval;
} __attribute__((packed));

struct usb_string_descriptor_t
{
    uint8_t  len;       // ?
    uint8_t  type;      // 3
    uint16_t langid[10];
} __attribute__((packed));

struct usb_unistring_descriptor_t
{
    uint8_t  len;       // 2 + n
    uint8_t  type;      // 3
    uint8_t  wch[30];
} __attribute__((packed));

struct usb_request_t
{
    uint8_t type;
    uint8_t req;
    uint8_t loval;
    uint8_t hival;
    uint16_t index;
    uint16_t len;
} __attribute__((packed));

struct usb_cmd_blk_wrapper_t
{
    uint32_t sig;           /**< Signature: 0x43425355 */
    uint32_t tag;           /**< Transaction unique identifier */
    uint32_t len;           /**< Length */
    uint8_t  flags;         /**< Direction: 0x00 to device; 0x80 to host */
    uint8_t  lun;           /**< Logical Unit Number */
    uint8_t  cblen;         /**< Command length */
    uint8_t  cmd[16];       /**< Command data */
} __attribute__((packed));

struct usb_transaction_t
{
    // for control transactions
    uint8_t type;
    uint8_t req;
    uint8_t hival;
    uint8_t loval;
    uint8_t index;

    // for all transactions
    uint32_t len;
    int toggle;
    void *data;

    // for in and out transactions
    void *buf;

    struct usb_dev_t *dev;      /**< back pointer to USB device */
    struct usb_transfer_t *transfer;
    volatile struct usb_transaction_t *next;
};

struct usb_endpoint_t
{
    uint8_t  direction;
    uint8_t  type;
    uint8_t  addr;
    uint8_t  interval;
    uint16_t mps;       // max packet size
    uint8_t  toggle;
    struct usb_endpoint_t *next;
};

struct usb_transfer_t
{
    uint8_t type;
    uint8_t freq;
    volatile int success;
    uint32_t pktsz;
    void *data;

    struct usb_dev_t *dev;      /**< back pointer to USB device */
    //uint32_t endpoint;
    struct usb_endpoint_t *endpoint;

    void (*callback)(void *);
    void *callback_arg;

    volatile struct usb_transaction_t *trans_head, *trans_tail;

    volatile struct usb_transfer_t *next_inttransfer;
};

struct usb_interface_t
{
    size_t bytes_per_sector; /**< bytes per sector */
    volatile uint32_t cur_tag;

    struct usb_endpoint_t *endpoint_interrupt;
    struct usb_endpoint_t *endpoint_in;
    struct usb_endpoint_t *endpoint_out;

    struct usb_dev_t *usb;
    struct usb_interface_descriptor_t desc;
    struct usb_interface_t *next;
};

struct usb_ops_t
{
    void (*setup_transfer)(struct usb_transfer_t *);
    void (*schedule_transfer)(struct usb_transfer_t *);
    void (*wait_transfer)(struct usb_transfer_t *);
    int (*poll_transfer)(struct usb_transfer_t *);
    void (*delete_transfer)(struct usb_transfer_t *);
    int (*setup_transaction)(struct usb_transaction_t *);
    void (*in_transaction)(struct usb_transaction_t *);
    void (*out_transaction)(struct usb_transaction_t *);
    void (*free_transaction_data)(volatile struct usb_transaction_t *);
    unsigned int (*get_next_addr)(void *);
    void (*free_addr)(void *, unsigned int);
};

struct usb_dev_t
{
    uint8_t type;
    uint8_t speed;
    uint8_t num, bus;
    uint8_t cur_config;
    unsigned int port;
    void *priv;
    struct usb_endpoint_t *endpoints;
    struct usb_interface_t *interfaces;
    struct kernel_mutex_t lock;

    uint16_t spec;
    uint8_t  class;
    uint8_t  subclass;
    uint8_t  protocol;
    uint16_t vendor;
    uint16_t product;
    uint16_t release;
    uint8_t  manufacturerid;
    uint8_t  productid;
    uint8_t  serialid;
    uint8_t  configs;   // configuration count

    // mass storage devices and HID
    char product_name[32];
    char serial[32];

    // function pointers
    struct usb_ops_t *ops;
};


/****************************************
 * Functions defined in usb.c
 ****************************************/

/**
 * @brief Initialise a USB device.
 *
 * Initialise the given PCI device, which should be a Universal Serial Bus
 * (USB) capable device.
 *
 * @param   pci     PCI structure of the USB device to initialize
 *
 * @return  zero on success, -(errno) on failure.
 */
int usb_init(struct pci_dev_t *pci);

struct usb_dev_t *usb_create_dev(uint8_t bus, unsigned int port, uint8_t speed);
void usb_destroy_dev(struct usb_dev_t *dev);
int usb_setup_device(struct usb_dev_t *dev, unsigned int addr);
void usb_clear_feature_halt(struct usb_dev_t *dev, struct usb_endpoint_t *endpoint);

uint8_t usb_get_config(struct usb_dev_t *dev);
int usb_set_config(struct usb_dev_t *dev, uint32_t config);
uint8_t usb_get_iface(struct usb_dev_t *dev, uint16_t iface);
void usb_set_iface(struct usb_dev_t *dev, uint16_t iface, uint8_t alt_iface);

void usb_delete_transfer(struct usb_transfer_t *transfer);
void usb_setup_transfer(struct usb_dev_t *dev, struct usb_endpoint_t *endpoint,
                        struct usb_transfer_t *transfer, uint8_t type);
void usb_schedule_transfer(struct usb_transfer_t *transfer);
void usb_wait_transfer(struct usb_transfer_t *transfer);

uint8_t usb_setup_transaction(struct usb_transfer_t *transfer, 
                              uint8_t type, uint8_t req,
                              uint8_t hival, uint8_t loval,
                              uint16_t index, uint16_t len);

void usb_in_transaction(struct usb_transfer_t *transfer, 
                        int ctrl_handshake, void *buf, size_t len);

void usb_out_transaction(struct usb_transfer_t *transfer, 
                         int ctrl_handshake, void *buf, size_t len);

void usb_schedule_inttransfer(struct usb_dev_t *usb, struct usb_endpoint_t *endpoint,
                              struct usb_transfer_t *transfer, 
                              void *buf, size_t bufsz,
                              void (*callback)(void *), void *callback_arg,
                              uint8_t freq);
void remove_interrupt_transfer(struct usb_transfer_t *transfer);

int usb_ctrl_in(struct usb_dev_t *dev, void *buf,
                uint8_t type, uint8_t req,
                uint8_t hival, uint8_t loval,
                uint16_t index, uint16_t len);

int usb_ctrl_out(struct usb_dev_t *dev, void *buf,
                 uint8_t type, uint8_t req,
                 uint8_t hival, uint8_t loval,
                 uint16_t index, uint16_t len);

int usb_ctrl_set(struct usb_dev_t *dev,
                 uint8_t type, uint8_t req,
                 uint8_t hival, uint8_t loval, uint16_t index);

/****************************************
 * Functions defined in usb_msd.c
 ****************************************/

int init_msd(struct usb_interface_t *iface);
long usb_msd_strategy(struct disk_req_t *req);
long usb_msd_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel);
void usb_msd_remove(struct usb_interface_t *iface);
int usb_msd_read_sector_direct(void *__dev, uintptr_t phys_buf, uintptr_t virt_buf, uint32_t lba);

/****************************************
 * Functions defined in usb_ioctl.c
 ****************************************/

long usb_ctrl_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel);
ssize_t usb_ctrl_read(struct file_t *f, off_t *pos,
                      unsigned char *buf, size_t count, int kernel);
ssize_t usb_ctrl_write(struct file_t *f, off_t *pos,
                       unsigned char *buf, size_t count, int kernel);

#endif      /* KERNEL_USB_H */
