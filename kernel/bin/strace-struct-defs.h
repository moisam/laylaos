/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: strace-struct-defs.h
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
 *  \file strace-struct-defs.h
 *
 *  Different structure definitions for the strace utility.
 */

#define _POSIX_CPUTIME
#define _POSIX_THREAD_CPUTIME
#define _POSIX_MONOTONIC_CLOCK
#define _POSIX_CLOCK_SELECTION

#define __POSIX_VISIBLE             200809
#undef __GNU_VISIBLE
#define __GNU_VISIBLE   1
#undef __BSD_VISIBLE
#define __BSD_VISIBLE   1

#include <sys/resource.h>

#define NEQSTR(n)           [n] = #n

const char *rlimit_names[] =
{
    NEQSTR(RLIMIT_CPU),
    NEQSTR(RLIMIT_FSIZE),
    NEQSTR(RLIMIT_DATA),
    NEQSTR(RLIMIT_STACK),
    NEQSTR(RLIMIT_CORE),
    NEQSTR(RLIMIT_RSS),
    NEQSTR(RLIMIT_NPROC),
    NEQSTR(RLIMIT_NOFILE),
    NEQSTR(RLIMIT_MEMLOCK),
    NEQSTR(RLIMIT_AS),
    NEQSTR(RLIMIT_LOCKS),
    NEQSTR(RLIMIT_SIGPENDING),
    NEQSTR(RLIMIT_MSGQUEUE),
    NEQSTR(RLIMIT_NICE),
    NEQSTR(RLIMIT_RTPRIO),
    NEQSTR(RLIMIT_RTTIME),
};


#include <fcntl.h>

#ifndef _STRUCT_FLAG_T_DEFINED
#define _STRUCT_FLAG_T_DEFINED
struct flag_t
{
    int val;
    char *name;
};
#endif      /* _STRUCT_FLAG_T_DEFINED */

#define FEQSTR(n)           { n, #n }


struct flag_t open_flags[] =
{
    FEQSTR(O_RDONLY),
    FEQSTR(O_WRONLY),
    FEQSTR(O_RDWR),
    FEQSTR(O_APPEND),
    FEQSTR(O_CREAT),
    FEQSTR(O_TRUNC),
    FEQSTR(O_EXCL),
    FEQSTR(O_SYNC),
#ifdef O_NDELAY
    FEQSTR(O_NDELAY),
#endif
    FEQSTR(O_NONBLOCK),
    FEQSTR(O_NOCTTY),
    FEQSTR(O_CLOEXEC),
    FEQSTR(O_NOFOLLOW),
    FEQSTR(O_DIRECTORY),
    FEQSTR(O_EXEC),
    FEQSTR(O_SEARCH),
    FEQSTR(O_DIRECT),
#ifdef O_BINARY
    FEQSTR(O_BINARY),
#endif
#ifdef O_TEXT
    FEQSTR(O_TEXT),
#endif
    FEQSTR(O_DSYNC),
#ifdef O_RSYNC
    FEQSTR(O_RSYNC),
#endif
#ifdef O_TMPFILE
    FEQSTR(O_TMPFILE),
#endif
    FEQSTR(O_NOATIME),
#ifdef O_PATH
    FEQSTR(O_PATH),
#endif
};

#define OPEN_FLAGS_COUNT        (sizeof(open_flags) / sizeof(open_flags[0]))


struct flag_t at_flags[] =
{
    FEQSTR(AT_FDCWD),
    FEQSTR(AT_EACCESS),
    FEQSTR(AT_SYMLINK_NOFOLLOW),
    FEQSTR(AT_SYMLINK_FOLLOW),
    FEQSTR(AT_REMOVEDIR),
    FEQSTR(AT_EMPTY_PATH),
};

#define AT_FLAGS_COUNT          (sizeof(at_flags) / sizeof(at_flags[0]))


#include <sys/wait.h>

struct flag_t wait_flags[] =
{
    FEQSTR(WNOHANG),
    FEQSTR(WUNTRACED),
    FEQSTR(WSTOPPED),
    FEQSTR(WEXITED),
    FEQSTR(WCONTINUED),
    FEQSTR(WNOWAIT),
};

#define WAIT_FLAGS_COUNT        (sizeof(wait_flags) / sizeof(wait_flags[0]))


#include <sys/mman.h>

struct flag_t prot_flags[] =
{
    FEQSTR(PROT_READ),
    FEQSTR(PROT_WRITE),
    FEQSTR(PROT_EXEC),
    FEQSTR(PROT_NONE),
    FEQSTR(PROT_GROWSDOWN),
    FEQSTR(PROT_GROWSUP),
};

#define PROT_FLAGS_COUNT        (sizeof(prot_flags) / sizeof(prot_flags[0]))


#include <sys/mount.h>

struct flag_t mount_flags[] =
{
    FEQSTR(MS_RDONLY),
    FEQSTR(MS_NOSUID),
    FEQSTR(MS_NODEV),
    FEQSTR(MS_NOEXEC),
    FEQSTR(MS_SYNCHRONOUS),
    FEQSTR(MS_REMOUNT),
    FEQSTR(MS_MANDLOCK),
    FEQSTR(MS_DIRSYNC),
    FEQSTR(MS_NOATIME),
    FEQSTR(MS_NODIRATIME),
    FEQSTR(MS_BIND),
    FEQSTR(MS_MOVE),
    FEQSTR(MS_REC),
    FEQSTR(MS_SILENT),
    FEQSTR(MS_POSIXACL),
    FEQSTR(MS_UNBINDABLE),
    FEQSTR(MS_PRIVATE),
    FEQSTR(MS_SLAVE),
    FEQSTR(MS_SHARED),
#ifdef MS_REALTIME
    FEQSTR(MS_REALTIME),
#endif
    FEQSTR(MS_KERNMOUNT),
    FEQSTR(MS_I_VERSION),
    FEQSTR(MS_STRICTATIME),
    FEQSTR(MS_LAZYTIME),
    FEQSTR(MS_ACTIVE),
    FEQSTR(MS_NOUSER),
#ifdef MS_FORCE
    FEQSTR(MS_FORCE),
#endif
#ifdef MS_SYSROOT
    FEQSTR(MS_SYSROOT),
#endif
};

#define MOUNT_FLAGS_COUNT       (sizeof(mount_flags) / sizeof(mount_flags[0]))


struct flag_t umount_flags[] =
{
    FEQSTR(MNT_FORCE),
    FEQSTR(MNT_DETACH),
    FEQSTR(MNT_EXPIRE),
    FEQSTR(UMOUNT_NOFOLLOW),
};

#define UMOUNT_FLAGS_COUNT      (sizeof(umount_flags) / sizeof(umount_flags[0]))


#include <sys/ptrace.h>

struct flag_t ptrace_requests[] =
{
    FEQSTR(PTRACE_TRACEME),
    FEQSTR(PTRACE_PEEKTEXT),
    FEQSTR(PTRACE_PEEKDATA),
    FEQSTR(PTRACE_PEEKUSER),
    FEQSTR(PTRACE_POKETEXT),
    FEQSTR(PTRACE_POKEDATA),
    FEQSTR(PTRACE_POKEUSER),
    FEQSTR(PTRACE_CONT),
    FEQSTR(PTRACE_KILL),
    FEQSTR(PTRACE_SINGLESTEP),
    FEQSTR(PTRACE_GETREGS),
    FEQSTR(PTRACE_SETREGS),
    FEQSTR(PTRACE_GETFPREGS),
    FEQSTR(PTRACE_SETFPREGS),
    FEQSTR(PTRACE_ATTACH),
    FEQSTR(PTRACE_DETACH),
    FEQSTR(PTRACE_GETFPXREGS),
    FEQSTR(PTRACE_SETFPXREGS),
    FEQSTR(PTRACE_SET_SYSCALL),
    FEQSTR(PTRACE_SYSCALL),
    FEQSTR(PTRACE_GET_THREAD_AREA),
    FEQSTR(PTRACE_SET_THREAD_AREA),

#ifdef __x86_64__
    FEQSTR(PTRACE_ARCH_PRCTL),
#endif

    FEQSTR(PTRACE_SYSEMU),
    FEQSTR(PTRACE_SYSEMU_SINGLESTEP),
    FEQSTR(PTRACE_SINGLEBLOCK),
    FEQSTR(PTRACE_SETOPTIONS),
    FEQSTR(PTRACE_GETEVENTMSG),
    FEQSTR(PTRACE_GETSIGINFO),
    FEQSTR(PTRACE_SETSIGINFO),
    FEQSTR(PTRACE_GETREGSET),
    FEQSTR(PTRACE_SETREGSET),
    FEQSTR(PTRACE_SEIZE),
    FEQSTR(PTRACE_INTERRUPT),
    FEQSTR(PTRACE_LISTEN),
    FEQSTR(PTRACE_PEEKSIGINFO),
    FEQSTR(PTRACE_GETSIGMASK),
    FEQSTR(PTRACE_SETSIGMASK),
    FEQSTR(PTRACE_SECCOMP_GET_FILTER),
    FEQSTR(PTRACE_SECCOMP_GET_METADATA),
    FEQSTR(PTRACE_GET_SYSCALL_INFO),
};

#define PTRACE_REQUEST_COUNT        \
    (sizeof(ptrace_requests) / sizeof(ptrace_requests[0]))


#include <time.h>

struct flag_t clock_ids[] =
{
    FEQSTR(CLOCK_REALTIME_COARSE),
    FEQSTR(CLOCK_REALTIME),
    FEQSTR(CLOCK_PROCESS_CPUTIME_ID),
    FEQSTR(CLOCK_THREAD_CPUTIME_ID),
    FEQSTR(CLOCK_MONOTONIC),
    FEQSTR(CLOCK_MONOTONIC_RAW),
    FEQSTR(CLOCK_MONOTONIC_COARSE),
    FEQSTR(CLOCK_BOOTTIME),
    FEQSTR(CLOCK_REALTIME_ALARM),
    FEQSTR(CLOCK_BOOTTIME_ALARM),
};

#define CLOCK_IDS_COUNT         (sizeof(clock_ids) / sizeof(clock_ids[0]))


struct flag_t itimer_ids[] =
{
    FEQSTR(ITIMER_REAL),
    FEQSTR(ITIMER_VIRTUAL),
    FEQSTR(ITIMER_PROF),
};

#define TIMER_IDS_COUNT         (sizeof(itimer_ids) / sizeof(itimer_ids[0]))


//#include <sys/sched.h>
#include <sched.h>

struct flag_t sched_policies[] =
{
    FEQSTR(SCHED_OTHER),
    FEQSTR(SCHED_FIFO),
    FEQSTR(SCHED_RR),

#if defined(_POSIX_SPORADIC_SERVER)
    FEQSTR(SCHED_SPORADIC),
#endif

};

#define SCHED_POLICY_COUNT      (sizeof(sched_policies) / sizeof(sched_policies[0]))


struct flag_t sa_flags[] =
{
    FEQSTR(SA_NOCLDSTOP),
    FEQSTR(SA_ONSTACK),
    FEQSTR(SA_RESETHAND),
    FEQSTR(SA_RESTART),
    FEQSTR(SA_SIGINFO),
    FEQSTR(SA_NOCLDWAIT),
    FEQSTR(SA_NODEFER),
#ifdef SA_COOKIE
    FEQSTR(SA_COOKIE),
#endif
    FEQSTR(SA_RESTORER),
#ifdef SA_DEFER
    FEQSTR(SA_DEFER),
#endif
};

#define SA_FLAGS_COUNT          (sizeof(sa_flags) / sizeof(sa_flags[0]))

#undef FEQSTR

