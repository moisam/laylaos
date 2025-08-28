/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024, 2025 (c)
 * 
 *    file: i8254x.h
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
 *  \file i8254x.h
 *
 *  Functions and macros for working with Intel 8254x series network cards.
 */

#ifndef NET_INTEL8254_H
#define NET_INTEL8254_H

#include <kernel/net/netif.h>
#include <kernel/pci.h>
#include <kernel/mutex.h>

// Register offsets
#define i8254x_REG_CTRL             0x0000
#define i8254x_REG_STATUS           0x0008
#define i8254x_REG_EERD             0x0014
#define i8254x_REG_INTR             0x00C0
#define i8254x_REG_IMS              0x00D0
#define i8254x_REG_RCTL             0x0100
#define i8254x_REG_TCTL             0x0400

#define i8254x_REG_RDBAL            0x2800
#define i8254x_REG_RDBAH            0x2804
#define i8254x_REG_RDLEN            0x2808
#define i8254x_REG_RDH              0x2810
#define i8254x_REG_RDT              0x2818
#define i8254x_REG_RDTR             0x2820

#define i8254x_REG_TDBAL            0x3800
#define i8254x_REG_TDBAH            0x3804
#define i8254x_REG_TDLEN            0x3808
#define i8254x_REG_TDH              0x3810
#define i8254x_REG_TDT              0x3818

#define i8254x_REG_CRCERRS          0x4000
#define i8254x_REG_MTA              0x5200

// Bits for the CTRL register
#define CTRL_LRST                   (1 << 3)
#define CTRL_ASDE                   (1 << 5)
#define CTRL_SLU                    (1 << 6)
#define CTRL_ILOS                   (1 << 7)
#define CTRL_RST                    (1 << 26)
#define CTRL_PHYS_RST               (1 << 31)

// Bits for the RCTL register
#define RCTL_EN                         (1 << 1)
#define RCTL_SBP                        (1 << 2)
#define RCTL_UPE                        (1 << 3)
#define RCTL_MPE                        (1 << 4)
#define RCTL_LPE                        (1 << 5)
#define RCTL_LBM                        ((1 << 7) | (1 << 6))
#define RDMTS_HALF                      (0 << 8)
#define RDMTS_QUARTER                   (1 << 8)
#define RDMTS_EIGHTH                    (2 << 8)
#define RCTL_BAM                        (1 << 15)
#define RCTL_BSIZE_256                  (3 << 16)
#define RCTL_BSIZE_512                  (2 << 16)
#define RCTL_BSIZE_1024                 (1 << 16)
#define RCTL_BSIZE_2048                 (0 << 16)
#define RCTL_BSIZE_4096                 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192                 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384                ((1 << 16) | (1 << 25))
#define RCTL_BSEX                       (1 << 25)
#define RCTL_SECRC                      (1 << 26)

// Bits for the TCTL register
#define TCTL_EN                         (1 << 1)
#define TCTL_PSP                        (1 << 3)

// Bits for the IMS register
#define IMS_TXDW                        (1 << 0)
#define IMS_TXQE                        (1 << 1)
#define IMS_LSC                         (1 << 2)
#define IMS_RXO                         (1 << 6)
#define IMS_RXT                         (1 << 7)


struct i8254x_rx_desc_t
{
    volatile uint64_t address;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __attribute__((packed));

struct i8254x_tx_desc_t
{
    volatile uint64_t address;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t sta;
    volatile uint8_t css;
    volatile uint16_t special;
} __attribute__((packed));

struct i8254x_t
{
    struct netif_t netif;
    struct pci_dev_t *dev;

    int mmio;           /**< device uses memory-mapped I/O (MMIO) */

    uintptr_t iobase,   /**< I/O space base address */
              iosize;   /**< I/O space size */

    uint8_t nsaddr[6];

    volatile struct i8254x_rx_desc_t *rx_desc;
    volatile struct i8254x_tx_desc_t *tx_desc;
    //volatile uint16_t tx_tail, rx_tail;
    virtual_addr *inbuf_virt;   /**< virtual addresses out receive buffers */
    virtual_addr *outbuf_virt;  /**< virtual addresses out transmit buffers */

    struct netif_queue_t outq;
    volatile struct kernel_mutex_t lock;
    volatile struct task_t *task;
};


int i8254x_init(struct pci_dev_t *pci);
int i8254x_intr(struct regs *r, int unit);
void i8254x_do_intr(int unit);
void i8254x_receive(struct i8254x_t *dev);
int i8254x_transmit(struct netif_t *ifp, struct packet_t *p);
int i8254x_process_input(struct netif_t *ifp);
int i8254x_process_output(struct netif_t *ifp);

#endif      /* NET_INTEL8254_H */
