/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb_ehci.h
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
 *  \file usb_ehci.h
 *
 *  Enhanced Host Controller Interface (EHCI) driver definitions.
 */

#ifndef KERNEL_USB_EHCI_H
#define KERNEL_USB_EHCI_H

#include <sys/types.h>
#include <kernel/mutex.h>

#define EHCI_TDBUF_SIZE         (PAGE_SIZE)
#define EHCI_TDBUF_POOL_SIZE    (PAGE_SIZE * 32)
#define EHCI_MAX_QH             (PAGE_SIZE / sizeof(struct ehci_qh_t))
#define EHCI_MAX_TD             (PAGE_SIZE / sizeof(struct ehci_td_t))
#define EHCI_MAX_TDBUF          (EHCI_TDBUF_POOL_SIZE / EHCI_TDBUF_SIZE)

// EHCI register offsets
#define EHCI_REG_HCCAP          0x00            /**< Host Controller Capabilities Registers */
#define EHCI_REG_HCOP           (ehci->caplen)  /**< Host Controller Operational Registers */

// Capabilities Register offsets
#define HCCAP_CAPLENGTH             0x00            /**< Capability Register length */
#define HCCAP_HCIVERSION            0x02            /**< Interface version number */
#define HCCAP_HCSPARAMS             0x04            /**< Structural parameters */
#define HCCAP_HCCPARAMS             0x08            /**< Capability parameters */

// Operational Register offets
#define HCOP_USBCMD                 0x00            /**< USB command */
#define HCOP_USBSTS                 0x04            /**< USB status */
#define HCOP_USBINTR                0x08            /**< USB interrupt enable */
#define HCOP_FRINDEX                0x0C            /**< USB frame index */
#define HCOP_CTRLDSSEGMENT          0x10            /**< 4G segment selector */
#define HCOP_PERIODICLISTBASE       0x14            /**< Frame list base address */
#define HCOP_ASYNCLISTADDR          0x18            /**< Next asynchoronous list address */
#define HCOP_CONFIGFLAG             0x40            /**< Configured flag register */
#define HCOP_PORTSC                 0x44            /**< Port status/control */

// Command register bits
#define ECHI_USBCMD_FRLIST_MASK     0x0C            /**< Framelist size mask */
#define ECHI_USBCMD_INTTHRSHLD_MASK 0x00FF0000      /**< Interrupt threshold mask */
#define ECHI_USBCMD_ASYNCDOORBELL   (1 << 6)        /**< Interrupt on Async Advance Doorbell */
#define ECHI_USBCMD_ASYNCEN         (1 << 5)        /**< Enable async transfers */
#define ECHI_USBCMD_PERIODICEN      (1 << 4)        /**< Enable periodic transfers */

// Command register -- frame list sizes
#define EHCI_USBCMD_FRLIST_1024     0x00            /**< 1024 */
#define EHCI_USBCMD_FRLIST_512      0x04            /**< 512 */
#define EHCI_USBCMD_FRLIST_256      0x08            /**< 256 */

// Command register -- interrupt thresholds
#define EHCI_USBCMD_INTTHRSHLD_1    (1 << 16)       /**< 1 microframe */
#define EHCI_USBCMD_INTTHRSHLD_2    (1 << 17)       /**< 2 microframes */
#define EHCI_USBCMD_INTTHRSHLD_4    (1 << 18)       /**< 4 microframes */
#define EHCI_USBCMD_INTTHRSHLD_8    (1 << 19)       /**< 8 microframes */
#define EHCI_USBCMD_INTTHRSHLD_16   (1 << 20)       /**< 16 microframes */
#define EHCI_USBCMD_INTTHRSHLD_32   (1 << 21)       /**< 32 microframes */
#define EHCI_USBCMD_INTTHRSHLD_64   (1 << 22)       /**< 64 microframes */

// Status register bits
#define ECHI_USBSTS_USBINT      (1 << 0)        /**< USB interrupt (R/WC) */
#define ECHI_USBSTS_USBERRINT   (1 << 1)        /**< USB error interrupt (R/WC) */
#define ECHI_USBSTS_PORTCHG     (1 << 2)        /**< Port change detect (R/WC) */
#define ECHI_USBSTS_FRROLL      (1 << 3)        /**< Frame list rollover (R/WC) */
#define ECHI_USBSTS_HSERR       (1 << 4)        /**< Host system error (R/WC) */
#define ECHI_USBSTS_ASYNCINT    (1 << 5)        /**< Interrupt on async advance (R/WC) */
#define ECHI_USBSTS_HCHALTED    (1 << 12)       /**< Host controller halted (RO)
                                                     0=running; 1=halted */
#define ECHI_USBSTS_RECLAM      (1 << 13)       /**< Reclamation (RO) */
#define ECHI_USBSTS_PERIODSTS   (1 << 14)       /**< Periodic schedule status (RO)
                                                     0=disabled; 1=enabled */
#define ECHI_USBSTS_ASYNCSTS    (1 << 15)       /**< Asynchronous schedule status (RO)
                                                     0=disabled; 1=enabled */

// PORTSC registers bits
#define EHCI_PORTSC_CONN        (1 << 0)        /**< Current connect status (RO) */
#define EHCI_PORTSC_CONNCHG     (1 << 1)        /**< Connect status change (R/WC) */
#define EHCI_PORTSC_EN          (1 << 2)        /**< Port enabled/disabled (R/W) */
#define EHCI_PORTSC_ENCHG       (1 << 3)        /**< Port enable/disable change (R/WC) */
#define EHCI_PORTSC_OVRCUR      (1 << 4)        /**< Over-current active (RO) */
#define EHCI_PORTSC_OVRCURCHG   (1 << 5)        /**< Over-current change (R/WC) */
#define EHCI_PORTSC_RESUME      (1 << 6)        /**< Force port resume (R/W) */
#define EHCI_PORTSC_SUSPEND     (1 << 7)        /**< Suspend (R/W) */
#define EHCI_PORTSC_RESET       (1 << 8)        /**< Port reset (R/W) */
#define EHCI_PORTSC_POWER       (1 << 12)       /**< Port power (R/W or RO) */
#define EHCI_PORTSC_OWNER       (1 << 13)       /**< Port owner (R/W) */


/*
 * Queue Head structure.
 */
struct ehci_qh_t
{
    // DWORD 0
    volatile uint32_t next;     /**< 31:5: Queue Head Link Pointer (link to next queue head)
                                     4:3: Reserved
                                     2:1: QH/TD/FSTN Select (Typ):
                                          00b=iTD (isochronous TD)
                                          01b=QH (Queue Head)
                                          10b=siTD (split transaction isochronous TD)
                                          11b=FSTN (Frame Span Traversal Node)
                                     0: Terminate (T):
                                          1=Link pointer field not valid (last pointer)
                                          0=Link pointer field valid */

    // DWORD 1
    uint32_t devaddr          : 7;  /**< 6:0: Device address */
    uint32_t inactive         : 1;  /**< 7: Inactivate on next transaction (I) */
    uint32_t endpoint         : 4;  /**< 11:8: Endpoint number (Endpt) */
    uint32_t endpoint_speed   : 2;  /**< 13:12: Endpoint speed (EPS)
                                          00b=Fullspeed (12 Mb/s)
                                          01b=Lowspeed (1.5 Mb/s)
                                          10b=Highspeed (480 Mb/s)
                                          11b=Reserved */
    uint32_t toggle           : 1;  /**< 14: Data toggle control (DTC)
                                          0b=Data toggle from QH
                                          1b=Data toggle from TD */
    uint32_t head_flag        : 1;  /**< 15: Head of reclamation list flag (H) */
    uint32_t mps              : 11; /**< 26:16: Max packet size (max 1024) */
    uint32_t ctrl_endpoint_flag: 1; /**< 27: Control endpoint flag (C)
                                         1=control endpoint on a non-highspeed device */
    uint32_t nak_count_reload : 4;  /**< 28:31: NAK count reload value (RL) */

    // DWORD 2
    uint8_t int_sched_mask;         /**< 7:0: Interrupt schedule mask(S-mask) */
    uint8_t split_comp_mask;        /**< 15:8: Split completion mask (C-mask) */
    uint16_t hubaddr          : 7;  /**< 22:16: Hub address */
    uint16_t port             : 7;  /**< 29:23: Port number */
    uint16_t mult             : 2;  /**< 31:30: High-bandwidth pipe multiplier (Mult) */

    // DWORD 3-11 -- Transfer Overlay (9 dwords)
    uint32_t current;               /**< Current qTD pointer
                                         31:5: Current Element Transaction Descriptor Link Pointer
                                         4:0: Reserved */
    uint32_t next_qtd;              /**< 31:5: Next qTD pointer
                                         4:1: Reserved
                                         0: Terminate (T)
                                            1=Link pointer field not valid (last pointer)
                                            0=Link pointer field valid */
    uint32_t next_qtd_alt;          /**< 31:5: Alternate next qTD pointer
                                         4:1: NAK counter
                                         0: Terminate (T)
                                            1=Link pointer field not valid (last pointer)
                                            0=Link pointer field valid */

    union
    {
        volatile struct
        {
            uint8_t status;             /**< 7:0: Status */
            uint8_t pid           : 2;  /**< 9:8: PID code */
            uint8_t errcnt        : 2;  /**< 11:10: Error counter (C_ERR) */
            uint8_t curpage       : 3;  /**< 14:12: Current page */
            uint8_t intr          : 1;  /**< 15: Interrupt on Completion (IOC) */
            uint16_t bytes        : 15; /**< 30:16: Total bytes to transfer */
            uint16_t toggle       : 1;  /**< 31: Data toggle */
        } bits;

        volatile uint32_t raw;
    } token;

    uint32_t buf0;                  /**< 31:11: Buffer pointer (page 0)
                                         10:0: Current offset */
    uint32_t buf1;                  /**< 31:11: Buffer pointer (page 1)
                                         10:8: Reserved
                                         7:0: Split-transaction Complete-split Progress
                                                (C-prog-mask) */
    uint32_t buf2;                  /**< 31:11: Buffer pointer (page 2)
                                         10:5: Bytes sent/received during IN or OUT
                                                split transaction (S-bytes)
                                         4:0: Split-transaction Frame Tag (FrameTag) */
    uint32_t buf3;                  /**< 31:11: Buffer pointer (page 3)
                                         10:0: Reserved */
    uint32_t buf4;                  /**< 31:11: Buffer pointer (page 4)
                                         10:0: Reserved */

    uint32_t ext0;
    uint32_t ext1;
    uint32_t ext2;
    uint32_t ext3;
    uint32_t ext4;

    // not part of the EHCI standard
    volatile struct ehci_qh_t *nextvirt;     /**< virtual address of next QH */
    uintptr_t self_phys;            /**< physical address of this QH */

#ifndef __x86_64__
    uint32_t pad0[2];                /**< pad to 64 bytes */
#endif

    uint32_t pad1[11];
} __attribute__((packed));


/*
 * Transfer Descriptor structure.
 */
struct ehci_td_t
{
    uint32_t next_qtd;              /**< 31:5: Next qTD pointer
                                         4:1: Reserved
                                         0: Terminate (T)
                                            1=Link pointer field not valid (last pointer)
                                            0=Link pointer field valid */
    uint32_t next_qtd_alt;          /**< 31:5: Alternate next qTD pointer
                                         4:1: NAK counter
                                         0: Terminate (T)
                                            1=Link pointer field not valid (last pointer)
                                            0=Link pointer field valid */

    union
    {
        volatile struct
        {
            uint8_t status;             /**< 7:0: Status */
            uint8_t pktid         : 2;  /**< 9:8: PID code */
            uint8_t errcnt        : 2;  /**< 11:10: Error counter (C_ERR) */
            uint8_t curpage       : 3;  /**< 14:12: Current page */
            uint8_t intr          : 1;  /**< 15: Interrupt on Completion (IOC) */
            uint16_t bytes        : 15; /**< 30:16: Total bytes to transfer */
            uint16_t toggle       : 1;  /**< 31: Data toggle */
        } bits;

        volatile uint32_t raw;
    } token;

    uint32_t buf0;                  /**< 31:11: Buffer pointer (page 0)
                                         10:0: Current offset */
    uint32_t buf1;                  /**< 31:11: Buffer pointer (page 1)
                                         10:0: Reserved */
    uint32_t buf2;                  /**< 31:11: Buffer pointer (page 2)
                                         10:0: Reserved */
    uint32_t buf3;                  /**< 31:11: Buffer pointer (page 3)
                                         10:0: Reserved */
    uint32_t buf4;                  /**< 31:11: Buffer pointer (page 4)
                                         10:0: Reserved */
    uint32_t ext0;
    uint32_t ext1;
    uint32_t ext2;
    uint32_t ext3;
    uint32_t ext4;
    uint32_t pad[3];
} __attribute__((packed));


struct ehci_transaction_t
{
    struct ehci_td_t *tdvirt;   /**< TD virtual address */
    void *tdphys;               /**< TD physical address */
    void *tdbuf;                /**< TD buffer virtual address */
    void *inbuf;                /**< incoming buffer (maybe NULL) */
    size_t inlen;               /**< size of inbuf */
};


struct ehci_port_t
{
#define EHCI_PORT_FLAG_CONNECTED        0x01
    volatile int flags;              /**< port flags */

    unsigned int port;               /**< port number */
    volatile struct ehci_dev_t *ehci;/**< back pointer to EHCI device */
    volatile struct usb_dev_t *usb;  /**< if non-NULL, pointer to the USB device
                                          currently connected on this port */
};


struct ehci_dev_t
{
#define EHCI_FLAG_RUN                   0x01
#define EHCI_FLAG_PORTENABLED           0x02
//#define UHCI_FLAG_PERIODIC_QUEUE        0x04
    volatile int flags; /**< device flags */
    int caplen;         /**< capabilities register length */

    dev_t devid;        /**< device id */
    int mmio;           /**< device uses memory-mapped I/O (MMIO) */
    uint8_t port_count; /**< number of root ports */
    volatile struct ehci_port_t *ports;  /**< root ports */

    uintptr_t iobase,   /**< I/O space base address */
              iosize;   /**< I/O space size */

    uint32_t *framelist;        /**< frame list virtual address */
    void *framelist_phys;       /**< frame list physical address */

    uintptr_t qhpool;           /**< queue head pool */
    uintptr_t qhpool_phys;      /**< queue head pool physical address */

    uintptr_t tdpool;           /**< TD pool */
    uintptr_t tdpool_phys;      /**< TD pool physical address */

    uintptr_t tdbufpool;        /**< TD buffer pool */
    uintptr_t tdbufpool_phys;   /**< TD buffer pool physical address */

    uint8_t tdbuf_used[EHCI_MAX_TDBUF]; /**< TD buffer use bitmap */
    uint8_t td_used[EHCI_MAX_TD];       /**< TD use bitmap */
    uint8_t qh_used[EHCI_MAX_QH];       /**< QH use bitmap */

    volatile uint32_t addr_bitmap[MAX_DEV_PER_HC / sizeof(uint32_t)];
                                /**< bitmap of used addresses on this bus */

    volatile struct ehci_qh_t *async_qh;   /**< async queue head virtual address */
    volatile struct ehci_qh_t *tail_qh;    /**< async queue tail virtual address */
    struct kernel_mutex_t qh_lock;         /**< queue had lock */

    struct pci_dev_t *pci;      /**< back pointer to PCI device */
    struct ehci_dev_t *next;    /**< next EHCI device */
};


/****************************
 * Function prototypes
 ****************************/

int ehci_install(struct pci_dev_t *pci, struct pci_bar_t *bar);
void ehci_poll(void);
struct usb_dev_t *ehci_get_dev_struct(struct pci_dev_t *bus, uint8_t num);

#endif      /* KERNEL_USB_EHCI_H */
