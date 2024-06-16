/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: procfs_task_stat.c
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
 *  \file procfs_task_stat.c
 *
 *  This file implements some procfs filesystem functions (mainly the ones
 *  used to read task status files, i.e. /proc/[pid]/stat, /proc/[pid]/statm
 *  and /proc/[pid]/status, where pid is a process id).
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref procfs_ops structure, which is defined in procfs.c.
 */

//#define __DEBUG

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#include <errno.h>
#include <string.h>
//#include <sys/sched.h>
#include <sched.h>
#include <kernel/task.h>
#include <kernel/thread.h>
#include <kernel/ksignal.h>
#include <kernel/tty.h>
#include <kernel/asm.h>         // get_eip() & get_esp()
#include <mm/kheap.h>
#include <fs/procfs.h>

#include "../kernel/tty_inlines.h"


/* task states (short version) */
char task_state_chr[] =
{
    '-',
    'R',
    'R',
    'D',
    'S',
    'Z',
    'R',
    'T',
    '-',
};

/* task states (long version) */
char *task_state_str[] =
{
    "Invalid",
    "Running",
    "Ready",
    "Waiting",
    "Sleeping",
    "Zombie",
    "Idle",
    "Stopped",
    "Dying",
};

#define BUF_SPRINTF(fmt, ...)                           \
do {                                                    \
    ksprintf((char *)buf, 1024, fmt, __VA_ARGS__);      \
    len = strlen((char *)buf);                          \
    buf += len;                                         \
    buflen += len;                                      \
} while(0)

#define PAGE_TO_KB(p)                   (((p) * PAGE_SIZE) / 1024)


/*
 * Read /proc/[pid]/stat.
 */
size_t get_task_stat(struct task_t *task, char **_buf)
{
    if(!task || !_buf)
    {
        return 0;
    }

    char state = task_state_chr[task->state];
    dev_t ctty = 0;
    dev_t maj = 0, min = 0;
    pid_t tpgid = 0, ppid = 0, tgid = get_tgid(task);
    int threads = 0;
    size_t rss = 0;
    int prio = (task->sched_policy == SCHED_OTHER) ? 0 : task->priority;
    unsigned long pending = sigset_to_ulong((sigset_t *)&task->signal_pending);
    unsigned long blocked = sigset_to_ulong(&task->signal_mask);
    unsigned long sigignore = get_ignored_task_signals(task);
    unsigned long sigcatch = sigset_to_ulong(&task->signal_caught);
    volatile size_t buflen = 0;
    volatile char *buf;
    size_t len = 0;

    *_buf = NULL;
    PR_MALLOC(buf, 1024);
    *_buf = (char *)buf;

    if(task->threads)
    {
        threads = task->threads->thread_count;
    }
    else
    {
        threads = 1;
    }
    
    if(task->ctty <= 0)
    {
        ctty = 0;
        tpgid = 0;
    }
    else
    {
        ctty = task->ctty;
        
        struct tty_t *tty = get_struct_tty(ctty);
        
        if(tty)
        {
            tpgid = tty->pgid;
        }
        else
        {
            tpgid = 0;
        }
    }
    
    /*
     * tty number should be reported as follows:
     *    bits 0-7: low byte of minor dev number
     *    bits 8-15: major dev number
     *    bits 20-31: high byte of minor dev number
     *
     * See: https://man7.org/linux/man-pages/man5/proc.5.html
     */
    maj = MAJOR(ctty);
    min = MINOR(ctty);
    ctty = (min & 0xff) | ((maj & 0xff) << 8) | ((min & 0xff00) << 16);

    rss = get_task_pagecount(task) /* * PAGE_SIZE */;
    
    if(task->parent)
    {
        ppid = task->parent->pid;
    }
    
    /*
     * Print out the file's contents.
     * TODO: some of the numbers reported below are not accurate.
     *
     * For details, see: https://man7.org/linux/man-pages/man5/proc.5.html
     */

    BUF_SPRINTF("%d (%s) ", tgid, task->command);
    //BUF_SPRINTF("%d %d ", task->pid, task->tid);
    BUF_SPRINTF("%d ", task->pid);
    BUF_SPRINTF("%c %d ", state, ppid);
    BUF_SPRINTF("%d %d ", task->pgid, task->sid);
    BUF_SPRINTF("%lu %d %u ", ctty, tpgid, task->properties);

    BUF_SPRINTF("%lu %lu %lu %lu ",
                task->minflt, task->children_minflt,
                task->majflt, task->children_majflt);

    BUF_SPRINTF("%lu %lu %lu %lu ",
                task->user_time, task->sys_time,
                task->children_user_time, task->children_sys_time);

    BUF_SPRINTF("%d %d ", task->priority, task->nice ? (20 - task->nice) : 0);
    BUF_SPRINTF("%d %d ", threads, 0);

#ifdef __x86_64__
    BUF_SPRINTF("%lu %lu ", (long unsigned int)task->start_time,
                            KERNEL_MEM_END);
#else
    BUF_SPRINTF("%lu %u ", (long unsigned int)task->start_time,
                            KERNEL_MEM_END);
    //BUF_SPRINTF("%llu %u ", task->start_time, KERNEL_MEM_END);
#endif      /* !__x86_64__ */

    BUF_SPRINTF("%lu %lu ", rss, task->task_rlimits[RLIMIT_RSS].rlim_cur);

#ifdef __x86_64__
    BUF_SPRINTF("%lu %lu %lu ",
            task_get_code_start(task), task_get_code_end(task), STACK_START);
#else
    BUF_SPRINTF("%lu %lu %u ",
            //task->base_addr, task->end_code, KERNEL_MEM_START);
            task_get_code_start(task), task_get_code_end(task), STACK_START);
#endif      /* !__x86_64__ */

    //BUF_SPRINTF("%u %u ", task->r.esp, task->r.eip);
    
#ifdef __x86_64__
    if(task == get_cur_task())
    {
        BUF_SPRINTF("%lu %lu ", get_rsp(), get_rip());
    }
    else
    {
        BUF_SPRINTF("%lu %lu ", task->saved_context.rsp,
                                task->saved_context.rip);
    }
#else
    if(task == get_cur_task())
    {
        BUF_SPRINTF("%lu %lu ", get_esp(), get_eip());
    }
    else
    {
        BUF_SPRINTF("%u %u ", task->saved_context.esp,
                              task->saved_context.eip);
    }
#endif      /* !__x86_64__ */

    BUF_SPRINTF("%lu %lu %lu %lu ", pending, blocked, sigignore, sigcatch);

    BUF_SPRINTF("%lu %lu %lu ",
            (long unsigned int)((task->state == TASK_SLEEPING) ?
                                task->wait_channel : 0),
            (long unsigned int)0, (long unsigned int)0);

    BUF_SPRINTF("%d %d %u %u ", 0, 0, prio, task->sched_policy);

    BUF_SPRINTF("%lu %lu %ld ",
            (long unsigned int)0,
            (long unsigned int)0, (long unsigned int)0);

    BUF_SPRINTF("%lu %lu %lu ",
            task_get_data_start(task),
            task_get_data_end(task),
            task_get_data_end(task));

    BUF_SPRINTF("%lu %lu ",
            (long unsigned int)task->arg_start,
            (long unsigned int)task->arg_end);

#ifdef __x86_64__
    BUF_SPRINTF("%lu %lu %u\n",
            (long unsigned int)task->env_start,
            (long unsigned int)task->env_end,
            task->exit_status);
#else
    BUF_SPRINTF("%lu %lu %lu\n",
            (long unsigned int)task->env_start,
            (long unsigned int)task->env_end,
            task->exit_status);
#endif      /* !__x86_64__ */

    return buflen;
}


/*
 * Read /proc/[pid]/statm.
 */
size_t get_task_statm(struct task_t *task, char **buf)
{
    if(!task || !buf)
    {
        return 0;
    }

    size_t rss = get_task_pagecount(task);
    size_t shared = memregion_shared_pagecount(task);
    size_t text = memregion_text_pagecount(task);
    size_t data = memregion_data_pagecount(task) +
                  memregion_stack_pagecount(task);

    PR_MALLOC(*buf, 256);
    
    ksprintf(*buf, 256, "%lu %lu %lu %lu %lu %lu %lu\n",
            task->image_size, rss, shared, text, (size_t)0, data, (size_t)0);

    return strlen(*buf);
}


/*
 * Read /proc/[pid]/status.
 */
size_t get_task_status(struct task_t *task, char **_buf)
{
    if(!task || !_buf)
    {
        return 0;
    }

    ssize_t buflen = 0;
    size_t len = 0;
    int i;
    int threads = 0;
    char *buf;
    unsigned long pending = sigset_to_ulong((sigset_t *)&task->signal_pending);
    unsigned long blocked = sigset_to_ulong(&task->signal_mask);
    unsigned long sigignore = get_ignored_task_signals(task);
    unsigned long sigcatch = sigset_to_ulong(&task->signal_caught);

    *_buf = NULL;
    PR_MALLOC(buf, 1024);
    *_buf = buf;

    ksprintf(buf, 1024, "Name:   %s\n", task->command);
    len = strlen(buf);
    buf += len;
    buflen += len;
    
    BUF_SPRINTF("Umask:  %04o\n", task->fs ? task->fs->umask : 0);
    BUF_SPRINTF("State:  %c (%s)\n",
                task_state_chr[task->state], task_state_str[task->state]);
    //BUF_SPRINTF("Tid:    %d\n", task->tid);
    BUF_SPRINTF("Pid:    %d\n", task->pid);
    BUF_SPRINTF("Tgid:   %d\n", get_tgid(task) /* task->pid */);
    BUF_SPRINTF("Pgid:   %d\n", task->pgid);
    BUF_SPRINTF("PPid:   %d\n", task->parent ? task->parent->pid : 0);

    if(task->tracer_pid)
    {
        BUF_SPRINTF("TracerPid: %d\n", task->tracer_pid);

        /*
        struct task_t *tracer = get_task_by_tid(task->tracer_pid);
        
        if(tracer)
        {
            BUF_SPRINTF("TracerPid: %d\n", tracer->pid);
        }
        else
        {
            BUF_SPRINTF("TracerPid: %d\n", 0);
        }
        */
    }
    else
    {
        BUF_SPRINTF("TracerPid: %d\n", 0);
    }

    BUF_SPRINTF("Uid:    %u\t%u\t%u\n", task->uid, task->euid, task->ssuid);
    BUF_SPRINTF("Gid:    %u\t%u\t%u\n", task->gid, task->egid, task->ssgid);
    BUF_SPRINTF("FDSize: %d\n", task->ofiles ? NR_OPEN : 0);

    strcpy(buf, "Groups: ");
    buf += 8;
    buflen += 8;

    for(i = 0; i < NGROUPS_MAX; i++)
    {
        if(task->extra_groups[i] != (gid_t)-1)
        {
            BUF_SPRINTF("%u ", task->extra_groups[i]);
        }
    }

    BUF_SPRINTF("\nVmSize:    %8ld kB\n", PAGE_TO_KB(task->image_size));
    BUF_SPRINTF("VmRSS:     %8ld kB\n", PAGE_TO_KB(get_task_pagecount(task)));
    BUF_SPRINTF("RssAnon:   %8ld kB\n",
                    PAGE_TO_KB(memregion_anon_pagecount(task)));
    BUF_SPRINTF("RssFile:   %8ld kB\n",
                    PAGE_TO_KB(memregion_shared_pagecount(task)));
    BUF_SPRINTF("VmData:    %8ld kB\n",
                    PAGE_TO_KB(memregion_data_pagecount(task)));
    BUF_SPRINTF("VmStk:     %8ld kB\n",
                    PAGE_TO_KB(memregion_stack_pagecount(task)));
    BUF_SPRINTF("VmExe:     %8ld kB\n",
                    PAGE_TO_KB(memregion_text_pagecount(task)));

    // TODO: fix when we implement shared libs
    BUF_SPRINTF("VmLib:     %8ld kB\n", (long)0);
    
    // TODO: fix when we implement swapping
    BUF_SPRINTF("VmSwap:    %8ld kB\n", (long)0);
    
    // TODO: fix when we implement core dumping
    BUF_SPRINTF("CoreDumping:  %d\n", 0);

    if(task->threads)
    {
        threads = task->threads->thread_count;
    }

    BUF_SPRINTF("Threads:  %d\n", threads);
    BUF_SPRINTF("SigPnd:   %016lx\n", pending);
    BUF_SPRINTF("SigBlk:   %016lx\n", blocked);
    BUF_SPRINTF("SigIgn:   %016lx\n", sigignore);
    BUF_SPRINTF("SigCgt:   %016lx\n", sigcatch);

    return buflen;
}

