/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: ptrace.c
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
 *  \file ptrace.c
 *
 *  The kernel's process tracing (ptrace) implementation.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <kernel/laylaos.h>
#include <kernel/ksignal.h>
#include <kernel/thread.h>
#include <kernel/user.h>
#include <kernel/ptrace.h>
#include <kernel/gdt.h>         // struct user_desc
#include <kernel/syscall.h>
#include <fs/procfs.h>
#ifndef KERNEL
# define KERNEL
#endif
#include <sys/list.h>

#include "../kernel/task_funcs.c"


/*
 * Helper function to get tracee's general-purpose registers.
 */
static void get_regs(volatile struct task_t *tracee, struct user_regs_struct *rdest)
{
    volatile struct regs *rsrc = &tracee->saved_context;

    // NOTE: this shouldn't happen
    if(!rsrc)
    {
        kpanic("Invalid regs pointer in task struct (in get_regs)");
    }

#ifdef __x86_64__

    rdest->rbx = rsrc->rbx;
    rdest->rcx = rsrc->rcx;
    rdest->rdx = rsrc->rdx;
    rdest->rsi = rsrc->rsi;
    rdest->rdi = rsrc->rdi;
    rdest->rbp = rsrc->rbp;
    rdest->rax = rsrc->rax;
    rdest->ds = 0x23;
    rdest->es = 0x23;
    rdest->fs = 0x23;
    rdest->gs = 0x23;
    rdest->orig_rax = 0;    // TODO: what is the right value here?
    rdest->rip = rsrc->rip;
    rdest->cs = rsrc->cs;
    rdest->eflags = rsrc->rflags;
    rdest->rsp = rsrc->rsp;
    rdest->ss = rsrc->ss;
    rdest->r8 = rsrc->r8;
    rdest->r9 = rsrc->r9;
    rdest->r10 = rsrc->r10;
    rdest->r11 = rsrc->r11;
    rdest->r12 = rsrc->r12;
    rdest->r13 = rsrc->r13;
    rdest->r14 = rsrc->r14;
    rdest->r15 = rsrc->r15;

#else

    rdest->ebx = rsrc->ebx;
    rdest->ecx = rsrc->ecx;
    rdest->edx = rsrc->edx;
    rdest->esi = rsrc->esi;
    rdest->edi = rsrc->edi;
    rdest->ebp = rsrc->ebp;
    rdest->eax = rsrc->eax;
    rdest->xds = rsrc->ds;
    rdest->xes = rsrc->es;
    rdest->xfs = rsrc->fs;
    rdest->xgs = rsrc->gs;
    rdest->orig_eax = 0;    // TODO: what is the right value here?
    rdest->eip = rsrc->eip;
    rdest->xcs = rsrc->cs;
    rdest->eflags = rsrc->eflags;
    rdest->esp = rsrc->esp;
    rdest->xss = rsrc->ss;

#endif

}


/*
 * Helper function to get tracee's floating-point registers.
 */
static void get_fpregs(volatile struct task_t *tracee, struct user_fpregs_struct *r)
{

#ifdef __x86_64__

    // https://www.felixcloutier.com/x86/fxsave

    // http://www.jaist.ac.jp/iscenter-new/mpc/altix/altixdata/opt/intel/vtune/doc/users_guide/mergedProjects/analyzer_ec/mergedProjects/reference_olh/mergedProjects/instructions/instruct32_hh/vc129.htm

    A_memcpy(r, (void *)&tracee->fpregs, 512);

#else

    r->cwd = tracee->i387.cwd;
    r->swd = tracee->i387.swd;
    r->twd = tracee->i387.twd;
    r->fip = tracee->i387.fip;
    r->fcs = tracee->i387.fcs;
    r->foo = tracee->i387.foo;
    r->fos = tracee->i387.fos;
    A_memcpy(r->st_space, tracee->i387.st_space, 
                                sizeof(tracee->i387.st_space));

#endif

}


/*
 * Helper function to set tracee's general-purpose registers.
 * Some registers are not set to ensure we keep the kernel sane.
 */
static void set_regs(volatile struct task_t *tracee, struct user_regs_struct *rsrc)
{
    volatile struct regs *rdest = &tracee->saved_context;

    // NOTE: this shouldn't happen
    if(!rdest)
    {
        kpanic("Invalid regs pointer in task struct (in set_regs)");
    }

#ifdef __x86_64__

    rdest->rbx = rsrc->rbx;
    rdest->rcx = rsrc->rcx;
    rdest->rdx = rsrc->rdx;
    rdest->rsi = rsrc->rsi;
    rdest->rdi = rsrc->rdi;
    rdest->rbp = rsrc->rbp;
    rdest->rax = rsrc->rax;
    rdest->r8 = rsrc->r8;
    rdest->r9 = rsrc->r9;
    rdest->r10 = rsrc->r10;
    rdest->r11 = rsrc->r11;
    rdest->r12 = rsrc->r12;
    rdest->r13 = rsrc->r13;
    rdest->r14 = rsrc->r14;
    rdest->r15 = rsrc->r15;

#else

    rdest->ebx = rsrc->ebx;
    rdest->ecx = rsrc->ecx;
    rdest->edx = rsrc->edx;
    rdest->esi = rsrc->esi;
    rdest->edi = rsrc->edi;
    rdest->ebp = rsrc->ebp;
    rdest->eax = rsrc->eax;

#endif

}


/*
 * Helper function to set tracee's floating-point registers.
 */
static void set_fpregs(volatile struct task_t *tracee, struct user_fpregs_struct *r)
{

#ifdef __x86_64__

    /*
     * TODO: validate input before copying it blindly.
     */
    A_memcpy((void *)&tracee->fpregs, r, 512);

#else

    tracee->i387.cwd = r->cwd;
    tracee->i387.swd = r->swd;
    tracee->i387.twd = r->twd;
    tracee->i387.fip = r->fip;
    tracee->i387.fcs = r->fcs;
    tracee->i387.foo = r->foo;
    tracee->i387.fos = r->fos;

    A_memcpy(tracee->i387.st_space, r->st_space, 
                                sizeof(tracee->i387.st_space));

#endif

}


/*
 * Send a SIGKILL signal to tracees if the tracer exits.
 */
void ptrace_kill_tracees(struct task_t *tracer)
{
    struct list_item_t *item;
    struct task_t *tracee;

    if(!tracer->tracees)
    {
        return;
    }

    for(item = tracer->tracees->head; item != NULL; item = item->next)
    {
        tracee = (struct task_t *)item->val;
        
        if(tracee)
        {
            if(tracee->ptrace_options & PTRACE_O_EXITKILL)
            {
                user_add_task_signal(tracee, SIGKILL, 1);
            }
            else
            {
                ptrace_clear_state(tracee);
                unblock_task_no_preempt(tracee);
            }
        }
    }
}


/*
 * Set the tracee's tracer, and add the tracee to the tracer's tracees list.
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_set_tracer(volatile struct task_t *tracee, volatile struct task_t *tracer)
{
    if(!tracee || !tracer)
    {
        return -ESRCH;
    }
    
    // if the tracee is already being traced, return 0 if the request is for
    // the same tracer, or -EPERM otherwise
    if(tracee->tracer_pid)
    {
        return (tracee->tracer_pid == tracer->pid) ? 0 : -EPERM;
    }
    
    // mark the tracee as being traced
    __sync_or_and_fetch(&tracee->properties, (PROPERTY_TRACE_SYSCALLS | PROPERTY_TRACE_SIGNALS));
    
    // and add it to the tracer's list
    kernel_mutex_lock(&tracer->task_mutex);
    
    if(!tracer->tracees && !(tracer->tracees = list_create()))
    {
        kernel_mutex_unlock(&tracer->task_mutex);
        __sync_and_and_fetch(&tracee->properties, 
                        ~(PROPERTY_TRACE_SYSCALLS | PROPERTY_TRACE_SIGNALS));
        return -EBUSY;
    }
    
    if(!list_lookup(tracer->tracees, (void *)tracee))
    {
        list_add(tracer->tracees, (void *)tracee);
    }
    
    kernel_mutex_unlock(&tracer->task_mutex);
    tracee->tracer_pid = tracer->pid;
    
    return 0;
}


/*
 * Clear ptrace state.
 */
void ptrace_clear_state(volatile struct task_t *tracee)
{
    volatile struct task_t *tracer;

    if(tracee->tracer_pid && (tracer = get_task_by_tid(tracee->tracer_pid)))
    {
        kernel_mutex_lock(&tracer->task_mutex);
        list_remove(tracer->tracees, (struct task_t *)tracee);
        kernel_mutex_unlock(&tracer->task_mutex);
    }

    __sync_and_and_fetch(&tracee->properties, ~(PROPERTY_TRACE_SYSCALLS | PROPERTY_TRACE_SIGNALS));
    tracee->tracer_pid = 0;
    tracee->ptrace_options = 0;
    tracee->ptrace_eventmsg = 0;
}


/*
 * Copy ptrace state.
 */
void ptrace_copy_state(struct task_t *tracee2, struct task_t *tracee)
{
    if(ptrace_set_tracer(tracee2, get_task_by_tid(tracee->tracer_pid)) == 0)
    {
        tracee2->ptrace_options = tracee->ptrace_options;
    }
    
    tracee->ptrace_eventmsg = 0;
}


/*
 * Signal tracer.
 */
long ptrace_signal(int signum, int reason)
{
    volatile struct task_t *tracee = this_core->cur_task;
    volatile struct task_t *tracer;

    tracer = get_task_by_tid(tracee->tracer_pid);
    
    if(!tracer)
    {
        return signum;
    }

    tracee->exit_status = __W_STOPCODE(signum) | (reason << 16);
    __sync_or_and_fetch(&tracee->properties, PROPERTY_TRACE_SUSPENDED);


    if(signum == SIGTRAP || signum == (SIGTRAP | 0x80))
    {
        //struct sigaction *action = &tracee->sig->signal_actions[SIGTRAP];

        tracee->siginfo[SIGTRAP].si_signo = SIGTRAP;

        if(reason == PTRACE_EVENT_SYSCALL_ENTER ||
           reason == PTRACE_EVENT_SYSCALL_EXIT)
        {
            tracee->siginfo[SIGTRAP].si_code = signum;
        }
        else
        {
            tracee->siginfo[SIGTRAP].si_code = SI_KERNEL;
        }
    }

    schedule_and_block(tracer, tracee);

    signum = WSTOPSIG(tracee->exit_status);
    tracee->exit_status = __W_CONTINUED;
    
    return signum;
}


/*
 * Helper function to send a signal and continue the tracee.
 *
 * Returns:
 *    always 0.
 */
static long signal_and_continue(volatile struct task_t *tracee, int signum)
{
    int sigpending;

    __sync_and_and_fetch(&tracee->properties, ~PROPERTY_TRACE_SUSPENDED);
    sigpending = WSTOPSIG(tracee->exit_status);
    
    // check if the tracees has a pending signal
    if(sigpending && !(tracee->exit_status >> 16))
    {
        tracee->exit_status = __W_STOPCODE(signum);
        //sigdelset(&tracee->signal_pending, sigpending);
        unblock_task(tracee);
    }
    else if(signum)
    {
        user_add_task_signal(tracee, signum, 1);
    }
    else
    {
        unblock_task(tracee);
    }
    
    return 0;
}


/*
 * Indicate that this process is to be traced by its parent.
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_traceme(void)
{
    volatile struct task_t *tracee = this_core->cur_task;
    volatile struct task_t *tracer = tracee->tracer_pid ?
                                     get_task_by_tid(tracee->tracer_pid) : NULL;

    // already being traced
    if(tracer)
    {
        return -EPERM;
    }

    if(!(tracer = tracee->parent))
    {
        return -ESRCH;
    }
    
    return ptrace_set_tracer(tracee, tracer);
}


/*
 * Attach to the process specified in pid, making it a tracee of the calling
 * process. The tracee is sent a SIGSTOP, but will not necessarily have 
 * stopped by the completion of this call; use waitpid to wait for the tracee 
 * to stop.
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_attach(pid_t pid)
{
    volatile struct task_t *tracee;
    volatile struct task_t *tracer = this_core->cur_task;
    long res;

    if(!(tracee = get_task_by_tid(pid)) ||
        (tracee->tracer_pid && tracee->tracer_pid != tracer->pid) ||
        (tracee->properties & (PROPERTY_TRACE_SYSCALLS | 
                                    PROPERTY_TRACE_SIGNALS)) ||
        (!suser(tracer) && tracer->uid != tracee->uid))
    {
        return -ESRCH;
    }

    if((res = ptrace_set_tracer(tracee, tracer)) == 0)
    {
        user_add_task_signal(tracee, SIGSTOP, 1);
    }
    
    return res;
}


/*
 * Common prologue to all the upcoming functions.
 */
#define GET_TRACER_AND_TRACEE(tracer, tracee)           \
    volatile struct task_t *tracee;                     \
    volatile struct task_t *tracer = this_core->cur_task;\
    if(!(tracee = get_task_by_tid(pid)) ||              \
        (tracee->tracer_pid != tracer->pid) ||          \
       !(tracee->properties & PROPERTY_TRACE_SUSPENDED))\
    {                                                   \
        return -ESRCH;                                  \
    }

#define VALIDATE_DATA_PTR(data)                         \
    if(!data)                                           \
    {                                                   \
        return -EFAULT;                                 \
    }


/*
 * Read a word at the address 'addr' in the tracee's memory, returning the 
 * word as the result of the ptrace() call.
 *
 * NOTE: The tracee must be stopped before this call.
 * NOTE: This syscall return the result in 'data', while the libc wrapper 
 *       function ignores 'data' and returns the result as the function's
 *       return value.
 *
 * Inputs:
 *    pid => tracee's TID
 *    addr => where to read data from (tracee's memory)
 *    data => where to store data into (tracer's memory)
 *
 * Outputs:
 *    data => the result is stored here
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_peek_data(pid_t pid, void *addr, void *data)
{
    virtual_addr memstart, memend;
    size_t sz = sizeof(void *);
    void *buf;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);
    
    KDEBUG("ptrace_peek_data: addr 0x%x, data 0x%x\n", addr, data);

    memstart = (virtual_addr)addr;
    memend = memstart + sz;

    if(read_other_taskmem((struct task_t *)tracee, 0, 
                            memstart, memend, (char *)&buf, sz) != sz)
    {
        //KDEBUG("ptrace_peek_data: res %d\n", res);
        return -EFAULT;
    }
    
    return copy_to_user(data, &buf, sz);
}


/*
 * Copy the word 'data' to the address 'addr' in the tracee's memory.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    addr => where to write data to (tracee's memory)
 *    data => data word to copy (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_poke_data(pid_t pid, void *addr, void *data)
{
    virtual_addr memstart, memend;
    size_t sz = sizeof(void *);
    void *buf;
    GET_TRACER_AND_TRACEE(tracer, tracee);
    
    if(copy_from_user(&buf, &data, sz) != 0)
    {
        return -EFAULT;
    }

    memstart = (virtual_addr)addr;
    memend = memstart + sz;

    if(write_other_taskmem((struct task_t *)tracee, 0, memstart, memend, 
                                                    (char *)&buf, sz) != sz)
    {
        return -EFAULT;
    }
    
    return 0;
}


/*
 * Read a word at offset 'addr' in the tracee's USER area, which holds
 * the registers and other information about the task (see <sys/user.h>),
 * returning the word as the result of the ptrace() call. The given
 * 'addr' does not have to be word-aligned.
 *
 * NOTE: We don't have an actual USER area, so we collect information
 *       from the task struct.
 * NOTE: The tracee must be stopped before this call.
 * NOTE: This syscall return the result in 'data', while the libc wrapper 
 *       function ignores 'data' and returns the result as the function's
 *       return value.
 *
 * Inputs:
 *    pid => tracee's TID
 *    addr => where to read data from (tracee's USER area)
 *    data => where to store data into (tracer's memory)
 *
 * Outputs:
 *    data => the result is stored here
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_peek_user(pid_t pid, void *__addr, void *data)
{
    size_t sz = sizeof(int);
    size_t addr = (size_t)__addr;
    char *p;
    struct user u;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);

    if(addr >= sizeof(struct user))
    {
        return -EFAULT;
    }

    A_memset(&u, 0, sizeof(struct user));

    /*
     * See: https://linux-kernel.vger.kernel.narkive.com/WpxQ1Ilt/ptrace-ptrace-peekuser-behavior
     */

    get_regs(tracee, &(u.regs));
    get_fpregs(tracee, &(u.i387));
    u.u_fpvalid = (tracee->properties & PROPERTY_USED_FPU) ? 1 : 0;
    
    u.u_tsize = memregion_text_pagecount(tracee);
    u.u_dsize = memregion_data_pagecount(tracee);
    u.u_ssize = memregion_stack_pagecount(tracee);
    u.start_code = task_get_code_start(tracee);
    u.start_stack = USER_MEM_END;
    u.signal = WTERMSIG(tracee->exit_status);
    
    p = (char *)&u + addr;

    return copy_to_user(data, p, sz);
}


/*
 * Copy the word 'data' to offset 'addr' in the tracee's USER area, which 
 * holds the registers and other information about the task (see
 * <sys/user.h>). The given 'addr' does not have to be word-aligned.
 *
 * NOTE: We don't have an actual USER area, so we collect information
 *       from the task struct.
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    addr => where to copy data to (tracee's USER area)
 *    data => data word to copy (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_poke_user(pid_t pid, void *__addr, void *data)
{
    /*
     * TODO: implement this.
     */
    
    UNUSED(pid);
    UNUSED(__addr);
    UNUSED(data);
    
    return -EFAULT;
}


/*
 * Copy the tracee's general-purpose registers to the address 'data' in the
 * tracer. See <sys/user.h> for information on the format of this data.
 *
 * NOTE: We don't have an actual USER area, so we collect information
 *       from the task struct.
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => where to copy data to (tracer's memory)
 *
 * Outputs:
 *    register values are copied to 'data'
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_get_regs(pid_t pid, void *data)
{
    struct user_regs_struct u;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);
    get_regs(tracee, &u);

    return copy_to_user(data, &u, sizeof(struct user_regs_struct));
}


/*
 * Copy the tracee's floating-point registers to the address 'data' in the
 * tracer. See <sys/user.h> for information on the format of this data.
 *
 * NOTE: We don't have an actual USER area, so we collect information
 *       from the task struct.
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => where to copy data to (tracer's memory)
 *
 * Outputs:
 *    register values are copied to 'data'
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_get_fpregs(pid_t pid, void *data)
{
    struct user_fpregs_struct u;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);
    get_fpregs(tracee, &u);

    return copy_to_user(data, &u, sizeof(struct user_fpregs_struct));
}


/*
 * Modify the tracee's general-purpose registers from the address 'data' in the
 * tracer. See <sys/user.h> for information on the format of this data.
 *
 * NOTE: We don't have an actual USER area, so we collect information
 *       from the task struct.
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => where to copy data from (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_set_regs(pid_t pid, void *data)
{
    struct user_regs_struct u;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);
    
    if(copy_from_user(&u, data, sizeof(struct user_regs_struct)) != 0)
    {
        return -EFAULT;
    }

    set_regs(tracee, &u);

    return 0;
}


/*
 * Modify the tracee's floating-point registers from the address 'data' in the
 * tracer. See <sys/user.h> for information on the format of this data.
 *
 * NOTE: We don't have an actual USER area, so we collect information
 *       from the task struct.
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => where to copy data from (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_set_fpregs(pid_t pid, void *data)
{
    struct user_fpregs_struct u;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);

    if(copy_from_user(&u, data, sizeof(struct user_fpregs_struct)) != 0)
    {
        return -EFAULT;
    }

    set_fpregs(tracee, &u);

    return 0;
}


/*
 * Retrieve information about the signal that caused the stop. Copy a
 * siginfo_t structure from the tracee to the address 'data' in the tracer.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => where to copy data to (tracer's memory)
 *
 * Outputs:
 *    a struct siginfo_t is copied to 'data'
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_get_siginfo(pid_t pid, void *data)
{
    int signum;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);

    if(WIFSTOPPED(tracee->exit_status))
    {
        /*
         * TODO: This is not the right way of getting signfo.
         *       See: "Stopped states" section in ptrace manpage.
         */

        signum = WSTOPSIG(tracee->exit_status);

        if(signum > 0 && signum < NSIG)
        {
            return copy_to_user(data, (void *)&tracee->siginfo[signum],
                                                        sizeof(siginfo_t));
        }
    }
    
    return -EPERM;
}


/*
 * Set signal information: copy a siginfo_t structure from the address 'data'
 * in the tracer to the tracee.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => where to copy data from (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_set_siginfo(pid_t pid, void *data)
{
    int signum;
    siginfo_t siginfo;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);
    
    if(copy_from_user(&siginfo, data, sizeof(siginfo_t)) != 0)
    {
        return -EFAULT;
    }
    
    signum = siginfo.si_signo;
    
    if(signum > 0 && signum < NSIG)
    {
        A_memcpy((void *)&tracee->siginfo[signum], &siginfo, sizeof(siginfo_t));
        return 0;
    }

    return -EIO;
}


/*
 * Place a copy of the mask of blocked signals in the buffer pointed to by 
 * 'data'.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    addr => size of 'data' buffer, should be sizeof(sigset_t)
 *    data => where to copy data to (tracer's memory), should be a pointer 
 *            to a buffer of type sigset_t
 *
 * Outputs:
 *    a struct of type sigset_t is copied to 'data'
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_get_sigmask(pid_t pid, void *addr, void *data)
{
    sigset_t *sigset = (sigset_t *)data;
    size_t sz = (size_t)addr;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);

    if(sz < sizeof(sigset_t))
    {
        return -EFAULT;
    }
    
    return syscall_sigprocmask_internal((struct task_t *)tracee, 0, NULL, sigset, 0);
}


/*
 * Change the mask of blocked signals to the value specified in the buffer
 * pointed to by 'data'.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    addr => size of 'data' buffer, should be sizeof(sigset_t)
 *    data => where to copy data from (tracer's memory), should be a pointer 
 *            to a buffer of type sigset_t
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_set_sigmask(pid_t pid, void *addr, void *data)
{
    sigset_t *sigset = (sigset_t *)data;
    size_t sz = (size_t)addr;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);

    if(sz < sizeof(sigset_t))
    {
        return -EFAULT;
    }

    return syscall_sigprocmask_internal((struct task_t *)tracee, SIG_SETMASK, sigset, NULL, 0);
}


/*
 * Set ptrace options from data, which is interpreted as a bit mask of options
 * (see sys/ptrace.h and bits/ptrace-shared.h)
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => bitmask of options (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_set_options(pid_t pid, void *data)
{
    int ptrace_options;

    if(copy_from_user(&ptrace_options, &data, sizeof(int)) != 0)
    {
        return -EFAULT;
    }
    
    GET_TRACER_AND_TRACEE(tracer, tracee);
    tracee->ptrace_options = ptrace_options;
    
    return 0;
}


/*
 * Retrieve a message (as an unsigned long) about the ptrace event that just
 * happened, placing it at the address 'data' in the tracer.
 * For PTRACE_EVENT_EXIT, this is the tracee's exit status.
 * For PTRACE_EVENT_FORK, PTRACE_EVENT_VFORK, PTRACE_EVENT_VFORK_DONE, and
 * PTRACE_EVENT_CLONE, this is the PID of the new process.
 * For PTRACE_EVENT_SECCOMP, this is the seccomp filter's SECCOMP_RET_DATA
 * associated with the triggered rule.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => where to copy data to (tracer's memory)
 *
 * Outputs:
 *    message is copied to 'data'
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_get_eventmsg(pid_t pid, void *data)
{
    unsigned long msg;

    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(data);
    msg = tracee->ptrace_eventmsg;

    return copy_to_user(data, &msg, sizeof(unsigned long));
}


/*
 * Restart the stopped tracee process. If 'data' is nonzero, it is interpreted
 * as the number of a signal to be delivered to the tracee; otherwise, no
 * signal is delivered.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => signal number to deliver (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_continue(pid_t pid, void *data)
{
    int signum = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);

    if(data && copy_from_user(&signum, data, sizeof(int)) != 0)
    {
        return -EFAULT;
    }

    //tracee->properties &= ~PROPERTY_TRACE_SYSCALLS;

    return signal_and_continue(tracee, signum);
}


/*
 * Restart the stopped tracee as for PTRACE_CONT, but arrange for the tracee
 * to be stopped at the next entry to or exit from a system call (The tracee
 * will also, as usual, be stopped upon receipt of a signal.)
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => signal number to deliver (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
long ptrace_syscall(pid_t pid, void *data)
{
    int signum = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);

    if(data && copy_from_user(&signum, data, sizeof(int)) != 0)
    {
        return -EFAULT;
    }

    __sync_or_and_fetch(&tracee->properties, PROPERTY_TRACE_SYSCALLS);
    __sync_and_and_fetch(&tracee->properties, ~PROPERTY_TRACE_SYSEMU);

    return signal_and_continue(tracee, signum);
}


/*
 * Continue and stop on entry to the next system call, which will not be
 * executed. The 'data' argument is treated as for PTRACE_CONT.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => signal number to deliver (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_sysemu(pid_t pid, void *data)
{
    int signum = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);

    if(data && copy_from_user(&signum, data, sizeof(int)) != 0)
    {
        return -EFAULT;
    }

    __sync_or_and_fetch(&tracee->properties, PROPERTY_TRACE_SYSEMU);
    __sync_and_and_fetch(&tracee->properties, ~PROPERTY_TRACE_SYSCALLS);

    return signal_and_continue(tracee, signum);
}


/*
 * Restart the stopped tracee as for PTRACE_CONT, but arrange for the tracee
 * to be stopped after execution of a single instruction (The tracee will
 * also, as usual, be stopped upon receipt of a signal.)
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => signal number to deliver (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_singlestep(pid_t pid, void *data)
{
    int signum = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);

    if(data && copy_from_user(&signum, data, sizeof(int)) != 0)
    {
        return -EFAULT;
    }

    volatile struct regs *rdest = &tracee->saved_context;

#ifdef __x86_64__
    rdest->rflags |= 0x100;
#else
    //tracee->properties |= PROPERTY_TRACE_SINGLESTEP;
    rdest->eflags |= 0x100;
#endif

    return signal_and_continue(tracee, signum);
}


/*
 * Continue and stop on entry to the next system call, which will not be
 * executed. This is in addition to singlestepping if not in a syscall.
 * The 'data' argument is treated as for PTRACE_CONT.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => signal number to deliver (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_sysemu_singlestep(pid_t pid, void *data)
{
    int signum = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);

    if(data && copy_from_user(&signum, data, sizeof(int)) != 0)
    {
        return -EFAULT;
    }

    volatile struct regs *rdest = &tracee->saved_context;

    __sync_or_and_fetch(&tracee->properties, PROPERTY_TRACE_SYSEMU);
    __sync_and_and_fetch(&tracee->properties, ~PROPERTY_TRACE_SYSCALLS);

#ifdef __x86_64__
    rdest->rflags |= 0x100;
#else
    rdest->eflags |= 0x100;
#endif

    return signal_and_continue(tracee, signum);
}


/*
 * When in syscall-enter-stop, change the number of the system call that is
 * about to be executed to the number specified in the data argument.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => syscall number to execute (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_set_syscall(pid_t pid, void *data)
{
    int sysnum = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);

    if(copy_from_user(&sysnum, data, sizeof(int)) != 0)
    {
        return -EFAULT;
    }

    /*
    if(sysnum <= 0 || sysnum >= NR_SYSCALLS)
    {
        return -EIO;
    }
    */

    volatile struct regs *rdest = &tracee->saved_context;

    if(tracee->user_in_kernel_mode &&
       (tracee->properties & PROPERTY_IN_SYSCALL))
    {
        if(tracee->exit_status ==
                (__W_STOPCODE(SIGTRAP) | (PTRACE_EVENT_SYSCALL_ENTER << 16)))
        {
            SET_SYSCALL_NUMBER(rdest, sysnum);
        }
        else if(tracee->exit_status ==
                (__W_STOPCODE(SIGTRAP) | (PTRACE_EVENT_SYSCALL_EXIT << 16)))
        {
            SET_SYSCALL_RESULT(rdest, sysnum);
        }
    }
    
    return 0;
}


/*
 * Retrieve information about the system call that caused the stop. The 
 * information is placed into the buffer pointed by the 'data' argument.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    addr => size of 'data' buf, should be sizeof(struct ptrace_syscall_info)
 *    data => where to copy data to (tracer's memory), should be a pointer 
 *            to a buffer of type struct ptrace_syscall_info
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    The return value contains the number of bytes available to be written by
 *    the kernel. If the size of the data to be written by the kernel exceeds 
 *    the size specified by the 'addr' argument, the output data is truncated.
 *    -errno is returned on failure.
 */
static long ptrace_get_syscall_info(pid_t pid, void *addr, void *data)
{
    struct ptrace_syscall_info info;
    size_t sz;
    
    GET_TRACER_AND_TRACEE(tracer, tracee);
    VALIDATE_DATA_PTR(addr);
    VALIDATE_DATA_PTR(data);
    
    if(copy_from_user(&sz, addr, sizeof(size_t)) != 0)
    {
        return -EFAULT;
    }

    volatile struct regs *rdest = &tracee->saved_context;

    if(!tracee->user_in_kernel_mode ||
       !(tracee->properties & PROPERTY_IN_SYSCALL))
    {
        return -ESRCH;
    }
    
    if(sz > sizeof(struct ptrace_syscall_info))
    {
        sz = sizeof(struct ptrace_syscall_info);
    }
    
    A_memset(&info, 0, sizeof(struct ptrace_syscall_info));
    
    // TODO: use appropriate values when we implement seccomp and audit.h
    info.arch = 0;

#ifdef __x86_64__
    info.instruction_pointer = rdest->rip;
    info.stack_pointer = rdest->rsp;
#else
    info.instruction_pointer = rdest->eip;
    info.stack_pointer = rdest->esp;
#endif

    
#define STATUS(s)       (__W_STOPCODE(SIGTRAP) | (s << 16))

    if(tracee->exit_status == STATUS(PTRACE_EVENT_SYSCALL_ENTER))
    {
        info.op = PTRACE_SYSCALL_INFO_ENTRY;
        info.entry.nr = GET_SYSCALL_NUMBER(rdest);
        info.entry.args[0] = GET_SYSCALL_ARG1(rdest);
        info.entry.args[1] = GET_SYSCALL_ARG2(rdest);
        info.entry.args[2] = GET_SYSCALL_ARG3(rdest);
        info.entry.args[3] = GET_SYSCALL_ARG4(rdest);
        info.entry.args[4] = GET_SYSCALL_ARG5(rdest);
        info.entry.args[5] = 0;
    }
    else if(tracee->exit_status == STATUS(PTRACE_EVENT_SYSCALL_EXIT))
    {
        info.op = PTRACE_SYSCALL_INFO_EXIT;
        info.exit.rval = GET_SYSCALL_RESULT(rdest);
        info.exit.is_error = ((int)GET_SYSCALL_RESULT(rdest) < 0) ? 1 : 0;
    }
    else
    {
        info.op = PTRACE_SYSCALL_INFO_NONE;
    }

#undef STATUS
    
    if(copy_to_user(data, &info, sz) != 0)
    {
        return -EFAULT;
    }
    
    return sz;
}


/*
 * Send the tracee a SIGKILL to terminate it.
 *
 * Inputs:
 *    pid => tracee's TID
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_kill(pid_t pid)
{
    GET_TRACER_AND_TRACEE(tracer, tracee);

    user_add_task_signal(tracee, SIGKILL, 1);

    return 0;
}


/*
 * Restart the stopped tracee as for PTRACE_CONT, but first detach from it.
 *
 * NOTE: The tracee must be stopped before this call.
 *
 * Inputs:
 *    pid => tracee's TID
 *    data => signal number to deliver (tracer's memory)
 *
 * Outputs:
 *    none
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_detach(pid_t pid, void *data)
{
    int signum = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);

    if(data && copy_from_user(&signum, data, sizeof(int)) != 0)
    {
        return -EFAULT;
    }
    
    ptrace_clear_state(tracee);
    list_remove(tracer->tracees, (void *)tracee);

    return signal_and_continue(tracee, signum);
}


/*
 * This operation performs a similar task to get_thread_area(). It reads the
 * TLS entry in the GDT whose index is given in 'addr', placing a copy of the
 * entry into the struct user_desc pointed to by 'data'. (By contrast with
 * get_thread_area(), the entry_number of the struct user_desc is ignored.)
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_get_thread_area(pid_t pid, void *addr, void *data)
{
    struct user_desc tmp;
    struct user_desc *dp = (struct user_desc *)data;
    unsigned int n = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);
    
    if(!data)
    {
        return -EFAULT;
    }
    
    // check the validity of the user's address before we modify some of
    // its contents and then pass it to syscall_get_thread_area().
    if(copy_from_user(&tmp, data, sizeof(struct user_desc)) != 0)
    {
        return -EFAULT;
    }

    if(copy_from_user(&n, addr, sizeof(unsigned int)) != 0)
    {
        return -EFAULT;
    }
    
    // we don't need tmp as we only used it for address checking
    //dp->entry_number = (unsigned int)addr;
    dp->entry_number = n;
    
    return syscall_get_thread_area(dp);
}


/*
 * This operation performs a similar task to set_thread_area(). It sets the
 * TLS entry in the GDT whose index is given in 'addr', assigning it the data
 * supplied in the struct user_desc pointed to by 'data'. (By contrast with
 * set_thread_area(), the entry_number of the struct user_desc is ignored;
 * in other words, this ptrace operation can't be used to allocate a free
 * TLS entry.)
 *
 * Returns:
 *    0 on success, -errno on failure.
 */
static long ptrace_set_thread_area(pid_t pid, void *addr, void *data)
{
    struct user_desc tmp;
    struct user_desc *dp = (struct user_desc *)data;
    unsigned int n = 0;
    GET_TRACER_AND_TRACEE(tracer, tracee);
    
    if(!data)
    {
        return -EFAULT;
    }
    
    // check the validity of the user's address before we modify some of
    // its contents and then pass it to syscall_get_thread_area().
    if(copy_from_user(&tmp, data, sizeof(struct user_desc)) != 0)
    {
        return -EFAULT;
    }

    if(copy_from_user(&n, addr, sizeof(unsigned int)) != 0)
    {
        return -EFAULT;
    }
    
    // we don't need tmp as we only used it for address checking
    //dp->entry_number = (unsigned int)addr;
    dp->entry_number = n;
    
    return syscall_set_thread_area(dp);
}


/*
 * Handler for syscall ptrace().
 *
 * See: https://man7.org/linux/man-pages/man2/ptrace.2.html
 */
long syscall_ptrace(int request, pid_t pid, void *addr, void *data)
{
    KDEBUG("syscall_ptrace: req %d, pid %d, addr 0x%x, data 0x%x\n", request, pid, addr, data);

    switch(request)
    {
        case PTRACE_TRACEME:
            return ptrace_traceme();
        
        case PTRACE_PEEKTEXT:
        case PTRACE_PEEKDATA:
            return ptrace_peek_data(pid, addr, data);

        case PTRACE_POKETEXT:
        case PTRACE_POKEDATA:
            return ptrace_poke_data(pid, addr, data);

        case PTRACE_PEEKUSER:
            return ptrace_peek_user(pid, addr, data);
            
        case PTRACE_POKEUSER:
            return ptrace_poke_user(pid, addr, data);

        case PTRACE_GETREGS:
            return ptrace_get_regs(pid, data);
            
        case PTRACE_GETFPREGS:
            return ptrace_get_fpregs(pid, data);
            
        case PTRACE_SETREGS:
            return ptrace_set_regs(pid, data);
            
        case PTRACE_SETFPREGS:
            return ptrace_set_fpregs(pid, data);

        case PTRACE_GETSIGINFO:
            return ptrace_get_siginfo(pid, data);

        case PTRACE_SETSIGINFO:
            return ptrace_set_siginfo(pid, data);

        case PTRACE_GETSIGMASK:
            return ptrace_get_sigmask(pid, addr, data);

        case PTRACE_SETSIGMASK:
            return ptrace_set_sigmask(pid, addr, data);

        case PTRACE_SETOPTIONS:
            return ptrace_set_options(pid, data);

        case PTRACE_GETEVENTMSG:
            return ptrace_get_eventmsg(pid, data);

        case PTRACE_CONT:
            return ptrace_continue(pid, data);

        case PTRACE_SYSCALL:
            return ptrace_syscall(pid, data);

        case PTRACE_SYSEMU:
            return ptrace_sysemu(pid, data);

        case PTRACE_SINGLESTEP:
            return ptrace_singlestep(pid, data);

        case PTRACE_SYSEMU_SINGLESTEP:
            return ptrace_sysemu_singlestep(pid, data);

        case PTRACE_GET_SYSCALL_INFO:
            return ptrace_get_syscall_info(pid, addr, data);

        case PTRACE_SET_SYSCALL:
            return ptrace_set_syscall(pid, data);

        case PTRACE_KILL:
            return ptrace_kill(pid);

        case PTRACE_ATTACH:
            return ptrace_attach(pid);

        case PTRACE_DETACH:
            return ptrace_detach(pid, data);

        case PTRACE_GET_THREAD_AREA:
            return ptrace_get_thread_area(pid, addr, data);

        case PTRACE_SET_THREAD_AREA:
            return ptrace_set_thread_area(pid, addr, data);

            
        case PTRACE_GETREGSET:
        case PTRACE_SETREGSET:
        case PTRACE_PEEKSIGINFO:
        case PTRACE_SECCOMP_GET_FILTER:
        
        case PTRACE_LISTEN:
        case PTRACE_INTERRUPT:
        case PTRACE_SEIZE:

        default:
            return -EIO;
    }
}

