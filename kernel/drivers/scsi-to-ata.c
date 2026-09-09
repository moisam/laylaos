/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: scsi-to-ata.c
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
 *  \file scsi-to-ata.c
 *
 *  This file impelements the functions used by the kernel to translate SCSI
 *  commands to ATA commands so they can be passed to disks.
 */

//#define __DEBUG

#include <errno.h>
#include <scsi/scsi.h>
#include <scsi/scsi_ioctl.h>
#include <kernel/laylaos.h>
#include <kernel/ata.h>
#include <kernel/ahci.h>
#include <kernel/scsi.h>
#include <kernel/usb.h>
#include <kernel/user.h>
#include <mm/kheap.h>

const unsigned char scsi_command_size_tbl[8] =
{
	6, 10, 10, 12, 16, 12, 10, 10
};


/*
 * For SATAPI, we simply pass the command to the device, copying data in
 * and out as asked by the user.
 */
static long scsi_atapi_passthrough(struct ata_dev_s *dev, char *arg)
{
    struct scsi_ioctl_command_t *userarg = (struct scsi_ioctl_command_t *)arg;
    uintptr_t tmp_phys, tmp_virt;
    unsigned int inlen, outlen, op, cmdlen;
    long res;
    unsigned char cmdbuf[16];

    COPY_VAL_FROM_USER(&inlen, &userarg->inlen);
    COPY_VAL_FROM_USER(&outlen, &userarg->outlen);
    COPY_VAL_FROM_USER(&op, &userarg->data[0]);

    if(inlen > PAGE_SIZE || outlen > PAGE_SIZE)
    {
        return -EINVAL;
    }

    cmdlen = SCSI_COMMAND_SIZE(op);
    COPY_FROM_USER(cmdbuf, userarg->data, cmdlen);

    if(!(tmp_phys = (uintptr_t)pmmngr_alloc_block()))
    {
        return -ENOMEM;
    }

    tmp_virt = PHYS_TO_HIMEM(tmp_phys);
    A_memset((void *)tmp_virt, 0, PAGE_SIZE);

    if(inlen && copy_from_user((void *)tmp_virt, userarg->data + cmdlen, inlen) != 0)
    {
        pmmngr_free_block((void *)tmp_phys);
        return -EFAULT;
    }

    res = achi_satapi_read_packet(dev, tmp_phys, inlen ? inlen : outlen, 0, 0, cmdbuf, 1);

    if(res == 0)
    {
        if(outlen && copy_to_user(userarg->data, (void *)tmp_virt, outlen) != 0)
        {
            res = -EFAULT;
        }
    }

    pmmngr_free_block((void *)tmp_phys);

    return res;
}


/*
 * Helper to copy and fix byte-swapped ATA strings to standard ASCII.
 */
static void copy_ata_string(uint8_t *dest, const uint8_t *src, size_t len)
{
    size_t i;

    for(i = 0; i < len; i += 2)
    {
        dest[i]     = src[i + 1]; // Swap byte pairs
        dest[i + 1] = src[i];
    }
    
    // Clean up trailing spaces or non-printable garbage if needed
    // SCSI strings must be left-aligned and padded with ASCII spaces (0x20)
    for(i = 0; i < len; i++)
    {
        if(dest[i] < 0x20 || dest[i] > 0x7E)
        {
            dest[i] = 0x20; 
        }
    }
}


/*
 * Helper function to extract the relevant fields from an ATA IDENTIFY buffer
 * and return the equivalent SCSI INQUIRY result.
 */
static void ata_identify_to_scsi_inquiry(char *dest, char *src)
{
    struct ata_identify_t *ata = (struct ata_identify_t *)src;
    struct scsi_inquiry_resp_t *scsi = (struct scsi_inquiry_resp_t *)dest;

    // Clear destination
    A_memset(scsi, 0, sizeof(struct scsi_inquiry_resp_t));

    // Identify Device Type & Removable status
    // Word 0 Bit 7 indicates if the media is removable
    uint16_t gen_config = ata->general_config;
    int is_removable = (gen_config & (1 << 7)) ? 1 : 0;

    // Check if device is ATAPI or SATA
    if((gen_config & 0x8000) && ((gen_config >> 8) & 0x1F) == 0x05)
    {
        scsi->peripheral_device_type = 0x05; // CD/DVD-ROM
        scsi->rmb = 1;                       // Always removable
    }
    else
    {
        scsi->peripheral_device_type = 0x00; // HDD/SSD
        scsi->rmb = is_removable ? 1 : 0;
    }

    scsi->peripheral_qualifier = 0x00;       // Device is connected and valid

    // Set protocol versions
    scsi->version = 0x05;                    // Claim SPC-3 compliance
    scsi->response_data_format = 2;          // Mandatory for modern SCSI
    scsi->additional_length = 31;            // 36 bytes total minus 5 bytes header

    // Map Vendor ID (ATA doesn't have a 'Vendor' string, use generic)
    memcpy(scsi->vendor_id, "ATA     ", 8);

    // Map Product ID (Extract from the first 16 bytes of ATA Model String)
    uint8_t temp_model[40];
    copy_ata_string(temp_model, ata->model_number, 40);
    memcpy(scsi->product_id, temp_model, 16);

    // Map Product Revision (Extract from the first 4 bytes of ATA Firmware String)
    uint8_t temp_fw[8];
    copy_ata_string(temp_fw, ata->firmware_revision, 8);
    memcpy(scsi->product_revision, temp_fw, 4);
}


int parse_scsi_rw_command(unsigned char *cmdbuf, struct scsi_rw_args_t *out)
{
    unsigned char opcode = cmdbuf[0];

    // Determine direction
    if(opcode == SCSI_CMD_WRITE6 || opcode == SCSI_CMD_WRITE10 ||
       opcode == SCSI_CMD_WRITE12 || opcode == SCSI_CMD_WRITE16)
    {
        out->is_write = 1;
    }
    else if(opcode == SCSI_CMD_READ6 || opcode == SCSI_CMD_READ10 ||
            opcode == SCSI_CMD_READ12 || opcode == SCSI_CMD_READ16)
    {
        out->is_write = 0;
    }
    else
    {
        return 0;           // Not a recognized read/write opcode
    }

    switch(opcode)
    {
        case SCSI_CMD_READ6:  // READ(6)
        case SCSI_CMD_WRITE6: // WRITE(6)
            // LBA is spread across bits 0-4 of Byte 1, and all of Bytes 2 and 3
            out->lba = ((uint32_t)(cmdbuf[1] & 0x1F) << 16) |
                       ((uint32_t)cmdbuf[2] << 8) |
                       ((uint32_t)cmdbuf[3]);
            
            out->sector_count = cmdbuf[4];

            if(out->sector_count == 0)
            {
                out->sector_count = 256; // 0 in 6-byte commands means 256
            }
            break;

        case SCSI_CMD_READ10:  // READ(10)
        case SCSI_CMD_WRITE10: // WRITE(10)
            // LBA is in Bytes 2-5
            out->lba = ((uint32_t)cmdbuf[2] << 24) |
                       ((uint32_t)cmdbuf[3] << 16) |
                       ((uint32_t)cmdbuf[4] << 8)  |
                       ((uint32_t)cmdbuf[5]);
            
            // Sector count is in Bytes 7-8
            out->sector_count = ((uint32_t)cmdbuf[7] << 8) | 
                                ((uint32_t)cmdbuf[8]);
            break;

        case SCSI_CMD_READ12:  // READ(12)
        case SCSI_CMD_WRITE12: // WRITE(12)
            // LBA is in Bytes 2-5
            out->lba = ((uint32_t)cmdbuf[2] << 24) |
                       ((uint32_t)cmdbuf[3] << 16) |
                       ((uint32_t)cmdbuf[4] << 8)  |
                       ((uint32_t)cmdbuf[5]);
            
            // Sector count is in Bytes 6-9
            out->sector_count = ((uint32_t)cmdbuf[6] << 24) |
                                ((uint32_t)cmdbuf[7] << 16) |
                                ((uint32_t)cmdbuf[8] << 8)  |
                                ((uint32_t)cmdbuf[9]);
            break;

        case SCSI_CMD_READ16:  // READ(16)
        case SCSI_CMD_WRITE16: // WRITE(16)
            // LBA is a full 64-bit value in Bytes 2-9
            out->lba = ((uint64_t)cmdbuf[2] << 56) | ((uint64_t)cmdbuf[3] << 48) |
                       ((uint64_t)cmdbuf[4] << 40) | ((uint64_t)cmdbuf[5] << 32) |
                       ((uint64_t)cmdbuf[6] << 24) | ((uint64_t)cmdbuf[7] << 16) |
                       ((uint64_t)cmdbuf[8] << 8)  | ((uint64_t)cmdbuf[9]);
            
            // Sector count is in Bytes 10-13
            out->sector_count = ((uint32_t)cmdbuf[10] << 24) |
                                ((uint32_t)cmdbuf[11] << 16) |
                                ((uint32_t)cmdbuf[12] << 8)  |
                                ((uint32_t)cmdbuf[13]);
            break;
    }

    return 1;
}


/*
 * Translate an SCSI command to an ATA command and return the result.
 */
long scsi_to_ata_command(dev_t devid, struct ata_dev_s *dev, char *arg)
{
    struct scsi_ioctl_command_t *userarg = (struct scsi_ioctl_command_t *)arg;
    unsigned int inlen, outlen, op, bytes, cmdlen;
    long res;

    // SATAPI drive
    if(dev->type == IDE_SATAPI)
    {
        return scsi_atapi_passthrough(dev, arg);
    }

    // USB drive
    if(dev->type == IDE_UNKNOWN)
    {
        return scsi_usb_passthrough(dev, arg);
    }

    COPY_VAL_FROM_USER(&inlen, &userarg->inlen);
    COPY_VAL_FROM_USER(&outlen, &userarg->outlen);
    COPY_VAL_FROM_USER(&op, &userarg->data[0]);

    if(inlen > PAGE_SIZE || outlen > PAGE_SIZE)
    {
        return -EINVAL;
    }

    cmdlen = SCSI_COMMAND_SIZE(op);

    unsigned char cmdbuf[cmdlen];

    COPY_FROM_USER(cmdbuf, userarg->data, cmdlen);

    /*
     * TODO: we should check for r/w permissions here and fail with -EPERM.
     */

    switch(op)
    {
        case SCSI_CMD_INQUIRY:
        {
            uintptr_t tmp_phys, tmp_virt;
            struct scsi_inquiry_resp_t resp;

            if(!outlen)
            {
                return -EINVAL;
            }

            if(!(tmp_phys = (uintptr_t)pmmngr_alloc_block()))
            {
                return -ENOMEM;
            }

            tmp_virt = PHYS_TO_HIMEM(tmp_phys);
            A_memset((void *)tmp_virt, 0, PAGE_SIZE);

            if(ahci_sata_identify(dev->ahci, dev->port_index, tmp_phys, dev->type) < 0)
            {
                /*
                 * TODO: get sense data and return it in the buffer.
                 */
                pmmngr_free_block((void *)tmp_phys);
                return -EIO;
            }

            ata_identify_to_scsi_inquiry((char *)&resp, (char *)tmp_virt);
            pmmngr_free_block((void *)tmp_phys);

            bytes = MIN(outlen, sizeof(struct scsi_inquiry_resp_t));
            res = copy_to_user(userarg->data, &resp, bytes);

            return (res != 0) ? -EFAULT : 0;
        }

        case SCSI_CMD_READ_CAPACITY10:
        {
            struct scsi_read_cap10_resp_t resp;
            size_t sectors = dev->size / dev->bytes_per_sector;
            uint64_t max_lba = sectors - 1;

            if(max_lba > 0xFFFFFFFF)
            {
                // Drive is > 2 TiB limit. Return 0xFFFFFFFF to trigger READ CAPACITY (16)
                resp.returned_lba = SWAP32(0xFFFFFFFF);
            }
            else
            {
                resp.returned_lba = SWAP32((uint32_t)max_lba);
            }

            resp.block_length = SWAP32(dev->bytes_per_sector);

            bytes = MIN(outlen, sizeof(struct scsi_read_cap10_resp_t));
            res = copy_to_user(userarg->data, &resp, bytes);

            return (res != 0) ? -EFAULT : 0;
        }

        case SCSI_CMD_READ_CAPACITY16:
        {
            struct scsi_read_cap16_resp_t resp;
            size_t sectors = dev->size / dev->bytes_per_sector;
            uint64_t max_lba = sectors - 1;

            A_memset(&resp, 0, sizeof(struct scsi_read_cap16_resp_t));

            resp.returned_lba = SWAP64(max_lba);
            resp.block_length = SWAP32(dev->bytes_per_sector);

            // Report physical sector alignment if relevant (Word 106)
            if(dev->physlog & 0x2000)
            {       // Multiple logical blocks per physical block
                uint8_t exponent = dev->physlog & 0xF;
                resp.logical_blocks_per_physical_exponent = exponent;
            }

            bytes = MIN(outlen, sizeof(struct scsi_read_cap16_resp_t));
            res = copy_to_user(userarg->data, &resp, bytes);

            return (res != 0) ? -EFAULT : 0;
        }

        case SCSI_CMD_READ6:
        case SCSI_CMD_READ10:
        case SCSI_CMD_READ12:
        case SCSI_CMD_READ16:
        case SCSI_CMD_WRITE6:
        case SCSI_CMD_WRITE10:
        case SCSI_CMD_WRITE12:
        case SCSI_CMD_WRITE16:
        {
            uintptr_t tmp_phys, tmp_virt;
            struct scsi_rw_args_t rwargs = {0, };
            struct disk_req_t req;

            if(!parse_scsi_rw_command(cmdbuf, &rwargs))
            {
                return -EINVAL;
            }

            if((rwargs.sector_count * dev->bytes_per_sector) > PAGE_SIZE)
            {
                return -EINVAL;
            }

            if(!(tmp_phys = (uintptr_t)pmmngr_alloc_block()))
            {
                return -ENOMEM;
            }

            tmp_virt = PHYS_TO_HIMEM(tmp_phys);
            A_memset((void *)tmp_virt, 0, PAGE_SIZE);

            if(rwargs.is_write)
            {
                if(!inlen)
                {
                    pmmngr_free_block((void *)tmp_phys);
                    return -EINVAL;
                }

                if(copy_from_user((void *)tmp_virt, userarg->data + cmdlen, inlen) != 0)
                {
                    pmmngr_free_block((void *)tmp_phys);
                    return -EFAULT;
                }
            }
            else
            {
                if(!outlen)
                {
                    pmmngr_free_block((void *)tmp_phys);
                    return -EINVAL;
                }
            }

            req.dev = devid;
            req.data = tmp_virt;
            req.datasz = rwargs.sector_count * dev->bytes_per_sector;
            req.fs_blocksz = dev->bytes_per_sector;
            req.blockno = rwargs.lba;
            req.write = rwargs.is_write;

            res = ahci_strategy(&req);

            if(res > 0)
            {
                if(rwargs.is_write)
                {
                    res = 0;
                }
                else
                {
                    res = copy_to_user(userarg->data, (void *)tmp_virt, outlen);
                }
            }

            pmmngr_free_block((void *)tmp_phys);

            return res;
        }

        default:
            return -EINVAL;
    }
}

