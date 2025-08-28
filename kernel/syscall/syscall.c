/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: syscall.c
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
 *  \file syscall.c
 *
 *  Master syscall table, as well as some syscall handlers that were too
 *  short or otherwise did not fit better in any other file.
 *
 *  See the full list of Linux's syscalls at:
 *    http://lxr.linux.no/#linux+v3.2/arch/x86/include/asm/unistd_32.h
 *    https://chromium.googlesource.com/chromiumos/docs/+/master/constants/syscalls.md
 */

//#define __DEBUG

#define __USE_XOPEN_EXTENDED
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/ptrace.h>
#include <sys/sysinfo.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <kernel/asm.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/dev.h>
#include <kernel/syscall.h>
#include <kernel/timer.h>
#include <kernel/clock.h>
#include <kernel/idt.h>
#include <kernel/ksignal.h>
#include <kernel/ksigset.h>
#include <kernel/tty.h>
#include <kernel/swap.h>
#include <kernel/ptrace.h>
#include <kernel/ipc.h>
#include <kernel/modules.h>
#include <kernel/msr.h>
#include <kernel/fio.h>
#include <kernel/reboot.h>
#include <kernel/fcntl.h>
#include <mm/kheap.h>
#include <mm/mmap.h>
#include <mm/mmngr_virtual.h>
#include <fs/sockfs.h>
#include <fs/procfs.h>
//#include <syscall.h>

#include "../kernel/task_funcs.c"

static long syscall_nosys(void);

#define __SYSCALL_NOSYS             syscall_nosys


syscall_func syscalls[] =
{
    __SYSCALL_NOSYS,                //sys_setup
    syscall_exit,
    syscall_fork,                   // syscall_dispatcher.S
    syscall_read,                   // read.c
    syscall_write,                  // write.c
    syscall_open,                   // open.c
    syscall_close,
    syscall_waitpid,                // wait.c
    syscall_creat,
    syscall_link,                   // link.c
    syscall_unlink,                 // link.c
    syscall_execve,                 // execve.c
    syscall_chdir,                  // chdir.c
    syscall_time,
    syscall_mknod,
    syscall_chmod,                  // chmod.c
    syscall_lchown,                 // chown.c
    __SYSCALL_NOSYS,                // break - unimplemented in Linux
    syscall_stat,                   // stat.c
    syscall_lseek,
    syscall_getpid,
    syscall_mount,
    syscall_umount,
    syscall_setuid,                 // ids.c
    syscall_getuid,                 // ids.c
    syscall_stime,                  // deprecated syscall
    syscall_ptrace,                 // ptrace.c
    syscall_alarm,                  // itimer.c
    syscall_fstat,                  // stat.c
    syscall_pause,
    syscall_utime,                  // utime.c
    syscall_setheap,                // stty - unimplemented in Linux
    __SYSCALL_NOSYS,                // gtty - unimplemented in Linux
    syscall_access,                 // access.c
    syscall_nice,                   // nice.c
    __SYSCALL_NOSYS,                // ftime - unimplemented in Linux
    syscall_sync,                   // fsync.c
    syscall_kill,                   // kill.c
    syscall_rename,                 // rename.c
    syscall_mkdir,                  // mkdir.c
    syscall_rmdir,
    syscall_dup,                    // dup.c
    syscall_pipe,                   // pipe.c
    syscall_times,
    __SYSCALL_NOSYS,                // prof - unimplemented in Linux
    syscall_brk,
    syscall_setgid,                 // ids.c
    syscall_getgid,                 // ids.c
    syscall_signal,                 // signal.c
    syscall_geteuid,                // ids.c
    syscall_getegid,                // ids.c
    syscall_acct,                   // acct.c
    syscall_umount2,
    __SYSCALL_NOSYS,                // lock - unimplemented in Linux
    syscall_ioctl,                  // dev.c
    syscall_fcntl,                  // fcntl.c
    __SYSCALL_NOSYS,                // mpx - unimplemented in Linux
    syscall_setpgid,                // ids.c
    syscall_ulimit,                 // rlimit.c
    syscall_uname,
    syscall_umask,
    syscall_chroot,                 // chdir.c
    syscall_ustat,                  // statfs.c
    syscall_dup2,                   // dup.c
    syscall_getppid,                // ids.c
    syscall_getpgrp,                // ids.c
    syscall_setsid,                 // ids.c
    syscall_sigaction,
    __SYSCALL_NOSYS,                // sgetmask - obsolete Linux syscall
    __SYSCALL_NOSYS,                // ssetmask - obsolete Linux syscall
    syscall_setreuid,               // ids.c
    syscall_setregid,               // ids.c
    syscall_sigsuspend,             // signal.c
    syscall_sigpending,             // signal.c
    syscall_sethostname,
    syscall_setrlimit,              // rlimit.c
    syscall_getrlimit,              // rlimit.c
    syscall_getrusage,              // rlimit.c
    syscall_gettimeofday,
    syscall_settimeofday,
    syscall_getgroups,              // groups.c
    syscall_setgroups,              // groups.c
    syscall_select,                 // select.c
    syscall_symlink,                // symlink.c
    syscall_stat,                   // stat.c
    syscall_readlink,               // symlink.c
    __SYSCALL_NOSYS,                // uselib - obsolete Linux syscall
    syscall_swapon,                 // swap.c - TODO
    syscall_reboot,                 // reboot.c
    __SYSCALL_NOSYS,                // readdir - obsolete Linux syscall
    syscall_mmap,                   // mmap.c
    syscall_munmap,                 // mmap.c
    syscall_truncate,               // truncate.c
    syscall_ftruncate,              // truncate.c
    syscall_fchmod,                 // chmod.c
    syscall_fchown,                 // chown.c
    syscall_getpriority,            // nice.c
    syscall_setpriority,            // nice.c
    __SYSCALL_NOSYS,                // profil - unimplemented in Linux
    syscall_statfs,                 // statfs.c
    syscall_fstatfs,                // statfs.c
    __SYSCALL_NOSYS,                // ioperm - TODO
    syscall_socketcall,             // socket.c
    __SYSCALL_NOSYS,                // syslog - TODO
    syscall_setitimer,              // itimer.c
    syscall_getitimer,              // itimer.c
    syscall_stat,                   // stat.c
    syscall_lstat,                  // stat.c
    syscall_fstat,                  // stat.c
    syscall_uname,
    __SYSCALL_NOSYS,                // iopl - TODO
    syscall_vhangup,                // tty.c
    syscall_idle,                   // idle.c
    __SYSCALL_NOSYS,                // vm86old - TODO
    syscall_wait4,                  // wait.c
    syscall_swapoff,                // swap.c - TODO
    syscall_sysinfo,                // sysinfo.c
    syscall_ipc,                    // ipc.c
    syscall_fsync,                  // fsync.c
    syscall_sigreturn,              // signal.c
    syscall_clone,                  // kfork.c
    syscall_setdomainname,
    syscall_uname,
    __SYSCALL_NOSYS,                // modify_ldt - TODO
    __SYSCALL_NOSYS,                // adjtimex - TODO
    syscall_mprotect,               // mmap.c
    syscall_sigprocmask,            // signal.c
    __SYSCALL_NOSYS,                // create_module - obsolete Linux syscall
    syscall_init_module,            // init_module.c
    syscall_delete_module,          // delete_module.c
    __SYSCALL_NOSYS,                // get_kernel_syms - obsolete Linux syscall
    __SYSCALL_NOSYS,                // quotactl - TODO
    syscall_getpgid,                // ids.c
    syscall_fchdir,                 // chdir.c
    //syscall_vdso_shared_page,       // bdflush - obsolete Linux syscall
    __SYSCALL_NOSYS,                // bdflush - obsolete Linux syscall
    syscall_sysfs,                  // fstab.c
    __SYSCALL_NOSYS,                // personality
    __SYSCALL_NOSYS,                // afs_syscall - unimplemented in Linux
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // _llseek - TODO
    syscall_getdents,
    syscall_select,                 // select.c
    syscall_flock,                  // flock.c
    syscall_msync,                  // memregion.c
    syscall_readv,                  // read.c
    syscall_writev,                 // write.c
    syscall_getsid,                 // ids.c
    syscall_fdatasync,              // fsync.c
    syscall_sysctl,                 // kern_sysctl.c
    syscall_mlock,                  // mlock.c
    syscall_munlock,                // mlock.c
    syscall_mlockall,               // mlock.c
    syscall_munlockall,             // mlock.c
    syscall_sched_setparam,         // sched.c
    syscall_sched_getparam,         // sched.c
    syscall_sched_setscheduler,     // sched.c
    syscall_sched_getscheduler,     // sched.c
    syscall_sched_yield,            // sched.c
    syscall_sched_get_priority_max, // sched.c
    syscall_sched_get_priority_min, // sched.c
    syscall_sched_rr_get_interval,  // sched.c
    syscall_nanosleep,              // clock.c
    syscall_mremap,                 // mmap.c
    syscall_setresuid,              // ids.c
    syscall_getresuid,              // ids.c
    __SYSCALL_NOSYS,                // vm86 - TODO
    __SYSCALL_NOSYS,                // query_module - TODO
    syscall_poll,                   // poll.c
    __SYSCALL_NOSYS,                // nfsservctl - unimplemented in Linux
    syscall_setresgid,              // ids.c
    syscall_getresgid,              // ids.c
    __SYSCALL_NOSYS,                // prctl - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_sigtimedwait,           // signal.c
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_pread,                  // read.c
    syscall_pwrite,                 // write.c
    syscall_chown,                  // chown.c
    syscall_getcwd,
    __SYSCALL_NOSYS,                // capget
    __SYSCALL_NOSYS,                // capset
    syscall_signaltstack,           // signal.c
    __SYSCALL_NOSYS,                // sendfile - TODO
    __SYSCALL_NOSYS,                // getpmsg - unimplemented in Linux
    __SYSCALL_NOSYS,                // putmsg - unimplemented in Linux
    syscall_fork,       // fork will handle vfork as well
    __SYSCALL_NOSYS,                // ugetrlimit ??
    __SYSCALL_NOSYS,                // mmap2 - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_lchown,                 // chown.c
    syscall_getuid,                 // ids.c
    syscall_getgid,                 // ids.c
    syscall_geteuid,                // ids.c
    syscall_getegid,                // ids.c
    syscall_setreuid,               // ids.c
    syscall_setregid,               // ids.c
    syscall_getgroups,              // groups.c
    syscall_setgroups,              // groups.c
    syscall_fchown,                 // chown.c
    syscall_setresuid,              // ids.c
    syscall_getresuid,              // ids.c
    syscall_setresgid,              // ids.c
    syscall_getresgid,              // ids.c
    syscall_chown,                  // chown.c
    syscall_setuid,                 // ids.c
    syscall_setgid,                 // ids.c
    __SYSCALL_NOSYS,                // setfsuid - TODO
    __SYSCALL_NOSYS,                // setfsgid - TODO
    __SYSCALL_NOSYS,                // pivot_root - TODO
    syscall_mincore,                // mmap.c
    __SYSCALL_NOSYS,                // madvise - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // unimplemented in Linux
    __SYSCALL_NOSYS,                // unimplemented in Linux
    syscall_gettid,                 // thread.c
    __SYSCALL_NOSYS,                // readahead - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // tkill - obsolete Linux syscall
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // sched_setaffinity - TODO
    __SYSCALL_NOSYS,                // sched_getaffinity - TODO
    syscall_set_thread_area,        // gdt.c
    syscall_get_thread_area,        // gdt.c
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // unimplemented in Linux
    syscall_exit_group,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_timer_create,           // posix_timers.c
    syscall_timer_settime,          // posix_timers.c
    syscall_timer_gettime,          // posix_timers.c
    syscall_timer_getoverrun,       // posix_timers.c
    syscall_timer_delete,           // posix_timers.c
    syscall_clock_settime,          // clock.c
    syscall_clock_gettime,          // clock.c
    syscall_clock_getres,           // clock.c
    syscall_clock_nanosleep,        // clock.c
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_tgkill,                 // thread.c
    syscall_utimes,                 // utime.c
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // vserver - unimplemented in Linux
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // kexec_load - TODO
    syscall_waitid,                 // wait.c
    __SYSCALL_NOSYS,                // unimplemented in Linux
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_openat,                 // open.c
    syscall_mkdirat,                // mkdir.c
    syscall_mknodat,
    syscall_fchownat,               // chown.c
    syscall_futimesat,              // utime.c
    syscall_fstatat,                // stat.c
    syscall_unlinkat,               // link.c
    syscall_renameat,               // rename.c
    syscall_linkat,                 // link.c
    syscall_symlinkat,              // symlink.c
    syscall_readlinkat,             // symlink.c
    syscall_fchmodat,               // chmod.c
    syscall_faccessat,              // access.c
    syscall_pselect,                // select.c
    syscall_ppoll,                  // poll.c
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // splice - TODO
    __SYSCALL_NOSYS,                // sync_file_range - TODO
    __SYSCALL_NOSYS,                // tee - TODO
    __SYSCALL_NOSYS,                // vmsplice - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_utimensat,              // utimensat - TODO
    __SYSCALL_NOSYS,                // signalfd - TODO
    __SYSCALL_NOSYS,                // timerfd_create - TODO
    __SYSCALL_NOSYS,                // eventfd - TODO
    __SYSCALL_NOSYS,                // fallocate - TODO
    __SYSCALL_NOSYS,                // timerfd_settime - TODO
    __SYSCALL_NOSYS,                // timerfd_gettime - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_dup3,                   // dup.c
    syscall_pipe2,                  // pipe.c
    __SYSCALL_NOSYS,
    syscall_preadv,                 // read.c
    syscall_pwritev,                // write.c
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // recvmmsg - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_prlimit,                // rlimit.c
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // clock_adjtime - TODO
    syscall_syncfs,                 // fsync.c
    __SYSCALL_NOSYS,                // sendmmsg - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,                // process_vm_readv - TODO
    __SYSCALL_NOSYS,                // process_vm_writev - TODO
    __SYSCALL_NOSYS,                // kcmp - TODO
    __SYSCALL_NOSYS,                // finit_module - TODO
    __SYSCALL_NOSYS,                // sched_setattr - TODO
    __SYSCALL_NOSYS,                // sched_getattr - TODO
    __SYSCALL_NOSYS,                // renameat2 - TODO
    __SYSCALL_NOSYS,
    syscall_getrandom,
    __SYSCALL_NOSYS,                // memfd_create - TODO
    __SYSCALL_NOSYS,
    syscall_execveat,               // execve.c
    syscall_socket,                 // socket.c
    syscall_socketpair,             // socket.c
    syscall_bind,                   // socket.c
    syscall_connect,                // socket.c
    syscall_listen,                 // socket.c
    syscall_accept,                 // socket.c
    syscall_getsockopt,             // socket.c
    syscall_setsockopt,             // socket.c
    syscall_getsockname,            // socket.c
    syscall_getpeername,            // socket.c
    syscall_sendto,                 // socket.c
    syscall_sendmsg,                // socket.c
    syscall_recvfrom,               // socket.c
    syscall_recvmsg,                // socket.c
    syscall_shutdown,               // socket.c
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    syscall_mlock2,                 // mlock.c
    __SYSCALL_NOSYS,                // copy_file_range - TODO
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,
    __SYSCALL_NOSYS,

    syscall_msgget,
    syscall_msgsnd,
    syscall_msgrcv,
    syscall_msgctl,
    
    syscall_semget,
    syscall_semop,
    syscall_semctl,

    syscall_shmat,
    syscall_shmctl,
    syscall_shmdt,
    syscall_shmget,
};

#define __NR_SYSCALLS     sizeof(syscalls)/sizeof(void *)
unsigned int NR_SYSCALLS = 0;

struct
{
    long hits;
    unsigned long long ticks;
} syscall_profiles[__NR_SYSCALLS] = { 0, };


/*
 * Initialize syscalls.
 */
void syscall_init(void)
{
    NR_SYSCALLS = __NR_SYSCALLS;

#ifdef __x86_64__

    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry64);
    wrmsr(IA32_STAR, ((uint64_t)0x08 << 32) | (uint64_t)(0x2b - 0x10) << 48);
    wrmsr(IA32_FMASK, 0x700);   // clear TF/DF/IF on syscall entry

#else

    //install_isr(0x80, 0xEE, 0x08, syscall_dispatcher);
    install_isr(0x80, 0xEE, 0x08, syscall_entry);

#endif

}


static long syscall_nosys(void)
{
    //printk("SYSCALL NOSYS:\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    return -ENOSYS;
}


/*
 * Helper functions.
 */

static inline void __set_syscall_flags(struct task_t *ct)
{
    update_task_times(ct);

    /* if user task, mark as running in kernel mode */
    ct->user_in_kernel_mode = 1;

    /* and mark it as running a syscall, so that if an interrupt happens, we
     * don't try to process signals as we will do that after coming back from
     * the syscall.
     */
    __sync_or_and_fetch(&ct->properties, PROPERTY_IN_SYSCALL);
}


static inline void __unset_syscall_flags(struct task_t *ct)
{
    update_task_times(ct);

    /* if user task, mark as running in kernel mode */
    ct->user_in_kernel_mode = 0;

    /* and mark it as running a syscall, so that if an interrupt happens, we
     * don't try to process signals as we will do that after coming back from
     * the syscall.
     */
    __sync_and_and_fetch(&ct->properties, ~PROPERTY_IN_SYSCALL);
}


void set_syscall_flags(struct task_t *ct)
{
    __set_syscall_flags(ct);
}


void unset_syscall_flags(struct task_t *ct)
{
    __unset_syscall_flags(ct);
}


/*
 * If the PTRACE_O_TRACESYSGOOD option is set, the ptrace manpage says:
 *    When delivering system call traps, set bit 7 in the signal number
 *    (i.e., deliver SIGTRAP|0x80). This makes it easy for the tracer to
 *    distinguish normal traps from those caused by a system call.
 */

#define SIGNAL(reason)                                                      \
{                                                                           \
    int sig = SIGTRAP |                                                     \
                ((ct->ptrace_options & PTRACE_O_TRACESYSGOOD) ? 0x80 : 0);  \
    ptrace_signal(sig, reason);                                             \
}

#define MAY_CHECK_SIGNALS(ct, r)                            \
    if(!ksigisemptyset((sigset_t *)&ct->signal_pending))    \
        check_pending_signals(r);


/*
 * Syscall dispatcher.
 */
void syscall_dispatcher(struct regs *r)
{
    struct task_t *ct = (struct task_t *)this_core->cur_task;
    syscall_func func;
    long res;
    unsigned syscall_num = GET_SYSCALL_NUMBER(r);
    unsigned long long oticks = ticks;
    
    //if(ct->pid >= 16) printk("syscall_dispatcher: syscall %d\n", GET_SYSCALL_NUMBER(r));

    if(syscall_num >= (unsigned)NR_SYSCALLS)
    {
        /* Kill task with SIGSYS signal */
        user_add_task_signal(ct, SIGSYS, 1);
        MAY_CHECK_SIGNALS(ct, r);

        /* We came back, that means the task handled the signal.
         * In this case, return -ENOSYS to the caller.
         */
        SET_SYSCALL_RESULT(r, -ENOSYS);
        return;
    }
    
    syscall_profiles[syscall_num].hits++;
    __set_syscall_flags(ct);
    //ct->syscall_regs = r;

    /* enable interrupts, so that if the syscall takes too long, we don't 
     * end up with a spurious interrupt because we missed something like the
     * timer interrupt.
     */
    sti();

    /* notify the tracer (if any) */
    if(ct->properties & PROPERTY_TRACE_SYSEMU)
    {
        SIGNAL(PTRACE_EVENT_SYSCALL_ENTER);
        goto skip;
    }

    if(ct->properties & PROPERTY_TRACE_SYSCALLS)
    {
        SIGNAL(PTRACE_EVENT_SYSCALL_ENTER);
        
        // the tracer injected a bogus syscall
        if(GET_SYSCALL_NUMBER(r) == 0)
        {
            SET_SYSCALL_RESULT(r, -ENOSYS);
            goto skip;
        }
    }
    
    /* do the syscall */
    func = syscalls[syscall_num];
    __atomic_store_n(&(this_core->cur_task->interrupted_syscall), 0, __ATOMIC_SEQ_CST);

    if(syscall_num == __NR_sigreturn)
    {
        // Don't store the return value of sigreturn as the upper bytes of 
        // rax will be chopped to an int, and this syscall doesn't return
        // anyway to where it was called from.
        func(r, GET_SYSCALL_ARG1(r));
    }
    else
    {
        // These syscalls need the current registers to work properly. We used
        // to store the current syscall registers in the task struct but this
        // led to problems with nested syscalls/irqs.
        // TODO: find a cleaner way to avoid this unnecessary if/else.
        if(syscall_num == __NR_fork || syscall_num == __NR_vfork ||
           syscall_num == __NR_clone || syscall_num == __NR_pause || 
           syscall_num == __NR_sigsuspend)
        {
            res = func(r, GET_SYSCALL_ARG1(r));
        }
        else
        {
            res = func(GET_SYSCALL_ARG1(r), GET_SYSCALL_ARG2(r), GET_SYSCALL_ARG3(r),
                       GET_SYSCALL_ARG4(r), GET_SYSCALL_ARG5(r));
        }
    
        if(res == -ERESTARTSYS)
        {
            __atomic_store_n(&(this_core->cur_task->interrupted_syscall), syscall_num, __ATOMIC_SEQ_CST);
            //ct->interrupted_syscall = syscall_num;
            res = -EINTR;
        }

        SET_SYSCALL_RESULT(r, res);
    }

skip:

    /* notify the tracer (if any) */
    if(ct->properties & PROPERTY_TRACE_SYSCALLS)
    {
        SIGNAL(PTRACE_EVENT_SYSCALL_EXIT);
    }

    
    //ct->regs = NULL;
    syscall_profiles[syscall_num].ticks += ticks - oticks;
    
    /* check for signals */
    
    /* idle_task can't receive signals */
    //if(ct != idle_task)
    if(syscall_num != __NR_sigreturn &&
      !(ct->properties & PROPERTY_HANDLING_SIG))
    {
        /* TODO: don't check signals if code segment was supervisor */

        /* check signals */
        MAY_CHECK_SIGNALS(ct, r);
    }

    cli();
    __unset_syscall_flags(ct);
}


/*
 * Helper function to check whether the current user has access to the
 * given file node. If user_ruid is set, the caller's REAL uid/gid are used
 * instead of their EFFECTIVE uid/gid.
 */
long has_access(struct fs_node_t *node, int mode, int use_ruid)
{
    //printk("has_access: node->mode 0x%x, mode 0x%x\n", (node->mode & 0777), mode);
    
	int res = node->mode & 0777;
	uid_t uid = use_ruid ? this_core->cur_task->uid : this_core->cur_task->euid;
    struct mount_info_t *dinfo;

    if(!node /* || node->links == 0 */)    // deleted file - no access whatsoever
    {
        return -EINVAL;
    }

    /* if superuser, we may grant all permissions except for EXEC where at 
     * least one exec bit must be set
     */
	if(suser(this_core->cur_task))
	{
		if(res & 0111)
		{
			res = 0777;
		}
		else
		{
			res = 0666;
		}
	}
	
	if(uid == node->uid)
	{
	    // this is the owner -- check user bits
		res >>= 6;
	}
	else if(gid_perm(node->gid, use_ruid))
	{
		res >>= 3;
	}
	
	mode &= 0007;
	res &= 0007;

    if((dinfo = node_mount_info(node)))
    //if((dinfo = get_mount_info(node->dev)))
    {
        // can't grant write access if the filesystem was mount readonly
        if((mode & WRITE) && (dinfo->mountflags & MS_RDONLY))
        {
            KDEBUG("has_access: mounted MS_RDONLY\n");
            return -EROFS;
        }

        // can't grant execute access if the filesystem was mount MS_NOEXEC
        if((mode & EXECUTE) && (dinfo->mountflags & MS_NOEXEC))
        {
            KDEBUG("has_access: mounted MS_NOEXEC\n");
            return -EACCES;
        }
    }
	
	//printk("has_access: mode 0x%x & res 0x%x = 0x%x\n", mode, res, (res & mode));

	return ((res & mode) == mode) ? 0 : -EACCES;
}


/*
 * Handler for syscall exit().
 */
long syscall_exit(int code)
{
    //printk("syscall_exit: code %d\n", code);
    terminate_task(__W_EXITCODE(code, 0));
    return -1;
}


/*
 * Handler for syscall exit_group().
 */
long syscall_exit_group(int code)
{
    //printk("syscall_exit_group: code %d\n", code);
    terminate_thread_group(__W_EXITCODE(code, 0));
    return -1;
}


/*
 * Handler for syscall close().
 */
long syscall_close(int fd)
{
	struct file_t *f = NULL;
    struct fs_node_t *node;

    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }
	
	cloexec_clear(this_core->cur_task, fd);

    remove_task_locks((struct task_t *)this_core->cur_task, f);

	this_core->cur_task->ofiles->ofile[fd] = NULL;
	
	return closef(f);
}


/*
 * Handler for syscall creat().
 */
long syscall_creat(char *pathname, mode_t mode)
{
	return syscall_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}


/*
 * Handler for syscall time().
 */
long syscall_time(time_t *tloc)
{
    time_t i = now();

    if(!tloc)
    {
        return -EFAULT;
    }

    //return copy_to_user(tloc, &i, sizeof(time_t));
    COPY_VAL_TO_USER(tloc, &i);
    return 0;
}


/*
 * Handler for syscall mknodat().
 *
 * TODO: haven't been tested.
 *
 * See: https://man7.org/linux/man-pages/man2/mknod.2.html
 */
long syscall_mknodat(int dirfd, char *pathname, mode_t mode, dev_t dev)
{
#define open_flags      OPEN_USER_CALLER | OPEN_NOFOLLOW_SYMLINK

	struct fs_node_t *node;
	long res = 0;

    if((res = vfs_mknod(pathname, mode, dev, dirfd, open_flags, &node)) == 0)
    {
        // dirty flag was set by vfs_open()
        release_node(node);
    }

    return res;

#undef open_flags
}


/*
 * Handler for syscall mknod().
 */
long syscall_mknod(char *pathname, mode_t mode, dev_t dev)
{
    return syscall_mknodat(AT_FDCWD, pathname, mode, dev);
}


/*
 * Handler for syscall lseek().
 */
long syscall_lseek(int fd, off_t offset, int origin)
{
    struct file_t *f;
    struct fs_node_t *node;
    long tmp;

    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(IS_PIPE(node))
    {
        return -ESPIPE;
    }
    
    /*
     * NOTE: lseek manpage says:
     *       "On Linux, using lseek() on a terminal device fails with the 
     *        error ESPIPE."
     */

    switch (origin)
    {
        case SEEK_SET:
            if(offset < 0)
            {
                return -EINVAL;
            }
            
            f->pos = offset;
            break;
            
        case SEEK_CUR:
            if(f->pos + offset < 0)
            {
                return -EINVAL;
            }
            
            f->pos += offset;
            break;
            
        case SEEK_END:
            tmp = node->size + offset;

            if(tmp < 0)
            {
                return -EINVAL;
            }
            
            f->pos = tmp;
            break;
            
        default:
            return -EINVAL;
    }
    
    return f->pos;
}


/*
 * Handler for syscall mount().
 */
long syscall_mount(char *source, char *target, char *fstype, 
                   int flags, char *options)
{
    long res;
    dev_t dev = 0;

    if(this_core->cur_task->euid != 0)
    {
        return -EPERM;
    }
    
    if(!source || !*source || !target || !*target || !fstype || !*fstype)
    {
        return -EINVAL;
    }

    if((res = vfs_path_to_devid(source, fstype, &dev)) < 0)
    {
        return res;
    }
    
    res = vfs_mount(dev, target, fstype, flags, options);

    return res;
}


/*
 * Handler for syscall umount2().
 *
 * TODO: Add support to umount2() flags.
 *       See: https://man7.org/linux/man-pages/man2/umount2.2.html
 */
long syscall_umount2(char *target, int user_flags)
{
    long res;
    struct fs_node_t *fnode = NULL;
    dev_t dev;
    int flags = O_RDONLY | ((user_flags & UMOUNT_NOFOLLOW) ? O_NOFOLLOW : 0);
	int open_flags = OPEN_USER_CALLER | OPEN_NOFOLLOW_MPOINT;

    if(!suser(this_core->cur_task))
    {
        return -EPERM;
    }

    if(!target || !*target)
    {
        return -EINVAL;
    }
    
    if((res = vfs_open(target, flags, 0777, AT_FDCWD, &fnode, open_flags)) < 0)
    {
        return res;
    }

    if(!fnode)
    {
        return -ENOENT;
    }

    if(has_access(fnode, READ | WRITE, 0) != 0)
    {
        release_node(fnode);
        return -EPERM;
    }
    
    // check if this is the target mount point or the source device node
    if(S_ISDIR(fnode->mode))
    {
        if(!(fnode->flags & FS_NODE_MOUNTPOINT))
        {
            release_node(fnode);
            return -EINVAL;
        }

        dev = fnode->ptr->dev;
    }
    else
    {
        if(!S_ISBLK(fnode->mode))
        {
            release_node(fnode);
            return -ENOTBLK;
        }
    
        dev = (dev_t)fnode->blocks[0];
    }

    if(MAJOR(dev) >= NR_DEV)
    {
        release_node(fnode);
        return -ENXIO;
    }

    release_node(fnode);

    return vfs_umount(dev, user_flags);
}


/*
 * Handler for syscall umount().
 */
long syscall_umount(char *target)
{
    return syscall_umount2(target, 0);
}


/*
 * Handler for syscall stime().
 */
long syscall_stime(long *buf)
{
    /*
     * NOTE: This function is deprecated. See 'man stime'.
     */
    UNUSED(buf);
    return -ENOSYS;
}


/*
 * Handler for syscall pause().
 *
 * pause() causes the calling process (or thread) to sleep until a signal is
 * delivered that either terminates the process or causes the invocation of a
 * signal-catching function.
 *
 * Returns -EINTR if the task is not terminated by a signal.
 */
long syscall_pause(struct regs *r)
{
    while(1)
    {
    	struct task_t *ct = (struct task_t *)this_core->cur_task;
        volatile sigset_t ocaught, ncaught;
        int signum;
        
        block_task(ct, 1);

        ksigemptyset((sigset_t *)&ocaught);
        ksigorset((sigset_t *)&ocaught, (sigset_t *)&ocaught, 
                                                    &ct->signal_caught);
        check_pending_signals(r);
        //ksigemptyset(&ncaught);
        ksignotset((sigset_t *)&ncaught, (sigset_t *)&ocaught);
        ksigandset((sigset_t *)&ncaught, (sigset_t *)&ncaught, 
                                                    &ct->signal_caught);

        for(signum = 1; signum < NSIG; signum++)
        {
            if(ksigismember((sigset_t *)&ncaught, signum))
            {
                struct sigaction *action = &ct->sig->signal_actions[signum];
                
                if(action->sa_handler != SIG_IGN && 
                   action->sa_handler != SIG_DFL)
                {
                    return -EINTR;
                }
            }
        }
    }
    
    KDEBUG("syscall_pause: done\n");
    return -EINTR;
}


/*
 * Handler for syscall rmdir().
 */
long syscall_rmdir(char *pathname)
{
    return vfs_rmdir(AT_FDCWD, pathname, 0);
}


/*
 * Handler for syscall times().
 */
long syscall_times(struct tms *buf)
{
    struct tms buf2;
    
    if(!buf)
    {
        return ticks;
    }

    buf2.tms_utime = this_core->cur_task->user_time;
    buf2.tms_stime = this_core->cur_task->sys_time;
    buf2.tms_cutime = this_core->cur_task->children_user_time;
    buf2.tms_cstime = this_core->cur_task->children_sys_time;
    COPY_TO_USER(buf, &buf2, sizeof(struct tms));

    return ticks;
}


/*
 * Handler for syscall setheap().
 */
long syscall_setheap(void *data_end)
{
    if((uintptr_t)data_end < 0x100000 || (uintptr_t)data_end >= USER_MEM_END)
    {
        /*
        printk("syscall_setheap: EINVAL - addr 0x%x\n", data_end);
        screen_refresh(NULL);
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        */
        return -EINVAL;
    }

    /*
     * Find the memory region containing the task's current brk.
     * If the brk is page-aligned, we look for the address one page below it,
     * as a page-aligned brk will fall at the higher end of the memory region,
     * and memregion_containing() will not find it as it looks for an address
     * range between the given address and the address + PAGE_SIZE.
     */
    struct memregion_t *memregion = memregion_containing(this_core->cur_task,
                                        (PAGE_ALIGNED((virtual_addr)data_end) ?
                                         ((virtual_addr)data_end - PAGE_SIZE) :
                                          (virtual_addr)data_end));

    if(!memregion)
    {
        /*
        printk("syscall_setheap: EFAULT - addr 0x%x\n", data_end);
        screen_refresh(NULL);
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        */
        return -EFAULT;
    }

    this_core->cur_task->end_data = (uintptr_t)data_end;

    return 0;
}


/*
 * Handler for syscall brk().
 */
long syscall_brk(long incr, volatile uintptr_t *res)
{
	struct task_t *t = (struct task_t *)this_core->cur_task;
    uintptr_t old_end_data = t->end_data;
    uintptr_t end_data_seg = t->end_data + incr;

    /*
     * Find the memory region containing the task's current brk.
     * If the brk is page-aligned, we look for the address one page below it,
     * as a page-aligned brk will fall at the higher end of the memory region,
     * and memregion_containing() will not find it as it looks for an address
     * range between the given address and the address + PAGE_SIZE.
     */
    struct memregion_t *memregion = memregion_containing(t,
                                        (virtual_addr)
                                            (PAGE_ALIGNED(t->end_data) ?
                                             (t->end_data - PAGE_SIZE) :
                                              t->end_data));
    
    /*
     * If the caller asked for memory (i.e. incr != 0), we try to allocate
     * memory. If we can't, because the data segment is about to collide with
     * the stack, we return error. If incr == 0, we simply return the current
     * brk address.
     */

    if(incr > 0)
    {
        // if the new size is not page-aligned, make it so
        uintptr_t end = align_up(end_data_seg);

        uintptr_t private_flag = (memregion &&
                                (memregion->flags & MEMREGION_FLAG_PRIVATE)) ?
                                                I86_PTE_PRIVATE : 0;

        if(end_data_seg >= t->end_stack)
        {
            kpanic("syscall_brk: ENOMEM (1)\n");
            return -ENOMEM;
        }

        if(exceeds_rlimit(t, RLIMIT_DATA, (end - task_get_data_start(t))))
        {
            kpanic("syscall_brk: ENOMEM (2)\n");
            return -ENOMEM;
        }
        
        // now alloc memory for the new pages, starting from the current
        // brk (aligned to the nearest lower page size), up to the new
        // brk address.
        uintptr_t i;
        int err = 0;
        pt_entry *pt;
        
        for(i = align_down(t->end_data); i < end; i += PAGE_SIZE)
        {
            pt = get_page_entry((void *)i);
            
            if(!pt)
            {
                err = 1;
                break;
            }
            
            if(!PTE_PRESENT(*pt))
            {
                //pt = get_page_entry((void *)i);

                if(!vmmngr_alloc_page(pt, PTE_FLAGS_PWU | private_flag))
                {
                    err = 1;
                    break;
                }

                vmmngr_flush_tlb_entry(i);
                A_memset((void *)i, 0, PAGE_SIZE);
            }
        }

        if(err)
        {
            // unmap pages
            for(i = end - PAGE_SIZE;
                i > t->end_data;
                i -= PAGE_SIZE)
            {
                pt_entry *pt = get_page_entry((void *)i);
            
                if(pt)
                {
                    vmmngr_free_page(pt);
                    vmmngr_flush_tlb_entry(i);
                }
            }
        }
        else
        {
            t->end_data = end_data_seg;

            if(memregion)
            {
                memregion->size = (end - memregion->addr) / PAGE_SIZE;
            }
        }
    }
    else if(incr < 0)
    {
        if(end_data_seg < task_get_data_start(t))
        {
            kpanic("syscall_brk: ENOMEM (3)\n");
            return -ENOMEM;
        }

        // if the new size is not page-aligned, make it so
        uintptr_t start = align_up(end_data_seg);
        uintptr_t end = align_up(t->end_data);

        vmmngr_free_pages(start, end - start);
        t->end_data = end_data_seg;

        if(memregion)
        {
            memregion->size = (start - memregion->addr) / PAGE_SIZE;
        }
    }
    
    COPY_VAL_TO_USER(res, &old_end_data);
    return 0;
}


/*
 * Handler for syscall uname().
 */
long syscall_uname(struct utsname *name)
{
    if(!name)
    {
        return -EFAULT;
    }
    
    COPY_TO_USER(name, &myname, sizeof(struct utsname));

    return 0;
}


/*
 * Handler for syscall umask().
 */
long syscall_umask(mode_t mask)
{
    if(!this_core->cur_task || !this_core->cur_task->fs)
    {
        return 0;
    }

    mode_t old = this_core->cur_task->fs->umask;
    this_core->cur_task->fs->umask = mask & 0777;

    return old;
}


/*
 * Handler for syscall setdomainname().
 */
long syscall_setdomainname(char *name, size_t len)
{
	if(!name || !*name || len >= _UTSNAME_LENGTH)
	{
	    return -EINVAL;
	}

    if(!suser(this_core->cur_task))
    {
        return -EPERM;
    }
	
	COPY_FROM_USER(myname.domainname, name, len);
	myname.domainname[len] = '\0';
	
	return 0;
}


/*
 * Handler for syscall sethostname().
 */
long syscall_sethostname(char *name, size_t len)
{
	if(!name || !*name || len >= _UTSNAME_LENGTH)
	{
	    return -EINVAL;
	}

    if(!suser(this_core->cur_task))
    {
        return -EPERM;
    }
	
	COPY_FROM_USER(myname.nodename, name, len);
	myname.nodename[len] = '\0';
	
	return 0;
}


/*
 * Handler for syscall gettimeofday().
 */
long syscall_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    long res;
    struct timespec tstmp;
    struct timeval tvtmp;

    if(!tv)
    {
        return -EFAULT;
    }
    
    if(tz)
    {
        // don't support timezones for now
        return -EINVAL;
    }

    if((res = do_clock_gettime(CLOCK_REALTIME, &tstmp)) < 0)
    {
        return res;
    }

    tvtmp.tv_sec = tstmp.tv_sec;
    tvtmp.tv_usec = tstmp.tv_nsec / NSEC_PER_USEC;

	//return copy_to_user(tv, &tvtmp, sizeof(struct timeval));
    COPY_VAL_TO_USER(&tv->tv_sec, &tvtmp.tv_sec);
    COPY_VAL_TO_USER(&tv->tv_usec, &tvtmp.tv_usec);
    return 0;
}


/*
 * Handler for syscall settimeofday().
 */
long syscall_settimeofday(struct timeval *tv, struct timezone *tz)
{
    struct timeval tmp;
    struct timespec tp;
    
    if(!tv)
    {
        return -EFAULT;
    }

    if(tz)
    {
        // don't support timezones for now
        return -EINVAL;
    }

    if(!suser(this_core->cur_task))
    {
        return -EPERM;
    }

	COPY_FROM_USER(&tmp, tv, sizeof(struct timeval));

    if(tmp.tv_usec < 0 || tmp.tv_usec >= 1000)
    {
        return -EINVAL;
    }
    
    tp.tv_sec = tmp.tv_sec;
    tp.tv_nsec = (tmp.tv_sec * NSEC_PER_USEC);

    return do_clock_settime(CLOCK_REALTIME, &tp);
}


/*
 * Handler for syscall getdents().
 */
long syscall_getdents(int fd, void *dp, int count)
{
	struct file_t *f;
    struct fs_node_t *node;

    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

	if((size_t)f->pos >= node->size)
	{
	    return 0;
	}
	
	return vfs_getdents(node, &f->pos, dp, count);
}


/*
 * Handler for syscall getcwd().
 */
long syscall_getcwd(char *buf, size_t sz)
{
    long res;
    size_t len;
    char *cwd;
    struct fs_node_t *node;

    if(!buf)
    {
        return -EFAULT;
    }
    
    if(!sz)
    {
        return -EINVAL;
    }

    if(!this_core->cur_task->fs || 
       !this_core->cur_task->fs->cwd ||
       this_core->cur_task->fs->cwd->refs == 0)
    {
        return -ENOENT;
    }

    node = this_core->cur_task->fs->cwd;

    if((res = getpath(node, &cwd)) != 0)
    {
        return res;
    }
    
    if((len = strlen(cwd)) >= sz)
    {
        kfree(cwd);
        return -ERANGE;
    }
    
    if(copy_to_user(buf, cwd, len + 1) != 0)
    {
        kfree(cwd);
        return -EFAULT;
    }
    
    kfree(cwd);

    return len;
}


/*
 * Handler for syscall getrandom().
 */
long syscall_getrandom(void *buf, size_t buflen, unsigned int flags, 
                       ssize_t *copied)
{
    void *tmp;
    ssize_t res;
    ssize_t (*func)(dev_t, unsigned char *, size_t);

    if(!buf || !copied)
    {
        return -EFAULT;
    }
    
    // We currently support GRND_RANDOM only.
    // We check for GRND_NONBLOCK although we don't actually block if no
    // random numbers are available due to our current implementation.
    if(flags & ~(GRND_RANDOM | GRND_NONBLOCK))
    {
        return -EINVAL;
    }
    
    if(!(tmp = kmalloc(buflen)))
    {
        return -EAGAIN;
    }
    
    func = (flags & GRND_RANDOM) ? randdev_read : uranddev_read;
    A_memset(tmp, 0, buflen);
    res = (ssize_t)func(0, tmp, buflen);
    
    if(copy_to_user(buf, tmp, res) != 0)
    {
        kfree(tmp);
        return -EFAULT;
    }
    
    kfree(tmp);

    COPY_VAL_TO_USER(copied, &res);
    return 0;
}


/*
 * Read /proc/syscalls.
 */
size_t get_syscalls(char **buf)
{
    size_t len, count = 0, bufsz = 2048;
    char tmp[64];
    char *p;
    size_t i;

    PR_MALLOC(*buf, bufsz);
    p = *buf;
    *p = '\0';

    ksprintf(p, bufsz, "Num      Hits     Ticks\n");
    count = strlen(p);
    p += count;

    for(i = 0; i < NR_SYSCALLS; i++)
    {
        if(syscall_profiles[i].hits == 0)
        {
            continue;
        }

        ksprintf(tmp, 64, "%3d %9ld %9llu\n", i, 
                                              syscall_profiles[i].hits,
                                              syscall_profiles[i].ticks);
        len = strlen(tmp);
        
        if(count + len >= bufsz)
        {
            PR_REALLOC(*buf, bufsz, count);
            p = *buf + count;
        }

        count += len;
        strcpy(p, tmp);
        p += len;
    }

    return count;
}

