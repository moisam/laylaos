/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: ahci.h
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
 *  \file ahci.h
 *
 *  Functions and structure defines for working with AHCI (Advance Host 
 *  Controller Interface) or SATA devices.
 */

#ifndef AHCI_H
#define AHCI_H


/* Maximum number of AHCI disks we can handle:
 *     16 whole disks
 *     15 partitions per disk
 *    256 disks/partitions in total
 */
#define MAX_AHCI_DEVICES        256

/* Maximum number of AHCI CD-ROMS we can handle
 */
#define MAX_AHCI_CDROMS         26

/*
 * All AHCI devices have a major of 8 (/dev/sdX)...
 */
#define AHCI_DEV_MAJ            8

/*
 * ...except for CD-ROMs, which have a major of 11 (/dev/scdX).
 */
#define AHCI_CDROM_MAJ          11

/* Our master table for AHCI disks and their partitions */
extern struct ata_dev_s *ahci_disk_dev[MAX_AHCI_DEVICES];
extern struct ata_dev_s *ahci_cdrom_dev[MAX_AHCI_CDROMS];
extern struct parttab_s *ahci_disk_part[MAX_AHCI_DEVICES];


/**
 * \enum FIS_TYPE
 *
 * types of a Frame Information Structure (FIS) packet
 */
typedef enum
{
    FIS_TYPE_REG_H2D    = 0x27,   /**< Register FIS - host to device */
    FIS_TYPE_REG_D2H    = 0x34,   /**< Register FIS - device to host */
    FIS_TYPE_DMA_ACT    = 0x39,   /**< DMA activate FIS - device to host */
    FIS_TYPE_DMA_SETUP  = 0x41,   /**< DMA setup FIS - bidirectional */
    FIS_TYPE_DATA       = 0x46,   /**< Data FIS - bidirectional */
    FIS_TYPE_BIST       = 0x58,   /**< BIST activate FIS - bidirectional */
    FIS_TYPE_PIO_SETUP  = 0x5F,   /**< PIO setup FIS - device to host */
    FIS_TYPE_DEV_BITS   = 0xA1,   /**< Set device bits FIS - device to host */
} FIS_TYPE;


/**
 * @struct HBA_PORT
 * @brief Host Bus Adapter (HBA) port.
 *
 * A structure to represent a port on the Host Bus Adapter (HBA) and
 * its control registers. It must be declared volatile so the compiler
 * will not optimize out reads/writes.
 */
typedef volatile struct
{
    uint32_t clb;           /**< 0x00, command list base address,
                                 1K-byte aligned */
    uint32_t clbu;          /**< 0x04, command list base address 
                                 upper 32 bits */
    uint32_t fb;            /**< 0x08, FIS base address, 256-byte aligned */
    uint32_t fbu;           /**< 0x0C, FIS base address upper 32 bits */
    uint32_t is;            /**< 0x10, interrupt status */
    uint32_t ie;            /**< 0x14, interrupt enable */
    uint32_t cmd;           /**< 0x18, command and status */
    uint32_t rsv0;          /**< 0x1C, Reserved */
    uint32_t tfd;           /**< 0x20, task file data */
    uint32_t sig;           /**< 0x24, signature */
    uint32_t ssts;          /**< 0x28, SATA status (SCR0:SStatus) */
    uint32_t sctl;          /**< 0x2C, SATA control (SCR2:SControl) */
    uint32_t serr;          /**< 0x30, SATA error (SCR1:SError) */
    uint32_t sact;          /**< 0x34, SATA active (SCR3:SActive) */
    uint32_t ci;            /**< 0x38, command issue */
    uint32_t sntf;          /**< 0x3C, SATA notification */
    uint32_t fbs;           /**< 0x40, FIS-based switch control */
    uint32_t rsv1[11];      /**< 0x44 ~ 0x6F, Reserved */
    uint32_t vendor[4];     /**< 0x70 ~ 0x7F, vendor specific */
} HBA_PORT;


/**
 * @struct HBA_MEM
 * @brief Host Bus Adapter (HBA) memory.
 *
 * A structure to represent Host Bus Adapter (HBA) memory space.
 * It must be declared volatile so the compiler will not optimize
 * out reads/writes.
 */
typedef volatile struct
{
    // 0x00 - 0x2B, Generic Host Control
    uint32_t cap;           /**< 0x00, Host capability */
    uint32_t ghc;           /**< 0x04, Global host control */
    uint32_t is;            /**< 0x08, Interrupt status */
    uint32_t pi;            /**< 0x0C, Port implemented */
    uint32_t vs;            /**< 0x10, Version */
    uint32_t ccc_ctl;       /**< 0x14, Command completion coalescing control */
    uint32_t ccc_pts;       /**< 0x18, Command completion coalescing ports */
    uint32_t em_loc;        /**< 0x1C, Enclosure management location */
    uint32_t em_ctl;        /**< 0x20, Enclosure management control */
    uint32_t cap2;          /**< 0x24, Host capabilities extended */
    uint32_t bohc;          /**< 0x28, BIOS/OS handoff control and status */
 
    // 0x2C - 0x9F, Reserved
    uint8_t  rsv[0xA0-0x2C];
 
    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t  vendor[0x100-0xA0];
 
    // 0x100 - 0x10FF, Port control registers
    HBA_PORT    ports[1];    /**< ports 1 ~ 32 */
} HBA_MEM;


/**
 * @struct FIS_REG_H2D
 * @brief Register FIS - Host to Device.
 *
 * A structure to represent an H2D (Host to Device) Frame Information
 * Structure (FIS) packet. It is used by the host to send a command
 * to the device.
 */
typedef struct
{
    // DWORD 0
    uint8_t  fis_type;      /**< FIS_TYPE_REG_H2D */

    uint8_t  pmport:4;      /**< Port multiplier */
    uint8_t  rsv0:3;        /**< Reserved */
    uint8_t  c:1;           /**< 1: Command, 0: Control */

    uint8_t  command;       /**< Command register */
    uint8_t  featurel;      /**< Feature register, 7:0 */

    // DWORD 1
    uint8_t  lba0;          /**< LBA low register, 7:0 */
    uint8_t  lba1;          /**< LBA mid register, 15:8 */
    uint8_t  lba2;          /**< LBA high register, 23:16 */
    uint8_t  device;        /**< Device register */

    // DWORD 2
    uint8_t  lba3;          /**< LBA register, 31:24 */
    uint8_t  lba4;          /**< LBA register, 39:32 */
    uint8_t  lba5;          /**< LBA register, 47:40 */
    uint8_t  featureh;      /**< Feature register, 15:8 */

    // DWORD 3
    uint8_t  countl;        /**< Count register, 7:0 */
    uint8_t  counth;        /**< Count register, 15:8 */
    uint8_t  icc;           /**< Isochronous command completion */
    uint8_t  control;       /**< Control register */

    // DWORD 4
    uint8_t  rsv1[4];       /**< Reserved */
} FIS_REG_H2D;


/**
 * @struct FIS_REG_D2H
 * @brief Register FIS - Device to Host.
 *
 * A structure to represent a D2H (Device to Host) Frame Information
 * Structure (FIS) packet. It is used by the device to notify the host
 * of a change in one or more registers.
 */
typedef struct
{
    // DWORD 0
    uint8_t  fis_type;      /**< FIS_TYPE_REG_D2H */

    uint8_t  pmport:4;      /**< Port multiplier */
    uint8_t  rsv0:2;        /**< Reserved */
    uint8_t  i:1;           /**< Interrupt bit */
    uint8_t  rsv1:1;        /**< Reserved */

    uint8_t  status;        /**< Status register */
    uint8_t  error;         /**< Error register */

    // DWORD 1
    uint8_t  lba0;          /**< LBA low register, 7:0 */
    uint8_t  lba1;          /**< LBA mid register, 15:8 */
    uint8_t  lba2;          /**< LBA high register, 23:16 */
    uint8_t  device;        /**< Device register */

    // DWORD 2
    uint8_t  lba3;          /**< LBA register, 31:24 */
    uint8_t  lba4;          /**< LBA register, 39:32 */
    uint8_t  lba5;          /**< LBA register, 47:40 */
    uint8_t  rsv2;          /**< Reserved */

    // DWORD 3
    uint8_t  countl;        /**< Count register, 7:0 */
    uint8_t  counth;        /**< Count register, 15:8 */
    uint8_t  rsv3[2];       /**< Reserved */

    // DWORD 4
    uint8_t  rsv4[4];       /**< Reserved */
} FIS_REG_D2H;


/**
 * @struct FIS_DATA
 * @brief Data FIS - Bidirectional.
 *
 * A structure to represent a Data Frame Information Structure (FIS) packet.
 * It is used by device or host to send a data payload to the other side.
 */
typedef struct
{
    // DWORD 0
    uint8_t  fis_type;      /**< FIS_TYPE_DATA */

    uint8_t  pmport:4;      /**< Port multiplier */
    uint8_t  rsv0:4;        /**< Reserved */

    uint8_t  rsv1[2];       /**< Reserved */

    // DWORD 1 ~ N
    uint32_t data[1];       /**< Payload */
} FIS_DATA;


/**
 * @struct FIS_PIO_SETUP
 * @brief PIO Setup FIS - Device to Host.
 *
 * A structure to represent a PIO Setup Frame Information Structure (FIS)
 * packet. It is used by device to tell the host it is ready to send/receive
 * PIO data.
 */
typedef struct
{
    // DWORD 0
    uint8_t  fis_type;      /**< FIS_TYPE_PIO_SETUP */

    uint8_t  pmport:4;      /**< Port multiplier */
    uint8_t  rsv0:1;        /**< Reserved */
    uint8_t  d:1;           /**< Data transfer direction, 1 - device to host */
    uint8_t  i:1;           /**< Interrupt bit */
    uint8_t  rsv1:1;

    uint8_t  status;        /**< Status register */
    uint8_t  error;         /**< Error register */

    // DWORD 1
    uint8_t  lba0;          /**< LBA low register, 7:0 */
    uint8_t  lba1;          /**< LBA mid register, 15:8 */
    uint8_t  lba2;          /**< LBA high register, 23:16 */
    uint8_t  device;        /**< Device register */

    // DWORD 2
    uint8_t  lba3;          /**< LBA register, 31:24 */
    uint8_t  lba4;          /**< LBA register, 39:32 */
    uint8_t  lba5;          /**< LBA register, 47:40 */
    uint8_t  rsv2;          /**< Reserved */

    // DWORD 3
    uint8_t  countl;        /**< Count register, 7:0 */
    uint8_t  counth;        /**< Count register, 15:8 */
    uint8_t  rsv3;          /**< Reserved */
    uint8_t  e_status;      /**< New value of status register */

    // DWORD 4
    uint16_t tc;            /**< Transfer count */
    uint8_t  rsv4[2];       /**< Reserved */
} FIS_PIO_SETUP;


/**
 * @struct FIS_DMA_SETUP
 * @brief DMA Setup FIS - Device to Host.
 *
 * A structure to represent a DMA Setup Frame Information Structure (FIS)
 * packet. It is used by device to tell the host it is ready to send/receive
 * DMA data.
 */
typedef struct
{
    // DWORD 0
    uint8_t  fis_type;      /**< FIS_TYPE_DMA_SETUP */

    uint8_t  pmport:4;      /**< Port multiplier */
    uint8_t  rsv0:1;        /**< Reserved */
    uint8_t  d:1;           /**< Data transfer direction, 1 - device to host */
    uint8_t  i:1;           /**< Interrupt bit */
    uint8_t  a:1;           /**< Auto-activate. Specifies if DMA Activate
                                 FIS is needed */

    uint8_t  rsved[2];      /**< Reserved */

    // DWORD 1&2
    uint64_t DMAbufferID;   /**< DMA Buffer Identifier. Used to Identify DMA
                                 buffer in host memory.
                                 SATA Spec says host specific and not in Spec.
                                 Trying AHCI spec might work */

    // DWORD 3
    uint32_t rsvd;          /**< More reserved */

    // DWORD 4
    uint32_t DMAbufOffset;  /**< Byte offset into buffer.
                                 First 2 bits must be 0 */

    // DWORD 5
    uint32_t TransferCount; /**< Number of bytes to transfer.
                                 Bit 0 must be 0 */

    // DWORD 6
    uint32_t resvd;         /**< Reserved */
} FIS_DMA_SETUP;


/**
 * @struct HBA_FIS
 * @brief Received FIS - Device to Host.
 *
 * A structure to represent a Received Frame Information Structure (FIS)
 * packet.
 */
typedef volatile struct
{
    // 0x00
    FIS_DMA_SETUP   dsfis;     /**< DMA Setup FIS */
    uint8_t         pad0[4];

    // 0x20
    FIS_PIO_SETUP   psfis;     /**< PIO Setup FIS */
    uint8_t         pad1[12];

    // 0x40
    FIS_REG_D2H     rfis;      /**< Register - Device to Host FIS */
    uint8_t         pad2[4];

    // 0x58
    uint64_t        sdbfis;    /**< Set Device Bit FIS */
    //FIS_DEV_BITS    sdbfis;  /**< Set Device Bit FIS */

    // 0x60
    uint8_t         ufis[64];

    // 0xA0
    uint8_t         rsv[0x100-0xA0];
} HBA_FIS;


/**
 * @struct HBA_CMD_HEADER
 * @brief Command Header.
 *
 * A structure to represent a Command Header, which is used in the Command
 * Lists the host uses to let the device know the commands it needs to do.
 */
typedef struct
{
    // DW0
    uint8_t  cfl:5;         /**< Command FIS length in DWORDS, 2 ~ 16 */
    uint8_t  a:1;           /**< ATAPI */
    uint8_t  w:1;           /**< Write, 1: H2D, 0: D2H */
    uint8_t  p:1;           /**< Prefetchable */

    uint8_t  r:1;           /**< Reset */
    uint8_t  b:1;           /**< BIST */
    uint8_t  c:1;           /**< Clear busy upon R_OK */
    uint8_t  rsv0:1;        /**< Reserved */
    uint8_t  pmp:4;         /**< Port multiplier port */

    uint16_t prdtl;         /**< Physical region descriptor table length
                                 in entries */

    // DW1
    volatile
    uint32_t prdbc;         /**< Physical region descriptor byte count
                                 transferred */

    // DW2, 3
    uint32_t ctba;          /**< Command table descriptor base address */
    uint32_t ctbau;         /**< Command table descriptor base address
                                 upper 32 bits */

    // DW4 - 7
    uint32_t rsv1[4];       /**< Reserved */
} HBA_CMD_HEADER;


/**
 * @struct HBA_PRDT_ENTRY
 * @brief PRDT entry.
 *
 * A structure to represent a Physical Region Descriptor Table (PRDT) entry.
 * It is used by the host to inform the device of the requested data payload
 * address and size.
 */
typedef struct
{
    uint32_t dba;           /**< Data base address */
    uint32_t dbau;          /**< Data base address upper 32 bits */
    uint32_t rsv0;          /**< Reserved */

    // DW3
    uint32_t dbc:22;        /**< Byte count, 4M max */
    uint32_t rsv1:9;        /**< Reserved */
    uint32_t i:1;           /**< Interrupt on completion */
} HBA_PRDT_ENTRY;


/**
 * @struct HBA_CMD_TBL
 * @brief Command table.
 *
 * A structure to represent a command table entry.
 * It is used by the host to inform the device of the requested commands. It
 * is used with one or more PRDT entries (see \ref HBA_PRDT_ENTRY).
 */
typedef struct
{
    // 0x00
    uint8_t  cfis[64];      /**< Command FIS */

    // 0x40
    uint8_t  acmd[16];      /**< ATAPI command, 12 or 16 bytes */

    // 0x50
    uint8_t  rsv[48];       /**< Reserved */

    // 0x80
    HBA_PRDT_ENTRY prdt_entry[1];    /**< Physical region descriptor table
                                          entries, 0 ~ 65535 */
} HBA_CMD_TBL;


/**
 * @struct ahci_dev_t
 * @brief The ahci_dev_t structure.
 *
 * A structure to represent an AHCI device.
 */
struct ahci_dev_t
{
    dev_t devid;                /**< Device ID */
    uintptr_t iobase,           /**< I/O base address */
              iosize;           /**< Size of I/O space */
    uintptr_t port_clb[32];     /**< Virtual addresses for us to write to each
                                     port's command list base address */
    uintptr_t port_fb[32];      /**< Same for port's FIS base address */
    uintptr_t port_ctba[32];    /**< Same for port's command list buffer */
    volatile struct kernel_mutex_t port_lock[32];    /**< Port lock */
    struct pci_dev_t *pci;      /**< Pointer to PCI device struct */
    struct task_t *task;        /**< Pointer to IRQ handler task */
    struct ahci_dev_t *next;    /**< Next AHCI device */
};


/**
 * @brief General Block Read/Write Operations.
 *
 * This is a swiss-army knife function that handles both read and write
 * operations from disk/media. The buffer specified in \a buf tells the
 * function everything it needs to know: how many bytes to read or write,
 * where to read/write the data to/from, which device to use for the
 * read/write operation, and whether the operation is a read or write.
 *
 * @param   req     disk I/O request struct with read/write details
 *
 * @return number of bytes read or written on success, -(errno) on failure
 */
long ahci_strategy(struct disk_req_t *req);

/**
 * @brief General block device control function.
 *
 * Perform ioctl operations on an AHCI device.
 *
 * @param   dev     device id
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
long ahci_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel);

/**
 * @brief Initialise AHCI disk devices.
 *
 * Probe the given \a pci device, which should be an AHCI controller, to find
 * the types of AHCI devices attached to it, and initialize those devices.
 *
 * @param   pci     PCI structure of the AHCI device to initialize
 *
 * @return  nothing.
 */
void ahci_init(struct pci_dev_t *pci);


/*********************************************
 * Internal functions
 *********************************************/

long achi_satapi_read_packet_virt(struct ata_dev_s *dev,
                                  uintptr_t virt_buf, size_t bufsz,
                                  size_t lba, int sectors, unsigned char *packet);

long achi_satapi_write_packet_virt(struct ata_dev_s *dev,
                                   uintptr_t virt_buf, size_t bufsz,
                                   size_t lba, int sectors, unsigned char *packet);

#endif      /* AHCI_H */
