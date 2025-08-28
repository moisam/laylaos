/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: dev.c
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
 *  \file dev.c
 *
 *  This file contains the global block and character device master tables
 *  (see \ref bdev_tab and \ref cdev_tab). It also contains the dev_init()
 *  function, which is called at boot time to populate the kernel's device
 *  tree, as well as general device I/O and ioctl functions.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/asm.h>
#include <kernel/ata.h>
#include <kernel/tty.h>
#include <kernel/hda.h>
#include <kernel/ahci.h>
#include <kernel/fio.h>
#include <kernel/cdrom.h>
//#include <kernel/mouse.h>
#include <fs/sockfs.h>
#include <fs/devpts.h>
#include <gui/fb.h>
#include <mm/kheap.h>

/*
 * Master block and char device list. We select the function to call based on
 * the device's major number. The device number (major & minor) is passed on
 * to the read/write/ioctl function so it knows which device to operate on.
 *
 * For Linux device list, see:
 *    https://www.kernel.org/doc/Documentation/admin-guide/devices.txt
 */

#define ZDIRENT     NULL
#define ZCACHE      
#define ZDEV_ENTRY  { NULL, NULL, NULL, NULL, NULL, NULL, ZDIRENT, ZCACHE }

struct bdev_ops_t bdev_tab[NR_DEV] =
{
    ZDEV_ENTRY,

    /* 1 = RAM disk */
    { ramdev_strategy, NULL, NULL, ramdev_ioctl, NULL, NULL, ZDIRENT, ZCACHE },
    { NULL, },

    /* 3 = hda, hdb */
    { ata_strategy, NULL, NULL, ata_ioctl, NULL, NULL, ZDIRENT, ZCACHE },
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,

    /* 7 = loopback devices */
    { lodev_strategy, NULL, NULL, lodev_ioctl, NULL, NULL, ZDIRENT, ZCACHE },

    /* 8 = sda, ... sdp */
    { ahci_strategy, NULL, NULL, ahci_ioctl, NULL, NULL, ZDIRENT, ZCACHE },
    ZDEV_ENTRY,
    ZDEV_ENTRY,

    /* 11 = scd0, ... */
    { ahci_strategy, NULL, NULL, ahci_cdrom_ioctl, NULL, NULL, ZDIRENT, ZCACHE },
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,
    ZDEV_ENTRY,

    /* 22 = hdc, hdd */
    { ata_strategy, NULL, NULL, ata_ioctl, NULL, NULL, ZDIRENT, ZCACHE },
};


struct cdev_ops_t cdev_tab[NR_DEV] =
{
    { NULL, NULL, NULL, NULL, NULL },

    /* 1 = mem char devices */
    { memdev_char_read, memdev_char_write, NULL,
      memdev_char_select, memdev_char_poll },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },

    /* 4 = ttyx (tty0, tty1, ...) */
    { ttyx_read, ttyx_write, tty_ioctl, tty_select, tty_poll },

    /* 5 = tty (current tty) */
    { ttyx_read, ttyx_write, tty_ioctl, tty_select, tty_poll },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },

    /* 10 = misc devices */
    { miscdev_read, miscdev_write, miscdev_ioctl, miscdev_select, miscdev_poll },

    /* 11 = raw keyboard device */
    //{ kbddev_read, NULL, NULL, kbddev_select, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },

    /* 13 = input core */
    { inputdev_read, inputdev_write, NULL /* inputdev_ioctl */,
                     inputdev_select, inputdev_poll },

    /* 14 = dsp */
    { snddev_read, snddev_write, snddev_ioctl, snddev_select, snddev_poll },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL },

    /* 29 = framebuffer device */
    { NULL /* fb_read */, NULL, fb_ioctl, NULL, NULL },
};


/*
 * Initialize the kernel's device list.
 */
void dev_init(void)
{
    add_dev_node("mem", TO_DEVID(1, 1), (S_IFCHR | 0640));      // crw-r-----
    add_dev_node("kmem", TO_DEVID(1, 2), (S_IFCHR | 0440));     // cr--r-----
    add_dev_node("null", TO_DEVID(1, 3), (S_IFCHR | 0666));     // crw-rw-rw-
    //add_dev_node("port", TO_DEVID(1, 4), (S_IFCHR | 0666));
    add_dev_node("zero", TO_DEVID(1, 5), (S_IFCHR | 0666));     // crw-rw-rw-
    //add_dev_node("core", TO_DEVID(1, 6), (S_IFCHR | 0666));
    add_dev_node("full", TO_DEVID(1, 7), (S_IFCHR | 0666));     // crw-rw-rw-
    add_dev_node("random", TO_DEVID(1, 8), (S_IFCHR | 0666));   // crw-rw-rw-
    add_dev_node("urandom", TO_DEVID(1, 9), (S_IFCHR | 0666));  // crw-rw-rw-
    //add_dev_node("aio", TO_DEVID(1, 10), (S_IFCHR | 0666));

    add_dev_node("tty", TO_DEVID(5, 0), (S_IFCHR | 0666));      // crw-rw-rw-
    add_dev_node("ptmx", TO_DEVID(5, 2), (S_IFCHR | 0666));     // crw-rw-rw-
    add_dev_node("pts", 0, (S_IFDIR | 0755));                   // drwxr-xr-x
    add_dev_node("shm", 0, (S_IFDIR | 0777));                   // drwxrwxrwx

    add_dev_node("fb0", TO_DEVID(29, 0), (S_IFCHR | 0440));     // cr--r-----
    //add_dev_node("guiev", TO_DEVID(13, 64), (S_IFCHR | 0660));     // crw-rw----

    add_dev_node("loop-control", TO_DEVID(10, 237), (S_IFCHR | 0664)); // crw-rw-r--

    /*
     * TODO: this should be under /dev/input.
     */
    add_dev_node("mouse0", TO_DEVID(13, 32), (S_IFCHR | 0440)); // cr--r-----

    //add_dev_node("kbd", TO_DEVID(11, 0), (S_IFCHR | 0440));     // cr--r-----


    int i, j, k;

    // add ttys
    for(i = 0; i < NTTYS; i++)
    {
        static char buf[8] = { 't', 't', 'y', '\0', '\0', };

        buf[3] = '0' + i;
        add_dev_node(buf, TO_DEVID(4, i), (S_IFCHR | 0620));     // crw--w----
    }

    // add ramdisks
    for(i = 0; i < NR_RAMDISK; i++)
    {
        static char buf[8] = { 'r', 'a', 'm', '\0', '\0', '\0', '\0' };
        
        if(ramdisk[i].start)
        {
            j = 3;
            k = i;

            if(k >= 100)
            {
                buf[j++] = '0' + (int)(k / 100);
                k %= 100;
            }

            if(k >= 10)
            {
                buf[j++] = '0' + (int)(k / 10);
            }

            buf[j++] = '0' + (int)(k % 10);
            buf[j] = '\0';

            add_dev_node(buf, TO_DEVID(1, i), (S_IFBLK | 0444));
        }
    }

    if(ramdisk[250].start)
    {
        add_dev_node("initrd", TO_DEVID(1, 250), (S_IFBLK | 0444));
    }
    
    // add functions to handle master pseudoterminal devices
    cdev_tab[PTY_MASTER_MAJ].read = ttyx_read /* pty_master_read */;
    cdev_tab[PTY_MASTER_MAJ].write = ttyx_write /* pty_master_write */;
    cdev_tab[PTY_MASTER_MAJ].ioctl = tty_ioctl;

	// handle pseudoterminal master devices (major 2) separately, as they read
	// from the (slave's) write queue, and write to the (slave's) read queue
    cdev_tab[PTY_MASTER_MAJ].select = pty_master_select;
    cdev_tab[PTY_MASTER_MAJ].poll = pty_master_poll;

    // add functions to handle slave pseudoterminal devices
    cdev_tab[PTY_SLAVE_MAJ].read = ttyx_read;
    cdev_tab[PTY_SLAVE_MAJ].write = ttyx_write;
    cdev_tab[PTY_SLAVE_MAJ].ioctl = tty_ioctl;
    cdev_tab[PTY_SLAVE_MAJ].select = tty_select;
    cdev_tab[PTY_SLAVE_MAJ].poll = tty_poll;
    
    // add sound devices
    // dsp -> first digital audio device
    // dsp1 -> second digital audio
    // audio -> Sun-compatible digital audio
    struct hda_dev_t *hda = first_hda ? first_hda->next : NULL;

    if(first_hda)
    {
        add_dev_node("dsp", first_hda->devid, (S_IFCHR | 0666));  // crw-rw-rw-
        add_dev_node("audio", first_hda->devid, (S_IFCHR | 0666));  // crw-rw-rw-
        add_dev_node("audio0", first_hda->devid, (S_IFCHR | 0666));  // crw-rw-rw-
    }
    else
    {
        // create a dummy output device
        dev_t dummy = create_dummy_hda();
        add_dev_node("dsp", dummy, (S_IFCHR | 0666));  // crw-rw-rw-
        add_dev_node("audio", dummy, (S_IFCHR | 0666));  // crw-rw-rw-
        add_dev_node("audio0", dummy, (S_IFCHR | 0666));  // crw-rw-rw-
    }

    i = 1;
    
    while(hda)
    {
        char buf[8];
        
        ksprintf(buf, 8, "dsp%d", i);
        add_dev_node(buf, hda->devid, (S_IFCHR | 0666));     // crw-rw-rw-
        ksprintf(buf, 8, "audio%d", i);
        add_dev_node(buf, hda->devid, (S_IFCHR | 0666));     // crw-rw-rw-

        hda = hda->next;
        i++;
    }
}


long syscall_ioctl_internal(int fd, unsigned int cmd, char *arg, int kernel)
{
	struct file_t *fp = NULL;
    struct fs_node_t *node = NULL;
	dev_t dev;
	mode_t mode;
	int major;
    long (*func)(dev_t, unsigned int, char *, int) = NULL;

    if(fdnode(fd, this_core->cur_task, &fp, &node) != 0)
	{
		return -EBADF;
	}

    if(fp->flags & O_PATH)
    {
        return -EBADF;
    }

	mode = node->mode;
	
	if(IS_SOCKET(node))
	{
	    return sockfs_ioctl(fp, cmd, arg, kernel);
	}
	
	// can only handle blk & char devices
	if(!S_ISCHR(mode) && !S_ISBLK(mode))
	{
		return -ENOTTY;
	}
	
	dev = node->blocks[0];
	major = MAJOR(dev);
	
	if(major >= NR_DEV)
	{
		printk("dev: ioctl on an unknown %s device (0x%x)\n",
		       S_ISCHR(mode) ? "char" : "block", dev);
		return -ENOTTY;
	}
	
	func = S_ISCHR(mode) ? cdev_tab[major].ioctl : bdev_tab[major].ioctl;
	
	if(!func)
	{
		return -ENOTTY;
	}
	
	return func(dev, cmd, arg, kernel);
}


/*
 * Handler for syscall ioctl().
 */
long syscall_ioctl(int fd, unsigned int cmd, char *arg)
{
    return syscall_ioctl_internal(fd, cmd, arg, 0);
}

