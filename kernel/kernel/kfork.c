/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kfork.c
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
 *  \file kfork.c
 *
 *  Kernel fork() implementation.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/thread.h>
#include <kernel/asm.h>
#include <kernel/fpu.h>
#include <kernel/timer.h>
#include <kernel/ksignal.h>
#include <kernel/ptrace.h>
#include <kernel/syscall.h>
#include <kernel/ksigset.h>
#include <mm/kstack.h>
#include <mm/kheap.h>
#include <fs/procfs.h>

#include "task_funcs.c"


static struct task_t *dup_task(struct task_t *parent, int share_parent_structs)
{
    int i;
    pid_t pid;
    struct task_t *new_task = task_alloc();

    if(!new_task)
    {
        return NULL;
    }
    
    /* save the pid as we'll overwrite it in the following code */
    pid = new_task->pid;
    
    /* store these pointers before we overwrite them with the copy below */
    struct task_files_t *files = new_task->ofiles;
    struct task_fs_t *fs = new_task->fs;
    struct task_sig_t *sig = new_task->sig;
    struct task_threads_t *threads = new_task->threads;
    struct task_common_t *common = new_task->common;
    
    /* for starters, copy the parent task to the child */
    A_memcpy((void *)new_task, (void *)parent, sizeof(struct task_t));
    
    /* now restore the saved pointers */
    new_task->ofiles = files;
    new_task->fs = fs;
    new_task->sig = sig;
    new_task->threads = threads;
    new_task->common = common;
    
    init_kernel_mutex(&new_task->task_mutex);

    if(share_parent_structs)
    {
        new_task->parent = parent->parent;

        kfree(new_task->ofiles);
        kfree(new_task->fs);
        kfree(new_task->sig);
        kfree(new_task->threads);
        kfree(new_task->common);
        
        new_task->ofiles = parent->ofiles;
        new_task->fs = parent->fs;
        new_task->sig = parent->sig;
        new_task->threads = parent->threads;
        new_task->common = parent->common;

        kernel_mutex_lock(&(parent->mem->mutex));
        new_task->mem = parent->mem;
        kernel_mutex_unlock(&(parent->mem->mutex));
        
        kernel_mutex_lock(&(new_task->threads->mutex));
        new_task->thread_group_next =
                new_task->threads->thread_group_leader->thread_group_next;
        new_task->threads->thread_group_leader->thread_group_next = new_task;

        new_task->threads->thread_count++;
        kernel_mutex_unlock(&(new_task->threads->mutex));
    }
    else
    {
        new_task->parent = parent;

        A_memcpy(new_task->ofiles, parent->ofiles, 
                                sizeof(struct task_files_t));
        A_memcpy(new_task->fs, parent->fs, sizeof(struct task_fs_t));
        A_memcpy(new_task->sig, parent->sig, sizeof(struct task_sig_t));
        A_memcpy(new_task->common, parent->common, 
                                sizeof(struct task_common_t));
        
        kernel_mutex_lock(&(parent->mem->mutex));
        new_task->mem = task_mem_dup(parent->mem);
        kernel_mutex_unlock(&(parent->mem->mutex));

        if(new_task->mem == NULL)
        {
            task_free(new_task, 1);
            return NULL;
        }

        init_kernel_mutex(&new_task->ofiles->mutex);
        init_kernel_mutex(&new_task->fs->mutex);
        init_kernel_mutex(&new_task->threads->mutex);

        new_task->threads->thread_group_leader = new_task;
        new_task->threads->thread_count = 1;
        new_task->threads->tgid = pid;
        new_task->thread_group_next = NULL;

        if(parent->fs->root)
        {
            new_task->fs->root->refs++;
        }

        if(parent->fs->cwd)
        {
            new_task->fs->cwd->refs++;
        }

        /* increment open file refs */
        for(i = 0; i < NR_OPEN; i++)
        {
            /* TODO: this call should take care of releasing file locks */
            if(new_task->ofiles->ofile[i])
            {
                new_task->ofiles->ofile[i]->refs++;
            }
        }
        
        /* child doesn't inherit parent's interval timers */
        A_memset(&new_task->itimer_real, 0, sizeof(struct itimer_t));
        A_memset(&new_task->itimer_virt, 0, sizeof(struct itimer_t));
        A_memset(&new_task->itimer_prof, 0, sizeof(struct itimer_t));
    }

    /*
     * Things to keep in the forked child:
     *   - signal stack
     *   - signal dispositions
     *   - signal mask
     */
    
    new_task->children = 0;
    new_task->first_child = 0;
    new_task->first_sibling = 0;
    new_task->pid = pid;
    A_memset(&new_task->next, 0, sizeof(new_task->next));
    new_task->minflt = 0;
    new_task->majflt = 0;
    new_task->children_minflt = 0;
    new_task->children_majflt = 0;
    new_task->start_time = ticks;
    
    /* reset times */
    new_task->user_time = 0;
    new_task->sys_time = 0;
    new_task->children_user_time = 0;
    new_task->children_sys_time = 0;

    /* clear pending signals */
    ksigemptyset((sigset_t *)&new_task->signal_pending);
    ksigemptyset(&new_task->signal_caught);
    new_task->woke_by_signal = 0;
    
    /* reset counters */
    new_task->read_count = 0;
    new_task->write_count = 0;
    new_task->read_calls = 0;
    new_task->write_calls = 0;
    
    ptrace_clear_state(new_task);

    new_task->properties &= ~(PROPERTY_VFORK);
    task_add_child(new_task->parent, new_task);
    
    return new_task;
}


/*
 * Handler for syscall fork().
 */
int syscall_fork(void)
{
    static int first_fork = 1;
    
    struct task_t *parent = cur_task;

    int vforking = (GET_SYSCALL_NUMBER(parent->regs) == __NR_vfork);
    int cloning = (GET_SYSCALL_NUMBER(parent->regs) == __NR_clone);
    
    //printk("syscall_fork:\n");
    //screen_refresh(NULL);

    if(cloning && parent->threads->thread_count >= THREADS_PER_PROCESS)
    {
        return -ENFILE;
    }
    
    /* duplicate parent */
    struct task_t *new_task = dup_task(parent, cloning);
    struct regs r;

    if(!new_task)
    {
        return -EAGAIN;
    }
    
    A_memcpy(&r, parent->regs, sizeof(struct regs));

    /* if vforking, mark the child as such */
    if(vforking)
    {
        new_task->properties |= PROPERTY_VFORK;
    }

    // user esp is passed as the 2nd argument to the clone syscall (%ecx on
    // x86, %rsi on x86-64)
    if(cloning)
    {
#ifdef __x86_64__
        r.userrsp = GET_SYSCALL_ARG2(&r);
#else
        r.useresp = GET_SYSCALL_ARG2(&r);
#endif
    }
    
#ifdef __x86_64__
    r.rax = 0;
#else
    r.eax = 0;
#endif

    /* 
     * clone the page directory (if vforking, parent and child share memory and
     * no copy-on-write is applied).
     */
    if(!vforking && !cloning)
    {
        if(clone_task_pd(parent, new_task) != 0)
        //if(clone_task_pd(parent, new_task, 1) != 0)
        {
            task_free(new_task, 1);
            return -EAGAIN;
        }
    }

    /* create a new kstack */
    if(get_kstack(&new_task->kstack_phys, &new_task->kstack_virt) != 0)
    {
        free_pd(new_task->pd_virt);
        task_free(new_task, (!vforking && !cloning));
        return -EAGAIN;
    }

    // get a pointer to the new task's stack
    uintptr_t sp = (uintptr_t)new_task->kstack_virt;

    /* first fork - init task */
    if(first_fork)
    {
        set_init_task(new_task);
        new_task->parent = new_task;
        //new_task->user = 1;
        new_task->nice = 40;
        first_fork = 0;
    }

#define PUSH(v)             sp -= sizeof(uintptr_t);        \
                            *(volatile uintptr_t *)sp = (uintptr_t)(v);

    if(!new_task->user)
    {
#ifdef __x86_64__
        r.userrsp = sp;
        r.rbp = sp;
#else
        r.useresp = sp;
        r.ebp = sp;
#endif
    }
    else
    {
        r.cs = 0x1b;
        r.ss = 0x23;
    }

    KDEBUG("do_fork: 5a - sp " _XPTR_ "\n", sp);

    // bootstrap new task's stack
    sp -= sizeof(struct regs);
    A_memcpy((void *)sp, &r, sizeof(struct regs));
    new_task->regs = (struct regs *)sp;
    PUSH(resume_user);

#undef PUSH

#ifdef __x86_64__
    new_task->saved_context.rsp = (uintptr_t)sp;
    new_task->saved_context.rbp = (uintptr_t)new_task->kstack_virt;
    new_task->saved_context.rflags &= ~0x200;
#else
    new_task->saved_context.esp = (unsigned int)sp;
    new_task->saved_context.ebp = (unsigned int)new_task->kstack_virt;
    new_task->saved_context.eflags &= ~0x200;
    KDEBUG("do_fork: esp 0x%x\n", new_task->saved_context.esp);
#endif

    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    /* add to the end of ready queue */
    new_task->timeslice = get_task_timeslice(new_task);
    new_task->state = TASK_READY;
    reset_task_timeslice(new_task);
    
    lock_scheduler();
    append_to_ready_queue(new_task);
    unlock_scheduler();

    if(parent->properties & PROPERTY_TRACE_SIGNALS)
    {
        int event = 0;

        // clone()
        if(cloning && (parent->ptrace_options & PTRACE_O_TRACECLONE))
        {
            /*
             * If cloning and PTRACE_O_TRACECLONE is set, the ptrace manpage 
             * says:
             *    Stop the tracee at the next clone and automatically start 
             *    tracing the newly cloned process, which will start with a 
             *    SIGSTOP, or PTRACE_EVENT_STOP if PTRACE_SEIZE was used. 
             *    A waitpid by the tracer will return a status value such that
             *
             *        status>>8 == (SIGTRAP | (PTRACE_EVENT_CLONE<<8))
             *
             *    The PID of the new process can be retrieved with
             *    PTRACE_GETEVENTMSG.
             */
            event = PTRACE_EVENT_CLONE;
        }

        // vfork()
        if(vforking && (parent->ptrace_options & PTRACE_O_TRACEVFORK))
        {
            /*
             * If vforking and PTRACE_O_TRACEVFORK is set, the ptrace manpage 
             * says:
             *    Stop the tracee at the next vfork and automatically start 
             *    tracing the newly vforked process, which will start with a 
             *    SIGSTOP, or PTRACE_EVENT_STOP if PTRACE_SEIZE was used. 
             *    A waitpid by the tracer will return a status value such that
             *
             *        status>>8 == (SIGTRAP | (PTRACE_EVENT_VFORK<<8))
             *
             *    The PID of the new process can be retrieved with
             *    PTRACE_GETEVENTMSG.
             *
             * NOTE: PTRACE_O_TRACEVFORKDONE is handled below.
             */
            event = PTRACE_EVENT_VFORK;
        }

        // fork()
        if(!vforking && !cloning && 
                (parent->ptrace_options & PTRACE_O_TRACEFORK))
        {
            /*
             * If forking and PTRACE_O_TRACEFORK is set, the ptrace manpage 
             * says:
             *    Stop the tracee at the next fork and automatically start 
             *    tracing the newly forked process, which will start with a 
             *    SIGSTOP, or PTRACE_EVENT_STOP if PTRACE_SEIZE was used. 
             *    A waitpid by the tracer will return a status value such that
             *
             *        status>>8 == (SIGTRAP | (PTRACE_EVENT_FORK<<8))
             *
             *    The PID of the new process can be retrieved with
             *    PTRACE_GETEVENTMSG.
             */
            event = PTRACE_EVENT_FORK;
        }

        if(event)
        {
            ptrace_copy_state(new_task, parent);
            user_add_task_signal(new_task, SIGSTOP, 1);
            //parent->ptrace_eventmsg = tid;
            parent->ptrace_eventmsg = new_task->pid;
            ptrace_signal(SIGTRAP, event);
        }
    }

    /* if vforking, block the parent */
    if(vforking)
    {
        block_task(parent, 0);

        if((parent->properties & PROPERTY_TRACE_SIGNALS) &&
           (parent->ptrace_options & PTRACE_O_TRACEVFORKDONE))
        {
            //parent->ptrace_eventmsg = tid;
            parent->ptrace_eventmsg = new_task->pid;
            ptrace_signal(SIGTRAP, PTRACE_EVENT_VFORK_DONE);
        }
    }
    
    system_forks++;

    KDEBUG("do_fork: kstack phys 0x%lx, virt 0x%lx\n", new_task->kstack_phys, new_task->kstack_virt);

    // syscall clone returns thread's id, while syscall fork returns pid
    return cloning ? new_task->pid : new_task->threads->tgid;
}

