/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: scsi.h
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
 *  \file scsi.h
 *
 *  Functions and structure defines for working with SCSI devices.
 */

#ifndef SCSI_H
#define SCSI_H

#include <endian.h>
#include <stdint.h>
#include <string.h>

/*
 * SCSI commands
 */
#define SCSI_CMD_TEST_UNIT_READY        0x00
#define SCSI_CMD_REQUEST_SENSE          0x03
#define SCSI_CMD_INQUIRY                0x12
#define SCSI_CMD_READ_CAPACITY10        0x25
#define SCSI_CMD_READ_CAPACITY16        0x9E
#define SCSI_CMD_READ6                  0x08
#define SCSI_CMD_READ10                 0x28
#define SCSI_CMD_READ12                 0xA8
#define SCSI_CMD_READ16                 0x88
#define SCSI_CMD_WRITE6                 0x0A
#define SCSI_CMD_WRITE10                0x2A
#define SCSI_CMD_WRITE12                0xAA
#define SCSI_CMD_WRITE16                0x8A
#define SCSI_CMD_MODE_SENSE6            0x1A
#define SCSI_CMD_MODE_SENSE10           0x5A
#define SCSI_CMD_MODE_SELECT6           0x15
#define SCSI_CMD_MODE_SELECT10          0x55
#define SCSI_CMD_START_STOP_UNIT        0x1B

/*
 * Swap macros for big endianness
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SWAP32(x)                       __builtin_bswap32(x)
#define SWAP64(x)                       __builtin_bswap64(x)
#else
#define SWAP32(x)                       (x)
#define SWAP64(x)                       (x)
#endif

/*
 * Code for the macro and scsi_command_size_tbl is taken from the Linux sources.
 *
 * See: https://github.com/torvalds/linux/blob/master/drivers/scsi/scsi_common.c
 *      https://github.com/torvalds/linux/blob/master/include/scsi/scsi_common.h
 */
#define SCSI_COMMAND_SIZE(opcode)   scsi_command_size_tbl[((opcode) >> 5) & 7]

extern const unsigned char scsi_command_size_tbl[8];


struct scsi_rw_args_t
{
    uint64_t lba;
    uint32_t sector_count;
    int is_write;
};


struct scsi_ioctl_command_t
{
    unsigned int inlen;
    unsigned int outlen;
    unsigned char data[];
};


// ATA IDENTIFY structure offsets (Partial layout for required fields)
struct ata_identify_t
{
    uint16_t general_config;       // Word 0
    uint16_t reserved1[9];         // Words 1-9
    uint8_t  serial_number[20];    // Words 10-19 (Byte-swapped ASCII)
    uint16_t reserved2[3];         // Words 20-22
    uint8_t  firmware_revision[8]; // Words 23-26 (Byte-swapped ASCII)
    uint8_t  model_number[40];     // Words 27-46 (Byte-swapped ASCII)
    // ... total structure is 512 bytes
} __attribute__((packed));

// Response Layout for SCSI INQUIRY (Minimum 36 Bytes)
struct scsi_inquiry_resp_t
{
    uint8_t  peripheral_device_type : 5; // Bits 0-4: 0x00 for HDD, 0x05 for CD-ROM
    uint8_t  peripheral_qualifier   : 3; // Bits 5-7: 0x00 if connected
    uint8_t  reserved1              : 7;
    uint8_t  rmb                    : 1; // Bit 7: Removable Medium Bit
    uint8_t  version;                    // SPC version (e.g., 0x05 for SPC-3)
    uint8_t  response_data_format   : 4; // Bits 0-3: Must be 2
    uint8_t  hi_sup                 : 1;
    uint8_t  norm_aca               : 1;
    uint8_t  reserved2              : 2;
    uint8_t  additional_length;          // Length of remaining data (31 for a 36-byte payload)
    uint8_t  sccs_reserved[3];
    uint8_t  vendor_id[8];               // 8 bytes ASCII (padded with spaces)
    uint8_t  product_id[16];             // 16 bytes ASCII (padded with spaces)
    uint8_t  product_revision[4];        // 4 bytes ASCII (padded with spaces)
} __attribute__((packed));

// Response layout for SCSI READ CAPACITY (10) - 8 Bytes Total
struct scsi_read_cap10_resp_t
{
    uint32_t returned_lba;     // Big-Endian: Max LBA index (Total Sectors - 1)
    uint32_t block_length;     // Big-Endian: Sector size (usually 512 or 4096)
} __attribute__((packed));

// Response layout for SCSI READ CAPACITY (16) - 32 Bytes Total
struct scsi_read_cap16_resp_t
{
    uint64_t returned_lba;     // Big-Endian: Max LBA index
    uint32_t block_length;     // Big-Endian: Sector size
    uint8_t  prot_type : 3;    // Protection Information
    uint8_t  prot_en   : 1;
    uint8_t  p_i_exponent : 4;
    uint8_t  logical_blocks_per_physical_exponent : 4;
    uint8_t  lowest_aligned_lba_high : 4;
    uint8_t  lowest_aligned_lba_low;
    uint8_t  reserved[22];
} __attribute__((packed));


/*********************************************
 * Internal functions
 *********************************************/

long scsi_to_ata_command(dev_t devid, struct ata_dev_s *dev, char *arg);
int parse_scsi_rw_command(unsigned char *cmdbuf, struct scsi_rw_args_t *out);

#endif      /* SCSI_H */
