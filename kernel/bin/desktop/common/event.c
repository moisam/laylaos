/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: event.c
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
 *  \file event.c
 *
 *  This file contains the core function that retrieves events from the server
 *  to pass to client applications.
 */

#include <errno.h>
#include <unistd.h>
#include <string.h>
#ifndef _POSIX_THREADS
#define _POSIX_THREADS          1
#endif
#include <sched.h>
#include <sys/select.h>
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/screen.h"

#define GLOB                    __global_gui_data

#define ASSERT_NOERR(res)           \
    if(res < 0)                     \
    {                               \
        evbuf->type = EVENT_ERROR;  \
        evbuf->err._errno = errno;  \
        return 0;                   \
    }


ssize_t __get_event(int evfd, volatile struct event_t *evbuf, 
                    size_t bufsz, int wait)
{
    volatile ssize_t res;
    fd_set rdfs;

    if(!wait)
    {
        static struct timeval zero_time = { 0, };

        FD_ZERO(&rdfs);
        FD_SET(evfd, &rdfs);

        if(select(evfd + 1, &rdfs, NULL, NULL, &zero_time) <= 0)
        {
            evbuf->type = EVENT_ERROR;
            evbuf->err._errno = ETIMEDOUT;
            return 0;
        }

        //res = read(evfd, (void *)evbuf, bufsz);
        res = direct_read(evfd, (void *)evbuf, bufsz);
        ASSERT_NOERR(res);

        if(res > 0)
        {
            return res;
        }
    
        evbuf->type = 0;
        evbuf->valid_reply = 0;
        return 0;
    }

    while(1)
    {
        FD_ZERO(&rdfs);
        FD_SET(evfd, &rdfs);
        
        res = select(evfd + 1, &rdfs, NULL, NULL, NULL);
        ASSERT_NOERR(res);

        if(res > 0)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            //res = read(evfd, (void *)evbuf, bufsz);
            res = direct_read(evfd, (void *)evbuf, bufsz);
            ASSERT_NOERR(res);

            if(res > 0)
            {
                return res;
            }
        }
    }
}


void notify_win_title_event(int fd, char *title, winid_t dest, winid_t src)
{
    size_t titlelen = title ? (strlen(title) + 1) : 1;
    size_t bufsz = sizeof(struct event_buf_t) + titlelen;
    /* volatile */ struct event_buf_t *evbuf = malloc(bufsz + 1);

    if(!evbuf)
    {
        return;
    }
    
    if(title)
    {
        memcpy((void *)evbuf->buf, title, titlelen);
    }
    else
    {
        evbuf->buf[0] = '\0';
    }

    evbuf->type = EVENT_CHILD_WINDOW_TITLE_SET;
    evbuf->bufsz = titlelen;
    evbuf->src = src;
    evbuf->dest = dest;
    evbuf->valid_reply = 1;
    //write(fd, (void *)evbuf, bufsz);
    direct_write(fd, (void *)evbuf, bufsz);


    free((void *)evbuf);
}

