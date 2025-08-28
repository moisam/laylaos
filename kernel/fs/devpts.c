/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: devpts.c
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
 *  \file devpts.c
 *
 *  This file implements the devpts filesystem, which provides the
 *  functionality to work with pseudo-terminal (pty) devices.
 *  The pty device multiplexer is accessed by opening /dev/ptmx. The
 *  slave pty devices are accessed via /dev/pts, which is where the
 *  devpts is usually mounted.
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref devpts_ops structure.
 */

//#define __DEBUG

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <fs/devpts.h>
#include <kernel/laylaos.h>
#include <kernel/tty.h>
#include <kernel/task.h>
#include <kernel/mutex.h>
#include <kernel/user.h>
#include <kernel/clock.h>
#include <kernel/kgroups.h>
#include <kernel/syscall.h>
#include <kernel/dev.h>
#include <kernel/fcntl.h>
#include <mm/kheap.h>

#include "../kernel/tty_inlines.h"

#define LAST_PTY                &(pty_slaves[MAX_PTY_DEVICES])

// although we start numbering slave pty devices from 0, we start numbering
// the associated inodes from 2, to (a) maintain uniformity with other
// filesystems, especially ext2, and (b) allow error checking when an inode
// number of zero is passed to the vfs.
#define ROOT_INODE              2
#define FIRST_INODE             3   // first inode after the root
#define LAST_INODE              (MAX_PTY_DEVICES + FIRST_INODE)

// access mode for /dev/pts (drwxr-xr-x)
#define ROOT_MODE               (S_IFDIR | 0755)

// this allows us to mount devpts on /dev/pts
dev_t DEVPTS_DEVID = TO_DEVID(240, 3);

// pseudoterminal master multiplexor device number
dev_t PTMX_DEVID = TO_DEVID(5, 2);

// pseudoterminal slave devices list
struct pty_t *pty_slaves[MAX_PTY_DEVICES];

// and the lock to access the above list
volatile struct kernel_mutex_t pty_lock;

// devpts root -> /dev/pts
struct fs_node_t *devpts_root;

// filesystem operations
struct fs_ops_t devpts_ops =
{
    // inode operations
    .read_inode = devpts_read_inode,
    .write_inode = devpts_write_inode,
    //.trunc_inode = devpts_trunc_inode,
    .alloc_inode = NULL,
    .free_inode = NULL,
    .bmap = NULL,
    .read_symlink = NULL,
    .write_symlink = NULL,

    // directory operations
    .finddir = devpts_finddir,
    .finddir_by_inode = devpts_finddir_by_inode,
    .addir = NULL,
    .mkdir = NULL,
    .deldir = NULL,
    .dir_empty = NULL,
    .getdents = devpts_getdents,

    // device operations
    .mount = NULL,
    .umount = NULL,
    .read_super = devpts_read_super,
    .write_super = NULL,
    .put_super = devpts_put_super,
    .ustat = NULL,
    .statfs = NULL,
};


/*
 * Initialize devpts.
 */
void devpts_init(void)
{
    // make sure devfs is init'ed only once
    static int inited = 0;
    //struct pty_t *dev;
    time_t t = now();
    
    if(inited)
    {
        printk("devpts: trying to re-init devpts\n");
        return;
    }
    
    memset(pty_slaves, 0, sizeof(pty_slaves));
    init_kernel_mutex(&pty_lock);
    fs_register("devpts", &devpts_ops);

    if(!(devpts_root = get_empty_node()))
    {
        kpanic("Failed to create devpts!\n");
    }

    devpts_root->ops = &devpts_ops;
    devpts_root->mode = ROOT_MODE;
    devpts_root->links = 2;
    devpts_root->refs = 1;
    devpts_root->size = 2;
    devpts_root->inode = ROOT_INODE;
    devpts_root->ctime = t;
    devpts_root->mtime = t;
    devpts_root->atime = t;

    // use one of the reserved dev ids
    devpts_root->dev = DEVPTS_DEVID;

    inited = 1;
}


/*
 * Read the filesystem's superblock and root inode.
 * This function fills in the mount info struct's block_size, super,
 * and root fields.
 */
long devpts_read_super(dev_t dev, struct mount_info_t *d,
                       size_t bytes_per_sector)
{
    UNUSED(bytes_per_sector);

    if(MINOR(dev) != MINOR(DEVPTS_DEVID))
    {
        return -EINVAL;
    }
    
    d->block_size = 0;
    d->super = NULL;
    d->root = devpts_root;

    return 0;
}


static inline int valid_devpts_node(struct fs_node_t *node)
{
    if(!node || node->dev != DEVPTS_DEVID /* MAJOR(node->dev) != PTY_SLAVE_MAJ */)
    {
        return 0;
    }
    
    return 1;
}


static void devpts_free_inode_internal(struct pty_t **ptyptr)
{
    KDEBUG("%s:\n", __func__);
    
    struct pty_t *pty = *ptyptr;

    if(pty)
    {
        kfree((void *)pty->tty.read_q.buf);
        kfree((void *)pty->tty.write_q.buf);
        kfree((void *)pty->tty.secondary.buf);
        kfree(pty);
        *ptyptr = NULL;

        devpts_root->size--;
        devpts_root->links--;
    }
}


/*
 * Release the filesystem's superblock and its buffer.
 * Called when unmounting the filesystem.
 */
void devpts_put_super(dev_t dev, struct superblock_t *super)
{
    KDEBUG("%s:\n", __func__);

    struct pty_t **n, **ln = LAST_PTY;
    //struct pty_t *slave;

    UNUSED(dev);
    UNUSED(super);

    kernel_mutex_lock(&pty_lock);

    for(n = pty_slaves; n < ln; n++)
    {
        devpts_free_inode_internal(n);
    }

    kernel_mutex_unlock(&pty_lock);
}


/*
 * Get a pseudo-terminal device's tty struct.
 */
struct tty_t *devpts_get_struct_tty(dev_t dev)
{
    if(dev <= 0)
    {
        return NULL;
    }

    int minor = MINOR(dev);
    int major = MAJOR(dev);
    
    if(major != PTY_MASTER_MAJ && major != PTY_SLAVE_MAJ)
    {
        return NULL;
    }
    
    if(minor >= MAX_PTY_DEVICES || !pty_slaves[minor])
    {
        return NULL;
    }
    
    // slave pty with a closed master pty
    if(pty_slaves[minor]->tty.flags & TTY_FLAG_MASTER_CLOSED)
    {
        return NULL;
    }
    
    return &pty_slaves[minor]->tty;
}


/*
 * Given a pty master device number, return the corresponding slave device
 * number. This is used to implement the ptsname() function.
 */
int pty_slave_index(dev_t dev)
{
    if(dev <= 0)
    {
        return -ENOTTY;
    }

    int minor = MINOR(dev);
    int major = MAJOR(dev);
    
    if(major != PTY_MASTER_MAJ)
    {
        return -ENOTTY;
    }
    
    if(minor >= MAX_PTY_DEVICES || !pty_slaves[minor])
    {
        return -ENOTTY;
    }
    
    // slave pty with a closed master pty
    if(pty_slaves[minor]->tty.flags & TTY_FLAG_MASTER_CLOSED)
    {
        return -ENOTTY;
    }
    
    return pty_slaves[minor]->index;
}


/*
 * Perform a select operation on a master pty device.
 */
long pty_master_select(struct file_t *f, int which)
{
    struct tty_t *tty;
    dev_t dev;

    if(!f || !f->node)
    {
        return 0;
    }
    
	if(!S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];
	
	if(MAJOR(dev) != PTY_MASTER_MAJ)
	{
	    return 0;
	}
	
	if(!(tty = devpts_get_struct_tty(dev)))
	{
	    return 0;
	}

	switch(which)
	{
    	case FREAD:
            if(ttybuf_is_empty(&tty->write_q))
            {
        		//selrecord(&tty->wsel);
        		selrecord(&tty->write_q.sel);
                return 0;
            }
            return 1;

    	case FWRITE:
    	    if(ttybuf_is_full(&tty->read_q))
    	    {
        		//selrecord(&tty->rsel);
        		selrecord(&tty->read_q.sel);
                return 0;
            }
            return 1;
    	    
    	case 0:
    	    // TODO: (we should be handling exceptions)
   			break;
	}

	return 0;
}


/*
 * Perform a poll operation on a master pty device.
 */
long pty_master_poll(struct file_t *f, struct pollfd *pfd)
{
    long res = 0;
    struct tty_t *tty;
    dev_t dev;

    if(!f || !f->node || !S_ISCHR(f->node->mode))
	{
        pfd->revents |= POLLNVAL;
	    return 0;
	}
	
	dev = f->node->blocks[0];

	if(MAJOR(dev) != PTY_MASTER_MAJ)
	{
        pfd->revents |= POLLNVAL;
	    return 0;
	}
	
	if(!(tty = devpts_get_struct_tty(dev)))
	{
        pfd->revents |= POLLERR;
	    return 0;
	}

    if(pfd->events & POLLIN)
    {
        if(ttybuf_is_empty(&tty->write_q))
        {
            //selrecord(&tty->wsel);
            selrecord(&tty->write_q.sel);
        }
        else
        {
            pfd->revents |= POLLIN;
            res = 1;
        }
    }

    if(pfd->events & POLLOUT)
    {
        if(ttybuf_is_full(&tty->read_q))
        {
            //selrecord(&tty->rsel);
            selrecord(&tty->read_q.sel);
        }
        else
        {
            pfd->revents |= POLLOUT;
            res = 1;
        }
    }
    
    return res;
}


/*
 * Create a new master pty device.
 */
long pty_master_create(struct fs_node_t **master)
{
    struct fs_node_t *m;
    struct pty_t **n, **ln = LAST_PTY;
    struct pty_t *slave;
    char *rbuf = NULL, *wbuf = NULL, *sbuf = NULL;

    *master = NULL;
    
    if(!(m = get_empty_node()))
    {
        return -ENOMEM;
    }

    // alloc a new node to represent pseudoterminal's slave device
    if(!(slave = kmalloc(sizeof(struct pty_t))))
    {
        release_node(m);
        return -ENOMEM;
    }
    
    // init the master/slave structs and set appropriate permissions
    // See: https://man7.org/linux/man-pages/man3/grantpt.3.html

    memset(slave, 0, sizeof(struct pty_t));
    slave->uid = this_core->cur_task->uid;
    slave->gid = get_kgroup(KGROUP_TTY);
    slave->mode = S_IFCHR | 0620;   // crw--w----

    // alloc read, write and secondary bufs
    if(!(rbuf = (char *)kmalloc(TTY_BUF_SIZE)) ||
       !(wbuf = (char *)kmalloc(TTY_BUF_SIZE)) ||
       !(sbuf = (char *)kmalloc(TTY_BUF_SIZE)))
    {
        goto fail;
    }

    // init queues
    ttybuf_init(&slave->tty.read_q, rbuf /*, TTY_BUF_SIZE */);
    ttybuf_init(&slave->tty.write_q, wbuf /*, TTY_BUF_SIZE */);
    ttybuf_init(&slave->tty.secondary, sbuf /*, TTY_BUF_SIZE */);

    tty_set_defaults(&slave->tty);
    slave->tty.write = NULL /* dummy_write */;

    // keep the slave pty locked until someone calls unlockpt()
    slave->tty.flags |= TTY_FLAG_LOCKED;
    
    m->uid = slave->uid;
    m->gid = slave->gid;
    m->mode = slave->mode;
    m->refs = 1;
    m->select = pty_master_select;
    m->poll = pty_master_poll;
    
    //m->read = chr_read;
    //m->write = chr_write;
    m->read = ttyx_read /* pty_master_read */;
    m->write = ttyx_write /* pty_master_write */;
    
    kernel_mutex_lock(&pty_lock);
    
    // find an unused slot in the table and store the new slave device node
    // there - we also store the slot's index in the caller's node's private
    // data, so that subsequent calls to unlockpt() and ptsname() know which
    // slave device to use
    for(n = pty_slaves; n < ln; n++)
    {
        if(*n == NULL)
        {
            devpts_root->size++;
            devpts_root->links++;

            *n = slave;

            // store the slave device's index in the private data field of
            // the file node
            slave->index = n - pty_slaves;
            m->blocks[0] = TO_DEVID(PTY_MASTER_MAJ, (n - pty_slaves));
            kernel_mutex_unlock(&pty_lock);
            
            *master = m;
            
            return 0;
        }
    }

    kernel_mutex_unlock(&pty_lock);

fail:
    kfree(rbuf);
    kfree(wbuf);
    kfree(sbuf);
    kfree(slave);
    release_node(m);
    return -ENOMEM;
}


/*
 * Close a master pty device.
 */
void pty_master_close(struct fs_node_t *node)
{
    int minor, major;
    struct pty_t *slave;
    dev_t dev = node->blocks[0];

    minor = MINOR(dev);
    major = MAJOR(dev);
    
    KDEBUG("pty_master_close: maj 0x%x, min 0x%x\n", major, minor);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(major != PTY_MASTER_MAJ)
    {
        return;
    }
    
    kernel_mutex_lock(&pty_lock);
    
    if(minor >= MAX_PTY_DEVICES || !(slave = pty_slaves[minor]))
    {
        kernel_mutex_unlock(&pty_lock);
        return;
    }

    KDEBUG("pty_master_close: maj 0x%x, min 0x%x, refs %d\n", major, minor, slave->refs);

    //if(--slave->mrefs == 0)
    {
        slave->tty.flags |= TTY_FLAG_MASTER_CLOSED;
        
        tty_send_signal(slave->tty.pgid, SIGHUP);
        tty_send_signal(slave->tty.pgid, SIGCONT);
        
        // if no one is using the slave device, remove it
        if(slave->refs == 0)
        {
            devpts_free_inode_internal(&pty_slaves[minor]);
        }
    }

    kernel_mutex_unlock(&pty_lock);
}


/*
 * Close a slave pty device.
 */
void pty_slave_close(struct fs_node_t *node)
{
    int minor, major;
    struct pty_t *slave;
    dev_t dev = node->blocks[0];

    minor = MINOR(dev);
    major = MAJOR(dev);
    
    KDEBUG("pty_master_close: maj 0x%x, min 0x%x\n", major, minor);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    if(major != PTY_SLAVE_MAJ)
    {
        return;
    }

    kernel_mutex_lock(&pty_lock);

    if(minor >= MAX_PTY_DEVICES || !(slave = pty_slaves[minor]))
    {
        kernel_mutex_unlock(&pty_lock);
        return;
    }

    KDEBUG("pty_slave_close: maj 0x%x, min 0x%x, refs %d\n", major, minor, slave->refs);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    slave->refs--;
    //node->links--;

    // if no one is using the slave device, and the master is down, remove 
    // the slave device
    if((slave->refs == 0) && (slave->tty.flags & TTY_FLAG_MASTER_CLOSED))
    {
        devpts_free_inode_internal(&pty_slaves[minor]);
    }

    kernel_mutex_unlock(&pty_lock);
}


/*
 * Open a slave pty device.
 */
long pty_slave_open(struct fs_node_t *node)
{
    int minor, major;
    struct pty_t *slave;
    dev_t dev = node->blocks[0];

    minor = MINOR(dev);
    major = MAJOR(dev);
    
    if(major != PTY_SLAVE_MAJ)
    {
        return -ENOTTY;
    }
    
    kernel_mutex_lock(&pty_lock);

    if(minor >= MAX_PTY_DEVICES || !(slave = pty_slaves[minor]))
    {
        kernel_mutex_unlock(&pty_lock);
        return -ENOTTY;
    }

    if(slave->tty.flags & (TTY_FLAG_MASTER_CLOSED | TTY_FLAG_LOCKED))
    {
        kernel_mutex_unlock(&pty_lock);
        
        /*
         * TODO: is this the right errno to return here?
         */
        return -EBUSY;
    }

    slave->refs++;
    //node->links++;

    kernel_mutex_unlock(&pty_lock);

    return 0;
}


#if 0

/*
 * Read data from a master pseudoterminal device. This is handled separate from
 * the slave device, as the master "reads" from the write queue.
 *
 * Slave pseudoterminal devices are handled by ttyx_read(), much like regular
 * terminal devices.
 *
 * Inputs:
 *    dev => device number of the terminal device to read from.
 *           major should be PTY_MASTER_MAJ (2), while minor should be between 
 *           0 and MAX_PTY_DEVICES - 1, inclusive
 *    buf => input buffer where data is copied
 *    count => number of characters to read
 *
 * Output:
 *    buf => data is copied to this buffer
 *
 * Returns:
 *    number of characters read on success, -errno on failure
 */
ssize_t pty_master_read(struct file_t *f, off_t *pos,
                        unsigned char *buf, size_t _count, int kernel)
{
    UNUSED(pos);

    dev_t dev = f->node->blocks[0];
    struct tty_t *tty;
    unsigned char c, *p = buf;
    volatile size_t count = _count;
    volatile int has_signal = 0;
    struct task_t *ct = cur_task;
    
    /*
    if(!buf)
    {
        return -EINVAL;
    }
    */
    
    if(!(tty = devpts_get_struct_tty(dev)))
    {
        return -EINVAL;
    }

    // check the given user address is valid
    if(!kernel)
    {
        if(valid_addr(ct, (virtual_addr)p, (virtual_addr)p + count - 1) != 0)
        {
            add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)p);
            return -EFAULT;
        }
    }
    
    // read input
    while(count > 0)
    {
        has_signal = ct->woke_by_signal;
        
        // stop if we receive a signal
        if(has_signal)
        {
            break;
        }
        
        // sleep if the write queue is empty
        if(ttybuf_is_empty(&tty->write_q))
        {
            //sleep_if_empty(tty, &tty->write_q);
            sleep_if_empty(tty, &tty->write_q, 0);
            KDEBUG("pty_master_read: woke up - pid %d\n", ct->pid);
            continue;
        }
        
        // get next char
        do
        {
            c = ttybuf_dequeue(&tty->write_q);
            
            // copy next char to user buf
            *p++ = c;
            
            // stop if we got our count
            if(--count == 0)
            {
                break;
            }
            
            // loop until we get our count chars or queue is empty
        } while(count > 0 && !ttybuf_is_empty(&tty->write_q));
    }
    
    // check if we were interrupted by a signal and no chars read
    if(ct->woke_by_signal && (p - buf) == 0)
    {
        KDEBUG("pty_master_read: interrupted\n");
        return -ERESTARTSYS;
    }
    

    // wakeup waiters if we read anything
    if((p - buf))
    {
        // wake up waiting tasks
        unblock_tasks(&tty->write_q);
    
        // wake up select() waiting tasks
        selwakeup(&tty->wsel);
    }

    //KDEBUG("pty_master_read: got %d chars\n", (p - buf));

    // return count of bytes read
    return (ssize_t)(p - buf);
}


/*
 * Write data to a master pseudoterminal device. This is handled separate from
 * the slave device, as the master "writes" to the read queue.
 *
 * Slave pseudoterminal devices are handled by ttyx_write(), much like regular
 * terminal devices.
 *
 * Inputs:
 *    dev => device number of the terminal device to write to.
 *           major should be PTY_MASTER_MAJ (2), while minor should be between 
 *           0 and MAX_PTY_DEVICES - 1, inclusive
 *    buf => output buffer from which data is copied
 *    count => number of characters to write
 *
 * Output:
 *    none
 *
 * Returns:
 *    number of characters written on success, -errno on failure
 */
ssize_t pty_master_write(struct file_t *f, off_t *pos,
                         unsigned char *buf, size_t _count, int kernel)
{
    UNUSED(pos);

    dev_t dev = f->node->blocks[0];
    struct tty_t *tty;
    unsigned char c, *p = buf;
    volatile size_t count = _count;
    volatile int has_signal = 0;
    struct task_t *ct = cur_task;
    
    /*
    if(!buf)
    {
        return -EINVAL;
    }
    */
    
    if(!(tty = devpts_get_struct_tty(dev)))
    {
        return -EINVAL;
    }
    
    // check the given user address is valid
    if(!kernel)
    {
        if(valid_addr(ct, (virtual_addr)p, (virtual_addr)p + count - 1) != 0)
        {
            add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)p);
            return -EFAULT;
        }
    }

    ct->woke_by_signal = 0;

    // write output
    while(count > 0)
    {
        // wait until input buffer has space
        sleep_if_full(&tty->read_q);

        has_signal = ct->woke_by_signal;
        
        // stop if we receive a signal
        if(has_signal)
        {
            break;
        }
        
        // copy data as long as there is space in the input buffer
        while(count > 0 && !ttybuf_is_full(&tty->read_q))
        {
            c = *p;
            
            p++;
            count--;
            ttybuf_enqueue(&tty->read_q, c);
        }
        
        // copy input to secondary buffer, and wakeup any waiting readers
        copy_to_buf(tty);
        
        // if there is still input, it means the queue is full, so sleep until
        // there is space
        if(count > 0)
        {
            lock_scheduler();
            //preempt(&ct->r);
            scheduler();
            unlock_scheduler();
        }
    }
    
    // check if we were interrupted by a signal and no chars written
    if(ct->woke_by_signal && (p - buf) == 0)
    {
        return -ERESTARTSYS;
    }
    
    // return count of bytes written
    return (ssize_t)(p - buf);
}

#endif


/*
 * Helper function that copies info from a devpts node to an incore
 * (memory resident) node.
 */
void devpts_inode_to_incore(struct fs_node_t *n, struct pty_t *i)
{
    /*
     * We lazily use current date & time for all time values.
     *
     * TODO: store correct mtime, ctime and atime values.
     */
    int j;
    time_t t = now();
    
    n->inode = i->index + FIRST_INODE;
    n->mode = i->mode;
    n->uid = i->uid;
    n->gid = i->gid;
    n->atime = t;
    n->mtime = t;
    n->ctime = t;
    n->size = 0;
    //n->links = i->refs;
    n->links = 1;
    
    n->blocks[0] = TO_DEVID(PTY_SLAVE_MAJ, i->index);
    
    for(j = 1; j < 15; j++)
    {
        n->blocks[j] = 0;
    }
}


/*
 * Helper function that copies info from an incore (memory resident) 
 * node to a devpts node.
 */
void devpts_incore_to_inode(struct pty_t *i, struct fs_node_t *n)
{
    //i->inode = n->inode;
    i->mode = n->mode;
    i->uid = n->uid;
    i->gid = n->gid;
    i->refs = n->links;
    
    /*
    i->atime = n->atime;
    i->mtime = n->mtime;
    i->ctime = n->ctime;
    i->size = n->size;
    
    i->dev = n->blocks[0];
    */
}


/*
 * Reads inode data structure.
 */
long devpts_read_inode(struct fs_node_t *node)
{
    ino_t ino;
    int index;
    struct pty_t *slave;
    
    if(!valid_devpts_node(node))
    {
        return -EINVAL;
    }

    KDEBUG("devpts_read_inode: dev 0x%x, n 0x%x\n", node->dev, node->inode);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    ino = node->inode;
    index = ino - FIRST_INODE;
    
    // root node
    if(ino == ROOT_INODE)
    {
        return 0;
    }

    // other dev nodes
    kernel_mutex_lock(&pty_lock);
    
    if(index >= MAX_PTY_DEVICES || !(slave = pty_slaves[index]))
    {
        kernel_mutex_unlock(&pty_lock);
        return -ENOENT;
    }

    devpts_inode_to_incore(node, slave);
    kernel_mutex_unlock(&pty_lock);
    return 0;
}


/*
 * Writes inode data structure.
 */
long devpts_write_inode(struct fs_node_t *node)
{
    ino_t ino;
    int index;
    struct pty_t *slave;

    if(!valid_devpts_node(node))
    {
        return -EINVAL;
    }

    KDEBUG("devpts_write_inode: dev 0x%x, n 0x%x\n", node->dev, node->inode);

    ino = node->inode;
    index = ino - FIRST_INODE;
    
    // root node
    if(ino == ROOT_INODE)
    {
        return 0;
    }

    // other dev nodes
    kernel_mutex_lock(&pty_lock);
    
    if(index >= MAX_PTY_DEVICES || !(slave = pty_slaves[index]))
    {
        kernel_mutex_unlock(&pty_lock);
        //return -ENOENT;
        return 0;
    }

    devpts_incore_to_inode(slave, node);
    kernel_mutex_unlock(&pty_lock);
    return 0;
}


STATIC_INLINE struct dirent *entry_to_dirent(int index, struct pty_t *pty)
{
    // should be enough for device names, which should be '0' to '64', or
    // whatever MAX_PTY_DEVICES is set to.
    int namelen = 4;
    unsigned int reclen = GET_DIRENT_LEN(namelen);
    struct dirent *entry = kmalloc(reclen);

    if(!entry)
    {
        return NULL;
    }

    entry->d_ino = pty->index + FIRST_INODE;
    entry->d_off = index;
    entry->d_type = DT_CHR;
    //sprintf(entry->d_name, "%d", pty->index);
    ksprintf(entry->d_name, 4, "%d", pty->index);
    entry->d_reclen = reclen;
    
    return entry;
}


/*
 * Quick conversion of a pty name to a decimal number, knowing that all names
 * under /dev/pts are decimal numbers between 0 and MAX_PTY_DEVICES - 1.
 *
 * Returns -EINVAL on error; otherwise returns 0 and places the converted
 * number in *res.
 */
int name_to_index(char *name, int *res)
{
    char *p = name;
    int n = 0;
    
    *res = 0;

    while(*p)
    {
        if(*p == '0')
        {
            // accept only '0', but reject things like '07' or '0x7'
            // this is to ensure we only match things like '1' to '1',
            // but not '01' or '001', etc.
            if(p == name && p[1] != '\0')
            {
                return -EINVAL;
            }
            
            n = (n * 10);
        }
        else if(*p >= '1' && *p <= '9')
        {
            n = (n * 10) + (*p - '0');
        }
        else
        {
            return -EINVAL;
        }
        
        p++;
    }
    
    *res = n;
    return 0;
}


static inline int root_dirent(char *filename, struct dirent **entry)
{
    struct pty_t tmp;

    tmp.index = ROOT_INODE - FIRST_INODE;
    tmp.mode = ROOT_MODE;

    // use index 0 for '.', and 1 for '..'
    *entry = entry_to_dirent((filename[1] == '.'), &tmp);
    return 0;
}


/*
 * Find the given filename in the parent directory.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    filename => the searched-for filename
 *
 * Outputs:
 *    entry => if the filename is found, its entry is converted to a kmalloc'd
 *             dirent struct, and the result is stored in this field
 *    dbuf => the disk buffer representing the disk block containing the found
 *            filename, this is useful if the caller wants to delete the file
 *            after finding it (vfs_unlink(), for example)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                file's entry
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long devpts_finddir(struct fs_node_t *dir, char *filename,
                    struct dirent **entry,
                    struct cached_page_t **dbuf, size_t *dbuf_off)
{
    int i;
    
    if(!valid_devpts_node(dir))
    {
        return -EINVAL;
    }

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    KDEBUG("devpts_finddir: name %s\n", filename);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    if(filename[0] == '.')
    {
        if(filename[1] =='\0' ||                            // '.'
           (filename[1] == '.' && filename[2] =='\0'))      // '..'
        {
            return root_dirent(filename, entry);
        }
    }
    
    // knowing that /dev/pts devices all have names consisting of decimal
    // digits, convert the filename to a number, then check the array entry
    // at the given index number
    if(name_to_index(filename, &i) != 0)
    {
        return -ENOENT;
    }
    
    if(i < 0 || i >= MAX_PTY_DEVICES)
    {
        return -ENOENT;
    }

    kernel_mutex_lock(&pty_lock);
    
    if(pty_slaves[i])
    {
        *entry = entry_to_dirent(i + 2, pty_slaves[i]);
        kernel_mutex_unlock(&pty_lock);
        return *entry ? 0 : -ENOMEM;
    }

    kernel_mutex_unlock(&pty_lock);

    return -ENOENT;
}


/*
 * Find the given inode in the parent directory.
 * Called during pathname resolution when constructing the absolute pathname
 * of a given inode.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    node => the searched-for inode
 *
 * Outputs:
 *    entry => if the node is found, its entry is converted to a kmalloc'd
 *             dirent struct, and the result is stored in this field
 *    dbuf => the disk buffer representing the disk block containing the found
 *            filename, this is useful if the caller wants to delete the file
 *            after finding it (vfs_unlink(), for example)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                file's entry
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long devpts_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                             struct dirent **entry,
                             struct cached_page_t **dbuf, size_t *dbuf_off)
{
    int i;

    if(!valid_devpts_node(dir) || !valid_devpts_node(node))
    {
        return -EINVAL;
    }

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;
    
    // devpts root node
    if(node->inode == ROOT_INODE)
    {
        return root_dirent(".", entry);
    }
    
    // device nodes
    i = node->inode - FIRST_INODE;

    if(i < 0 || i >= MAX_PTY_DEVICES)
    {
        return -ENOENT;
    }

    kernel_mutex_lock(&pty_lock);
    
    if(pty_slaves[i])
    {
        *entry = entry_to_dirent(i + 2, pty_slaves[i]);
        kernel_mutex_unlock(&pty_lock);
        return *entry ? 0 : -ENOMEM;
        //return 0;
    }

    kernel_mutex_unlock(&pty_lock);

    return -ENOENT;
}


/*
 * Get dir entries.
 *
 * Inputs:
 *     dir => node of dir to read from
 *     pos => byte position to start reading entries from
 *     dp => buffer in which to store dir entries
 *     count => max number of bytes to read (i.e. size of dp)
 *
 * Returns:
 *     number of bytes read on success, -errno on failure
 */
long devpts_getdents(struct fs_node_t *dir, off_t *pos, void *buf, int bufsz)
{
    size_t offset, count = 0;
    size_t dirsz = MAX_PTY_DEVICES + 2;
    size_t reclen;
    static size_t namelen = 4;
    struct dirent *dent;
    char *b = (char *)buf;
    char name[4];
    struct pty_t tmp, *pty = NULL;
    
    UNUSED(dir);

    /*
     * Offsets in the /dev/pts directory refer to the following entries:
     *     Offset 0 => '.'
     *     Offset 1 => '..'
     *     Offset 2 => first dev entry, i.e. pty_slaves[0]
     *     Offset 2+n => pty_slaves[n]
     */

    offset = *pos;
    
    while(offset < dirsz)
    {
        if(offset == 0)             // '.'
        {
            tmp.index = ROOT_INODE - FIRST_INODE;
            tmp.mode = ROOT_MODE;
            strcpy(name, ".");
            pty = &tmp;
        }
        else if(offset == 1)        // '..'
        {
            tmp.index = ROOT_INODE - FIRST_INODE;
            tmp.mode = ROOT_MODE;
            strcpy(name, "..");
            pty = &tmp;
        }
        else
        {
            pty = pty_slaves[offset - 2];

            if(!pty)
            {
                offset++;
                continue;
            }

            //sprintf(name, "%d", pty->index);
            ksprintf(name, sizeof(name), "%d", pty->index);
        }

        // calc dirent record length
        reclen = sizeof(struct dirent) + namelen + 1;

        // make it 4-byte aligned
        if(reclen & 3)
        {
            reclen &= ~3;
            reclen += 4;
        }
        
        // check the buffer has enough space for this entry
        if((count + reclen) > (size_t)bufsz)
        {
            KDEBUG("devpts_getdents: count 0x%x (1)\n", count);
            break;
        }
        
        dent = (struct dirent *)b;

        dent->d_ino = pty->index + FIRST_INODE;
        dent->d_off = offset;
        dent->d_type = DT_CHR;
        strcpy(dent->d_name, name);

        dent->d_reclen = reclen;
        b += reclen;
        count += reclen;
        offset++;
    }
    
    *pos = offset;

    return count;
}

