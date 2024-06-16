/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: strace.h
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
 *  \file strace.h
 *
 *  Macros and function declarations for the strace utility.
 */

#include <stdint.h>
#include <sys/types.h>      // pid_t
#include <stdio.h>

extern int arr_limit;
extern int string_limit;

struct stracee_t
{
    pid_t pid;
    int prev_syscall;
    FILE *log;
};

#define ERR_EXIT(fmt, ...)                  \
do {                                        \
    fprintf(stderr, fmt, __VA_ARGS__);      \
    exit(EXIT_FAILURE);                     \
} while(0)


/*
 * Definitions used in processing -e quiet, --quiet, --silent and --silence options.
 */

#define QUIET_ATTACH                0
#define QUIET_EXIT                  1

/*
 * Define the array in only one file.
 */
#ifdef DEFINE_QUIET_MASK

char quiet_mask[] =
{
    [QUIET_ATTACH] = 0,
    [QUIET_EXIT  ] = 0,
};

#else   /* !DEFINE_QUIET_MASK */

extern char quiet_mask[];

#endif  /* DEFINE_QUIET_MASK */


/*
 * Definitions used in processing -e decode-fds, --decode-fds options.
 */

#define DECODE_FDS_PATH             0

/*
 * Define the array in only one file.
 */
#ifdef DEFINE_DECODE_FDS_MASK

char decode_fds_mask[] =
{
    [DECODE_FDS_PATH] = 0,
};

#else   /* !DEFINE_DECODE_FDS_MASK */

extern char decode_fds_mask[];

#endif  /* DEFINE_DECODE_FDS_MASK */


void print_arg_i(struct stracee_t *tracee, size_t i);
void print_arg_ui(struct stracee_t *tracee, size_t i);
void print_arg_fd(struct stracee_t *tracee, int i);
void print_arg_dirfd(struct stracee_t *tracee, int i);
void print_arg_dev(struct stracee_t *tracee, uintptr_t d);
void print_arg_sig(struct stracee_t *tracee, int sig);
void print_arg_resource(struct stracee_t *tracee, int which);
void print_arg_ptr(struct stracee_t *tracee, uintptr_t ptr);
uintptr_t tracee_get_ptr(struct stracee_t *tracee, uintptr_t addr);
void print_arg_strarr(struct stracee_t *tracee, uintptr_t arr);
void print_arg_fds(struct stracee_t *tracee,
                                    uintptr_t ptr, size_t fdcount);
void print_arg_utimbuf(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_timeval(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_tms(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_timespec(struct stracee_t *tracee, uintptr_t ptr);
void print_sigset(struct stracee_t *tracee, sigset_t *set);
void print_arg_sigset(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_sigaction(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_rlimit(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_rusage(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_itimerval(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_itimerspec(struct stracee_t *tracee, uintptr_t ptr);
void print_sched_param(struct stracee_t *tracee, uintptr_t ptr);
void print_stack_t(struct stracee_t *tracee, uintptr_t ptr);
void print_sigevent(struct stracee_t *tracee, uintptr_t ptr);
void print_utsname(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_sysinfo(struct stracee_t *tracee, uintptr_t ptr);
void print_arg_buf(struct stracee_t *tracee, uintptr_t buf, size_t count);
void print_arg_str(struct stracee_t *tracee, uintptr_t buf);
void print_argmode(struct stracee_t *tracee, size_t mode);
void print_arg_accmode(struct stracee_t *tracee, size_t mode);
void print_open_flags(struct stracee_t *tracee, uintptr_t flags);
void print_at_flags(struct stracee_t *tracee, uintptr_t flags);
void print_wait_flags(struct stracee_t *tracee, uintptr_t flags);
void print_mount_flags(struct stracee_t *tracee, uintptr_t flags);
void print_umount_flags(struct stracee_t *tracee, uintptr_t flags);
void print_sa_flags(struct stracee_t *tracee, int flags);
void print_prot_flags(struct stracee_t *tracee, int flags);
void print_ptrace_request(struct stracee_t *tracee, size_t req);
void print_clock_id(struct stracee_t *tracee, size_t id);
void print_clock_flags(struct stracee_t *tracee, size_t flags);
void print_itimer_id(struct stracee_t *tracee, size_t id);
void print_sched_policy(struct stracee_t *tracee, size_t id);
void print_arg_prio(struct stracee_t *tracee, int which);
void print_mmap_args(struct stracee_t *tracee, uintptr_t ptr);
void print_mremap_args(struct stracee_t *tracee, uintptr_t ptr);
void print_sysctl_args(struct stracee_t *tracee, uintptr_t ptr);
void print_pselect_args(struct stracee_t *tracee, uintptr_t ptr);
void print_sendto_args(struct stracee_t *tracee, uintptr_t ptr);
void print_recvfrom_args(struct stracee_t *tracee, uintptr_t ptr);
void print_msgrcv_args(struct stracee_t *tracee, uintptr_t ptr);


void process_eoption_trace(char *myname, char *_optstr);
void process_eoption_signal(char *myname, char *_optstr);
void process_eoption_status(char *myname, char *_optstr);
void process_eoption_silent(char *myname, char *_optstr);
void process_eoption_decode_fds(char *myname, char *_optstr);
void process_eoption_inject(char *myname, char *_optstr);
void process_eoption_fault(char *myname, char *_optstr);

