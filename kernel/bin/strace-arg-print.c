/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: strace-arg-print.c
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
 *  \file strace-arg-print.c
 *
 *  Helper functions to print different types of syscall arguments. This file
 *  is part of the trace utility program.
 */

#undef __GNU_VISIBLE
#define __GNU_VISIBLE   1
#define KERNEL      // we need this to get the kernel's syscall macros
#include "strace.h"
#include "strace-sig.h"
#include "strace-struct-defs.h"
#include <errno.h>
#include <unistd.h>
#include <sys/sysctl.h>
////#include <sys/time.h>
#include <sys/times.h>
//#include <sys/utime.h>
#include <utime.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <kernel/syscall.h>


void print_arg_i(struct stracee_t *tracee, size_t i)
{
    fprintf(tracee->log, "%zd", i);
}

void print_arg_ui(struct stracee_t *tracee, size_t i)
{
    fprintf(tracee->log, "%zu", i);
}

void print_arg_fd(struct stracee_t *tracee, int i)
{
    fprintf(tracee->log, "%d", i);
}

void print_arg_dirfd(struct stracee_t *tracee, int i)
{
    if(i == AT_FDCWD)
    {
        fprintf(tracee->log, "AT_FDCWD");
    }
    else
    {
        fprintf(tracee->log, "%d", i);
    }
}

void print_arg_dev(struct stracee_t *tracee, uintptr_t d)
{
#ifdef __x86_64__
    fprintf(tracee->log, "%#0lx", d);
#else
    fprintf(tracee->log, "%#0x", d);
#endif
}

void print_arg_sig(struct stracee_t *tracee, int sig)
{
    if(sig >= 0 && sig < sig_name_count)
    {
        fprintf(tracee->log, "%s", sig_names[sig]);
    }
    else
    {
        fprintf(tracee->log, "%d", sig);
    }
}

void print_arg_resource(struct stracee_t *tracee, int which)
{
    if(which >= 0 && which < RLIMIT_NLIMITS)
    {
        fprintf(tracee->log, "%s", rlimit_names[which]);
    }
    else
    {
        fprintf(tracee->log, "%d", which);
    }
}

void print_arg_ptr(struct stracee_t *tracee, uintptr_t ptr)
{
    if(ptr == 0)
    {
        fprintf(tracee->log, "NULL");
    }
    else
    {
#ifdef __x86_64__
        fprintf(tracee->log, "%#0lx", ptr);
#else
        fprintf(tracee->log, "%#0x", ptr);
#endif
    }
}

int tracee_get_bytes(struct stracee_t *tracee, uintptr_t addr,
                     char *buf, size_t bufsz)
{
    //void *buf = (void *)_buf;
    //size_t bufsz = (_bufsz + sizeof(void *) - 1) / sizeof(void *);
    size_t i, j, k;
    long int word = 0;

    for(i = 0; i < bufsz; i += sizeof(void *), addr += sizeof(void *))
    {
        //if(ptrace(PTRACE_PEEKDATA, tracee->pid, (void *)addr, &buf[i]) != 0)
        if((word = ptrace(PTRACE_PEEKDATA, tracee->pid,
                            (void *)addr)) < 0 && errno != 0)
        {
            return -1;
        }
        
        // don't overread
        k = (i <= bufsz - sizeof(void *)) ? sizeof(void *) : (bufsz - i);

        for(j = 0; j < k; j++)
        {
            char c = word & 0xff;
            
            *buf++ = c;
            word >>= 8;
        }
    }
    
    return 0;
}

uintptr_t tracee_get_ptr(struct stracee_t *tracee, uintptr_t addr)
{
    //void *word = NULL;
    long int word = 0;

    //if(ptrace(PTRACE_PEEKDATA, tracee->pid, (void *)addr, &word) != 0)
    if((word = ptrace(PTRACE_PEEKDATA, tracee->pid,
                        (void *)addr)) < 0 && errno != 0)
    {
        return 0;
    }
    
    return (uintptr_t)word;
}

void print_arg_strarr(struct stracee_t *tracee, uintptr_t arr)
{
    uintptr_t word;
    int i;

    fprintf(tracee->log, "[");
    word = tracee_get_ptr(tracee, arr);
    
    for(i = 0; i < arr_limit; i++)
    {
        print_arg_str(tracee, word);
        arr += sizeof(void *);
        word = tracee_get_ptr(tracee, arr);
        
        if(word)
        {
            fprintf(tracee->log, ", ");
        }
        else
        {
            break;
        }
    }
    
    if(i == arr_limit)
    {
        fprintf(tracee->log, "...");
    }
    
    fprintf(tracee->log, "]");
}

void print_arg_fds(struct stracee_t *tracee,
                                    uintptr_t ptr, size_t fdcount)
{
    size_t bufsz = (((sizeof(int) * fdcount) + sizeof(void *) - 1) /
                        sizeof(void *)) * sizeof(void *);
    char buf[bufsz];
    int *fds = (int *)buf;
    size_t i;

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "[");

        for(i = 0; i < fdcount; i++)
        {
            fprintf(tracee->log, "%d", fds[i]);
        
            if(i + 1 < fdcount)
            {
                fprintf(tracee->log, ", ");
            }
        }

        fprintf(tracee->log, "]");
    }
}

#define ALIGNED_STRUCT_SZ(st)       \
    ((sizeof(st) + sizeof(void *) - 1) / sizeof(void *)) * sizeof(void *)

void print_arg_utimbuf(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct utimbuf);
    char buf[bufsz];
    struct utimbuf *times = (struct utimbuf *)buf;

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
#ifdef __x86_64__
        fprintf(tracee->log, "{actime=%lu,modtime=%lu}",
#else
        fprintf(tracee->log, "{actime=%llu,modtime=%llu}",
#endif
                             times->actime, times->modtime);
    }
}

void print_arg_timeval(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct timeval);
    char buf[bufsz];
    struct timeval *tv = (struct timeval *)buf;

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
#ifdef __x86_64__
        fprintf(tracee->log, "{tv_sec=%lu,tv_usec=%lu}",
#else
        fprintf(tracee->log, "{tv_sec=%llu,tv_usec=%lu}",
#endif
                             tv->tv_sec, tv->tv_usec);
    }
}

void print_arg_tms(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct tms);
    char buf[bufsz];
    struct tms *tms = (struct tms *)buf;

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{tms_utime=%lu,tms_stime=%lu,"
                             "tms_cutime=%lu,tms_cstime=%lu}",
                             tms->tms_utime, tms->tms_stime,
                             tms->tms_cutime, tms->tms_cstime);
    }
}

void print_arg_timespec(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct timespec);
    char buf[bufsz];
    struct timespec *ts = (struct timespec *)buf;

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
#ifdef __x86_64__
        fprintf(tracee->log, "{tv_sec=%lu,tv_nsec=%lu}",
#else
        fprintf(tracee->log, "{tv_sec=%llu,tv_nsec=%lu}",
#endif
                             ts->tv_sec, ts->tv_nsec);
    }
}

void print_sigset(struct stracee_t *tracee, sigset_t *set)
{
    int i, flag = 0;

    fprintf(tracee->log, "[");
    
    for(i = 0; i < NSIG; i++)
    {
        if(sigismember(set, i))
        {
            if(flag)
            {
                fprintf(tracee->log, "|");
            }
            
            print_arg_sig(tracee, i);
            flag = 1;
        }
    }

    fprintf(tracee->log, "]");
}

void print_arg_sigset(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(sigset_t);
    char buf[bufsz];
    sigset_t *set = (sigset_t *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        print_sigset(tracee, set);
    }
}

void print_arg_sigaction(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct sigaction);
    char buf[bufsz];
    struct sigaction *sa = (struct sigaction *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{sa_mask=");
        print_sigset(tracee, &sa->sa_mask);
        fprintf(tracee->log, ",sa_handler=");
        
        if(sa->sa_handler == SIG_IGN)
        {
            fprintf(tracee->log, "SIG_IGN");
        }
        else if(sa->sa_handler == SIG_DFL)
        {
            fprintf(tracee->log, "SIG_DFL");
        }
        else
        {
            print_arg_ptr(tracee, (uintptr_t)sa->sa_handler);
        }

        fprintf(tracee->log, ",sa_cookie=");
        print_arg_ptr(tracee, (uintptr_t)sa->sa_cookie);

        fprintf(tracee->log, ",sa_restorer=");
        print_arg_ptr(tracee, (uintptr_t)sa->sa_restorer);

        fprintf(tracee->log, ",sa_flags=");
        print_sa_flags(tracee, sa->sa_flags);
        fprintf(tracee->log, "}");
    }
}

#define print_rlim(lim)                         \
    if(lim == RLIM_INFINITY)                    \
        fprintf(tracee->log, "RLIM_INFINITY");  \
    else {                                      \
        unsigned long long _r = (unsigned long long)(lim);  \
        fprintf(tracee->log, "%llu", _r); \
    }

void print_arg_rlimit(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct rlimit);
    char buf[bufsz];
    struct rlimit *rlim = (struct rlimit *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{rlim_cur=");
        print_rlim(rlim->rlim_cur);
        fprintf(tracee->log, ",rlim_max=");
        print_rlim(rlim->rlim_max);
        fprintf(tracee->log, "}");
    }
}

#undef print_rlim

void print_arg_rusage(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct rusage);
    char buf[bufsz];
    struct rusage *ru = (struct rusage *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{ru_utime=");

#ifdef __x86_64__
        fprintf(tracee->log, "{tv_sec=%lu,tv_usec=%lu},",
#else
        fprintf(tracee->log, "{tv_sec=%llu,tv_usec=%lu},",
#endif
                             ru->ru_utime.tv_sec, ru->ru_utime.tv_usec);

        fprintf(tracee->log, "ru_stime=");

#ifdef __x86_64__
        fprintf(tracee->log, "{tv_sec=%lu,tv_usec=%lu},",
#else
        fprintf(tracee->log, "{tv_sec=%llu,tv_usec=%lu},",
#endif
                             ru->ru_stime.tv_sec, ru->ru_stime.tv_usec);

        fprintf(tracee->log, "ru_maxrss=%lu,", ru->ru_maxrss);
        fprintf(tracee->log, "ru_ixrss=%lu,", ru->ru_ixrss);
        fprintf(tracee->log, "ru_idrss=%lu,", ru->ru_idrss);
        fprintf(tracee->log, "ru_isrss=%lu,", ru->ru_isrss);
        fprintf(tracee->log, "ru_minflt=%lu,", ru->ru_minflt);
        fprintf(tracee->log, "ru_majflt=%lu,", ru->ru_majflt);
        fprintf(tracee->log, "ru_nswap=%lu,", ru->ru_nswap);
        fprintf(tracee->log, "ru_inblock=%lu,", ru->ru_inblock);
        fprintf(tracee->log, "ru_oublock=%lu,", ru->ru_oublock);
        fprintf(tracee->log, "ru_msgsnd=%lu,", ru->ru_msgsnd);
        fprintf(tracee->log, "ru_msgrcv=%lu,", ru->ru_msgrcv);
        fprintf(tracee->log, "ru_nsignals=%lu,", ru->ru_nsignals);
        fprintf(tracee->log, "ru_nvcsw=%lu,", ru->ru_nvcsw);
        fprintf(tracee->log, "ru_nivcsw=%lu}", ru->ru_nivcsw);
    }
}

void print_arg_itimerval(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct itimerval);
    char buf[bufsz];
    struct itimerval *tv = (struct itimerval *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{it_interval=");

#ifdef __x86_64__
        fprintf(tracee->log, "{tv_sec=%lu,tv_usec=%lu}",
#else
        fprintf(tracee->log, "{tv_sec=%llu,tv_usec=%lu}",
#endif
                             tv->it_interval.tv_sec, tv->it_interval.tv_usec);

        fprintf(tracee->log, ",it_value=");

#ifdef __x86_64__
        fprintf(tracee->log, "{tv_sec=%lu,tv_usec=%lu}",
#else
        fprintf(tracee->log, "{tv_sec=%llu,tv_usec=%lu}",
#endif
                             tv->it_value.tv_sec, tv->it_value.tv_usec);

        fprintf(tracee->log, "}");
    }
}

void print_arg_itimerspec(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct itimerspec);
    char buf[bufsz];
    struct itimerspec *ts = (struct itimerspec *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{it_interval=");

#ifdef __x86_64__
        fprintf(tracee->log, "{tv_sec=%lu,tv_nsec=%lu}",
#else
        fprintf(tracee->log, "{tv_sec=%llu,tv_nsec=%lu}",
#endif

                             ts->it_interval.tv_sec, ts->it_interval.tv_nsec);
        fprintf(tracee->log, ",it_value=");

#ifdef __x86_64__
        fprintf(tracee->log, "{tv_sec=%lu,tv_nsec=%lu}",
#else
        fprintf(tracee->log, "{tv_sec=%llu,tv_nsec=%lu}",
#endif

                             ts->it_value.tv_sec, ts->it_value.tv_nsec);
        fprintf(tracee->log, "}");
    }
}

void print_sched_param(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct sched_param);
    char buf[bufsz];
    struct sched_param *sp = (struct sched_param *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{sched_priority=%d}", sp->sched_priority);
    }
}

void print_stack_t(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(stack_t);
    char buf[bufsz];
    stack_t *sp = (stack_t *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{ss_sp=%p,", sp->ss_sp);
        fprintf(tracee->log, "ss_flags=%d,", sp->ss_flags);
        fprintf(tracee->log, "ss_size=%lu}", sp->ss_size);
    }
}

void print_sigevent(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct sigevent);
    char buf[bufsz];
    struct sigevent *se = (struct sigevent *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{sigev_notify=");
        
        switch(se->sigev_notify)
        {
            case SIGEV_NONE:
                fprintf(tracee->log, "SIGEV_NONE,");
                break;

            case SIGEV_SIGNAL:
                fprintf(tracee->log, "SIGEV_SIGNAL,");
                break;

            case SIGEV_THREAD:
                fprintf(tracee->log, "SIGEV_THREAD,");
                break;

            default:
                fprintf(tracee->log, "%d,", se->sigev_notify);
                break;
        }
        
        fprintf(tracee->log, "sigev_signo=");
        print_arg_sig(tracee, se->sigev_signo);
        fprintf(tracee->log, ",sigev_value=%p,", se->sigev_value.sival_ptr);
        fprintf(tracee->log, "sigev_notify_function=%p,", se->sigev_notify_function);
        fprintf(tracee->log, "sigev_notify_attributes=%p}", se->sigev_notify_attributes);
    }
}

void print_utsname(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct utsname);
    char buf[bufsz];
    struct utsname *u = (struct utsname *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{sysname=%s,", u->sysname);
        fprintf(tracee->log, "nodename=%s,", u->nodename);
        fprintf(tracee->log, "release=%s,", u->release);
        fprintf(tracee->log, "version=%s,", u->version);
        fprintf(tracee->log, "machine=%s,", u->machine);
        fprintf(tracee->log, "domainname=%s}", u->domainname);
    }
}

void print_arg_sysinfo(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct sysinfo);
    char buf[bufsz];
    struct sysinfo *info = (struct sysinfo *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "{uptime=%ld,", info->uptime);
        fprintf(tracee->log, "loads={%lu,%lu,%lu},",
                             info->loads[0], info->loads[1], info->loads[2]);
        fprintf(tracee->log, "totalram=%lu,", info->totalram);
        fprintf(tracee->log, "sharedram=%lu,", info->sharedram);
        fprintf(tracee->log, "bufferram=%lu,", info->bufferram);
        fprintf(tracee->log, "totalswap=%lu,", info->totalswap);
        fprintf(tracee->log, "freeswap=%lu,", info->freeswap);
        fprintf(tracee->log, "procs=%u,", info->procs);
        fprintf(tracee->log, "totalhigh=%lu,", info->totalhigh);
        fprintf(tracee->log, "freehigh=%lu,", info->freehigh);
        fprintf(tracee->log, "mem_unit=%u}", info->mem_unit);
    }
}

void print_arg_buf(struct stracee_t *tracee, uintptr_t buf, size_t count)
{
    //printf("print_arg_buf: count %ld, buf %#0x\n", count, buf);

    if(count <= 0)
    {
        fprintf(tracee->log, "...");
    }
    else if(buf == 0)
    {
        fprintf(tracee->log, "NULL");
    }
    else
    {
        size_t i = 0, j, k;
        ////void *word = NULL;
        //uintptr_t word = 0;
        long int word = 0;
        
        fprintf(tracee->log, "\"");
        
        while(i < count && i < string_limit)
        {
            //if(ptrace(PTRACE_PEEKDATA, tracee->pid, (void *)buf, &word) != 0)
            if((word = ptrace(PTRACE_PEEKDATA,
                                tracee->pid, (void *)buf)) < 0 && errno != 0)
            {
                break;
            }
            
            // don't overread
            k = (i <= count - sizeof(void *)) ?
                    sizeof(void *) : (count - i);
            
            for(j = 0; j < k; j++)
            {
                char c = word & 0xff;
                //printf("[%#x]", c);

                if(c == '\\')
                {
                    fprintf(tracee->log, "\\\\");
                }
                else if(c == '"')
                {
                    fprintf(tracee->log, "\\\"");
                }
                else if(c >= ' ' && c < '~')
                {
                    fprintf(tracee->log, "%c", c);
                }
                else if(c == '\r')
                {
                    fprintf(tracee->log, "\\r");
                }
                else if(c == '\n')
                {
                    fprintf(tracee->log, "\\n");
                }
                else if(c == '\t')
                {
                    fprintf(tracee->log, "\\t");
                }
                else
                {
                    fprintf(tracee->log, "\\x%02x", c);
                }
                
                word >>= 8;
            }
                
            buf += sizeof(void *);
            i += sizeof(void *);
        }

        fprintf(tracee->log, "\"");
        
        if(i < count)
        {
            fprintf(tracee->log, "...");
        }
    }
}


void print_arg_str(struct stracee_t *tracee, uintptr_t buf)
{
    if(buf == 0)
    {
        fprintf(tracee->log, "NULL");
    }
    else
    {
        size_t i = 0, j, k;
        ////void *word = NULL;
        //uintptr_t word = 0;
        long int word = 0;
        
        fprintf(tracee->log, "\"");
        
        while(i < string_limit)
        {
            //if(ptrace(PTRACE_PEEKDATA, tracee->pid, (void *)buf, &word) != 0)
            if((word = ptrace(PTRACE_PEEKDATA,
                                tracee->pid, (void *)buf)) < 0 && errno != 0)
            {
                //printf("print_arg_str: couldn't peekdata\n");
                fprintf(tracee->log, "...");
                break;
            }

            //printf("print_arg_str: word = '%d'\n", word);
            
            // don't overread
            k = (i <= string_limit - sizeof(void *)) ?
                    sizeof(void *) : (string_limit - i);
            
            for(j = 0; j < k; j++)
            {
                char c = word & 0xff;

                if(c == '\\')
                {
                    fprintf(tracee->log, "\\\\");
                }
                else if(c == '"')
                {
                    fprintf(tracee->log, "\\\"");
                }
                else if(c >= ' ' && c < '~')
                {
                    fprintf(tracee->log, "%c", c);
                }
                else if(c == '\r')
                {
                    fprintf(tracee->log, "\\\r");
                }
                else if(c == '\n')
                {
                    fprintf(tracee->log, "\\\n");
                }
                else if(c == '\0')
                {
                    fprintf(tracee->log, "\"");
                    return;
                }
                else
                {
                    fprintf(tracee->log, "\\x%02x", c);
                }
                
                word >>= 8;
            }
                
            buf += sizeof(void *);
            i += sizeof(void *);
        }

        if(i == string_limit)
        {
            fprintf(tracee->log, "\"...");
        }
        else
        {
            fprintf(tracee->log, "\"");
        }
    }
}

void print_argmode(struct stracee_t *tracee, size_t mode)
{
    fprintf(tracee->log, "%o", (unsigned int)(mode & 0777));
    
    if(S_ISBLK(mode))
    {
        fprintf(tracee->log, "|S_IFBLK");
    }
    else if(S_ISCHR(mode))
    {
        fprintf(tracee->log, "|S_IFCHR");
    }
    else if(S_ISDIR(mode))
    {
        fprintf(tracee->log, "|S_IFDIR");
    }
    else if(S_ISFIFO(mode))
    {
        fprintf(tracee->log, "|S_IFIFO");
    }
    else if(S_ISLNK(mode))
    {
        fprintf(tracee->log, "|S_IFLNK");
    }
    else if(S_ISSOCK(mode))
    {
        fprintf(tracee->log, "|S_IFSOCK");
    }
    else
    {
        fprintf(tracee->log, "|S_IFREG");
    }
    
    if(mode & S_ISUID)
    {
        fprintf(tracee->log, "|S_ISUID");
    }

    if(mode & S_ISGID)
    {
        fprintf(tracee->log, "|S_ISGID");
    }

    if(mode & S_ISVTX)
    {
        fprintf(tracee->log, "|S_ISVTX");
    }
}

void print_arg_accmode(struct stracee_t *tracee, size_t mode)
{
    int flag = 0;

    if(mode == F_OK)
    {
        fprintf(tracee->log, "F_OK");
    	return;
    }
    
    if(mode & R_OK)
    {
        fprintf(tracee->log, "R_OK");
        flag = 1;
    }

    if(mode & W_OK)
    {
        if(flag)
        {
            fprintf(tracee->log, "|");
        }

        fprintf(tracee->log, "W_OK");
        flag = 1;
    }

    if(mode & X_OK)
    {
        if(flag)
        {
            fprintf(tracee->log, "|");
        }

        fprintf(tracee->log, "X_OK");
    }
}

void print_flags_generic(struct stracee_t *tracee,
                                       uintptr_t flags, char *_default,
                                       struct flag_t *farr, int farr_size)
{
    int i;

    if(!flags)
    {
        fprintf(tracee->log, _default);
        return;
    }

    for(i = 0; i < farr_size; i++)
    {
        if(flags & farr[i].val)
        {
            fprintf(tracee->log, farr[i].name);
            flags &= ~farr[i].val;
            
            if(flags)
            {
                fprintf(tracee->log, "|");
            }
        }
    }
    
    if(flags)
    {
#ifdef __x86_64__
        fprintf(tracee->log, "(%#lx)", flags);
#else
        fprintf(tracee->log, "(%#x)", flags);
#endif
    }
}

void print_open_flags(struct stracee_t *tracee, uintptr_t flags)
{
    print_flags_generic(tracee, flags, "O_RDONLY", open_flags, OPEN_FLAGS_COUNT);
}

void print_at_flags(struct stracee_t *tracee, uintptr_t flags)
{
    print_flags_generic(tracee, flags, "0", at_flags, AT_FLAGS_COUNT);
}

void print_wait_flags(struct stracee_t *tracee, uintptr_t flags)
{
    print_flags_generic(tracee, flags, "0", wait_flags, WAIT_FLAGS_COUNT);
}

void print_mount_flags(struct stracee_t *tracee, uintptr_t flags)
{
    print_flags_generic(tracee, flags, "0", mount_flags, MOUNT_FLAGS_COUNT);
}

void print_umount_flags(struct stracee_t *tracee, uintptr_t flags)
{
    print_flags_generic(tracee, flags, "0", umount_flags, UMOUNT_FLAGS_COUNT);
}

void print_sa_flags(struct stracee_t *tracee, int flags)
{
    print_flags_generic(tracee, flags, "0", sa_flags, SA_FLAGS_COUNT);
}

void print_prot_flags(struct stracee_t *tracee, int flags)
{
    print_flags_generic(tracee, flags, "0", prot_flags, PROT_FLAGS_COUNT);
}

void print_id_generic(struct stracee_t *tracee, size_t id,
                                       struct flag_t *farr, int farr_size)
{
    int i;

    for(i = 0; i < farr_size; i++)
    {
        if(id == farr[i].val)
        {
            fprintf(tracee->log, farr[i].name);
            return;
        }
    }

    fprintf(tracee->log, "%zd", id);
}

void print_ptrace_request(struct stracee_t *tracee, size_t req)
{
    print_id_generic(tracee, req, ptrace_requests, PTRACE_REQUEST_COUNT);
}

void print_clock_id(struct stracee_t *tracee, size_t id)
{
    print_id_generic(tracee, id, clock_ids, CLOCK_IDS_COUNT);
}

void print_clock_flags(struct stracee_t *tracee, size_t flags)
{
    if(flags == TIMER_ABSTIME)
    {
        fprintf(tracee->log, "TIMER_ABSTIME");
    }
    else
    {
        fprintf(tracee->log, "%zu", flags);
    }
}

void print_itimer_id(struct stracee_t *tracee, size_t id)
{
    print_id_generic(tracee, id, itimer_ids, TIMER_IDS_COUNT);
}

void print_sched_policy(struct stracee_t *tracee, size_t id)
{
    print_id_generic(tracee, id, sched_policies, SCHED_POLICY_COUNT);
}


void print_arg_prio(struct stracee_t *tracee, int which)
{
    if(which == PRIO_PROCESS)
    {
        fprintf(tracee->log, "PRIO_PROCESS");
    }
    else if(which == PRIO_PGRP)
    {
        fprintf(tracee->log, "PRIO_PGRP");
    }
    else if(which == PRIO_USER)
    {
        fprintf(tracee->log, "PRIO_USER");
    }
    else
    {
        fprintf(tracee->log, "%d", which);
    }
}


void print_mmap_args(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct syscall_args);
    char buf[bufsz];
    struct syscall_args *a = (struct syscall_args *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "%p, ", (void *)a->args[0]);
        fprintf(tracee->log, "%lu, ", (size_t)a->args[1]);
        //fprintf(tracee->log, "%d, ", (int)a->args[2]);
        print_prot_flags(tracee, (int)a->args[2]);
        fprintf(tracee->log, ", %d, ", (int)a->args[3]);
        fprintf(tracee->log, "%d, ", (int)a->args[4]);
        fprintf(tracee->log, "%ld, ", (off_t)a->args[5]);
        fprintf(tracee->log, "%p", (void **)a->args[6]);
    }
}

void print_mremap_args(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct syscall_args);
    char buf[bufsz];
    struct syscall_args *a = (struct syscall_args *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "%p, ", (void *)a->args[0]);
        fprintf(tracee->log, "%lu, ", (size_t)a->args[1]);
        fprintf(tracee->log, "%lu, ", (size_t)a->args[2]);
        fprintf(tracee->log, "%d, ", (int)a->args[3]);
        fprintf(tracee->log, "%p, ", (void *)a->args[4]);
        fprintf(tracee->log, "%p", (void **)a->args[5]);
    }
}

void print_sysctl_args(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct __sysctl_args);
    char buf[bufsz];
    struct __sysctl_args *a = (struct __sysctl_args *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "%p, ", a->name);
        fprintf(tracee->log, "%d, ", a->nlen);
        fprintf(tracee->log, "%p, ", a->oldval);
        fprintf(tracee->log, "%p, ", a->oldlenp);
        fprintf(tracee->log, "%p, ", a->newval);
        fprintf(tracee->log, "%lu", a->newlen);
    }
}


void print_pselect_args(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct syscall_args);
    char buf[bufsz];
    struct syscall_args *a = (struct syscall_args *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "%d, ", (int)a->args[0]);
        fprintf(tracee->log, "%p, ", (void *)a->args[1]);
        fprintf(tracee->log, "%p, ", (void *)a->args[2]);
        fprintf(tracee->log, "%p, ", (void *)a->args[3]);
        print_arg_timespec(tracee, (uintptr_t)a->args[4]);
        fprintf(tracee->log, ", ");
        print_arg_sigset(tracee, (uintptr_t)a->args[5]);
    }
}

void print_sendto_args(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct syscall_args);
    char buf[bufsz];
    struct syscall_args *a = (struct syscall_args *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "%d, ", (int)a->args[0]);
        fprintf(tracee->log, "%p, ", (void *)a->args[1]);
        fprintf(tracee->log, "%lu, ", (size_t)a->args[2]);
        fprintf(tracee->log, "%d, ", (int)a->args[3]);
        fprintf(tracee->log, "%p, ", (void *)a->args[4]);
        fprintf(tracee->log, "%lu, ", (size_t)a->args[5]);
    }
}

void print_recvfrom_args(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct syscall_args);
    char buf[bufsz];
    struct syscall_args *a = (struct syscall_args *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "%d, ", (int)a->args[0]);
        fprintf(tracee->log, "%p, ", (void *)a->args[1]);
        fprintf(tracee->log, "%lu, ", (size_t)a->args[2]);
        fprintf(tracee->log, "%d, ", (int)a->args[3]);
        fprintf(tracee->log, "%p, ", (void *)a->args[4]);
        fprintf(tracee->log, "%p, ", (void *)a->args[5]);
    }
}

void print_msgrcv_args(struct stracee_t *tracee, uintptr_t ptr)
{
    size_t bufsz = ALIGNED_STRUCT_SZ(struct syscall_args);
    char buf[bufsz];
    struct syscall_args *a = (struct syscall_args *)buf;

    if(!ptr)
    {
        fprintf(tracee->log, "NULL");
        return;
    }

    if(tracee_get_bytes(tracee, ptr, buf, bufsz) != 0)
    {
        print_arg_ptr(tracee, ptr);
    }
    else
    {
        fprintf(tracee->log, "%d, ", (int)a->args[0]);
        fprintf(tracee->log, "%p, ", (void *)a->args[1]);
        fprintf(tracee->log, "%lu, ", (size_t)a->args[2]);
        fprintf(tracee->log, "%ld, ", (long)a->args[3]);
        fprintf(tracee->log, "%d, ", (int)a->args[4]);
        fprintf(tracee->log, "%p, ", (size_t *)a->args[5]);
    }
}

