/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb_uhci.h
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
 *  \file usb_uhci.h
 *
 *  Universal Host Controller Interface (UHCI) driver definitions.
 */

#ifndef KERNEL_USB_UHCI_H
#define KERNEL_USB_UHCI_H

#include <sys/types.h>
#include <kernel/mutex.h>

#define UHCI_TDBUF_SIZE      (1024)
#define UHCI_TDBUF_POOL_SIZE (PAGE_SIZE * 16)
#define UHCI_MAX_QH          (PAGE_SIZE / sizeof(struct uhci_qh_t))
#define UHCI_MAX_TD          (PAGE_SIZE / sizeof(struct uhci_td_t))
#define UHCI_MAX_TDBUF       (UHCI_TDBUF_POOL_SIZE / UHCI_TDBUF_SIZE)


// UHCI register offsets
#define UHCI_REG_CMD        0x00        /**< USB Command - Read/Write */
#define UHCI_REG_STS        0x02        /**< USB Status - Read/Write clear */
#define UHCI_REG_INT        0x04        /**< USB Interrupt Enable - Read/Write */
#define UHCI_REG_FRNUM      0x06        /**< Frame Numer - Read/Write */
#define UHCI_REG_FRBASEADDR 0x08        /**< Frame List Base Address - Read/Write */
#define UHCI_REG_SOFMOD     0x0C        /**< Start of Frame Modify - Read/Write */
#define UHCI_REG_PORTSC1    0x10        /**< Port 1 Status/Control - Read/Write clear */
#define UHCI_REG_PORTSC2    0x12        /**< Port 2 Status/Control - Read/Write clear */

// Command register bits
#define UHCI_CMD_RS         (1 << 0)    /**< Run/stop: 1=run; 0=stop */
#define UHCI_CMD_HCRST      (1 << 1)    /**< Host controller reset */
#define UHCI_CMD_GRST       (1 << 2)    /**< Global reset */
#define UHCI_CMD_EGSM       (1 << 3)    /**< Enter global suspend mode */
#define UHCI_CMD_FGR        (1 << 4)    /**< Force global resume */
#define UHCI_CMD_SWDBG      (1 << 5)    /**< Software debug: 1=debug; 0=normal */
#define UHCI_CMD_CF         (1 << 6)    /**< Configure flag */
#define UHCI_CMD_MAXP       (1 << 7)    /**< Max packet: 1=64 bytes; 0=32 bytes */

// Port scan register bits
#define UHCI_PORTSC_CS      (1 << 0)    /**< 1=device present on port; 0=no device present */
#define UHCI_PORTSC_CS_CHG  (1 << 1)    /**< 1=change in current connect status;
                                             0=no change */
#define UHCI_PORTSC_EN      (1 << 2)    /**< 1=port enabled; 0=port disabled */
#define UHCI_PORTSC_EN_CHG  (1 << 3)    /**< 1=port enabled/disabled status changed;
                                             0=no change */
#define UHCI_PORTSC_RES_DET (1 << 6)    /**< 1=resume detected on port; 0=no resume detected */
#define UHCI_PORTSC_VALID   (1 << 7)    /**< Reserved, always reads as 1 */
#define UHCI_PORTSC_LOSPEED (1 << 8)    /**< 1=low speed device attached; 0=fullspeed device */
#define UHCI_PORTSC_RST     (1 << 9)    /**< 1=port in reset; 0=port not */
#define UHCI_PORTSC_SUSPEND (1 << 12)   /**< 1=port in suspend state; 0=port not */

// Interrupt enable register bits
#define UHCI_INT_MASK       (0xF)
#define UHCI_INT_TMOUT_EN   (1 << 0)    /**< Timeout/CRC interrupt enable */
#define UHCI_INT_RES_EN     (1 << 1)    /**< Resume interrupt enable */
#define UHCI_INT_IOC_EN     (1 << 2)    /**< Interrupt on complete enable */
#define UHCI_INT_SHRTPKT_EN (1 << 3)    /**< Short packet interrupt enable */

// Interrupt status register bits
#define UHCI_STS_MASK       (0x3F)
#define UHCI_STS_INT        (1 << 0)    /**< USB interrupt */
#define UHCI_STS_ERR        (1 << 1)    /**< USB error interrupt */
#define UHCI_STS_RESDET     (1 << 2)    /**< Resume detected */
#define UHCI_STS_HOSTERR    (1 << 3)    /**< Host system error */
#define UHCI_STS_PROCERR    (1 << 4)    /**< Host controller process error */
#define UHCI_STS_HCHALTED   (1 << 5)    /**< Host controller halted */

// Legacy support bits (from PCI register 0xC0)
#define UHCI_PCI_LEGACY_PIRQ    0x2000  // IRQ carried on PCI
#define UHCI_PCI_LEGACY_NOCHG   0x5040  // RO bits
#define UHCI_PCI_LEGACY_STS     0x8F00  // Status bits cleared by writing 1's


struct uhci_dev_t;


/*
 * Transfer Descriptors describe transactions. They must be aligned on a 
 * 16-byte boundary. All 4 types of USB transfers share the same TD format
 * (See UHCI spec, page 21):
 *
 * 31  30|29 |28  27|26|25 |24 |23    21|20|19|18  16|15|14|     11|10   8|7  4|3|2 |1|0|
 * +-----------------------+-------------------------+--------------------+-------------+
 * |                               Link Pointer                                |0|Vf|Q|T|
 * +-----------------------+-------------------------+--------------------+-------------+
 * |  R  |SPD| CERR |LS|ISO|IOC|       Status        |      R      |       ActLen       |
 * +-----------------------+-------------------------+--------------------+-------------+
 * |               MaxLen               |R |D |   EndPt   |Device Address |     PID     |
 * +-----------------------+-------------------------+--------------------+-------------+
 * |                                   Buffer Pointer                                   |
 * +-----------------------+-------------------------+--------------------+-------------+
 */
struct uhci_td_t
{
    // DWORD 0 -- TD link pointer
    volatile uint32_t next; /**< 31:4: Link pointer to next TD/QH
                                 3: Reserved
                                 2: Depth/Breadth Select (Vf):
                                    1=Depth first (process next transaction in queue)
                                    0=Breadth first (process next queue)
                                 1: QH/TD Select (Q):
                                    1=QH
                                    0=TD
                                 0: Terminate (T):
                                    1=Link pointer field not valid
                                    0=Link pointer field valid */

    // DWORD 1 -- TD control and status
    union
    {
        struct
        {
            uint32_t len        : 11;   /**< 10:0 Actual length (number of bytes transferred) */
            uint32_t res1       : 5;    /**< 15:11: Reserved */
            uint32_t res2       : 1;    /**< 16: Reserved */
            uint32_t bserr      : 1;    /**< 17: 1=bitsuff error (host controller 
                                                   received more than 6 ones in a row) */
            uint32_t touterr    : 1;    /**< 18: 1=CRC/Time-out error */
            uint32_t nakrcv     : 1;    /**< 19: 1=NAK received */
            uint32_t babble     : 1;    /**< 20: 1=babble detected */
            uint32_t buferr     : 1;    /**< 21: 1=data buffer error (overrun/underrun) */
            uint32_t stall      : 1;    /**< 22: 1=stalled (serios error occurred) */
            uint32_t active     : 1;    /**< 23: 1=active (executing) */
            uint32_t intoncomp  : 1;    /**< 24: Interrupt on Complete; 1=issue */
            uint32_t isochrsel  : 1;    /**< 25: 1=isochronous transfer descriptor;
                                                 0=non-isochronous transfer */
            uint32_t lospeed    : 1;    /**< 26: 1=low speed device; 0=fullspeed device */
            uint32_t errcnt     : 2;    /**< 28:27: Error count */
            uint32_t shortpkt   : 1;    /**< 29: Short packet detect: 1=enable; 0=disable */
            uint32_t res3       : 2;    /**< 31:30 Reserved */
        };

        volatile uint32_t dword1;
    };

    // DWORD 2 -- TD token
    union
    {
        struct
        {
#define UHCI_TD_PKTID_SETUP     0x2D
#define UHCI_TD_PKTID_IN        0x69
#define UHCI_TD_PKTID_OUT       0xE1
            uint32_t pktid      : 8;    /**< 7:0: Packet identification; IN/OUT/SETUP only */

            uint32_t devaddr    : 7;    /**< 14:8: Device address */
            uint32_t endpoint   : 4;    /**< 18:15: Endpoint (upto 16) */
            uint32_t toggle     : 1;    /**< 19: Data toggle: 0=DATA0; 1=DATA1 */
            uint32_t res4       : 1;    /**< 20: Reserved */
            uint32_t maxlen     : 11;   /**< 31:21: Maximum length:
                                               0x000     1 byte
                                               0x001     2 bytes
                                               ...       ...
                                               0x3FE     1023 bytes
                                               0x3FF     1024 bytes
                                               ...       ...
                                               0x4FF     1280 bytes
                                               0x500-0x7FE are illegal
                                               0x7FF     0 bytes (null data packet) */
        };

        volatile uint32_t dword2;
    };

    // DWORD 3 -- TD buffer pointer
    volatile uint32_t buf;               /**< Physical buffer pointer */

    // DWORD 4-7 -- Reserved for software
    volatile void *virtbuf;             /**< Virtual buffer pointer */

#ifndef __x86_64__
    uint32_t dword6, dword7;
#endif

    uint32_t alloced;               /**< 1 if this TD is alloc'd, 0 if free */
    char pad[4];                    /**< padding to 16-byte boundary */
} __attribute__((packed));


/*
 * Queue heads support Control, Bulk, and Interrupt transfer. They must be
 *  aligned on a 16-byte boundary.
 */
struct uhci_qh_t
{
    volatile uint32_t next;     /**< 31:4: Queue Head Link Pointer (link to next queue head)
                                     3:2 Reserved
                                     1: QH/TD Select (Q):
                                        1=QH
                                        0=TD
                                     0: Terminate (T):
                                        1=Link pointer field not valid (last pointer)
                                        0=Link pointer field valid */
    volatile uint32_t transfer; /**< 31:4: Queue Element Link Pointer (next TD/QH in this queue) 
                                     3: Reserved
                                     2: Reserved
                                     1: QH/TD Select (Q):
                                        1=QH
                                        0=TD
                                     0: Terminate (T):
                                        1=Terminate (no valid queue entries) */
    //struct uhci_td_t *first, *last; /**< Not part of the UHCI spec, used for housekeeping */
    uintptr_t self_phys;            /**< physical address of this QH */
    struct uhci_qh_t *nextvirt;     /**< virtual address of next QH */
    uint8_t alloced;                /**< 1 if this QH is alloc'd, 0 if free */
    uint8_t freq;                   /**< transfer frequency */
    char pad[6];                    /**< padding to 16-byte boundary */
} __attribute__((packed));


/*
 * A frame list has 1024 entries and represents a window in time. It must be
 * aligned on a 4 kilobyte boundary.
 * Each entry corresponds to a frame (1 ms), and refers to transactions
 * to be performed during that frame.
 */
struct uhci_framelist_t
{
    uint32_t fr[1024];  /**< physical address of first data object in each frame */
                        /* Bit 0 (Terminate):
                               1=Empty frame (pointer is invalid)
                               0=Pointer is valid (points to QH or TD)
                           Bit 1 (QH/TD Select):
                               1=QH (queue head)
                               0=TD (transfer descriptor)
                         */
};


struct uhci_transaction_t
{
    struct uhci_td_t *tdvirt;   /**< TD virtual address */
    void *tdphys;               /**< TD physical address */
    void *tdbuf;                /**< TD buffer virtual address */
    void *inbuf;                /**< incoming buffer (maybe NULL) */
    size_t inlen;               /**< size of inbuf */
};


struct uhci_port_t
{
#define UHCI_PORT_FLAG_CONNECTED        0x01
#define UHCI_PORT_FLAG_LOSPEED          0x02
    volatile int flags;         /**< port flags */

    unsigned int port;          /**< port number */
    struct uhci_dev_t *uhci;    /**< back pointer to UHCI device */
    struct usb_dev_t *usb;      /**< if non-NULL, pointer to the USB device
                                     currently connected on this port */
};


struct uhci_dev_t
{
#define UHCI_FLAG_RUN                   0x01
#define UHCI_FLAG_PORTENABLED           0x02
    volatile int flags; /**< device flags */

    dev_t devid;        /**< device id */
    int mmio;           /**< device uses memory-mapped I/O (MMIO) */
    uint8_t port_count; /**< number of root ports */
    volatile struct uhci_port_t *ports;  /**< root ports */

    uintptr_t iobase,   /**< I/O space base address */
              iosize;   /**< I/O space size */

    struct uhci_framelist_t *framelist;  /**< frame list virtual address */
    void *framelist_phys;       /**< frame list physical address */

    struct uhci_qh_t *qhpool;   /**< queue head pool */
    void *qhpool_phys;          /**< queue head pool physical address */

    struct uhci_td_t *tdpool;   /**< TD pool */
    uintptr_t tdpool_phys;      /**< TD pool physical address */

    uintptr_t tdbufpool;        /**< TD buffer pool */
    uintptr_t tdbufpool_phys;   /**< TD buffer pool physical address */
    uint8_t tdbuf_used[UHCI_MAX_TDBUF]; /**< TD buffer use bitmap */

    volatile uint32_t addr_bitmap[MAX_DEV_PER_HC / sizeof(uint32_t)];
                                /**< bitmap of used addresses on this bus */

    //struct uhci_qh_t *qh;       /**< main queue head virtual address */
    struct uhci_qh_t *qh_32ms;    /**< 32ms queue head virtual address */
    struct uhci_qh_t *qh_16ms;    /**< 16ms queue head virtual address */
    struct uhci_qh_t *qh_8ms;     /**< 8ms queue head virtual address */
    struct uhci_qh_t *qh_4ms;     /**< 4ms queue head virtual address */
    struct uhci_qh_t *qh_2ms;     /**< 2ms queue head virtual address */
    struct uhci_qh_t *qh_1ms;     /**< 1ms queue head virtual address */
    struct kernel_mutex_t qh_lock;/**< queue had lock */

    struct pci_dev_t *pci;      /**< back pointer to PCI device */
    struct uhci_dev_t *next;    /**< next UHCI device */
};

/****************************
 * Function prototypes
 ****************************/

int uhci_install(struct pci_dev_t *pci, struct pci_bar_t *bar);
void uhci_poll(void);
struct usb_dev_t *uhci_get_dev_struct(struct pci_dev_t *bus, uint8_t num);

#endif      /* KERNEL_USB_UHCI_H */
