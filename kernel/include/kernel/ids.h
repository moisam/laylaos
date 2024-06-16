/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ids.h
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
 *  \file ids.h
 *
 *  Helper macros for working with task and thread ids.
 */


#ifndef __IDS_H__
#define __IDS_H__

// macro to help loop through a task's threads
#define for_each_thread(thread, task)                   \
    for(thread = task->threads->thread_group_leader;    \
        thread != NULL;                                 \
        thread = thread->thread_group_next)


#define setid(task, which, id)                      \
{                                                   \
    struct task_t *thread;                          \
    kernel_mutex_lock(&(task->threads->mutex));     \
    for_each_thread(thread, task)                   \
        thread-> which = id;                        \
    kernel_mutex_unlock(&(task->threads->mutex));   \
}


#define setrootid(task, which, id)                  \
{                                                   \
    struct task_t *thread;                          \
    kernel_mutex_lock(&(task->threads->mutex));     \
    for_each_thread(thread, task)                   \
    {                                               \
        thread-> which = id;                        \
        thread->e##which = id;                      \
        thread->ss##which = id;                     \
    }                                               \
    kernel_mutex_unlock(&(task->threads->mutex));   \
}


/**********************************
 * Functions defined in ids.c
 **********************************/

/**
 * @brief Handler for syscall setgid().
 *
 * Set caller task's group id.
 *
 * @param   gid     group id
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setgid(gid_t gid);

/**
 * @brief Handler for syscall getgid().
 *
 * Get caller task's real group id.
 *
 * @return  real group id.
 */
int syscall_getgid(void);

/**
 * @brief Handler for syscall getegid().
 *
 * Get caller task's effective group id.
 *
 * @return  effective group id.
 */
int syscall_getegid(void);

/**
 * @brief Handler for syscall setuid().
 *
 * Set caller task's user id.
 *
 * @param   uid     user id
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setuid(uid_t uid);

/**
 * @brief Handler for syscall getuid().
 *
 * Get caller task's real user id.
 *
 * @return  real user id.
 */
int syscall_getuid(void);

/**
 * @brief Handler for syscall geteuid().
 *
 * Get caller task's effective user id.
 *
 * @return  effective user id.
 */
int syscall_geteuid(void);

/**
 * @brief Handler for syscall setpgid().
 *
 * Set a task's process group id.
 *
 * @param   pid     process id (see unistd.h for special values)
 * @param   pgid    process group id (see unistd.h for special values)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setpgid(pid_t pid, pid_t pgid);

/**
 * @brief Handler for syscall getpgid().
 *
 * Get process group id.
 *
 * @param   pid     process id (pass 0 for the calling task)
 *
 * @return  process group id.
 */
int syscall_getpgid(pid_t pid);

/**
 * @brief Handler for syscall getpgrp().
 *
 * Get calling task's process group id.
 *
 * @return  process group id.
 */
int syscall_getpgrp(void);

//int syscall_setpgrp(pid_t pid, pid_t pgid);

/**
 * @brief Handler for syscall getpid().
 *
 * Get calling task's process id.
 *
 * @return  process id.
 */
int syscall_getpid(void);

/**
 * @brief Handler for syscall getppid().
 *
 * Get calling task's parent's process id.
 *
 * @return  parent process id.
 */
int syscall_getppid(void);

/**
 * @brief Handler for syscall getsid().
 *
 * Get session id.
 *
 * @param   pid     process id (pass 0 for the calling task)
 *
 * @return  session id.
 */
int syscall_getsid(pid_t pid);

/**
 * @brief Handler for syscall setsid().
 *
 * Set calling task's session id.
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setsid(void);

/**
 * @brief Handler for syscall setreuid().
 *
 * Set calling task's real and/or effective user ids.
 *
 * @param   ruid    real user id to set (unless -1 is passed)
 * @param   euid    effective user id to set (unless -1 is passed)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setreuid(uid_t ruid, uid_t euid);

/**
 * @brief Handler for syscall setregid().
 *
 * Set calling task's real and/or effective group ids.
 *
 * @param   rgid    real group id to set (unless -1 is passed)
 * @param   egid    effective group id to set (unless -1 is passed)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setregid(gid_t rgid, gid_t egid);

/**
 * @brief Handler for syscall setresuid().
 *
 * Set calling task's real, effective and saved set user ids.
 *
 * @param   newruid    real user id to set (unless -1 is passed)
 * @param   neweuid    effective user id to set (unless -1 is passed)
 * @param   newsuid    saved-set-user-id to set (unless -1 is passed)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setresuid(uid_t newruid, uid_t neweuid, uid_t newsuid);

/**
 * @brief Handler for syscall setresgid().
 *
 * Set calling task's real, effective and saved set group ids.
 *
 * @param   newrgid    real group id to set (unless -1 is passed)
 * @param   newegid    effective group id to set (unless -1 is passed)
 * @param   newsgid    saved-set-group-id to set (unless -1 is passed)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setresgid(gid_t newrgid, gid_t newegid, gid_t newsgid);

/**
 * @brief Handler for syscall getresuid().
 *
 * Get calling task's real, effective and saved set user ids.
 *
 * @param   ruid    real user id is returned here
 * @param   euid    effective user id is returned here
 * @param   suid    saved-set-user-id is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);

/**
 * @brief Handler for syscall getresgid().
 *
 * Get calling task's real, effective and saved set group ids.
 *
 * @param   rgid    real group id is returned here
 * @param   egid    effective group id is returned here
 * @param   sgid    saved-set-group-id is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);

#endif      /* __IDS_H__ */
