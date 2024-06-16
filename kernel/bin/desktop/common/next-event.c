/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: next-event.c
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
 *  \file next-event.c
 *
 *  Functions to poll events from the server to present to user programs.
 */

#include <errno.h>
#include <stdlib.h>
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/mutex.h"

#define GLOB                __global_gui_data
#define EVENT_ANY           (0)

/* static */ mutex_t __global_evlock;


static void poll_events(void)
{
    struct queued_ev_t *qe;
    ssize_t bytes;

    //mutex_lock(&__global_evlock);

    while((bytes = __get_event(GLOB.serverfd, GLOB.evbuf_internal, 
                                                    GLOB.evbufsz, 0)) > 0)
    {
        // add to queue for later processing
        if((qe = malloc(sizeof(struct queued_ev_t))))
        {
            if((qe->data = malloc(bytes)))
            {
                A_memcpy(qe->data, GLOB.evbuf_internal, bytes);
                qe->next = NULL;

                if(GLOB.first_queued_ev)
                {
                    qe->prev = GLOB.last_queued_ev;
                    GLOB.last_queued_ev->next = qe;
                    GLOB.last_queued_ev = qe;
                }
                else
                {
                    qe->prev = NULL;
                    GLOB.first_queued_ev = qe;
                    GLOB.last_queued_ev = qe;
                }
            }
            else
            {
                // drop the event - TODO: fix this?
                free(qe);
            }
        }
    }

    //mutex_unlock(&__global_evlock);
}


#define IS_INTERNAL_EVENT(type)                 \
    ((type) == EVENT_WINDOW_CREATED ||          \
     (type) == EVENT_MENU_FRAME_CREATED ||      \
     (type) == EVENT_DIALOG_CREATED ||          \
     (type) == EVENT_RESOURCE_LOADED ||         \
     (type) == EVENT_MOUSE_GRABBED ||           \
     (type) == EVENT_KEYBOARD_GRABBED ||        \
     (type) == EVENT_SCREEN_INFO ||             \
     (type) == EVENT_COLOR_PALETTE_DATA ||      \
     (type) == EVENT_CURSOR_LOADED ||           \
     (type) == EVENT_CURSOR_INFO ||             \
     (type) == EVENT_CLIPBOARD_HAS_DATA ||      \
     (type) == EVENT_CLIPBOARD_DATA ||          \
     (type) == EVENT_CLIPBOARD_SET ||           \
     (type) == EVENT_WINDOW_ATTRIBS ||          \
     (type) == EVENT_WINDOW_RESIZE_CONFIRM ||   \
     (type) == EVENT_WINDOW_NEW_CANVAS ||       \
     (type) == EVENT_MODIFIER_KEYS ||           \
     (type) == EVENT_KEYS_STATE)

struct event_t *next_event_for_seqid(struct window_t *window,
                                     uint32_t seqid, int wait)
{
    volatile struct queued_ev_t *qe;
    struct event_t *ev;

    mutex_lock(&__global_evlock);

    while(1)
    {
        // make sure we get anything that is pending
        poll_events();

        // nothing queued
        if(!GLOB.first_queued_ev)
        {
            mutex_unlock(&__global_evlock);

            if(!wait)
            {
                return NULL;
            }

            // wait until there is some input
            // don't wait forever, as another thread might have polled
            // the events before we come back
            fd_set rdfs;
            FD_ZERO(&rdfs);
            FD_SET(GLOB.serverfd, &rdfs);
            struct timeval tv = { .tv_sec = 2, .tv_usec = 0, };
            select(GLOB.serverfd + 1, &rdfs, NULL, NULL, &tv);
            //sched_yield();

            // retry
            mutex_lock(&__global_evlock);
            continue;
        }

        qe = GLOB.first_queued_ev;

        while(qe)
        {
            ev = (struct event_t *)qe->data;

            // check we have the right event
            if((window && window->winid != ev->dest) ||
               (seqid && seqid != ev->seqid))
            {
                qe = qe->next;
                continue;
            }

            /*
             * seqid is non-zero when we are called by an internal function,
             * e.g. create_window(). Applications and toolkits (e.g. SDL and
             * Qt) have their own event polling functions, which can steal
             * some events from our internal functions. To avoid this, we do
             * not return the "internal" events to these toolkits so that our
             * library functions can work correctly.
             */
            if(seqid == EVENT_ANY && IS_INTERNAL_EVENT(ev->type))
            {
                qe = qe->next;
                continue;
            }

            if(qe->next)
            {
                qe->next->prev = qe->prev;
            }

            if(qe->prev)
            {
                qe->prev->next = qe->next;
            }

            if(qe == GLOB.first_queued_ev)
            {
                GLOB.first_queued_ev = qe->next;

                if(!GLOB.first_queued_ev)
                {
                    GLOB.last_queued_ev = NULL;
                }
            }
            else if(qe == GLOB.last_queued_ev)
            {
                GLOB.last_queued_ev = qe->prev;
            }

            /*
             * A request (from client) can have 2 types of error reply (from 
             * the server):
             *    - If __get_event() fails to read the next event, the event
             *      type is set to EVENT_ERROR and errno is set for us.
             *    - Internal server error, in which case the event type is not
             *      set to EVENT_ERROR and errno is not set appropriately.
             *      In this case, we need to set event type to EVENT_ERROR
             *      and set errno accordingly. This way, the caller need only
             *      check to see if event type equals EVENT_ERROR and act
             *      accordingly.
             */

            if(!ev->valid_reply)
            {
                ev->type = EVENT_ERROR;
                // the err._errno field should be set by the server
                __set_errno(ev->err._errno);
            }

            free((void *)qe);
            mutex_unlock(&__global_evlock);

            return ev;
        }

        if(!wait)
        {
            // If we reach here with events on the queue, it means they are
            // all internal events. If we are not called by a library 
            // function and are not blocking, return null. Internal library
            // functions call us through get_server_reply(), which blocks
            // by default.
            mutex_unlock(&__global_evlock);
            return NULL;
        }
    }
}


/*
 * This function retrieves the next event of any type.
 */
struct event_t *next_event(void)
{
    return next_event_for_seqid(NULL, EVENT_ANY, 1);
}


struct event_t *get_server_reply(uint32_t seqid)
{
    return next_event_for_seqid(NULL, seqid, 1);
}


static inline int have_non_internal_events(void)
{
    volatile struct queued_ev_t *qe;

    mutex_lock(&__global_evlock);

    qe = GLOB.first_queued_ev;

    while(qe)
    {
        if(!IS_INTERNAL_EVENT(((struct event_t *)qe->data)->type))
        {
            mutex_unlock(&__global_evlock);
            return 1;
        }

        qe = qe->next;
    }

    mutex_unlock(&__global_evlock);
    return 0;
}


static inline int do_check_pending_events_timeout(struct timeval *tvp)
{
    fd_set fdset;

    FD_ZERO(&fdset);
    FD_SET(GLOB.serverfd, &fdset);

    if(select(GLOB.serverfd + 1, &fdset, NULL, NULL, tvp) > 0)
    {
        // make sure we get anything that is pending
        mutex_lock(&__global_evlock);
        poll_events();
        mutex_unlock(&__global_evlock);

        return have_non_internal_events();
    }
    
    return 0;
}


/*
 * This function checks if there are any pending events with timeout in secs.
 */
int pending_events_timeout(time_t secs)
{
    struct timeval tv = { .tv_sec = secs, .tv_usec = 0, };

    // first check to see if there are queued events
    if(have_non_internal_events())
    //if(GLOB.first_queued_ev)
    {
        return 1;
    }
    
    // nothing in the queue, check if there are fresh new events
    return do_check_pending_events_timeout(&tv);
}

/*
 * This function checks if there are any pending events with timeout in microsecs.
 */
int pending_events_utimeout(suseconds_t usecs)
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = usecs, };

    while(tv.tv_usec >= 1000000)
    {
        tv.tv_sec++;
        tv.tv_usec -= 1000000;
    }

    // first check to see if there are queued events
    if(have_non_internal_events())
    //if(GLOB.first_queued_ev)
    {
        return 1;
    }
    
    // nothing in the queue, check if there are fresh new events
    return do_check_pending_events_timeout(&tv);
}

/*
 * This function checks if there are any pending events with no timeout.
 */
int pending_events(void)
{
    return pending_events_timeout(0);
}

