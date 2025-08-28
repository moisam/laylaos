/*-
 * Copyright (c) 2021, 2022, 2023, 2024, 2025
 *    Mohammed Isam [mohammed_isam1984@yahoo.com]
 * Copyright (c) 1982, 1986, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 *  \file kern_sysctl.c
 *
 *  Handler for the sysctl system call.
 */

#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>

#include <kernel/task.h>
#include <kernel/mutex.h>
#include <kernel/user.h>

sysctlfn kern_sysctl;
sysctlfn hw_sysctl;

/*
extern sysctlfn vm_sysctl;
extern sysctlfn fs_sysctl;
extern sysctlfn net_sysctl;
extern sysctlfn cpu_sysctl;
*/

volatile struct kernel_mutex_t sysctl_lock = { 0, };


int syscall_sysctl(struct __sysctl_args *__args)
{
    int error;
    size_t oldlen = 0;
    sysctlfn *fn = NULL;
    int name[CTL_MAXNAME];
    struct __sysctl_args args;
    
    if(!__args)
    {
        return -EINVAL;
    }

    if(copy_from_user(&args, __args, sizeof(struct __sysctl_args)) != 0)
    {
        return -EINVAL;
    }

    if(args.newval != NULL && !suser(this_core->cur_task))
    {
        return -EPERM;
    }
    
    /*
     * all top-level sysctl names are non-terminal
     */
    if(args.nlen > CTL_MAXNAME || args.nlen < 2)
    {
        return -EINVAL;
    }
    
    if ((error = copy_from_user(&name, args.name, args.nlen * sizeof(int))))
    {
        return error;
    }

    switch (name[0])
    {
        case CTL_KERN:
            fn = kern_sysctl;
            break;

        case CTL_HW:
            fn = hw_sysctl;
            break;

        /*
        case CTL_VM:
            fn = vm_sysctl;
            break;

        case CTL_NET:
            fn = net_sysctl;
            break;

        case CTL_FS:
            fn = fs_sysctl;
            break;

        case CTL_MACHDEP:
            fn = cpu_sysctl;
            break;
        */

        default:
            return -EOPNOTSUPP;
    }

    if(args.oldlenp &&
       (error = copy_from_user(&oldlen, args.oldlenp, sizeof(oldlen))))
    {
        return error;
    }
    
    if(args.oldval != NULL)
    {
        kernel_mutex_lock(&sysctl_lock);
    }
    
    error = (*fn)(name + 1, args.nlen - 1, args.oldval, &oldlen, 
                                           args.newval, args.newlen);
    
    if(args.oldval != NULL)
    {
        kernel_mutex_unlock(&sysctl_lock);
    }
    
    if(error)
    {
        return error;
    }
    
    if(args.oldlenp)
    {
        error = copy_to_user(args.oldlenp, &oldlen, sizeof(oldlen));
    }
    
    return oldlen;
}


/*
 * kernel related system variables.
 */
int kern_sysctl(int *name, int namelen, void *oldp, size_t *oldlenp, 
                void *newp, size_t newlen)
{
    /* all sysctl names at this level are terminal */
    if(namelen != 1)
    {
        return -ENOTDIR;        /* overloaded */
    }

    switch(name[0])
    {
        case KERN_OSTYPE:
            return (sysctl_rdstring(oldp, oldlenp, newp, ostype));

        case KERN_OSRELEASE:
            return (sysctl_rdstring(oldp, oldlenp, newp, osrelease));

        case KERN_OSREV:
            return (sysctl_rdint(oldp, oldlenp, newp, osrev));

        case KERN_VERSION:
            return (sysctl_rdstring(oldp, oldlenp, newp, version));

        case KERN_MAXPROC:
            return (sysctl_rdint(oldp, oldlenp, newp, NR_TASKS));

        case KERN_MAXFILES:
            return (sysctl_rdint(oldp, oldlenp, newp, NR_FILE));

        case KERN_ARGMAX:
            return (sysctl_rdint(oldp, oldlenp, newp, ARG_MAX));

        case KERN_HOSTNAME:
            return sysctl_string(oldp, oldlenp, newp, newlen,
                myname.nodename, _UTSNAME_LENGTH);

        case KERN_BOOTTIME:
            {
                struct timeval boottime;
                
                boottime.tv_sec = get_startup_time();
                boottime.tv_usec = 0;

                return (sysctl_rdstruct(oldp, oldlenp, newp, &boottime,
                                        sizeof(struct timeval)));
            }

        case KERN_POSIX1:
            return (sysctl_rdint(oldp, oldlenp, newp, _POSIX_VERSION));

        case KERN_NGROUPS:
            return (sysctl_rdint(oldp, oldlenp, newp, NGROUPS_MAX));

        case KERN_JOB_CONTROL:
            return (sysctl_rdint(oldp, oldlenp, newp, 1));

        case KERN_SAVED_IDS:
#ifdef _POSIX_SAVED_IDS
            return (sysctl_rdint(oldp, oldlenp, newp, 1));
#else
            return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif

        default:
            return -EOPNOTSUPP;
    }

    /* NOTREACHED */
}


/*
 * hardware related system variables.
 */
int hw_sysctl(int *name, int namelen, void *oldp, size_t *oldlenp, 
              void *newp, size_t newlen)
{
    UNUSED(newlen);
    
    /* all sysctl names at this level are terminal */
    if(namelen != 1)
    {
        return -ENOTDIR;        /* overloaded */
    }

    switch(name[0])
    {
        case HW_MACHINE:
            return (sysctl_rdstring(oldp, oldlenp, newp, machine));

        case HW_MODEL:
            return (sysctl_rdstring(oldp, oldlenp, newp, cpu_model));

        case HW_CPU_FREQ:
            return (sysctl_rdint(oldp, oldlenp, newp, 1));    /* XXX */

        case HW_NCPU:
            return (sysctl_rdint(oldp, oldlenp, newp, 1));    /* XXX */

        case HW_BYTEORDER:
            return (sysctl_rdint(oldp, oldlenp, newp, BYTE_ORDER));

        case HW_PHYSMEM:
            return (sysctl_rdint(oldp, oldlenp, newp,
                                (pmmngr_get_block_count() * PAGE_SIZE)));

        case HW_USERMEM:
            return (sysctl_rdint(oldp, oldlenp, newp,
                         ((pmmngr_get_block_count() - 
                            memregion_kernel_pagecount((struct task_t *)
                                                        this_core->cur_task)) *
                                        PAGE_SIZE)));

        case HW_PAGESIZE:
            return (sysctl_rdint(oldp, oldlenp, newp, PAGE_SIZE));

        default:
            return -EOPNOTSUPP;
    }

    /* NOTREACHED */
}


/*
 * Validate parameters and get old / set new parameters
 * for an integer-valued sysctl function.
 */
int sysctl_int(void *oldp, size_t *oldlenp, 
               void *newp, size_t newlen, int *valp)
{
    int error = 0;

    if(oldp && *oldlenp < sizeof(int))
    {
        return -ENOMEM;
    }
    
    if(newp && newlen != sizeof(int))
    {
        return -EINVAL;
    }
    
    *oldlenp = sizeof(int);
    
    if(oldp)
    {
        error = copy_to_user(oldp, valp, sizeof(int));
    }
    
    if(error == 0 && newp)
    {
        error = copy_from_user(valp, newp, sizeof(int));
    }
    
    return error;
}


/*
 * As above, but read-only.
 */
int sysctl_rdint(void *oldp, size_t *oldlenp, void *newp, int val)
{
    int error = 0;

    if(oldp && *oldlenp < sizeof(int))
    {
        return -ENOMEM;
    }
    
    if(newp)
    {
        return -EPERM;
    }

    *oldlenp = sizeof(int);

    if(oldp)
    {
        error = copy_to_user(oldp, &val, sizeof(int));
    }
    
    return error;
}


/*
 * Validate parameters and get old / set new parameters
 * for a string-valued sysctl function.
 */
int sysctl_string(void *oldp, size_t *oldlenp, void *newp, size_t newlen, 
                  char *str, int maxlen)
{
    int len, error = 0;

    len = strlen(str) + 1;

    if(oldp && *oldlenp < (size_t)len)
    {
        return -ENOMEM;
    }
    
    if(newp && newlen >= (size_t)maxlen)
    {
        return -EINVAL;
    }
    
    if(oldp)
    {
        *oldlenp = len;
        error = copy_to_user(oldp, str, len);
    }
    
    if(error == 0 && newp)
    {
        error = copy_from_user(str, newp, newlen);
        str[newlen] = 0;
    }
    
    return error;
}


/*
 * As above, but read-only.
 */
int sysctl_rdstring(void *oldp, size_t *oldlenp, void *newp, char *str)
{
    int len, error = 0;

    len = strlen(str) + 1;

    if(oldp && *oldlenp < (size_t)len)
    {
        return -ENOMEM;
    }
    
    if(newp)
    {
        return -EPERM;
    }
    
    *oldlenp = len;

    if(oldp)
    {
        error = copy_to_user(oldp, str, len);
    }
    
    return error;
}


/*
 * Validate parameters and get old / set new parameters
 * for a structure oriented sysctl function.
 */
int sysctl_struct(void *oldp, size_t *oldlenp, void *newp, size_t newlen, 
                  void *sp, int len)
{
    int error = 0;

    if(oldp && *oldlenp < (size_t)len)
    {
        return -ENOMEM;
    }
    
    if(newp && newlen > (size_t)len)
    {
        return -EINVAL;
    }
    
    if(oldp)
    {
        *oldlenp = len;
        error = copy_to_user(oldp, sp, len);
    }

    if(error == 0 && newp)
    {
        error = copy_from_user(sp, newp, len);
    }
    
    return error;
}


/*
 * Validate parameters and get old parameters
 * for a structure oriented sysctl function.
 */
int sysctl_rdstruct(void *oldp, size_t *oldlenp, void *newp, void *sp, int len)
{
    int error = 0;

    if(oldp && *oldlenp < (size_t)len)
    {
        return -ENOMEM;
    }
    
    if(newp)
    {
        return -EPERM;
    }
    
    *oldlenp = len;

    if(oldp)
    {
        error = copy_to_user(oldp, sp, len);
    }
    
    return error;
}

