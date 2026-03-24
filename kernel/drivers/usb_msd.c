/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb_msd.c
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
 *  \file usb_msd.c
 *
 *  The Universal Serial Bus (USB) driver code is split into several files:
 *    - usb.c       => main entry point and general functions
 *    - usb_msd.c   => functions to handle Mass Storage Devices (MSD)
 *    - usb_hid.c   => functions to handle Human Interaction Devices (HID)
 *    - usb_hub.c   => functions to handle USB hubs
 *    - usb_ioctl.c => functions to handle ioctl() calls
 *    - usb_ohci.c  => OHCI layer
 *    - usb_uhci.c  => UHCI layer
 *    - usb_ehci.c  => EHCI layer
 */

//#define __DEBUG

#include <errno.h>
#include <sys/hdreg.h>
#include <kernel/pci.h>
#include <kernel/pciio.h>
#include <kernel/asm.h>
#include <kernel/usb.h>
#include <kernel/ata.h>
#include <kernel/ahci.h>
#include <kernel/dev.h>
#include <kernel/cdrom.h>
#include <mm/kheap.h>

#define SCSI_CMD_TEST_UNIT_READY        0x00
#define SCSI_CMD_REQUEST_SENSE          0x03
#define SCSI_CMD_INQUIRY                0x12
#define SCSI_CMD_READ_CAPACITY          0x25
#define SCSI_CMD_READ                   0x28
#define SCSI_CMD_WRITE                  0x2A

#define CBW_SIG                         0x43425355
#define CBW_SIG_OK                      0x53425355
#define CBW_SIG_ERR                     0x01010101

#if 0
/* Our master table for USB MSD disks and their partitions */
struct ata_dev_s *usb_disk_dev[MAX_USB_MSD_DEVICES];
struct parttab_s *usb_disk_part[MAX_USB_MSD_DEVICES];
struct kernel_mutex_t usb_disk_tablock;

void usb_msd_register_dev(void *__dev, struct parttab_s *part, int n);
#endif
int usb_msd_read_sector_direct(void *__dev, uintptr_t phys_buf, uintptr_t virt_buf, uint32_t lba);

// defined in ahci.c
void ahci_register_dev(void *__dev, struct parttab_s *part, int n);
long __ahci_remove_dev(dev_t dev_id, int remove_parent, int force);


static void prep_scsi_cmd(struct usb_cmd_blk_wrapper_t *cbw, 
                          uint32_t tag, uint8_t cmd, uint32_t lba, uint16_t len, uint32_t blksz)
{
    cbw->sig = CBW_SIG;
    cbw->tag = tag;
    cbw->lun = 0;

#define CMD_BYTES8(a, b, c, d, e, f, g, h)              \
    cbw->cmd[0] = cmd;                                  \
    cbw->cmd[1] = a; cbw->cmd[2] = b; cbw->cmd[3] = c;  \
    cbw->cmd[4] = d; cbw->cmd[5] = e; cbw->cmd[6] = f;  \
    cbw->cmd[7] = g; cbw->cmd[8] = h;                   \
    for(int z = 9; z < 16; z++) cbw->cmd[z] = 0;

    switch(cmd)
    {
        case SCSI_CMD_TEST_UNIT_READY:
            cbw->len = 0;
            cbw->flags = 0;
            cbw->cblen = 6;
            CMD_BYTES8(0, 0, 0, 0, 0, 0, 0, 0);
            break;

        case SCSI_CMD_REQUEST_SENSE:
            cbw->len = 18;
            cbw->flags = 0x80;
            cbw->cblen = 6;
            CMD_BYTES8(0, 0, 0, 18, 0, 0, 0, 0);
            break;

        case SCSI_CMD_INQUIRY:
            cbw->len = 36;
            cbw->flags = 0x80;
            cbw->cblen = 6;
            CMD_BYTES8(0, 0, 0, 36, 0, 0, 0, 0);
            break;

        case SCSI_CMD_READ_CAPACITY:
            cbw->len = 8;
            cbw->flags = 0x80;
            cbw->cblen = 10;
            CMD_BYTES8(0, BYTE4(lba), BYTE3(lba), BYTE2(lba), BYTE1(lba), 0, 0, 0);
            break;

        case SCSI_CMD_READ:
            cbw->len = len * blksz;
            cbw->flags = 0x80;
            cbw->cblen = 10;
            CMD_BYTES8(0, BYTE4(lba), BYTE3(lba), BYTE2(lba), BYTE1(lba), 0, BYTE2(len), BYTE1(len));
            break;

        case SCSI_CMD_WRITE:
            cbw->len = len * blksz;
            cbw->flags = 0;
            cbw->cblen = 10;
            CMD_BYTES8(0, BYTE4(lba), BYTE3(lba), BYTE2(lba), BYTE1(lba), 0, BYTE2(len), BYTE1(len));
            break;
    }

#undef CMD_BYTES8

}


static void usb_bulk_reset(struct usb_interface_t *iface)
{
    usb_ctrl_set(iface->usb, 0x21, 0xFF, 0, 0, iface->desc.interfacenum);
}


static int reset_recovery(struct usb_interface_t *iface)
{
    int res;
    unsigned int i;

    // reset interface
    usb_bulk_reset(iface);

    // clear feature HALT to bulkin and bulkout endpoints
    usb_clear_feature_halt(iface->usb, iface->endpoint_in);
    usb_clear_feature_halt(iface->usb, iface->endpoint_out);

    // set first config
    if((res = usb_set_config(iface->usb, 1)) < 0)
    {
        printk("usb: reset-recovery: failed to set config (err %d)\n", res);
        return res;
    }

    if((i = usb_get_config(iface->usb)) != 1)
    {
        printk("usb: reset-recovery: failed to get config (expected 1, got %d)\n", i);
        return -EIO;
    }

    iface->endpoint_in->toggle = 0;
    iface->endpoint_out->toggle = 0;

    // reset interface
    usb_bulk_reset(iface);

    return 0;
}


static int check_scsi_cmd(struct usb_interface_t *iface, uint32_t tag, void *statbuf)
{
    int err = 0;
    volatile uint32_t dword;
    volatile uint8_t byte;

    /*
    printk("check_scsi_cmd: ");
    for(volatile int i = 0; i < 13; i++) printk("%x ", ((char *)statbuf)[i]);
    printk("\n");
    */

    // check signature
    dword = *(volatile uint32_t *)statbuf;

    if(dword != CBW_SIG_OK)
    {
        printk("usb: SCSI cmd returned wrong signature (0x%x)\n", dword);
        err = -EIO;
    }

    // check tag
    dword = *(((volatile uint32_t *)statbuf) + 1);

    if(dword != tag)
    {
        printk("usb: SCSI cmd returned wrong tag (0x%x, expected 0x%x)\n", dword, tag);
        err = -EIO;
    }

    // check data residue
    dword = *(((volatile uint32_t *)statbuf) + 2);

    if(dword != 0)
    {
        printk("usb: SCSI cmd returned data residue (0x%x)\n", dword);
        //err = -EIO;
    }

    // check status byte
    byte = *(((volatile uint8_t *)statbuf) + 12);

    switch(byte)
    {
        case 0:
            break;

        case 1:
            printk("usb: SCSI cmd failed\n");
            err = -EIO;
            break;

        case 2:
            printk("usb: SCSI cmd failed - reset recovery\n");
            reset_recovery(iface);
            err = -EIO;
            break;

        default:
            printk("usb: SCSI cmd failed (err %d)\n", byte);
            err = -EIO;
            break;
    }

    //printk("check_scsi_cmd: err %d\n", err);
    return err;
}

static inline int sense_ok(struct usb_interface_t *iface, char *statbuf);

static int send_scsi_cmd(struct usb_interface_t *iface, 
                         uint8_t cmd, uint32_t lba, uint16_t __len,
                         void *buf, void *statbuf)
{
    struct usb_cmd_blk_wrapper_t cbw;
    struct usb_transfer_t transfer;
    char tmp[16];
    size_t len = __len;
    uint32_t tag;
    uint32_t blksz = iface->bytes_per_sector;

    kernel_mutex_lock_infinite_wait(&iface->usb->lock);

    tag = iface->cur_tag++;
    prep_scsi_cmd(&cbw, tag, cmd, lba, len, blksz);

    usb_setup_transfer(iface->usb, iface->endpoint_out, &transfer, USB_TRANSFER_BULK);
    usb_out_transaction(&transfer, 0, &cbw, 31);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    if(!transfer.success)
    {
        switch_tty(1);
        printk("usb: failed to issue SCSI cmd 0x%x (send_scsi_cmd)\n", cmd);
        kpanic("*****\n");
        kernel_mutex_unlock(&iface->usb->lock);
        return -EIO;
    }

    if(cmd == SCSI_CMD_READ || cmd == SCSI_CMD_WRITE)
    {
        len *= blksz;
    }

    statbuf = statbuf ? statbuf : tmp;

    usb_setup_transfer(iface->usb, iface->endpoint_in, &transfer, USB_TRANSFER_BULK);

    if(len)
    {
        usb_in_transaction(&transfer, 0, buf, len);
    }

    usb_in_transaction(&transfer, 0, statbuf, 13);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    if(!transfer.success)
    {
        /*
        kernel_mutex_unlock(&iface->usb->lock);
        send_scsi_cmd(iface, SCSI_CMD_TEST_UNIT_READY, 0, 0, 0, statbuf);
        sense_ok(iface, statbuf);
        */

        switch_tty(1);
        printk("usb: failed to read SCSI cmd status (send_scsi_cmd)\n");
        kpanic("*****\n");
        kernel_mutex_unlock(&iface->usb->lock);
        return -EIO;
    }

    kernel_mutex_unlock(&iface->usb->lock);

    return check_scsi_cmd(iface, tag, statbuf);
}


static int send_scsi_cmd_out(struct usb_interface_t *iface, 
                             uint8_t cmd, uint32_t lba, uint16_t __len,
                             void *buf, void *statbuf)
{
    struct usb_cmd_blk_wrapper_t cbw;
    struct usb_transfer_t transfer;
    char tmp[16];
    size_t len = __len;
    uint32_t tag;
    uint32_t blksz = iface->bytes_per_sector;

    kernel_mutex_lock_infinite_wait(&iface->usb->lock);

    tag = iface->cur_tag++;
    prep_scsi_cmd(&cbw, tag, cmd, lba, len, blksz);

    if(cmd == SCSI_CMD_WRITE)
    {
        len *= blksz;
    }

    usb_setup_transfer(iface->usb, iface->endpoint_out, &transfer, USB_TRANSFER_BULK);
    usb_out_transaction(&transfer, 0, &cbw, 31);
    usb_out_transaction(&transfer, 0, buf, len);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    if(!transfer.success)
    {
        switch_tty(1);
        printk("usb: failed to issue SCSI cmd 0x%x (send_scsi_cmd_out)\n", cmd);
        kpanic("*****\n");
        kernel_mutex_unlock(&iface->usb->lock);
        return -EIO;
    }

    statbuf = statbuf ? statbuf : tmp;

    usb_setup_transfer(iface->usb, iface->endpoint_in, &transfer, USB_TRANSFER_BULK);
    usb_in_transaction(&transfer, 0, statbuf, 13);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    if(!transfer.success)
    {
        switch_tty(1);
        printk("usb: failed to read SCSI cmd status (send_scsi_cmd_out)\n");
        kpanic("*****\n");
        kernel_mutex_unlock(&iface->usb->lock);
        return -EIO;
    }

    kernel_mutex_unlock(&iface->usb->lock);

    return 0;
}


/*
uint8_t get_max_lun(struct usb_interface_t *iface)
{
    struct usb_transfer_t transfer;
    uint8_t max_lun = -1;

    usb_setup_transfer(iface->usb, iface->usb->endpoints, &transfer, USB_TRANSFER_CTRL);
    usb_setup_transaction(&transfer, 0xA1, 0xFE, 0, 0, iface->desc.interfacenum, 1);
    usb_in_transaction(&transfer, 0, &max_lun, 1);
    usb_out_transaction(&transfer, 1, 0, 0);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    printk("usb-msd: max_lun %u\n", max_lun);
    kpanic("^^^^^^^^^^\n");

    return max_lun;
}
*/


static inline int sense_ok(struct usb_interface_t *iface, char *statbuf)
{
    struct sense_data_t sense_data;

    A_memset(&sense_data, 0, sizeof(struct sense_data_t));

    if(send_scsi_cmd(iface, SCSI_CMD_REQUEST_SENSE, 0, 
                        sizeof(struct sense_data_t), &sense_data, statbuf) < 0)
    {
        return -EIO;
    }

    // check we got valid data
    if(!(sense_data.err_code & 0x80))
    {
        return -EINVAL;
    }

    sense_data.sense_key &= 0x0f;

    if(sense_data.sense_key == 0x02 ||              // Not Ready
       sense_data.additional_sense_code == 0x30 ||  // Cannot Read Medium
       sense_data.additional_sense_code == 0x3A)    // Medium Not Present
    {
        // no media
        printk("usb-msd: sense data: errcode 0x%x, key 0x%x, asc 0x%x\n",
                sense_data.err_code, 
                (sense_data.sense_key & 0x0f), 
                sense_data.additional_sense_code);

        return -ENOENT;
    }

    // sense_key == 0 is no error
    return (sense_data.sense_key == 0) ? 0 : -EIO;
}


int test_unit_ready(struct usb_interface_t *iface)
{
    char statbuf[24];
    char buf[24];
    int res = -EIO;
    volatile uint8_t byte;
    volatile int timeout = 50;

    while(timeout--)
    {
        if((res = send_scsi_cmd(iface, SCSI_CMD_TEST_UNIT_READY, 0, 0, 0, statbuf)) < 0)
        {
            printk("usb-msd: TEST_UNIT_READY failed (err %d)\n", res);

            if((res = sense_ok(iface, statbuf)) == -ENOENT)
            {
                return -ENOENT;
            }

            tick_delay(10);
            continue;
            //return res;
        }

        byte = BYTE1(*(((uint32_t *)statbuf) + 3));

        if(byte == 0)
        {
            if((res = sense_ok(iface, statbuf)) < 0)
            {
                if(res == -ENOENT)
                {
                    return -ENOENT;
                }

                tick_delay(10);
                continue;
            }

            res = BYTE1(*(((uint32_t *)statbuf) + 3));
            byte = (buf[2] & 0x0F);

            if(byte == 0 || byte == 2)
            {
                break;
            }
        }
    }

    return res;
}


void usb_msd_remove(struct usb_interface_t *iface)
{
    int i;

    for(i = 0; i < MAX_AHCI_DEVICES; i += 16)
    {
        if(ahci_disk_dev[i] && ahci_disk_dev[i]->priv == iface)
        {
            __ahci_remove_dev(TO_DEVID(AHCI_DEV_MAJ, i), 1, 1);
            return;
        }
    }
}


#define SECTORS_TO_DO               4

long usb_msd_read(struct ata_dev_s *dev, size_t lba, int sectors, uintptr_t buf)
{
    struct usb_interface_t *iface = dev->priv;

    if(!dev || !dev->priv)
    {
        return -EINVAL;
    }

    if(sectors <= SECTORS_TO_DO)
    {
        return send_scsi_cmd(iface, SCSI_CMD_READ, lba, sectors, (void *)buf, 0);
    }
    else
    {
        volatile int remaining = sectors, howmany = SECTORS_TO_DO;

        while(remaining > 0)
        {
            //printk("usb_msd_read: sectors %d, remaining %d, howmany %d\n", sectors, remaining, howmany);

            if(send_scsi_cmd(iface, SCSI_CMD_READ, lba, howmany, (void *)buf, 0) < 0)
            {
                return -EIO;
            }

            lba += SECTORS_TO_DO;
            remaining -= SECTORS_TO_DO;
            buf += (iface->bytes_per_sector * SECTORS_TO_DO);

            if(remaining < SECTORS_TO_DO)
            {
                howmany = remaining;
            }
        }

        return 0;
    }
}


long usb_msd_write(struct ata_dev_s *dev, size_t lba, int sectors, uintptr_t buf)
{
    struct usb_interface_t *iface = dev->priv;

    if(!dev || !dev->priv)
    {
        return -EINVAL;
    }

    if(sectors <= SECTORS_TO_DO)
    {
        return send_scsi_cmd_out(iface, SCSI_CMD_WRITE, lba, sectors, (void *)buf, 0);
    }
    else
    {
        volatile int remaining = sectors, howmany = SECTORS_TO_DO;

        while(remaining > 0)
        {
            if(send_scsi_cmd_out(iface, SCSI_CMD_WRITE, lba, howmany, (void *)buf, 0) < 0)
            {
                return -EIO;
            }

            lba += SECTORS_TO_DO;
            remaining -= SECTORS_TO_DO;
            buf += (iface->bytes_per_sector * SECTORS_TO_DO);

            if(remaining < SECTORS_TO_DO)
            {
                howmany = remaining;
            }
        }

        return 0;
    }
}

#undef SECTORS_TO_DO


/*
 * General AHCI Block Read/Write Operations
 */
long usb_msd_strategy(struct disk_req_t *req)
{
    size_t block;
    long res = 0;
    int sectors_per_block, sectors_to_read;
    int min = MINOR(req->dev);
    /*
    struct ata_dev_s *dev = usb_disk_dev[min];
    struct parttab_s *part = usb_disk_part[min];
    */
    struct ata_dev_s *dev = ahci_disk_dev[min];
    struct parttab_s *part = ahci_disk_part[min];
    
    if(!dev || !dev->priv)
    {
        printk("usb_msd_strategy: invalid device 0x%x\n", req->dev);
        return -ENODEV;
    }

    sectors_to_read = req->datasz / dev->bytes_per_sector;
    sectors_per_block = req->fs_blocksz / dev->bytes_per_sector;
    block = req->blockno * sectors_per_block;
    block += part ? part->lba : 0;

    if(!req->write)
    {
        res = usb_msd_read(dev, block, sectors_to_read, req->data);
    }
    else
    {
        res = usb_msd_write(dev, block, sectors_to_read, req->data);
    }

    return res ? -EIO : (long)(sectors_to_read * dev->bytes_per_sector);
}


int usb_msd_read_sector_direct(void *__dev, uintptr_t phys_buf, uintptr_t virt_buf, uint32_t lba)
{
    UNUSED(phys_buf);

    return usb_msd_read((struct ata_dev_s *)__dev, lba, 1, virt_buf);
}


int init_msd(struct usb_interface_t *iface)
{
    char buf[36];
    int res;
    uint32_t lba, blksz;
    struct ata_dev_s *dev;
    volatile struct usb_endpoint_t *endpoint;

    if(!iface->usb || !iface->usb->endpoints)
    {
        return -EINVAL;
    }

    for(endpoint = iface->usb->endpoints; endpoint != NULL; endpoint = endpoint->next)
    {
        if(endpoint->type == USB_ENDPOINT_INTERRUPT)
        {
            iface->endpoint_interrupt = (struct usb_endpoint_t *)endpoint;
        }
        else if(endpoint->type == USB_ENDPOINT_BULK)
        {
            if(endpoint->direction == USB_ENDPOINT_OUT)
            {
                iface->endpoint_out = (struct usb_endpoint_t *)endpoint;
            }
            else
            {
                iface->endpoint_in = (struct usb_endpoint_t *)endpoint;
            }
        }
    }

    if(!iface->endpoint_out || !iface->endpoint_in)
    {
        printk("usb-msd: mass storage device has invalid IN/OUT endpoints\n");
        return -EINVAL;
    }

    iface->endpoint_out->toggle = 0;
    iface->endpoint_in->toggle = 0;

    // reset interface
    //usb_bulk_reset(iface);
    tick_delay(50);

    //printk("usb-msd: iface 0x%x, proto 0x%x\n", iface->desc.subclass, iface->desc.protocol);
    //kpanic("*****\n");
    //get_max_lun(iface);

    // send SCSI command INQUIRY
    if((res = send_scsi_cmd(iface, SCSI_CMD_INQUIRY, 0, 36, buf, 0)) < 0)
    {
        printk("usb-msd: INQUIRY failed (err %d)\n", res);
        return res;
    }

    // send SCSI command TEST UNIT READY
    if((res = test_unit_ready(iface)) < 0)
    {
        printk("usb-msd: failed to test unit ready (err %d)\n", res);
        return res;
    }

    // send SCSI command READ CAPACITY
    if((res = send_scsi_cmd(iface, SCSI_CMD_READ_CAPACITY, 0, 8, buf, 0)) < 0)
    {
        printk("usb-msd: READ_CAPACITY failed (err %d)\n", res);
        return res;
    }

    // XXX: result returned in 2 big-endian uint32_t's
    lba = buf[3] | (buf[2] << 8) | (buf[1] << 16) | (buf[0] << 24);
    blksz = buf[7] | (buf[6] << 8) | (buf[5] << 16) | (buf[4] << 24);

    printk("usb-msd: last lba %u, blksz %u, capacity %u\n",
            lba, blksz, (lba + 1) * blksz);

    // read partitions
    if(!(dev = kmalloc(sizeof(struct ata_dev_s))))
    {
        printk("usb-msd: insufficient memory to init mass storage device\n");
        return -ENOMEM;
    }
    
    A_memset(dev, 0, sizeof(struct ata_dev_s));

    dev->type = IDE_UNKNOWN;
    //dev->irq = pci->irq[0];
    dev->base = 0;
    dev->priv = iface;
    dev->bytes_per_sector = blksz;
    dev->size = (lba + 1) * blksz;

    iface->bytes_per_sector = blksz;

    // add the new device and read the MBR
    printk("usb-msd: reading disk MBR\n");
    /*
    usb_msd_register_dev(dev, NULL, 0);
    read_disk_mbr("usb", dev, dev->bytes_per_sector, read_sector_direct, usb_msd_register_dev);
    */
    ahci_register_dev(dev, NULL, 0);
    read_disk_mbr("usb", dev, dev->bytes_per_sector, usb_msd_read_sector_direct, ahci_register_dev);

    return 0;
}

