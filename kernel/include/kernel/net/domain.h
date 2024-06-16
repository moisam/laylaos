/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: domain.h
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
 *  \file domain.h
 *
 *  Functions and macros for handling network domains.
 */

#ifndef NET_DOMAIN_H
#define NET_DOMAIN_H

/**
 * @struct domain_t
 * @brief The domain_t structure.
 *
 * A structure to represent a network domain.
 */
struct domain_t
{
    int family;             /**< domain family */
    char *name;             /**< domain name */
    struct proto_t *proto;  /**< pointer to first protocol */
    struct proto_t *lproto; /**< pointer to last protocol */
};


/**
 * @var domains
 * @brief Network domains.
 *
 * Master table of registered network domains.
 */
extern struct domain_t *domains[];

/**
 * @var unix_domain
 * @brief Unix domain.
 *
 * Table of Unix domain protocols.
 */
extern struct domain_t unix_domain;

/**
 * @var inet_domain
 * @brief Internet domain IPv4.
 *
 * Table of Internet domain protocols for IP version 4.
 */
extern struct domain_t inet_domain;

/**
 * @var inet6_domain
 * @brief Internet domain IPv6.
 *
 * Table of Internet domain protocols for IP version 6.
 */
extern struct domain_t inet6_domain;

#endif      /* NET_DOMAIN_H */
