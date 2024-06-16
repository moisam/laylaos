/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: task-defs.h
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
 *  \file task-defs.h
 *
 *  Task (or process) related structure and macro definitions.
 */

#ifndef __TASK_DEFS__
#define __TASK_DEFS__


/* Thread-scheduling classes */
/* We have newlib now, which defines these in sys/sched.h */
#define NR_SCHED_CLASS          3

#define MIN_FIFO_PRIO           1   /**< min FIFO priority */
#define MAX_FIFO_PRIO           59  /**< max FIFO priority */
#define MIN_RR_PRIO             60  /**< min round-robin priority */
#define MAX_RR_PRIO             99  /**< max round-robin priority */
#define MIN_USER_PRIO           0   /**< min user priority */
#define MAX_USER_PRIO           0   /**< max user priority */


/* task states */
//#define TASK_DYING              8   // exiting
#define TASK_STOPPED            7   /**< task being traced */
#define TASK_IDLE               6   /**< task idle or being created */
#define TASK_ZOMBIE             5   /**< task being terminated */
#define TASK_SLEEPING           4   /**< task in high priority sleep 
                                           (interruptible) */
#define TASK_WAITING            3   /**< task in low priority sleep
                                           (uninterruptible) */
#define TASK_READY              2   /**< task ready to run */
#define TASK_RUNNING            1   /**< task running */

/* task properties */
#define PROPERTY_TRACE_SYSCALLS     (1 << 0)    /**< syscalls being traced */
#define PROPERTY_TRACE_SYSEMU       (1 << 1)    /**< syscalls being emulated */
#define PROPERTY_TRACE_SIGNALS      (1 << 2)    /**< signals being traced */
#define PROPERTY_TRACE_SUSPENDED    (1 << 3)    /**< suspended during trace */

#define PROPERTY_FINISHING          (1 << 6)    /**< task is dying */
#define PROPERTY_VFORK              (1 << 7)    /**< task is the child of a 
                                                     vfork() call */
#define PROPERTY_USED_FPU           (1 << 8)    /**< task has used the fpu */
#define PROPERTY_IN_WAIT            (1 << 9)    /**< task is blocked waiting
                                                     for children */

#if 0
#define PROPERTY_TIMEOUT            (1 << 9)    /**< a call to block_task2()  
                                                     has timed out */
#endif

#define PROPERTY_HANDLING_SIG       (1 << 11)   /**< task is handling 
                                                     a signal */
#define PROPERTY_IN_SYSCALL         (1 << 12)   /**< task in the middle of 
                                                     a syscall */
#define PROPERTY_HANDLING_PAGEFAULT (1 << 13)   /**< task is handling a 
                                                     page fault */
#define PROPERTY_DYNAMICALLY_LOADED (1 << 14)   /**< dynamically loaded */


#define NR_TASKS                    256         /**< max system tasks 
                                                     (i.e. task table size) */

#if (NR_TASKS > MAX_NR_TASKS)
# error "NR_TASKS is higher than the maximum allowed in MAX_NR_TASKS"
#endif

//#include <sys/syslimits.h>      // NGROUPS_MAX
#include <limits.h>      // NGROUPS_MAX


#define TASK_COMM_LEN           16  /**< length of the task 'command' field */

/*
 * Flags for struct kernel_task_t flags field.
 */
#define KERNEL_TASK_ELEVATED_PRIORITY       0x01    /**< create kernel task
                                                           with elevated 
                                                           priority */


#include <signal.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <kernel/mutex.h>
#include <kernel/timer.h>
#include <mm/mmngr_virtual.h>
#include <kernel/vfs.h>


/**
 * @struct task_files_t
 * @brief The task_files_t structure.
 *
 * A structure to represent a task's open files.
 */
struct task_files_t
{
    struct file_t *ofile[NR_OPEN];  /**< open files */
    struct kernel_mutex_t mutex;    /**< struct lock */
};

/**
 * @struct task_fs_t
 * @brief The task_fs_t structure.
 *
 * A structure to represent a task's filesystem state.
 */
struct task_fs_t
{
    struct fs_node_t *root,         /**< task root directory */
                     *cwd;          /**< task current working directory */
    mode_t umask;                   /**< file creation mask */
    struct kernel_mutex_t mutex;    /**< struct lock */
};

/**
 * @struct task_sig_t
 * @brief The task_sig_t structure.
 *
 * A structure to represent a task's signal handlers.
 */
struct task_sig_t
{
    struct sigaction signal_actions[NSIG];  /**< signal handlers */
};

/**
 * @struct task_threads_t
 * @brief The task_threads_t structure.
 *
 * A structure to represent a task's threads.
 */
struct task_threads_t
{
    struct task_t *thread_group_leader; /**< thread group leader */
    int thread_count;       /**< number of threads in this task */
    pid_t tgid;             /**< thread group id */
    unsigned long group_user_time,      /**< thread group user time */
                  group_sys_time;       /**< thread group system time */
    struct kernel_mutex_t mutex;        /**< struct lock */
};

/**
 * @struct task_common_t
 * @brief The task_common_t structure.
 *
 * A structure to represent common fields shared by all threads in a task.
 */
struct task_common_t
{
    /* task-wide interval timers, shared by all threads */
    struct itimer_t __itimer_real,      /**< real timer */
                    __itimer_virt,      /**< virtual timer */
                    __itimer_prof;      /**< profiling timer */

#define itimer_real   common->__itimer_real
#define itimer_virt   common->__itimer_virt
#define itimer_prof   common->__itimer_prof

    /* resource limits */
    struct rlimit __task_rlimits[RLIMIT_NLIMITS];   /**< resource limits */

#define task_rlimits  common->__task_rlimits

    /* per-task POSIX timers */
    unsigned int __last_timerid;        /**< last used timer id */
    struct posix_timer_t *__head;       /**< head of POSIX timer list */

#define last_timerid  common->__last_timerid
#define posix_timers  common->__head

    struct kernel_mutex_t mutex;        /**< struct lock */
};

/**
 * @struct i387_state_t
 * @brief The i387_state_t structure.
 *
 * A structure to store FPU registers.
 */
struct i387_state_t
{
    long cwd;
    long swd;
    long twd;
    long fip;
    long fcs;
    long foo;
    long fos;
    long st_space[20];    /* 8*10 bytes for each FP-reg = 80 bytes */
};


/**
 * @struct task_t
 * @brief The task_t structure.
 *
 * The task control block (TCB).
 *
 * The fields are hard-coded as their offsets are used in asm routines, e.g.
 * syscall_dispatcher.S, switch_task.S and run_kernel_task.S, so please
 * DON'T MOVE THEM AROUND!
 */
struct task_t
{
    pid_t pid;                  /**< process id */

    int user;                   /**< user or kernel process? */

    int user_in_kernel_mode;    /**< is user process running in kernel mode
                                       (i.e. suspended during a syscall?) */

    /* 
     * For user tasks, temp kernel stack when syscalling 
     */
    physical_addr kstack_phys;  /**< kernel stack physical address */
    virtual_addr kstack_virt;   /**< kernel stack virtual address */

    /* 
     * Task's page directory 
     */
    physical_addr pd_phys;      /**< page directory physical address */
    virtual_addr pd_virt;       /**< page directory virtual address */

    /* current task register state - currently (see laylaos.h):
     *     i686 -> 76 bytes
     *     x86_64 -> 184 bytes
     */
    struct regs saved_context;  /**< saved context */

#ifdef __x86_64__

    struct
    {
        uintptr_t rbp, rdi, rsi, rdx, r8, rip, rsp;
    } execve;

#endif

    int state;                  /**< task running state */

    int time_left;              /**< time to run */
    
    int timeslice;

    int properties;             /**< task properties */

    int sched_policy;           /**< scheduling policy */

    int priority;               /**< task priority in the queue */

    void *wait_channel;         /**< if task is sleeping, this tells the 
                                       waiting channel, i.e. what address
                                       is task waiting on */

    int children;               /**< number of child tasks */

    uint32_t exit_status;       /**< exit status of the process 
                                       after termination */

    struct task_fs_t *fs;       /**< filesystem info */

    struct task_t *next,                /**< pointer to next task */
                  *prev;                /**< pointer to previous task */

    struct task_t *parent;              /**< pointer to parent task */

    /* 
     * Pointers to first child and first sibling 
     */
    volatile struct task_t *first_child,    /**< pointer to first child */
                           *first_sibling;  /**< pointer to first sibling */
  
    /* 
     * Fields to support multi-threading 
     */
    struct task_threads_t *threads;     /**< thread info */
    struct task_t *thread_group_next;   /**< next thread in group */

    struct
    {
        uintptr_t base;
        uintptr_t limit;
    } ldt;                              /**< thread local storage info */

    struct task_files_t *ofiles;        /**< open files handlers */

    uint32_t cloexec;               /**< which files are closed on exec() */
  
    struct task_vm_t *mem;          /**< task memory map */

    /*
     * Signals 
     */
    struct task_sig_t *sig;             /**< task signal info */
    siginfo_t siginfo[NSIG];            /**< siginfo structures for every 
                                               delivered signal */
    volatile sigset_t signal_pending;   /**< pending signals */
    sigset_t signal_mask;               /**< signal mask */
    sigset_t saved_signal_mask;         /**< saved signal mask */
    sigset_t signal_caught;             /**< caught signals */
    sigset_t signal_timer;              /**< signals raised by POSIX timer
                                               expiration */
    stack_t signal_stack;               /**< signal stack */

    volatile int woke_by_signal;        /**< set when task gets woken up by
                                               a signal */

    dev_t ctty;                         /**< the controlling terminal */
  
    int nice;                           /**< nice value */

    pid_t pgid;                         /**< process group id */

    pid_t sid;                          /**< session id */

    /* 
     * User id's & Group id's 
     */
    uid_t uid,                          /**< real UID */
          euid,                         /**< effective UID */
          ssuid;                        /**< saved SUID */
    gid_t gid,                          /**< real GID */
          egid,                         /**< effective GID */
          ssgid;                        /**< saved SGID */

    /*
     * NOTE: supplementary gids that are not used should be set to -1 (not 0),
     *       as zero is the root gid.
     */
    gid_t extra_groups[NGROUPS_MAX];    /**< supplementary group IDs */

    /*
     * Time the process has been using the CPU, measured in clock ticks 
     */
    unsigned long long start_time;      /**< task start time */
    unsigned long user_time,            /**< task user time */
                  sys_time;             /**< task system time */
    unsigned long children_user_time,   /**< collective children user time */
                  children_sys_time;    /**< collective children system time */

    struct task_common_t *common;       /**< resource limits and timers */

    struct kernel_mutex_t task_mutex;   /**< task lock */
    struct kernel_mutex_t *lock_held;

    /* 
     * Name of the executable whose code is running in this task 
     */
    char command[TASK_COMM_LEN];        /**< the 'command' field */
  
    /* 
     * Pointers to args and env strings 
     */
    void *arg_start,        /**< start address of task arguments */
         *arg_end;          /**< end address of task arguments */
    void *env_start,        /**< start address of task environment */
         *env_end;          /**< end address of task environment */
  
    /* 
     * The executable's device and inode numbers 
     */
    dev_t exe_dev;          /**< Exe device id */
    ino_t exe_inode;        /**< Exe inode number */
  
    /*
     * page fault counter:
     *    minflt -> minor faults, not requiring loading a memory page from disk
     *    majflt -> major fault, requiring a page load
     */
    unsigned long minflt,           /**< minor page faults */
                  majflt;           /**< major page faults */
    unsigned long children_minflt,  /**< children minor page faults */
                  children_majflt;  /**< children major page faults */
    
    /*
     * r/w accounting info:
     *    number of bytes read & written
     *    number of read & write calls
     */
    unsigned long read_count,       /**< number of read bytes */
                  write_count;      /**< number of written bytes */
    unsigned int  read_calls,       /**< number of read calls */
                  write_calls;      /**< number of write calls */
  
    /*
     * fields to support process tracing
     */

    struct regs *regs;          /**< traced registers, also contains user
                                     registers on entry to syscall */
    unsigned int interrupted_syscall;   /**< number of the 
                                               interrupted syscall */

    struct list_t *tracees;     /**< if this task is tracing other tasks, 
                                       this is the list of tracees */
  
    pid_t tracer_pid;           /**< if this task is being traced, this is
                                       the thread id of the tracer */
  
    int ptrace_options;         /**< ptrace options, set by PTRACE_SETOPTIONS
                                       (see `man 2 ptrace`) */
  
    unsigned long ptrace_eventmsg;  /**< ptrace message to be retrieved by 
                                           requesting PTRACE_GETEVENTMSG, the
                                           actual value depends on the msg 
                                           (see `man 2 ptrace`) */
  
    /* FPU math state */
#ifdef __x86_64__
    uint64_t fpregs[64] __attribute__((aligned(16)));   /**< XMM registers */
#else
    struct i387_state_t i387;       /**< FPU math registers */
#endif
};

#endif      /* __TASK_DEFS__ */
