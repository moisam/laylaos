/*-
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
//#include <sys/systm.h>
#include <sys/file.h>
//#include <sys/unistd.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>

#include <kernel/task.h>
#include <kernel/mutex.h>
#include <kernel/user.h>

sysctlfn kern_sysctl;
sysctlfn hw_sysctl;
#ifdef DEBUG
sysctlfn debug_sysctl;
#endif

/*
extern sysctlfn vm_sysctl;
extern sysctlfn fs_sysctl;
extern sysctlfn net_sysctl;
extern sysctlfn cpu_sysctl;
*/

struct kernel_mutex_t sysctl_lock = { 0, };


int syscall_sysctl(struct __sysctl_args *__args)
{
    int error;
    size_t oldlen = 0;
    sysctlfn *fn = NULL;
    int name[CTL_MAXNAME];
    struct __sysctl_args args;
    struct task_t *ct = cur_task;
    
    if(!__args)
    {
        return -EINVAL;
    }

    if(copy_from_user(&args, __args, sizeof(struct __sysctl_args)) != 0)
    {
        return -EINVAL;
    }

    if(args.newval != NULL && !suser(ct))
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

#ifdef DEBUG
        case CTL_DEBUG:
            fn = debug_sysctl;
            break;
#endif

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
 * Attributes stored in the kernel.
 */
long hostid;
//int securelevel;

/*
 * kernel related system variables.
 */
int kern_sysctl(int *name, int namelen, void *oldp, size_t *oldlenp, 
                void *newp, size_t newlen)
{
    int error, inthostid;
    
    /* all sysctl names at this level are terminal */
    if (namelen != 1 && !(name[0] == KERN_PROC || name[0] == KERN_PROF))
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

        //case KERN_MAXVNODES:
        //    return(sysctl_int(oldp, oldlenp, newp, newlen, &desiredvnodes));

        case KERN_MAXPROC:
            return (sysctl_rdint(oldp, oldlenp, newp, NR_TASKS));

        case KERN_MAXFILES:
            return (sysctl_rdint(oldp, oldlenp, newp, NR_FILE));

        case KERN_ARGMAX:
            return (sysctl_rdint(oldp, oldlenp, newp, ARG_MAX));

        /*
        case KERN_SECURELVL:
            level = securelevel;
            if ((error = sysctl_int(oldp, oldlenp, newp, newlen, &level)) ||
                newp == NULL)
                return (error);
            if (level < securelevel && p->p_pid != 1)
                return (EPERM);
            securelevel = level;
            return (0);
        */

        case KERN_HOSTNAME:
            error = sysctl_string(oldp, oldlenp, newp, newlen,
                myname.nodename, _UTSNAME_LENGTH);
        
            return (error);

        case KERN_HOSTID:
            inthostid = hostid;  /* XXX assumes sizeof long <= sizeof int */
            error =  sysctl_int(oldp, oldlenp, newp, newlen, &inthostid);
            hostid = inthostid;
            return (error);

        case KERN_BOOTTIME:
            {
                struct timeval boottime;
                
                boottime.tv_sec = get_startup_time();
                boottime.tv_usec = 0;

                return (sysctl_rdstruct(oldp, oldlenp, newp, &boottime,
                                        sizeof(struct timeval)));
            }


        /*
        case KERN_CLOCKRATE:
            return (sysctl_clockrate(oldp, oldlenp));

        case KERN_BOOTTIME:
            return (sysctl_rdstruct(oldp, oldlenp, newp, &boottime,
                sizeof(struct timeval)));

        case KERN_VNODE:
            return (sysctl_vnode(oldp, oldlenp));

        case KERN_PROC:
            return (sysctl_doproc(name + 1, namelen - 1, oldp, oldlenp));

        case KERN_FILE:
            return (sysctl_file(oldp, oldlenp));

#ifdef GPROF
        case KERN_PROF:
            return (sysctl_doprof(name + 1, namelen - 1, oldp, oldlenp,
                newp, newlen));
#endif

        case KERN_POSIX1:
            return (sysctl_rdint(oldp, oldlenp, newp, _POSIX_VERSION));
    */

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

        /*
        case HW_USERMEM:
            return (sysctl_rdint(oldp, oldlenp, newp,
                ctob(physmem - cnt.v_wire_count)));
        */

        case HW_PAGESIZE:
            return (sysctl_rdint(oldp, oldlenp, newp, PAGE_SIZE));

        default:
            return -EOPNOTSUPP;
    }

    /* NOTREACHED */
}


#ifdef DEBUG

/*
 * Debugging related system variables.
 */
struct ctldebug debug0, debug1, debug2, debug3, debug4;
struct ctldebug debug5, debug6, debug7, debug8, debug9;
struct ctldebug debug10, debug11, debug12, debug13, debug14;
struct ctldebug debug15, debug16, debug17, debug18, debug19;

static struct ctldebug *debugvars[CTL_DEBUG_MAXID] =
{
    &debug0, &debug1, &debug2, &debug3, &debug4,
    &debug5, &debug6, &debug7, &debug8, &debug9,
    &debug10, &debug11, &debug12, &debug13, &debug14,
    &debug15, &debug16, &debug17, &debug18, &debug19,
};

int debug_sysctl(int *name, int namelen, void *oldp, size_t *oldlenp, 
                 void *newp, size_t newlen, struct proc *p)
{
    struct ctldebug *cdp;

    /* all sysctl names at this level are name and field */
    if(namelen != 2)
    {
        return -ENOTDIR;        /* overloaded */
    }
    
    cdp = debugvars[name[0]];
    
    if(cdp->debugname == 0)
    {
        return -EOPNOTSUPP;
    }
    
    switch(name[1])
    {
        case CTL_DEBUG_NAME:
            return (sysctl_rdstring(oldp, oldlenp, newp, cdp->debugname));

        case CTL_DEBUG_VALUE:
            return (sysctl_int(oldp, oldlenp, newp, newlen, cdp->debugvar));

        default:
            return -EOPNOTSUPP;
    }

    /* NOTREACHED */
}

#endif /* DEBUG */


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


#if 0

/*
 * Get file structures.
 */
int sysctl_file(char *where, size_t *sizep)
{
    //UNUSED(where);
    //UNUSED(sizep);
    
    int buflen, error;
    struct file *fp;
    char *start = where;

    buflen = *sizep;
    if (where == NULL) {
        /*
         * overestimate by 10 files
         */
        *sizep = sizeof(filehead) + (nfiles + 10) * sizeof(struct file);
        return (0);
    }

    /*
     * first copyout filehead
     */
    if (buflen < sizeof(filehead)) {
        *sizep = 0;
        return (0);
    }
    if (error = copy_to_user(where, &filehead, sizeof(filehead)))
    //if (error = copyout((caddr_t)&filehead, where, sizeof(filehead)))
        return (error);
    buflen -= sizeof(filehead);
    where += sizeof(filehead);

    /*
     * followed by an array of file structures
     */
    for (fp = filehead; fp != NULL; fp = fp->f_filef) {
        if (buflen < sizeof(struct file)) {
            *sizep = where - start;
            return (ENOMEM);
        }
        if (error = copy_to_user(where, fp, sizeof (struct file)))
        //if (error = copyout((caddr_t)fp, where, sizeof (struct file)))
            return (error);
        buflen -= sizeof(struct file);
        where += sizeof(struct file);
    }
    *sizep = where - start;

    return (0);
}

#endif


/*
 * try over estimating by 5 procs
 */
#define KERN_PROCSLOP    (5 * sizeof (struct kinfo_proc))

int sysctl_doproc(int *name, u_int namelen, char *where, size_t *sizep)
{
    register struct kinfo_proc *dp = (struct kinfo_proc *)where;
    register size_t needed = 0;
    int buflen = where != NULL ? *sizep : 0;
    struct eproc eproc;
    int error = 0;

    if(namelen != 2 && !(namelen == 1 && name[0] == KERN_PROC_ALL))
    {
        return -EINVAL;
    }
    
    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        volatile struct task_t tmpt;

        /*
         * Skip embryonic processes.
         */
        if((*t)->state == TASK_IDLE)
        {
            continue;
        }
        
        /*
         * TODO - make more efficient (see notes below).
         * do by session.
         */
        switch (name[0])
        {
            case KERN_PROC_PID:
                /* could do this with just a lookup */
                if((*t)->pid != (pid_t)name[1])
                {
                    continue;
                }
                break;

            case KERN_PROC_PGRP:
                /* could do this by traversing pgrp */
                if((*t)->pgid != (pid_t)name[1])
                {
                    continue;
                }
                break;

            case KERN_PROC_TTY:
                if((*t)->ctty <= 0 ||
                   (*t)->ctty != (dev_t)name[1])
                {
                    continue;
                }
                break;

            case KERN_PROC_UID:
                if((*t)->euid != (uid_t)name[1])
                {
                    continue;
                }
                break;

            case KERN_PROC_RUID:
                if((*t)->uid != (uid_t)name[1])
                {
                    continue;
                }
                break;
        }

        /*
         * Make a local copy so we can release the master task table's lock
         * for someone else who might need to access the table. We also don't
         * want to SIGFAULT while copying to userspace and die holding the
         * table's lock.
         */
        memcpy((void *)&tmpt, *t, sizeof(struct task_t));
        elevated_priority_unlock(&task_table_lock);

        if(buflen >= (int)sizeof(struct kinfo_proc))
        {
            fill_eproc((struct task_t *)&tmpt, &eproc);

            if((error = copy_to_user(&dp->kp_proc, (void *)&tmpt, 
                                     sizeof(struct task_t))))
            {
                return error;
            }

            if((error = copy_to_user(&dp->kp_eproc, &eproc, sizeof(eproc))))
            {
                return error;
            }
            
            dp++;
            buflen -= sizeof(struct kinfo_proc);
        }

        needed += sizeof(struct kinfo_proc);

        elevated_priority_relock(&task_table_lock);
    }

    elevated_priority_unlock(&task_table_lock);

    if(where != NULL)
    {
        *sizep = (caddr_t)dp - where;

        if(needed > *sizep)
        {
            return -ENOMEM;
        }
    }
    else
    {
        needed += KERN_PROCSLOP;
        *sizep = needed;
    }

    return 0;
}

/*
 * Fill in an eproc structure for the specified process.
 */
void fill_eproc(struct task_t *p, struct eproc *ep)
{
    int i;

    ep->e_paddr = p;
    ep->e_sess = 0;

    /*
     * NOTE: this is not filled in properly. See sys/ucred.h.
     *
     * TODO: this!
     */

    ep->e_ucred.cr_ref = 1;
    ep->e_ucred.cr_uid = p->uid;
    ep->e_ucred.cr_ngroups = 0;
    
    for(i = 0; i < NGROUPS_MAX; i++)
    {
        ep->e_ucred.cr_groups[i] = p->extra_groups[i];
        
        if(p->extra_groups[i] != (gid_t)-1)
        {
            ep->e_ucred.cr_ngroups++;
        }
    }

    ep->e_pcred.pc_ucred = 0;
    ep->e_pcred.p_ruid = p->uid;
    ep->e_pcred.p_svuid = p->ssuid;
    ep->e_pcred.p_rgid = p->gid;
    ep->e_pcred.p_svgid = p->ssgid;
    ep->e_pcred.p_refcnt = 1;


    ep->e_ppid = p->parent->pid;
    ep->e_pgid = p->pgid;
    if(group_leader(p))
        ep->e_flag |= EPROC_SLEADER;


#if 0
    if (p->p_stat == SIDL || p->p_stat == SZOMB) {
        ep->e_vm.vm_rssize = 0;
        ep->e_vm.vm_tsize = 0;
        ep->e_vm.vm_dsize = 0;
        ep->e_vm.vm_ssize = 0;
#ifndef sparc
        /* ep->e_vm.vm_pmap = XXX; */
#endif
    } else {
        register struct vmspace *vm = p->p_vmspace;

        ep->e_vm.vm_rssize = vm->vm_rssize;
        ep->e_vm.vm_tsize = vm->vm_tsize;
        ep->e_vm.vm_dsize = vm->vm_dsize;
        ep->e_vm.vm_ssize = vm->vm_ssize;
#ifndef sparc
        ep->e_vm.vm_pmap = vm->vm_pmap;
#endif
    }
    ep->e_jobc = p->p_pgrp->pg_jobc;
    if ((p->p_flag & P_CONTROLT) &&
         (tp = ep->e_sess->s_ttyp)) {
        ep->e_tdev = tp->t_dev;
        ep->e_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
        ep->e_tsess = tp->t_session;
    } else
        ep->e_tdev = NODEV;
    ep->e_flag = ep->e_sess->s_ttyvp ? EPROC_CTTY : 0;
    if (p->p_wmesg)
        strncpy(ep->e_wmesg, p->p_wmesg, WMESGLEN);
    ep->e_xsize = ep->e_xrssize = 0;
    ep->e_xccount = ep->e_xswrss = 0;
#endif
}

