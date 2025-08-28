/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: gpt_mbr.h
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
 *  \file gpt_mbr.h
 *
 *  Functions and macros for getting various information bits from Master
 *  Boot Record (MBR) and GUID Partition Table (GPT) disk entries. These are
 *  used by various disk drivers including ATA, AHCI and loopback device
 *  drivers.
 */

#ifndef GPT_MBR_H
#define GPT_MBR_H

/* partition table offsets */
static int mbr_offset[] = { 0x1be, 0x1ce, 0x1de, 0x1ee };
static uint8_t gpt_hdr_magic[] = "EFI PART";


static struct parttab_s *part_from_mbr_buf(uint8_t *ide_buf, int i)
{
    struct parttab_s *part = kmalloc(sizeof(struct parttab_s));

    if(part)
    {
        part->attribs        = ide_buf[mbr_offset[i] + 0] & 0xff;
        //part->bootable       = ide_buf[mbr_offset[i] + 0] & 0x80;
        part->start_head     = ide_buf[mbr_offset[i] + 1] & 0xff;
        part->start_sector   = ide_buf[mbr_offset[i] + 2] & 0x3f;
        part->start_cylinder = ide_buf[mbr_offset[i] + 3] |
                                ((ide_buf[mbr_offset[i] + 2] & 0xc0)<<8);
        part->system_id      = ide_buf[mbr_offset[i] + 4] & 0xff;
        part->end_head       = ide_buf[mbr_offset[i] + 5] & 0xff;
        part->end_sector     = ide_buf[mbr_offset[i] + 6] & 0x3f;
        part->end_cylinder   = ide_buf[mbr_offset[i] + 7] |
                                ((ide_buf[mbr_offset[i] + 6] & 0xc0)<<8);
        part->lba            = get_dword(ide_buf + mbr_offset[i] + 8);
        part->total_sectors  = get_dword(ide_buf + mbr_offset[i] + 12);
        part->dev = NULL;

        /*
        printk("    Partition %d\n", i+1);
        printk("      lba bytes 0x%x 0x%x 0x%x 0x%x\n",
                ide_buf[offset[i] + 8], ide_buf[offset[i] + 9],
                ide_buf[offset[i] + 10], ide_buf[offset[i] + 11]);
        printk("      attribs 0x%x (bootable %d)\n",
                part->attribs, (part->attribs & 0x80));
        printk("      start_head %u\n", part->start_head);
        printk("      start_sector %u\n", part->start_sector);
        printk("      start_cylinder %u\n", part->start_cylinder);
        printk("      system_id 0x%x\n", part->system_id);
        printk("      end_head %u\n", part->end_head);
        printk("      end_sector %u\n", part->end_sector);
        printk("      end_cylinder %u\n", part->end_cylinder);
        printk("      lba %u\n", part->lba);
        printk("      total_sectors %u\n", part->total_sectors);
        */
    }

    return part;
}


static struct parttab_s *part_from_gpt_ent(struct gpt_part_entry_t *ent)
{
    struct parttab_s *part = kmalloc(sizeof(struct parttab_s));

    if(part)
    {
        /*
         * NOTE: We do not process the attributes correctly here.
         *       Of note, the attribs field is 8 bytes long and we only
         *       store the first byte here.
         */
        part->attribs        = ent->attribs & 0xff;
        part->start_head     = 0;
        part->start_sector   = 0;
        part->start_cylinder = 0;
        part->system_id      = 0;
        part->end_head       = 0;
        part->end_sector     = 0;
        part->end_cylinder   = 0;
        part->lba            = ent->lba_start;
        part->total_sectors  = ent->lba_end - ent->lba_start;
        part->dev = NULL;

        /*
        printk("    Partition %d\n", dev_index);
        printk("      attribs 0x%x\n", part->attribs);
        printk("      lba %u\n", part->lba);
        printk("      total_sectors %u\n", part->total_sectors);
        */
    }

    return part;
}


static uint32_t get_gpthdr_lba(uint8_t *ide_buf)
{
    volatile int i;
    uint32_t gpthdr_lba = 0;

    for(i = 0; i < 4; i++)
    {
        // Check for GPT partition table signature
        if(ide_buf[mbr_offset[i] + 4] == 0xEE)
        {
            // The LBA of the GPT Partition Table Header is found at offset 8,
            // and is 4 bytes long (ideally should be 0x00000001).
            gpthdr_lba = get_dword(ide_buf + mbr_offset[i] + 8);
            break;
        }
    }

    return gpthdr_lba;
}


static int valid_gpt_signature(uint8_t *ide_buf)
{
    volatile int i;

    for(i = 0; i < 8; i++)
    {
        if(ide_buf[i] != gpt_hdr_magic[i])
        {
            printk("  Skipping disk with invalid GPT signature: '");
            printk("%c%c%c%c%c%c%c%c'\n", ide_buf[0], ide_buf[1], ide_buf[2],
                                          ide_buf[3], ide_buf[4], ide_buf[5],
                                          ide_buf[6], ide_buf[7]);
            return 0;
        }
    }

    return 1;
}


static int unused_gpt_entry(struct gpt_part_entry_t *ent)
{
    volatile int i;

    for(i = 0; i < 16; i++)
    {
        if(ent->guid[i] != 0)
        {
            break;
        }
    }

    return (i == 16);
}


__attribute__((unused))
static unsigned long long part_or_disk_size(struct ata_dev_s *dev, struct parttab_s *part)
{
    unsigned long long res;

    if(part == NULL)
    {
        // check for a whole disk
        res = dev->size ? dev->size :
                            (dev->sectors * dev->bytes_per_sector);
    }
    else
    {
        // get partition size
        res = part->total_sectors * dev->bytes_per_sector;
    }

    return res;
}

#endif      /* GPT_MBR_H */
