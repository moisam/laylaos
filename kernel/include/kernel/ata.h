/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ata.h
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
 *  \file ata.h
 *
 *  Functions and structure defines for working with ATA (Advanced Technology 
 *  Attachment) devices.
 */

#ifndef __ATA_H__
#define __ATA_H__

#include <stddef.h>
#include <stdint.h>
#include <kernel/rtc.h>		/* systime_t */
#include <kernel/pci.h>
#include <kernel/pcache.h>

/* disk types as recognised by the kernel */
#define IDE_PATA                    0x00
#define IDE_PATAPI                  0x01
#define IDE_SATA                    0x02
#define IDE_SATAPI                  0x03
#define IDE_UNKNOWN                 0xFF

/* I/O base registers */
#define ATA_REG_DATA                0x0000
#define ATA_REG_ERR                 0x0001
#define ATA_REG_FEATURE             0x0001
#define ATA_REG_SECTORCNT           0x0002
#define ATA_REG_SECTOR              0x0003
#define ATA_REG_TRACKLSB            0x0004
#define ATA_REG_TRACKMSB            0x0005
#define ATA_REG_DRVHD               0x0006
#define ATA_REG_STATUS              0x0007
#define ATA_REG_COMMAND             0x0007
#define ATA_REG_DEVCTRL             0x0008

/* Control registers */
#define ATA_REG_ALT_STATUS          0x0000
#define ATA_REG_CONTROL             0x0000

/* Status bits */
#define ATA_SR_BUSY                 0x80    // Busy
#define ATA_SR_DRDY                 0x40    // Drive ready
#define ATA_SR_DF                   0x20    // Drive write fault
#define ATA_SR_DSC                  0x10    // Drive seek complete
#define ATA_SR_DRQ                  0x08    // Data request ready
#define ATA_SR_CORR                 0x04    // Corrected data
#define ATA_SR_IDX                  0x02    // Index
#define ATA_SR_ERR                  0x01    // Error

/* Error status bits */
#define ATA_ER_BBK                  0x80    // Bad block
#define ATA_ER_UNC                  0x40    // Uncorrectable data
#define ATA_ER_MC                   0x20    // Media changed
#define ATA_ER_IDNF                 0x10    // ID mark not found
#define ATA_ER_MCR                  0x08    // Media change request
#define ATA_ER_ABRT                 0x04    // Command aborted
#define ATA_ER_TK0NF                0x02    // Track 0 not found
#define ATA_ER_AMNF                 0x01    // No address mark

/* Device control bits */
#define NIEN                        0x02
#define SRST                        0x04

/* IDE commands */
#define ATA_CMD_READ_PIO            0x20
#define ATA_CMD_READ_PIO_EXT        0x24
#define ATA_CMD_READ_DMA            0xC8
#define ATA_CMD_READ_DMA_EXT        0x25
#define ATA_CMD_WRITE_PIO           0x30
#define ATA_CMD_WRITE_PIO_EXT       0x34
#define ATA_CMD_WRITE_DMA           0xCA
#define ATA_CMD_WRITE_DMA_EXT       0x35
#define ATA_CMD_CACHE_FLUSH         0xE7
#define ATA_CMD_CACHE_FLUSH_EXT     0xEA
#define ATA_CMD_PACKET              0xA0
#define ATA_CMD_IDENTIFY_PACKET     0xA1
#define ATA_CMD_IDENTIFY            0xEC
#define ATA_CMD_SET_FEATURES        0xEF

#define ATAPI_CMD_MEDIA_LOCK        0xDE
#define ATAPI_CMD_TEST_UNIT_READY   0x00
#define ATAPI_CMD_REQUEST_SENSE     0x03

#define ATAPI_CMD_READ              0xA8
#define ATAPI_CMD_EJECT             0x1B

/* Feature commands */
#define ATA_FEAT_ENABLE_WCACHE      0x02  // Enable write caching
#define ATA_FEAT_XFER_MODE          0x03  // Set transfer mode
#define ATA_FEAT_DISABLE_RLA        0x55  // Disable read-lookahead
#define ATA_FEAT_DISABLE_WCACHE     0x82  // Disable write caching
#define ATA_FEAT_ENABLE_RLA         0xAA  // Enable read-lookahead

/* Identification space offsets */
#define ATA_IDENT_DEVICETYPE        0
#define ATA_IDENT_CYLINDERS         2
#define ATA_IDENT_HEADS             6
#define ATA_IDENT_BYTES_PER_SECTOR  10
#define ATA_IDENT_SECTORS           12
#define ATA_IDENT_SERIAL            20
#define ATA_IDENT_MODEL             54
#define ATA_IDENT_CAPABILITIES      98
#define ATA_IDENT_FIELDVALID        106
#define ATA_IDENT_MAX_LBA           120
#define ATA_IDENT_COMMANDSETS       164
#define ATA_IDENT_UDMA_MODE         176
#define ATA_IDENT_MAX_LBA_EXT       200

/* Transfer modes */
#define ATA_XFER_MODE_PIO           0x00
#define ATA_XFER_MODE_WDMA          0x20
#define ATA_XFER_MODE_UDMA          0x40

/* Bus master registers */
#define ATA_BUS_MASTER_REG_COMMAND  0x00
#define ATA_BUS_MASTER_REG_STATUS   0x02
#define ATA_BUS_MASTER_REG_PRDT     0x04

/* Bus master status register bits */
#define ATA_DMA_END                 0x00
#define ATA_DMA_START               0x01
#define ATA_DMA_ERROR               0x02
#define ATA_IRQ_PENDING             0x04
#define ATA_MASTER_DMA_INITED       0x20
#define ATA_SLAVE_DMA_INITED        0x40

/* Wait timeouts */
#define TIMEOUT_DRDY                50000
#define TIMEOUT_DRQ                 50000
#define TIMEOUT_BUSY                60000

/* 400ns delays */
#define ata_delay(port)                                     \
{                                                           \
    inb(port); inb(port); inb(port); inb(port); inb(port);  \
    inb(port); inb(port); inb(port); inb(port); inb(port);  \
    inb(port); inb(port); inb(port); inb(port); inb(port);  \
    inb(port); inb(port); inb(port); inb(port); inb(port);  \
    inb(port); inb(port); inb(port); inb(port); inb(port);  \
    inb(port); inb(port); inb(port); inb(port); inb(port);  \
    inb(port); inb(port); inb(port); inb(port); inb(port);  \
    inb(port); inb(port); inb(port); inb(port); inb(port);  \
}

/*
#define ata_delay(port)  \
{                        \
    int cnt = 4;         \
    do { inb(port); }    \
    while(--cnt);        \
}
*/

// macro to extract the primary/secondary bit
#define PS(dev)     (((dev)->masterslave & 2) >> 1)

// macro to extract the master/slave bit
#define MS(dev)     ((dev)->masterslave & 1)


// macros to get words and dwords from the MBR
#define U16(buf, i)         (unsigned int)((buf[i] | (buf[i+1]<<8)) & 0xffff)
#define U32(buf, i)         (unsigned int)(buf[i] | (buf[i+1]<<8) |         \
                                           (buf[i+2]<<16) | (buf[i+3]<<24))

#define get_dword(buf)	((uint32_t)(*(buf)) | (uint32_t)(*(buf+1)<<8) | \
                        ((uint32_t)(*(buf+2))<<16) | (uint32_t)(*(buf+3)<<24))


#ifndef ATAPI_SECTOR_SIZE
#define ATAPI_SECTOR_SIZE	        2048
#endif


/**
 * \def MAX_ATA_DEVICES
 *
 * maximum number of supported ATA devices
 */
#define MAX_ATA_DEVICES             64 * 2


/**
 * @struct parttab_s
 * @brief The parttab_s structure.
 *
 * A structure to represent an entry in the disk partition table.
 */
struct parttab_s
{
    char attribs;            /**< attributes - partition is bootable if bit 7
                                  is set (i.e. attribs & 0x80 == 0x80) */
    char start_head;         /**< start head */
    char start_sector;       /**< start sector */
    uint16_t start_cylinder; /**< start cylinder */
    char system_id;          /**< OS identifier */
    char end_head;           /**< end head */
    char end_sector;         /**< end sector */
    uint16_t end_cylinder;   /**< end cylinder */
    size_t lba;              /**< start Logical Block Address (LBA) */
    size_t total_sectors;    /**< total sectors in partition */
    struct ata_dev_s *dev;   /**< back pointer to the device to which this
                                  partition belongs */
};


/**
 * @struct ata_dev_s
 * @brief The ata_dev_s structure.
 *
 * A structure to represent an ATA device.
 */
struct ata_dev_s
{
    //unsigned char status;
    unsigned char type;      /**< ATA device type */
    unsigned int cylinders,  /**< total cylinders */
                 sectors;    /**< total sectors */
    unsigned char heads;     /**< total heads */
    //size_t total_sectors;
    size_t size;             /**< size in bytes */
    char serial[21];         /**< device serial number */
    char firmware[9];        /**< device firmware string */
    char model[41];          /**< device model string */
    uint16_t ctrl,           /**< CTRL register of the device */
             base;           /**< BASE register of the device */
    uint16_t bmide;          /**< Base of 8 I/O ports for Bus Master IDE */
    unsigned char nien;      /**< nIEN (No Interrupt) */
    unsigned char uses_dma;  /**< non-zero if device uses DMA */
    //uint32_t abar;
    int irq;                 /**< IRQ number */
    size_t bytes_per_sector; /**< bytes per sector */
    //systime_t last_access_time;

#define PRIMARY_MASTER		0
#define PRIMARY_SLAVE		1
#define SECONDARY_MASTER	2
#define	SECONDARY_SLAVE		3
    int masterslave;         /**< 0: primary master,
                                  1: primary slave,
                                  2: secondary master,
                                  3: secondary slave */

    //struct ata_dev_s *next;
    unsigned short sign;         /**< device signature */
    unsigned short capabilities; /**< device capabilities */
    unsigned int commandsets;    /**< device commandsets */
    
    physical_addr dma_buf_phys, PRDT_phys;  /**< physical address of DMA
                                                 buffer and PRDT */
    virtual_addr dma_buf_virt, PRDT_virt;   /**< virtual address of DMA
                                                 buffer and PRDT */
    size_t dma_buf_size;         /**< DMA buffer size */
    
    // these fields are used by AHCI disks
    struct ahci_dev_t *ahci;     /**< pointer to AHCI device (obviously only
                                      used by AHCI devices) */
    int port_index;              /**< AHCI port index (obviously only
                                      used by AHCI devices) */
};


/**
 * @struct ata_devtab_s
 * @brief The ata_devtab_s structure.
 *
 * A structure to represent an ATA device table. The kernel has 2 ATA tables,
 * one is for devices with major devid == 3, the other is for devices with
 * major devid == 22.
 */
struct ata_devtab_s
{
    struct ata_dev_s *dev[MAX_ATA_DEVICES];     /**< table of ATA devices */
    struct parttab_s *part[MAX_ATA_DEVICES];    /**< table of ATA device
                                                     partitions */
};


/**
 * @struct gpt_part_entry_t
 * @brief The gpt_part_entry_t structure.
 *
 * A structure to represent an entry in a disk's GUID Partition Table (GPT).
 */
struct gpt_part_entry_t
{
    uint8_t guid[16];
    uint8_t uuid[16];
    uint64_t lba_start, lba_end;
    uint64_t attribs;
    uint8_t name[];
};


/********************************
 * Functions defined in ata2.c
 ********************************/

/**
 * @brief Initialise ATA disk devices.
 *
 * Probe the given \a pci device, which should be an ATA controller, to find
 * the types of ATA devices attached to it, and initialize those devices.
 *
 * @param   pci     PCI structure of the ATA device to initialize
 *
 * @return  nothing.
 */
void ata_init(struct pci_dev_t *pci);

/**
 * @brief General block device control function.
 *
 * Perform ioctl operations on an ATA device.
 *
 * @param   dev     device id
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int ata_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel);


/**********************************
 * Functions defined in ata_rw.c
 **********************************/

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
int ata_strategy(struct disk_req_t *req);

/**
 * @brief Wait on an ATA device.
 *
 * Wait until an ATA device is ready and the status register is equal to the
 * given \a mask. The function uses a \a timeout to avoid sleeping forever
 * (if that is what the caller wants).
 *
 * @param   dev     ATA device
 * @param   mask    wait until the device's status register is equal to
 *                    this mask
 * @param   timeout timeout counter
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int ata_wait(struct ata_dev_s *dev, unsigned char mask, unsigned int timeout);

/**
 * @brief Read sectors from an ATA device.
 *
 * Read \a numsects sectors, starting at sector \a lba, into the buffer
 * pointed to by \a buf, from ATA device \a dev.
 *
 * @param   dev         ATA device
 * @param   numsects    number of sectors to read
 * @param   lba         Logical Block Address (LBA) of first sector to read
 * @param   buf         buffer to read into
 *
 * @return  zero on success, -(errno) on failure.
 */
int ata_read_sectors(struct ata_dev_s *dev, unsigned char numsects,
                     size_t lba, virtual_addr buf);

/**
 * @brief Write sectors to an ATA device.
 *
 * Write \a numsects sectors, starting at sector \a lba, from the buffer
 * pointed to by \a buf, to ATA device \a dev.
 *
 * @param   dev         ATA device
 * @param   numsects    number of sectors to write
 * @param   lba         Logical Block Address (LBA) of first sector to write
 * @param   buf         buffer to write from
 *
 * @return  zero on success, -(errno) on failure.
 */
int ata_write_sectors(struct ata_dev_s *dev, unsigned char numsects,
                      size_t lba, virtual_addr buf);


int atapi_test_unit_ready(struct ata_dev_s *dev, virtual_addr addr);
int atapi_request_sense(struct ata_dev_s *dev, virtual_addr addr);


/**********************************
 * Functions defined in ata_irq.c
 **********************************/

/**
 * @var disk_task
 * @brief kernel disk task.
 *
 * The kernel task that handles disk I/O requests.
 */
extern struct task_t *disk_task;

/**
 * @brief Request an ATA I/O operation.
 *
 * Add an I/O request to the ATA request list and unblock the kernel disk
 * task (if needed) to perform the request. The function sleeps until an
 * IRQ is received and the disk sector is read or written.
 *
 * @param   dev         ATA device
 * @param   lba         Logical Block Address (LBA) of the disk sector to
 *                        read or write
 * @param   numsects    number of disk sectors to read or write
 * @param   buf         buffer to read/write from/to
 * @param   write       non-zero if requesting a write, zero for reads
 * @param   func        optional function to handle the read/write request
 *                        (used in special places, like the CD-ROM driver
 *                         when it is sending a media sense packet). Either
 *                         \a buf OR \a func should be passed, NOT BOTH
 *
 * @return  count of bytes read or written on success, -(errno) on failure.
 *
 * @see     disk_task
 */
int ata_add_req(struct ata_dev_s *dev,
                size_t lba, unsigned char numsects,
                virtual_addr buf, int write,
                int (*func)(struct ata_dev_s *, virtual_addr));

/**
 * @brief Kernel disk task function.
 *
 * This is the function that is run by the kernel disk task (\ref disk_task).
 * It runs indefinitely, serving I/O requests and sleeping when there is no
 * disk activity.
 *
 * @param   arg     unused (all kernel callback functions require a single
 *                          argument of type void pointer)
 *
 * @return  nothing.
 *
 * @see     disk_task
 */
void disk_task_func(void *arg);

/**
 * @brief Wait for a disk IRQ.
 *
 * Wait for an IRQ from an ATA disk. This is frequently done during read and
 * write operations.
 *
 * @return  0 if IRQ is received, -(errno) otherwise.
 */
int ide_wait_irq(void);

/**
 * @brief Disk IRQ callback function.
 *
 * Called when an ATA disk raises an IRQ.
 *
 * @param   r       current CPU registers
 * @param   arg     unused (other IRQ callback functions use this to identify
 *                    the unit that raised the IRQ)
 *
 * @return  1 if the IRQ was handled, 0 if not.
 */
int ide_irq_callback(struct regs *r, int arg);


/****************************************
 * Functions defined in dev/blk/cdrom.c
 ****************************************/

/**
 * @brief Add a CD-ROM device node.
 *
 * After a CD-ROM device (PATAPI or SATAPI) is identified, this function is
 * called to add a cdrom device node under /dev, e.g. /dev/cdrom0 to identify
 * the first CD-ROM device and so on.
 *
 * @param   dev_id  device id
 * @param   mode    access mode bits
 *
 * @return  nothing.
 */
void add_cdrom_device(dev_t dev_id, mode_t mode);

#endif      /* __ATA_H__ */
