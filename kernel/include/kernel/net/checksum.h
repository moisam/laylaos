/* 
 *    Copyright 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: checksum.h
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
 *  \file checksum.h
 *
 *  Functions to calculate checksums for the network layer.
 */

#ifndef NET_CHECKSUM_H
#define NET_CHECKSUM_H

uint16_t checksum(void *buf, uint32_t len);

uint16_t tcp_checksum_ipv4(struct packet_t *p);
uint16_t tcp_checksum_ipv6(struct packet_t *p);

uint16_t udp_checksum_ipv4(struct packet_t *p);
uint16_t udp_checksum_ipv6(struct packet_t *p);

uint16_t icmp4_checksum(struct packet_t *p);
uint16_t icmp6_checksum(struct packet_t *p);

#endif      /* NET_CHECKSUM_H */
