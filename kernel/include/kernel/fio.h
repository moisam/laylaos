/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: fio.h
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
 *  \file fio.h
 *
 *  Helper functions for performing different file I/O operations.
 */

#ifndef __FIO_H__
#define __FIO_H__


/**
 * @brief Get file struct and node.
 *
 * Given the file descriptor \a fd, which is opened by \a task, get the 
 * \a file_t and the \a fs_node_t structs corresponding to this file.
 *
 * @param   fd          file descriptor
 * @param   t           task
 * @param   f           the file struct is returned here
 * @param   node        the file node struct is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
static inline int fdnode(int fd, struct task_t *t,
                         struct file_t **f, struct fs_node_t **node)
{
    *f = NULL;
    *node = NULL;

    if(!t || !t->ofiles)
    {
        return -EBADF;
    }

    if(fd < 0 || fd >= NR_OPEN ||
       !(*f = t->ofiles->ofile[fd]) || !(*node = (*f)->node))
    {
        return -EBADF;
    }
    
    return 0;
}


/**
 * @brief Check for valid file descriptors.
 *
 * Given the file descriptor \a fd, which is opened by \a task, check if the
 * given file descriptor is valid.
 *
 * @param   fd          file descriptor
 * @param   t           task
 *
 * @return  1 on success, 0 on failure.
 */
static inline int validfd(int fd, struct task_t *ct)
{
	if(fd < 0 || fd >= NR_OPEN || !ct->ofiles || !ct->ofiles->ofile[fd])
	{
		return 0;
	}
	
	return 1;
}

#endif      /* __FIO_H__ */
