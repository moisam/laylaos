/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: pipefs.c
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
 *  \file pipefs.c
 *
 *  This file implements pipefs filesystem functions, which provide access to
 *  the pipefs virtual filesystem.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/ksignal.h>
#include <kernel/user.h>
#include <kernel/fcntl.h>
#include <mm/mmngr_phys.h>
#include <mm/kstack.h>
#include <fs/pipefs.h>


#define PIPE_SIZE               (PAGE_SIZE * 2)

#define EMPTY_PIPE(head, tail)  ((head) == (tail))
#define FULL_PIPE(head, tail)   ((((tail) + 1) & (PIPE_SIZE - 1)) == (head))

#define PIPE_USED(head, tail)   ((tail + PIPE_SIZE - head) & (PIPE_SIZE - 1))
#define PIPE_SPACE_FOR(head, tail, n)   ((PIPE_SIZE - PIPE_USED(head, tail)) >= n)


/*
 * Free a pipe.
 */
void pipefs_free_node(struct fs_node_t *node)
{
    vmmngr_free_pages((virtual_addr)node->size, PIPE_SIZE);

    node->size = 0;
    node->refs = 0;
    node->blocks[0] = 0;    // pipe head pointer
    node->blocks[1] = 0;    // pipe tail pointer
}


/*
 * Create a new pipe node.
 */
struct fs_node_t *pipefs_get_node(void)
{
    struct fs_node_t *node;
    physical_addr phys;
    int flags = (this_core->cur_task->user ? I86_PTE_USER : 0) |
                  I86_PTE_PRESENT | I86_PTE_WRITABLE;

    // get a free node
    if((node = get_empty_node()) == NULL)
    {
        return NULL;
    }
    
    if((node->size = vmmngr_alloc_and_map(PIPE_SIZE, 0,
                                          flags, &phys, 
                                          REGION_PIPE)) == 0)
    {
        node->refs = 0;
        return NULL;
    }

    // exactly 2 = reader + writer
    node->refs = 2;
    node->blocks[0] = 0;    // pipe head pointer
    node->blocks[1] = 0;    // pipe tail pointer
    node->mode = S_IFIFO;
    node->flags |= FS_NODE_PIPE;

    node->select = pipefs_select;
    node->poll = pipefs_poll;

    node->read = pipefs_read;
    node->write = pipefs_write;

    return node;
}


/*
 * Read from a pipe.
 */
ssize_t pipefs_read(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t _count, int kernel)
{
    UNUSED(pos);

    struct fs_node_t *node = f->node;
    volatile size_t count = _count;
    volatile unsigned long head, tail;

    if(!(f->mode & PREAD_MODE))
    {
        return -EINVAL;
    }

    // check the given user address is valid
    if(!kernel &&
       valid_addr(this_core->cur_task, 
                    (virtual_addr)buf, (virtual_addr)buf + count - 1) != 0)
    {
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, (void *)buf);
        return -EFAULT;
    }

    if(count == 0)
    {
        return 0;
    }

    // now read
    unsigned char *d = buf;
    unsigned char *s = (unsigned char *)node->size;
    head = node->blocks[1];
    tail = node->blocks[0];

    if(node->size == 0)
    {
        kpanic("pipefs: reading from a deallocated pipe\n");
    }

    // if the pipe is empty:
    //   - return 0 if the writing end is closed
    //   - return -EAGAIN if this is a non-blocking file descriptor
    //   - sleep and wait for input otherwise
    while(EMPTY_PIPE(head, tail))
    {
        selwakeup(&node->select_channel);   // wakeup writers

        if(node->refs < 2)     // no more writers
        {
            return 0;
        }

        if(f->flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }

        selrecord(&node->select_channel);

        // wait for writers
        if(block_task2(&node->select_channel, PIT_FREQUENCY * 3) == EINTR)
        {
            return -EINTR;
        }

        head = node->blocks[1];
        tail = node->blocks[0];
    }

    while(count != 0 && !EMPTY_PIPE(head, tail))
    {
        count--;

        *d = s[head];
        d++;

        head = (head + 1) & (PIPE_SIZE - 1);
        node->blocks[1] = head;
        tail = node->blocks[0];
    }

    selwakeup(&node->select_channel);   // wakeup writers
    return d - buf;
}


/*
 * Write to a pipe.
 */
ssize_t pipefs_write(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t _count, int kernel)
{
    UNUSED(pos);

    struct fs_node_t *node = f->node;
    volatile size_t count = _count;
    volatile unsigned long head, tail;

    if(!(f->mode & PWRITE_MODE))
    {
        return -EINVAL;
    }

    // check the given user address is valid
    if(!kernel &&
       valid_addr(this_core->cur_task, 
                (virtual_addr)buf, (virtual_addr)buf + count - 1) != 0)
    {
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, (void *)buf);
        return -EFAULT;
    }

    if(node->refs < 2)     // no readers
    {
        user_add_task_signal(this_core->cur_task, SIGPIPE, 1);
        return -EPIPE;
    }

    if(count == 0)
    {
        return 0;
    }

    // now write
    unsigned char *d = buf;
    unsigned char *s = (unsigned char *)node->size;
    head = node->blocks[1];
    tail = node->blocks[0];

    if(node->size == 0)
    {
        kpanic("pipefs: writing to a deallocated pipe\n");
    }

    /*
     * According to pipe(7) manpage, O_NONBLOCK and the size of the write
     * are handled in the following way:
     *
     * O_NONBLOCK disabled, n <= PIPE_BUF
     *        All n bytes are written atomically; write(2) may block if
     *        there is not room for n bytes to be written immediately
     *
     * O_NONBLOCK enabled, n <= PIPE_BUF
     *        If there is room to write n bytes to the pipe, then
     *        write(2) succeeds immediately, writing all n bytes;
     *        otherwise write(2) fails, with errno set to EAGAIN.
     *
     * O_NONBLOCK disabled, n > PIPE_BUF
     *        The write is nonatomic: the data given to write(2) may be
     *        interleaved with write(2)s by other process; the write(2)
     *        blocks until n bytes have been written.
     *
     * O_NONBLOCK enabled, n > PIPE_BUF
     *        If the pipe is full, then write(2) fails, with errno set
     *        to EAGAIN.  Otherwise, from 1 to n bytes may be written
     *        (i.e., a "partial write" may occur; the caller should
     *        check the return value from write(2) to see how many bytes
     *        were actually written), and these bytes may be interleaved
     *        with writes by other processes.
     */

    if((f->flags & O_NONBLOCK) && !PIPE_SPACE_FOR(head, tail, count))
    {
        if(count <= PIPE_BUF || FULL_PIPE(head, tail))
        {
            return -EAGAIN;
        }
    }

    while(count != 0)
    {
        count--;

        while(FULL_PIPE(head, tail))
        {
            selwakeup(&node->select_channel);   // wakeup readers

            if(node->refs < 2)     // no readers
            {
                user_add_task_signal(this_core->cur_task, SIGPIPE, 1);
                return -EPIPE;
            }

            if((f->flags & O_NONBLOCK) && _count > PIPE_BUF)
            {
                return d - buf;
            }

            selrecord(&node->select_channel);

            // wait for readers
            if(block_task2(&node->select_channel, PIT_FREQUENCY * 3) == EINTR)
            {
                return -EINTR;
            }

            head = node->blocks[1];
            tail = node->blocks[0];
        }

        s[tail] = *d++;

        tail = (tail + 1) & (PIPE_SIZE - 1);
        node->blocks[0] = tail;
        head = node->blocks[1];
    }

    selwakeup(&node->select_channel);   // wakeup readers

    return d - buf;
}


/*
 * Perform a select operation on a pipe.
 */
long pipefs_select(struct file_t *f, int which)
{
	switch(which)
	{
    	case FREAD:
            if(!EMPTY_PIPE(f->node->blocks[1], f->node->blocks[0]))
    		{
    			return 1;
    		}

            // if there are no writers, wakeup readers so they can read EOF
            if(f->node->refs != 2)
    		{
    			return 1;
    		}
    		
    		selrecord(&f->node->select_channel);
    		break;

    	case FWRITE:
            if(!FULL_PIPE(f->node->blocks[1], f->node->blocks[0]))
    		{
    			return 1;
    		}

            // if there are no readers, wakeup writers so they can get SIGPIPE
            if(f->node->refs != 2)
    		{
    			return 1;
    		}
		    
    		selrecord(&f->node->select_channel);
    		break;

    	case 0:
    	    // TODO: (we should be handling exceptions)
            if(f->node->refs != 2)
    		{
    			return 1;
    		}

    		break;
	}

	return 0;
}


/*
 * Perform a poll operation on a pipe.
 */
long pipefs_poll(struct file_t *f, struct pollfd *pfd)
{
    long res = 0;

    if(pfd->events & POLLIN)
    {
        if(!EMPTY_PIPE(f->node->blocks[1], f->node->blocks[0]) ||
           f->node->refs != 2)
        {
            pfd->revents |= POLLIN;
            res = 1;
        }
        else
        {
    		selrecord(&f->node->select_channel);
        }
    }

    if(pfd->events & POLLOUT)
    {
        if(!FULL_PIPE(f->node->blocks[1], f->node->blocks[0]) ||
           f->node->refs != 2)
        {
            pfd->revents |= POLLOUT;
            res = 1;
        }
        else
        {
    		selrecord(&f->node->select_channel);
        }
    }

    // one end of the pipe has been closed
    if(f->node->refs != 2)
    {
        pfd->revents |= POLLHUP;
        res = 1;

        // this is the writing end of the pipe and there are no readers
        if(f->mode == 2)
        {
            pfd->revents |= POLLERR;
        }
    }
    
    return res;
}

