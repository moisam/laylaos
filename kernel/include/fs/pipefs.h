/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: pipefs.h
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
 *  \file pipefs.h
 *
 *  Functions and macros for working with the pipefs filesystem.
 */

#ifndef __PIPE_FSYS_H__
#define __PIPE_FSYS_H__

#include <stdint.h>
#include <sys/types.h>
#include <poll.h>


/**
 * @brief Free a pipe.
 *
 * Free a pipe, i.e. release the memory page used to implement the pipe and
 * set the file's size to zero.
 *
 * @param   node        file node referring to the open pipe
 *
 * @return  nothing.
 */
void pipefs_free_node(struct fs_node_t *node);

/**
 * @brief Create a new pipe node.
 *
 * Create a new file node that can be used as a pipe. This function allocates
 * a memory page (usually 4096 bytes) to be used as the pipe's First-In
 * First-Out (FIFO) queue and sets the appropriate file node fields.
 *
 * @return  new file node on success, NULL on failure.
 */
struct fs_node_t *pipefs_get_node(void);

/**
 * @brief Read from a pipe.
 *
 * Read at most \a count bytes from the pipe referred to by the given file
 * \a node into \a buf.
 *
 * @param   f           open file struct
 * @param   pos         position in file to read from
 * @param   buf         buffer to read data into
 * @param   count       maximum amount of bytes to read into \a buf
 * @param   kernel      0 if call is coming from userland, non-zero if from kernel
 *
 * @return number of bytes read on success, -(errno) on failure
 */
ssize_t pipefs_read(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel);

/**
 * @brief Write to a pipe.
 *
 * Write at most \a count bytes from \a buf to the pipe referred to by the
 * given file \a node.
 *
 * @param   f           open file struct
 * @param   pos         position in file to write to
 * @param   buf         buffer to write data from
 * @param   count       maximum amount of bytes to write from \a buf
 * @param   kernel      0 if call is coming from userland, non-zero if from kernel
 *
 * @return number of bytes written on success, -(errno) on failure
 */
ssize_t pipefs_write(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel);

/**
 * @brief Perform a select operation on a pipe.
 *
 * Perform a select operation on the pipe referred to by the file \a f.
 * The operation (passed in \a which), can be FREAD, FWRITE, 0, or a 
 * combination of these.
 *
 * @param   f       open file struct
 * @param   which   the select operation to perform
 *
 * @return  1 if there are selectable events, 0 otherwise.
 */
long pipefs_select(struct file_t *f, int which);

/**
 * @brief Perform a poll operation on a pipe.
 *
 * Perform a poll operation on the pipe referred to by the file \a f.
 * The \a events field of \a pfd contains a combination of requested events
 * (POLLIN, POLLOUT, ...). The resultant events are returned in the \a revents
 * member of \a pfd.
 *
 * @param   f       open file struct
 * @param   pfd     poll events
 *
 * @return  1 if there are pollable events, 0 otherwise.
 */
long pipefs_poll(struct file_t *f, struct pollfd *pfd);

#endif      /* __PIPE_FSYS_H__ */
