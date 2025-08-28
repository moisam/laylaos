/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: cdrom.h
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
 *  \file cdrom.h
 *
 *  Functions and structure defines for working with CD-ROM devices.
 *  These functions should handle both ATA and AHCI devices.
 */

#ifndef __CDROM_H__
#define __CDROM_H__

#define CD_MAXTRACK                 99
#define CD_BLOCK_OFFSET             150
#define CD_FRAMES                   75
#define CD_SECS                     60

/*
 * Values for byte 2 of the MODE SENSE command packet
 */
#define SENSE_PAGE_AUDIO            0x0E
#define SENSE_PAGE_CODE             0x3F
#define SENSE_PAGE_CTRL             0xC0
#define SENSE_PAGE_CTRL_CURRENT     0x00
#define SENSE_PAGE_CTRL_CHANGEABLE  0x40
#define SENSE_PAGE_CTRL_DEFAULT     0x80
#define SENSE_PAGE_CTRL_SAVED       0xC0

/*
 * Values for byte 4 of the START/STOP UNIT command packet
 */
#define CDROM_UNIT_STOP             0x00
#define CDROM_UNIT_START            0x01
#define CDROM_UNIT_EJECT            0x02


static inline void _lto2b(uint8_t *dest, uint32_t src)
{
    dest[0] = (src >> 8) & 0xff;
    dest[1] = (src & 0xff);
}

static inline void _lto3b(uint8_t *dest, uint32_t src)
{
    dest[0] = (src >> 16) & 0xff;
    dest[1] = (src >> 8) & 0xff;
    dest[2] = (src & 0xff);
}

static inline void _lto4b(uint8_t *dest, uint32_t src)
{
    dest[0] = (src >> 24) & 0xff;
    dest[1] = (src >> 16) & 0xff;
    dest[2] = (src >> 8) & 0xff;
    dest[3] = (src & 0xff);
}

static inline uint32_t _2btol(uint8_t *src)
{
    return ((uint32_t)src[0] << 8) | src[1];
}

static inline uint32_t _3btol(uint8_t *src)
{
    return ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
}

static inline uint32_t _4btol(uint8_t *src)
{
    return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) | ((uint32_t)src[2] << 8) | src[3];
}


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

long cdrom_test_unit_ready(struct ata_dev_s *dev, virtual_addr addr);
long cdrom_request_sense(struct ata_dev_s *dev, virtual_addr addr);
long ahci_cdrom_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel);

#endif      /* __CDROM_H__ */
