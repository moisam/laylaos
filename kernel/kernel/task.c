/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: task.c
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
 *  \file task.c
 *
 *  The kernel tasking implementation.
 */

//#define __DEBUG

#define __USE_XOPEN_EXTENDED

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <kernel/pic.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/clock.h>
#include <kernel/asm.h>
#include <kernel/ksignal.h>
#include <kernel/syscall.h>
#include <kernel/ptrace.h>
#include <kernel/tty.h>
#include <kernel/ipc.h>
#include <kernel/ksigset.h>
#include <kernel/fpu.h>
#include <kernel/msr.h>
#include <kernel/reboot.h>
#include <mm/kheap.h>
#include <mm/kstack.h>
#include <mm/mmap.h>
#include <fs/procfs.h>
#include <gui/vbe.h>
#include <sys/list.h>

#include "task_funcs.c"

#include "tty_inlines.h"

static struct task_t *task_alloc_internal(int alloc_vm_struct);
static inline struct task_t *get_next_runnable(void);

/* next pid for creating new tasks */
pid_t next_pid = 0;

/* pointers to current, idle and init tasks */
struct task_t *cur_task = 0;
struct task_t *idle_task = 0;
struct task_t *init_task = 0;

/* task run queues and master task table */
#define NR_QUEUE                100

struct task_queue_t ready_queue[NR_QUEUE];
struct task_queue_t blocked_queue;
struct task_queue_t zombie_queue;

struct kernel_mutex_t task_table_lock;
struct task_t *task_table[NR_TASKS];
static struct task_t task_free_cache;
int total_tasks = 0;

struct task_t placeholder_task;

int user_has_ready_tasks = 0;
int rr_has_ready_tasks = 0;
int fifo_has_ready_tasks = 0;


/*
 * Initialise tasking.
 */
void tasking_init(void)
{
    A_memset(&ready_queue, 0, sizeof(ready_queue));
    A_memset(&blocked_queue, 0, sizeof(blocked_queue));
    A_memset(&zombie_queue, 0, sizeof(zombie_queue));
    A_memset((void *)task_table, 0, sizeof(struct task_t *) * NR_TASKS);
    A_memset(&placeholder_task, 0, sizeof(struct task_t));

    for(int i = 0; i < NR_QUEUE; i++)
    {
        ready_queue[i].head.next = &ready_queue[i].head;
        ready_queue[i].head.prev = &ready_queue[i].head;
    }

    blocked_queue.head.next = &blocked_queue.head;
    blocked_queue.head.prev = &blocked_queue.head;
    zombie_queue.head.next = &zombie_queue.head;
    zombie_queue.head.prev = &zombie_queue.head;
  
    cli();
    
    cur_task = task_alloc_internal(1);

    task_table[0] = cur_task;
    idle_task = task_table[0];
    total_tasks = 1;

    prev_ticks = ticks;

    // switch our early timer to the proper tasking timer
    switch_timer();
    
    sti();

    A_memset(&task_free_cache, 0, sizeof(struct task_t));
    cur_task->pid = next_pid;
    cur_task->pgid = cur_task->pid;
    cur_task->pd_virt = (virtual_addr)vmmngr_get_directory_virt();
    cur_task->pd_phys = (physical_addr)vmmngr_get_directory_phys();
    //cur_task->sched_policy = SCHED_IDLE;
    cur_task->priority = 0;
    cur_task->sched_policy = SCHED_OTHER;
    cur_task->timeslice = 1;
    cur_task->time_left = 1;
    cur_task->fs->umask = 022;
    cur_task->fs->root = system_root_node;
    cur_task->fs->cwd = system_root_node;
    cur_task->ctty = 0;
    set_task_comm(cur_task, "idle");

    init_kernel_mutex(&task_table_lock);

    init_kernel_mutex(&cur_task->ofiles->mutex);
    init_kernel_mutex(&cur_task->fs->mutex);
    init_kernel_mutex(&cur_task->threads->mutex);
    init_kernel_mutex(&cur_task->mem->mutex);

    // init idle task's thread info
    cur_task->threads->thread_group_leader = cur_task;
    cur_task->threads->thread_count = 1;
    cur_task->threads->tgid = next_pid;
    cur_task->thread_group_next = NULL;

    cur_task->ldt.base = 0;
    cur_task->ldt.limit = 0xFFFFFFFF;

    // init idle task's memory map
    memregion_alloc_and_attach(cur_task, NULL, 0, 0,
                               KERNEL_MEM_START, KERNEL_MEM_END,
                               PROT_NONE, MEMREGION_TYPE_KERNEL, 
                               MAP_SHARED, 0);

    // set all supplementary groups to -1 (as 0 is the root user's gid)
    for(int i = 0; i < NGROUPS_MAX; i++)
    {
        cur_task->extra_groups[i] = (gid_t)-1;
    }

    if(get_kstack(&cur_task->kstack_phys, &cur_task->kstack_virt) != 0)
    {
        kpanic("Failed to get initial kstack!\n");
    }

    cur_task->state = TASK_RUNNING;
    
    set_task_rlimits(cur_task);

    init_signals();
}


volatile int IRQ_disable_counter = 0;

#ifndef __x86_64__

/*
 * The following 2 functions are taken from:
 *    https://wiki.osdev.org/Brendan%27s_Multi-tasking_Tutorial
 */

#pragma GCC push_options
#pragma GCC optimize("O0")
 
void lock_scheduler(void)
{
#ifndef SMP
    //cli();
    __asm__ __volatile__ ("cli");
    IRQ_disable_counter++;
#endif
}

void unlock_scheduler(void)
{
#ifndef SMP
    IRQ_disable_counter--;

    if(IRQ_disable_counter == 0)
    {
        //sti();
        __asm__ __volatile__ ("sti");
    }
#endif
}

#pragma GCC pop_options

#endif      /* __x86_64__ */


/*
 * The scheduler.
 */
void scheduler(void)
{
	register struct task_t *t = cur_task;

    if(t->state == TASK_RUNNING)
    {
        t->state = TASK_READY;
        
        /*
         * A running SCHED_FIFO thread that has been preempted by another 
         * thread of higher priority will stay at the head of the list for
         * its priority and will resume execution as soon as all threads of
         * higher priority are blocked again.
         *
         * If a SCHED_RR thread has been running for a time period
         * equal to or longer than the time quantum, it will be put at the
         * end of the list for its priority.  A SCHED_RR thread that has
         * been preempted by a higher priority thread and subsequently
         * resumes execution as a running thread will complete the unexpired
         * portion of its round-robin time quantum.  The length of the time
         * quantum can be retrieved using sched_rr_get_interval(2).
         */
        if(t->sched_policy == SCHED_RR)
        {
            if(t->time_left <= 0)
            {
                move_to_queue_end(t);
                reset_task_timeslice(t);
            }
        }
        else if(t->sched_policy == SCHED_OTHER)
        {
            if(t->next)
            {
                move_to_queue_end(t);
            }

            reset_task_timeslice(t);
        }
    }
    else if((t->state == TASK_ZOMBIE) &&
            (t->properties & PROPERTY_FINISHING))
    {
        // task is dying, let it finish
    	return;
    }


    if(t->state != TASK_ZOMBIE)
    {
        update_task_times(t);
    }

    
    /* volatile */ struct task_t *next = get_next_runnable();

    next->state = TASK_RUNNING;
    
    if(next != t)
    {
        system_context_switches++;

#ifdef __x86_64__
        fpu_state_save(t);
#endif

    	// return value of 1 means we came back from a context switch
    	if(save_context(t) == 1)
    	{

#ifdef __x86_64__
            fpu_state_restore(cur_task);
#endif

            return;
    	}

        // 0x30 - DATA Descriptor for TLS
#ifdef __x86_64__
        wrmsr(IA32_FS_BASE, next->ldt.base);
#else
        gdt_add_descriptor(GDT_TLS_DESCRIPTOR, 
                            next->ldt.base, next->ldt.limit, 0xF2);
#endif

    	restore_context((struct task_t *)next);
    }
}


struct task_t *get_cur_task(void)
{
    return cur_task;
}

struct task_t *get_init_task(void)
{
    return init_task;
}

struct task_t *get_idle_task(void)
{
    return idle_task;
}

void set_cur_task(struct task_t *task)
{
    cur_task = task;
}

void set_init_task(struct task_t *task)
{
    static int init_set = 0;
    
    if(init_set)
    {
        kpanic("Trying to re-set init task!");
    }
    
    init_set = 1;
    init_task = task;
}


/*
 * Set task command field.
 */
void set_task_comm(struct task_t *task, char *command)
{
    if(strlen(command) < TASK_COMM_LEN)
    {
        strcpy(task->command, command);
    }
    else
    {
        //strncpy(task->command, command, TASK_COMM_LEN - 1);
        memcpy(task->command, command, TASK_COMM_LEN - 1);
        task->command[TASK_COMM_LEN - 1] = '\0';
    }
}


virtual_addr task_get_xxx_end(struct task_t *task, int type)
{
    virtual_addr res = 0;
    struct memregion_t *tmp;
    
    for(tmp = task->mem->first_region; tmp != NULL; tmp = tmp->next)
    {
        if(tmp->type == type)
        {
            virtual_addr start = tmp->addr;
            virtual_addr end = start + (tmp->size * PAGE_SIZE);
            
            if(end > res)
            {
                res = end;
            }
        }
    }
    
    return res;
}


virtual_addr task_get_code_end(struct task_t *task)
{
    return task_get_xxx_end(task, MEMREGION_TYPE_TEXT);
}


virtual_addr task_get_data_end(struct task_t *task)
{
    return task_get_xxx_end(task, MEMREGION_TYPE_DATA);
}


virtual_addr task_get_xxx_start(struct task_t *task, int type)
{
    virtual_addr res = -1;
    struct memregion_t *tmp;
    
    for(tmp = task->mem->first_region; tmp != NULL; tmp = tmp->next)
    {
        if(tmp->type == type)
        {
            virtual_addr start = tmp->addr;
            virtual_addr end = start + (tmp->size * PAGE_SIZE);
            
            if(end < res)
            {
                res = end;
            }
        }
    }
    
    return res;
}


virtual_addr task_get_code_start(struct task_t *task)
{
    return task_get_xxx_start(task, MEMREGION_TYPE_TEXT);
}


virtual_addr task_get_data_start(struct task_t *task)
{
    return task_get_xxx_start(task, MEMREGION_TYPE_DATA);
}


static struct task_t *task_alloc_internal(int alloc_vm_struct)
{
    struct task_t *new_task = kmalloc(sizeof(struct task_t));

    if(!new_task)
    {
        return NULL;
    }
        
    A_memset(new_task, 0, sizeof(struct task_t));
    
#define kmalloc_struct(st)      (struct st *)kmalloc(sizeof(struct st))

    if(alloc_vm_struct && !(new_task->mem = kmalloc_struct(task_vm_t)))
    {
        kfree(new_task);
        return NULL;
    }
    
    if(!(new_task->ofiles = kmalloc_struct(task_files_t)) ||
       !(new_task->fs = kmalloc_struct(task_fs_t)) ||
       !(new_task->sig = kmalloc_struct(task_sig_t)) ||
       !(new_task->threads = kmalloc_struct(task_threads_t)) ||
       !(new_task->common = kmalloc_struct(task_common_t)))
    {
        kfree(new_task->ofiles);
        kfree(new_task->fs);
        kfree(new_task->sig);
        kfree(new_task->threads);
        kfree(new_task->mem);
        kfree(new_task->common);
        kfree(new_task);
        return NULL;
    }

    A_memset(new_task->ofiles, 0, sizeof(struct task_files_t));
    A_memset(new_task->fs, 0, sizeof(struct task_fs_t));
    A_memset(new_task->sig, 0, sizeof(struct task_sig_t));
    A_memset(new_task->threads, 0, sizeof(struct task_threads_t));
    A_memset(new_task->common, 0, sizeof(struct task_common_t));
    
    if(alloc_vm_struct)
    {
        A_memset(new_task->mem, 0, sizeof(struct task_vm_t));
    }
    
    return new_task;
}


/*
 * Allocate a new task struct.
 *
 * As this function is only called during fork/clone, we don't bother 
 * allocating memory for the new task's vm struct, as fork/clone will
 * free the vm struct and make a copy (fork) or use the parent's vm
 * struct (clone).
 */
struct task_t *task_alloc(void)
{
    if(total_tasks >= NR_TASKS)
    {
        return NULL;
    }

retry:
    next_pid++;
    
    /* account for pid roundup */
    if(next_pid < 0)
    {
        next_pid = 1;
    }

    /* find an empty slot in the task table */
    int i;
    
    elevated_priority_lock(&task_table_lock);

    for(i = 0; i < NR_TASKS; i++)
    {
        /* account for reused pids */
        if(task_table[i] && task_table[i]->pid == next_pid)
        {
            elevated_priority_unlock(&task_table_lock);
            goto retry;
        }
    }

    for(i = 1; i < NR_TASKS; i++)
    {
        if(task_table[i] == NULL)
        {
            break;
        }
    }
    
    if(i == NR_TASKS)
    {
        elevated_priority_unlock(&task_table_lock);
        return NULL;
    }

    // mark as used so we can unlock the table
    task_table[i] = &placeholder_task;
    elevated_priority_unlock(&task_table_lock);
        
    /* try to find a previously allocated task struct in the cache */
    struct task_t *new_task;

    kernel_mutex_lock(&task_free_cache.task_mutex);

    if(task_free_cache.next)
    {
        new_task = (struct task_t *)task_free_cache.next;
        task_free_cache.next = new_task->next;
        new_task->next = NULL;
        kernel_mutex_unlock(&task_free_cache.task_mutex);

        A_memset(new_task->ofiles, 0, sizeof(struct task_files_t));
        A_memset(new_task->fs, 0, sizeof(struct task_fs_t));
        A_memset(new_task->sig, 0, sizeof(struct task_sig_t));
        A_memset(new_task->threads, 0, sizeof(struct task_threads_t));
        A_memset(new_task->common, 0, sizeof(struct task_common_t));
        
        if(new_task->mem)
        {
            task_mem_free(new_task->mem);
            new_task->mem = NULL;
        }
        
        new_task->lock_held = NULL;
    }
    else
    {
        kernel_mutex_unlock(&task_free_cache.task_mutex);

        if(!(new_task = task_alloc_internal(0)))
        {
            task_table[i] = NULL;
            return NULL;
        }
    }

    task_table[i] = new_task;
    total_tasks++;

    new_task->pid = next_pid;

    return new_task;
}


/*
 * Free task struct.
 */
void task_free(struct task_t *task, int add_to_cache)
{
    int i;

    elevated_priority_lock(&task_table_lock);
    
    for(i = 0; i < NR_TASKS; i++)
    {
        if(task_table[i] == task)
        {
            task_table[i] = NULL;
            total_tasks--;
            break;
        }
    }

    elevated_priority_unlock(&task_table_lock);

    if(add_to_cache)
    {
        kernel_mutex_lock(&task_free_cache.task_mutex);
        task->next = task_free_cache.next;
        task_free_cache.next = task;
        kernel_mutex_unlock(&task_free_cache.task_mutex);
    }
    else
    {
        kfree(task);
    }
}


/*
 * Block task with timeout.
 */
int block_task2(void *wait_channel, int timeout_ticks)
{
	struct task_t *t = cur_task;
    
    if(timeout_ticks)
    {
        if(clock_wait(&waiter_head[0], t->pid, timeout_ticks, 0) == 0)
        {
            return EWOULDBLOCK;
        }
    }
    else
    {
        block_task(wait_channel, 1);
    }
    
	if(t->woke_by_signal /* && t->woke_by_signal != SIGCONT */)
	{
	    return EINTR;
	}
    
    return 0;
}


extern sigset_t unblockable_signals;    // signal.c

/*
 * Check if the given task has at least one pending deliverable signal.
 */
static inline int has_pending_signals(struct task_t *task)
{
    int signum;
    sigset_t permitted_signals;
    sigset_t deliverable_signals;

    if(!ksigisemptyset((sigset_t *)&task->signal_pending))
    {
        /* determine which signals are not blocked */
        ksigfillset(&permitted_signals);
        ksignotset(&permitted_signals, &task->signal_mask);
        ksigorset(&permitted_signals, &permitted_signals,
                    &unblockable_signals);

        /* determine which signals can be delivered */
        ksigandset(&deliverable_signals, &permitted_signals,
                    (sigset_t *)&task->signal_pending);

        for(signum = 1; signum < NSIG; signum++)
        {
            if(ksigismember(&deliverable_signals, signum))
            {
                return 1;
            }
        }
    }
    
    return 0;
}


/*
 * Block task.
 */
int block_task(void *wait_channel, int interruptible)
{
    lock_scheduler();

	struct task_t *t = cur_task;

    if(t->lock_held)
    {
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        kpanic("task sleeping with a held lock!\n");
    }

    t->wait_channel = wait_channel;
    t->state = interruptible ? TASK_SLEEPING : TASK_WAITING;

    remove_from_ready_queue(t);
    append_to_queue(t, &blocked_queue);

    if(interruptible)
    {
        if(!has_pending_signals(t))
        {
            t->woke_by_signal = 0;
            scheduler();
        }
        else
        {
            unblock_task(t);
        }

        unlock_scheduler();
        
        // if we came back, we either ignored or handled the signal
        return 1;
    }
    else
    {
        scheduler();
        unlock_scheduler();
        return 0;
    }
}


/*
 * Unblock tasks.
 */
void unblock_tasks(void *wait_channel)
{
    lock_scheduler();
    
    KDEBUG("unblock_tasks: 1 - pid 0x%x, wait_channel 0x%x\n", cur_task->pid, wait_channel);

	struct task_t *ct = cur_task;
    struct task_t *t = blocked_queue.head.next, *next;
    int runrun = 0;

    while(t != &blocked_queue.head)
    {
        next = (struct task_t *)t->next;

        if(t->wait_channel == wait_channel && t->state != TASK_ZOMBIE)
        {
            t->state = TASK_READY;
            t->wait_channel = NULL;

            if(t->priority > ct->priority)
            {
                runrun++;
            }

            KDEBUG("%s: pid %d\n", __func__, t->pid);
            remove_from_queue(t);

            /*
             * The sched (7) manpage says:
             *    When a blocked SCHED_FIFO thread becomes runnable, it will be
             *    inserted at the end of the list for its priority.
             */
            append_to_ready_queue(t);
        }

        t = next;
    }
    
    /*
     * The sched (7) manpage says:
     *    A running SCHED_FIFO thread that has been preempted by another
     *    thread of higher priority will stay at the head of the list
     *    for its priority and will resume execution as soon as all
     *    threads of higher priority are blocked again.
     */
    if(runrun)
    {
        scheduler();
    }
    
    unlock_scheduler();
}


/*
 * Unblock task but don't preempt.
 */
void unblock_task_no_preempt(struct task_t *task)
{
    lock_scheduler();
    
    if(task == NULL ||
       task->state == TASK_READY || task->state == TASK_RUNNING ||
       task->state == TASK_ZOMBIE)
    {
        // task is already unblocked or dead
        unlock_scheduler();
        return;
    }

    KDEBUG("unblock_task_no_preempt: pid %d\n", task->pid);

    task->state = TASK_READY;
    task->wait_channel = NULL;
    remove_from_queue(task);

    /*
     * The sched (7) manpage says:
     *    When a blocked SCHED_FIFO thread becomes runnable, it will be
     *    inserted at the end of the list for its priority.
     */
    append_to_ready_queue(task);

    unlock_scheduler();
}


/*
 * Unblock task.
 */
void unblock_task(struct task_t *task)
{
    lock_scheduler();
    
    if(task->state == TASK_READY || task->state == TASK_RUNNING ||
       task->state == TASK_ZOMBIE)
    {
        // task is already unblocked or dead
        unlock_scheduler();
        return;
    }

	register struct task_t *ct = cur_task;

    KDEBUG("unblock_task: pid %d\n", task->pid);

    task->state = TASK_READY;
    task->wait_channel = NULL;
    remove_from_queue(task);
    
    /*
     * The sched (7) manpage says:
     *    When a blocked SCHED_FIFO thread becomes runnable, it will be
     *    inserted at the end of the list for its priority.
     */
    append_to_ready_queue(task);

    /*
     * The sched (7) manpage says:
     *    A running SCHED_FIFO thread that has been preempted by another
     *    thread of higher priority will stay at the head of the list
     *    for its priority and will resume execution as soon as all
     *    threads of higher priority are blocked again.
     */
    if(task->priority > ct->priority)
    {
        scheduler();
    }

    unlock_scheduler();
}


static inline struct task_t *get_next_runnable(void)
{
    /* search queues, in turn, for a ready-to-run task */
    struct task_queue_t *queue;

    if(rr_has_ready_tasks)
    {
        for(queue = &ready_queue[MAX_RR_PRIO];
            queue >= &ready_queue[MIN_RR_PRIO]; queue--)
        {
            if(queue->head.next != &queue->head)
            {
                return queue->head.next;
            }
        }
    }

    if(fifo_has_ready_tasks)
    {
        for(queue = &ready_queue[MAX_FIFO_PRIO];
            queue >= &ready_queue[MIN_FIFO_PRIO]; queue--)
        {
            if(queue->head.next != &queue->head)
            {
                return queue->head.next;
            }
        }
    }

    if(ready_queue[0].head.next != &(ready_queue[0].head))
    {
        return ready_queue[0].head.next;
    }

    /* current task is the only runnable task? */
    if(cur_task->state == TASK_RUNNING || cur_task->state == TASK_READY)
    {
        return cur_task;
    }

    /* Running queues are empty? run idle task */
    return idle_task;
}


/*
 * Add child task to parent.
 */
void task_add_child(struct task_t *parent, struct task_t *child)
{
    if(!parent || !child)
    {
        return;
    }
    
    kernel_mutex_lock(&parent->task_mutex);
    
    child->first_sibling = NULL;
    child->parent = parent;

    if(parent->first_child == 0)
    {
        parent->first_child = child;
    }
    else
    {
        volatile struct task_t *sibling = parent->first_child;
    
        while(sibling->first_sibling)
        {
            sibling = sibling->first_sibling;
        }
        
        sibling->first_sibling = child;
    }

    parent->children++;
    kernel_mutex_unlock(&parent->task_mutex);
}


/*
 * Remove child task from parent.
 */
void task_remove_child(struct task_t *parent, struct task_t *child)
{
    if(!parent || !child)
    {
        return;
    }
    
    kernel_mutex_lock(&parent->task_mutex);
    volatile struct task_t *sibling = child->first_sibling;

    parent->children--;

    if(parent->first_child == child)
    {
        parent->first_child = sibling;
    }
    else
    {
        volatile struct task_t *tmp = parent->first_child;

        while(tmp->first_sibling != child)
        {
            tmp = tmp->first_sibling;
        }
        
        if(tmp)
        {
            tmp->first_sibling = sibling;
        }
    }

    kernel_mutex_unlock(&parent->task_mutex);
}


/*
 * Reap a zombie task.
 */
void reap_zombie(struct task_t *task)
{
    task_remove_child(task->parent, task);
    ptrace_clear_state(task);
    
    lock_scheduler();

    KDEBUG("%s: pid %d\n", __func__, task->pid);
    remove_from_queue(task);
    
    unlock_scheduler();

    /* free task kernel-stack memory */
    free_kstack(task->kstack_virt);

    if(task->mem)
    {
        /* free task page directory */
        free_pd(task->pd_virt);
        task->pd_virt = 0;
        task->pd_phys = 0;
    }

    KDEBUG("Done with Zombie task (%d)\n", task->pid);

    // if this was a thread, just free it's struct memory, otherwise add
    // the struct to our task struct cache
    if(!task->ofiles || !task->fs || !task->sig || 
       !task->threads || !task->common)
    {
        task_free(task, 0);
    }
    else
    {
        task_free(task, 1);
    }
    
    KDEBUG("Zombie done\n");
}


static void notify_tracer(struct task_t *t)
{
    /*
     * The ptrace manpage says:
     *    When the tracee calls _exit(2), it reports its death to its tracer.
     *    Other threads are not affected.
     *
     *    When any thread executes exit_group(2), every tracee in its thread
     *    group reports its death to its tracer.
     */
    if((t->properties & PROPERTY_TRACE_SIGNALS) && t->tracer_pid)
    {
        struct task_t *tracer = get_task_by_tid(t->tracer_pid);
        
        if(tracer)
        {
            int code = WCOREDUMP(t->exit_status) ? CLD_DUMPED :
                       (WIFSIGNALED(t->exit_status) ? CLD_KILLED : CLD_EXITED);

            siginfo_t siginfo = {
                         .si_code = code,
                         .si_pid = t->pid,
                         .si_uid = t->uid,
                         .si_status = t->exit_status,
                         .si_utime = t->user_time,
                         .si_stime = t->sys_time,
                        };

            add_task_signal(tracer, SIGCHLD, &siginfo, 1);
            //unblock_task_no_preempt(tracer);
        }
    }
}


static void zombify(struct task_t *t)
{
    t->properties |= PROPERTY_FINISHING;
    t->state = TASK_ZOMBIE;
    t->time_left = 0;

    lock_scheduler();
    remove_from_ready_queue(t);
    append_to_queue(t, &zombie_queue);
    unlock_scheduler();
}


static void zombie_loop(struct task_t *t)
{
    t->properties &= ~PROPERTY_FINISHING;

    for(;;)
    {
        KDEBUG("zombie_loop:\n");
        lock_scheduler();
        scheduler();
        unlock_scheduler();
    }
}


/*
 * Terminate task.
 */
void terminate_task(int code)
{
	struct task_t *t = cur_task;
	
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
	//printk("terminate_task: pid %d, code %d, comm %s\n", t->pid, code, t->command);

    if(t == init_task)
    {
        handle_init_exit(code);
    }
    
    if(t->lock_held)
    {
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        kpanic("task terminated with a held lock!\n");
    }

    /*
     * If exiting and PTRACE_O_TRACEEXIT is set, the ptrace manpage says:
     *    Stop the tracee at exit. A waitpid by the tracer will return a 
     *    status value such that
     *
     *       status>>8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))
     *
     *    The tracee's exit status can be retrieved with PTRACE_GETEVENTMSG.
     *
     *    The tracee is stopped early during process exit, when registers
     *    are still available, allowing the tracer to see where the exit
     *    occurred, whereas the normal exit notification is done after the
     *    process is finished exiting. Even though context is available, the
     *    tracer cannot prevent the exit from happening at this point.
     */
    if((t->properties & PROPERTY_TRACE_SIGNALS) &&
       (t->ptrace_options & PTRACE_O_TRACEEXIT))
    {
        t->ptrace_eventmsg = code;
        ptrace_signal(SIGTRAP, PTRACE_EVENT_EXIT);
    }

    t->exit_status = code;
    
    t->ldt.base = 0;
    t->ldt.limit = 0xFFFFFFFF;
    
    /*
     * Send SIGKILL to all tracees who have the PTRACE_O_EXITKILL option set.
     * Untrace the rest of tracees.
     */
    if(t->tracees)
    {
        ptrace_kill_tracees(t);
        list_free(t->tracees);
    }
    
    /* Cancel pending select() operations */
    task_cancel_select(t);


    /* 
     * if there are other threads and some of them are alive, just die.
     * the last thread to exit will orphanise our children, close open files,
     * signal our parent, and release filesystem nodes.
     */
    if(t->threads)
    {
        kernel_mutex_lock(&(t->threads->mutex));

        if(t->threads->thread_count > 1 && !other_threads_dead(t))
        {
        	KDEBUG("terminate_task: pid %d, tgid %d, threads %d\n", t->pid, tgid(t), t->threads->thread_count);

            struct task_t *tmp;

            //kernel_mutex_lock(&(t->threads->mutex));

            if(t->threads->thread_group_leader == t)
            {
                if(t->thread_group_next)
                {
                    t->threads->thread_group_leader = t->thread_group_next;
                }
            }
            else
            {
                for(tmp = t->threads->thread_group_leader;
                    tmp != NULL;
                    tmp = tmp->thread_group_next)
                {
                    if(tmp->thread_group_next == t)
                    {
                        tmp->thread_group_next = t->thread_group_next;
                        break;
                    }
                }
            }
        
            if(t->threads->thread_group_leader == NULL)
            {
                kpanic("NULL thread group leader struct\n");
            }
        
            t->threads->group_user_time += (t->user_time + t->children_user_time);
            t->threads->group_sys_time += (t->sys_time + t->children_sys_time);

            if(t->threads->thread_count == 1)
            {
                kpanic("Invalid thread count == 1\n");
            }

            t->threads->thread_count--;

            kernel_mutex_unlock(&(t->threads->mutex));

            // as we share some common structs with the other threads, remove
            // our references to them so they don't get overwritten when our
            // task struct is freed
            t->ofiles = NULL;
            t->fs = NULL;
            t->sig = NULL;
            t->threads = NULL;
            t->common = NULL;
            t->mem = NULL;

            zombify(t);
            notify_tracer(t);
            zombie_loop(t);
        }

        kernel_mutex_unlock(&(t->threads->mutex));
    }

    /* if there are other threads, they should be zombies, so reap them */
    reap_dead_threads(t);

    /* set thread group's exit status */
    if(t->threads)
    {
        t->threads->thread_group_leader->exit_status = code;
    }
    
    /* process accounting */
    task_account(t);

    /* release tty */
    set_controlling_tty(t->ctty, get_struct_tty(t->ctty), 0);

    /* undo unfinished semaphore operations */
    do_sem_undo(t);

    /* remove msg queues, semaphores & shmems */
    ipc_killall(t);
    
    /* disarm POSIX timers */
    disarm_timers(tgid(t));
    //disarm_timers(t);

    /*
     * free task memory pages, but keep the kernel stack and the page 
     * directory for now, as we'll need them to finish terminating the stack.
     * our parent will free them when it reaps the zombie task.
     * don't free pages when calling memregion_detach_user(), as
     * free_user_pages() will do it below.
     */
    memregion_detach_user(t, 0);
    free_user_pages(t->pd_virt);

    /* orphanize our poor children :( */
    struct task_t *child = (struct task_t *)t->first_child;

    while(child)
    {
        struct task_t *next = (struct task_t *)child->first_sibling;
        
        if(child->state == TASK_ZOMBIE)
        {
            reap_zombie(child);
        }
        else
        {
            task_add_child(init_task, child);
        }
        
        child = next;
    }
    
    t->first_child = NULL;
    
    /* close open files */
    int i;
    for(i = 0; i < NR_OPEN; i++)
    {
        /* NOTE: this call takes care of releasing file locks */
        if(t->ofiles->ofile[i])
        {
            syscall_close(i);
        }
    }

    if(t->fs)
    {
        release_node(t->fs->cwd);
        t->fs->cwd = NULL;
    
        release_node(t->fs->root);
        t->fs->root = NULL;
    }
    
    zombify(t);
    notify_tracer(t);

    if(t->parent)
    {
        /*
         * unblock our parent if we vforked (the call to add_task_signal() will
         * actually unblock the parent)
         *
         * we do this because after a vfork, the parent is blocked until 
         * the child:
         *   1. exits by calling _exit() or after receiving a signal
         *   2. calls execve()
         *
         * for more details, see: 
         *      https://man7.org/linux/man-pages/man2/vfork.2.html
         */
        if((t->properties & PROPERTY_VFORK) && 
           (t->parent->state == TASK_WAITING))
        {
            t->parent->state = TASK_SLEEPING;
        }

        if(t->parent != get_task_by_tid(t->tracer_pid))
        {
        	KDEBUG("terminate_task: pid %d, notifying parent\n", t->pid);

            int code = WCOREDUMP(t->exit_status) ? CLD_DUMPED :
                       (WIFSIGNALED(t->exit_status) ? CLD_KILLED : CLD_EXITED);
            add_task_child_signal(t, code, t->exit_status);
        }
    }
    
    zombie_loop(t);
}

