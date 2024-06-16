/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
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
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/ata.h>
#include <kernel/dev.h>
#include <kernel/task.h>
#include <fs/devfs.h>
#include <mm/kheap.h>

#define MAX_CDROM_DEVICES           16

struct cdrom_t
{
    dev_t dev;
    char name[8];
};

static struct cdrom_t cdroms[MAX_CDROM_DEVICES] = { { 0, }, };
static int last_index = 0;
static struct task_t *cdrom_task = NULL;

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

            if(MAJOR(cdroms[i].dev) == 3 || MAJOR(cdroms[i].dev) == 22)
            {
                KDEBUG("cdrom_task_func: checking (maj %d)\n", MAJOR(cdroms[i].dev));

                tab = (MAJOR(cdroms[i].dev) == 3) ? &tab1 : &tab2;
                dev = tab->dev[MINOR(cdroms[i].dev)];

                if(!dev)
                {
                    continue;
                }

                // Send a TEST UNIT READY command to the ATAPI drive to 
                // check if there is a disc. If there isn't (or the disc has
                // been ejected or changed), the ERR bit should be set in
                // the status register and the Media Change Request bit should
                // be set in the error register. We follow this by the 
                // REQUEST SENSE command to find out the details
                //
                // See: https://cygnus.speccy.cz/download/datasheety/atapi.pdf

                KDEBUG("cdrom_task_func: adding disk req\n");
                res = ata_add_req(dev, 0, 1, (virtual_addr)&buf,
                                            0, atapi_test_unit_ready);

                KDEBUG("cdrom_task_func: res %d, status 0x%x, err 0x%x\n", res, buf[0], buf[1]);

                if(/* (buf[0] & ATA_SR_ERR) && */ (buf[1] & ATA_ER_MC))
                {
                    struct sense_data_t sense_data;

                    A_memset(&sense_data, 0, sizeof(struct sense_data_t));
                    res = ata_add_req(dev, 0, 1, (virtual_addr)&sense_data,
                                                0, atapi_request_sense);

                    KDEBUG("cdrom_task_func: res %d, errcode 0x%x, key 0x%x, asc 0x%x\n", res, sense_data.err_code, (sense_data.sense_key & 0x0f), sense_data.additional_sense_code);

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
                            KDEBUG("cdrom_task_func: unmounting cdrom\n");

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

                            KDEBUG("cdrom_task_func: (re)mounting cdrom\n");

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

                            KDEBUG("cdrom_task_func: cdrom dev '%s'\n", name);
                            mount_internal("cdrom", name, 0);
                        }
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

