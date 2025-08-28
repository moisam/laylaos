/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: task.h
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
 *  \file task.h
 *
 *  Functions and macros to work with tasks (processes).
 */

#ifndef __TASK_H__
#define __TASK_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <signal.h>
//#include <sys/signal.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/timer.h>
#include <kernel/ids.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/memregion.h>       // memregion and task_vm_t struct definitions
#include <signal.h>

#include "bits/task-defs.h"
#include "rlimit.h"


/**
 * @struct task_queue_t
 * @brief The task_queue_t structure.
 *
 * A structure to represent a queue of tasks.
 */
struct task_queue_t
{
    volatile struct task_t head;    /**< pointer to queue head */
    int has_ready_tasks;   /**< non-zero if queue has ready-to-run tasks */
};


/*
 * Set/unset the close-on-exec flag for a given fd.
 * We do it this way so that if we change the implementation of the cloexec
 * field in struct task_t, we only change the code in one place.
 */
static inline void cloexec_set(volatile struct task_t *t, int fd)
{
    t->cloexec |= (1 << fd);
}

static inline void cloexec_clear(volatile struct task_t *t, int fd)
{
    t->cloexec &= ~(1 << fd);
}

static inline int is_cloexec(volatile struct task_t *t, int fd)
{
    return (t->cloexec & (1 << fd));
}


/*************************************
 * Some helper macros
 *************************************/

#define suser(ct)           !((ct)->euid && (ct)->uid)

#define group_leader(ct)    ((ct)->pgid == (ct)->threads->tgid)
#define session_leader(ct)  ((ct)->sid == (ct)->threads->tgid)

#define tgid(t)             ((t)->threads ? (t)->threads->tgid : (t)->pid)


// a short-hand for all the code that traverses the master task table
#define for_each_taskptr(t)                              \
    volatile struct task_t **t;                          \
    volatile struct task_t **lt = &task_table[NR_TASKS]; \
    for(t = task_table; t < lt; t++)


//extern struct task_t *idle_task;    /**< pointer to the idle task (pid 0) */
//extern struct task_t *cur_task;     /**< pointer to the current task */
extern struct task_t *init_task;    /**< pointer to the init task (pid 1) */

extern volatile struct task_t *task_table[];/**< the master task table */
extern struct task_queue_t ready_queue[];   /**< pointers to the 
                                                   ready-to-run queues */
extern struct task_queue_t blocked_queue;   /**< pointer to the queue of 
                                                   blocked tasks */
extern struct task_queue_t zombie_queue;    /**< pointer to the queue of
                                                   zombie tasks */

extern volatile struct kernel_mutex_t task_table_lock;   /**< master task table lock */
extern volatile struct kernel_mutex_t scheduler_lock;    /**< master scheduler lock */

extern int total_tasks;                     /**< total tasks on the system */
extern pid_t next_pid;              /**< next pid for creating new tasks */


/**************************************
 * Functions defined in task.c
 **************************************/

/**
 * @brief Initialize tasking.
 *
 * Initialize the kernel's tasking subsystem and kick off the idle task.
 *
 * @return  nothing.
 */
void tasking_init(void);

/**
 * @brief Get idle task.
 *
 * Get the idle task of the given cpu id. Used during boot to set up idle 
 * tasks for each of the available processors.
 *
 * @param   cpuid       cpu id
 *
 * @return  pointer to task.
 */
struct task_t *get_cpu_idle_task(int cpuid);

/**
 * @brief The scheduler.
 *
 * Schedule the next task to run.
 *
 * @return  nothing.
 */
void  scheduler(void);

/**
 * @brief Get init task.
 *
 * Get a pointer to the init task (pid 1).
 *
 * @return  pointer to task.
 */
struct task_t *get_init_task(void);

/**
 * @brief Get current task.
 *
 * Get a pointer to the currently running task. This function is mainly used
 * by kernel modules.
 *
 * @return  pointer to task.
 */
volatile struct task_t *get_cur_task(void);

/**
 * @brief Get idle task.
 *
 * Get a pointer to the idle task (pid 0).
 *
 * @return  pointer to task.
 */
volatile struct task_t *get_idle_task(void);

/**
 * @brief Set init task.
 *
 * The init task should be set only once during boot.
 *
 * @param   task    pointer to task
 *
 * @return  nothing.
 */
void set_init_task(struct task_t *task);

/**
 * @brief Set task command.
 *
 * Set the task's command field. The passed \a command should be shorter than
 * TASK_COMM_LEN, otherwise it will be truncated.
 *
 * @param   task        pointer to task
 * @param   command     command string
 *
 * @return  nothing.
 */
void set_task_comm(struct task_t *task, char *command);

/**
 * @brief Get code segment end.
 *
 * Helper function to get the end of the task's code segment.
 *
 * @param   task    pointer to task
 *
 * @return  virtual address.
 */
virtual_addr task_get_code_end(volatile struct task_t *task);

/**
 * @brief Get data segment end.
 *
 * Helper function to get the end of the task's data segment.
 *
 * @param   task    pointer to task
 *
 * @return  virtual address.
 */
virtual_addr task_get_data_end(volatile struct task_t *task);

/**
 * @brief Get code segment start.
 *
 * Helper function to get the start of the task's code segment.
 *
 * @param   task    pointer to task
 *
 * @return  virtual address.
 */
virtual_addr task_get_code_start(volatile struct task_t *task);

/**
 * @brief Get data segment start.
 *
 * Helper function to get the start of the task's data segment.
 *
 * @param   task    pointer to task
 *
 * @return  virtual address.
 */
virtual_addr task_get_data_start(volatile struct task_t *task);

/**
 * @brief Allocate a task struct.
 *
 * As this function is only called during fork/clone, we don't bother 
 * allocating memory for the new task's vm struct, as fork/clone will
 * free the vm struct and make a copy (fork) or use the parent's vm
 * struct (clone).
 *
 * @return  pointer to new task struct.
 */
struct task_t *task_alloc(void);

/**
 * @brief Free task struct.
 *
 * Called when reaping zombie tasks and when a fork fails midway.
 *
 * @param   task            pointer to task
 *
 * @return  nothing.
 */
void task_free(volatile struct task_t *task);

/**
 * @brief Blocked task callback.
 *
 * Callback function to be called by the clock soft interrupt when the
 * timeout set by block_task2() expires.
 *
 * @param   arg             pointer to blocked task
 *
 * @return  nothing.
 */
void block_task_callback(void *arg);

/**
 * @brief Block task with timeout.
 *
 * Send the calling task to sleep until an event occurs, it is woken up by
 * a signal, or the given \a timeout (in ticks) expires.
 *
 * @param   wait_channel    wait channel to sleep on
 * @param   timeout         timeout in ticks (if 0, task sleeps until a signal
 *                            is delivered or an I/O event occurs)
 *
 * @return  EWOULDBLOCK if \a timeout expired, EINTR if woken up by a signal,
 *            zero if woken by some other event.
 */
int block_task2(void *wait_channel, int timeout);

/**
 * @brief Block task.
 *
 * Send the calling task to sleep. If \a interruptible is non-zero, the sleep
 * can be interrupted by a signal.
 *
 * @param   wait_channel    wait channel to sleep on
 * @param   interruptible   non-zero for interruptible sleep
 *
 * @return  1 if interruptible sleep and woken by a signal, zero otherwise.
 */
int block_task(void *wait_channel, int interruptible);

/**
 * @brief Unblock tasks.
 *
 * Unblock all the tasks sleeping on the given \a wait_channel.
 *
 * @param   wait_channel    wait channel
 *
 * @return  nothing.
 */
void unblock_tasks(void *wait_channel);

/**
 * @brief Unblock task.
 *
 * Wake up a sleeping task, but do not preempt the current task if the
 * awoken task has a higher priority.
 *
 * @param   task            pointer to task
 *
 * @return  nothing.
 *
 * @see     unblock_task()
 */
void unblock_task_no_preempt(volatile struct task_t *task);

/**
 * @brief Unblock task.
 *
 * Wake up a sleeping task, could preempt the current task if the
 * awoken task has a higher priority.
 *
 * @param   task            pointer to task
 *
 * @return  nothing.
 *
 * @see     unblock_task_no_preempt()
 */
void unblock_task(volatile struct task_t *task);

/**
 * @brief Add child task to parent.
 *
 * Helper function to add the \a child task to the \a parent's children list.
 *
 * @param   parent          pointer to parent task
 * @param   child           pointer to child task
 *
 * @return  nothing.
 */
void task_add_child(struct task_t *parent, struct task_t *child);

/**
 * @brief Remove child task from parent.
 *
 * Helper function to remove the \a child task from the \a parent's 
 * children list.
 *
 * @param   parent          pointer to parent task
 * @param   child           pointer to child task
 *
 * @return  nothing.
 */
void task_remove_child(struct task_t *parent, volatile struct task_t *child);

/**
 * @brief Reap a zombie task.
 *
 * Called after the parent performs a wait() to collect the child task's
 * status. This function frees the task struct and removes the task
 * from the master task table.
 *
 * @param   task            pointer to task
 *
 * @return  nothing.
 */
void reap_zombie(volatile struct task_t *task);

/**
 * @brief Terminate task.
 *
 * Terminate a task, freeing most of its memory structures, notifying its
 * parent and its tracer (if it is being traced).
 *
 * @param   code            exit code
 *
 * @return  never returns.
 */
void terminate_task(int code);


void append_to_ready_queue_locked(volatile struct task_t *task, int move_queue);
void move_to_queue_end_locked(volatile struct task_t *task);
void task_change_priority(volatile struct task_t *t, int new_prio, int new_policy);
void schedule_and_block(volatile struct task_t *tracer, volatile struct task_t *tracee);


/**************************************
 * Functions defined in kernel_task.c
 **************************************/

/**
 * @brief Start a new kernel task.
 *
 * This is similar to fork, except it is called within the kernel and the
 * new task is a kernel task that could elevate its priority.
 *
 * @param   name            new task name
 * @param   func            kernel function to run in the new task
 * @param   func_arg        argument to pass to \a func
 * @param   t               where to store a pointer to the new task
 * @param   flags           task creation flags (currently the only defined
 *                            flag is \ref KERNEL_TASK_ELEVATED_PRIORITY)
 *
 * @return  new task pid in the parent and zero in the new child task 
 *          on success, or -(errno) on failure.
 */
pid_t start_kernel_task(char *name, void (*func)(void *), void *func_arg,
                        volatile struct task_t **t, int flags);

/**
 * @brief Unblock kernel task.
 *
 * Wake up a sleeping kernel task, but do not preempt the current task if the
 * awoken task has a higher priority.
 *
 * @param   task            pointer to task
 *
 * @return  nothing.
 */
void unblock_kernel_task(volatile struct task_t *task);


/**************************************
 * Functions defined in select.c
 **************************************/

/**
 * @brief Cancel task selects.
 *
 * Cancel all select() requests by the given task.
 * Called on task termination.
 *
 * @param   task    pointer to task
 *
 * @return  nothing.
 */
void task_cancel_select(struct task_t *task);


/**************************************
 * Functions defined in switch_task.S
 **************************************/

/**
 * @brief Save task context.
 *
 * Called from the scheduler during a context switch.
 *
 * @param   task            pointer to task
 *
 * @return  1 if returning from a context switch, 0 otherwise.
 */
int save_context(volatile struct task_t *task);

/**
 * @brief Restore task context.
 *
 * Called from the scheduler during a context switch.
 *
 * @param   task            pointer to task
 *
 * @return  nothing.
 */
void restore_context(volatile struct task_t *task);

#endif      /* __TASK_H__ */
