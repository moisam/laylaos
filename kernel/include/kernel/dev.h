/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: dev.h
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
 *  \file dev.h
 *
 *  Include header file for working with RAM disks, memory devices and other
 *  input devices.
 */

#ifndef __DEV_H__
#define __DEV_H__

#include <sys/types.h>
#include <poll.h>
#include <kernel/vfs.h>
#include <kernel/pcache.h>
#include <fs/dentry.h>

/**
 * \def NR_DEV
 *
 * Maximum number of block devices
 */
#define NR_DEV                      256


/*
 * Values for device ioctl()'s cmd argument.
 */
#define DEV_IOCTL_GET_BLOCKSIZE     1   /**< ask ioctl() to return disk
                                             block size for I/O operations */


/**
 * \def NODEV
 *
 * Device id that does not identify any device
 */
#ifndef NODEV
#define NODEV                   (dev_t)(-1)
#endif


/**
 * @struct cdev_ops_t
 * @brief The cdev_ops_t structure.
 *
 * A structure to represent character device operations.
 */
struct cdev_ops_t
{
    ssize_t (*read)(struct file_t *, off_t *, unsigned char *, size_t, int);
                                        /**< device read() function */
    ssize_t (*write)(struct file_t *, off_t *, unsigned char *, size_t, int);
                                        /**< device write() function */

    int (*ioctl)(dev_t dev, unsigned int cmd, char *arg, int kernel);
                        /**< device ioctl() function */
    int (*select)(struct file_t *f, int which);     /**< device select()
                                                         function */
    int (*poll)(struct file_t *, struct pollfd *);  /**< device poll()
                                                         function */
};


/**
 * @struct bdev_ops_t
 * @brief The bdev_ops_t structure.
 *
 * A structure to represent block device operations.
 */
struct bdev_ops_t
{
    int (*strategy)(struct disk_req_t *);   /**< device strategy (read/
                                                   write) function */

    int (*open)(dev_t dev);     /**< device open() function */
    int (*close)(dev_t dev);    /**< device close() function */
    int (*ioctl)(dev_t dev, unsigned int cmd, char *arg, int kernel);
                                /**< device ioctl() function */
    int (*select)(struct file_t *, int);            /**< device select()
                                                         function */
    int (*poll)(struct file_t *, struct pollfd *);  /**< device poll()
                                                         function */
    struct dentry_list_t *dentry_list;  /**< list of dentries representing
                                             files and dirs accessed on this
                                             device */
};


/**
 * @var bdev_tab
 * @brief block device table.
 *
 * Master table with function pointers to the device I/O and ioctl functions
 * for all block devices on the system.
 */
extern struct bdev_ops_t bdev_tab[];


/**
 * @var cdev_tab
 * @brief character device table.
 *
 * Master table with function pointers to the device I/O and ioctl functions
 * for all character devices on the system.
 */
extern struct cdev_ops_t cdev_tab[];


/**
 * @struct ramdisk_s
 * @brief The ramdisk_s structure.
 *
 * A structure to represent a RAM disk.
 */
struct ramdisk_s
{
    virtual_addr start,     /**< start address (virtual) */
                 end;       /**< end address (virtual) */
};


/**
 * @var ramdisk
 * @brief RAM disk table.
 *
 * A table containing pointers to all the loaded RAM disks (see ram.c).
 */
extern struct ramdisk_s ramdisk[];


/**
 * @brief Handler for syscall ioctl().
 *
 * Switch function, passes the ioctl request to the respective device's
 * ioctl function.
 *
 * @param   fd      file descriptor referring to a character or block device
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int syscall_ioctl(int fd, unsigned int cmd, char *arg);


/**
 * @brief Initialize the kernel's device list.
 *
 * Initialize the kernel's device list. This function is called at boottime
 * by devfs_create().
 *
 * @return  nothing.
 *
 * @see     devfs_create
 */
void dev_init(void);


/**
 * @brief Add a new device node.
 *
 * Add a device node under devfs. The created device node could then be
 * accessed using the given \a name and access \a mode, and its device
 * number will be \a dev.
 *
 * @param   name    the new device name
 * @param   dev     the new device devid
 * @param   mode    the new device's access permissions and device type
 *
 * @return  zero on success, -(errno) on failure.
 */
int add_dev_node(char *name, dev_t dev, mode_t mode);


/**
 * @brief Set the given device's gid.
 *
 * Set the group id the device identified by \a devname to \a gid.
 * This function is called at boottime to set the gid of some devices,
 * like the pseudoterminal multiplexer \a /dev/ptmx, and the memory devices
 * \a /dev/mem and \a /dev/kmem.
 *
 * @param   devname the device's name
 * @param   gid     the new device gid
 *
 * @return  zero on success, -(errno) on failure.
 */
int set_dev_gid(char *devname, gid_t gid);


/******************************
 * Character device functions
 ******************************/

/**
 * @brief Read from char device /dev/mem.
 *
 * Read bytes from the character memory device (major = 1, minor = 1).
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t memdev_read(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Write to char device /dev/mem.
 *
 * Write bytes to the character memory device (major = 1, minor = 1).
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t memdev_write(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Read from char device /dev/kmem.
 *
 * Read bytes from the character kernel memory device (major = 1, minor = 2).
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t kmemdev_read(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Write to char device /dev/kmem.
 *
 * Write bytes to the character kernel memory device (major = 1, minor = 2).
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t kmemdev_write(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Read from char device /dev/null.
 *
 * Read bytes from the character null device (major = 1, minor = 3).
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t nulldev_read(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Write to char device /dev/null.
 *
 * Write bytes to the character null device (major = 1, minor = 3).
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t nulldev_write(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Read from char device /dev/zero.
 *
 * Read bytes from the character zero device (major = 1, minor = 5).
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t zerodev_read(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Write to char device /dev/zero.
 *
 * Write bytes to the character zero device (major = 1, minor = 5).
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t zerodev_write(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Read from char device /dev/full.
 *
 * Read bytes from the character full device (major = 1, minor = 7).
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t fulldev_read(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Write to char device /dev/full.
 *
 * Write bytes to the character full device (major = 1, minor = 7).
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t fulldev_write(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Read from char device /dev/random.
 *
 * Read bytes from the character random device (major = 1, minor = 8).
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t randdev_read(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Write to char device /dev/random.
 *
 * Write bytes to the character random device (major = 1, minor = 8).
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t randdev_write(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Read from char device /dev/urandom.
 *
 * Read bytes from the character urandom device (major = 1, minor = 9).
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t uranddev_read(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Write to char device /dev/urandom.
 *
 * Write bytes to the character urandom device (major = 1, minor = 9).
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t uranddev_write(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Read from a memory char device (major = 1).
 *
 * Switch function to read bytes from a memory character device.
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t memdev_char_read(struct file_t *f, off_t *pos,
                         unsigned char *buf, size_t count, int kernel);

/**
 * @brief Write to a memory char device (major = 1).
 *
 * Switch function to write bytes to a memory character device.
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t memdev_char_write(struct file_t *f, off_t *pos,
                          unsigned char *buf, size_t count, int kernel);


/**
 * @brief Read from an input core device (major = 13).
 *
 * Switch function to read bytes from an input core character device.
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t inputdev_read(struct file_t *f, off_t *pos,
                      unsigned char *buf, size_t count, int kernel);

/**
 * @brief Write to an input core device (major = 13).
 *
 * Switch function to write bytes to an input core character device.
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t inputdev_write(struct file_t *f, off_t *pos,
                       unsigned char *buf, size_t count, int kernel);

/**
 * @brief Perform a select operation on an input core device (major = 13).
 *
 * After opening an input core device, a select operation
 * can be performed on the device (accessed via the file struct \a f).
 * The operation (passed in \a which), can be FREAD, FWRITE, 0, or a 
 * combination of these.
 *
 * @param   f       open file struct
 * @param   which   the select operation to perform
 *
 * @return  1 if there are selectable events, 0 otherwise.
 */
int inputdev_select(struct file_t *f, int which);

/**
 * @brief Perform a poll operation on an input core device (major = 13).
 *
 * After opening an input core device, a poll operation
 * can be performed on the device (accessed via the file struct \a f).
 * The \a events field of \a pfd contains a combination of requested events
 * (POLLIN, POLLOUT, ...). The resultant events are returned in the \a revents
 * member of \a pfd.
 *
 * @param   f       open file struct
 * @param   pfd     poll events
 *
 * @return  1 if there are pollable events, 0 otherwise.
 */
int inputdev_poll(struct file_t *f, struct pollfd *pfd);

/**
 * @brief Perform a select operation on a memory core device (major = 1).
 *
 * After opening a memory core device, a select operation
 * can be performed on the device (accessed via the file struct \a f).
 * The operation (passed in \a which), can be FREAD, FWRITE, 0, or a 
 * combination of these.
 *
 * @param   f       open file struct
 * @param   which   the select operation to perform
 *
 * @return  1 if there are selectable events, 0 otherwise.
 */
int memdev_char_select(struct file_t *f, int which);

/**
 * @brief Perform a poll operation on a memory core device (major = 1).
 *
 * After opening a memory core device, a poll operation
 * can be performed on the device (accessed via the file struct \a f).
 * The \a events field of \a pfd contains a combination of requested events
 * (POLLIN, POLLOUT, ...). The resultant events are returned in the \a revents
 * member of \a pfd.
 *
 * @param   f       open file struct
 * @param   pfd     poll events
 *
 * @return  1 if there are pollable events, 0 otherwise.
 */
int memdev_char_poll(struct file_t *f, struct pollfd *pfd);

/**
 * @brief Read from char device /dev/mouse0.
 *
 * Read bytes from the character mouse device (major = 13, minor = 32).
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read (must be >= 
 *                    sizeof(struct mouse_packet_t))
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t mousedev_read(dev_t dev, unsigned char *buf, size_t count);

/**
 * @brief Perform a select operation on /dev/mouse0.
 *
 * After opening the mouse device (major = 13, minor = 32), a select operation
 * can be performed on the device (accessed via the file struct \a f).
 * The operation (passed in \a which), can be FREAD, FWRITE, 0, or a 
 * combination of these.
 *
 * @param   dev     device id
 * @param   which   the select operation to perform
 *
 * @return  1 if there are selectable events, 0 otherwise.
 */
int mousedev_select(dev_t dev, int which);

/**
 * @brief Perform a poll operation on /dev/mouse0.
 *
 * After opening the mouse device (major = 13, minor = 32), a poll operation
 * can be performed on the device (accessed via the file struct \a f).
 * The \a events field of \a pfd contains a combination of requested events
 * (POLLIN, POLLOUT, ...). The resultant events are returned in the \a revents
 * member of \a pfd.
 *
 * @param   dev     device id
 * @param   pfd     poll events
 *
 * @return  1 if there are pollable events, 0 otherwise.
 */
int mousedev_poll(dev_t dev, struct pollfd *pfd);


/**
 * @brief General device control function.
 *
 * Perform ioctl operations on a sound device (major = 14).
 *
 * @param   dev     device id
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int snddev_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel);

/**
 * @brief Write to a sound device (major = 14).
 *
 * Switch function to write bytes to a sound character device.
 *
 * @param   dev     device id
 * @param   buf     buffer to write from
 * @param   count   number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t snddev_write(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel);

/**
 * @brief Read from a sound device (major = 14).
 *
 * Switch function to read bytes from a sound character device.
 *
 * @param   dev     device id
 * @param   buf     buffer to read to
 * @param   count   number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t snddev_read(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel);

/**
 * @brief Perform a select operation on a sound device (major = 14).
 *
 * After opening a sound device, a select operation
 * can be performed on the device (accessed via the file struct \a f).
 * The operation (passed in \a which), can be FREAD, FWRITE, 0, or a 
 * combination of these.
 *
 * @param   f       open file struct
 * @param   which   the select operation to perform
 *
 * @return  1 if there are selectable events, 0 otherwise.
 */
int snddev_select(struct file_t *f, int which);


/******************************
 * Block device functions
 ******************************/

/**
 * @brief Block device write function.
 *
 * Perform write operations on block disk devices.
 *
 * @param   dev         device id
 * @param   pos         offset in \a buf to write from
 * @param   buf         buffer to write from
 * @param   count       number of bytes to write
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t block_write(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel);

/**
 * @brief Block device read function.
 *
 * Perform read operations on block disk devices.
 *
 * @param   dev         device id
 * @param   pos         offset in \a buf to read to
 * @param   buf         buffer to read to
 * @param   count       number of bytes to read
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t block_read(struct file_t *f, off_t *pos,
                   unsigned char *buf, size_t count, int kernel);


/***********************************
 * RAM disk block device functions
 ***********************************/

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
int ramdev_strategy(struct disk_req_t *req);

/**
 * @brief Decompress the initial RAM disk (initrd).
 *
 * Decompress the initial RAM disk to memory and add /dev/ram0 and /dev/initrd
 * to the kernel's device tree.
 *
 * @param   data_start  start address of the initial RAM disk in memory
 * @param   data_end    end address of the initial RAM disk in memory
 *
 * @return  zero on success, -(errno) on failure.
 */
int ramdisk_init(virtual_addr data_start, virtual_addr data_end);

/**
 * @brief General device control function.
 *
 * Perform ioctl operations on a RAM disk (block, major = 1).
 *
 * @param   dev_id  device id
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int ramdev_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel);


/**************************************
 * Helper functions
 **************************************/

int syscall_ioctl_internal(int fd, unsigned int cmd, char *arg, int kernel);

#endif      /* __DEV_H__ */
