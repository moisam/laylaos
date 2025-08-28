/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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

#ifndef ISSPACE
#define ISSPACE(c)      ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
#endif

#define ALIGN_UP_TO_SIZET(len)                              \
    if((len) % sizeof(size_t))                              \
        (len) += (sizeof(size_t) - ((len) % sizeof(size_t)));


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
static long count_args(char **argv, char ***nargv, size_t *tlen)
{
    char **p = argv, *tmp;
    char **new_argv;
    size_t argc = 0, j, len;
    long i;

    *tlen = 0;
    *nargv = NULL;

    while((i = copy_from_user(&tmp, p, sizeof(char *))) == 0)
    {
        if(!tmp /* || !*tmp */)
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

        new_argv[j] = tmp;
        len++;
        ALIGN_UP_TO_SIZET(len);
        
        *tlen += len;
    }

    *nargv = new_argv;
    return argc;
}


static long count_invk_args(char **argv, char ***nargv, size_t *tlen)
{
    long i = 0;
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
        ALIGN_UP_TO_SIZET(len);
        
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
    if(PTE_FRAME(*pt) /* PTE_PRESENT(*pt) */)
    {
        return 1;
    }

    if(!vmmngr_alloc_page(pt, PTE_FLAGS_PWU | I86_PTE_PRIVATE))
    {
        return 0;
    }

    vmmngr_flush_tlb_entry(addr);
    A_memset((void *)(addr & ~(PAGE_SIZE - 1)), 0, PAGE_SIZE);

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
    
    // calculate offsets
    p2 = (char *)(stack - tlen);
    arr = (uintptr_t)p2 - ((argc + 1) * sizeof(uintptr_t));

    if(dyn_loaded)
    {
        p2 -= 8;
        arr -= (sizeof(uintptr_t) + 8);
    }

    parr = (uintptr_t *)arr;
    
    // make sure we have alloc'd pages.
    // we need pages for the arguments themselves, in addition to the
    // arg pointers we'll place below the args in the stack.
    tmp = arr & ~(PAGE_SIZE - 1);

    while(tmp < stack)
    {
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


static inline void *malloced_copy(void *p, int count)
{
    void *buf = (void *)kmalloc(count + 1);
    
    if(buf)
    {
        A_memcpy(buf, p, count);
    }
    
    return buf;
}


static long parse_interpreter_line(char *line, char *end, char **resarg, int maxargs)
{
    char *nl, *p, *p2;
    long i = 0;

    // find the newline char
    for(nl = line + 2; nl < end; nl++)
    {
        if(*nl == '\r' || *nl == '\n')
        {
            break;
        }
    }

    if(nl == end)
    {
        return 0;
    }

    p = line + 2;

    while(p < nl)
    {
        while(p < nl && ISSPACE(*p))
        {
            p++;
        }

        if(p == nl)
        {
            break;
        }

        if(i == maxargs - 1)
        {
            p2 = nl;
        }
        else
        {
            p2 = p;

            while(p2 < nl && !ISSPACE(*p2))
            {
                p2++;
            }
        }

        if(!(resarg[i] = malloced_copy(p, p2 - p)))
        {
            while(i--)
            {
                if(resarg[i])
                {
                    kfree(resarg[i]);
                    resarg[i] = NULL;
                }
            }

            return 0;
        }

        resarg[i][p2 - p] = '\0';
        p = p2;
        i++;
    }

    return i;
}


static inline long get_exec_filenode(int dirfd, char *path, int open_flags,
                                     struct fs_node_t **filenode)
{
    long res;

    if((res = vfs_open_internal(path, dirfd, 
                                filenode, open_flags)) < 0)
    {
        return res;
    }

    if(!*filenode)
    {
        return -ENOENT;
    }
    
    // check it is a regular file
    if(!S_ISREG((*filenode)->mode))
    {
        release_node(*filenode);
        return -EACCES;
    }
    
    // check we have exec permission
    if(has_access(*filenode, EXECUTE, 0) != 0)
    {
        /* 
         * TODO: handle interpreter scripts
         */

        release_node(*filenode);
        return -ENOEXEC;
    }

    return 0;
}


/*
 * Handler for syscall execve().
 *
 * The actual execve syscall function.
 */

#define VALID_FLAGS         (AT_SYMLINK_NOFOLLOW)

long syscall_execve(char *path, char **argv, char **env)
{
    return syscall_execveat(AT_FDCWD, path, argv, env, 0);
}


/*
 * Handler for syscall execveat().
 */
long syscall_execveat(int dirfd, char *path, 
                      char **argv, char **env, int flags)
{
    volatile long res = 0, i;
    int argc = 0, envc = 0, invkc = 0;
    size_t arglen = 0, envlen = 0, invklen = 0;
    uintptr_t stack = 0, argp = 0, envp = 0, invkp = 0, eip = 0;
    char **new_argv = NULL, **new_env = NULL, **new_invk = NULL;
    size_t *auxv = NULL;
    struct fs_node_t *filenode = NULL;
    struct cached_page_t *buf = NULL;
    struct mount_info_t *dinfo;
    pid_t oldtid;
    int followlink = !(flags & AT_SYMLINK_NOFOLLOW);
    
    // init exec is a special case as path is in kernel space not user space.
    // we also pass the OPEN_CREATE_DENTRY flag so that vfs_open_internal()
    // creates a dentry we can use with e.g. when reading /proc/[pid]/maps
	int open_flags = 
	    (((this_core->cur_task == init_task) || !this_core->cur_task->user) ?
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
    if((res = get_exec_filenode(dirfd, path, open_flags, &filenode)) < 0)
    {
        kfree(auxv);
        return res;
	}

    // read executable header
    if(!(buf = get_cached_page(filenode, 0, 0)))
    {
        release_node(filenode);
        kfree(auxv);
        return -EACCES;
	}

    // get a kernel stack (if we don't already have one)
    if(!this_core->cur_task->kstack_virt)
    {
        if(get_kstack((physical_addr *)&this_core->cur_task->kstack_phys, 
                      (virtual_addr *)&this_core->cur_task->kstack_virt) != 0)
        {
            res = -ENOMEM;
            goto die;
        }
    }
    
    // Count argv & envp and copy args & env to kernel memory.
    // We do this before freeing user space, after which we'll have no access
    // to user data!
    if((argc = count_args(argv, &new_argv, &arglen)) < 0)
    {
        res = argc;
        goto die;
    }
    
    if((envc = count_args(env, &new_env, &envlen)) < 0)
    {
        res = envc;
        goto die;
    }
    
    if((invkc = count_invk_args(invk, &new_invk, &invklen)) < 0)
    {
        res = invkc;
        goto die;
    }

    // Check if this is an executable script by looking for a shebang.
    // Such scripts begin with a line like:
    //    #!interpreter [optional-arg]
    if(((char *)buf->virt)[0] == '#' && ((char *)buf->virt)[1] == '!')
    {
        char **tmpargv, *interpargs[4];
        size_t len;

        if((res = 
                parse_interpreter_line((char *)buf->virt, 
                        (char *)buf->virt + buf->len, interpargs, 4)) == 0)
        {
            res = -EINVAL;
            goto die;
        }

        // Interpreter name must be an absolute path
        if(interpargs[0][0] != '/')
        {
            for(i = 0; i < 4; i++)
            {
                if(interpargs[i])
                {
                    kfree(interpargs[i]);
                }
            }

            res = -EINVAL;
            goto die;
        }

        // We need to make room for the interpreter name and the optional arg
        if(!(tmpargv = krealloc(new_argv, sizeof(char *) * (argc + res + 1))))
        {
            res = -ENOMEM;
            goto die;
        }

        new_argv = tmpargv;
        argc += res;

        // Adjust arglen
        for(i = 0; i < res; i++)
        {
            len = strlen(interpargs[i]) + 1;
            ALIGN_UP_TO_SIZET(len);
            arglen += len;
        }

        // Move args, including the NULL terminator, 1 or 2 places to the right,
        // depending on whether we have an optional arg (2 places) or not (1 place)
        for(i = argc; i >= res; i--)
        {
            new_argv[i] = new_argv[i - res];
        }

        // The interpreter will need the absolute pathname of the script, while
        // argv[0] is very likely to be relative
        kfree(new_argv[i + 1]);
        len = strlen(new_invk[0]);

        if(!(new_argv[i + 1] = malloced_copy(new_invk[0], len)))
        {
            res = -ENOMEM;
            goto die;
        }

        new_argv[i + 1][len] = '\0';

        // The new argv[0] will definitely be longer than the original, as
        // the new one is an absolute pathname, instead of a relative one.
        // We simply add the new length, ignoring the old one, which will
        // result in a engligible amount of wasted bytes
        len++;
        ALIGN_UP_TO_SIZET(len);
        arglen += len;

        // Now copy the interpreter arg(s)
        for( ; i >= 0; i--)
        {
            new_argv[i] = interpargs[i];
        }

        free_tmpmem(new_invk);
        invk[0] = new_argv[0];
        invk[1] = basename(invk[0]);

        if((invkc = count_invk_args(invk, &new_invk, &invklen)) < 0)
        {
            res = invkc;
            goto die;
        }

        release_cached_page(buf);
        release_node(filenode);
        buf = NULL;
        filenode = NULL;
        __asm__ __volatile("":::"memory");

        // get the executable's file node
        if((res = get_exec_filenode(AT_FDCWD, new_argv[0], 
                                        OPEN_KERNEL_CALLER |
                                        OPEN_FOLLOW_SYMLINK |
                                        OPEN_CREATE_DENTRY, &filenode)) < 0)
        {
            goto die;
    	}

        // read executable header
        if(!(buf = get_cached_page(filenode, 0, 0)))
        {
            res = -EACCES;
            goto die;
    	}
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
    res = other_threads_dead(this_core->cur_task);

    while(!res)
    {
        scheduler();
        res = other_threads_dead(this_core->cur_task);
    }

    this_core->cur_task->ldt.base = 0;
    this_core->cur_task->ldt.limit = 0xFFFFFFFF;

#ifdef __x86_64__

    wrmsr(IA32_FS_BASE, 0);

#else

    // 0x30 - DATA Descriptor for TLS
    gdt_add_descriptor(GDT_TLS_DESCRIPTOR, 0, 0xFFFFFFFF, 0xF2);

#endif

    /* if there are other threads, they should be zombies, so reap them */
    //reap_dead_threads(cur_task);

    /* set thread group's exit status */
    this_core->cur_task->threads->thread_group_leader = (struct task_t *)this_core->cur_task;
    this_core->cur_task->threads->thread_count = 1;
    this_core->cur_task->thread_group_next = NULL;
    __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_DYNAMICALLY_LOADED);

    /*
     * Reset the task's tid.
     */
    int found = 0;
    oldtid = this_core->cur_task->pid;

    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(*t && *t != this_core->cur_task && (*t)->pid == this_core->cur_task->pid)
        {
            found = 1;
            break;
        }
    }
    
    elevated_priority_unlock(&task_table_lock);

    if(!found && this_core->cur_task->pid != tgid(this_core->cur_task))
    {
        this_core->cur_task->pid = tgid(this_core->cur_task);
    }

    set_task_comm((struct task_t *)this_core->cur_task, invk[1]);

    disarm_timers(tgid(this_core->cur_task));
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
    //       but not the memory region structs.

    kernel_mutex_lock(&(this_core->cur_task->mem->mutex));

    memregion_detach_user((struct task_t *)this_core->cur_task, 0);

    if(this_core->cur_task->properties & PROPERTY_VFORK)
    {
        // if this task was vforked, it used the parent's page directory
        // and now it needs its own, so clone the idle task's page directory
        if(clone_task_pd((struct task_t *)get_idle_task(), 
                         (struct task_t *)this_core->cur_task) != 0)
        {
            kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));
            res = 0;
            goto die;
        }

        // now load the new page directory
        vmmngr_switch_pdirectory((pdirectory *)this_core->cur_task->pd_phys,
                                 (pdirectory *)this_core->cur_task->pd_virt);
    }
    else
    {
        free_user_pages(this_core->cur_task->pd_virt);
    }

    kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));

    // Load ELF file sections to memory
    if((res = elf_load_file(filenode, buf, auxv, ELF_FLAG_NONE)) != 0)
    {
        res = 0;
        goto die;
    }
    
    this_core->cur_task->exe_dev = filenode->dev;
    this_core->cur_task->exe_inode = filenode->inode;

    // change task's permissions if executable is suid and:
    //    - the underlying filesystem is not mounted nosuid
    //    - the calling process is not being ptraced

    if((dinfo = get_mount_info(filenode->dev)) &&
       !(dinfo->flags & MS_NOSUID) &&
       !(this_core->cur_task->properties & PROPERTY_TRACE_SIGNALS))
    {
        if((filenode->mode & S_ISUID) == S_ISUID)
        {
            this_core->cur_task->euid = filenode->uid;
        }

        if((filenode->mode & S_ISGID) == S_ISGID)
        {
            this_core->cur_task->egid = filenode->gid;
        }
    }


    release_cached_page(buf);
    release_node(filenode);
    
    // bootstrap the new process's stack
    argp = copy_strs(argc, new_argv, STACK_START, arglen,
                     (this_core->cur_task->properties & PROPERTY_DYNAMICALLY_LOADED));
    free_tmpmem(new_argv);
    
    this_core->cur_task->arg_start = (void *)(STACK_START - arglen);
    this_core->cur_task->arg_end = (void *)STACK_START;

    if(!argp)
    {
        kfree(auxv);
        syscall_exit(-1);
    }
    
    if(this_core->cur_task->properties & PROPERTY_DYNAMICALLY_LOADED)
    {
        argc++;
    }
    
    envp = copy_strs(envc, new_env, argp, envlen, 0);
    free_tmpmem((char **)new_env);
    
    this_core->cur_task->env_start = (void *)(argp - envlen);
    this_core->cur_task->env_end = (void *)argp;

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
    this_core->cur_task->execve.rdi = argc;
    this_core->cur_task->execve.rsi = argp;
    this_core->cur_task->execve.rdx = envp;
    this_core->cur_task->execve.r8 = invkp;

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
    if(memregion_alloc_and_attach((struct task_t *)this_core->cur_task, 
                                  NULL, 0, 0,
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
        if(this_core->cur_task->sig->signal_actions[i].sa_handler == SIG_IGN)
        {
            continue;
        }
        
        ksigemptyset(&this_core->cur_task->sig->signal_actions[i].sa_mask);
        this_core->cur_task->sig->signal_actions[i].sa_handler = SIG_DFL;
        this_core->cur_task->sig->signal_actions[i].sa_cookie = NULL;
        this_core->cur_task->sig->signal_actions[i].sa_flags = 0;
    }
    
    ksigemptyset((sigset_t *)&this_core->cur_task->signal_pending);
    ksigemptyset((sigset_t *)&this_core->cur_task->signal_caught);
    //ksigemptyset((sigset_t *)&this_core->cur_task->signal_mask);
    ksigemptyset((sigset_t *)&this_core->cur_task->signal_timer);
    this_core->cur_task->woke_by_signal = 0;
    //this_core->cur_task->sigreturn = 0;
    A_memset((void *)&this_core->cur_task->signal_stack, 0, sizeof(stack_t));

    // close open files that are marked close-on-exec
    for(i = 0; i < NR_OPEN; i++)
    {
        if(is_cloexec(this_core->cur_task, i))
        {
            syscall_close(i);
        }
    }

    this_core->cur_task->cloexec = 0;

    this_core->cur_task->end_stack = (uintptr_t)stack;

#ifdef __x86_64__
    this_core->cur_task->execve.rip = eip;
    this_core->cur_task->execve.rbp = (uint64_t)stack;
    this_core->cur_task->execve.rsp = (uint64_t)stack;
#else
    this_core->cur_task->execve.eip = eip;
    this_core->cur_task->execve.ebp = (uint32_t)stack;
    this_core->cur_task->execve.esp = (uint32_t)stack;
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
    if(this_core->cur_task->properties & PROPERTY_VFORK)
    {
        __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_VFORK);

        if(this_core->cur_task->parent->state == TASK_WAITING)
        {
            unblock_task(this_core->cur_task->parent);
        }
    }


#ifndef __x86_64__
    forget_fpu(this_core->cur_task);
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
    if((this_core->cur_task->properties & PROPERTY_TRACE_SIGNALS) &&
       (this_core->cur_task->ptrace_options & PTRACE_O_TRACEEXEC))
    {
        this_core->cur_task->ptrace_eventmsg = oldtid;
        ptrace_signal(SIGTRAP, PTRACE_EVENT_EXEC);
    }

    enter_user();
    for(;;) ;

    return 0;

die:

    if(new_invk)
    {
        free_tmpmem(new_invk);
    }

    if(new_argv)
    {
        free_tmpmem(new_argv);
    }

    if(new_env)
    {
        free_tmpmem(new_env);
    }

    if(filenode)
    {
        release_node(filenode);
    }

    if(buf)
    {
        release_cached_page(buf);
    }

    if(auxv)
    {
        kfree(auxv);
    }

    if(res)
    {
        return res;
    }
    else
    {
        syscall_exit(-1);
        return -ENOSYS;     // keep gcc happy
    }
}

