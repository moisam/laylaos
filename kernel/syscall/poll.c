/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: poll.c
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
 *  \file poll.c
 *
 *  The kernel's I/O polling implementation.
 */

#include <errno.h>
#include <poll.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/ksignal.h>


int pollwait;


static int pollscan(struct pollfd *fds, nfds_t nfds)
{
    int count = 0, fd;
    nfds_t i;
    struct file_t *f;
    
    if(!fds)
    {
        return 0;
    }

    for(i = 0; i < nfds; i++)
    {
        fd = fds[i].fd;
        
        if(fd < 0)
        {
            continue;
        }
        
        f = this_core->cur_task->ofiles->ofile[fd];

        if(f == NULL || !f->node || !f->node->poll)
        {
            fds[i].revents |= POLLNVAL;
            continue;
        }

        if(f->node->poll(f, &fds[i]))
        {
            count++;
        }
    }
    
    return count;
}


static int poll_internal(struct pollfd *fds, nfds_t nfds,
                         struct timespec *tmo_p)
{
    int error = 0;
    unsigned long timo;
    unsigned long long oticks;
    nfds_t i;
    struct pollfd fdcopy[nfds];
    
    //printk("poll_internal: fds 0x%lx, nfds %d, tm 0x%lx, mask 0x%lx\n", fds, nfds, tmo_p, sigmask);

    if(fds && nfds)
    {
        COPY_FROM_USER(fdcopy, fds, sizeof(struct pollfd) * nfds);
    }

    //printk("poll_internal: 1\n");
    
    for(i = 0; i < nfds; i++)
    {
        fdcopy[i].revents = 0;
    }

    oticks = ticks;

    if(tmo_p)
    {
        timo = timespec_to_ticks(tmo_p);

        /*
         * if the timeout is less than 1 ticks (because the caller specified
         * timeout in usecs that are less than the clock resolution), sleep
         * for 1 tick.
         */
        if(timo == 0 && tmo_p->tv_nsec)
        {
            timo = 1;
        }
    }
    else
    {
        timo = 0;
    }

retry:

    error = pollscan(fdcopy, nfds);

    /*
     * Negative result is error, positive result is fd count.
     * Either way, wrap up and return.
     */
    if(error)
    {
        goto done;
    }

    /*
     * If both fields of the timeval structure are zero, return immediately.
     * If timeout is NULL (no timeout), poll() can block indefinitely.
     */
    if(tmo_p)
    {
        if(tmo_p->tv_sec == 0 && tmo_p->tv_nsec == 0)
        {
            goto done;
        }
        
        if(ticks >= (oticks + timo))
        {
            goto done;
        }
        
        timo -= (ticks - oticks);
    }

    error = block_task2(&pollwait, timo);

    if(error == 0)
    {
        goto retry;
    }
    
    error = -error;
    
done:

    /* poll is not restarted after signals... */
    if(error == -EWOULDBLOCK)
    {
        error = 0;
    }
    else if(error >= 0 && fds && nfds)
    {
        COPY_TO_USER(fds, fdcopy, sizeof(struct pollfd) * nfds);
    }

    return error;
}


/*
 * Handler for syscall poll().
 */
int syscall_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    struct timespec tmp, *ts = NULL;
    
    /*
     * Specifying a negative value in timeout means an infinite timeout.
     * Specifying a timeout of zero causes poll() to return immediately,
     * even if no file descriptors are ready.
     */
    if(timeout >= 0)
    {
        tmp.tv_sec = 0;
        tmp.tv_nsec = timeout * NSEC_PER_MSEC;

        while(tmp.tv_nsec > NSEC_PER_SEC)
        {
            tmp.tv_nsec -= NSEC_PER_SEC;
            tmp.tv_sec++;
        }

        ts = &tmp;
    }

    return poll_internal(fds, nfds, ts);
}


/*
 * Handler for syscall ppoll().
 */
int syscall_ppoll(struct pollfd *fds, nfds_t nfds,
                  struct timespec *tmo_p, sigset_t *sigmask)
{
    sigset_t newsigmask, origmask;
    struct timespec tmp, *ts = NULL;
    int res;
    
    if(tmo_p)
    {
        COPY_FROM_USER(&tmp, tmo_p, sizeof(struct timespec));
        ts = &tmp;
    }

    if(sigmask)
    {
        COPY_FROM_USER(&newsigmask, sigmask, sizeof(sigset_t));
        syscall_sigprocmask_internal((struct task_t *)this_core->cur_task, 
                                     SIG_SETMASK, &newsigmask, &origmask, 1);
    }

    res = poll_internal(fds, nfds, ts);

    if(sigmask)
    {
        syscall_sigprocmask_internal((struct task_t *)this_core->cur_task,
                                     SIG_SETMASK, &origmask, NULL, 1);
    }

    return res;
}

