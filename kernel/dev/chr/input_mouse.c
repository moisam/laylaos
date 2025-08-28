/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: input_mouse.c
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
 *  \file input_mouse.c
 *
 *  Read and write functions for the character mouse device (major = 13,
 *  minor = 32).
 */

#define __USE_XOPEN_EXTENDED
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <kernel/laylaos.h>
#include <kernel/select.h>
#include <kernel/mouse.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/fcntl.h>

extern struct selinfo mouse_ssel;

// NOTE: ensure this is a multiple of 2!
#define NR_PACKETS          2048

struct mouse_packet_t incoming_mouse_packets[NR_PACKETS];

volatile int incoming_first_packet = 0, incoming_cur_packet = 0;


/*
 * Add a new mouse packet.
 */
void add_mouse_packet(int dx, int dy, mouse_buttons_t buttons)
{
    incoming_mouse_packets[incoming_cur_packet].dx = dx;
    incoming_mouse_packets[incoming_cur_packet].dy = dy;
    incoming_mouse_packets[incoming_cur_packet].buttons = buttons;
    //incoming_cur_packet = (incoming_cur_packet + 1) % NR_PACKETS;
    incoming_cur_packet = (incoming_cur_packet + 1) & (NR_PACKETS - 1);

    if(incoming_cur_packet == incoming_first_packet)
    {
        //incoming_first_packet = (incoming_first_packet + 1) % NR_PACKETS;
        incoming_first_packet = (incoming_first_packet + 1) & (NR_PACKETS - 1);
    }
}


/*
 * Check for mouse packets.
 */
void mouse_task_func(void *arg)
{
    UNUSED(arg);

    for(;;)
    {
        if(incoming_cur_packet != incoming_first_packet)
        {
            selwakeup(&mouse_ssel);
        }

        //block_task(incoming_mouse_packets, 0);
        block_task2(incoming_mouse_packets, PIT_FREQUENCY);
    }
}


/*
 * Read from char device /dev/mouse0.
 */
ssize_t mousedev_read(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);

    /* volatile */ struct mouse_packet_t *packet = (struct mouse_packet_t *)buf;

    if(count < sizeof(struct mouse_packet_t) || !buf)
    {
        return -EINVAL;
    }

    while(1)
    {
        if(incoming_cur_packet != incoming_first_packet)
        {
            COPY_VAL_TO_USER(&packet->dx,
                      &incoming_mouse_packets[incoming_first_packet].dx);
            COPY_VAL_TO_USER(&packet->dy,
                      &incoming_mouse_packets[incoming_first_packet].dy);
            COPY_VAL_TO_USER(&packet->buttons,
                      &incoming_mouse_packets[incoming_first_packet].buttons);

            //incoming_first_packet = (incoming_first_packet + 1) % NR_PACKETS;
            incoming_first_packet = (incoming_first_packet + 1) & (NR_PACKETS - 1);
            
            return sizeof(struct mouse_packet_t);
        }
    }
}


/*
 * Perform a select operation on /dev/mouse0.
 */
int mousedev_select(dev_t dev, int which)
{
    UNUSED(dev);

	switch(which)
	{
    	case FREAD:
            if(incoming_cur_packet != incoming_first_packet)
            {
                return 1;
            }

            selrecord(&mouse_ssel);
            return 0;

    	case FWRITE:
            /*
             * TODO: we don't support writing for now
             */
   			break;
    	    
    	case 0:
   			break;
	}

	return 0;
}


/*
 * Perform a poll operation on /dev/mouse0.
 */
int mousedev_poll(dev_t dev, struct pollfd *pfd)
{
    UNUSED(dev);
    int res = 0;

    if(pfd->events & POLLIN)
    {
        if(incoming_cur_packet == incoming_first_packet)
        {
            selrecord(&mouse_ssel);
        }
        else
        {
            pfd->revents |= POLLIN;
            res = 1;
        }
    }

    // TODO: we don't support writing for now (that is, no POLLOUT events)
    
    return res;
}

