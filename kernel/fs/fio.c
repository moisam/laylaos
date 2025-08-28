/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: fio.c
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
 *  \file fio.c
 *
 *  Helper functions for performing different file I/O operations.
 */

#include <errno.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
//#include <fs/sockfs.h>
#include <kernel/loop_internal.h>
#include <fs/devpts.h>
#include <mm/kheap.h>
#include <kernel/net/socket.h>


long fdnode(int fd, struct task_t *t, struct file_t **f, struct fs_node_t **node)
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


static long fdalloc(int *res)
{
    int fd;
	volatile struct task_t *ct = this_core->cur_task;
    
    *res = -1;
    
	// find an empty slot in the task's file table
	for(fd = 0; fd < NR_OPEN; fd++)
	{
		if(ct->ofiles->ofile[fd] == NULL)
		{
        	// clear the close-on-exec flag
        	cloexec_clear(ct, fd);
		    *res = fd;
		    return 0;
	    }
	}
	
	// task's file table is full
	return -ENFILE;
}


/* 
 * Allocate a user file descriptor and a file struct, with the descriptor 
 * pointing at the struct.
 *
 * Output:
 *    *_fd => file descriptor in the range 0..NR_OPEN
 *    *_f => pointer to file struct in the master file table
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
long falloc(int *_fd, struct file_t **_f)
{
	struct file_t *f, *lf;
	int fd;
	volatile struct task_t *ct = this_core->cur_task;

	*_f = NULL;
	*_fd = -1;

    if(fdalloc(&fd) != 0)
    {
    	return -ENFILE;
    }

    if(fd < 0)
    {
    	return -ENFILE;
    }
    
	// find an empty slot in the master file table
	for(f = ftab, lf = &ftab[NR_FILE]; f < lf; f++)
	{
    	kernel_mutex_lock(&f->lock);
		if(!f->refs)
		{
        	//f->refs++;
            __sync_fetch_and_add(&(f->refs), 1);
        	kernel_mutex_unlock(&f->lock);
		    break;
		}
    	kernel_mutex_unlock(&f->lock);
    }
    
	// master file table is full
	if(f >= lf)
	{
		return -EMFILE;
	}
	
	ct->ofiles->ofile[fd] = f;
	*_f = f;
	*_fd = fd;
	
	return 0;
}


long closef(struct file_t *f)
{
    if(!f)
    {
        return 0;
    }
    
	if(f->refs == 0)
	{
		printk("vfs: closing a file with 0 refs\n");
		return -EINVAL;
	}

    __sync_fetch_and_sub(&(f->refs), 1);

	if(f->refs)
	//if(--f->refs)
	{
	    // last close of a loopback device
	    if(f->refs == 1 && f->node && (f->node->flags & FS_NODE_LOOP_BACKING))
	    {
	        lodev_release(f);
	    }

		return 0;
	}
	
	if(f->node)
	{
        struct fs_node_t *node = f->node;

        f->node = NULL;

	    if(IS_SOCKET(node))
    	{
    	    //sockfs_close(f);
    	    socket_close(node->data);
            node->data = 0;
            node->links = 0;
	    }
	    else if(S_ISCHR(node->mode))
	    {
            if(MAJOR(node->blocks[0]) == PTY_SLAVE_MAJ)
    	    {
    	        pty_slave_close(node);
    	    }
    	    else if(MAJOR(node->blocks[0]) == PTY_MASTER_MAJ)
    	    {
    	        // instead of calling release_node(), free the node here as
    	        // it was independently alloc'd by pty_master_create()
    	        pty_master_close(node);
    	    }

#if 0

        	register struct task_t *t = get_cur_task();

            /* release tty if needed */
            if(f->node->blocks[0] == t->ctty)
            {
                set_controlling_tty(t->ctty, get_struct_tty(t->ctty), 0);
            }

#endif

	    }
	    else
	    {
            // if this is the last close of a file referencing this node, and the
            // node's links == 0 (i.e. marked for deletion), remove any hanging
            // cache pages so the node can finally be deleted
            if(node->links == 0 && files_referencing_node(node) == 0)
            {
                remove_unreferenced_cached_pages(node);
            }
	    }

    	release_node(node);
	}
	
	return 0;
}

