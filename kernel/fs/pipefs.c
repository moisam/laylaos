/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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


#define PIPE_SIZE           (PAGE_SIZE * 2)

#define EMPTY_PIPE(p)       ((p)->blocks[0] == (p)->blocks[1])
#define FULL_PIPE(p)        ((((p)->blocks[0] - (p)->blocks[1]) & \
                             (PIPE_SIZE - 1)) ==          \
                                    (PIPE_SIZE - 1))


/*
 * Free a pipe.
 */
void pipefs_free_node(struct fs_node_t *node)
{
    vmmngr_free_pages((virtual_addr)node->size, PIPE_SIZE);

    node->size = 0;
}


/*
 * Create a new pipe node.
 */
struct fs_node_t *pipefs_get_node(void)
{
    struct fs_node_t *node;
    physical_addr phys;
    struct task_t *ct = cur_task;
    int flags = (ct->user ? I86_PTE_USER : 0) |
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
                    unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    struct task_t *ct = cur_task;

    if(!(f->mode & PREAD_MODE))
    {
        return -EINVAL;
    }

    // check the given user address is valid
    if(valid_addr(ct, (virtual_addr)buf, (virtual_addr)buf + count - 1) != 0)
    {
        add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)buf);
        return -EFAULT;
    }

    while(EMPTY_PIPE(f->node))   // empty node
    {
        //printk("pipefs_read: pipe full - refs %d\n", node->refs);
        unblock_tasks(&f->node->sleeping_task);    // wakeup writers
        selwakeup(&f->node->select_channel);
        
        if(f->node->refs < 2)     // no more writers
        {
            return 0;
        }

        if(f->flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }
        
        block_task2(&f->node->sleeping_task, 1000);       // wait for writers
    }
    
    // now read
    unsigned char *d = buf;
    unsigned char *s = (unsigned char *)f->node->size;

    while((count != 0) && (f->node->blocks[0] != f->node->blocks[1]))
    {
        count--;

        *d = s[f->node->blocks[1]];
        d++;

        f->node->blocks[1] = (f->node->blocks[1] + 1) % PIPE_SIZE;
    }

    unblock_tasks(&f->node->sleeping_task);    // wakeup writers
    selwakeup(&f->node->select_channel);
    return d - buf;
}


/*
 * Write to a pipe.
 */
ssize_t pipefs_write(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    struct task_t *ct = cur_task;

    if(!(f->mode & PWRITE_MODE))
    {
        return -EINVAL;
    }

    // check the given user address is valid
    if(valid_addr(ct, (virtual_addr)buf, (virtual_addr)buf + count - 1) != 0)
    {
        add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)buf);
        return -EFAULT;
    }

    unblock_tasks(&f->node->sleeping_task);    // wakeup readers
    //unblock_tasks(&node->blocks[1]);    // wakeup select() readers
    selwakeup(&f->node->select_channel);

    if(f->node->refs < 2)     // no readers
    {
        user_add_task_signal(ct, SIGPIPE, 1);
        return -EPIPE;
    }

    // now write
    unsigned char *d = buf;
    unsigned char *s = (unsigned char *)f->node->size;
    
    while(count != 0)
    {
        count--;

        // while pipe is full
        while(FULL_PIPE(f->node))
        {
            unblock_tasks(&f->node->sleeping_task);    // wakeup readers
            selwakeup(&f->node->select_channel);

            if(f->node->refs < 2)     // no readers
            {
                user_add_task_signal(ct, SIGPIPE, 1);
                return -EPIPE;
            }

            if(f->flags & O_NONBLOCK)
            {
                return -EAGAIN;
            }

            block_task(&f->node->sleeping_task, 0);       // wait for readers
        }
        
        s[f->node->blocks[0]] = *d++;
        
        f->node->blocks[0] = (f->node->blocks[0] + 1) % PIPE_SIZE;
    }

    unblock_tasks(&f->node->sleeping_task);    // wakeup readers
    selwakeup(&f->node->select_channel);

    return d - buf;
}


/*
 * Perform a select operation on a pipe.
 */
int pipefs_select(struct file_t *f, int which)
{
	switch(which)
	{
    	case FREAD:
            if(!EMPTY_PIPE(f->node))
    		{
    			return 1;
    		}
    		
    		// TODO: select on the pipe's reading end
    		//selrecord(&node->blocks[1]);    
    		selrecord(&f->node->select_channel);
    		break;

    	case FWRITE:
            if(!FULL_PIPE(f->node))
    		{
    			return 1;
    		}
		    
		    // TODO: select on the pipe's writing end
    		//selrecord(&node->blocks[0]);    
    		selrecord(&f->node->select_channel);
    		break;

    	case 0:
    	    // TODO: (we should be handling exceptions)
    		break;
	}

	return 0;
}


/*
 * Perform a poll operation on a pipe.
 */
int pipefs_poll(struct file_t *f, struct pollfd *pfd)
{
    int res = 0;

    if(pfd->events & POLLIN)
    {
        if(!EMPTY_PIPE(f->node))
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
        if(!FULL_PIPE(f->node))
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

