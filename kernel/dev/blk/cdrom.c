/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: cdrom.c
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
 *  \file cdrom.c
 *
 *  This file impelements a single function, add_cdrom_device(), which is
 *  used to create CD-ROM device noder under /dev.
 */

//#define __DEBUG
#include <sys/cdio.h>
#include <sys/scsiio.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/ata.h>
#include <kernel/dev.h>
#include <kernel/task.h>
#include <kernel/ahci.h>
#include <kernel/cdrom.h>
#include <kernel/io.h>
#include <fs/devfs.h>
#include <mm/kheap.h>
#include <mm/mmap.h>

#define MAX_CDROM_DEVICES           16

struct cdrom_t
{
    dev_t dev;
    char name[8];
};

static struct cdrom_t cdroms[MAX_CDROM_DEVICES] = { { 0, }, };
static int last_index = 0;
static volatile struct task_t *cdrom_task = NULL;

extern struct ata_devtab_s tab1;     // for devices with maj == 3
extern struct ata_devtab_s tab2;     // for devices with maj == 22


/*
 * Structure to represent the data returned by the REQUEST SENSE command.
 */
struct sense_data_t
{
    uint8_t err_code;
    uint8_t reserved1;
    uint8_t sense_key;
    uint32_t info;
    uint8_t additional_sense_len;
    uint32_t cmd_specific_info;
    uint8_t additional_sense_code;
    uint8_t additional_sense_code_qualifier;
    uint8_t field_replacable_unit_code;
    uint8_t sense_key_specific[3];
    uint8_t additional_sense_bytes[1];
} __attribute__((packed));


struct cd_toc_t
{
    struct ioc_toc_header header;
    struct cd_toc_entry entries[CD_MAXTRACK + 1];  /* +1 for the leadout */
} __attribute__((packed));


struct cd_audio_page_t
{
    uint8_t pg_code;
    uint8_t pg_length;
    uint8_t flags;
    uint8_t unused[2];
    uint8_t format_lba; /* valid only for SCSI CDs */
    uint8_t lb_per_sec[2];
    struct port_control
    {
        uint8_t channels;
        uint8_t volume;
    } port[4];
} __attribute__((packed));


void cdrom_task_func(void *arg)
{
    int i, res;
    uint8_t buf[2];
    struct ata_devtab_s *tab;
    struct ata_dev_s *dev;

    UNUSED(arg);

    for(;;)
    {
        if(disk_task == NULL)
        {
            // we are too early as the disk task has not been forked yet
            block_task2(&cdrom_task, PIT_FREQUENCY * 5);
            continue;
        }

        for(i = 0; i < MAX_CDROM_DEVICES; i++)
        {
            KDEBUG("cdrom_task_func: devid[%d] = 0x%x\n", i, cdroms[i].dev);

            if(cdroms[i].dev == 0)
            {
                continue;
            }

            dev = NULL;

            if(MAJOR(cdroms[i].dev) == 3 || MAJOR(cdroms[i].dev) == 22)
            {
                tab = (MAJOR(cdroms[i].dev) == 3) ? &tab1 : &tab2;
                dev = tab->dev[MINOR(cdroms[i].dev)];
            }
            else if(MAJOR(cdroms[i].dev) == AHCI_CDROM_MAJ)
            {
                dev = ahci_cdrom_dev[MINOR(cdroms[i].dev)];
            }

            if(!dev)
            {
                continue;
            }

            KDEBUG("cdrom: cdrom_task_func() checking (maj %d)\n", MAJOR(cdroms[i].dev));

            // Send a TEST UNIT READY command to the ATAPI drive to 
            // check if there is a disc. If there isn't (or the disc has
            // been ejected or changed), the ERR bit should be set in
            // the status register and the Media Change Request bit should
            // be set in the error register. We follow this by the 
            // REQUEST SENSE command to find out the details
            //
            // See: https://cygnus.speccy.cz/download/datasheety/atapi.pdf

            KDEBUG("cdrom: cdrom_task_func() adding disk req\n");
            res = ata_add_req(dev, 0, 1, (virtual_addr)&buf,
                                            0, cdrom_test_unit_ready);

            KDEBUG("cdrom: res %d, status 0x%x, err 0x%x\n", res, buf[0], buf[1]);

            if(/* (buf[0] & ATA_SR_ERR) && */ (buf[1] & ATA_ER_MC))
            {
                struct sense_data_t sense_data;

                A_memset(&sense_data, 0, sizeof(struct sense_data_t));
                res = ata_add_req(dev, 0, 1, (virtual_addr)&sense_data,
                                                0, cdrom_request_sense);

                KDEBUG("cdrom: res %d, errcode 0x%x, key 0x%x, asc 0x%x\n",
                            res, sense_data.err_code, 
                            (sense_data.sense_key & 0x0f), 
                            sense_data.additional_sense_code);

                // check data is valid
                if(!(sense_data.err_code & 0x80))
                {
                    continue;
                }

                // 0xf0 = 0x70 | 0x80 => current error
                // 0xf1 = 0x71 | 0x80 => deferred error
                if(sense_data.err_code != 0xf0 &&
                   sense_data.err_code != 0xf1)
                {
                    continue;
                }

                // SENSE KEY values:
                //   0x00    No Sense (ok)
                //   0x01    Recovered Error
                //   0x02    Not Ready
                //   0x03    Medium Error
                //   0x04    Hardware Error
                //   0x05    Illegal Request
                //   0x06    Unit Attention (reset/medium change)
                //   0x07    Data Protect
                //   0x08    Reserved
                //   0x09    Reserved
                //   0x0A    Reserved
                //   0x0B    Aborted Command
                //   0x0E    Miscompare
                //   0x0F    Reserved
                //
                // See: https://cygnus.speccy.cz/download/datasheety/atapi.pdf

                sense_data.sense_key &= 0x0f;

                // Some ADDITIONAL SENSE CODES of interest:
                //   0x28    Medium May Have Changed
                //   0x30    Cannot Read Medium
                //   0x3A    Medium Not Present

                if(sense_data.sense_key == 0x02 ||
                   sense_data.additional_sense_code == 0x30 ||
                   sense_data.additional_sense_code == 0x3A)
                {
                    // no media - unmount device if still mounted
                    if(get_mount_info(cdroms[i].dev))
                    {
                        printk("cdrom: unmounting cdrom\n");

                        if(vfs_umount(cdroms[i].dev, 0) == 0)
                        {
                            //cdroms[i].dev = 0;
                            dev->size = 0;
                            dev->bytes_per_sector = ATAPI_SECTOR_SIZE;
                        }
                    }
                }
                else if(sense_data.sense_key == 0x06 ||
                        sense_data.additional_sense_code == 0x28)
                {
                    // maybe new media - try to remount the device
                    // using info from fstab
                    if(!get_mount_info(cdroms[i].dev))
                    {
                        //struct dirent *entry;
                        char name[32];

                        printk("cdrom: (re)mounting cdrom\n");

                        /*
                            if(devfs_find_deventry(cdrom_devid[i],
                                                         1, &entry) != 0)
                            {
                                continue;
                            }

                            ksprintf(name, 32, "/dev/%s", entry->d_name);
                            kfree(entry);
                        */

                        ksprintf(name, 32, "/dev/%s", cdroms[i].name);
                        printk("cdrom: cdrom dev '%s'\n", name);
                        mount_internal("cdrom", name, 0);
                    }
                }

                //screen_refresh(NULL);
                //__asm__ __volatile__("xchg %%bx, %%bx"::);
            }
        }

        block_task2(&cdrom_task, PIT_FREQUENCY * 5);
    }
}


/*
 * Add a CD-ROM device node.
 */
void add_cdrom_device(dev_t dev_id, mode_t mode)
{
    //char buf[8];
    
    if(last_index >= MAX_CDROM_DEVICES)
    {
        return;
    }
    
    /*
    ksprintf(buf, 8, "cdrom%d", last_index);
    add_dev_node(buf, dev_id, mode);
    cdrom_devid[last_index++] = dev_id;
    */
    ksprintf(cdroms[last_index].name, 8, "cdrom%d", last_index);
    add_dev_node(cdroms[last_index].name, dev_id, mode);
    cdroms[last_index].dev = dev_id;
    last_index++;

    // fork the cdrom task (if not done already)
    if(cdrom_task == NULL)
    {
        (void)start_kernel_task("cdrom", cdrom_task_func, NULL,
                                &cdrom_task, 0 /* KERNEL_TASK_ELEVATED_PRIORITY */);
    }
}


/*
 * Send a TEST UNIT READY (0x00) command to the ATAPI device.
 * Parameter addr should point to a 2-byte buffer. The status register
 * will be returned in the first byte and the error register will be
 * returned in the second byte.
 */
long cdrom_test_unit_ready(struct ata_dev_s *dev, virtual_addr addr)
{
    unsigned char packet[12];
    long res;
    int status, err;
    uint8_t *buf = (uint8_t *)addr;

    if(!dev)
    {
        printk("cdrom: cdrom_test_unit_ready() invalid device\n");
        return -ENODEV;
    }

    if(!(dev->type & 1))     // PATA or SATA
    {
        printk("cdrom: cdrom_test_unit_ready() device is not SATAPI/ATAPI\n");
        return -ENODEV;
    }

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_TEST_UNIT_READY;

    if(dev->type == IDE_PATAPI)
    {
        res = atapi_read_packet(dev, packet, 12, NULL, 0, 1);
        status = inb(dev->base + ATA_REG_STATUS);
        err = inb(dev->base + ATA_REG_ERR);
    }
    else
    {
        HBA_MEM *hba = (HBA_MEM *)dev->ahci->iobase;
        HBA_PORT *port = &hba->ports[dev->port_index];

        res = achi_satapi_read_packet_virt(dev, 0, 0, 0, 0, packet);
        status = port->ssts;
        err = port->serr;
    }

    KDEBUG("cdrom: cdrom_test_unit_ready() status 0x%x, err 0x%x\n", status, err);

    buf[0] = status;
    buf[1] = err;
    //screen_refresh(NULL);
    //for(;;);

    return res;
}


/*
 * Send a REQUEST SENSE (0x03) command to the ATAPI device.
 * Parameter addr should point to a buffer that is 18 bytes in size.
 * The sense data returned by the device will be stored there.
 */
long cdrom_request_sense(struct ata_dev_s *dev, virtual_addr addr)
{
    unsigned char packet[12];

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_REQUEST_SENSE;
    packet[4] = 18;

    if(dev->type == IDE_PATAPI)
    {
        return atapi_read_packet(dev, packet, 12, (void *)addr, 18, 1);
    }
    else
    {
        return achi_satapi_read_packet_virt(dev, addr, 18, 0, 0, packet);
    }
}


static long cdrom_mode_sense(struct ata_dev_s *dev, void *buf, int page, int sz)
{
    unsigned char packet[12];

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_MODE_SENSE;
    packet[1] = page;
    _lto2b(&packet[7], sz);

    if(dev->type == IDE_PATAPI)
    {
        return atapi_read_packet(dev, packet, 12, buf, sz, 0);
    }
    else
    {
        return achi_satapi_read_packet_virt(dev, page, sz, 0, 0, packet);
    }
}


static long cdrom_mode_select(struct ata_dev_s *dev, void *buf, int page, int sz)
{
    unsigned char packet[12];

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_MODE_SELECT;
    packet[1] = (1 << 4);
    packet[2] = page;
    _lto2b(&packet[7], sz);

    if(dev->type == IDE_PATAPI)
    {
        return atapi_write_packet(dev, packet, 12, buf, sz, 0);
    }
    else
    {
        return achi_satapi_write_packet_virt(dev, page, sz, 0, 0, packet);
    }
}


STATIC_INLINE long copy_arg(void *dest, void *src, size_t sz, int kernel)
{
    if(kernel)
    {
        A_memcpy(dest, src, sz);
        return 0;
    }
    else
    {
        return copy_from_user(dest, src, sz);
    }
}


STATIC_INLINE long copy_res(void *dest, void *src, size_t sz, int kernel)
{
    if(kernel)
    {
        A_memcpy(dest, src, sz);
        return 0;
    }
    else
    {
        return copy_to_user(dest, src, sz);
    }
}


static long cdrom_play_msf(struct ata_dev_s *dev, char *arg, int kernel)
{
    struct ioc_play_msf msf;
    unsigned char packet[12];

    if(copy_arg(&msf, arg, sizeof(struct ioc_play_msf), kernel) != 0)
    {
        return -EFAULT;
    }

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_PLAY_AUDIO_MSF;
    packet[3] = msf.start_m;
    packet[4] = msf.start_s;
    packet[5] = msf.start_f;
    packet[6] = msf.end_m;
    packet[7] = msf.end_s;
    packet[8] = msf.end_f;

    if(dev->type == IDE_PATAPI)
    {
        return atapi_read_packet(dev, packet, 12, NULL, 0, 0);
    }
    else
    {
        return achi_satapi_read_packet_virt(dev, 0, 0, 0, 0, packet);
    }
}


static long cdrom_read_subchannel(struct ata_dev_s *dev, char *arg, int kernel)
{
    struct ioc_read_subchannel subchan;
    struct cd_sub_channel_info data;
    unsigned char packet[12];
    int len;
    long res;

    if(copy_arg(&subchan, arg, sizeof(struct ioc_read_subchannel), kernel) != 0)
    {
        return -EFAULT;
    }

    len = subchan.data_len;

    if(len > (int)sizeof(data) || len < (int)sizeof(struct cd_sub_channel_header))
    {
        return -EINVAL;
    }

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_READ_SUBCHANNEL;
    packet[1] = (subchan.address_format == CD_MSF_FORMAT) ? (1 << 1) : 0;
    packet[2] = (1 << 6);
    packet[3] = subchan.data_format;
    packet[6] = subchan.track;
    _lto2b(&packet[7], len);

    if(dev->type == IDE_PATAPI)
    {
        res = atapi_read_packet(dev, packet, 12, &data, len, 0);
    }
    else
    {
        res = achi_satapi_read_packet_virt(dev, (uintptr_t)&data, len, 0, 0, packet);
    }

    if(res < 0)
    {
        return res;
    }

    len = _2btol(data.header.data_len);
    len += sizeof(struct cd_sub_channel_header);

    if(len > subchan.data_len)
    {
        len = subchan.data_len;
    }

    return copy_res(subchan.data, &data, len, kernel);
}


static long cdrom_read_toc(struct ata_dev_s *dev, int format, int start,
                           int control, void *data, size_t datalen)
{
    unsigned char packet[12];

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_READ_TOC;
    packet[1] = (format == CD_MSF_FORMAT) ? (1 << 1) : 0;
    packet[6] = start;
    packet[9] = control;
    _lto2b(&packet[7], datalen);

    if(dev->type == IDE_PATAPI)
    {
        return atapi_read_packet(dev, packet, 12, data, datalen, 0);
    }
    else
    {
        return achi_satapi_read_packet_virt(dev, (uintptr_t)data, datalen, 0, 0, packet);
    }
}


static long cdrom_read_tocheader(struct ata_dev_s *dev, char *arg, int kernel)
{
    struct ioc_toc_header th;
    long res;

    if(copy_arg(&th, arg, sizeof(struct ioc_toc_header), kernel) != 0)
    {
        return -EFAULT;
    }

    if((res = cdrom_read_toc(dev, 0, 0, 0, &th, sizeof(th))) != 0)
    {
        return res;
    }

    return copy_res(arg, &th, sizeof(th), kernel);
}


static long cdrom_read_tocentries(struct ata_dev_s *dev, char *arg, int kernel)
{
    struct cd_toc_t toc;
    struct ioc_read_toc_entry te;
    int len;
    long res;

    if(copy_arg(&te, arg, sizeof(struct ioc_read_toc_entry), kernel) != 0)
    {
        return -EFAULT;
    }

    len = te.data_len;

    if(len > (int)sizeof(toc.entries) ||
       len < (int)sizeof(struct cd_toc_entry))
    {
        return -EINVAL;
    }

    if((res = cdrom_read_toc(dev, te.address_format, te.starting_track, 0,
                              &toc, len + sizeof(struct ioc_toc_header))) != 0)
    {
        return res;
    }

    len = toc.header.len - 
            (sizeof(toc.header.starting_track) + 
                    sizeof(toc.header.ending_track));

    if(len > te.data_len)
    {
        len = te.data_len;
    }

    return copy_res(te.data, toc.entries, len, kernel);
}


static long cdrom_read_msaddr(struct ata_dev_s *dev, char *arg)
{
    struct cd_toc_t toc;
    long res;

    if(*(int *)arg != 0)
    {
        return -EINVAL;
    }

    if((res = cdrom_read_toc(dev, 0, 0, 0x40, &toc, 
                                sizeof(struct ioc_toc_header) + 
                                        sizeof(struct cd_toc_entry))) != 0)
    {
        return res;
    }

    *(int *)arg = 
        (toc.header.len >= 10 && toc.entries[0].track > 1) ?
                        toc.entries[0].addr.lba : 0;
    return 0;
}


static long cdrom_getvol(struct ata_dev_s *dev, char *arg, int kernel)
{
    struct cd_audio_page_t page;
    struct ioc_vol vol;
    long res;

    if((res = cdrom_mode_sense(dev, &page, SENSE_PAGE_AUDIO, 
                                sizeof(struct cd_audio_page_t))) < 0)
    {
        return res;
    }

    vol.vol[0] = page.port[0].volume;
    vol.vol[1] = page.port[1].volume;
    vol.vol[2] = page.port[2].volume;
    vol.vol[3] = page.port[3].volume;

    return copy_res(arg, &vol, sizeof(struct ioc_vol), kernel);
}


static long cdrom_setvol(struct ata_dev_s *dev, char *arg, int kernel)
{
    struct cd_audio_page_t page, page2;
    struct ioc_vol vol;
    long res;

    if(copy_arg(&vol, arg, sizeof(struct ioc_vol), kernel) != 0)
    {
        return -EFAULT;
    }

    if((res = cdrom_mode_sense(dev, &page, SENSE_PAGE_AUDIO, 
                                sizeof(struct cd_audio_page_t))) < 0)
    {
        return res;
    }

    if((res = cdrom_mode_sense(dev, &page2, 
                                SENSE_PAGE_AUDIO|SENSE_PAGE_CTRL_CHANGEABLE, 
                                sizeof(struct cd_audio_page_t))) < 0)
    {
        return res;
    }

    page.port[0].volume = vol.vol[0] & page2.port[0].volume;
    page.port[1].volume = vol.vol[1] & page2.port[1].volume;
    page.port[2].volume = vol.vol[2] & page2.port[2].volume;
    page.port[3].volume = vol.vol[3] & page2.port[3].volume;

    page.port[0].channels = 1;
    page.port[1].channels = 2;

    return cdrom_mode_select(dev, &page, SENSE_PAGE_AUDIO, sizeof(struct cd_audio_page_t));
}


static long cdrom_pause(struct ata_dev_s *dev, int resume)
{
    unsigned char packet[12];

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_PAUSE_RESUME;
    packet[8] = (resume & 0xff);

    if(dev->type == IDE_PATAPI)
    {
        return atapi_read_packet(dev, packet, 12, NULL, 0, 0);
    }
    else
    {
        return achi_satapi_read_packet_virt(dev, 0, 0, 0, 0, packet);
    }
}


static long cdrom_start(struct ata_dev_s *dev, int type)
{
    unsigned char packet[12];

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_START_STOP;
    packet[4] = (type & 0xff);

    if(dev->type == IDE_PATAPI)
    {
        return atapi_read_packet(dev, packet, 12, NULL, 0, 0);
    }
    else
    {
        return achi_satapi_read_packet_virt(dev, 0, 0, 0, 0, packet);
    }
}


static long cdrom_prevent(struct ata_dev_s *dev, int prevent)
{
    unsigned char packet[12];

    // setup SCSI packet
    A_memset(packet, 0, 12);
    packet[0] = ATAPI_CMD_PREVENT_ALLOW;
    packet[4] = (prevent & 0xff);

    if(dev->type == IDE_PATAPI)
    {
        return atapi_read_packet(dev, packet, 12, NULL, 0, 0);
    }
    else
    {
        return achi_satapi_read_packet_virt(dev, 0, 0, 0, 0, packet);
    }
}


static long cdrom_command(struct ata_dev_s *dev, dev_t devid,
                          char *arg, int kernel)
{
    struct disk_req_t req;
    scsireq_t scsireq;
    size_t len;
    long res;
    int pages;
    uintptr_t tmp_phys, tmp_virt;

    if(copy_arg(&scsireq, arg, sizeof(scsireq_t), kernel) != 0)
    {
        return -EFAULT;
    }

    len = scsireq.datalen;

    if(scsireq.flags != SCCMD_READ)
    {
        return -EBADF;
    }

    if(dev->type == IDE_PATAPI)
    {
        return atapi_read_packet(dev, scsireq.cmd, scsireq.cmdlen, scsireq.databuf, len, 0);
    }

    // if the command involves data, allocate enough memory pages
    // so that we can ensure we have proper physical page addresses
    // to pass to the AHCI driver
    if(len)
    {
        len = align_up(len);
        pages = len / PAGE_SIZE;
        tmp_virt = vmmngr_alloc_and_map(pages, 0, PTE_FLAGS_PW,
                                            &tmp_phys, REGION_DMA);

        if(!tmp_virt)
        {
            ((scsireq_t *)arg)->retsts = SCCMD_UNKNOWN;
            return -ENOMEM;
        }
    }
    else
    {
        tmp_virt = 0;
    }

    req.dev = devid;
    req.data = tmp_virt;
    req.datasz = len;
    req.fs_blocksz = dev->bytes_per_sector ? 
                                dev->bytes_per_sector : ATAPI_SECTOR_SIZE;
    req.blockno = (scsireq.cmd[2] << 24) | (scsireq.cmd[3] << 16) |
                  (scsireq.cmd[4] << 8) | (scsireq.cmd[5] << 0);
    req.write = (scsireq.flags == SCCMD_WRITE);

    /*
     * TODO: we should get sense info for failed transfers
     */
    if((res = ahci_strategy(&req)) < 0)
    {
        ((scsireq_t *)arg)->retsts = SCCMD_UNKNOWN;
    }
    else
    {
        ((scsireq_t *)arg)->retsts = SCCMD_OK;
        ((scsireq_t *)arg)->datalen_used = scsireq.datalen;

        if(len)
        {
            A_memcpy(scsireq.databuf, (void *)tmp_virt, scsireq.datalen);
        }
    }

    if(tmp_virt)
    {
        vmmngr_free_pages(tmp_virt, tmp_virt + len);
    }

    return 0;
}


long ahci_cdrom_ioctl(dev_t devid, unsigned int cmd, char *arg, int kernel)
{
    struct ata_dev_s *dev = NULL;

    if(MAJOR(devid) == 3 || MAJOR(devid) == 22)
    {
        struct ata_devtab_s *tab = (MAJOR(devid) == 3) ? &tab1 : &tab2;

        dev = tab->dev[MINOR(devid)];
    }
    else if(MAJOR(devid) == AHCI_CDROM_MAJ)
    {
        dev = ahci_cdrom_dev[MINOR(devid)];
    }

    if(!dev)
    {
        return -EINVAL;
    }

    switch(cmd)
    {
        case CDIOCPLAYMSF:
            return cdrom_play_msf(dev, arg, kernel);

        case CDIOCREADSUBCHANNEL:
            return cdrom_read_subchannel(dev, arg, kernel);

        case CDIOREADTOCHEADER:
            return cdrom_read_tocheader(dev, arg, kernel);

        case CDIOREADTOCENTRIES:
            return cdrom_read_tocentries(dev, arg, kernel);

        case CDIOREADMSADDR:
            return cdrom_read_msaddr(dev, arg);

        case CDIOCGETVOL:
            return cdrom_getvol(dev, arg, kernel);

        case CDIOCSETVOL:
            return cdrom_setvol(dev, arg, kernel);

        case CDIOCPAUSE:
            return cdrom_pause(dev, 0);

        case CDIOCRESUME:
            return cdrom_pause(dev, 1);

        case CDIOCSTART:
            return cdrom_start(dev, CDROM_UNIT_START);

        case CDIOCSTOP:
            return cdrom_start(dev, CDROM_UNIT_STOP);

        case CDIOCCLOSE:
            return cdrom_start(dev, CDROM_UNIT_START|CDROM_UNIT_EJECT);

        case CDIOCEJECT:
            return cdrom_start(dev, CDROM_UNIT_STOP|CDROM_UNIT_EJECT);

        case CDIOCALLOW:
            return cdrom_prevent(dev, 0);

        case CDIOCPREVENT:
            return cdrom_prevent(dev, 1);

        /*
        case CDIOCLOCK:
            return cdrom_prevent(dev, *(int *)arg ? 1 : 0);
        */

        case SCIOCCOMMAND:
            return cdrom_command(dev, devid, arg, kernel);
    }

    // if this is an AHCI CD-ROM, check the other commands
    if(MAJOR(devid) == AHCI_CDROM_MAJ)
    {
        return ahci_ioctl(devid, cmd, arg, kernel);
    }

    return -EINVAL;
}

