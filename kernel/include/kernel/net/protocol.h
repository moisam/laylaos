/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: protocol.h
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
 *  \file protocol.h
 *
 *  Functions and macros for working with network protocols.
 */

#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

/**
 * @struct proto_t
 * @brief The proto_t structure.
 *
 * A structure to represent a network protocol.
 */
struct proto_t
{
    int sock_type;              /**< socket type */
    int protocol;               /**< protocol type */
    struct domain_t *domain;    /**< domain pointer */
    struct sockops_t *sockops;  /**< pointer to socket operations struct */
};


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief Initialize network protocols.
 *
 * Initialize the protocol layer and call each registered protocol's init 
 * function.
 *
 * @return  nothing.
 */
void network_init(void);

/**
 * @brief Get protocol.
 *
 * Find a protocol given its family and protocol type.
 *
 * @param   family      protocol family
 * @param   type        protocol type
 *
 * @return  pointer to protocol on success, NULL on failure.
 */
struct proto_t *find_proto_by_type(int family, int type);

/**
 * @brief Get protocol.
 *
 * Find a protocol given its family, protocol id and/or type.
 *
 * @param   family      protocol family
 * @param   protocol    protocol identifier
 * @param   type        protocol type
 *
 * @return  pointer to protocol on success, NULL on failure.
 */
struct proto_t *find_proto(int family, int protocol, int type);

#endif      /* NET_PROTOCOL_H */
