/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ne2000.h
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
 *  \file ne2000.h
 *
 *  Functions and macros for working with NE2000 network cards.
 */

#ifndef NET_NE2000_H
#define NET_NE2000_H

#include <kernel/net/netif.h>
#include <kernel/pci.h>
#include <kernel/mutex.h>

/* NE2000 page 0 registers */
#define REG_COMMAND                     0x00
#define REG_PAGESTART                   0x01
#define REG_PAGESTOP                    0x02
#define REG_BOUNDARY                    0x03
#define REG_TRANSMIT_STATUS             0x04
#define REG_TRANSMIT_PAGE               0x04
#define REG_TRANSMIT_BYTECOUNT0         0x05
#define REG_TRANSMIT_BYTECOUNT1         0x06
#define REG_INTERRUPT_STATUS            0x07
#define REG_REMOTE_STARTADDRESS0        0x08
#define REG_REMOTE_STARTADDRESS1        0x09
#define REG_REMOTE_BYTECOUNT0           0x0A
#define REG_REMOTE_BYTECOUNT1           0x0B
#define REG_RECEIVE_CONFIGURATION       0x0C
#define REG_TRANSMIT_CONFIGURATION      0x0D
#define REG_DATA_CONFIGURATION          0x0E
#define REG_INTERRUPTMASK               0x0F

#define REG_NE_RESET                    0x1F
#define REG_NE_DATA                     0x10

/* NE2000 page 1 registers */
#define REG_P1_PAR0                     0x01
#define REG_P1_CURPAGE                  0x07
#define REG_P1_MAR0                     0x08

/* NE2000 Command Register bits */
#define CR_STOP                         0x01    // Stop controller
#define CR_START                        0x02    // Start controller
#define CR_TRANS                        0x04    // Transmit packet
#define CR_RREAD                        0x08    // Remote read
#define CR_RWRITE                       0x10    // Remote write
#define CR_NODMA                        0x20    // No Remote DMA present
#define CR_PG0                          0x00    // Select Page 0
#define CR_PG1                          0x40    // Select Page 1
#define CR_PG2                          0x80    // Select Page 2

/* NE2000 Interrupt Register bits */
#define IR_RX                           0x01    // Successful packet Rx
#define IR_TX                           0x02    // Successful packet Tx
#define IR_RXE                          0x04    // Packet Rx  w/error
#define IR_TXE                          0x08    // Packet Tx  w/error 
#define IR_ROVRN                        0x10    // Receiver overrun in the ring
#define IR_CTRS                         0x20    // Diagnostic counters need attn
#define IR_RDC                          0x40    // Remote DMA Complete
#define IR_RESET                        0x80    // Reset Complete

/* NE2000 Data Configuration Register bits */
#define DR_WTS                          0x01    // Word Transfer Select
#define DR_BOS                          0x02    // Byte Order Select
#define DR_LAS                          0x04    // Long Address Select
#define DR_BMS                          0x08    // Burst Mode Select
#define DR_AR                           0x10    // Autoinitialize Remote
#define DR_FT0                          0x20    // Fifo Threshold Select
#define DR_FT1                          0x40    // Fifo Threshold Select

/* NE2000 Receive Configuration Register bits */
#define RR_SEP                          0x01    // Save error packets
#define RR_AR                           0x02    // Accept Runt packets
#define RR_AB                           0x04    // Accept Broadcast packets
#define RR_AM                           0x08    // Accept Multicast packets
#define RR_PRO                          0x10    // Promiscuous physical
#define RR_MON                          0x20    // Monitor mode

/* NE2000 Transmit Configuration Register bits */
#define TR_CRC                          0x01    // Inhibit CRC
#define TR_LB0                          0x02    // Encoded Loopback Control
#define TR_LB1                          0x04    // Encoded Loopback Control
#define TR_ATD                          0x08    // Auto Transmit Disable
#define TR_OFST                         0x10    // Collision Offset Enable


struct ne2000_t
{
    struct netif_t netif;
    struct pci_dev_t *dev;
    uint16_t iobase;

    uint8_t saprom[16];
    uint8_t nsaddr[6];
    int word_mode;

    uint8_t next_packet;

    struct netif_queue_t outq;
    struct kernel_mutex_t lock;
};


int ne2000_init(struct pci_dev_t *pci);
int ne2000_intr(struct regs *r, int unit);
void ne2000_do_intr(int unit);
void ne2000_receive(struct ne2000_t *ne);
int ne2000_transmit(struct netif_t *ifp, struct packet_t *p);
void ne2000_process_input(struct netif_t *ifp);
void ne2000_process_output(struct netif_t *ifp);

#endif      /* NET_NE2000_H */
