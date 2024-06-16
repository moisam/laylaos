/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: signal.c
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
 *  \file signal.c
 *
 *  The kernel signal handling implementation.
 */

//#define __DEBUG

#define __USE_XOPEN_EXTENDED
#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <kernel/ksignal.h>
#include <kernel/pic.h>
#include <kernel/user.h>
#include <kernel/ptrace.h>
#include <kernel/syscall.h>
#include <kernel/ksigset.h>
#include <kernel/timer.h>
#include <kernel/fpu.h>
#include <kernel/asm.h>
#include <signal.h>

#include "signal_funcs.c"


sigset_t unblockable_signals;


/*
 * Sigset to unsigned long.
 */
unsigned long sigset_to_ulong(sigset_t *set)
{
    unsigned long res = 0;
    size_t i;
    
    for(i = 1; i <= NSIG; i++)
    {
        if(ksigismember(set, i))
        {
            res |= (1 << (i - 1));
        }
    }

    return res;
}


/*
 * Get ignored task signals.
 */
unsigned long get_ignored_task_signals(struct task_t *task)
{
    unsigned long res = 0;
    size_t i;
    
    if(!task || !task->sig)
    {
        return 0;
    }

    for(i = 0; i < NSIG; i++)
    {
        struct sigaction *action = &task->sig->signal_actions[i];
  
        if(action && action->sa_handler == SIG_IGN)
        {
            res |= (1 << i);
        }
    }

    return res;
}


/*
 * Initialise signals.
 */
void init_signals(void)
{
    ksigemptyset(&unblockable_signals);
    ksigaddset(&unblockable_signals, SIGKILL);
    ksigaddset(&unblockable_signals, SIGSTOP);
}


static void restart_syscall(struct task_t *ct, struct regs *r)
{
    //printk("restart_syscall: syscall %d, n %d (%d)\n", ct->interrupted_syscall, GET_SYSCALL_NUMBER(r), -ERESTARTSYS);

    if(ct->interrupted_syscall &&
       GET_SYSCALL_NUMBER(r) == (uintptr_t)-ERESTARTSYS)
    {
        SET_SYSCALL_NUMBER(r, ct->interrupted_syscall);
        ct->interrupted_syscall = 0;
        syscall_dispatcher(r);
    }
    else
    {
        ct->interrupted_syscall = 0;
    }
}


static int handle_signal(struct task_t *ct, struct regs *r, int signum)
{
    struct sigaction *action = &ct->sig->signal_actions[signum];

    if(ct->state == TASK_ZOMBIE)
    {
        return 1;
    }
    
    ct->properties |= PROPERTY_HANDLING_SIG;
    ct->regs = r;

    //printk("handle_signal: pid %d, signum %d\n", ct->pid, signum);

    /*
     * Notify the tracer (if any). The ptrace manpage says:
     *    SIGKILL does not generate signal-delivery-stop and therefore the
     *    tracer can't suppress it.  SIGKILL kills even within system calls
     *    (syscall-exit-stop is not generated prior to death by SIGKILL).
     *    The net effect is that SIGKILL always kills the process (all its
     *    threads), even if some threads of the process are ptraced.
     */
    if((ct->properties & PROPERTY_TRACE_SIGNALS) && (signum != SIGKILL))
    {
        signum = ptrace_signal(signum, 0);
    }


    if((action->sa_handler != SIG_DFL) && !(action->sa_flags & SA_RESTART))
    {
        //ct->interrupted_syscall = 0;
        
        if(GET_SYSCALL_NUMBER(r) == (uintptr_t)-ERESTARTSYS)
        {
            SET_SYSCALL_RESULT(r, (uintptr_t)-EINTR);
        }
    }

    
    if(signum <= 0 || signum >= NSIG)
    {
        goto ignore;
    }

    if(action->sa_handler == SIG_IGN)
    {
        goto ignore;
    }

    ksigaddset(&ct->signal_caught, signum);

    if(action->sa_handler == SIG_DFL)
    {
        switch(signum)
        {
            case SIGSTOP:
            case SIGTSTP:
            case SIGTTIN:
            case SIGTTOU:
                /*
                 * store the STOP status so that parent will find it if it
                 * wants (by calling waitpid()).
                 */
                ct->exit_status = __W_STOPCODE(signum);

                /* notify parent */
                add_task_child_signal(ct, CLD_STOPPED, signum);

                //KDEBUG("Task %d (%d) blocking\n", ct->pid, ct->pgid);

                /* block me */
                block_task(ct, 1);

                //KDEBUG("Task %d (%d) waking up\n", ct->pid, ct->pgid);

                ct->properties &= ~PROPERTY_HANDLING_SIG;
                ct->regs = NULL;

                return 0;

            case SIGCONT:
                ct->exit_status = __W_CONTINUED;

                /* notify parent */
                add_task_child_signal(ct, CLD_CONTINUED, signum);

                /* don't do anything, we are already awake! */
                goto ignore;

            case SIGCHLD:
            case SIGURG:
            case SIGPWR:
            case SIGWINCH:
                goto ignore;

            /* make a core dump for the signals that require the feature */
            case SIGQUIT:
            case SIGILL:
            case SIGTRAP:
            case SIGABRT:
            case SIGBUS:
            case SIGFPE:
            case SIGSEGV:
            case SIGXCPU:
            case SIGXFSZ:
            case SIGSYS:
                dump_core();
                /* fall through */

            default:
                /* default action is to terminate the process */
                //terminate_thread_group(__W_EXITCODE(0, signum));
                terminate_task(__W_EXITCODE(0, signum));
                __builtin_unreachable();
        }
    }

    //ct->regs = NULL;

    /* we need to call user space signal handler */
    void (*handler)() = action->sa_sigaction;
    volatile uintptr_t stack;
    ucontext_t *context;
    
    if((action->sa_flags & SA_ONSTACK) && ct->signal_stack.ss_sp)
    {
        // execute handler on alternate signal stack provided by signalstack()
        stack = (uintptr_t)ct->signal_stack.ss_sp;
        ct->signal_stack.ss_flags |= SS_ONSTACK;
    }
    else
    {
        // execute on normal stack
#ifdef __x86_64__
        stack = (uintptr_t)r->userrsp;
#else
        stack = (uintptr_t)r->useresp;
#endif
    }

     /*
      * the stack should look like (for x86):
      *
      *	0x00(%esp) - %eax
      *	0x04(%esp) - %ebx
      *	0x08(%esp) - %ecx
      *	0x0C(%esp) - %edx
      *	0x10(%esp) - %edi
      *	0x14(%esp) - %esi
      *	0x18(%esp) - %ebp
      *	0x1C(%esp) - %fs
      *	0x20(%esp) - %es
      *	0x24(%esp) - %ds
      *	0x28(%esp) - %eip
      *	0x2C(%esp) - %cs
      *	0x30(%esp) - %eflags
      *	0x34(%esp) - %useresp
      *	0x38(%esp) - %ss
      */

#define PUSH(v)             stack -= sizeof(uintptr_t);         \
                            *(volatile uintptr_t *)stack = (uintptr_t)(v);


#ifdef __x86_64__
    fpu_state_save(ct);

    stack -= sizeof(uintptr_t) * 64;
    A_memcpy((void *)stack, ct->fpregs, sizeof(uintptr_t) * 64);
    /*
    for(int i = 0; i < 64; i++)
    {
        PUSH(ct->fpregs[i]);
    }
    */
#endif


    PUSH(ct->interrupted_syscall);

    stack -= sizeof(ucontext_t);
    context = (ucontext_t *)stack;
    context->uc_mcontext.gregs[REG_R8] = r->r8;
    context->uc_mcontext.gregs[REG_R9] = r->r9;
    context->uc_mcontext.gregs[REG_R10] = r->r10;
    context->uc_mcontext.gregs[REG_R11] = r->r11;
    context->uc_mcontext.gregs[REG_R12] = r->r12;
    context->uc_mcontext.gregs[REG_R13] = r->r13;
    context->uc_mcontext.gregs[REG_R14] = r->r14;
    context->uc_mcontext.gregs[REG_R15] = r->r15;
    context->uc_mcontext.gregs[REG_RSP] = r->userrsp;
    context->uc_mcontext.gregs[REG_RBP] = r->rbp;
    context->uc_mcontext.gregs[REG_RDI] = r->rdi;
    context->uc_mcontext.gregs[REG_RSI] = r->rsi;
    context->uc_mcontext.gregs[REG_RDX] = r->rdx;
    context->uc_mcontext.gregs[REG_RCX] = r->rcx;
    context->uc_mcontext.gregs[REG_RBX] = r->rbx;
    context->uc_mcontext.gregs[REG_RAX] = r->rax;
    context->uc_mcontext.gregs[REG_RIP] = r->rip;
    context->uc_mcontext.fpregs = (void *)(stack + sizeof(ucontext_t));
#ifdef __x86_64__
    context->uc_mcontext.gregs[REG_EFL] = r->rflags;
#else
    context->uc_mcontext.gregs[REG_EFL] = r->eflags;
#endif

#if 0
    stack -= sizeof(struct regs);
    A_memcpy((void *)stack, r, sizeof(struct regs));
#endif



#ifdef SA_COOKIE
    if(action->sa_flags & SA_COOKIE)
    {
        /*
         * TODO: implement this!
         */
        
        terminate_task(__W_EXITCODE(0, signum));
        __builtin_unreachable();
    }
#endif
    
    /* we should have the address of sa_restorer */
    if(!(action->sa_flags & SA_RESTORER))
    {
        terminate_task(__W_EXITCODE(0, signum));
        __builtin_unreachable();
    }

    /* We should push values on the stack for calling the signal handler 
     * defined as:
     *       void func(int signo, siginfo_t *info, void *context);
     *
     * That is, we always act as if SA_SIGINFO was set for the signal.
     *
     * Values for info.si_code:
     * SI_USER: The signal was sent by the kill() function. The 
     *          implementation may set si_code to SI_USER if the signal was
     *          sent by the raise() or abort() functions or any similar 
     *          functions provided as implementation extensions.
     * SI_QUEUE: The signal was sent by the sigqueue() function.
     * SI_TIMER: The signal was generated by the expiration of a timer 
     *           set by timer_settime().
     * SI_ASYNCIO: The signal was generated by the completion of an 
     *             asynchronous I/O request.
     * SI_MESGQ: [MSG] [Option Start] The signal was generated by the 
     *           arrival of a message on an empty message queue.
     * 
     * If si_code is SI_USER or SI_QUEUE, or is <= 0, signal was generated 
     * by a process, and thus si_pid and si_uid should be set to PID and
     * real UID of the sender.
     */
    volatile uintptr_t info = stack - sizeof(siginfo_t);

    stack = info;


    // ensure the stack is 16-byte aligned, in case the user signal handler
    // decided to use SSE instructions
    /*
    __asm__ __volatile__("xchg %%bx, %%bx"::);
    if((stack - (4 * sizeof(uintptr_t))) & 127)
    {
        stack = (stack & ~127) + (4 * sizeof(uintptr_t));
    }
    */


    PUSH(context);       // void *context
    //PUSH(0);       // void *context
    PUSH(info);    // siginfo_t *info

    PUSH(signum);                // int signo
    PUSH(action->sa_restorer);

#undef PUSH
  
    /* modify the signal mask according to user's request */
    save_sigmask();
    ksigorset(&ct->signal_mask, &ct->signal_mask, &action->sa_mask);

    /*
     * Do not prevent the signal from being received from within its own
     * signal handler.
     */
    if(action->sa_flags & SA_NODEFER)
    {
        ksigdelset(&ct->signal_mask, signum);   // allow current signal
    }
    else
    {
        ksigaddset(&ct->signal_mask, signum);   // block current signal
    }

    if(action->sa_flags & SA_RESETHAND)
    {
        if(signum != SIGILL && signum != SIGTRAP)
        {
            action->sa_handler = SIG_DFL;
            action->sa_flags  &= ~SA_SIGINFO;
        }
    }

    // If this is an alarm signal (from a POSIX timer), restart the timer
    if(ksigismember(&ct->signal_timer, signum))
    {
        ktimer_t timerid;
        struct posix_timer_t *timer;
        void *ptr;

        timerid = ct->siginfo[signum].si_value.sival_int;

        if(!(timer = get_posix_timer(tgid(ct), timerid)))
        {
            ksigdelset(&ct->signal_timer, signum);
            goto ignore;
        }

        ptr = timer->sigev.sigev_value.sival_ptr;
        ct->siginfo[signum].si_value.sival_ptr = ptr;
        timer->saved_overruns = timer->cur_overruns;
        timer->cur_overruns = 0;

        if(copy_to_user(ptr, &timerid, sizeof(ktimer_t)) != 0)
        {
            ksigdelset(&ct->signal_timer, signum);
            add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, ptr);
            goto ignore;
        }

        ksigdelset(&ct->signal_timer, signum);
    }

    A_memcpy((void *)info, &ct->siginfo[signum], sizeof(siginfo_t));

    cli();
    ct->regs = NULL;

    // NOTE: This shouldn't return, instead it will syscall __NR_sigreturn.
    //       The PROPERTY_HANDLING_SIG flag is cleared by syscall_sigreturn().
    do_user_sighandler((uintptr_t)stack, handler);

    return 1;


ignore:
    ct->properties &= ~PROPERTY_HANDLING_SIG;
    restart_syscall(ct, r);
    return 0;
}


/*
 * Check and dispatch pending signals.
 */
void check_pending_signals(struct regs *r)
{
    int signum;
    struct task_t *ct = cur_task;
    sigset_t permitted_signals;
    sigset_t deliverable_signals;

    while(!ksigisemptyset((sigset_t *)&ct->signal_pending))
    {
        /* determine which signals are not blocked */
        ksigfillset(&permitted_signals);
        ksignotset(&permitted_signals, &ct->signal_mask);
        ksigorset(&permitted_signals, &permitted_signals,
                                      &unblockable_signals);

        /* determine which signals can be delivered */
        ksigandset(&deliverable_signals, &permitted_signals, 
                                         (sigset_t *)&ct->signal_pending);

        for(signum = 1; signum < NSIG; signum++)
        {
            if(ksigismember(&deliverable_signals, signum))
            {
                break;
            }
        }
        
        if(signum == NSIG)
        {
            break;
        }

        ksigdelset((sigset_t *)&ct->signal_pending, signum);

        if(handle_signal(ct, r, signum))
        {
            return;
        }
    }
}


/*
 * Handler for syscall sigreturn().
 */
int syscall_sigreturn(uintptr_t __user_stack)
{
    /*
     * To get the saved registers, we need to pop the stuff we pushed on the
     * stack in signal_dispatcher(), which includes:
     *
     *     siginfo_t
     *     (unsigned int)0       // void *context
     *     (unsigned int)info    // pointer to the above siginfo_t
     *     signum                // int signo
     *
     * We assume the size of siginfo_t is 32 bytes (8 struct members times 4
     * bytes for int type). If the struct definition changes, e.g. if we use
     * another C library or port to another platform like x86-64, we need to
     * change this number in the code below!
     */

    struct task_t *ct = cur_task;
    //struct regs *rsaved;
    unsigned int interrupted_syscall;
    volatile uintptr_t user_stack = __user_stack;
    ucontext_t *context;

    restore_sigmask();

    user_stack += sizeof(uintptr_t) * 3;
    /*
    user_stack += sizeof(uintptr_t);        // int signo
    user_stack = (*(uintptr_t *)user_stack);
    */

    user_stack += sizeof(siginfo_t);
    
    /*
     * NOTE: The user task might have changed the stack and modified, in
     *       particular, CS, SS and EFLAGS (e.g. disabling interrupts).
     *       We ensure the stack contains valid values here.
     */

    context = (ucontext_t *)user_stack;
    ct->regs->r8 = context->uc_mcontext.gregs[REG_R8];
    ct->regs->r9 = context->uc_mcontext.gregs[REG_R9];
    ct->regs->r10 = context->uc_mcontext.gregs[REG_R10];
    ct->regs->r11 = context->uc_mcontext.gregs[REG_R11];
    ct->regs->r12 = context->uc_mcontext.gregs[REG_R12];
    ct->regs->r13 = context->uc_mcontext.gregs[REG_R13];
    ct->regs->r14 = context->uc_mcontext.gregs[REG_R14];
    ct->regs->r15 = context->uc_mcontext.gregs[REG_R15];
    ct->regs->userrsp = context->uc_mcontext.gregs[REG_RSP];
    ct->regs->rbp = context->uc_mcontext.gregs[REG_RBP];
    ct->regs->rdi = context->uc_mcontext.gregs[REG_RDI];
    ct->regs->rsi = context->uc_mcontext.gregs[REG_RSI];
    ct->regs->rdx = context->uc_mcontext.gregs[REG_RDX];
    ct->regs->rcx = context->uc_mcontext.gregs[REG_RCX];
    ct->regs->rbx = context->uc_mcontext.gregs[REG_RBX];
    ct->regs->rax = context->uc_mcontext.gregs[REG_RAX];
    ct->regs->rip = context->uc_mcontext.gregs[REG_RIP];

#ifdef __x86_64__
    ct->regs->rflags = context->uc_mcontext.gregs[REG_EFL];
#else
    ct->regs->eflags = context->uc_mcontext.gregs[REG_EFL];
#endif

#if 0
    // get the saved regs
    rsaved = (struct regs *)user_stack;
    COPY_FROM_USER(ct->regs, rsaved, sizeof(struct regs));
#endif

    // user mode data selector is 0x20 + RPL 3
    ct->regs->ss = 0x23;

    // CS user mode code selector is 0x18 + RPL 3
    ct->regs->cs = 0x1B;

#ifdef __x86_64__
    // enable interrupts
    ct->regs->rflags |= 0x200;
#else
    ct->regs->ds = 0x23;
    ct->regs->es = 0x23;
    ct->regs->fs = 0x33;
    ct->regs->gs = 0x33;

    // enable interrupts
    ct->regs->eflags |= 0x200;
#endif


    user_stack += sizeof(ucontext_t);
#if 0
    user_stack += sizeof(struct regs);
#endif
    ct->interrupted_syscall = *(volatile uintptr_t *)user_stack;
    interrupted_syscall = ct->interrupted_syscall;

    
#ifdef __x86_64__
    user_stack += sizeof(uintptr_t);
    A_memcpy(ct->fpregs, (void *)user_stack, sizeof(uintptr_t) * 64);
    /*
    for(int i = 0; i < 64; i++)
    {
        user_stack += sizeof(uintptr_t);
        ct->fpregs[63 - i] = *(volatile uintptr_t *)user_stack;
    }
    */
    
    fpu_state_restore(ct);
#endif

    ct->properties &= ~PROPERTY_HANDLING_SIG;
    ct->signal_stack.ss_flags &= ~SS_ONSTACK;
    restart_syscall(ct, ct->regs);

    //printk("syscall_sigreturn: intsys %d, res %d\n", interrupted_syscall, interrupted_syscall ? GET_SYSCALL_NUMBER(ct->regs) : 0);
    return interrupted_syscall ? GET_SYSCALL_NUMBER(ct->regs) : 0;
    //return 0;
}


// defined in arch/XXX/irq.c
extern volatile int nested_irqs;

void check_signals_after_irq(struct regs *r)
{
    struct task_t *ct = cur_task;
    struct task_t *idle = idle_task;
    
    // don't process signals if we are serving an IRQ that occurred while we 
    // were serving another IRQ (i.e. nested IRQs)
    if(nested_irqs > 1)
    {
        return;
    }
    
    // don't process signals while in a syscall, as we will do this after we
    // finish the syscall
    if(!ct ||
       ct->properties & PROPERTY_IN_SYSCALL ||
       ct->properties & PROPERTY_HANDLING_SIG)
    {
        return;
    }

    if(ct != idle)
    {
        check_pending_signals(r);
    }
}


/*
 * Save task sigmask.
 */
void save_sigmask(void)
{
    struct task_t *ct = cur_task;

    ksigemptyset(&ct->saved_signal_mask);
    ksigorset(&ct->saved_signal_mask,
             &ct->saved_signal_mask, &ct->signal_mask);
}


/*
 * Restore the signal mask from the saved mask.
 * This is called by syscall_sigreturn() upon return from user signal handler.
 */
void restore_sigmask(void)
{
    struct task_t *ct = cur_task;

    kernel_mutex_lock(&ct->task_mutex);
    ksigemptyset(&ct->signal_mask);
    ksigorset(&ct->signal_mask,
             &ct->signal_mask, &ct->saved_signal_mask);
    kernel_mutex_unlock(&ct->task_mutex);
}


/*
 * Handler for syscall sigaction().
 */
int syscall_sigaction(int signum,
                      struct sigaction *newact, struct sigaction *oldact)
{
    struct task_t *ct = cur_task;

    if(signum <= 0 || signum >= NSIG || !ct || !ct->sig)
    {
        return -EINVAL;
    }
  
    /* can't ignore nor catch KILL and STOP signals */
    if(signum == SIGKILL || signum == SIGSTOP)
    {
        return -EINVAL;
    }
  
    kernel_mutex_lock(&ct->task_mutex);
    struct sigaction *act = &ct->sig->signal_actions[signum];
  
    /* save the old action */
    if(oldact)
    {
        if(copy_to_user(oldact, act, sizeof(struct sigaction)) != 0)
        {
            kernel_mutex_unlock(&ct->task_mutex);
            return -EFAULT;
        }
    }
  
    if(newact)
    {
        if(newact->sa_handler == SIG_ERR)
        {
            kernel_mutex_unlock(&ct->task_mutex);
            return -EINVAL;
        }
        
        if(newact->sa_handler != SIG_IGN && newact->sa_handler != SIG_DFL)
        {
            if(!(newact->sa_flags & SA_RESTORER) || !(newact->sa_restorer))
            {
                kernel_mutex_unlock(&ct->task_mutex);
                return -EINVAL;
            }
        }
        
        if(copy_from_user(act, newact, sizeof(struct sigaction)) != 0)
        {
            kernel_mutex_unlock(&ct->task_mutex);
            return -EFAULT;
        }
        
        // ensure SIGKILL and SIGSTOP are not blocked
        ksigdelset(&act->sa_mask, SIGKILL);
        ksigdelset(&act->sa_mask, SIGSTOP);
    }
  
    kernel_mutex_unlock(&ct->task_mutex);
    return 0;
}


/*
 * Handler for syscall signal() - not implemented.
 */
int syscall_signal(int signum, void *handler, void *sa_restorer)
{
    UNUSED(signum);
    UNUSED(handler);
    UNUSED(sa_restorer);
    
    return -ENOSYS;
}


int syscall_sigpending_internal(sigset_t* set, int kernel)
{
    struct task_t *ct = cur_task;

    if(!set || !ct)
    {
        return -EINVAL;
    }
    
    if(kernel)
    {
        //A_memcpy((void *)set, (void *)&ct->signal_pending, sizeof(sigset_t));
        copy_sigset(set, (void *)&ct->signal_pending);
        return 0;
    }
    else
    {
        //COPY_TO_USER((void *)set, (void *)&ct->signal_pending, 
        //                                    sizeof(sigset_t));
        return copy_sigset_to_user(set, (void *)&ct->signal_pending); 
    }
    
    //return 0;
}


/*
 * Handler for syscall sigpending().
 */
int syscall_sigpending(sigset_t* set)
{
    return syscall_sigpending_internal(set, 0);
}


/*
 * Handler for syscall sigtimedwait().
 */
int syscall_sigtimedwait(sigset_t *set, siginfo_t *info,
                         struct timespec *ts)
{
    struct task_t *ct = cur_task;
    struct timespec ats;
    sigset_t pending, wanted, blocked;
    int signum;
    int error = 0;
    unsigned long timo;
    unsigned long long oticks;

    if(!set || !ct)
    {
        return -EINVAL;
    }

    //COPY_FROM_USER(&wanted, (void *)set, sizeof(sigset_t));
    if(copy_sigset_from_user(&wanted, set) != 0)
    {
        return -EFAULT;
    }

    /* make sure SIGKILL and SIGSTOP are not waited for */
    ksigdelset(&wanted, SIGKILL);
    ksigdelset(&wanted, SIGSTOP);
    
    /* do not wait on blocked signals */
    //A_memcpy(&blocked, (void *)&ct->signal_mask, sizeof(sigset_t));
    copy_sigset(&blocked, &ct->signal_mask);
    ksigandset(&blocked, &blocked, &wanted);

    if(!ksigisemptyset(&blocked))
    {
        return -EINVAL;
    }

    /* Check for timeout */
    oticks = ticks;

    if(ts)
    {
        COPY_FROM_USER(&ats, ts, sizeof(ats));
        timo = timespec_to_ticks(&ats);

        /*
         * if the timeout is less than 1 ticks (because the caller specified
         * timeout in usecs that are less than the clock resolution), sleep
         * for 1 tick.
         */
        if(timo == 0 && ats.tv_nsec)
        {
            timo = 1;
        }
    }
    else
    {
        timo = 0;
    }

retry:
    /*
     * See if there is a pending signal.
     */
    //A_memcpy(&pending, (void *)&ct->signal_pending, sizeof(sigset_t));
    copy_sigset(&pending, (void *)&ct->signal_pending);
    
    if(!ksigisemptyset(&pending))
    {
        for(signum = 1; signum < NSIG; signum++)
        {
            if(ksigismember(&pending, signum))
            {
                if(ksigismember(&wanted, signum))
                {
                    if(info)
                    {
                        COPY_TO_USER(info, &ct->siginfo[signum], 
                                           sizeof(siginfo_t));
                    }
                    
                    ksigdelset((sigset_t *)&ct->signal_pending, signum);
                    return signum;
                }
                
                /* TODO: should we remove signum from pending sigs here? */
                return -EINTR;
            }
        }
    }

    /*
     * No signal is pending yet. If both fields of struct timespec == 0,
     * return immediately.
     */
    if(ts)
    {
        if(ats.tv_sec == 0 && ats.tv_nsec == 0)
        {
            goto done;
        }
        
        if(ticks >= (oticks + timo))
        {
            goto done;
        }
        
        timo -= (ticks - oticks);
    }

    error = block_task2(ct, timo);

    if(error == 0)
    {
        goto retry;
    }
    
    error = -error;

done:
    return (error == -EWOULDBLOCK) ? -EAGAIN : error;
}


int syscall_sigprocmask_internal(struct task_t *ct, int how,
                                 sigset_t* userset,
                                 sigset_t* oldset, int kernel)
{
    /* save the old signal mask */
    if(oldset)
    {
        if(kernel)
        {
            //A_memcpy(oldset, &ct->signal_mask, sizeof(sigset_t));
            copy_sigset(oldset, &ct->signal_mask);
        }
        else
        {
            //if(copy_to_user(oldset, &ct->signal_mask,
            //                    sizeof(sigset_t)) != 0)
            if(copy_sigset_to_user(oldset, &ct->signal_mask) != 0)
            {
                return -EFAULT;
            }
        }
    }
  
    kernel_mutex_lock(&ct->task_mutex);
  
    if(userset)
    {
        sigset_t set;

        if(kernel)
        {
            //A_memcpy(&set, userset, sizeof(sigset_t));
            copy_sigset(&set, userset);
        }
        else
        {
            //if(copy_from_user(&set, userset,
            //                    sizeof(sigset_t)) != 0)
            if(copy_sigset_from_user(&set, userset) != 0)
            {
                kernel_mutex_unlock(&ct->task_mutex);
                return -EFAULT;
            }
        }
    
        switch(how)
        {
            case SIG_BLOCK:
	            ksigorset(&ct->signal_mask, &ct->signal_mask, &set);
	            break;
	            
            case SIG_UNBLOCK:
	            ksignotset(&set, &set);
	            ksigandset(&ct->signal_mask, &ct->signal_mask, &set);
	            break;
	            
            case SIG_SETMASK:
	            //A_memcpy(&ct->signal_mask, &set, sizeof(sigset_t));
	            copy_sigset(&ct->signal_mask, &set);
	            break;
	            
            default:
                kernel_mutex_unlock(&ct->task_mutex);
	            return -EINVAL;
        };
        
        /* make sure SIGKILL and SIGSTOP are not blocked */
        ksigdelset(&ct->signal_mask, SIGKILL);
        ksigdelset(&ct->signal_mask, SIGSTOP);
        //update_pending_signals(t);
    }

    kernel_mutex_unlock(&ct->task_mutex);
    return 0;
}


/*
 * Handler for syscall sigprocmask().
 */
int syscall_sigprocmask(int how, sigset_t *userset, sigset_t *oldset)
{
    return syscall_sigprocmask_internal(cur_task, how, userset, oldset, 0);
}


/*
 * Handler for syscall sigsuspend().
 */
int syscall_sigsuspend(sigset_t *set)
{
    sigset_t old_mask;
    struct task_t *ct = cur_task;
    kernel_mutex_lock(&ct->task_mutex);
  
    if(set)
    {
        //A_memcpy(&old_mask, &ct->signal_mask, sizeof(sigset_t));
        copy_sigset(&old_mask, &ct->signal_mask);

        //if(copy_from_user(&ct->signal_mask,
        //                        set, sizeof(sigset_t)) != 0)
        if(copy_sigset_from_user(&ct->signal_mask, set) != 0)
        {
            kernel_mutex_unlock(&ct->task_mutex);
            return -EFAULT;
        }

        /* make sure SIGKILL and SIGSTOP are not blocked */
        ksigdelset(&ct->signal_mask, SIGKILL);
        ksigdelset(&ct->signal_mask, SIGSTOP);
    }
  
    kernel_mutex_unlock(&ct->task_mutex);
  
    /* wait for a signal to wake up.... */
    syscall_pause();
    //block_task(ct, 1);

    kernel_mutex_lock(&ct->task_mutex);
  
    if(set)
    {
        //A_memcpy(&ct->signal_mask, &old_mask, sizeof(sigset_t));
        copy_sigset(&ct->signal_mask, &old_mask);
    }

    kernel_mutex_unlock(&ct->task_mutex);
    return -EINTR;
}


/*
 * TODO: dump the core.
 */
void dump_core(void)
{
    /*
     * NOTE: POSIX says:
     *          There is a potential security problem in creating a core file if 
     *          the process was set-user-ID and the current user is not the owner 
     *          of the program, if the process was set-group-ID and none of the 
     *          user's groups match the group of the program, or if the user does 
     *          not have permission to write in the current directory. In this 
     *          situation, an implementation either should not create a core file 
     *          or should make it unreadable by the user.
     */
}


/*
 * Handler for syscall signalstack().
 */
int syscall_signaltstack(stack_t *ss, stack_t *old_ss)
{
    stack_t newss;
    struct memregion_t *memregion = NULL;
    struct task_t *ct = cur_task;

    if(old_ss)
    {
        COPY_TO_USER(old_ss, &ct->signal_stack, sizeof(stack_t));
    }
    
    if(ss)
    {
        COPY_FROM_USER(&newss, ss, sizeof(stack_t));

        kernel_mutex_lock(&ct->task_mutex);
        
        // check for invalid flags
        if(newss.ss_flags &&
           newss.ss_flags != SS_ONSTACK && newss.ss_flags != SS_DISABLE)
        {
            kernel_mutex_unlock(&ct->task_mutex);
            return -EINVAL;
        }

        // check we are not changing stack while a signal handler is
        // executing on the stack
        if(ct->signal_stack.ss_flags == SS_ONSTACK)
        {
            kernel_mutex_unlock(&ct->task_mutex);
            return -EPERM;
        }
        
        // if setting new signal stack ...
        if(newss.ss_flags == 0 || newss.ss_flags == SS_ONSTACK)
        {
            newss.ss_flags = 0;
        
            // check the new stack size is >= minimum allowed
            if(newss.ss_size < MINSIGSTKSZ)
            {
                kernel_mutex_unlock(&ct->task_mutex);
                return -ENOMEM;
            }
            
            // check the stack address is valid
            // check we're not trying to map kernel memory
            if(((uintptr_t)newss.ss_sp >= USER_MEM_END) ||
               (((uintptr_t)newss.ss_sp + newss.ss_size) > USER_MEM_END))
            {
                kernel_mutex_unlock(&ct->task_mutex);
                return -EINVAL;
            }

            if((memregion = memregion_containing(ct,
                                        (virtual_addr)newss.ss_sp)) == NULL)
            {
                kernel_mutex_unlock(&ct->task_mutex);
                SYSCALL_EFAULT(newss.ss_sp);
            }
        }

        A_memcpy(&ct->signal_stack, &newss, sizeof(stack_t));
        kernel_mutex_unlock(&ct->task_mutex);
    }

    return 0;
}


/*
 * Add a signal to a task.
 */
int add_task_signal(struct task_t *task, int signum, 
                    siginfo_t *siginfo, int force)
{
    struct task_t *ct = cur_task;
    
    if(!task)
    {
        return -ESRCH;
    }

    if(signum < 0 || signum >= NSIG)
    {
        return -EINVAL;
    }

    /* NULL signal is used for debugging & error checking, but doesn't actually
       deliver a signal */
    if(signum == 0)
    {
        return 0;
    }
  
    /*
     * TODO: if a child sent us a SIGCHLD and we set SA_NOCLDSTOP flag, 
     *       ignore the signal. 
     */

    if(ksigismember((sigset_t *)&task->signal_pending, signum))
    {
        goto out;
    }
    
    /* don't signal kernel server tasks */
    if(!task->user)
    {
        return -EPERM;
    }

    /* check the sender has permission to send the signal */
    if(!force &&
       (ct->uid != task->uid) && (ct->uid != task->ssuid) &&
       (ct->euid != task->uid) && (ct->euid != task->ssuid))
    {
        return -EPERM;
    }
  
    if(signum == SIGSTOP || signum == SIGTSTP || 
       signum == SIGTTIN || signum == SIGTTOU)
    {
        ksigdelset((sigset_t *)&task->signal_pending, SIGCONT);
    }
    else if(signum == SIGCONT)
    {
        ksigdelset((sigset_t *)&task->signal_pending, SIGSTOP);
        ksigdelset((sigset_t *)&task->signal_pending, SIGTSTP);
        ksigdelset((sigset_t *)&task->signal_pending, SIGTTIN);
        ksigdelset((sigset_t *)&task->signal_pending, SIGTTOU);
    }
  
    /* other signal.. just add to pending queue */
    ksigaddset((sigset_t *)&task->signal_pending, signum);

    if(siginfo)
    {
        A_memcpy(&task->siginfo[signum], siginfo, sizeof(siginfo_t));
    }
    else
    {
        A_memset(&task->siginfo[signum], 0, sizeof(siginfo_t));
    }
    
    task->siginfo[signum].si_signo = signum;
    

out:

    /* a sleeping task must be woken up to receive the KILL signal */
    if((task->state == TASK_SLEEPING) &&
       task->sig->signal_actions[signum].sa_handler != SIG_IGN &&
       !ksigismember(&task->signal_mask, signum))
    {
        KDEBUG("add_task_signal: waking task with signum %d\n", signum);
        task->woke_by_signal = signum;
        //unblock_task(task);
        unblock_task_no_preempt(task);
    }

    return 0;
}


/*
 * Handy way of sending a signal from a user task.
 */
int user_add_task_signal(struct task_t *t, int signum, int force)
{
    struct task_t *ct = cur_task;
    siginfo_t siginfo = {
                         .si_code = SI_USER,
                         .si_pid = ct->pid,
                         .si_uid = ct->uid,
                        };

    return add_task_signal(t, signum, &siginfo, force);
}


/*
 * Send SIGCHLD to the task's parent, filling the appropriate fields in
 * the siginfo_t structure. Field 'status' contains the task's exit status
 * (and code should be CLD_EXITED), or the signal number that caused the
 * task to change state.
 */
int add_task_child_signal(struct task_t *t, int code, int status)
{
    if(!t->parent)
    {
        return 0;
    }

    KDEBUG("add_task_child_signal: t->parent->properties 0x%x\n", t->parent->properties);

    // parent might want to block SIGCHLD and wait for us to change
    // status by calling one of the wait() functions, in which case it
    // will be blocked and we need to wake it up
    if(t->parent->properties & PROPERTY_IN_WAIT)
    {
        unblock_task_no_preempt(t->parent);
        return 0;
    }

    // check if parent cares about us changing status
    if(t->parent->sig)
    {
        struct sigaction *act = &t->parent->sig->signal_actions[SIGCHLD];

        if((act->sa_handler != SIG_IGN) && !(act->sa_flags & SA_NOCLDSTOP))
        {
            siginfo_t siginfo = {
                         .si_code = code,
                         .si_pid = t->pid,
                         .si_uid = t->uid,
                         .si_status = status,
                         .si_utime = t->user_time,
                         .si_stime = t->sys_time,
                        };

            return add_task_signal(t->parent, SIGCHLD, &siginfo, 1);
        }
    }
    
    return 0;
}


int add_task_timer_signal(struct task_t *t, int signum, ktimer_t timerid)
{
    siginfo_t siginfo = {
                         .si_code = SI_TIMER,
                         .si_value.sival_int = timerid,
                         //.si_value.sival_ptr = ptr,
                        };

    KDEBUG("add_task_timer_signal: signum %d, timerid %d\n", signum, timerid);

    //COPY_TO_USER(ptr, &timerid, sizeof(ktimer_t));
    ksigaddset(&t->signal_timer, signum);

    return add_task_signal(t, signum, &siginfo, 1);
}


int add_task_segv_signal(struct task_t *t, int signum, int code, void *addr)
{
    siginfo_t siginfo = {
                         .si_code = code,
                         .si_addr = addr,
                        };

    return add_task_signal(t, signum, &siginfo, 1);
}

