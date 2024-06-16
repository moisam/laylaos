/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: strace.c
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
 *  \file strace.c
 *
 *  A debug program used to trace syscalls.
 */

#define _GNU_SOURCE
#include "strace.h"
#include "strace-errno.h"
#include "strace-syscall.h"
#include "strace-sig.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/user.h>
#define KERNEL      // we need this to get the kernel's syscall macros
#include <kernel/syscall.h>

#define DEBUG_PRINT(...)        if(debug_on) fprintf(stderr, __VA_ARGS__)

#define MAX_STRACES         32

struct stracee_t stracee[MAX_STRACES];

char *ver = "1.0";
char *logfilename = NULL;
char *run_as_user = NULL;
int stracee_count = 0;

// output options
int output_separately = 0;
int output_append = 0;
int summary_only = 0;
int follow_forks = 0;
int string_limit = 32;
int arr_limit = 10;
int detach_on = 0;
int debug_on = 0;

// interrupt state options
#define INTERRUPT_ANYWHERE          1
#define INTERRUPT_WAITING           2
#define INTERRUPT_NEVER             3
#define INTERRUPT_NEVER_TSTP        4

int interruptible = 0;

// new environ
char **newenv = NULL;
size_t newenv_size = 8, newenv_index = 0;


#define ASSERT_OPTION_EXISTS(o)     \
    if(!optarg)                     \
        ERR_EXIT("%s: option '--%s' missing argument\n", argv[0], o);


#define SWITCH3(arg, op1, op2, op3)         \
    switch(arg)                             \
    {                                       \
        case op1:                           \
            fprintf(tracee->log, #op1);     \
                break;                      \
        case op2:                           \
            fprintf(tracee->log, #op2);     \
            break;                          \
        case op3:                           \
            fprintf(tracee->log, #op3);     \
            break;                          \
        default:                            \
            print_arg_i(tracee, arg);       \
            break;                          \
    }


void block_fatal_signals(sigset_t *set)
{
    sigemptyset(set);

    sigaddset(set, SIGHUP);
    sigaddset(set, SIGINT);
    sigaddset(set, SIGQUIT);
    sigaddset(set, SIGILL);
    sigaddset(set, SIGTRAP);
    sigaddset(set, SIGABRT);
    sigaddset(set, SIGBUS);
    sigaddset(set, SIGFPE);
    sigaddset(set, SIGKILL);
    sigaddset(set, SIGUSR1);
    sigaddset(set, SIGSEGV);
    sigaddset(set, SIGUSR2);
    sigaddset(set, SIGPIPE);
    sigaddset(set, SIGALRM);
    sigaddset(set, SIGTERM);
    sigaddset(set, SIGSTKFLT);
    sigaddset(set, SIGXCPU);
    sigaddset(set, SIGXFSZ);
    sigaddset(set, SIGVTALRM);
    sigaddset(set, SIGPROF);
    sigaddset(set, SIGIO);
    sigaddset(set, SIGPWR);
    sigaddset(set, SIGSYS);

    if(interruptible == INTERRUPT_NEVER_TSTP)
    {
        sigaddset(set, SIGTSTP);
    }

    sigprocmask(SIG_BLOCK, set, NULL);
}


void unblock_fatal_signals(sigset_t *set)
{
    sigprocmask(SIG_UNBLOCK, set, NULL);
}


void maybe_error(struct stracee_t *tracee, struct user_regs_struct *regs)
{
    int res = GET_SYSCALL_RESULT(regs);
    const char *str;

    fprintf(tracee->log, ") = %d", res);
    
    if(res < 0)
    {
        res = -res;
        str = (res < errno_count) ? errno_names[res] : NULL;
        
        if(str)
        {
            fprintf(tracee->log, " %s (%s)", str, strerror(res));
        }
        else
        {
            fprintf(tracee->log, " %d (%s)", res, strerror(res));
        }
    }
    
    fprintf(tracee->log, "\n");
}


void syscall_handle(struct stracee_t *tracee, struct user_regs_struct *regs)
{
    int sys = GET_SYSCALL_NUMBER(regs);

    if(sys < 0 || sys >= syscall_name_count)
    {
        return;
    }
    
    if(!syscall_inject_mask || !syscall_inject_mask[sys].inject)
    {
        return;
    }

    // inject a syscall
    if(syscall_inject_mask[sys].syscall)
    {
        int sysres = syscall_inject_mask[sys].syscall;
        ptrace(PTRACE_SYSCALL, tracee->pid, NULL, &sysres);
    }
    else if(syscall_inject_mask[sys].error || syscall_inject_mask[sys].retval)
    {
        ptrace(PTRACE_SYSCALL, tracee->pid, NULL, NULL);
    }

    // inject a signal
    if(syscall_inject_mask[sys].signum)
    {
        kill(tracee->pid, syscall_inject_mask[sys].signum);
    }
}


#define MAYBE_PRINT_ARG(tracee, regs, func, arg)    \
    if(GET_SYSCALL_RESULT(regs) == 0)               \
        func(tracee, arg);                          \
    else                                            \
        print_arg_ptr(tracee, arg);


void syscall_finish(struct stracee_t *tracee, int sys, struct user_regs_struct *regs)
{
    //int sys = GET_SYSCALL_NUMBER(regs);
    int sysres;

    if(sys < 0 || sys >= syscall_mask_count)
    {
        return;
    }
    
    if(!syscall_mask[sys])
    {
        return;
    }

    if(syscall_inject_mask && syscall_inject_mask[sys].inject)
    {
        if(syscall_inject_mask[sys].retval)
        {
            sysres = syscall_inject_mask[sys].retval;
            SET_SYSCALL_RESULT(regs, sysres);
            ptrace(PTRACE_SYSCALL, tracee->pid, NULL, &sysres);
        }
        else if(syscall_inject_mask[sys].error)
        {
            sysres = -syscall_inject_mask[sys].error;
            SET_SYSCALL_RESULT(regs, sysres);
            ptrace(PTRACE_SYSCALL, tracee->pid, NULL, &sysres);
        }
    }
    
    sysres = GET_SYSCALL_RESULT(regs);

    if(sysres == 0 && !syscall_status_mask[SYSCAL_STATUS_SUCCESSFUL])
    {
        return;
    }

    if(sysres != 0 && sys != __NR_brk && !syscall_status_mask[SYSCAL_STATUS_FAILED])
    {
        return;
    }
    
    fprintf(tracee->log, "%s(", syscall_names[sys]);
    
    switch(sys)
    {
        /*
         * Yet unimplemented syscalls.
         * TODO: update this when we implement them.
         */
        case __NR_ioperm:
        case __NR_syslog:
            break;

        /*
         * Obsolete Linux syscalls.
         */
        case __NR_setup:
        case __NR_break:
        case __NR_gtty:
        case __NR_ftime:
        case __NR_prof:
        case __NR_lock:
        case __NR_mpx:
        case __NR_sgetmask:
        case __NR_ssetmask:
        case __NR_uselib:
        case __NR_readdir:
        case __NR_profil:
            break;

        // int syscall_exit(int code);
        case __NR_exit:
            /* fallthrough */

        // int syscall_exit_group(int code);
        case __NR_exit_group:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            //fprintf(tracee->log, ") = ?\n");
            return;

        // int clone(int flags, void *tls);
        case __NR_clone:
            /*
             * TODO: process and output clone() flags.
             */
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_fork(void);
        case __NR_fork:
        case __NR_vfork:
            break;

        // int syscall_read(int _fd, unsigned char *buf, size_t count, ssize_t *copied);
        case __NR_read:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            
            if(sysres == 0)
            {
                print_arg_buf(tracee, GET_SYSCALL_ARG2(regs),
                              tracee_get_ptr(tracee, GET_SYSCALL_ARG4(regs)));
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            }

            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_write(int _fd, unsigned char *buf, size_t count, ssize_t *copied);
        case __NR_write:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_buf(tracee, GET_SYSCALL_ARG2(regs), GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_open(char *filename, int flags, mode_t mode);
        case __NR_open:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_open_flags(tracee, GET_SYSCALL_ARG2(regs));
            
            if(GET_SYSCALL_ARG2(regs) & O_CREAT)
            {
                fprintf(tracee->log, ", ");
                print_argmode(tracee, GET_SYSCALL_ARG3(regs));
            }

            break;

        // int syscall_close(int fd);
        case __NR_close:
            /* fallthrough */

        // int syscall_dup(int fildes);
        case __NR_dup:
            /* fallthrough */

        // int syscall_fchdir(int fd);
        case __NR_fchdir:
            /* fallthrough */

        // int syscall_fsync(int fd);
        case __NR_fsync:
            /* fallthrough */

        // int syscall_fdatasync(int fd);
        case __NR_fdatasync:
            /* fallthrough */

        // int syscall_syncfs(int fd);
        case __NR_syncfs:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_waitpid(pid_t pid, int *status, int options);
        case __NR_waitpid:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_wait_flags(tracee, GET_SYSCALL_ARG3(regs) | WEXITED | WSTOPPED);
            break;

        // int syscall_link(char *oldname, char *newname);
        case __NR_link:
            /* fallthrough */

        // int syscall_symlink(char *target, char *linkpath);
        case __NR_symlink:
            /* fallthrough */

        // int syscall_rename(char *oldpath, char *newpath);
        case __NR_rename:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_execve(char *path, char **argv, char **env);
        case __NR_execve:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            if(sysres == 0)
            {
                print_arg_strarr(tracee, GET_SYSCALL_ARG2(regs));
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            }
            
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_acct(char *filename);
        case __NR_acct:
            /* fallthrough */

        // int syscall_chdir(char *filename);
        case __NR_chdir:
            /* fallthrough */

        // int syscall_chroot(char *filename);
        case __NR_chroot:
            /* fallthrough */

        // int syscall_rmdir(char *pathname);
        case __NR_rmdir:
            /* fallthrough */

        // int syscall_umount(char *target);
        case __NR_umount:
            /* fallthrough */

        // int syscall_unlink(char *pathname);
        case __NR_unlink:
            /* fallthrough */

        // int syscall_swapoff(char *path);
        case __NR_swapoff:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_time(time_t *tloc);
        case __NR_time:
            if(sysres == 0)
            {
                uintptr_t tloc = tracee_get_ptr(tracee, GET_SYSCALL_ARG1(regs));
                print_arg_i(tracee, tloc);
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            }

            break;

        // int syscall_mknod(char *pathname, mode_t mode, dev_t dev);
        case __NR_mknod:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_argmode(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_dev(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_lchown(char *filename, uid_t uid, gid_t gid);
        case __NR_lchown:
        case __NR_lchown32:
            /* fallthrough */

        // int syscall_chown(char *filename, uid_t uid, gid_t gid);
        case __NR_chown:
        case __NR_chown32:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_stat(char *filename, struct stat *statbuf);
        case __NR_oldstat:
        case __NR_oldlstat:
        case __NR_stat:
        case __NR_lstat:
            /*
             * TODO: dereference and output the contents of statbuf after
             *       the syscall returns successfully.
             */
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_lseek(int fd, off_t offset, int origin);
        case __NR_lseek:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            
            SWITCH3(GET_SYSCALL_ARG3(regs), SEEK_SET, SEEK_CUR, SEEK_END);
            break;

        // int syscall_mount(char *source, char *target, char *fstype,
        //                   int flags, char *options);
        case __NR_mount:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_mount_flags(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_getsid(pid_t pid);
        case __NR_getsid:
            /* fallthrough */

        // int syscall_getpgid(pid_t pid);
        case __NR_getpgid:
            /* fallthrough */

        // int syscall_sched_getscheduler(pid_t pid);
        case __NR_sched_getscheduler:
            /* fallthrough */

        // int syscall_setgid(gid_t gid);
        case __NR_setgid:
        case __NR_setgid32:
            /* fallthrough */

        // int syscall_setuid(uid_t uid);
        case __NR_setuid:
        case __NR_setuid32:
            /* fallthrough */

        // int syscall_alarm(unsigned int seconds);
        case __NR_alarm:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_stime(long *buf);
        case __NR_stime:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_ptrace(int request, pid_t pid, void *addr, void *data);
        case __NR_ptrace:
            print_ptrace_request(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_fstat(int fd, struct stat *statbuf);
        case __NR_oldfstat:
        case __NR_fstat:
            /*
             * TODO: dereference and output the contents of statbuf after
             *       the syscall returns successfully.
             */
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_utime(char *filename, struct utimbuf *times);
        case __NR_utime:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_utimbuf(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_setheap(void *data_end);
        case __NR_setheap:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_access(char *filename, int mode)
        case __NR_access:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_accmode(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_mlockall(int flags);
        case __NR_mlockall:
            /* fallthrough */

        // int syscall_nice(int increment);
        case __NR_nice:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_kill(pid_t pid, int signum);
        case __NR_kill:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_sig(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_creat(char *pathname, mode_t mode);
        case __NR_creat:
            /* fallthrough */

        // int syscall_chmod(char *filename, mode_t mode);
        case __NR_chmod:
            /* fallthrough */

        // int syscall_mkdir(char *pathname, mode_t mode);
        case __NR_mkdir:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_argmode(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_pipe(int *fildes);
        case __NR_pipe:
            if(sysres == 0)
            {
                print_arg_fds(tracee, GET_SYSCALL_ARG1(regs), 2);
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            }
            break;

        // int syscall_times(struct tms *buf);
        case __NR_times:
            MAYBE_PRINT_ARG(tracee, regs, print_arg_tms, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_brk(unsigned long incr);
        case __NR_brk:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            //fprintf(tracee->log, ") = %#0lx\n", GET_SYSCALL_RESULT(regs));
            break;

        // int syscall_signal(int signum, void *handler, void *sa_restorer);
        case __NR_signal:
            print_arg_sig(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_umount2(char *target, int flags);
        case __NR_umount2:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_umount_flags(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_ioctl(int fd, unsigned int cmd, char *arg);
        case __NR_ioctl:
            /* fallthrough */

        // int syscall_fcntl(int fd, int cmd, void *arg);
        case __NR_fcntl:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_setreuid(uid_t ruid, uid_t euid);
        case __NR_setreuid:
        case __NR_setreuid32:
            /* fallthrough */

        // int syscall_setregid(gid_t rgid, gid_t egid);
        case __NR_setregid:
        case __NR_setregid32:
            /* fallthrough */

        // int syscall_setpgid(pid_t pid, pid_t pgid);
        case __NR_setpgid:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_ulimit(int cmd, long newlimit);
        case __NR_ulimit:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_uname(struct utsname *name);
        case __NR_oldolduname:
        case __NR_olduname:
        case __NR_uname:
            MAYBE_PRINT_ARG(tracee, regs, print_utsname, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_umask(mode_t mask);
        case __NR_umask:
            print_argmode(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_ustat(dev_t dev, struct ustat *ubuf);
        case __NR_ustat:
            /*
             * TODO: dereference and output the contents of ustat after
             *       the syscall returns successfully.
             */
            print_arg_dev(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_dup2(int oldfd, int newfd);
        case __NR_dup2:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_fd(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_sigaction(int signum,
        //                       struct sigaction* newact,
        //                       struct sigaction* oldact);
        case __NR_sigaction:
            print_arg_sig(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_sigaction(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_sigaction, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_sigsuspend(sigset_t *set);
        case __NR_sigsuspend:
            print_arg_sigset(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_sigpending(sigset_t* set);
        case __NR_sigpending:
            MAYBE_PRINT_ARG(tracee, regs, print_arg_sigset, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_sethostname(char *name, size_t len);
        case __NR_sethostname:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_setrlimit(int resource, struct rlimit *rlim);
        case __NR_setrlimit:
            print_arg_resource(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_rlimit(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_getrlimit(int resource, struct rlimit *rlim);
        case __NR_getrlimit:
            print_arg_resource(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_rlimit, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_getrusage(int who, struct rusage *r_usage);
        case __NR_getrusage:
            SWITCH3(GET_SYSCALL_ARG1(regs), RUSAGE_SELF, RUSAGE_CHILDREN, RUSAGE_THREAD);
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_rusage, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_gettimeofday(struct timeval *restrict tv,
        //                          struct timezone *restrict tz);
        case __NR_gettimeofday:
            MAYBE_PRINT_ARG(tracee, regs, print_arg_timeval, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_settimeofday(struct timeval *tv, struct timezone *tz);
        case __NR_settimeofday:
            print_arg_timeval(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_getgroups(int gidsetsize, gid_t grouplist[]);
        case __NR_getgroups:
        case __NR_getgroups32:
            /*
             * TODO: dereference and output the group list.
             */
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_setgroups(int ngroups, gid_t grouplist[]);
        case __NR_setgroups:
        case __NR_setgroups32:
            /*
             * TODO: dereference and output the group list.
             */
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_select(u_int n, fd_set *readfds, fd_set *writefds,
        //                    fd_set *exceptfds, struct timeval *timeout);
        case __NR_select:
        case __NR_newselect:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_arg_timeval(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_readlink(char *pathname, char *buf,
        //                      size_t bufsize, ssize_t *__copied);
        case __NR_readlink:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            if(GET_SYSCALL_RESULT(regs) == 0)
            {
                print_arg_buf(tracee, GET_SYSCALL_ARG2(regs),
                              tracee_get_ptr(tracee, GET_SYSCALL_ARG4(regs)));
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            }

            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_swapon(char *path, int swapflags);
        case __NR_swapon:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_reboot(int cmd);
        case __NR_reboot:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_mmap(struct syscall_args *__args);
        case __NR_mmap:
            print_mmap_args(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_mremap(struct syscall_args *__args);
        case __NR_mremap:
            print_mremap_args(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_mlock(void *addr, size_t len);
        case __NR_mlock:
            /* fallthrough */

        // int syscall_munlock(void *addr, size_t len);
        case __NR_munlock:
            /* fallthrough */

        // int syscall_munmap(void *addr, size_t length);
        case __NR_munmap:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_truncate(char *pathname, off_t length);
        case __NR_truncate:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_ftruncate(int fd, off_t length);
        case __NR_ftruncate:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_fchmod(int fd, mode_t mode);
        case __NR_fchmod:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_argmode(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_fchown(int fd, uid_t uid, gid_t gid);
        case __NR_fchown:
        case __NR_fchown32:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_getpriority(int which, id_t who, int *__nice);
        case __NR_getpriority:
            print_arg_prio(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");

            if(GET_SYSCALL_RESULT(regs) == 0)
            {
                uintptr_t prio = tracee_get_ptr(tracee, GET_SYSCALL_ARG3(regs));
                print_arg_i(tracee, prio);
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            }

            break;

        // int syscall_setpriority(int which, id_t who, int value);
        case __NR_setpriority:
            print_arg_prio(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_statfs(char *path, struct statfs *buf);
        case __NR_statfs:
            /*
             * TODO: dereference and output the contents of statfs after
             *       the syscall returns successfully.
             */
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_fstatfs(int fd, struct statfs *buf);
        case __NR_fstatfs:
            /*
             * TODO: dereference and output the contents of statfs after
             *       the syscall returns successfully.
             */
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_socketcall(int call, unsigned long *args);
        case __NR_socketcall:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_setitimer(int which, struct itimerval *value,
        //                       struct itimerval *ovalue);
        case __NR_setitimer:
            print_itimer_id(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_itimerval(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_itimerval, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_getitimer(int which, struct itimerval *value);
        case __NR_getitimer:
            print_itimer_id(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_itimerval, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_wait4(pid_t pid, int *stat_addr,
        //                   int options, struct rusage *rusage);
        case __NR_wait4:
            /*
             * TODO: dereference and output the contents of rusage after
             *       the syscall returns successfully.
             */
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_wait_flags(tracee, GET_SYSCALL_ARG3(regs) | WEXITED | WSTOPPED);
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_sysinfo(struct sysinfo *info);
        case __NR_sysinfo:
            MAYBE_PRINT_ARG(tracee, regs, print_arg_sysinfo, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_ipc(int call, unsigned long *args)
        case __NR_ipc:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_sigreturn(uintptr_t user_stack);
        case __NR_sigreturn:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_setdomainname(char *name, size_t len);
        case __NR_setdomainname:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_mprotect(void *addr, size_t length, int prot);
        case __NR_mprotect:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_prot_flags(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_sigprocmask(int how, sigset_t *userset, sigset_t *oldset);
        case __NR_sigprocmask:
            SWITCH3(GET_SYSCALL_ARG1(regs), SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK);
            fprintf(tracee->log, ", ");
            print_arg_sigset(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_sigset, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_init_module(void *module_image, unsigned long len,
        //                         char *param_values);
        case __NR_init_module:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_delete_module(char *__name, unsigned int flags);
        case __NR_delete_module:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_sysfs(int option, uintptr_t fsid, char *buf);
        case __NR_sysfs:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_getdents(int fd, void *dp, int count);
        case __NR_getdents:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_flock(int fd, int operation);
        case __NR_flock:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            SWITCH3(GET_SYSCALL_ARG2(regs), LOCK_SH, LOCK_EX, LOCK_UN);
            break;

        // int syscall_mlock2(void *addr, size_t len, unsigned int flags);
        case __NR_mlock2:
            /* fallthrough */

        // int syscall_msync(void *addr, size_t length, int flags);
        case __NR_msync:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_readv(int fd, struct iovec *iov, int count, ssize_t *copied);
        case __NR_readv:
            /* fallthrough */

        // int syscall_writev(int fd, struct iovec *iov, int count, ssize_t *copied);
        case __NR_writev:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_sysctl(struct __sysctl_args *__args);
        case __NR_sysctl:
            print_sysctl_args(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_sched_setparam(pid_t pid, struct sched_param *param);
        case __NR_sched_setparam:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_sched_param(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_sched_getparam(pid_t pid, struct sched_param *param);
        case __NR_sched_getparam:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_sched_param, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_sched_setscheduler(pid_t pid, int policy,
        //                                struct sched_param *param);
        case __NR_sched_setscheduler:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_sched_policy(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_sched_param(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_sched_get_priority_max(int policy);
        case __NR_sched_get_priority_max:
            /* fallthrough */

        // int syscall_sched_get_priority_min(int policy);
        case __NR_sched_get_priority_min:
            print_sched_policy(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_sched_rr_get_interval(pid_t pid, struct timespec *tp);
        case __NR_sched_rr_get_interval:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_timespec, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_nanosleep(struct timespec *__rqtp, struct timespec *__rmtp);
        case __NR_nanosleep:
            /*
             * TODO: dereference and output the contents of timespec's after
             *       the syscall returns successfully.
             */
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_setresuid(uid_t newruid, uid_t neweuid, uid_t newsuid);
        case __NR_setresuid:
        case __NR_setresuid32:
            /* fallthrough */

        // int syscall_setresgid(gid_t newrgid, gid_t newegid, gid_t newsgid);
        case __NR_setresgid:
        case __NR_setresgid32:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
        case __NR_getresgid:
        case __NR_getresgid32:
            /* fallthrough */

        // int syscall_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
        case __NR_getresuid:
        case __NR_getresuid32:
            if(GET_SYSCALL_RESULT(regs) == 0)
            {
                uintptr_t id = tracee_get_ptr(tracee, GET_SYSCALL_ARG1(regs));
                print_arg_ui(tracee, id);
                fprintf(tracee->log, ", ");
                id = tracee_get_ptr(tracee, GET_SYSCALL_ARG2(regs));
                print_arg_ui(tracee, id);
                fprintf(tracee->log, ", ");
                id = tracee_get_ptr(tracee, GET_SYSCALL_ARG3(regs));
                print_arg_ui(tracee, id);
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
                fprintf(tracee->log, ", ");
                print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
                fprintf(tracee->log, ", ");
                print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            }

            break;

        // int syscall_poll(struct pollfd *fds, nfds_t nfds, int timeout);
        case __NR_poll:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            break;


        // int syscall_pread(int _fd, void *buf, size_t count,
        //                   off_t offset, ssize_t *copied);
        case __NR_pread:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            
            if(GET_SYSCALL_RESULT(regs) == 0)
            {
                print_arg_buf(tracee, GET_SYSCALL_ARG2(regs),
                              tracee_get_ptr(tracee, GET_SYSCALL_ARG5(regs)));
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            }

            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_pwrite(int fd, void *buf, size_t count,
        //                    off_t offset, ssize_t *copied);
        case __NR_pwrite:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_buf(tracee, GET_SYSCALL_ARG2(regs), GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_getcwd(char *buf, size_t sz);
        case __NR_getcwd:
            if(GET_SYSCALL_RESULT(regs) == 0)
            {
                print_arg_buf(tracee, GET_SYSCALL_ARG1(regs),
                                      GET_SYSCALL_ARG2(regs));
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            }

            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_sigaltstack(stack_t *ss, stack_t *old_ss);
        case __NR_signalstack:
            print_stack_t(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_stack_t, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_mincore(void *addr, size_t length, unsigned char *vec);
        case __NR_mincore:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_set_thread_area(struct user_desc *u_info);
        case __NR_set_thread_area:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_get_thread_area(struct user_desc *u_info);
        case __NR_get_thread_area:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            break;


        // int syscall_timer_create(clockid_t clockid, struct sigevent *sevp,
        //                          timer_t *timerid);
        case __NR_timer_create:
            print_clock_id(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_sigevent(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");

            if(GET_SYSCALL_RESULT(regs) == 0)
            {
                uintptr_t id = tracee_get_ptr(tracee, GET_SYSCALL_ARG3(regs));
                print_arg_i(tracee, id);
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            }

            break;

        // int syscall_timer_settime(timer_t timerid, int flags,
        //                           struct itimerspec *new_value,
        //                           struct itimerspec *old_value);
        case __NR_timer_settime:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_itimerspec(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_itimerspec, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_timer_gettime(timer_t timerid, struct itimerspec *curr_value);
        case __NR_timer_gettime:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_itimerspec, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_timer_getoverrun(timer_t timerid);
        case __NR_timer_getoverrun:
            /* fallthrough */

        // int syscall_timer_delete(timer_t timerid);
        case __NR_timer_delete:
            print_arg_ui(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int  syscall_clock_settime(clockid_t clock_id, struct timespec *tp);
        case __NR_clock_settime:
            print_clock_id(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_itimerspec(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int  syscall_clock_gettime(clockid_t clock_id, struct timespec *tp);
        case __NR_clock_gettime:
            /* fallthrough */

        // int syscall_clock_getres(clockid_t clock_id, struct timespec *res);
        case __NR_clock_getres:
            print_clock_id(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_timespec, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_clock_nanosleep(clockid_t clock_id, int flags, 
        //                             struct timespec *__rqtp,
        //                             struct timespec *__rmtp);
        case __NR_clock_nanosleep:
            /*
             * TODO: dereference and output the contents of timespec's after
             *       the syscall returns successfully.
             */
            print_clock_id(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_clock_flags(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_tgkill(pid_t tgid, pid_t tid, int sig);
        case __NR_tgkill:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_sig(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_utimes(char *filename, struct timeval *times);
        case __NR_utimes:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_timeval(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);
        case __NR_waitid:
            /*
             * TODO: dereference and output the contents of siginfo_t after
             *       the syscall returns successfully.
             */
            SWITCH3(GET_SYSCALL_ARG1(regs), P_ALL, P_PGID, P_PID);
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_wait_flags(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_openat(int dirfd, char *filename, int flags, mode_t mode);
        case __NR_openat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_open_flags(tracee, GET_SYSCALL_ARG3(regs));
            
            if(GET_SYSCALL_ARG3(regs) & O_CREAT)
            {
                fprintf(tracee->log, ", ");
                print_argmode(tracee, GET_SYSCALL_ARG4(regs));
            }

            break;

        // int syscall_mkdirat(int dirfd, char *pathname, mode_t mode);
        case __NR_mkdirat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_argmode(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_mknodat(int dirfd, char *pathname, mode_t mode, dev_t dev);
        case __NR_mknodat:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_argmode(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_dev(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_fchownat(int dirfd, char *pathname,
        //                      uid_t uid, gid_t gid, int flags);
        case __NR_fchownat:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_at_flags(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_futimesat(int dirfd, char *pathname, struct timeval *times);
        case __NR_futimesat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_timeval(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_fstatat(int fd, char *filename,
        //                     struct stat *statbuf, int flags);
        case __NR_fstatat:
            /*
             * TODO: dereference and output the contents of statbuf after
             *       the syscall returns successfully.
             */
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_at_flags(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_unlinkat(int dirfd, char *pathname, int flags);
        case __NR_unlinkat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_at_flags(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_renameat(int olddirfd, char *oldpath,
        //                      int newdirfd, char *newpath);
        case __NR_renameat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_dirfd(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_linkat(int olddirfd, char *oldname,
        //                    int newdirfd, char *newname, int flags);
        case __NR_linkat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_dirfd(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_at_flags(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_symlinkat(char *target, int newdirfd, char *linkpath);
        case __NR_symlinkat:
            print_arg_str(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_dirfd(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_readlinkat(int dirfd, char *pathname, char *buf,
        //                        size_t bufsize, ssize_t *__copied);
        case __NR_readlinkat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");

            if(GET_SYSCALL_RESULT(regs) == 0)
            {
                print_arg_buf(tracee, GET_SYSCALL_ARG3(regs),
                              tracee_get_ptr(tracee, GET_SYSCALL_ARG5(regs)));
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            }

            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_fchmodat(int dirfd, char *pathname, mode_t mode, int flags);
        case __NR_fchmodat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_argmode(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_at_flags(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_faccessat(int dirfd, char *filename, int mode, int flags)
        case __NR_faccessat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_accmode(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_at_flags(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_pselect(struct syscall_args *__args);
        case __NR_pselect:
            print_pselect_args(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_ppoll(struct pollfd *fds, nfds_t nfds,
        //                   struct timespec *tmo_p, sigset_t *sigmask);
        case __NR_ppoll:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_timespec(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_sigset(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_dup3(int oldfd, int newfd, int flags);
        case __NR_dup3:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_fd(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_pipe2(int *fildes, int flags);
        case __NR_pipe2:
            if(GET_SYSCALL_RESULT(regs) == 0)
            {
                print_arg_fds(tracee, GET_SYSCALL_ARG1(regs), 2);
            }
            else
            {
                print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            }

            break;

        // int syscall_preadv(int fd, struct iovec *iov, int count,
        //                    off_t offset, ssize_t *copied);
        case __NR_preadv:
            /* fallthrough */

        // int syscall_pwritev(int fd, struct iovec *iov, int count,
        //                     off_t offset, ssize_t *copied);
        case __NR_pwritev:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_prlimit(pid_t pid, int resource, struct rlimit *new_limit,
        //                     struct rlimit *old_limit);
        case __NR_prlimit:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_resource(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_rlimit(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");

            MAYBE_PRINT_ARG(tracee, regs, print_arg_rlimit, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_getrandom(void *buf, size_t buflen,
        //                       unsigned int flags, ssize_t *copied);
        case __NR_getrandom:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_execveat(int dirfd, char *path, char **argv,
        //                      char **env, int flags);
        case __NR_execveat:
            print_arg_dirfd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_str(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_strarr(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_at_flags(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_socket(int domain, int type, int protocol);
        case __NR_socket:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_socketpair(int domain, int type, int protocol, int *rsv);
        case __NR_socketpair:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_bind(int s, struct sockaddr *name, socklen_t namelen);
        case __NR_bind:
            /* fallthrough */

        // int syscall_connect(int s, struct sockaddr *name, socklen_t namelen);
        case __NR_connect:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_listen(int s, int backlog);
        case __NR_listen:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_accept(int s, struct sockaddr *name, socklen_t *anamelen);
        case __NR_accept:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_getsockopt(int s, int level, int name, void *val, int *avalsize);
        case __NR_getsockopt:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_setsockopt(int s, int level, int name, void *val, int valsize);
        case __NR_setsockopt:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG5(regs));
            break;

        // int syscall_getsockname(int fdes, struct sockaddr *asa, socklen_t *alen);
        case __NR_getsockname:
            /* fallthrough */

        // int syscall_getpeername(int fdes, struct sockaddr *asa, socklen_t *alen);
        case __NR_getpeername:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_sendto(struct syscall_args *__args);
        case __NR_sendto:
            print_sendto_args(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_recvfrom(struct syscall_args *__args);
        case __NR_recvfrom:
            print_recvfrom_args(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_sendmsg(int s, struct msghdr *_msg, int flags);
        case __NR_sendmsg:
            /* fallthrough */

        // int syscall_recvmsg(int s, struct msghdr *_msg, int flags);
        case __NR_recvmsg:
            print_arg_fd(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_shutdown(int s, int how);
        case __NR_shutdown:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            break;

// END of Linux x86 syscall list (last entry is #384)
// The following are LaylaOS-specific syscalls (they exist on non-x86 Linux)

        // int syscall_msgget(key_t key, int msgflg);
        case __NR_msgget:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            break;

        // int syscall_msgsnd(int msqid, void *msgp, size_t msgsz, int msgflg);
        case __NR_msgsnd:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_msgrcv(struct syscall_args *__args);
        case __NR_msgrcv:
            print_msgrcv_args(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_msgctl(int msqid, int cmd, struct msqid_ds *buf);
        case __NR_msgctl:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_semget(key_t key, int nsems, int semflg);
        case __NR_semget:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_semop(int semid, struct sembuf *sops, size_t nsops);
        case __NR_semop:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ui(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_semctl(int semid, int semnum, int cmd, union semun *arg);
        case __NR_semctl:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_shmat(int shmid, void *shmaddr, int shmflg, void **result);
        case __NR_shmat:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG4(regs));
            break;

        // int syscall_shmctl(int shmid, int cmd, struct shmid_ds *buf);
        case __NR_shmctl:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_ptr(tracee, GET_SYSCALL_ARG3(regs));
            break;

        // int syscall_shmdt(void *shmaddr);
        case __NR_shmdt:
            print_arg_ptr(tracee, GET_SYSCALL_ARG1(regs));
            break;

        // int syscall_shmget(key_t key, size_t size, int shmflg);
        case __NR_shmget:
            print_arg_i(tracee, GET_SYSCALL_ARG1(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG2(regs));
            fprintf(tracee->log, ", ");
            print_arg_i(tracee, GET_SYSCALL_ARG3(regs));
            break;

        /*
         * Syscalls that take zero arguments.
         */

        case __NR_getpid:
        case __NR_gettid:
        case __NR_getppid:
        case __NR_getpgrp:
        case __NR_setsid:
        case __NR_getuid:
        case __NR_getgid:
        case __NR_geteuid:
        case __NR_getegid:
        case __NR_getuid32:
        case __NR_getgid32:
        case __NR_geteuid32:
        case __NR_getegid32:
        case __NR_pause:
        case __NR_sync:
        case __NR_vhangup:
        case __NR_idle:
        case __NR_munlockall:
        case __NR_sched_yield:
            break;

        default:
            fprintf(tracee->log, "...");
            break;
    }
    
    if(sys == __NR_exit || sys == __NR_exit_group)
    {
        fprintf(tracee->log, ") = ?\n");
    }
    else if(sys == __NR_brk)
    {
        fprintf(tracee->log, ") = %#0x\n", sysres);
    }
    else
    {
        maybe_error(tracee, regs);
    }

    fflush(tracee->log);
}


struct stracee_t *get_stracee(pid_t pid)
{
    int i;

    for(i = 0; i < stracee_count; i++)
    {
        if(stracee[i].pid == pid)
        {
            return &stracee[i];
        }
    }
    
    return NULL;
}


static void print_short_usage(char *myname)
{
    fprintf(stderr, "Usage: %s [options] { -p pid | command [args] }\n\n"
                    "See %s --help for details\n",
                    myname, myname);
}


#define ERR_UNKNOWN_OPTION_ARG(op)                              \
    ERR_EXIT("%s: option '%s' passed unknown argument: %s\n",   \
             argv[0], op, optarg);

#define ERR_UNKNOWN_OPTION(op)  \
    ERR_EXIT("%s: unknown option : %s\n", argv[0], op);


int main(int argc, char **argv)
{
    int c;
    int option_index = 0;
    //int optind;
    //char *optarg = NULL;
    pid_t pid;
    char *tmp;
    sigset_t set;
    
    DEBUG_PRINT("%s: parsing options\n", argv[0]);

    static struct option long_options[] =
    {
        {"attach",             required_argument, 0, 'p'},
        {"debug",              no_argument      , 0, 'd'},
        {"decode-fds",         required_argument, 0,  0 },
        {"detch-on",           required_argument, 0, 'b'},
        {"env",                required_argument, 0, 'E'},
        {"event",              required_argument, 0, 'e'},
        {"fault",              required_argument, 0,  0 },
        {"failed-only",        no_argument      , 0, 'Z'},
        {"follow-forks",       no_argument      , 0, 'f'},
        {"help",               no_argument      , 0, 'h'},
        {"inject",             required_argument, 0,  0 },
        {"interruptible",      required_argument, 0, 'I'},
        {"output",             required_argument, 0, 'o'},
        {"output-append-mode", no_argument      , 0, 'A'},
        {"output-separately",  no_argument      , 0,  0 },
        {"quiet",              required_argument, 0,  0 },
        {"signal",             required_argument, 0,  0 },
        {"silence",            required_argument, 0,  0 },
        {"silent",             required_argument, 0,  0 },
        {"status",             required_argument, 0,  0 },
        {"string-limit",       required_argument, 0, 's'},
        {"successful-only",    no_argument      , 0, 'z'},
        {"summary-only",       no_argument      , 0, 'c'},
        {"trace",              required_argument, 0,  0 },
        {"user",               required_argument, 0, 'u'},
        {"version",            no_argument      , 0, 'v'},
        {0, 0, 0, 0}
    };
    
    memset(stracee, 0, sizeof(stracee));
    
    while((c = getopt_long(argc, argv, "+b:ce:fho:p:s:u:vzAE:I:Z",
                                long_options, &option_index)) != -1)
    {
        switch(c)
        {
            case 0:
                DEBUG_PRINT("%s: got option: %s\n", argv[0], long_options[option_index].name);

                if(strcmp(long_options[option_index].name, "decode-fds") == 0)
                {
                    ASSERT_OPTION_EXISTS("decode-fds");
                    process_eoption_decode_fds(argv[0], optarg);
                }
                else if(strcmp(long_options[option_index].name, "fault") == 0)
                {
                    ASSERT_OPTION_EXISTS("fault");
                    process_eoption_fault(argv[0], optarg);
                }
                else if(strcmp(long_options[option_index].name, "inject") == 0)
                {
                    ASSERT_OPTION_EXISTS("inject");
                    process_eoption_inject(argv[0], optarg);
                }
                else if(strcmp(long_options[option_index].name, "output-separately") == 0)
                {
                    output_separately = 1;
                }
                else if(strcmp(long_options[option_index].name, "quiet") == 0 ||
                        strcmp(long_options[option_index].name, "silence") == 0 ||
                        strcmp(long_options[option_index].name, "silent") == 0)
                {
                    ASSERT_OPTION_EXISTS(long_options[option_index].name);
                    process_eoption_silent(argv[0], optarg);
                }
                else if(strcmp(long_options[option_index].name, "signal") == 0)
                {
                    ASSERT_OPTION_EXISTS("signal");
                    process_eoption_signal(argv[0], optarg);
                }
                else if(strcmp(long_options[option_index].name, "status") == 0)
                {
                    ASSERT_OPTION_EXISTS("status");
                    process_eoption_status(argv[0], optarg);
                }
                else if(strcmp(long_options[option_index].name, "trace") == 0)
                {
                    ASSERT_OPTION_EXISTS("trace");
                    process_eoption_trace(argv[0], optarg);
                }
                break;

            case 'b':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                // only execve syscall is supported for this option
                if(strcmp(optarg, "execve") == 0)
                {
                    detach_on = __NR_execve;
                }
                else
                {
                    //ERR_EXIT("%s: option '-b' passed unknown argument: %s\n",
                    //         argv[0], optarg);
                    ERR_UNKNOWN_OPTION_ARG("-b");
                }
                break;

            case 'c':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                summary_only = 1;
                break;

            case 'd':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                debug_on = 1;
                break;

            case 'e':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                if(strstr(optarg, "decode-fd=") == optarg)
                {
                    process_eoption_decode_fds(argv[0], optarg + 10);
                }
                else if(strstr(optarg, "decode-fds=") == optarg)
                {
                    process_eoption_decode_fds(argv[0], optarg + 11);
                }
                else if(strstr(optarg, "fault=") == optarg)
                {
                    process_eoption_fault(argv[0], optarg + 6);
                }
                else if(strstr(optarg, "inject=") == optarg)
                {
                    process_eoption_inject(argv[0], optarg + 7);
                }
                else if(strstr(optarg, "quiet=") == optarg)
                {
                    process_eoption_silent(argv[0], optarg + 6);
                }
                else if(strstr(optarg, "silent=") == optarg)
                {
                    process_eoption_silent(argv[0], optarg + 7);
                }
                else if(strstr(optarg, "silence=") == optarg)
                {
                    process_eoption_silent(argv[0], optarg + 8);
                }
                else if(strstr(optarg, "signal=") == optarg)
                {
                    process_eoption_signal(argv[0], optarg + 7);
                }
                else if(strstr(optarg, "signals=") == optarg)
                {
                    process_eoption_signal(argv[0], optarg + 8);
                }
                else if(strstr(optarg, "status=") == optarg)
                {
                    process_eoption_status(argv[0], optarg + 7);
                }
                else if(strstr(optarg, "trace=") == optarg)
                {
                    process_eoption_trace(argv[0], optarg + 6);
                }
                else
                {
                    //ERR_EXIT("%s: option '-e' passed unknown argument: %d\n",
                    //         argv[0], pid);
                    ERR_UNKNOWN_OPTION_ARG("-e");
                }
                break;

            case 'f':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                follow_forks = 1;
                break;

            case 'o':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                logfilename = optarg;
                break;

            case 'p':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                if(stracee_count == MAX_STRACES - 1)
                {
                    ERR_EXIT("%s: max number of tracees reached: %d\n",
                             argv[0], MAX_STRACES);
                }
                
                if((pid = atoi(optarg)) <= 0)
                {
                    //ERR_EXIT("%s: invalid tracee pid: %d\n", argv[0], pid);
                    ERR_UNKNOWN_OPTION_ARG("-p");
                }
                
                stracee[stracee_count++].pid = pid;
                break;

            case 's':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                string_limit = 1;
                break;

            case 'u':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                run_as_user = optarg;
                break;

            case 'z':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                process_eoption_status(argv[0], "successful");
                break;

            case 'A':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                output_append = 1;
                break;

            case 'E':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                if(!newenv && !(newenv = malloc(newenv_size * sizeof(char *))))
                {
                    ERR_EXIT("%s: insufficient memory\n", argv[0]);
                }

                if((newenv_index == newenv_size) &&
                   !(tmp = realloc(newenv, (newenv_size <<= 1) * sizeof(char *))))
                {
                    ERR_EXIT("%s: insufficient memory\n", argv[0]);
                }
                
                newenv[newenv_index++] = optarg;
                break;

            case 'I':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                if(strcmp(optarg, "1") == 0 || strcmp(optarg, "anywhere") == 0)
                {
                    interruptible = INTERRUPT_ANYWHERE;
                }
                else if(strcmp(optarg, "2") == 0 || strcmp(optarg, "waiting") == 0)
                {
                    interruptible = INTERRUPT_WAITING;
                }
                else if(strcmp(optarg, "3") == 0 || strcmp(optarg, "never") == 0)
                {
                    interruptible = INTERRUPT_NEVER;
                }
                else if(strcmp(optarg, "4") == 0 || strcmp(optarg, "never_tstp") == 0)
                {
                    interruptible = INTERRUPT_NEVER_TSTP;
                }
                else
                {
                    ERR_UNKNOWN_OPTION_ARG("-I");
                }
                break;

            case 'Z':
                DEBUG_PRINT("%s: got option: -%c\n", argv[0], c);

                process_eoption_status(argv[0], "failed");
                break;

            case 'v':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                printf("strace utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options] { -p pid | command [args] }\n\n"
                       "Options:\n"
                       "  -h, --help        Show this help and exit\n"
                       "  -n, --numeric     "
                            "Show vendor and device codes instead of names\n"
                       "  -v, --version     Print version and exit\n"
                       "\n", argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case '?':
                break;

            default:
                abort();
        }
    }

    DEBUG_PRINT("%s: log filename: %s\n", argv[0], logfilename ? logfilename : "none");
    
    // set default interrupt state if none is specified
    if(interruptible == 0)
    {
        interruptible = logfilename ? INTERRUPT_NEVER : INTERRUPT_WAITING;
    }
    
    if(stracee_count == 0 && optind == argc)
    {
        fprintf(stderr, "%s: missing argument(s)\n", argv[0]);
        print_short_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(stracee_count && optind < argc)
    {
        fprintf(stderr, "%s: you cannot specify both -p and a command name\n",
                        argv[0]);
        print_short_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    DEBUG_PRINT("%s: tracees (with -p option): %d\n", argv[0], stracee_count);

    if(stracee_count == 0)
    {
        if(!(pid = fork()))
        {
            pid_t mypid = getpid();

            DEBUG_PRINT("%s: child %d: tracee entry\n", argv[0], mypid);

            if(ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
            {
                fprintf(stderr, "%s: child %d: ptrace failed: %s\n",
                                argv[0], mypid, strerror(errno));
                exit(EXIT_FAILURE);
            }

            DEBUG_PRINT("%s: child %d: now being traced\n", argv[0], mypid);
            
            if(newenv)
            {
                DEBUG_PRINT("%s: child %d: setting new environ\n", argv[0], mypid);

                for(c = 0; c < newenv_index; c++)
                {
                    char *eq = strchr(newenv[c], '=');
                    
                    if(eq)
                    {
                        *eq = '\0';
                        setenv(newenv[c], eq + 1, 1);
                    }
                    else
                    {
                        unsetenv(newenv[c]);
                    }
                }
            }
            
            DEBUG_PRINT("%s: child %d: calling execve\n", argv[0], mypid);
            DEBUG_PRINT("%s: child %d: argv[0] %s argv %p, env %p\n", argv[0], mypid, argv[optind], &argv[optind], environ);

            execvp(argv[optind], &argv[optind]);
            return ENOEXEC;
        }
        else if(pid < 0)
        {
            fprintf(stderr, "%s: failed to fork: %s\n", argv[0], strerror(errno));
            exit(EXIT_FAILURE);
        }

        DEBUG_PRINT("%s: child tracee pid: %d\n", argv[0], pid);
        
        stracee[0].pid = pid;
        //stracee[0].tid = gettid(pid);
        stracee[0].prev_syscall = -1;
        stracee_count = 1;
        
        if(logfilename)
        {
            if(!(stracee[0].log = fopen(logfilename, output_append ? "a" : "w")))
            {
                fprintf(stderr, "%s: failed to open '%s': %s\n",
                                argv[0], logfilename, strerror(errno));
                kill(pid, SIGKILL);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            stracee[0].log = stderr;
        }
    }
    else
    {
        DEBUG_PRINT("%s: attaching to tracees\n", argv[0]);

        for(c = 0; c < stracee_count; c++)
        {
            DEBUG_PRINT("%s: tracee[%d] - pid %d\n", argv[0], c, stracee[c].pid);

            stracee[c].prev_syscall = -1;

            if(ptrace(PTRACE_ATTACH, stracee[c].pid, NULL, NULL) < 0)
            {
                fprintf(stderr, "%s: ptrace for pid %d failed: %s\n",
                                argv[0], stracee[c].pid, strerror(errno));

                while(--c > 0)
                {
                    ptrace(PTRACE_DETACH, stracee[c].pid, NULL, NULL);
                }

                exit(EXIT_FAILURE);
            }

            if(logfilename)
            {
                if(output_separately)
                {
                    char buf[strlen(logfilename) + 16];
                    
                    sprintf(buf, "%s.%d", logfilename, stracee[c].pid);

                    if(!(stracee[c].log = fopen(buf, "w")))
                    {
                        fprintf(stderr, "%s: failed to open '%s': %s\n",
                                        argv[0], buf, strerror(errno));

                        while(--c > 0)
                        {
                            ptrace(PTRACE_DETACH, stracee[c].pid, NULL, NULL);
                        }

                        exit(EXIT_FAILURE);
                    }
                }
                else if(c == 0)
                {
                    if(!(stracee[c].log = fopen(logfilename, output_append ? "a" : "w")))
                    {
                        fprintf(stderr, "%s: failed to open '%s': %s\n",
                                        argv[0], logfilename, strerror(errno));

                        while(--c > 0)
                        {
                            ptrace(PTRACE_DETACH, stracee[c].pid, NULL, NULL);
                        }

                        exit(EXIT_FAILURE);
                    }
                }
                else
                {
                    stracee[c].log = stracee[0].log;
                }
            }
            else
            {
                stracee[c].log = stderr;
            }

            if(!quiet_mask[QUIET_ATTACH])
            {
                fprintf(stracee[c].log, "[ Process %d attached ]\n",
                                        stracee[c].pid);
            }
        }
    }

    // block fatal signals
    if(interruptible == INTERRUPT_NEVER || interruptible == INTERRUPT_NEVER_TSTP)
    {
        block_fatal_signals(&set);
    }
    
    // main loop
    
    while(1)
    {
        DEBUG_PRINT("%s: waiting for children\n", argv[0]);

        int status = 0;
        struct stracee_t *tracee = NULL;
        
        pid = waitpid(-1, &status, WSTOPPED);

        DEBUG_PRINT("%s: waitpid returned %d\n", argv[0], pid);
        
        if(pid < 0)
        {
            if(errno == ECHILD)
            {
                break;
            }
            
            fprintf(stderr, "%s: waitpid: %s\n", argv[0], strerror(errno));
            continue;
        }
        
        if(!(tracee = get_stracee(pid)))
        {
            continue;
        }
        
        if(WIFSTOPPED(status))
        {
            if(WSTOPSIG(status) == SIGTRAP)
            {
                struct user_regs_struct regs;
                int event;
                
                ptrace(PTRACE_GETREGS, pid, NULL, &regs);
                
                event = (status >> 16) & 0xff;
                
                switch(event)
                {
                    case PTRACE_EVENT_SYSCALL_ENTER:
                        DEBUG_PRINT("%s: tracee %d: entering syscall\n", argv[0], pid);
                        /*
                        if(tracee->prev_syscall == __NR_execve)
                        {
                            syscall_finish(tracee, __NR_execve, NULL);
                        }
                        
                        tracee->prev_syscall = GET_SYSCALL_NUMBER(&regs);
                        syscall_handle(tracee, &regs);
                        */

                        tracee->prev_syscall = GET_SYSCALL_NUMBER(&regs);
                        syscall_handle(tracee, &regs);
                        
                        if(detach_on == tracee->prev_syscall)
                        {
                            if(!quiet_mask[QUIET_ATTACH])
                            {
                                fprintf(tracee->log, "[ Process %d detached ]\n",
                                                     stracee->pid);
                            }

                            tracee->pid = 0;
                            
                            if(tracee->log != stderr)
                            {
                                fclose(tracee->log);
                                tracee->log = NULL;
                            }
                            
                            continue;
                        }
                        
                        break;

                    case PTRACE_EVENT_SYSCALL_EXIT:
                        DEBUG_PRINT("%s: tracee %d: finishing syscall\n", argv[0], pid);
                        /*
                        syscall_finish(tracee, tracee->prev_syscall, &regs);
                        tracee->prev_syscall = -1;
                        */

                        // block fatal signals
                        if(interruptible == INTERRUPT_WAITING)
                        {
                            block_fatal_signals(&set);
                        }

                        syscall_finish(tracee, tracee->prev_syscall, &regs);
                        tracee->prev_syscall = -1;

                        if(interruptible == INTERRUPT_WAITING)
                        {
                            unblock_fatal_signals(&set);
                        }

                        break;

                    default:
                        fprintf(tracee->log, "Unknown event: %d\n", event);
                        break;
                }
                
                DEBUG_PRINT("%s: tracee %d: continuing\n", argv[0], pid);
                ptrace(PTRACE_CONT, pid, NULL, NULL);
            }
            else
            {
                /*
                fprintf(tracee->log, "--- %s ---\n", sig_names[WSTOPSIG(status)]);
                ptrace(PTRACE_CONT, pid, NULL, (void*)(uintptr_t)WSTOPSIG(status));
                */

                int res = WSTOPSIG(status);
                fprintf(tracee->log, "--- %s ---\n", sig_names[res]);
                ptrace(PTRACE_CONT, pid, NULL, &res);
            }
        }
        else if(WIFSIGNALED(status))
        {
            if(!quiet_mask[QUIET_EXIT])
            {
                fprintf(tracee->log, "+++ killed by %s +++\n",
                                     sig_names[WTERMSIG(status)]);
            }
        }
        else if(WIFEXITED(status))
        {
            // mimic the last syscall (likely exit or group_exit)
            if(tracee->prev_syscall != -1)
            {
                fprintf(tracee->log, "%s(%d) = ?\n",
                        syscall_names[tracee->prev_syscall], WEXITSTATUS(status));
                //syscall_finish(tracee, tracee->prev_syscall, NULL);
                tracee->prev_syscall = -1;
            }

            if(!quiet_mask[QUIET_EXIT])
            {
                fprintf(tracee->log, "+++ exited with %d +++\n",
                                     WEXITSTATUS(status));
            }
        }
    }
    
    DEBUG_PRINT("%s: done!\n", argv[0]);
    exit(EXIT_SUCCESS);
}

