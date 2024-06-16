/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: domain.c
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
 *  \file domain.c
 *
 *  Network domain tables.
 */

#include <stddef.h>
//#include <sys/systm.h>
#include <sys/socket.h>
#include <kernel/net/domain.h>
#include <kernel/net/protocol.h>


extern struct proto_t unix_proto[3];
extern struct proto_t inet_proto[5];
extern struct proto_t inet6_proto[5];


struct domain_t *domains[] =
{
    &unix_domain,
    &inet_domain,
    &inet6_domain,
    NULL,
};

#define ARRSZ(a)    sizeof((a)) / sizeof((a)[0])

struct domain_t unix_domain =
{
    AF_UNIX, "unix", unix_proto, &unix_proto[ARRSZ(unix_proto)]
};

struct domain_t inet_domain =
{
    AF_INET, "internet", inet_proto, &inet_proto[ARRSZ(inet_proto)]
};

struct domain_t inet6_domain =
{
    AF_INET6, "internet", inet6_proto, &inet6_proto[ARRSZ(inet6_proto)]
};

