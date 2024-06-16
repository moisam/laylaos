/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: notify.h
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
 *  \file notify.h
 *
 *  Internal functions used by the ICMP layer to notify of different errors.
 */

#ifndef NET_NOTIFY_H
#define NET_NOTIFY_H

void notify_socket_unreachable(struct packet_t *p, int is_ipv6);
void notify_proto_unreachable(struct packet_t *p, int is_ipv6);
void notify_dest_unreachable(struct packet_t *p, int is_ipv6);
void notify_ttl_expired(struct packet_t *p, int is_ipv6);
void notify_packet_too_big(struct packet_t *p, int is_ipv6);

#endif      /* NET_NOTIFY_H */
