/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: dummy.h
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
 *  \file dummy.h
 *
 *  Functions and macros for dummy filesystem functions.
 */

#ifndef __DUMMY_FSYS_H__
#define __DUMMY_FSYS_H__

#include <stdint.h>
#include <sys/types.h>
#include <poll.h>

/**
 * @var dummyfs_ops
 * @brief dummy filesystem ops.
 *
 * A filesystem operations structure for unmountable filesystems (e.g. sockfs
 * and pipefs).
 */
extern struct fs_ops_t dummyfs_ops;

/**
 * @brief General block device control function.
 *
 * A dummy ioctl function for devices with no ioctl functionality.
 *
 * @param   dev_id  device id
 * @param   cmd     ioctl command (device specific)
 * @param   arg     optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
long dummyfs_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel);

/**
 * @brief Perform a dummy select operation.
 *
 * A dummy select function for devices with no select functionality.
 *
 * @param   f       open file struct
 * @param   which   the select operation to perform
 *
 * @return  always 1.
 */
long dummyfs_select(struct file_t *f, int which);

/**
 * @brief Perform a dummy poll operation.
 *
 * A dummy poll function for devices with no polling functionality.
 *
 * @param   f       open file struct
 * @param   pfd     poll events
 *
 * @return  always 1.
 */
long dummyfs_poll(struct file_t *f, struct pollfd *pfd);

/**
 * @brief Perform a dummy read operation.
 *
 * A dummy read function for devices with no such functionality.
 *
 * @param   f       open file struct
 * @param   pos     position in file to read from
 * @param   buf     data is copied to this buffer
 * @param   count   number of characters to read
 * @param   kernel  0 if call is coming from userland, non-zero if from kernel
 *
 * @return  number of characters read on success, -(errno) on failure
 */
ssize_t dummyfs_read(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel);

/**
 * @brief Perform a dummy write operation.
 *
 * A dummy write function for devices with no such functionality.
 *
 * @param   f       open file struct
 * @param   pos     position in file to write to
 * @param   buf     data is copied from this buffer
 * @param   count   number of characters to write
 * @param   kernel  0 if call is coming from userland, non-zero if from kernel
 *
 * @return  number of characters read on success, -(errno) on failure
 */
ssize_t dummyfs_write(struct file_t *f, off_t *pos,
                      unsigned char *buf, size_t count, int kernel);

#endif      /* __DUMMY_FSYS_H__ */
