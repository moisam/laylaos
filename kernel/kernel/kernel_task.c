/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kernel_task.c
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
 *  \file kernel_task.c
 *
 *  Functions to create and run kernel tasks.
 */

//#define __DEBUG

#include <string.h>
#include <sys/syscall.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/asm.h>
#include <kernel/syscall.h>
#include <kernel/fpu.h>

#include "task_funcs.c"

// defined in run_kernel_task.c
extern void run_kernel_task(void);

#define NR_KTASKS               64

// NOTE: DON'T change the order of the fields in this struct as their offsets
//       are used in assembly code in run_kernel_task()
struct kernel_task_t
{
    char name[8];
    void (*func)(void *arg);
    void *func_arg;
    int flags;
    pid_t pid;
} kernel_tasks[NR_KTASKS];

struct kernel_mutex_t kernel_task_lock;


static inline void init_table(void)
{
    static int inited = 0;
    
    if(!inited)
    {
        inited = 1;
        memset(kernel_tasks, 0, sizeof(struct kernel_task_t) * NR_KTASKS);
        init_kernel_mutex(&kernel_task_lock);
    }
}


struct kernel_task_t *get_ktask(pid_t pid)
{
    int i;
    kernel_mutex_lock(&kernel_task_lock);
    
    for(i = 0; i < NR_KTASKS; i++)
    {
        if(kernel_tasks[i].pid == pid)
        {
            break;
        }
    }

    kernel_mutex_unlock(&kernel_task_lock);
    
    if(i >= NR_KTASKS)
    {
        return NULL;
    }
    
    return &kernel_tasks[i];
}


static struct kernel_task_t *add_ktask(char *name,
                                       void (*func)(void *), void *func_arg,
                                       pid_t pid, int flags)
{
    int i;
    kernel_mutex_lock(&kernel_task_lock);
    
    for(i = 0; i < NR_KTASKS; i++)
    {
        if(kernel_tasks[i].pid == 0)
        {
            break;
        }
    }

    if(i >= NR_KTASKS)
    {
        kernel_mutex_unlock(&kernel_task_lock);
        return NULL;
    }
    
    if(strlen(name) >= 8)
    {
        //strncpy(kernel_tasks[i].name, name, 7);
        memcpy(kernel_tasks[i].name, name, 7);
        kernel_tasks[i].name[7] = '\0';
    }
    else
    {
        strcpy(kernel_tasks[i].name, name);
    }
    
    kernel_tasks[i].func = func;
    kernel_tasks[i].func_arg = func_arg;
    kernel_tasks[i].pid = pid;
    kernel_tasks[i].flags = flags;
    kernel_mutex_unlock(&kernel_task_lock);
    
    return &kernel_tasks[i];
}


void ktask_elevate_priority(void)
{
    struct task_t *ct = cur_task;
    uintptr_t s = int_off();

    KDEBUG("%s: pid %d\n", __func__, ct->pid);
    remove_from_ready_queue(ct);
    ct->priority = MAX_FIFO_PRIO;
    ct->sched_policy = SCHED_FIFO;
    ct->user = 0;
    ct->nice = 0;
    append_to_ready_queue(ct);
    
    int_on(s);
}


#include "nanoprintf.h"

pid_t start_kernel_task(char *name, void (*func)(void *), void *func_arg,
                        struct task_t **t, int flags)
{
    register pid_t pid;

    init_table();

    struct task_t *ct = cur_task;
    struct regs r;

    fpu_state_save(ct);
    ct->regs = &r;
    save_context(ct);
    memcpy(&r, &ct->saved_context, sizeof(struct regs));

#ifdef __x86_64__
    r.rip = (uintptr_t)run_kernel_task;
    r.rflags |= 0x200;
    r.rax = __NR_fork;
#else
    r.eip = (uintptr_t)run_kernel_task;
    r.eflags |= 0x200;
    r.eax = __NR_fork;
#endif

    pid = syscall_fork();

    if(pid < 0)
    {
        printk("kernel: failed to fork kernel task\n");
        return pid;
    }
    
    // parent
    if(!add_ktask(name, func, func_arg, pid, flags))
    {
        kpanic("Failed to add kernel task entry!");
    }
    
    if(t)
    {
        char tmp[TASK_COMM_LEN];
        *t = get_task_by_id(pid);
        (*t)->user = 0;
        //sprintf(tmp, "[%s]", name);
        npf_snprintf(tmp, TASK_COMM_LEN, "[%s]", name);
        set_task_comm(*t, tmp);
    }
    
    KDEBUG("start_kernel_task: parent - name '%s', pid %d, func 0x%x\n", name, pid, func);
    
    return pid;
}


void unblock_kernel_task(struct task_t *task)
{
    if(!task)
    {
        return;
    }

    uintptr_t s = int_off();
    
    if(task->state == TASK_READY || task->state == TASK_RUNNING ||
       task->state == TASK_ZOMBIE)
    {
        // task is already unblocked
        int_on(s);
        return;
    }

    task->state = TASK_READY;
    task->wait_channel = NULL;
    
    KDEBUG("%s: pid %d\n", __func__, task->pid);

    remove_from_queue(task);
    append_to_ready_queue(task);
    
    int_on(s);
}

