/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: sockfs.h
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
 *  \file sockfs.h
 *
 *  Functions and macros for working with the sockfs filesystem.
 */

#ifndef __SOCK_FSYS_H__
#define __SOCK_FSYS_H__

#ifndef KERNEL
#define KERNEL
#endif

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <poll.h>


/**
 * @brief Create a new socket node.
 *
 * Create a new file node that can be used as a socket. This function sets
 * the appropriate file node fields for a socket.
 *
 * @return  new file node on success, NULL on failure.
 */
struct fs_node_t *sockfs_get_node(void);

/**
 * @brief Read from a socket.
 *
 * Read at most \a count bytes from the socket referred to by the given file
 * \a node into \a buf.
 *
 * @param   f           open file referring to the open socket
 * @param   pos         position in file to read from
 * @param   buf         buffer to read data into
 * @param   count       maximum amount of bytes to read into \a buf
 * @param   kernel      0 if call is coming from userland, non-zero if from kernel
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t sockfs_read(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel);

/**
 * @brief Write to a socket.
 *
 * Write at most \a count bytes from \a buf to the socket referred to by the
 * given file \a node.
 *
 * @param   f           open file referring to the open socket
 * @param   pos         position in file to write to
 * @param   buf         buffer to write data from
 * @param   count       maximum amount of bytes to write from \a buf
 * @param   kernel      0 if call is coming from userland, non-zero if from kernel
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t sockfs_write(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel);

/**
 * @brief General block device control function.
 *
 * Perform ioctl operations on a socket.
 *
 * @param   fp      file node referring to the open socket
 * @param   cmd     ioctl command (device specific)
 * @param   data    optional argument (depends on \a cmd)
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero or a positive result on success, -(errno) on failure.
 */
int sockfs_ioctl(struct file_t *fp, int cmd, char *data, int kernel);

/**
 * @brief Perform a select operation on a socket.
 *
 * Perform a select operation on the socket referred to by the file \a f.
 * The operation (passed in \a which), can be FREAD, FWRITE, 0, or a 
 * combination of these.
 *
 * @param   fp      open file struct
 * @param   which   the select operation to perform
 *
 * @return  1 if there are selectable events, 0 otherwise.
 */
int sockfs_select(struct file_t *fp, int which);

/**
 * @brief Perform a poll operation on a socket.
 *
 * Perform a poll operation on the socket referred to by the file \a f.
 * The \a events field of \a pfd contains a combination of requested events
 * (POLLIN, POLLOUT, ...). The resultant events are returned in the \a revents
 * member of \a pfd.
 *
 * @param   f       open file struct
 * @param   pfd     poll events
 *
 * @return  1 if there are pollable events, 0 otherwise.
 */
int sockfs_poll(struct file_t *f, struct pollfd *pfd);

#endif      /* __SOCK_FSYS_H__ */
