/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: execve.c
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
 *  \file execve.c
 *
 *  Kernel execve() implementation.
 */

//#define __DEBUG

#include <errno.h>
#include <libgen.h>             // for POSIX basename()
#include <string.h>
#include <fcntl.h>              // AT_FDCWD
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/pcache.h>
#include <kernel/vfs.h>
#include <kernel/elf.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/fpu.h>
#include <kernel/ptrace.h>
#include <kernel/ipc.h>
#include <kernel/ksigset.h>
#include <kernel/msr.h>
#include <mm/kheap.h>
#include <mm/kstack.h>
#include <mm/mmap.h>
#include <fs/procfs.h>
#include <gui/vbe.h>


#ifdef __x86_64__
# define STACK_STEP     8
#else
# define STACK_STEP     4
#endif


/*
 * Free temporary memory used to store argv & envp.
 */
static void free_tmpmem(char **argv)
{
    char **p = argv;
    
    if(!argv)
    {
        return;
    }
    
    while(*p)
    {
        kfree(*p++);
    }
    
    kfree(argv);
}


/*
 * Count the number of arguments/environ-variables, as well as the total
 * length of all arguments in the given list.
 *
 * Inputs:
 *    argv => argument list
 *
 * Outputs:
 *    nargv => new argument list pointer is stored here
 *    tlen => total length of all strings is stored here
 *
 * Returns:
 *    arg count on success, -errno on failure
 */
static int count_args(char **argv, char ***nargv, size_t *tlen)
{
    char **p = argv, *tmp;
    char **new_argv;
    size_t argc = 0, j, len;
    int i;

    *tlen = 0;
    *nargv = NULL;

    while((i = copy_from_user(&tmp, p, sizeof(char *))) == 0)
    {
        if(!tmp || !*tmp)
        {
            break;
        }

        argc++;
        p++;
    }

    if(!argc)
    {
        return -EINVAL;
    }

    if(i < 0)
    {
        return -EFAULT;
    }

    if(!(new_argv = kmalloc(sizeof(char *) * (argc + 1))))
    {
        return -ENOMEM;
    }

    A_memset(new_argv, 0, sizeof(char *) * (argc + 1));
    p = argv;

    for(j = 0; j < argc; j++, p++)
    {
        if(copy_str_from_user(*p, &tmp, &len) < 0)
        {
            free_tmpmem(new_argv);
            return -EFAULT;
        }

        KDEBUG("count_args: arg %s\n", tmp);

        new_argv[j] = tmp;
        len++;

        if(len % sizeof(size_t))
        {
            len += (sizeof(size_t) - (len % sizeof(size_t)));
        }

        KDEBUG("count_args: len %d\n", len);
        
        *tlen += len;
    }

    KDEBUG("count_args: final *tlen %d\n", *tlen);

    *nargv = new_argv;
    return argc;
}


static int count_invk_args(char **argv, char ***nargv, size_t *tlen)
{
    int i = 0;
    size_t len;
    char **p = argv;
    char **new_argv = (char **)kmalloc(sizeof(char *) * 3);
    char **np = new_argv;
    *tlen = 0;
    *nargv = NULL;
    
    if(!new_argv)
    {
        return -ENOMEM;
    }
    
    A_memset((void *)new_argv, 0, sizeof(char *) * 3);
    
    for(i = 0; i < 2; i++, p++)
    {
        len = strlen(*p) + 1;
        *np = (char *)kmalloc(len);
        
        if(!*np)
        {
            free_tmpmem(new_argv);
            return -ENOMEM;
        }
        
        A_memcpy(*np, *p, len);
        
        if(len % sizeof(size_t))
        {
            len += (sizeof(size_t) - (len % sizeof(size_t)));
        }
        
        *tlen += len;
        np++;
    }

    *nargv = new_argv;
    return i;
}


static int may_alloc_page(uintptr_t addr)
{
    pt_entry *pt = get_page_entry((void *)addr);

    if(!pt)
    {
        return 0;
    }

    // check if we have already alloc'd this page and if so, skip it
    if(PTE_PRESENT(*pt))
    {
        return 1;
    }

    if(!vmmngr_alloc_page(pt, PTE_FLAGS_PWU | I86_PTE_PRIVATE))
    {
        return 0;
    }

    vmmngr_flush_tlb_entry(addr);
    return 1;
}


/*
 * Copy argv/envp strings to the top of user's memory. The user stack will
 * begin right below this top segment of memory, which is just below 0xC0000000.
 * Pointers to strings are stored in memory just below this.
 * Here is how it looks:
 *
 *    +----------------+ High memory (0xC0000000 the 1st time we're called)
 *    | argN           |
 *    +----------------+
 *    | ...            |
 *    +----------------+
 *    | arg0           |
 *    +----------------+
 *    | N arg pointers |
 *    +----------------+ Lower memory (depends on how many args are there)
 *
 * Inputs:
 *    argc => argument count
 *    argv => argument list
 *    stack => top of stack - we will copy argv starting @ stack-4 and going
 *             towards lower memory
 *    tlen => total length of all argv strings
 *
 * Returns:
 *    address of last argument copied to stack on success, 0 on failure
 */
static uintptr_t copy_strs(int argc, char **argv, uintptr_t stack, 
                           size_t tlen, int dyn_loaded)
{
    uintptr_t tmp, arr, *parr;
    volatile char *p, *p2;
    int i = 0;
    KDEBUG("copy_strs: argc " _XPTR_ ", argv " _XPTR_ ", stack " _XPTR_ ", tlen " _XPTR_ "\n", argc, argv, stack, tlen);
    
    // calculate offsets
    p2 = (char *)(stack - tlen);
    arr = (uintptr_t)p2 - ((argc + 1) * sizeof(uintptr_t));

    if(dyn_loaded)
    {
        p2 -= 8;
        arr -= (sizeof(uintptr_t) + 8);
    }

    parr = (uintptr_t *)arr;
    KDEBUG("copy_strs: p2 " _XPTR_ ", arr " _XPTR_ ", parr " _XPTR_ "\n", p2, arr, parr);
    
    // make sure we have alloc'd pages.
    // we need pages for the arguments themselves, in addition to the
    // arg pointers we'll place below the args in the stack.
    tmp = arr & ~(PAGE_SIZE - 1);

    while(tmp < stack)
    {
        KDEBUG("copy_strs: tmp " _XPTR_ ", stack " _XPTR_ "\n", tmp, stack);

        if(!may_alloc_page(tmp))
        {
            return 0;
        }
        
        tmp += PAGE_SIZE;
    }

    // add entry for the dynamic loader if needed
    if(dyn_loaded)
    {
        static char ldso[] = "ld.so";
        *parr++ = (uintptr_t)p2;
        p = ldso;

        while((*p2++ = *p++))
        {
            ;
        }
        
        uintptr_t r = (uintptr_t)p2 % sizeof(uintptr_t);
        
        if(r)
        {
            p2 += (sizeof(uintptr_t) - r);
        }
    }
    
    // copy the arguments to the new process's stack
    while(i < argc)
    {
        p = argv[i];
        *parr++ = (uintptr_t)p2;
        KDEBUG("copy_strs: %d/%d '%s'\n", i+1, argc, p);
        
        while((*p2++ = *p++))
        {
            ;
        }
        
        uintptr_t r = (uintptr_t)p2 % sizeof(uintptr_t);
        
        if(r)
        {
            p2 += (sizeof(uintptr_t) - r);
        }
        
        i++;
    }
    
    *parr = 0;
    return arr;
}


/*
 * Handler for syscall execve().
 *
 * The actual execve syscall function.
 */

#define VALID_FLAGS         (AT_SYMLINK_NOFOLLOW)

int syscall_execve(char *path, char **argv, char **env)
{
    return syscall_execveat(AT_FDCWD, path, argv, env, 0);
}


/*
 * Handler for syscall execveat().
 */
int syscall_execveat(int dirfd, char *path, 
                     char **argv, char **env, int flags)
{
    KDEBUG("do_execve: path '%s' @ " _XPTR_ ", argv @ " _XPTR_ ", env @ " _XPTR_ "\n", path, path, argv, env);

    int res, i;
    int argc = 0, envc = 0, invkc = 0;
    size_t arglen = 0, envlen = 0, invklen = 0;
    uintptr_t stack = 0, argp = 0, envp = 0, invkp = 0, eip = 0;
    char **new_argv = NULL, **new_env = NULL, **new_invk = NULL;
    size_t *auxv;
    struct fs_node_t *filenode = NULL;
    struct cached_page_t *buf = NULL;
    struct mount_info_t *dinfo;
    pid_t oldtid;
    int followlink = !(flags & AT_SYMLINK_NOFOLLOW);
    
    // init exec is a special case as path is in kernel space not user space.
    // we also pass the OPEN_CREATE_DENTRY flag so that vfs_open_internal()
    // creates a dentry we can use with e.g. when reading /proc/[pid]/maps
	int open_flags = (((cur_task == init_task) || !cur_task->user) ?
	                    OPEN_KERNEL_CALLER : OPEN_USER_CALLER) |
	                        (followlink ? OPEN_FOLLOW_SYMLINK : 0) | 
	                            OPEN_CREATE_DENTRY;

    if(!path || !argv || !env || (flags & ~VALID_FLAGS))
    {
        return -EINVAL;
    }

#define AUXV_MEMSZ          (AUXV_SIZE * sizeof(size_t) * 2)

    if(!(auxv = kmalloc(AUXV_MEMSZ)))
    {
        return -ENOMEM;
    }

    A_memset(auxv, 0, AUXV_MEMSZ);
    
    // This array has two members:
    // invk[0] => full pathname of the exec'd program (newlib will save this
    //            in the global variable program_invocation_name)
    // invk[1] => short name of the exec'd program (newlib will save this
    //            in the global variable program_invocation_short_name)
    char *invk[] = { path, basename(path), NULL };

    // get the executable's file node
    if((res = vfs_open_internal(path, dirfd, 
                                &filenode, open_flags)) < 0)
    {
        KDEBUG("do_execve - res = %d\n", res);
        kfree(auxv);
        return res;
    }

    if(!filenode)
    {
        kfree(auxv);
        return -ENOENT;
    }
    
    // check it is a regular file
    if(!S_ISREG(filenode->mode))
    {
        KDEBUG("do_execve - filenode->mode = %d\n", filenode->mode);
        release_node(filenode);
        kfree(auxv);
        return -EACCES;
    }
    
    // check we have exec permission
    if(has_access(filenode, EXECUTE, 0) != 0)
    {
        /* 
         * TODO: handle interpreter scripts
         */

        release_node(filenode);
        kfree(auxv);
        return -ENOEXEC;
    }

    // read executable header
    if(!(buf = get_cached_page(filenode, 0, 0)))
    {
        release_node(filenode);
        kfree(auxv);
        return -EACCES;
	}

    // get a kernel stack (if we don't already have one)
    if(!cur_task->kstack_virt)
    {
        if(get_kstack((physical_addr *)&cur_task->kstack_phys, 
                      (virtual_addr *)&cur_task->kstack_virt) != 0)
        {
            release_node(filenode);
            release_cached_page(buf);
            kfree(auxv);
            return -ENOMEM;
        }
    }
    
    // Count argv & envp and copy args & env to kernel memory.
    // We do this before freeing user space, after which we'll have no access
    // to user data!
    if((argc = count_args(argv, &new_argv, &arglen)) < 0)
    {
        release_node(filenode);
        release_cached_page(buf);
        kfree(auxv);
        return argc;
    }
    
    if((envc = count_args(env, &new_env, &envlen)) < 0)
    {
        free_tmpmem(new_argv);
        release_node(filenode);
        release_cached_page(buf);
        kfree(auxv);
        return envc;
    }
    
    if((invkc = count_invk_args(invk, &new_invk, &invklen)) < 0)
    {
        free_tmpmem(new_argv);
        free_tmpmem(new_env);
        release_node(filenode);
        release_cached_page(buf);
        //brelse(buf);
        kfree(auxv);
        return argc;
    }

    /*
     * TODO: The execve manpage says:
     *
     *    All process attributes are preserved during an execve(), except
     *    the following:
     *    *  The dispositions of any signals that are being caught are reset
     *       to the default (signal(7)).
     *    *  Any alternate signal stack is not preserved (sigaltstack(2)).
     *    *  Memory mappings are not preserved (mmap(2)).
     *    *  Attached System V shared memory segments are detached (shmat(2)).
     *    *  POSIX shared memory regions are unmapped (shm_open(3)).
     *    *  Open POSIX message queue descriptors are closed (mq_overview(7)).
     *    *  Any open POSIX named semaphores are closed (sem_overview(7)).
     *    *  POSIX timers are not preserved (timer_create(2)).
     *    *  Any open directory streams are closed (opendir(3)).
     *    *  Memory locks are not preserved (mlock(2), mlockall(2)).
     *    *  Exit handlers are not preserved (atexit(3), on_exit(3)).
     *    *  The floating-point environment is reset to the default (see fenv(3)).
     */

    /* kill the other threads */
    __terminate_thread_group();
    
    cur_task->ldt.base = 0;
    cur_task->ldt.limit = 0xFFFFFFFF;

#ifdef __x86_64__

    wrmsr(IA32_FS_BASE, 0);

#else

    // 0x30 - DATA Descriptor for TLS
    gdt_add_descriptor(GDT_TLS_DESCRIPTOR, 0, 0xFFFFFFFF, 0xF2);

#endif

    /* if there are other threads, they should be zombies, so reap them */
    reap_dead_threads(cur_task);
    
    /* set thread group's exit status */
    cur_task->threads->thread_group_leader = cur_task;
    cur_task->threads->thread_count = 1;
    cur_task->thread_group_next = NULL;
    cur_task->properties &= ~PROPERTY_DYNAMICALLY_LOADED;

    /*
     * Reset the task's tid.
     */
    int found = 0;
    oldtid = cur_task->pid;

    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(*t && *t != cur_task && (*t)->pid == cur_task->pid)
        {
            found = 1;
            break;
        }
    }
    
    elevated_priority_unlock(&task_table_lock);

    if(!found && cur_task->pid != tgid(cur_task))
    {
        cur_task->pid = tgid(cur_task);
    }

    set_task_comm(cur_task, invk[1]);

    disarm_timers(tgid(cur_task));
    //disarm_timers(ct);

    // Free current user mem pages. We do this because loading the new
    // executable might result in some memory pages being mapped to
    // different addresses, and we'll lose the old mappings and end up
    // with allocated (but unused) physical memory frames.
    
    // NOTE: we cannot go back if something wrong happens after releasing
    //       our mem pages!

    // don't free pages as free_user_pages() will do it below.
    // we need two calls as free_user_pages() will also free the page tables.

    // NOTE: we don't free pages if cur_task was created by calling vfork,
    //       as the parent and child process share the same memory space
    //       and memory region structs.

    kernel_mutex_lock(&(cur_task->mem->mutex));

    memregion_detach_user(cur_task, 0);

    if(cur_task->properties & PROPERTY_VFORK)
    {
        // if this task was vforked, it used the parent's page directory
        // and now it needs its own, so clone the idle task's page directory
        if(clone_task_pd(idle_task, cur_task) != 0)
        {
            kernel_mutex_unlock(&(cur_task->mem->mutex));
            free_tmpmem(new_invk);
            free_tmpmem(new_argv);
            free_tmpmem(new_env);
            kfree(auxv);
            syscall_exit(-1);
        }

        // now load the new page directory
        vmmngr_switch_pdirectory((pdirectory *)cur_task->pd_phys,
                                 (pdirectory *)cur_task->pd_virt);
    }
    else
    {
        free_user_pages(cur_task->pd_virt);
    }

    kernel_mutex_unlock(&(cur_task->mem->mutex));
    
    // load ELF file sections to memory
    if((res = elf_load_file(filenode, buf, auxv, ELF_FLAG_NONE)) != 0)
    {
        free_tmpmem(new_invk);
        free_tmpmem(new_argv);
        free_tmpmem(new_env);
        release_node(filenode);
        release_cached_page(buf);
        kfree(auxv);
        syscall_exit(-1);
    }
    
    cur_task->exe_dev = filenode->dev;
    cur_task->exe_inode = filenode->inode;

    // change task's permissions if executable is suid and:
    //    - the underlying filesystem is not mounted nosuid
    //    - the calling process is not being ptraced

    if((dinfo = get_mount_info(filenode->dev)) &&
       !(dinfo->flags & MS_NOSUID) &&
       !(cur_task->properties & PROPERTY_TRACE_SIGNALS))
    {
        if((filenode->mode & S_ISUID) == S_ISUID)
        {
            cur_task->euid = filenode->uid;
        }

        if((filenode->mode & S_ISGID) == S_ISGID)
        {
            cur_task->egid = filenode->gid;
        }
    }


    release_cached_page(buf);
    release_node(filenode);
    
    // bootstrap the new process's stack
    argp = copy_strs(argc, new_argv, STACK_START, arglen,
                     (cur_task->properties & PROPERTY_DYNAMICALLY_LOADED));
    free_tmpmem(new_argv);
    
    cur_task->arg_start = (void *)(STACK_START - arglen);
    cur_task->arg_end = (void *)STACK_START;

    if(!argp)
    {
        kfree(auxv);
        syscall_exit(-1);
    }
    
    if(cur_task->properties & PROPERTY_DYNAMICALLY_LOADED)
    {
        argc++;
    }
    
    envp = copy_strs(envc, new_env, argp, envlen, 0);
    free_tmpmem((char **)new_env);
    
    cur_task->env_start = (void *)(argp - envlen);
    cur_task->env_end = (void *)argp;

    if(!envp)
    {
        kfree(auxv);
        syscall_exit(-1);
    }


    invkp = copy_strs(invkc, new_invk, envp, invklen, 0);
    free_tmpmem(new_invk);

    if(!invkp)
    {
        kfree(auxv);
        syscall_exit(-1);
    }

    stack = invkp;


    for(found = 0; found < AUXV_SIZE * 2; found += 2)
    {
        if(auxv[found] == AT_ENTRY)
        {
            eip = auxv[found + 1];
            //break;
        }
        else if(auxv[found] == 0)
        {
            auxv[found] = AT_EXECFN;
            auxv[found + 1] = *(uintptr_t *)invkp;
            break;
        }
    }

    if(!eip)
    {
        kpanic("invalid eip in syscall_execveat()\n");
    }

    // Copy the auxiliary vector onto the stack
    stack -= AUXV_MEMSZ;
    stack &= ~0x0f;
    may_alloc_page(stack);
    //printk("stack %lx, auxv %lx\n", stack, auxv);
    A_memcpy((void *)stack, auxv, AUXV_MEMSZ);

    kfree(auxv);
    
    /*
     * At this point, the new stack looks like:
     *
     *    +----------------+ High memory (STACK_START = 0xC0000000 on x86)
     *    | argN           |
     *    +----------------+
     *    | ...            |
     *    +----------------+
     *    | arg0           |
     *    +----------------+
     *    | N arg pointers |
     *    +----------------+
     *
     *    +----------------+
     *    | envN           |
     *    +----------------+
     *    | ...            |
     *    +----------------+
     *    | env0           |
     *    +----------------+
     *    | N env pointers |
     *    +----------------+
     *
     *    +----------------+
     *    | invkN          |
     *    +----------------+
     *    | ...            |
     *    +----------------+
     *    | invk0          |
     *    +----------------+
     *    | N invk pointers|
     *    +----------------+
     *
     *    +----------------+
     *    | aux vector     |
     *    +----------------+ Lower memory (passed to task in %esp)
     */


#ifdef __x86_64__

    // main() args => argc, argv, envp, invkp
    cur_task->execve.rdi = argc;
    cur_task->execve.rsi = argp;
    cur_task->execve.rdx = envp;
    cur_task->execve.r8 = invkp;

#endif      /* !__x86_64__ */

    // we may need to alloc another page for the env and arg pointers if the
    // current bottom of the stack lies at the bottom of a physical page
    may_alloc_page(stack - (3 * sizeof(uintptr_t)));

#define PUSH(v)             stack -= sizeof(uintptr_t);         \
                            *(volatile uintptr_t *)stack = (uintptr_t)(v);

    //PUSH(invkp);
    PUSH(envp);
    PUSH(argp);
    PUSH(argc);

#undef PUSH

    // add the newly allocated stack to the task's memory map
    if(memregion_alloc_and_attach(cur_task, NULL, 0, 0,
                                  (virtual_addr)stack & ~(PAGE_SIZE - 1),
                                  STACK_START,
                                  PROT_READ | PROT_WRITE,
                                  MEMREGION_TYPE_STACK,
                                  MAP_PRIVATE /* | MEMREGION_FLAG_USER */, 0) 
                                                != 0)
    {
        syscall_exit(-1);
    }

    // reset task signals (except for ignored signals)
    for(i = 0; i < NSIG; i++)
    {
        if(cur_task->sig->signal_actions[i].sa_handler == SIG_IGN)
        {
            continue;
        }
        
        ksigemptyset(&cur_task->sig->signal_actions[i].sa_mask);
        cur_task->sig->signal_actions[i].sa_handler = SIG_DFL;
        cur_task->sig->signal_actions[i].sa_cookie = NULL;
        cur_task->sig->signal_actions[i].sa_flags = 0;
    }
    
    ksigemptyset((sigset_t *)&cur_task->signal_pending);
    ksigemptyset(&cur_task->signal_caught);
    //ksigemptyset((sigset_t *)&cur_task->signal_mask);
    ksigemptyset(&cur_task->signal_timer);
    cur_task->woke_by_signal = 0;
    //cur_task->sigreturn = 0;
    A_memset(&cur_task->signal_stack, 0, sizeof(stack_t));

    // close open files that are marked close-on-exec
    for(i = 0; i < NR_OPEN; i++)
    {
        if(is_cloexec(cur_task, i))
        {
            syscall_close(i);
        }
    }

    cur_task->cloexec = 0;

    cur_task->end_stack = (uintptr_t)stack;

#ifdef __x86_64__
    cur_task->execve.rip = eip;
    cur_task->execve.rbp = (uint64_t)stack;
    cur_task->execve.rsp = (uint64_t)stack;
#else
    cur_task->execve.eip = eip;
    cur_task->execve.ebp = (uint32_t)stack;
    cur_task->execve.esp = (uint32_t)stack;
#endif
    
    /*
     * unblock our parent if we vforked
     *
     * we do this because after a vfork, the parent is blocked until the child:
     * 1. exits by calling _exit() or after receiving a signal
     * 2. calls execve()
     *
     * for more details, see: 
     *      https://man7.org/linux/man-pages/man2/vfork.2.html
     */
    if((cur_task->properties & PROPERTY_VFORK) && 
       (cur_task->parent->state == TASK_WAITING))
    {
        cur_task->properties &= ~PROPERTY_VFORK;
        unblock_task(cur_task->parent);
    }


#ifndef __x86_64__
    forget_fpu(cur_task);
#endif

    /*
     * If execve and PTRACE_O_TRACEEXEC is set, the ptrace manpage says:
     *    Stop the tracee at the next execve. A waitpid(2) by the tracer will
     *    return a status value such that
     *
     *       status>>8 == (SIGTRAP | (PTRACE_EVENT_EXEC<<8))
     *
     *    If the execing thread is not a thread group leader, the thread ID is
     *    reset to thread group leader's ID before this stop. The former thread
     *    ID can be retrieved with PTRACE_GETEVENTMSG.
     */
    if((cur_task->properties & PROPERTY_TRACE_SIGNALS) &&
       (cur_task->ptrace_options & PTRACE_O_TRACEEXEC))
    {
        cur_task->ptrace_eventmsg = oldtid;
        ptrace_signal(SIGTRAP, PTRACE_EVENT_EXEC);
    }

    enter_user();
    for(;;) ;

    return 0;
}

