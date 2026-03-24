/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: siginfo-nopad-def.h
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
 *  \file siginfo-nopad-def.h
 *
 *  Redifinition of the siginfo_t structure without padding. The struct is 
 *  taken from musl's signal.h but with the padding taken out and with field
 *  names changed slightly to avoid clashing with signal.h. This reduces
 *  the struct's size from 128 bytes to 48 bytes. In turn, this reduces our
 *  struct task_t's size from 10408 bytes to 5200 bytes only.
 */

#ifndef __SIGINFO_NO_PAD_DEFINED__
#define __SIGINFO_NO_PAD_DEFINED__

typedef struct {
	int sinp_signo, sinp_code, sinp_errno;
	union {
		struct {
			union {
				struct {
					pid_t pid;
					uid_t uid;
				} __piduid;
				struct {
					int timerid;
					int overrun;
				} __timer;
			} __first;
			union {
				union sigval value;
				struct {
					int status;
					clock_t utime, stime;
				} __sigchld;
			} __second;
		} sinp_common;

		struct {
			void *addr;
			short addr_lsb;
			union {
				struct {
					void *lower;
					void *upper;
				} __addr_bnd;
				unsigned pkey;
			} __first;
		} sinp_sigfault;

		struct {
			long band;
			int fd;
		} sinp_sigpoll;

		struct {
			void *call_addr;
			int syscall;
			unsigned arch;
		} sinp_sigsys;
	} sinp_fields;
} siginfo_nopad_t;

#define sinp_pid     sinp_fields.sinp_common.__first.__piduid.pid
#define sinp_uid     sinp_fields.sinp_common.__first.__piduid.uid
#define sinp_status  sinp_fields.sinp_common.__second.__sigchld.status
#define sinp_utime   sinp_fields.sinp_common.__second.__sigchld.utime
#define sinp_stime   sinp_fields.sinp_common.__second.__sigchld.stime
#define sinp_value   sinp_fields.sinp_common.__second.value
#define sinp_addr    sinp_fields.sinp_sigfault.addr

#endif      /* __SIGINFO_NO_PAD_DEFINED__ */
