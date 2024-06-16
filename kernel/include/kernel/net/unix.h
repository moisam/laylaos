/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: unix.h
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
 *  \file unix.h
 *
 *  Functions and macros for handling Unix packets.
 */

#ifndef NET_UNIX_H
#define NET_UNIX_H

// externs defined in unix.c
extern struct netif_queue_t unix_inq;
extern struct netif_queue_t unix_outq;
extern struct sockops_t unix_sockops;


/*********************************************
 * Functions defined in network/unix.c
 *********************************************/

int unix_push(struct packet_t *p);

int socket_unix_bind(struct socket_t *so, 
                     struct sockaddr *name, socklen_t namelen);

int socket_unix_connect(struct socket_t *so, 
                        struct sockaddr *name, socklen_t namelen);

#endif      /* NET_UNIX_H */
