/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb_ohci.h
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
 *  \file usb_ohci.h
 *
 *  Open Host Controller Interface (OHCI) driver definitions.
 */

#ifndef KERNEL_USB_OHCI_H
#define KERNEL_USB_OHCI_H

#include <sys/types.h>
#include <kernel/mutex.h>

#define OHCI_MAX_INT_INDEX      (6)
#define OHCI_MAX_TD             (PAGE_SIZE / sizeof(struct ohci_td_t))
#define OHCI_MAX_ED             (PAGE_SIZE / sizeof(struct ohci_ed_t))

#define OHCI_TDBUF_SIZE         (1024)
#define OHCI_TDBUF_POOL_SIZE    (PAGE_SIZE * 16)
#define MAX_TDBUF               (OHCI_TDBUF_POOL_SIZE / OHCI_TDBUF_SIZE)

// OHCI register offsets
// Registers that refer to addresses (e.g. Current periodic ED address) all 
// refer to physical (not virtual) memory addresses
#define OHCI_REG_REV            0x00        /**< Spec revision */
#define OHCI_REG_CTRL           0x04        /**< Control */
#define OHCI_REG_CMD_STS        0x08        /**< Command status */
#define OHCI_REG_INT_STS        0x0C        /**< Interrupt status */
#define OHCI_REG_INT_EN         0x10        /**< Enabled interrupts */
#define OHCI_REG_INT_DIS        0x14        /**< Disabled interrupts */
#define OHCI_REG_HCCA           0x18        /**< Host Controller Communication Area (HCCA)
                                                 physical address */
#define OHCI_REG_PERIOD_CUR_ED  0x1C        /**< Current periodic (isochronous or interrupt) 
                                                 Endpoint Descriptor */
#define OHCI_REG_CTRL_HEAD_ED   0x20        /**< Control list first Endpoint Descriptor */
#define OHCI_REG_CTRL_CUR_ED    0x24        /**< Control list current Endpoint Descriptor */
#define OHCI_REG_BULK_HEAD_ED   0x28        /**< Bulk list first Endpoint Descriptor */
#define OHCI_REG_BULK_CUR_ED    0x2C        /**< Bulk list current Endpoint Descriptor */
#define OHCI_REG_DONE_HEAD      0x30        /**< Done queue head */
#define OHCI_REG_FRAME_INTRVL   0x34        /**< Frame interval */
#define OHCI_REG_FRAME_REM      0x38        /**< Remaining time in current frame */
#define OHCI_REG_FRAME_NUM      0x3C        /**< Frame counter */
#define OHCI_REG_PERIOD_START   0x40        /**< Periodic list start time */
#define OHCI_REG_LS_THRESHOLD   0x44        /**< LS packet threshold */
#define OHCI_REG_ROOTHUB_DESCA  0x48        /**< Root hub descriptor */
#define OHCI_REG_ROOTHUB_DESCB  0x4C        /**< Root hub descriptor */
#define OHCI_REG_ROOTHUB_STS    0x50        /**< Root hub status */
#define OHCI_REG_ROOTHUB_PRSTS  0x54        /**< Root hub port status */

// Control register bits
#define OHCI_CTRL_CBSR          ((1 << 0) | (1 << 1))   /**< Control and bulk
                                                             transfer relationship */
#define OHCI_CTRL_PERIOD_EN     (1 << 2)    /**< Enable period transfers */
#define OHCI_CTRL_ISOC_EN       (1 << 3)    /**< Enable isochronous transfers */
#define OHCI_CTRL_CTRL_EN       (1 << 4)    /**< Enable control transfers */
#define OHCI_CTRL_BULK_EN       (1 << 5)    /**< Enable bulk transfers */
#define OHCI_CTRL_HCFS          ((1 << 6) | (1 << 7))   /**< Host controller
                                                             functional state */
#define OHCI_CTRL_INTREDIR      (1 << 8)    /**< IRQ redirect */
#define OHCI_CTRL_REMWAKEUP     (1 << 9)    /**< Remote wakeup */
#define OHCI_CTRL_REMWAKEUP_EN  (1 << 10)   /**< Enable remote wakeup */

// Command Status register bits
#define OHCI_CMDSTS_RESET       (1 << 0)    /**< Reset */
#define OHCI_CMDSTS_CLF         (1 << 1)    /**< Control list filled */
#define OHCI_CMDSTS_BLF         (1 << 2)    /**< Bulk list filled */
#define OHCI_CMDSTS_OCR         (1 << 3)    /**< Ownership change request */
#define OHCI_CMDSTS_SOC         ((1 << 16) | (1 << 17)) /**< Scheduling overrun count */

// Interrupt register bits
#define OHCI_INTSTS_SCHED_OVRRN (1 << 0)    /**< Scheduling overrun */
#define OHCI_INTSTS_WR_DONE     (1 << 1)    /**< Writeback done head */
#define OHCI_INTSTS_SOF         (1 << 2)    /**< Start of frame */
#define OHCI_INTSTS_RESUME_DET  (1 << 3)    /**< Resume detected */
#define OHCI_INTSTS_ERR         (1 << 4)    /**< Unrecoverable error */
#define OHCI_INTSTS_FRAME_OVRFL (1 << 5)    /**< Frame number overflow */
#define OHCI_INTSTS_RHSC        (1 << 6)    /**< Root hub status change */
#define OHCI_INTSTS_OWN_CHG     (1 << 30)   /**< Ownership change */
#define OHCI_INTSTS_MAST_INT_EN (1 << 31)   /**< Master interrupt enable */

// Root Hub Descriptor A register bits
#define OHCI_RHA_PORT_MASK        (0x000000FF)/**< Port numbers max. 15 */
#define OHCI_RHA_PWR_SWITCH_MODE  (1 << 8)    /**< Power switching mode */
#define OHCI_RHA_NO_PWR_SWITCH    (1 << 9)    /**< No power switching */
#define OHCI_RHA_DEV_TYPE         (1 << 10)   /**< Device type */
#define OHCI_RHA_OVRCUR_PROT_MODE (1 << 11)   /**< Over current protection mode */
#define OHCI_RHA_NO_OVRCUR_PROT   (1 << 12)   /**< No over current protection */

// Root Hub Status register bits
#define OHCI_RHS_PWRSTAT        (1 << 0)    /**< Local power status */
#define OHCI_RHS_OVRCUR_IND     (1 << 1)    /**< Over current indicator */
#define OHCI_RHS_REMWAKEUP_EN   (1 << 15)   /**< Remote wakeup enable */
#define OHCI_RHS_PWRSTAT_CHG    (1 << 16)   /**< Local power status change */
#define OHCI_RHS_OVRCUR_IND_CHG (1 << 17)   /**< Over current indicator change */
#define OHCI_RHS_REMWAKEUP_CLR  (1 << 31)   /**< Clear remote wakeup enable */

// Root Hub Port Status registers bits
#define OHCI_RHP_CUR_CONN_STS    (1 << 0)   /**< Current connection status */
#define OHCI_RHP_PORTEN_STS      (1 << 1)   /**< Port enable status */
#define OHCI_RHP_PORTSUSPND_STS  (1 << 2)   /**< Port suspend status */
#define OHCI_RHP_PORTOVRCUR      (1 << 3)   /**< Port over current indicator */
#define OHCI_RHP_PORTRST_STS     (1 << 4)   /**< Port reset status */
#define OHCI_RHP_PORTPWR_STS     (1 << 8)   /**< Port power status */
#define OHCI_RHP_LOSPEED         (1 << 9)   /**< Lowspeed device attached */
#define OHCI_RHP_CONN_STS_CHG    (1 << 16)  /**< Connection status change */
#define OHCI_RHP_PORTEN_STS_CHG  (1 << 17)  /**< Port enable status change */
#define OHCI_RHP_PORTSUSPND_STS_CHG (1 << 18) /**< Port suspend status change */
#define OHCI_RHP_PORTOVRCUR_CHG  (1 << 19)  /**< Port over current indicator change */
#define OHCI_RHP_PORTRST_STS_CHG (1 << 20)  /**< Port reset status change */

// Hardware Controller (HC) operational states
#define OHCI_HC_RESET           (0)
#define OHCI_HC_RESUME          (1 << 6)
#define OHCI_HC_OPERATIONAL     (1 << 7)
#define OHCI_HC_SUSPEND         ((1 << 6) | (1 << 7))


struct ohci_hcca_t
{
    volatile uint32_t ed_int_head[32];      /**< Interrupt EDs */
    volatile uint16_t curframe;             /**< Current frame number */
    volatile uint16_t unused;               /**< Set to 0 by host controller */
    volatile uint32_t done_head;            /**< Current value of done head */
    uint8_t res[116];                       /**< Reserved */
} __attribute__((packed));


/*
 * Transfer Descriptors describe transactions. There are 2 types:
 *   (a) General TD: Used for control, bulk and interrupt transfers, and must
 *       be aligned on a 16-byte boundary.
 *   (b) Isochronous TD: Used for isochronous transfers, and must be aligned
 *       on a 32-byte boundary.
 *
 * The General TD is structured as follows:
 *
 * 31     28|27 26|25 24|23   21|20 19|18|                                    |3       0|
 * +--------+-----+-----+-------+-----+--+----------------------------------------------+
 * |   CC   | EC  |  T  |  DI   | DP  |R |                                              |
 * +--------+-----+-----+-------+-----+--+----------------------------------------------+
 * |                          Current Buffer Pointer (CBP)                              |
 * +--------------------------------------------------------------------------+---------+
 * |                                Next TD (NextTD)                          |    0    |
 * +--------------------------------------------------------------------------+---------+
 * |                                 Buffer End (BE)                                    |
 * +------------------------------------------------------------------------------------+
 *
 * We extend the struct to 32 bytes so we can store housekeeping info at the end.
 */
struct ohci_td_t
{
    union
    {
        struct
        {
            volatile uint32_t res      : 18; /**< 0:17: not used */
            volatile uint32_t smallpkt : 1;  /**< 18: 1=accept packet smaller than buffer */

#define OHCI_TD_DIRECTION_SETUP     0
#define OHCI_TD_DIRECTION_OUT       1
#define OHCI_TD_DIRECTION_IN        2
            volatile uint32_t direction: 2;  /**< 19:20: transfer direction:
                                                         00b SETUP
                                                         01b OUT
                                                         10b IN
                                                         11b reserved */
            volatile uint32_t delayint : 3;  /**< 21:23: wait this number of frames before
                                                         sending interrupt */
            volatile uint32_t toggle   : 1;  /**< 24: data toggle */
            volatile uint32_t togglefromtd: 1;  /**< 25: toggle from TD */
            volatile uint32_t errcnt   : 2;  /**< 26:27: error count */
            volatile uint32_t sts      : 4;  /**< 28:31: status of last transaction */
        };

        volatile uint32_t dword0;
    };

    volatile uint32_t curbuf;        /**< Current buffer's physical address */
    volatile uint32_t next;          /**< Pointer to next TD's physical address */
    volatile uint32_t bufend;        /**< Current buffer's end */

    // not part of the OHCI standard
    volatile uint32_t alloced;      /**< 1 if this TD is alloc'd, 0 if free */
    volatile uint32_t self_phys;    /**< Physical address of this TD */

    volatile void *virtbuf;         /**< Current buffer's virtual address */

#ifndef __x86_64__
    char pad[4];                    /**< Padding to 32-byte boundary */
#endif

} __attribute__((packed));


/*
 * Endpoint Descriptors describe endpoints. They must be aligned on a 
 * 16-byte boundary:
 *
 * 31             27|26                            16|15|14|13|1211|10     7|6  4|  |1|0|
 * +----------------+--------------------------------+--+--+--+----+--------+-----------+
 * |                |      Maximum Packet Size       |F |K |S | D  |   EN   |    FA     |
 * +----------------+--------------------------------+--+--+--+----+--------+-----------+
 * |                       TD Queue Tail Pointer (TailP)                         |      |
 * +-----------------------------------------------------------------------------+--+-+-+
 * |                       TD Queue Head Pointer (HeadP)                         |  |C|H|
 * +-----------------------------------------------------------------------------+--+-+-+
 * |                     Next Endpoint Descriptor (NextED)                       |      |
 * +-----------------------------------------------------------------------------+------+
 *
 * We extend the struct to 32 bytes so we can store housekeeping info at the end.
 */
struct ohci_ed_t
{
    union
    {
        struct
        {
            volatile uint32_t dev      : 7;  /**< 0:6: device address */
            volatile uint32_t endpoint : 4;  /**< 7:10: endpoint number */
            volatile uint32_t direction: 2;  /**< 11:12: transfer direction */
            volatile uint32_t speed    : 1;  /**< 13: 0=fullspeed; 1=low speed */
            volatile uint32_t skip     : 1;  /**< 14: 1=skip to next ED */
            volatile uint32_t format   : 1;  /**< 15: 1 for isochronous transfers */
            volatile uint32_t mps      : 11; /**< 16:26: maximum packet size */
            volatile uint32_t res      : 5;  /**< 27:31: not used */
        };

        volatile uint32_t dword0;
    };

    volatile uint32_t td_qtail;      /**< TD queue tail */
    volatile uint32_t td_qhead;      /**< TD queue head */
    volatile uint32_t next;          /**< Pointer to next ED's physical address */

    // not part of the OHCI standard
    volatile uint32_t alloced;      /**< 1 if this ED is alloc'd, 0 if free */
    volatile uint32_t self_phys;    /**< Physical address of this ED */

    volatile struct ohci_td_t *td_dummy;   /**< Dummy tail TD virtual address */

#ifndef __x86_64__
    char pad[4];                    /**< padding to 32-byte boundary */
#endif

} __attribute__((packed));


struct ohci_port_t
{
#define OHCI_PORT_FLAG_CONNECTED        0x01
    volatile int flags;         /**< port flags */

    unsigned int port;          /**< port number */
    struct ohci_dev_t *ohci;    /**< back pointer to OHCI device */
    struct usb_dev_t *usb;      /**< if non-NULL, pointer to the USB device
                                     currently connected on this port */
};


struct ohci_transaction_t
{
    struct ohci_td_t *tdvirt;       /**< TD virtual address */
    struct ohci_td_t *tdcopyvirt;   /**< virtual address of TD copy */
    void *tdbuf;                    /**< TD buffer virtual address */
    void *inbuf;                    /**< incoming buffer (maybe NULL) */
    size_t inlen;                   /**< size of inbuf */
};


struct ohci_dev_t
{
#define OHCI_FLAG_RUN                   0x01
#define OHCI_FLAG_PORTENABLED           0x02
    volatile int flags; /**< device flags */

    dev_t devid;        /**< device id */
    int mmio;           /**< device uses memory-mapped I/O (MMIO) */
    uint8_t port_count; /**< number of root ports */
    volatile struct ohci_port_t *ports;  /**< root ports */

    uintptr_t iobase,   /**< I/O space base address */
              iosize;   /**< I/O space size */

    struct ohci_hcca_t *hcca;   /**< Host Controller Communications Area (HCCA) */
    uintptr_t hcca_phys;        /**< HCCA physical address */

    struct ohci_td_t *tdpool;       /**< TD pool */
    uintptr_t tdpool_phys;          /**< TD pool physical address */

    struct ohci_ed_t *edpool;       /**< ED pool */
    uintptr_t edpool_phys;          /**< ED pool physical address */

    uintptr_t tdbufpool;            /**< TD buffer pool */
    uintptr_t tdbufpool_phys;       /**< TD buffer pool physical address */
    uint8_t tdbuf_used[MAX_TDBUF];  /**< TD buffer use bitmap */

    struct ohci_ed_t *ed_int_head[OHCI_MAX_INT_INDEX];
                                    /**< ED heads for interrupt transfers */
    struct ohci_ed_t *ed_bulk_head; /**< ED head for bulk transfers */
    struct ohci_ed_t *ed_ctrl_head; /**< ED head for control transfers */

    volatile uint32_t addr_bitmap[MAX_DEV_PER_HC / sizeof(uint32_t)];
                                /**< bitmap of used addresses on this bus */

    struct kernel_mutex_t lock; /**< structure lock */

    struct pci_dev_t *pci;      /**< back pointer to PCI device */
    struct ohci_dev_t *next;    /**< next OHCI device */
};


/****************************
 * Function prototypes
 ****************************/

int ohci_install(struct pci_dev_t *pci, struct pci_bar_t *bar);
void ohci_poll(void);
struct usb_dev_t *ohci_get_dev_struct(struct pci_dev_t *bus, uint8_t num);

#endif      /* KERNEL_USB_OHCI_H */
