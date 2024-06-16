/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: strace-syscall.h
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
 *  \file strace-syscall.h
 *
 *  Syscall macros and definitions for the strace utility.
 */

#include <sys/syscall.h>

/*
 * Define the array in only one file.
 */
#ifdef DEFINE_SYSCALL_NAMES

const char *syscall_names[] =
{
    [__NR_setup             ] = "setup",
    [__NR_exit              ] = "exit",
    [__NR_fork              ] = "fork",
    [__NR_read              ] = "read",
    [__NR_write             ] = "write",
    [__NR_open              ] = "open",
    [__NR_close             ] = "close",
    [__NR_waitpid           ] = "waitpid",
    [__NR_creat             ] = "creat",
    [__NR_link              ] = "link",
    [__NR_unlink            ] = "unlink",
    [__NR_execve            ] = "execve",
    [__NR_chdir             ] = "chdir",
    [__NR_time              ] = "time",
    [__NR_mknod             ] = "mknod",
    [__NR_chmod             ] = "chmod",
    [__NR_lchown            ] = "lchown",
    [__NR_break             ] = "break",
    [__NR_oldstat           ] = "oldstat",
    [__NR_lseek             ] = "lseek",
    [__NR_getpid            ] = "getpid",
    [__NR_mount             ] = "mount",
    [__NR_umount            ] = "umount",
    [__NR_setuid            ] = "setuid",
    [__NR_getuid            ] = "getuid",
    [__NR_stime             ] = "stime",
    [__NR_ptrace            ] = "ptrace",
    [__NR_alarm             ] = "alarm",
    [__NR_oldfstat          ] = "oldfstat",
    [__NR_pause             ] = "pause",
    [__NR_utime             ] = "utime",
    [__NR_setheap           ] = "setheap",
    [__NR_gtty              ] = "gtty",
    [__NR_access            ] = "access",
    [__NR_nice              ] = "nice",
    [__NR_ftime             ] = "ftime",
    [__NR_sync              ] = "sync",
    [__NR_kill              ] = "kill",
    [__NR_rename            ] = "rename",
    [__NR_mkdir             ] = "mkdir",
    [__NR_rmdir             ] = "rmdir",
    [__NR_dup               ] = "dup",
    [__NR_pipe              ] = "pipe",
    [__NR_times             ] = "times",
    [__NR_prof              ] = "prof",
    [__NR_brk               ] = "brk",
    [__NR_setgid            ] = "setgid",
    [__NR_getgid            ] = "getgid",
    [__NR_signal            ] = "signal",
    [__NR_geteuid           ] = "geteuid",
    [__NR_getegid           ] = "getegid",
    [__NR_acct              ] = "acct",
    [__NR_umount2           ] = "umount2",
    [__NR_lock              ] = "lock",
    [__NR_ioctl             ] = "ioctl",
    [__NR_fcntl             ] = "fcntl",
    [__NR_mpx               ] = "mpx",
    [__NR_setpgid           ] = "setpgid",
    [__NR_ulimit            ] = "ulimit",
    [__NR_oldolduname       ] = "oldolduname",
    [__NR_umask             ] = "umask",
    [__NR_chroot            ] = "chroot",
    [__NR_ustat             ] = "ustat",
    [__NR_dup2              ] = "dup2",
    [__NR_getppid           ] = "getppid",
    [__NR_getpgrp           ] = "getpgrp",
    [__NR_setsid            ] = "setsid",
    [__NR_sigaction         ] = "sigaction",
    [__NR_sgetmask          ] = "sgetmask",
    [__NR_ssetmask          ] = "ssetmask",
    [__NR_setreuid          ] = "setreuid",
    [__NR_setregid          ] = "setregid",
    [__NR_sigsuspend        ] = "sigsuspend",
    [__NR_sigpending        ] = "sigpending",
    [__NR_sethostname       ] = "sethostname",
    [__NR_setrlimit         ] = "setrlimit",
    [__NR_getrlimit         ] = "getrlimit",
    [__NR_getrusage         ] = "getrusage",
    [__NR_gettimeofday      ] = "gettimeofday",
    [__NR_settimeofday      ] = "settimeofday",
    [__NR_getgroups         ] = "getgroups",
    [__NR_setgroups         ] = "setgroups",
    [__NR_select            ] = "select",
    [__NR_symlink           ] = "symlink",
    [__NR_oldlstat          ] = "oldlstat",
    [__NR_readlink          ] = "readlink",
    [__NR_uselib            ] = "uselib",
    [__NR_swapon            ] = "swapon",
    [__NR_reboot            ] = "reboot",
    [__NR_readdir           ] = "readdir",
    [__NR_mmap              ] = "mmap",
    [__NR_munmap            ] = "munmap",
    [__NR_truncate          ] = "truncate",
    [__NR_ftruncate         ] = "ftruncate",
    [__NR_fchmod            ] = "fchmod",
    [__NR_fchown            ] = "fchown",
    [__NR_getpriority       ] = "getpriority",
    [__NR_setpriority       ] = "setpriority",
    [__NR_profil            ] = "profil",
    [__NR_statfs            ] = "statfs",
    [__NR_fstatfs           ] = "fstatfs",
    [__NR_ioperm            ] = "ioperm",
    [__NR_socketcall        ] = "socketcall",
    [__NR_syslog            ] = "syslog",
    [__NR_setitimer         ] = "setitimer",
    [__NR_getitimer         ] = "getitimer",
    [__NR_stat              ] = "stat",
    [__NR_lstat             ] = "lstat",
    [__NR_fstat             ] = "fstat",
    [__NR_olduname          ] = "olduname",	                // 109

    [__NR_vhangup           ] = "vhangup",                  // 111
    [__NR_idle              ] = "idle",                     // 112

    [__NR_wait4             ] = "wait4",                    // 114
    [__NR_swapoff           ] = "swapoff",
    [__NR_sysinfo           ] = "sysinfo",
    [__NR_ipc               ] = "ipc",
    [__NR_fsync             ] = "fsync",
    [__NR_sigreturn         ] = "sigreturn",
    [__NR_clone             ] = "clone",
    [__NR_setdomainname     ] = "setdomainname",
    [__NR_uname             ] = "uname",	                // 122

    [__NR_mprotect          ] = "mprotect",                 // 125
    [__NR_sigprocmask       ] = "sigprocmask",              // 126

    [__NR_init_module       ] = "init_module",              // 128
    [__NR_delete_module     ] = "delete_module",            // 129

    [__NR_getpgid           ] = "getpgid",                  // 132
    [__NR_fchdir            ] = "fchdir",                   // 133

    [__NR_sysfs             ] = "sysfs",                    // 135

    [__NR_getdents          ] = "getdents",                 // 141
    [__NR_newselect         ] = "newselect",
    [__NR_flock             ] = "flock",
    [__NR_msync             ] = "msync",
    [__NR_readv             ] = "readv",
    [__NR_writev            ] = "writev",
    [__NR_getsid            ] = "getsid",
    [__NR_fdatasync         ] = "fdatasync",
    [__NR_sysctl            ] = "sysctl",
    [__NR_mlock             ] = "mlock",
    [__NR_munlock           ] = "munlock",
    [__NR_mlockall          ] = "mlockall",
    [__NR_munlockall        ] = "munlockall",
    [__NR_sched_setparam    ] = "sched_setparam",
    [__NR_sched_getparam    ] = "sched_getparam",
    [__NR_sched_setscheduler] = "sched_setscheduler",
    [__NR_sched_getscheduler] = "sched_getscheduler",
    [__NR_sched_yield       ] = "sched_yield",
    [__NR_sched_get_priority_max] = "sched_get_priority_max",
    [__NR_sched_get_priority_min] = "sched_get_priority_min",
    [__NR_sched_rr_get_interval ] = "sched_rr_get_interval",
    [__NR_nanosleep         ] = "nanosleep",
    [__NR_mremap            ] = "mremap",
    [__NR_setresuid         ] = "setresuid",
    [__NR_getresuid         ] = "getresuid",                // 165

    [__NR_poll              ] = "poll",                     // 168

    [__NR_setresgid         ] = "setresgid",                // 170
    [__NR_getresgid         ] = "getresgid",                // 171

    [__NR_pread             ] = "pread",                    // 180
    [__NR_pwrite            ] = "pwrite",
    [__NR_chown             ] = "chown",
    [__NR_getcwd            ] = "getcwd",                   // 183

    [__NR_signalstack       ] = "signalstack",              // 186

    [__NR_vfork             ] = "vfork",                    // 190

    [__NR_lchown32          ] = "lchown32",                 // 198
    [__NR_getuid32          ] = "getuid32",
    [__NR_getgid32          ] = "getgid32",
    [__NR_geteuid32         ] = "geteuid32",
    [__NR_getegid32         ] = "getegid32",
    [__NR_setreuid32        ] = "setreuid32",
    [__NR_setregid32        ] = "setregid32",
    [__NR_getgroups32       ] = "getgroups32",
    [__NR_setgroups32       ] = "setgroups32",
    [__NR_fchown32          ] = "fchown32",
    [__NR_setresuid32       ] = "setresuid32",
    [__NR_getresuid32       ] = "getresuid32",
    [__NR_setresgid32       ] = "setresgid32",
    [__NR_getresgid32       ] = "getresgid32",
    [__NR_chown32           ] = "chown32",
    [__NR_setuid32          ] = "setuid32",
    [__NR_setgid32          ] = "setgid32",                 // 214

    [__NR_mincore           ] = "mincore",                  // 218

    [__NR_gettid            ] = "gettid",                   // 224

    [__NR_set_thread_area   ] = "set_thread_area",          // 243
    [__NR_get_thread_area   ] = "get_thread_area",          // 244

    [__NR_exit_group        ] = "exit_group",               // 252

    [__NR_timer_create      ] = "timer_create",             // 259
    [__NR_timer_settime     ] = "timer_settime",
    [__NR_timer_gettime     ] = "timer_gettime",
    [__NR_timer_getoverrun  ] = "timer_getoverrun",
    [__NR_timer_delete      ] = "timer_delete",
    [__NR_clock_settime     ] = "clock_settime",
    [__NR_clock_gettime     ] = "clock_gettime",
    [__NR_clock_getres      ] = "clock_getres",
    [__NR_clock_nanosleep   ] = "clock_nanosleep",          // 267

    [__NR_tgkill            ] = "tgkill",                   // 270
    [__NR_utimes            ] = "utimes",                   // 271

    [__NR_waitid            ] = "waitid",                   // 284

    [__NR_openat            ] = "openat",                   // 295
    [__NR_mkdirat           ] = "mkdirat",
    [__NR_mknodat           ] = "mknodat",
    [__NR_fchownat          ] = "fchownat",
    [__NR_futimesat         ] = "futimesat",
    [__NR_fstatat           ] = "fstatat",
    [__NR_unlinkat          ] = "unlinkat",
    [__NR_renameat          ] = "renameat",
    [__NR_linkat            ] = "linkat",
    [__NR_symlinkat         ] = "symlinkat",
    [__NR_readlinkat        ] = "readlinkat",
    [__NR_fchmodat          ] = "fchmodat",
    [__NR_faccessat         ] = "faccessat",
    [__NR_pselect           ] = "pselect",
    [__NR_ppoll             ] = "ppoll",                    // 309

    [__NR_dup3              ] = "dup3",	                    // 330
    [__NR_pipe2             ] = "pipe2",	                // 331

    [__NR_preadv            ] = "preadv",                   // 333
    [__NR_pwritev           ] = "pwritev",                  // 334

    [__NR_prlimit           ] = "prlimit",                  // 340

    [__NR_syncfs            ] = "syncfs",	                // 344

    [__NR_getrandom         ] = "getrandom",                // 355

    [__NR_execveat          ] = "execveat",                 // 358
    [__NR_socket            ] = "socket",
    [__NR_socketpair        ] = "socketpair",
    [__NR_bind              ] = "bind",
    [__NR_connect           ] = "connect",
    [__NR_listen            ] = "listen",
    [__NR_accept            ] = "accept",
    [__NR_getsockopt        ] = "getsockopt",
    [__NR_setsockopt        ] = "setsockopt",
    [__NR_getsockname       ] = "getsockname",
    [__NR_getpeername       ] = "getpeername",
    [__NR_sendto            ] = "sendto",
    [__NR_sendmsg           ] = "sendmsg",
    [__NR_recvfrom          ] = "recvfrom",
    [__NR_recvmsg           ] = "recvmsg",
    [__NR_shutdown          ] = "shutdown",                 // 373

    [__NR_mlock2            ] = "mlock2",                   // 376

// END of Linux x86 syscall list (last entry is #384)
// The following are LaylaOS-specific syscalls (they exist on non-x86 Linux)

    [__NR_msgget            ] = "msgget",                   // 385
    [__NR_msgsnd            ] = "msgsnd",
    [__NR_msgrcv            ] = "msgrcv",
    [__NR_msgctl            ] = "msgctl",
    [__NR_semget            ] = "semget",
    [__NR_semop             ] = "semop",
    [__NR_semctl            ] = "semctl",
    [__NR_shmat             ] = "shmat",
    [__NR_shmctl            ] = "shmctl",
    [__NR_shmdt             ] = "shmdt",
    [__NR_shmget            ] = "shmget",                   // 395
};

int syscall_name_count = (sizeof(syscall_names) / sizeof(syscall_names[0]));

#else   /* !DEFINE_SYSCALL_NAMES */

extern char *syscall_names[];
extern int syscall_name_count;

#endif  /* DEFINE_SYSCALL_NAMES */

/*
#define SYSCALL_NAMES_COUNT             \
    (sizeof(syscall_names) / sizeof(syscall_names[0]))
*/


/*
 * Define the array in only one file.
 */
#ifdef DEFINE_SYSCALL_MASK

char syscall_mask[] =
{
    [__NR_setup             ] = 1,
    [__NR_exit              ] = 1,
    [__NR_fork              ] = 1,
    [__NR_read              ] = 1,
    [__NR_write             ] = 1,
    [__NR_open              ] = 1,
    [__NR_close             ] = 1,
    [__NR_waitpid           ] = 1,
    [__NR_creat             ] = 1,
    [__NR_link              ] = 1,
    [__NR_unlink            ] = 1,
    [__NR_execve            ] = 1,
    [__NR_chdir             ] = 1,
    [__NR_time              ] = 1,
    [__NR_mknod             ] = 1,
    [__NR_chmod             ] = 1,
    [__NR_lchown            ] = 1,
    [__NR_break             ] = 1,
    [__NR_oldstat           ] = 1,
    [__NR_lseek             ] = 1,
    [__NR_getpid            ] = 1,
    [__NR_mount             ] = 1,
    [__NR_umount            ] = 1,
    [__NR_setuid            ] = 1,
    [__NR_getuid            ] = 1,
    [__NR_stime             ] = 1,
    [__NR_ptrace            ] = 1,
    [__NR_alarm             ] = 1,
    [__NR_oldfstat          ] = 1,
    [__NR_pause             ] = 1,
    [__NR_utime             ] = 1,
    [__NR_setheap           ] = 1,
    [__NR_gtty              ] = 1,
    [__NR_access            ] = 1,
    [__NR_nice              ] = 1,
    [__NR_ftime             ] = 1,
    [__NR_sync              ] = 1,
    [__NR_kill              ] = 1,
    [__NR_rename            ] = 1,
    [__NR_mkdir             ] = 1,
    [__NR_rmdir             ] = 1,
    [__NR_dup               ] = 1,
    [__NR_pipe              ] = 1,
    [__NR_times             ] = 1,
    [__NR_prof              ] = 1,
    [__NR_brk               ] = 1,
    [__NR_setgid            ] = 1,
    [__NR_getgid            ] = 1,
    [__NR_signal            ] = 1,
    [__NR_geteuid           ] = 1,
    [__NR_getegid           ] = 1,
    [__NR_acct              ] = 1,
    [__NR_umount2           ] = 1,
    [__NR_lock              ] = 1,
    [__NR_ioctl             ] = 1,
    [__NR_fcntl             ] = 1,
    [__NR_mpx               ] = 1,
    [__NR_setpgid           ] = 1,
    [__NR_ulimit            ] = 1,
    [__NR_oldolduname       ] = 1,
    [__NR_umask             ] = 1,
    [__NR_chroot            ] = 1,
    [__NR_ustat             ] = 1,
    [__NR_dup2              ] = 1,
    [__NR_getppid           ] = 1,
    [__NR_getpgrp           ] = 1,
    [__NR_setsid            ] = 1,
    [__NR_sigaction         ] = 1,
    [__NR_sgetmask          ] = 1,
    [__NR_ssetmask          ] = 1,
    [__NR_setreuid          ] = 1,
    [__NR_setregid          ] = 1,
    [__NR_sigsuspend        ] = 1,
    [__NR_sigpending        ] = 1,
    [__NR_sethostname       ] = 1,
    [__NR_setrlimit         ] = 1,
    [__NR_getrlimit         ] = 1,
    [__NR_getrusage         ] = 1,
    [__NR_gettimeofday      ] = 1,
    [__NR_settimeofday      ] = 1,
    [__NR_getgroups         ] = 1,
    [__NR_setgroups         ] = 1,
    [__NR_select            ] = 1,
    [__NR_symlink           ] = 1,
    [__NR_oldlstat          ] = 1,
    [__NR_readlink          ] = 1,
    [__NR_uselib            ] = 1,
    [__NR_swapon            ] = 1,
    [__NR_reboot            ] = 1,
    [__NR_readdir           ] = 1,
    [__NR_mmap              ] = 1,
    [__NR_munmap            ] = 1,
    [__NR_truncate          ] = 1,
    [__NR_ftruncate         ] = 1,
    [__NR_fchmod            ] = 1,
    [__NR_fchown            ] = 1,
    [__NR_getpriority       ] = 1,
    [__NR_setpriority       ] = 1,
    [__NR_profil            ] = 1,
    [__NR_statfs            ] = 1,
    [__NR_fstatfs           ] = 1,
    [__NR_ioperm            ] = 1,
    [__NR_socketcall        ] = 1,
    [__NR_syslog            ] = 1,
    [__NR_setitimer         ] = 1,
    [__NR_getitimer         ] = 1,
    [__NR_stat              ] = 1,
    [__NR_lstat             ] = 1,
    [__NR_fstat             ] = 1,
    [__NR_olduname          ] = 1,	                    // 109

    [__NR_vhangup           ] = 1,                      // 111
    [__NR_idle              ] = 1,                      // 112

    [__NR_wait4             ] = 1,                      // 114
    [__NR_swapoff           ] = 1,
    [__NR_sysinfo           ] = 1,
    [__NR_ipc               ] = 1,
    [__NR_fsync             ] = 1,
    [__NR_sigreturn         ] = 1,
    [__NR_clone             ] = 1,
    [__NR_setdomainname     ] = 1,
    [__NR_uname             ] = 1,	                    // 122

    [__NR_mprotect          ] = 1,                      // 125
    [__NR_sigprocmask       ] = 1,                      // 126

    [__NR_init_module       ] = 1,                      // 128
    [__NR_delete_module     ] = 1,                      // 129

    [__NR_getpgid           ] = 1,                      // 132
    [__NR_fchdir            ] = 1,                      // 133

    [__NR_sysfs             ] = 1,                      // 135

    [__NR_getdents          ] = 1,                      // 141
    [__NR_newselect         ] = 1,
    [__NR_flock             ] = 1,
    [__NR_msync             ] = 1,
    [__NR_readv             ] = 1,
    [__NR_writev            ] = 1,
    [__NR_getsid            ] = 1,
    [__NR_fdatasync         ] = 1,
    [__NR_sysctl            ] = 1,
    [__NR_mlock             ] = 1,
    [__NR_munlock           ] = 1,
    [__NR_mlockall          ] = 1,
    [__NR_munlockall        ] = 1,
    [__NR_sched_setparam    ] = 1,
    [__NR_sched_getparam    ] = 1,
    [__NR_sched_setscheduler] = 1,
    [__NR_sched_getscheduler] = 1,
    [__NR_sched_yield       ] = 1,
    [__NR_sched_get_priority_max] = 1,
    [__NR_sched_get_priority_min] = 1,
    [__NR_sched_rr_get_interval ] = 1,
    [__NR_nanosleep         ] = 1,
    [__NR_mremap            ] = 1,
    [__NR_setresuid         ] = 1,
    [__NR_getresuid         ] = 1,                      // 165

    [__NR_poll              ] = 1,                      // 168

    [__NR_setresgid         ] = 1,                      // 170
    [__NR_getresgid         ] = 1,                      // 171

    [__NR_pread             ] = 1,                      // 180
    [__NR_pwrite            ] = 1,
    [__NR_chown             ] = 1,
    [__NR_getcwd            ] = 1,                      // 183

    [__NR_signalstack       ] = 1,                      // 186

    [__NR_vfork             ] = 1,                      // 190

    [__NR_lchown32          ] = 1,                      // 198
    [__NR_getuid32          ] = 1,
    [__NR_getgid32          ] = 1,
    [__NR_geteuid32         ] = 1,
    [__NR_getegid32         ] = 1,
    [__NR_setreuid32        ] = 1,
    [__NR_setregid32        ] = 1,
    [__NR_getgroups32       ] = 1,
    [__NR_setgroups32       ] = 1,
    [__NR_fchown32          ] = 1,
    [__NR_setresuid32       ] = 1,
    [__NR_getresuid32       ] = 1,
    [__NR_setresgid32       ] = 1,
    [__NR_getresgid32       ] = 1,
    [__NR_chown32           ] = 1,
    [__NR_setuid32          ] = 1,
    [__NR_setgid32          ] = 1,                      // 214

    [__NR_mincore           ] = 1,                      // 218

    [__NR_gettid            ] = 1,                      // 224

    [__NR_set_thread_area   ] = 1,                      // 243
    [__NR_get_thread_area   ] = 1,                      // 244

    [__NR_exit_group        ] = 1,                      // 252

    [__NR_timer_create      ] = 1,                      // 259
    [__NR_timer_settime     ] = 1,
    [__NR_timer_gettime     ] = 1,
    [__NR_timer_getoverrun  ] = 1,
    [__NR_timer_delete      ] = 1,
    [__NR_clock_settime     ] = 1,
    [__NR_clock_gettime     ] = 1,
    [__NR_clock_getres      ] = 1,
    [__NR_clock_nanosleep   ] = 1,                      // 267

    [__NR_tgkill            ] = 1,                      // 270
    [__NR_utimes            ] = 1,                      // 271

    [__NR_waitid            ] = 1,                      // 284

    [__NR_openat            ] = 1,                      // 295
    [__NR_mkdirat           ] = 1,
    [__NR_mknodat           ] = 1,
    [__NR_fchownat          ] = 1,
    [__NR_futimesat         ] = 1,
    [__NR_fstatat           ] = 1,
    [__NR_unlinkat          ] = 1,
    [__NR_renameat          ] = 1,
    [__NR_linkat            ] = 1,
    [__NR_symlinkat         ] = 1,
    [__NR_readlinkat        ] = 1,
    [__NR_fchmodat          ] = 1,
    [__NR_faccessat         ] = 1,
    [__NR_pselect           ] = 1,
    [__NR_ppoll             ] = 1,                      // 309

    [__NR_dup3              ] = 1,	                    // 330
    [__NR_pipe2             ] = 1,	                    // 331

    [__NR_preadv            ] = 1,                      // 333
    [__NR_pwritev           ] = 1,                      // 334

    [__NR_prlimit           ] = 1,                      // 340

    [__NR_syncfs            ] = 1,	                    // 344

    [__NR_getrandom         ] = 1,                      // 355

    [__NR_execveat          ] = 1,                      // 358
    [__NR_socket            ] = 1,
    [__NR_socketpair        ] = 1,
    [__NR_bind              ] = 1,
    [__NR_connect           ] = 1,
    [__NR_listen            ] = 1,
    [__NR_accept            ] = 1,
    [__NR_getsockopt        ] = 1,
    [__NR_setsockopt        ] = 1,
    [__NR_getsockname       ] = 1,
    [__NR_getpeername       ] = 1,
    [__NR_sendto            ] = 1,
    [__NR_sendmsg           ] = 1,
    [__NR_recvfrom          ] = 1,
    [__NR_recvmsg           ] = 1,
    [__NR_shutdown          ] = 1,                      // 373

    [__NR_mlock2            ] = 1,                      // 376

// END of Linux x86 syscall list (last entry is #384)
// The following are LaylaOS-specific syscalls (they exist on non-x86 Linux)

    [__NR_msgget            ] = 1,                      // 385
    [__NR_msgsnd            ] = 1,
    [__NR_msgrcv            ] = 1,
    [__NR_msgctl            ] = 1,
    [__NR_semget            ] = 1,
    [__NR_semop             ] = 1,
    [__NR_semctl            ] = 1,
    [__NR_shmat             ] = 1,
    [__NR_shmctl            ] = 1,
    [__NR_shmdt             ] = 1,
    [__NR_shmget            ] = 1,                      // 395
};

int syscall_mask_count = (sizeof(syscall_mask) / sizeof(syscall_mask[0]));

#else   /* !DEFINE_SYSCALL_MASK */

extern char syscall_mask[];
extern int syscall_mask_count;

#endif  /* DEFINE_SYSCALL_MASK */

/*
#define SYSCALL_MASK_COUNT              \
    (sizeof(syscall_mask) / sizeof(syscall_mask[0]))
*/


#define SET_SYSCALL_MASK(list)              \
{                                           \
    int *i, syscalls[] = { list, 0 };       \
    for(i = syscalls; *i; i++)              \
        syscall_mask[*i] = 1;               \
}


#define NETWORK_SYSCALL_LIST                                        \
    __NR_sethostname, __NR_socketcall, __NR_setdomainname,          \
    __NR_socket, __NR_socketpair, __NR_bind, __NR_connect,          \
    __NR_listen, __NR_accept, __NR_getsockopt, __NR_setsockopt,     \
    __NR_getsockname, __NR_getpeername, __NR_sendto, __NR_sendmsg,  \
    __NR_recvfrom, __NR_recvmsg, __NR_shutdown


#define FILE_SYSCALL_LIST                                           \
    __NR_open, __NR_creat, __NR_link, __NR_unlink, __NR_execve,     \
    __NR_chdir, __NR_mknod, __NR_chmod, __NR_lchown, __NR_oldstat,  \
    __NR_mount, __NR_umount, __NR_oldfstat, __NR_utime,             \
    __NR_access, __NR_rename, __NR_mkdir, __NR_rmdir, __NR_times,   \
    __NR_acct, __NR_umount2, __NR_chroot, __NR_symlink,             \
    __NR_oldlstat, __NR_readlink, __NR_uselib, __NR_swapon,         \
    __NR_truncate, __NR_statfs, __NR_stat, __NR_lstat,              \
    __NR_swapoff, __NR_chown, __NR_getcwd, __NR_lchown32,           \
    __NR_chown32, __NR_utimes, __NR_openat, __NR_mkdirat,           \
    __NR_mknodat, __NR_unlinkat, __NR_renameat, __NR_linkat,        \
    __NR_symlinkat, __NR_readlinkat, __NR_execveat


#define DESC_SYSCALL_LIST                                           \
    __NR_read, __NR_write, __NR_open, __NR_close, __NR_lseek,       \
    __NR_oldfstat, __NR_dup, __NR_pipe, __NR_ioctl, __NR_fcntl,     \
    __NR_dup2, __NR_select, __NR_readdir, __NR_mmap,                \
    __NR_ftruncate, __NR_fchmod, __NR_fchown, __NR_fstatfs,         \
    __NR_fstat, __NR_fsync, __NR_fchdir, __NR_getdents,             \
    __NR_newselect, __NR_flock, __NR_readv, __NR_writev,            \
    __NR_fdatasync, __NR_poll, __NR_pread, __NR_pwrite,             \
    __NR_fchown32, __NR_fchownat, __NR_futimesat, __NR_fstatat,     \
    __NR_fchmodat, __NR_faccessat, __NR_pselect, __NR_ppoll,        \
    __NR_dup3, __NR_pipe2, __NR_preadv, __NR_pwritev, __NR_syncfs


#define MEMORY_SYSCALL_LIST                                         \
    __NR_setheap, __NR_brk, __NR_mmap, __NR_munmap, __NR_mprotect,  \
    __NR_msync, __NR_mlock, __NR_munlock, __NR_mlockall,            \
    __NR_munlockall, __NR_mremap, __NR_mincore, __NR_mlock2


#define IPC_SYSCALL_LIST                                            \
    __NR_ipc, __NR_msgget, __NR_msgsnd, __NR_msgrcv, __NR_msgctl,   \
    __NR_semget, __NR_semop, __NR_semctl, __NR_shmat, __NR_shmctl,  \
    __NR_shmdt, __NR_shmget


#define SIGNAL_SYSCALL_LIST                                         \
    __NR_alarm, __NR_pause, __NR_kill, __NR_signal, __NR_sigaction, \
    __NR_sgetmask, __NR_ssetmask, __NR_sigsuspend, __NR_sigpending, \
    __NR_sigprocmask, __NR_signalstack


#define PROCESS_SYSCALL_LIST                                        \
    __NR_exit, __NR_fork, __NR_waitpid, __NR_execve, __NR_pause,    \
    __NR_kill, __NR_wait4, __NR_clone, __NR_vfork, __NR_exit_group, \
    __NR_tgkill, __NR_waitid, __NR_execveat


#define CREDS_SYSCALL_LIST                                          \
    __NR_setuid, __NR_getuid, __NR_setgid, __NR_getgid,             \
    __NR_geteuid, __NR_getegid, __NR_setpgid, __NR_getppid,         \
    __NR_getpgrp, __NR_setsid, __NR_setreuid, __NR_setregid,        \
    __NR_getgroups, __NR_setgroups, __NR_getpgid, __NR_setresuid,   \
    __NR_getresuid, __NR_setresgid, __NR_getresgid, __NR_getuid32,  \
    __NR_getgid32, __NR_geteuid32, __NR_getegid32, __NR_setreuid32, \
    __NR_setregid32, __NR_getgroups32, __NR_setgroups32,            \
    __NR_setresuid32, __NR_getresuid32, __NR_setresgid32,           \
    __NR_getresgid32, __NR_setuid32, __NR_setgid32


#define CLOCK_SYSCALL_LIST                                          \
    __NR_clock_settime, __NR_clock_gettime, __NR_clock_getres,      \
    __NR_clock_nanosleep


#define PURE_SYSCALL_LIST                                           \
    __NR_getegid, __NR_getegid32, __NR_geteuid, __NR_geteuid32,     \
    __NR_getgid, __NR_getgid32, __NR_getpgrp, __NR_getpid,          \
    __NR_getppid, __NR_get_thread_area, __NR_gettid, __NR_getuid,   \
    __NR_getuid32


#define STAT_SYSCALL_LIST       __NR_oldstat, __NR_stat
#define LSTAT_SYSCALL_LIST      __NR_oldlstat, __NR_lstat
#define FSTAT_SYSCALL_LIST      __NR_oldfstat, __NR_fstat, __NR_fstatat

#define STATFS_SYSCALL_LIST     __NR_statfs
#define FSTATFS_SYSCALL_LIST    __NR_fstatfs


/*
 * Definitions used in processing -e status, --status, -z and -Z options.
 */

#define SYSCAL_STATUS_SUCCESSFUL            0
#define SYSCAL_STATUS_FAILED                1

/*
 * Define the array in only one file.
 */
#ifdef DEFINE_SYSCALL_STATUS_MASK

char syscall_status_mask[] =
{
    [SYSCAL_STATUS_SUCCESSFUL] = 1,
    [SYSCAL_STATUS_FAILED    ] = 1,
};

#else   /* !DEFINE_SYSCALL_STATUS_MASK */

extern char syscall_status_mask[];

#endif  /* DEFINE_SYSCALL_STATUS_MASK */


/*
 * Definitions used in processing -e inject, --inject, -e fault and --fault options.
 */

struct inject_t
{
    int inject;     // 1 = inject, 0 = don't inject
    int error;      // error and retval are mutually exclusive
    int retval;
    int signum;     // 0 = don't send a signal on syscall entry
    int syscall;    // syscall num to inject (only works for %pure syscalls)
};

extern struct inject_t *syscall_inject_mask;

