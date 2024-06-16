/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: net.h
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
 *  \file net.h
 *
 *  Network soft interrupts functions.
 */

#ifndef __KERNEL_NET_H__
#define __KERNEL_NET_H__

#include <endian.h>

#ifdef __GNUC__
#define	__bswap16(_x)	__builtin_bswap16(_x)
#define	__bswap32(_x)	__builtin_bswap32(_x)
#define	__bswap64(_x)	__builtin_bswap64(_x)
#else /* __GNUC__ */
static inline uint16_t __bswap16(uint16_t _x)
{
	return ((uint16_t)((_x >> 8) | ((_x << 8) & 0xff00)));
}

static inline uint32_t __bswap32(uint32_t _x)
{
	return ((uint32_t)((_x >> 24) | ((_x >> 8) & 0xff00) |
	    ((_x << 8) & 0xff0000) | ((_x << 24) & 0xff000000)));
}

static inline uint64_t __bswap64(uint64_t _x)
{
	return ((uint64_t)((_x >> 56) | ((_x >> 40) & 0xff00) |
	    ((_x >> 24) & 0xff0000) | ((_x >> 8) & 0xff000000) |
	    ((_x << 8) & ((uint64_t)0xff << 32)) |
	    ((_x << 24) & ((uint64_t)0xff << 40)) |
	    ((_x << 40) & ((uint64_t)0xff << 48)) | ((_x << 56))));
}
#endif /* !__GNUC__ */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define	htonl(_x)	__bswap32(_x)
#define	htons(_x)	__bswap16(_x)
#define	ntohl(_x)	__bswap32(_x)
#define	ntohs(_x)	__bswap16(_x)
#else
#define	htonl(_x)	((uint32_t)(_x))
#define	htons(_x)	((uint16_t)(_x))
#define	ntohl(_x)	((uint32_t)(_x))
#define	ntohs(_x)	((uint16_t)(_x))
#endif

#endif      /* __KERNEL_NET_H__ */
