/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: ipc.h
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
 *  \file ipc.h
 *
 *  Functions and macro definitions for working with SysV Inter-Process
 *  Communication (IPC) structs. These include semaphones, message queues,
 *  and shared memory. Functions are defined in the source files under the
 *  kernel/ipc/ directory.
 */

#ifndef __KERNEL_IPC_H__
#define __KERNEL_IPC_H__

#ifndef __USE_GNU
#define __USE_GNU                   // for MSG_EXCEPT
#endif

#ifndef __USE_MISC
#define __USE_MISC                  // for SHM_DEST
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <kernel/mutex.h>
#include <kernel/syscall.h>         // struct syscall_args

#ifndef __SEMUN_DEFINED__
/* Argument to `semctl'. */
union semun
{
    int val;                    // value for SETVAL
    struct semid_ds *buf;       // buffer for IPC_STAT & IPC_SET
    unsigned short *array;      // array for GETALL & SETALL
};
#define __SEMUN_DEFINED__
#endif

/*
 * Flags for checking read/write permission (only one is used, depending on 
 * which system call is being served).
 */
#define READ_PERMISSION     1       /**< read permission */
#define WRITE_PERMISSION    2       /**< write permission */


/**
 * @struct ipcq_t
 * @brief The ipcq_t structure.
 *
 * General structure representing a queue of messages, semaphores or shared
 * memory segments.
 */
struct ipcq_t
{
    key_t key;      /**< queue key */
    int queue_id;   /**< queue id */

    union
    {
        struct shmid_ds shmid;  /**< shmem queue descriptor */
        struct msqid_ds msqid;  /**< message queue descriptor */
        struct semid_ds semid;  /**< semaphore queue descriptor */
    };

    union
    {
        struct msgmap_hdr_t *msg_head;  /**< head of message queue */
        struct shmmap_hdr_t *shm_head;
        //struct sem_map_hdr *sem_head;
    };

    union
    {
        struct msgmap_hdr_t *msg_tail;  /**< tail of message queue */
        //struct shm_map_hdr *shm_tail;
        //struct sem_map_hdr *sem_tail;
    };

    /* for shared memory & semaphores */
    union
    {
        struct memregion_s *memregion;  /**< memory region list of a shared
                                             memory segment */
        struct semaphore_t *sem_array;  /**< semaphore array */
    };

    /* for access synchronization */
    volatile struct kernel_mutex_t lock;         /**< queue lock */
};


/* flags for use by ipc/msg.c */
#define IPC_MSG_MAXDATA_BYTES           0x4000  /**< max queue size = 16k */
#define IPC_MSG_MAXMSG_SIZE             0x2000  /**< max msg size   =  8k */
#define IPC_MSG_MAX_QUEUES              128     /**< max msg queues */

/* flags for use by ipc/sem.c */
#define IPC_SEM_NSEMS_MAX               250     /**< max number of semaphores
                                                     per set */
#define IPC_SEM_NSOPS_MAX               32      /**< max number of operations
                                                     per semop() call */
#define IPC_SEM_MAX_QUEUES              128     /**< max number of semaphore
                                                     sets, system-wide */
#define IPC_SEM_MAX_SEMAPHORES          32000   /**< max number of semaphores,
                                                     system-wide */
#define IPC_SEM_MAX_VAL                 32767   /**< max val for a semaphore */

/* flags for use by ipc/shm.c */
#define IPC_SHM_SIZE_MIN                1           /**< min shmem segment 
                                                         size = 1 byte */
#define IPC_SHM_SIZE_MAX                0x4000000   /**< max shmem segment 
                                                         size = 64 MiB */
#define IPC_SHM_MAX_QUEUES              4096        /**< max number of shared
                                                         memory segments,
                                                         system-wide */


/**
 * @struct msgmap_hdr_t
 * @brief The msgmap_hdr_t structure.
 *
 * The system message map is a pool of messages, each consisting of a header
 * (this struct), followed by the message data proper.
 */
struct msgmap_hdr_t
{
    size_t size;    /**< total msg size (sizeof(mtext) + sizeof(mtype)) */
    struct msgmap_hdr_t *prev,  /**< previous header in list */
                        *next;  /**< next header in list */
};

/**
 * @struct shmmap_hdr_t
 * @brief The shmmap_hdr_t structure.
 *
 * A shared memory mapping header.
 */
struct shmmap_hdr_t
{
    int count;              /**< number of mapped pages */
    physical_addr frames[]; /**< physical addresses of mapped pages */
};

/**
 * @struct semaphore_t
 * @brief The semaphore_t structure.
 *
 * A semaphore.
 */
struct semaphore_t
{
    unsigned short semval;      /**< semaphore value */
    unsigned short semzcnt;     /**< number of tasks waiting for zero */
    unsigned short semncnt;     /**< number of tasks waiting for increase */
    pid_t sempid;               /**< PID of task that last operated on sem */
};


/**********************************
 * Functions defined in ipc/ipc.c
 **********************************/

/**
 * @brief Initialise SysV IPC queues.
 *
 * Initialise the semaphore, shared memory, and message queues.
 *
 * @return  nothing.
 */
void ipc_init(void);

/**
 * @brief Check SysV IPC permissions.
 *
 * Check if the given task has permission to perform a read/write operation
 * on some SysV IPC queue.
 *
 * @param   perm    SysV IPC permission struct
 * @param   task    task requesting permission
 * @param   what    type of requested permission: \ref READ_PERMISSION or 
 *                    \ref WRITE_PERMISSION
 *
 * @return  1 if task has permission, 0 otherwise.
 */
int ipc_has_perm(struct ipc_perm *perm, volatile struct task_t *task, int what);

/**
 * @brief Remove task IPC queues.
 *
 * Remove all msg queues and semaphores opened by this task.
 * Called from execve() and terminate_task().
 *
 * @param   task    task to remove its queues
 *
 * @return  nothing.
 *
 * @see     execve(), terminate_task()
 */
void ipc_killall(struct task_t *task);

/**
 * @brief Handler for syscall ipc().
 *
 * Switch function for different SysV IPC operations.
 *
 * @param   call    one of the IPCOP_* macros that are defined in sys/ipc_ops.h
 * @param   args    arguments (depend on \a call)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_ipc(int call, unsigned long *args);


/**********************************
 * Functions defined in ipc/msg.c
 **********************************/

/**
 * @brief Initialise SysV message queues.
 *
 * Initialise the message queue subsystem.
 *
 * @return  nothing.
 */
void msg_init(void);

/**
 * @brief Handler for syscall msgctl().
 *
 * Message queue control function.
 *
 * @param   msqid   message queue id
 * @param   cmd     command to perform (IPC_STAT, IPC_SET, or IPC_RMID)
 * @param   buf     data buffer, contents depend on \a cmd
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_msgctl(int msqid, int cmd, struct msqid_ds *buf);

/**
 * @brief Handler for syscall msgget().
 *
 * Get or create a new message queue.
 *
 * @param   key     message queue key
 * @param   msgflg  flags (see sys/msg.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_msgget(key_t key, int msgflg);

/**
 * @brief Handler for syscall msgget().
 *
 * Get a message from a message queue.
 *
 * @param   msqid   message queue id
 * @param   msgp    message data
 * @param   msgsz   count of bytes in \a msgp
 * @param   msgflg  flags (see sys/msg.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_msgsnd(int msqid, void *msgp, size_t msgsz, int msgflg);

/**
 * @brief Handler for syscall msgrcv().
 *
 * Get a message from a message queue.
 *
 * @param   __args  packed syscall arguments (see syscall.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_msgrcv(struct syscall_args *__args);

/**
 * @brief Remove task message queues.
 *
 * Remove all msg queues opened by this task.
 * Called from execve() and terminate_task(), by calling ipc_killall().
 *
 * @param   task    task to remove its queues
 *
 * @return  nothing.
 *
 * @see     execve(), terminate_task(), ipc_killall()
 */
void msg_killall(struct task_t *task);


/**********************************
 * Functions defined in ipc/sem.c
 **********************************/

/**
 * @brief Undo semaphore operations.
 *
 * Called during task exit, to undo semaphore operations not finished by the
 * dying task.
 *
 * @param   task    task to remove its queues
 *
 * @return  nothing.
 */
void do_sem_undo(struct task_t *task);

/**
 * @brief Initialise SysV semaphore queues.
 *
 * Initialise the semaphore queue subsystem.
 *
 * @return  nothing.
 */
void sem_init(void);

/**
 * @brief Handler for syscall semctl().
 *
 * Semaphore queue control function.
 *
 * @param   semid   semaphore queue id
 * @param   semnum  semaphore number in queue
 * @param   cmd     command to perform (IPC_STAT, IPC_SET, IPC_RMID, GETALL,
 *                    SETALL, GETNCNT, GETZCNT, GETPID, GETVAL or SETVAL)
 * @param   arg     data buffer, contents depend on \a cmd
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_semctl(int semid, int semnum, int cmd, union semun *arg);

/**
 * @brief Handler for syscall semget().
 *
 * Get or create a new message queue.
 *
 * @param   key     semaphore queue key
 * @param   nsems   semaphore count
 * @param   semflg  flags (see sys/sem.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_semget(key_t key, int nsems, int semflg);

/**
 * @brief Handler for syscall semop().
 *
 * Perform semaphore operations.
 *
 * @param   semid   semaphore queue id
 * @param   sops    semaphore operations
 * @param   nsops   umber of operations in \a sops
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_semop(int semid, struct sembuf *sops, size_t nsops);

/**
 * @brief Remove task semaphore queues.
 *
 * Remove all semaphore queues opened by this task.
 * Called from execve() and terminate_task(), by calling ipc_killall().
 *
 * @param   task    task to remove its queues
 *
 * @return  nothing.
 *
 * @see     execve(), terminate_task(), ipc_killall()
 */
void sem_killall(struct task_t *task);


/**********************************
 * Functions defined in ipc/shm.c
 **********************************/

/**
 * @brief Initialise SysV shared memory queues.
 *
 * Initialise the shared memory queue subsystem.
 *
 * @return  nothing.
 */
void shm_init(void);

/**
 * @brief Handler for syscall shmctl().
 *
 * Shared memory queue control function.
 *
 * @param   shmid   shmem queue id
 * @param   cmd     command to perform (IPC_STAT, IPC_SET, IPC_RMID, GETALL,
 *                    SETALL, GETNCNT, GETZCNT, GETPID, GETVAL or SETVAL)
 * @param   buf     data buffer, contents depend on \a cmd
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_shmctl(int shmid, int cmd, struct shmid_ds *buf);

/**
 * @brief Handler for syscall shmget().
 *
 * Get or create a new message queue.
 *
 * @param   key     semaphore queue key
 * @param   size    semaphore queue size
 * @param   shmflg  flags (see sys/shm.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_shmget(key_t key, size_t size, int shmflg);

/**
 * @brief Handler for syscall shmat().
 *
 * Attach a shared memory region.
 *
 * @param   shmid       shmem id
 * @param   shmaddr     where to attach in memory, if NULL, the kernel chooses
 *                        an appropriate address
 * @param   shmflg      flags (see sys/shm.h)
 * @param   result      attachment address is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_shmat(int shmid, void *shmaddr, int shmflg, volatile void **result);

/**
 * @brief Attach shared memory region.
 *
 * Helper function to attach a shared memory region.
 * Called from task_mem_dup().
 *
 * @param   task        task requesting shmem attachment
 * @param   memregion   memory region to which the shmem is attached
 * @param   shmaddr     memory address to attach at
 *
 * @return  zero on success, -(errno) on failure.
 */
long shmat_internal(struct task_t *task, struct memregion_t *memregion,
                                         void *shmaddr);

/**
 * @brief Detach shared memory region.
 *
 * Helper function to detach a shared memory region.
 * Called from memregion_detach().
 *
 * @param   task        task requesting shmem detachment
 * @param   memregion   memory region to which the shmem is attached
 * @param   shmaddr     memory address to detach
 *
 * @return  zero on success, -(errno) on failure.
 */
long shmdt_internal(struct task_t *task, struct memregion_t *memregion,
                                         void *shmaddr);

/**
 * @brief Handler for syscall shmdt().
 *
 * Detach a shared memory region.
 *
 * @param   shmaddr     address of region to detach
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_shmdt(void *shmaddr);

/**
 * @brief Get shmem id for a given memory region.
 *
 * If \a memregion represents a shared memory segment, this function returns 
 * the shared memory area id (to pass to shmat, shmdt, ...).
 *
 * @param   virt        memory address to detach
 * @param   memregion   memory region to which the shmem is attached
 *
 * @return  zero or a positive index on success, -(errno) on failure.
 */
int memregion_to_shmid(void *virt, struct memregion_t *memregion);

/**
 * @brief Get shmem page count.
 *
 * Return the count of memory pages attached to shared memory regions 
 * systemwide.
 *
 * @return  shmem page count.
 */
size_t get_shm_page_count(void);

//void shm_killall(struct task_t *task);

#endif      /* __KERNEL_IPC_H__ */
