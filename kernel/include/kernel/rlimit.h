/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: rlimit.h
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
 *  \file rlimit.h
 *
 *  Functions for working with resource usage limits.
 */

#ifndef __KERNEL_RLIMITS_H__
#define __KERNEL_RLIMITS_H__

/**
 * @struct task_rlimit_t
 * @brief The task_rlimit_t structure.
 *
 * A structure to represent a resource usage limit within the kernel.
 */
struct task_rlimit_t
{
    char *name;             /**< resource name */
    char *units;            /**< resource units description */
    struct rlimit rlimit;   /**< resource limit value */
};


/**
 * @var default_rlimits
 * @brief default resource limits.
 *
 * This is a pointer to the default builtin resource usage limits.
 */
extern struct task_rlimit_t default_rlimits[];


#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

/**
 * @brief Check resource usage.
 *
 * Check if the given \a value exceeds the soft limit of the given \a resource.
 *
 * @param   t           pointer to task
 * @param   resource    one of the RLIMIT_* definitions from sys/resource.h
 * @param   value       resource limit value to check
 *
 * @return  1 if resouce limit is exceeded, 0 otherwise.
 */
INLINE int exceeds_rlimit(volatile struct task_t *t, int resource, rlim_t value)
{
    rlim_t limit;

    if(resource < 0 || resource >= RLIMIT_NLIMITS)
    {
        return 1;
    }
    
    limit = t->task_rlimits[resource].rlim_cur;

    return (limit != RLIM_INFINITY && value >= limit);
}


/**************************************
 * Functions defined in rlimit.c
 **************************************/

/**
 * @brief Handler for syscall getrusage().
 *
 * Get resource usage.
 *
 * @param   who     one of: RUSAGE_SELF, RUSAGE_CHILDREN, RUSAGE_THREAD
 * @param   r_usage resource usage is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_getrusage(int who, struct rusage *r_usage);

/**
 * @brief Handler for syscall getrlimit().
 *
 * Get resource limits.
 *
 * @param   resource    one of the RLIMIT_* definitions from sys/resource.h
 * @param   rlim        resource limits are returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_getrlimit(int resource, struct rlimit *rlim);

/**
 * @brief Handler for syscall setrlimit().
 *
 * Set resource limits.
 *
 * @param   resource    one of the RLIMIT_* definitions from sys/resource.h
 * @param   rlim        new resource limits
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_setrlimit(int resource, struct rlimit *rlim);

/**
 * @brief Handler for syscall prlimit().
 *
 * Get and/or set resource limits.
 *
 * @param   pid         process id
 * @param   resource    one of the RLIMIT_* definitions from sys/resource.h
 * @param   new_rlim    if not NULL, new resource limits
 * @param   old_rlim    if not NULL, old resource limits are returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_prlimit(pid_t pid, int resource, 
                     struct rlimit *new_rlim, struct rlimit *old_rlim);

/**
 * @brief Handler for syscall ulimit().
 *
 * Get or set resource limits. This syscall is obsolete.
 * See: https://man7.org/linux/man-pages/man3/ulimit.3.html
 *
 * @param   cmd         see possible values in sys/resource.h
 * @param   newlimit    new resource limits
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_ulimit(int cmd, long newlimit);


/*********************
 * Helper functions.
 *********************/

/**
 * @brief Set default resource limits.
 *
 * Set the resource limits of the given \a task to the default values.
 *
 * @param   task        pointer to task
 *
 * @return  nothing.
 */
void set_task_rlimits(struct task_t *task);

#endif      /* __KERNEL_RLIMITS_H__ */
