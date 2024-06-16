/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: syscall.h
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
 *  \file syscall.h
 *
 *  General functions and macros to work with syscalls.
 */

#ifndef __KERNEL_SYSCALL_H__
#define __KERNEL_SYSCALL_H__

#include <utime.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sysinfo.h>
//#include <sys/sched.h>
#include <sys/wait.h>
#include <kernel/vfs.h>
#include <kernel/thread.h>
#include <kernel/gdt.h>
#include <kernel/net/socket.h>

#include "bits/syscall-defs.h"


/**
 * @var NR_SYSCALLS
 * @brief Number of syscalls.
 *
 * Contains the number of supported system calls.
 */
extern int NR_SYSCALLS;


/*
 * We use fork to handle the clone syscall.
 */
#define syscall_clone       syscall_fork


/**********************************
 * Functions defined in syscall.c
 **********************************/

/**
 * @brief Initialise syscalls.
 *
 * Initialize the kernel's system call service and register syscall IRQ 
 * handler.
 *
 * @return  nothing.
 */
void syscall_init(void);

/**
 * @brief Syscall dispatcher.
 *
 * System call switch function and the entry point to all syscalls.
 * The syscall's result is returned in the EAX (x86) or RAX (x86-64) register
 * of the \a r structure.
 *
 * @param   r       current CPU registers (contain syscall arguments)
 *
 * @return  nothing.
 */
void syscall_dispatcher(struct regs *r);

/**
 * @brief Check access permissions.
 *
 * Helper function to check whether the current user has access to the
 * given file \a node. If \a user_ruid is set, the caller's REAL uid/gid
 * are used instead of their EFFECTIVE uid/gid.
 *
 * @param   node        file node
 * @param   mode        access permissions to check
 * @param   use_ruid    if non-zero, use the real uid/gid, otherwise use the
 *                        effective uid/gid
 *
 * @return  zero if caller has access, -(ACCES) if not.
 */
int has_access(struct fs_node_t *node, int mode, int use_ruid);

/**
 * @brief Handler for syscall exit().
 *
 * Terminate the calling thread.
 *
 * @param   code        exit code
 *
 * @return  never returns.
 */
int syscall_exit(int code);

/**
 * @brief Handler for syscall exit_group().
 *
 * Terminate the calling thread and all other threads in the thread group.
 *
 * @param   code        exit code
 *
 * @return  never returns.
 */
int syscall_exit_group(int code);

/**
 * @brief Handler for syscall close().
 *
 * Close an open file.
 *
 * @param   fd          file descriptor
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_close(int fd);

/**
 * @brief Handler for syscall creat().
 *
 * Create a new file. This is a wrapper function that calls syscall_open()
 * internally to do the actual work.
 *
 * @param   pathname        path name
 * @param   mode            file mode and access permissions
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_open()
 */
int syscall_creat(char *pathname, mode_t mode);

/**
 * @brief Handler for syscall time().
 *
 * Get system time in seconds since 1 Jan 1970.
 *
 * @param   tloc        system time is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_time(time_t *tloc);

/**
 * @brief Handler for syscall mknodat().
 *
 * Create a new special or regular file.
 *
 * @param   dirfd           \a pathname is interpreted relative to this 
 *                            directory
 * @param   pathname        path name (absolute or relative)
 * @param   mode            file mode and access permissions
 * @param   dev             if file type is S_IFCHR or S_IFBLK, this contains
 *                            the new device's device id
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_mknod()
 */
int syscall_mknodat(int dirfd, char *pathname, mode_t mode, dev_t dev);

/**
 * @brief Handler for syscall mknod().
 *
 * Create a new special or regular file.
 *
 * @param   pathname        path name (absolute or relative)
 * @param   mode            file mode and access permissions
 * @param   dev             if file type is S_IFCHR or S_IFBLK, this contains
 *                            the new device's device id
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_mknodat()
 */
int syscall_mknod(char *pathname, mode_t mode, dev_t dev);

/**
 * @brief Handler for syscall lseek().
 *
 * Reposition read/write file offset.
 *
 * @param   fd          file descriptor
 * @param   offset      new file offset
 * @param   origin      one of: SEEK_CUR, SEEK_SET or SEEK_END
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_lseek(int fd, off_t offset, int origin);

/**
 * @brief Handler for syscall mount().
 *
 * Mount a filesystem.
 *
 * @param   source      pathname of filesystem to mount
 * @param   target      pathname of destination mount point
 * @param   fstype      filesystem type
 * @param   flags       mount flags (see the MS_* definitions in sys/mount.h)
 * @param   options     mount options (filesystem-specific)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_mount(char *source, char *target, char *fstype, 
                  int flags, char *options);

/**
 * @brief Handler for syscall umount2().
 *
 * Unmount a filesystem.
 *
 * @param   target      pathname of mounted filesystem
 * @param   flags       unmount flags (see the MNT_* and UMOUNT_* definitions
 *                        in sys/mount.h)
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_umount()
 */
int syscall_umount2(char *target, int flags);

/**
 * @brief Handler for syscall umount().
 *
 * Unmount a filesystem.
 *
 * @param   target      pathname of mounted filesystem
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_umount2()
 */
int syscall_umount(char *target);

/**
 * @brief Handler for syscall stime().
 *
 * This syscall is unimplemented as it is deprecated.
 *
 * @param   buf     unused
 *
 * @return  -(ENOSYS) always.
 */
int syscall_stime(long *buf);

/**
 * @brief Handler for syscall pause().
 *
 * pause() causes the calling process (or thread) to sleep until a signal is
 * delivered that either terminates the process or causes the invocation of a
 * signal-catching function.
 *
 * @return  -(EINTR) if the task is not terminated by a signal.
 */
int syscall_pause(void);

/**
 * @brief Handler for syscall rmdir().
 *
 * Remove an empty directory.
 *
 * @param   pathname        pathname of directory to remove
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_rmdir(char *pathname);

/**
 * @brief Handler for syscall times().
 *
 * Get task times.
 *
 * @param   buf         system and user times are returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_times(struct tms *buf);

/**
 * @brief Handler for syscall setheap().
 *
 * Set the calling task's heap start.
 *
 * @param   data_end    specify where data ends and heap starts
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setheap(void *data_end);

/**
 * @brief Handler for syscall brk().
 *
 * Change the calling task's break address (essentially change the data 
 * segment size).
 *
 * @param   incr    if non-zero, amount to increment current data segment
 *                    by in bytes
 * @param   res     the new break address is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_brk(unsigned long incr, uintptr_t *res);

/**
 * @brief Handler for syscall uname().
 *
 * Get name and information about the current kernel.
 *
 * @param   name    kernel information is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_uname(struct utsname *name);

/**
 * @brief Handler for syscall umask().
 *
 * Set file mode creation mask.
 *
 * @param   mask    new mask value
 *
 * @return  old mask value.
 */
int syscall_umask(mode_t mask);

/**
 * @brief Handler for syscall setdomainname().
 *
 * Set the machine's domain name.
 *
 * @param   name    new domain name
 * @param   len     length of \a name in bytes
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setdomainname(char *name, size_t len);

/**
 * @brief Handler for syscall sethostname().
 *
 * Set the machine's host name.
 *
 * @param   name    new host name
 * @param   len     length of \a name in bytes
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sethostname(char *name, size_t len);

/**
 * @brief Handler for syscall gettimeofday().
 *
 * Get system time.
 *
 * @param   tv      time value is returned here
 * @param   tz      timezone (currently unimplemented, should be NULL)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_gettimeofday(struct timeval *restrict tv, 
                         struct timezone *restrict tz);

/**
 * @brief Handler for syscall settimeofday().
 *
 * Set system time.
 *
 * @param   tv      new time value
 * @param   tz      timezone (currently unimplemented, should be NULL)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_settimeofday(struct timeval *tv, struct timezone *tz);

/**
 * @brief Handler for syscall getdents().
 *
 * Get directory entries.
 *
 * @param   fd      file descriptor
 * @param   dp      buffer (directory entries are returned here)
 * @param   count   size of \a buffer
 *
 * @return  number of bytes read on success, -(errno) on failure.
 */
int syscall_getdents(int fd, void *dp, int count);

/**
 * @brief Handler for syscall getcwd().
 *
 * Get current working directory.
 *
 * @param   buf     buffer (directory path is returned here)
 * @param   sz      size of \a buffer
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getcwd(char *buf, size_t sz);

/**
 * @brief Handler for syscall getrandom().
 *
 * Get a series of random bytes.
 *
 * @param   buf     buffer
 * @param   buflen  size of \a buffer
 * @param   flags   flags (see the GRND_* definitions in sys/random.h)
 * @param   copied  number of bytes copied is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getrandom(void *buf, size_t buflen, unsigned int flags, 
                      ssize_t *copied);


/**********************************
 * Functions defined in access.c
 **********************************/

/**
 * @brief Handler for syscall access().
 *
 * Check file access permissions
 *
 * @param   filename    file to check
 * @param   mode        requested permissions
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_access(char *filename, int mode);

/**
 * @brief Handler for syscall faccessat().
 *
 * Check file access permissions
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   filename    filename to check
 * @param   mode        either F_OK, or one of more of R_OK, W_OK and X_OK
 * @param   flags       zero or one or more of AT_EACCESS and 
 *                        AT_SYMLINK_NOFOLLOW (see fcntl.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_faccessat(int dirfd, char *filename, int mode, int flags);


/**********************************
 * Functions defined in acct.c
 **********************************/

/**
 * @brief Handler for syscall acct().
 *
 * Turn accounting on if 'filename' is an existing file. The system will then 
 * write a record for each process as it terminates, to this file. If 
 * 'filename' is NULL, turn accounting off.
 *
 * This call is restricted to the super-user.
 *
 * See: https://man7.org/linux/man-pages/man2/acct.2.html
 *
 * @param   filename    file to use for accounting, NULL to turn accounting off
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_acct(char *filename);

/**
 * @brief Write task accounting information.
 *
 * Called during task termination.
 *
 * @param   task        pointer to terminating task
 *
 * @return  nothing.
 */
void task_account(struct task_t *task);


/**********************************
 * Functions defined in chdir.c
 **********************************/

/**
 * @brief Handler for syscall chdir().
 *
 * Change the calling task's current working directory.
 *
 * @param   filename    path of new current working directory
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_chdir(char *filename);

/**
 * @brief Handler for syscall fchdir().
 *
 * Change the calling task's current working directory.
 *
 * @param   fd          file descriptor to use as new current working directory
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_fchdir(int fd);

/**
 * @brief Handler for syscall chroot().
 *
 * Change the calling task's root directory.
 *
 * @param   filename    path of new root directory
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_chroot(char *filename);


/**********************************
 * Functions defined in chmod.c
 **********************************/

/**
 * @brief Handler for syscall chmod().
 *
 * Change file access permissions.
 *
 * @param   filename    file name
 * @param   mode        access permissions
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_chmod(char *filename, mode_t mode);

/**
 * @brief Handler for syscall fchmod().
 *
 * Change file access permissions.
 *
 * @param   fd          file descriptor
 * @param   mode        access permissions
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_fchmod(int fd, mode_t mode);

/**
 * @brief Handler for syscall fchmodat().
 *
 * Change file access permissions.
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   pathname    file name
 * @param   mode        access permissions
 * @param   flags       zero or AT_SYMLINK_NOFOLLOW
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_fchmodat(int dirfd, char *pathname, mode_t mode, int flags);


/**********************************
 * Functions defined in chown.c
 **********************************/

/**
 * @brief Handler for syscall chown().
 *
 * Change file ownership.
 *
 * @param   filename    file name
 * @param   uid         user id
 * @param   gid         group id
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_chown(char *filename, uid_t uid, gid_t gid);

/**
 * @brief Handler for syscall lchown().
 *
 * Change file ownership. Similar to syscall_chown() but if \a filename is
 * a symbolic link, this function works on the symbolic link itself, not the
 * file it links to.
 *
 * @param   filename    file name
 * @param   uid         user id
 * @param   gid         group id
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_lchown(char *filename, uid_t uid, gid_t gid);

/**
 * @brief Handler for syscall fchown().
 *
 * Change file ownership.
 *
 * @param   fd          file descriptor
 * @param   uid         user id
 * @param   gid         group id
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_fchown(int fd, uid_t uid, gid_t gid);

/**
 * @brief Handler for syscall fchownat().
 *
 * Change file ownership.
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   pathname    file name
 * @param   uid         user id
 * @param   gid         group id
 * @param   flags       zero or AT_SYMLINK_NOFOLLOW (the other flag, 
 *                        AT_EMPTY_PATH, is not supported yet)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_fchownat(int dirfd, char *pathname, uid_t uid, gid_t gid, 
                     int flags);


/**********************************
 * Functions defined in dup.c
 **********************************/

/**
 * @brief Handler for syscall dup3().
 *
 * Duplicate a file descriptor. Similar to syscall_dup2() except for the
 * extra \a flags argument.
 *
 * @param   oldfd       file descriptor to duplicate
 * @param   newfd       file descriptor to copy \a oldfd into
 * @param   flags       zero or O_CLOEXEC to set the close-on-exec flag
 *
 * @return  zero or positive number on success, -(errno) on failure.
 *
 * @see     syscall_dup2(), syscall_dup()
 */
int syscall_dup3(int oldfd, int newfd, int flags);

/**
 * @brief Handler for syscall dup2().
 *
 * Duplicate a file descriptor.
 *
 * @param   oldfd       file descriptor to duplicate
 * @param   newfd       file descriptor to copy \a oldfd into
 *
 * @return  zero or positive number on success, -(errno) on failure.
 *
 * @see     syscall_dup3(), syscall_dup()
 */
int syscall_dup2(int oldfd, int newfd);

/**
 * @brief Handler for syscall dup().
 *
 * Duplicate a file descriptor using the lowest possible new file descriptor.
 *
 * @param   filedes     file descriptor to duplicate
 *
 * @return  zero or positive number on success, -(errno) on failure.
 *
 * @see     syscall_dup3(), syscall_dup2()
 */
int syscall_dup(int filedes);


/**********************************
 * Functions defined in execve.c
 **********************************/

/**
 * @brief Handler for syscall execve().
 *
 * Execute a program.
 *
 * @param   path    program executable pathname
 * @param   argv    pointer to new program's arguments
 * @param   env     pointer to new program's environment
 *
 * @return  does not return on success, -(errno) on failure.
 *
 * @see     syscall_execveat()
 */
int syscall_execve(char *path, char **argv, char **env);

/**
 * @brief Handler for syscall execveat().
 *
 * Execute a program.
 *
 * @param   dirfd   \a path is interpreted relative to this directory
 * @param   path    program executable pathname
 * @param   argv    pointer to new program's arguments
 * @param   env     pointer to new program's environment
 * @param   flags   zero or AT_SYMLINK_NOFOLLOW (the other flag, 
 *                    AT_EMPTY_PATH, is not supported yet)
 *
 * @return  does not return on success, -(errno) on failure.
 *
 * @see     syscall_execve()
 */
int syscall_execveat(int dirfd, char *path, 
                     char **argv, char **env, int flags);


/**********************************
 * Functions defined in flock.c
 **********************************/

/**
 * @brief Handler for syscall flock().
 *
 * Acquire or release file locks.
 *
 * @param   fd          file descriptor
 * @param   operation   either LOCK_SH, LOCK_EX or LOCK_UN
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_flock(int fd, int operation);


/**********************************
 * Functions defined in fsync.c
 **********************************/

/**
 * @brief Handler for syscall fdatasync().
 *
 * Synchronize a file's in-core state with storage device. 
 * Unlike syscall_fsync(), this function does not synchronize file metadata.
 *
 * @param   fd          file descriptor
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_fsync()
 */
int syscall_fdatasync(int fd);

/**
 * @brief Handler for syscall fsync().
 *
 * Synchronize a file's in-core state with storage device. 
 * Unlike syscall_fdatasync(), this function synchronizes file metadata.
 *
 * @param   fd          file descriptor
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_fdatasync()
 */
int syscall_fsync(int fd);

/**
 * @brief Handler for syscall sync().
 *
 * Commit filesystem caches to disk.
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_syncfs()
 */
int syscall_sync(void);

/**
 * @brief Handler for syscall syncfs().
 *
 * Commit filesystem caches to disk.
 * Unlike syscall_sync(), this function only synchronizes the filesystem 
 * containing the file referred to by the open file descriptor \a fd.
 *
 * @param   fd          file descriptor
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     syscall_sync()
 */
int syscall_syncfs(int fd);


/**********************************
 * Functions defined in groups.c
 **********************************/

/**
 * @brief Check group permissions.
 *
 * Helper function to check whether the calling process belongs to the group
 * identified by the given \a gid. If \a use_rgid is set, the caller's REAL 
 * gid is used instead of their EFFECTIVE gid.
 *
 * @param   gid         group id
 * @param   use_rgid    if non-zero, use the real gid, otherwise use the
 *                        effective gid
 *
 * @return  1 if caller has permission, 0 if not.
 */
int gid_perm(gid_t gid, int use_rgid);

/**
 * @brief Handler for syscall getgroups().
 *
 * Get list of supplementary group IDs.
 *
 * NOTE: supplementary gids that are not used should be
 *       set to -1 (not 0) as zero is the root gid.
 *
 * @param   gidsetsize  number of entries in \a grouplist, if zero it means
 *                        the caller only wants the group count
 * @param   grouplist   supplementary group IDs are returned here
 *
 * @return  group count on success, -(errno) on failure.
 *
 * @see     https://man7.org/linux/man-pages/man2/getgroups.2.html
 */
int syscall_getgroups(int gidsetsize, gid_t grouplist[]);

/**
 * @brief Handler for syscall setgroups().
 *
 * Set list of supplementary group IDs.
 *
 * NOTE: supplementary gids that are not used should be
 *       set to -1 (not 0) as zero is the root gid.
 *
 * @param   ngroups     number of entries in \a grouplist
 * @param   grouplist   supplementary group IDs
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setgroups(int ngroups, gid_t grouplist[]);


/**********************************
 * Functions defined in idle.c
 **********************************/

/**
 * @brief Handler for syscall idle().
 *
 * Run the idle task. Only task #0 can call this function, which doesn't
 * return. Everyone else gets -EPERM.
 *
 * @return  does not return on success, -(errno) on failure.
 */
int syscall_idle(void);


/**************************************
 * Functions defined in kern_sysctl.c
 **************************************/

/**
 * @brief Handler for syscall sysctl().
 *
 * System control function switch.
 *
 * @param   __args      packed syscall arguments (see syscall.h and 
 *                        sys/socket.h)
 *
 * @return  zero or positive number on success, -(errno) on failure.
 */
int syscall_sysctl(struct __sysctl_args *__args);


/**************************************
 * Functions defined in kfork.c
 **************************************/

/**
 * @brief Handler for syscall fork().
 *
 * Create a new task.
 *
 * @return  new task pid in the parent and zero in the new child task 
 *          on success, or -(errno) on failure.
 */
int syscall_fork(void);


/**************************************
 * Functions defined in kill.c
 **************************************/

/**
 * @brief Handler for syscall kill().
 *
 * Send a signal to one or more tasks.
 *
 * @param   pid     process id to send signal to (see signal.h for 
 *                    special values)
 * @param   signum  signal number
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_kill(pid_t pid, int signum);


/**************************************
 * Functions defined in link.c
 **************************************/

/**
 * @brief Handler for syscall link().
 *
 * Make a new name (i.e. hard link) for a file.
 *
 * @param   oldname     existing file name
 * @param   newname     file name of the new link
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_link(char *oldname, char *newname);

/**
 * @brief Handler for syscall linkat().
 *
 * Make a new name (i.e. hard link) for a file.
 *
 * @param   olddirfd    \a oldname is interpreted relative to this directory
 * @param   oldname     existing file name
 * @param   newdirfd    \a newname is interpreted relative to this directory
 * @param   newname     file name of the new link
 * @param   flags       zero or AT_SYMLINK_NOFOLLOW (the other flag, 
 *                        AT_EMPTY_PATH, is not supported yet)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_linkat(int olddirfd, char *oldname,
                   int newdirfd, char *newname, int flags);

/**
 * @brief Handler for syscall unlink().
 *
 * Delete a file name and possibly the file it refers to.
 *
 * @param   pathname    existing file name
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_unlink(char *pathname);

/**
 * @brief Handler for syscall unlinkat().
 *
 * Delete a file name and possibly the file it refers to.
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   pathname    existing file name
 * @param   flags       zero or AT_REMOVEDIR
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_unlinkat(int dirfd, char *pathname, int flags);


/**************************************
 * Functions defined in mkdir.c
 **************************************/

/**
 * @brief Handler for syscall mkdir().
 *
 * Create a new empty directory.
 *
 * @param   pathname    file name
 * @param   mode        access permissions
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_mkdir(char *pathname, mode_t mode);

/**
 * @brief Handler for syscall mkdirat().
 *
 * Create a new empty directory.
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   pathname    file name
 * @param   mode        access permissions
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_mkdirat(int dirfd, char *pathname, mode_t mode);


/**************************************
 * Functions defined in nice.c
 **************************************/

/**
 * @brief Handler for syscall nice().
 *
 * Change the calling task's nice value.
 *
 * @param   incr    if non-zero, amount to increment or decrement current
 *                    nice value by
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_nice(int incr);

/**
 * @brief Handler for syscall getpriority().
 *
 * Get task's scheduling priority.
 *
 * @param   which   one of: PRIO_PROCESS, PRIO_PGRP, or PRIO_USER
 * @param   who     interpreted according to \a which
 * @param   __nice  result is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getpriority(int which, id_t who, int *__nice);

/**
 * @brief Handler for syscall setpriority().
 *
 * Set task's scheduling priority.
 *
 * @param   which   one of: PRIO_PROCESS, PRIO_PGRP, or PRIO_USER
 * @param   who     interpreted according to \a which
 * @param   value   priority value to set
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setpriority(int which, id_t who, int value);


/**************************************
 * Functions defined in open.c
 **************************************/

/**
 * @brief Handler for syscall open().
 *
 * Open or create a new file.
 *
 * @param   pathname        path name
 * @param   flags           open flags (see the O_* definitions in fcntl.h)
 * @param   mode            file mode and access permissions
 *
 * @return  zero or positive file descriptor on success, -(errno) on failure.
 *
 * @see     syscall_openat()
 */
int syscall_open(char *pathname, int flags, mode_t mode);

/**
 * @brief Handler for syscall openat().
 *
 * Open or create a new file.
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   pathname    path name
 * @param   flags       open flags (see the O_* definitions in fcntl.h)
 * @param   mode        file mode and access permissions
 *
 * @return  zero or positive file descriptor on success, -(errno) on failure.
 *
 * @see     syscall_open()
 */
int syscall_openat(int dirfd, char *pathname, int flags, mode_t mode);

/**
 * @brief Open a temporary file descriptor.
 *
 * Helper function to create a new open file descriptor and assign the given
 * \a node to it.
 *
 * @param   node        file node
 *
 * @return  zero or positive file descriptor on success, -(errno) on failure.
 */
int open_tmp_fd(struct fs_node_t *node);


/**************************************
 * Functions defined in pipe.c
 **************************************/

/**
 * @brief Handler for syscall pipe().
 *
 * Create a pipe.
 *
 * @param   filedes     two file descriptors are returned here: filedes[0]
 *                        refers to the reading end, filedes[1] refers to the
 *                        writing end
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_pipe(int *filedes);

/**
 * @brief Handler for syscall pipe2().
 *
 * Create a pipe.
 *
 * @param   filedes     two file descriptors are returned here: filedes[0]
 *                        refers to the reading end, filedes[1] refers to the
 *                        writing end
 * @param   flags       zero or a combination of one or more of: O_CLOEXEC,
 *                        O_NONBLOCK, O_DIRECT (the last one is not yet
 *                        implemented)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_pipe2(int *filedes, int flags);


/**************************************
 * Functions defined in poll.c
 **************************************/

/**
 * @brief Handler for syscall poll().
 *
 * Switch function for I/O polling operations.
 *
 * @param   fds         file descriptors to check for events
 * @param   nfds        number of elements in \a fds
 * @param   timeout     if zero, return immediately; if negative, wait
 *                        indefinitely; if positive, timeout for the wait
 *
 * @return  zero or positive number on success, -(errno) on failure.
 */
int syscall_poll(struct pollfd *fds, nfds_t nfds, int timeout);

/**
 * @brief Handler for syscall ppoll().
 *
 * Switch function for I/O polling operations.
 * A call to ppoll() is atomically similar to performing the following:
 *
 * \code
 *    sigset_t origmask;
 *    int timeout;
 *
 *    timeout = (tmo_p == NULL) ? -1 :
 *                  (tmo_p->tv_sec * 1000 + tmo_p->tv_nsec / 1000000);
 *    pthread_sigmask(SIG_SETMASK, &sigmask, &origmask);
 *    ready = poll(&fds, nfds, timeout);
 *    pthread_sigmask(SIG_SETMASK, &origmask, NULL);
 * \endcode
 *
 * @param   fds         file descriptors to check for events
 * @param   nfds        number of elements in \a fds
 * @param   tmo_p       if not NULL, timeout for the wait
 * @param   sigmask     signal mask to set before polling
 *
 * @return  zero or positive number on success, -(errno) on failure.
 */
int syscall_ppoll(struct pollfd *fds, nfds_t nfds,
                  struct timespec *tmo_p, sigset_t *sigmask);


/**************************************
 * Functions defined in read.c
 **************************************/

/**
 * @brief Handler for syscall read().
 *
 * Read at most \a count bytes from the file referred to by the given file
 * descriptor \a _fd into \a buf.
 *
 * @param   _fd         file descriptor
 * @param   buf         buffer to read data into
 * @param   count       maximum amount of bytes to read into \a buf
 * @param   copied      number of bytes read is returned here
 *
 * @return zero on success, -(errno) on failure
 */
int syscall_read(int _fd, unsigned char *buf, size_t count, ssize_t *copied);

/**
 * @brief Handler for syscall pread().
 *
 * Read at most \a count bytes from the file referred to by the given file
 * descriptor \a _fd into \a buf, starting at the given \a offset in file.
 *
 * @param   _fd         file descriptor
 * @param   buf         buffer to read data into
 * @param   count       maximum amount of bytes to read into \a buf
 * @param   offset      offset in file to begin reading from
 * @param   copied      number of bytes read is returned here
 *
 * @return zero on success, -(errno) on failure
 */
int syscall_pread(int _fd, void *buf, size_t count, off_t offset, 
                  ssize_t *copied);

/**
 * @brief Handler for syscall readv().
 *
 * Read at most \a count bytes from the file referred to by the given file
 * descriptor \a _fd into multiple buffers.
 *
 * @param   _fd         file descriptor
 * @param   iov         buffers to read data into (see sys/uio.h)
 * @param   count       maximum amount of bytes to read into buffers
 * @param   copied      number of bytes read is returned here
 *
 * @return zero on success, -(errno) on failure
 */
int syscall_readv(int _fd, struct iovec *iov, int count, ssize_t *copied);

/**
 * @brief Handler for syscall preadv().
 *
 * Read at most \a count bytes from the file referred to by the given file
 * descriptor \a _fd into multiple buffers, starting at the given \a offset 
 * in file.
 *
 * @param   _fd         file descriptor
 * @param   iov         buffers to read data into (see sys/uio.h)
 * @param   count       maximum amount of bytes to read into buffers
 * @param   offset      offset in file to begin reading from
 * @param   copied      number of bytes read is returned here
 *
 * @return zero on success, -(errno) on failure
 */
int syscall_preadv(int _fd, struct iovec *iov, int count,
                   off_t offset, ssize_t *copied);


/**************************************
 * Functions defined in rename.c
 **************************************/

/**
 * @brief Handler for syscall rename().
 *
 * Rename a file.
 *
 * @param   oldname     existing file name
 * @param   newname     new file name
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_rename(char *oldname, char *newname);

/**
 * @brief Handler for syscall renameat().
 *
 * Rename a file.
 *
 * @param   olddirfd    \a oldname is interpreted relative to this directory
 * @param   oldname     existing file name
 * @param   newdirfd    \a newname is interpreted relative to this directory
 * @param   newname     new file name
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_renameat(int olddirfd, char *oldname, int newdirfd, char *newname);


/**************************************
 * Functions defined in sched.c
 **************************************/

/**
 * @brief Handler for syscall sched_rr_get_interval().
 *
 * Get the SCHED_RR interval for the named process.
 *
 * @param   pid     process id (or 0 to specify the calling process)
 * @param   tp      round-robin time quantum is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sched_rr_get_interval(pid_t pid, struct timespec *tp);

/**
 * @brief Handler for syscall sched_getparam().
 *
 * Get scheduling parameters for the named process.
 *
 * @param   pid     process id (or 0 to specify the calling process)
 * @param   param   scheduling params are returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sched_getparam(pid_t pid, struct sched_param *param);

/**
 * @brief Handler for syscall sched_setparam().
 *
 * Set scheduling parameters for the named process.
 *
 * @param   pid     process id (or 0 to specify the calling process)
 * @param   param   scheduling params to set
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sched_setparam(pid_t pid, struct sched_param *param);

/**
 * @brief Handler for syscall sched_getscheduler().
 *
 * Get scheduling policy for the named process.
 *
 * @param   pid     process id (or 0 to specify the calling process)
 *
 * @return  sched policy on success, -(errno) on failure.
 */
int syscall_sched_getscheduler(pid_t pid);

/**
 * @brief Handler for syscall sched_setscheduler().
 *
 * Set scheduling policy and/or parameters for the named process.
 *
 * @param   pid     process id (or 0 to specify the calling process)
 * @param   policy  scheduling policy (SCHED_FIFO, SCHED_RR, SCHED_OTHER)
 * @param   param   if not NULL, scheduling params to set
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sched_setscheduler(pid_t pid, int policy, 
                               struct sched_param *param);

/**
 * @brief Handler for syscall sched_get_priority_max().
 *
 * Get the maximum priority value that can be used with the given scheduling 
 * \a policy.
 *
 * @param   policy  scheduling policy (SCHED_FIFO, SCHED_RR, SCHED_OTHER)
 *
 * @return  zero or positive number on success, -(errno) on failure.
 */
int syscall_sched_get_priority_max(int policy);

/**
 * @brief Handler for syscall sched_get_priority_min().
 *
 * Get the minimum priority value that can be used with the given scheduling 
 * \a policy.
 *
 * @param   policy  scheduling policy (SCHED_FIFO, SCHED_RR, SCHED_OTHER)
 *
 * @return  zero or positive number on success, -(errno) on failure.
 */
int syscall_sched_get_priority_min(int policy);

/**
 * @brief Handler for syscall sched_yield().
 *
 * Yield the processor.
 *
 * @return  zero always.
 */
int syscall_sched_yield(void);


/**************************************
 * Functions defined in stat.c
 **************************************/

/**
 * @brief Handler for syscall stat().
 *
 * Get file info.
 *
 * @param   filename    file name
 * @param   statbuf     file info is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_stat(char *filename, struct stat *statbuf);

/**
 * @brief Handler for syscall lstat().
 *
 * Get file info. Similar to syscall_stat() but if \a filename is
 * a symbolic link, this function works on the symbolic link itself, not the
 * file it links to.
 *
 * @param   filename    file name
 * @param   statbuf     file info is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_lstat(char *filename, struct stat *statbuf);

/**
 * @brief Handler for syscall fstat().
 *
 * Get file info.
 *
 * @param   fd          file descriptor
 * @param   statbuf     file info is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_fstat(int fd, struct stat *statbuf);

/**
 * @brief Handler for syscall fstat().
 *
 * Get file info.
 *
 * @param   dirfd       \a filename is interpreted relative to this directory
 * @param   filename    file name
 * @param   statbuf     file info is returned here
 * @param   flags       zero or AT_SYMLINK_NOFOLLOW
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_fstatat(int dirfd, char *filename,
                    struct stat *statbuf, int flags);


/**************************************
 * Functions defined in statfs.c
 **************************************/

/**
 * @brief Handler for syscall statfs().
 *
 * Get filesystem statistics.
 *
 * @param   path        name of a file within the mounted filesystem
 * @param   buf         filesystem statistics are returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_statfs(char *path, struct statfs *buf);

/**
 * @brief Handler for syscall fstatfs().
 *
 * Get filesystem statistics.
 *
 * @param   fd          file descriptor referring to a file within the 
 *                        mounted filesystem
 * @param   buf         filesystem statistics are returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_fstatfs(int fd, struct statfs *buf);

/**
 * @brief Handler for syscall ustat().
 *
 * Get filesystem statistics.
 *
 * @param   dev         device id of a device containing a mounted filesystem
 * @param   ubuf        filesystem statistics are returned here
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see: https://man7.org/linux/man-pages/man2/ustat.2.html
 */
int syscall_ustat(dev_t dev, struct ustat *ubuf);


/**************************************
 * Functions defined in sysinfo.c
 **************************************/

/**
 * @brief Handler for syscall sysinfo().
 *
 * Get system info.
 *
 * @param   info        buffer to be filled with sysinfo
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sysinfo(struct sysinfo *info);


/**************************************
 * Functions defined in symlink.c
 **************************************/

/**
 * @brief Handler for syscall symlink().
 *
 * Make a new name (soft link) for a file.
 *
 * @param   target      string to write to the new file
 * @param   linkpath    file name of the new link
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_symlink(char *target, char *linkpath);

/**
 * @brief Handler for syscall symlinkat().
 *
 * Make a new name (soft link) for a file.
 *
 * @param   target      string to write to the new file
 * @param   newdirfd    \a linkpath is interpreted relative to this directory
 * @param   linkpath    file name of the new link
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_symlinkat(char *target, int newdirfd, char *linkpath);

/**
 * @brief Handler for syscall readlink().
 *
 * Read the contents of a symbolic link.
 *
 * @param   pathname    file name
 * @param   buf         buffer to hold the contents of the symbolic link
 * @param   bufsize     size of \a buffer
 * @param   __copied    number of bytes copied to buf is stored here (the C 
 *                        library will return this as the result of the call)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_readlink(char *pathname, char *buf,
                     size_t bufsize, ssize_t *__copied);

/**
 * @brief Handler for syscall readlinkat().
 *
 * Read the contents of a symbolic link.
 *
 * @param   dirfd       \a pathname is interpreted relative to this directory
 * @param   pathname    file name
 * @param   buf         buffer to hold the contents of the symbolic link
 * @param   bufsize     size of \a buffer
 * @param   __copied    number of bytes copied to buf is stored here (the C 
 *                        library will return this as the result of the call)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_readlinkat(int dirfd, char *pathname, char *buf,
                       size_t bufsize, ssize_t *__copied);

/**
 * @brief Follow a symbolic link.
 *
 * Helper function that reads the contents of a symlink, opens the target and
 * returns the opened target file node.
 *
 * @param   link    the symlink we want to follow
 * @param   parent  parent directory
 * @param   flags   flags to pass to vfs_open() when opening the target
 * @param   target  the loaded symlink target will be stored here
 *
 * @return  zero on success, -(errno) on failure.
 */
int follow_symlink(struct fs_node_t *link, struct fs_node_t *parent,
                   int flags, struct fs_node_t **target);

/**
 * @brief Read the contents of a symbolic link.
 *
 * Helper function that reads the contents of a symlink (effectively what the
 * readlink syscall does, except this function needs a file node pointer 
 * instead of a path).
 * If the symlink's target is longer than bufsz, the target is truncated.
 * No null-terminating byte is added to the buffer.
 *
 * @param   link    the symlink we want to read
 * @param   buf     the symlink target will be stored here
 * @param   bufsz   \a buf's size
 * @param   kernel  set if the caller is a kernel function (i.e. \a buf 
 *                    address is in kernel memory), 0 if \a buf is a 
 *                    userspace address
 *
 * @return  number of bytes read on success, -(errno) on failure
 */
int read_symlink(struct fs_node_t *link, char *buf, size_t bufsz, int kernel);


/*********************************************
 * Functions defined in syscall_dispatcher.S
 *********************************************/

#ifdef __x86_64__
extern void syscall_entry64(void);
#else
extern void syscall_entry(void);
#endif

extern void resume_user(void);
extern void enter_user(void);


/**************************************
 * Functions defined in truncate.c
 **************************************/

/**
 * @brief Handler for syscall truncate().
 *
 * Truncate a file to a specified size.
 *
 * @param   pathname    file name
 * @param   length      new file size
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_truncate(char *pathname, off_t length);

/**
 * @brief Handler for syscall ftruncate().
 *
 * Truncate a file to a specified size.
 *
 * @param   fd          file descriptor
 * @param   length      new file size
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_ftruncate(int fd, off_t length);


/**************************************
 * Functions defined in utime.c
 **************************************/

/**
 * @brief Handler for syscall utime().
 *
 * Change file last access and modification times.
 *
 * @param   filename    file name
 * @param   times       if NULL, times are set to current time, otherwise 
 *                        they are set to the times specified in this struct
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_utime(char *filename, struct utimbuf *times);

/**
 * @brief Handler for syscall utimes().
 *
 * Change file last access and modification times.
 *
 * @param   filename    file name
 * @param   times       if NULL, times are set to current time, otherwise 
 *                        they are set to the times specified in this struct
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_utimes(char *filename, struct timeval *times);

/**
 * @brief Handler for syscall futimesat().
 *
 * Change file last access and modification times.
 *
 * @param   dirfd       \a filename is interpreted relative to this directory
 * @param   filename    file name
 * @param   times       if NULL, times are set to current time, otherwise 
 *                        they are set to the times specified in this struct
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_futimesat(int dirfd, char *filename, struct timeval *times);


/**************************************
 * Functions defined in wait.c
 **************************************/

/**
 * @brief Handler for syscall waitpid().
 *
 * Wait for process to change state.
 *
 * @param   pid         process id (see `man wait` for special values)
 * @param   status      status is returned here
 * @param   options     see sys/wait.h for possible values
 *
 * @return  process id (or zero if no process has changed status and WNOHANG 
 *            is specified in \a options) on success, -(errno) on failure.
 */
int syscall_waitpid(pid_t pid, int *status, int options);

/**
 * @brief Handler for syscall wait4().
 *
 * Wait for process to change state.
 *
 * @param   pid         process id (interpreted according to \a idtype)
 * @param   status      status is returned here
 * @param   options     see sys/wait.h for possible values
 * @param   rusage      process resource usage is returned here
 *
 * @return  process id (or zero if no process has changed status and WNOHANG 
 *            is specified in \a options) on success, -(errno) on failure.
 */
int syscall_wait4(pid_t pid, int *status,
                  int options, struct rusage *rusage);

/**
 * @brief Handler for syscall waitid().
 *
 * Wait for process to change state.
 *
 * @param   idtype      one of: P_PID, P_PGID, P_ALL
 * @param   id          process id (interpreted according to \a idtype)
 * @param   infop       status info is returned here
 * @param   options     see sys/wait.h for possible values
 *
 * @return  process id (or zero if no process has changed status and WNOHANG 
 *            is specified in \a options) on success, -(errno) on failure.
 */
int syscall_waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);


/**************************************
 * Functions defined in write.c
 **************************************/

/**
 * @brief Handler for syscall write().
 *
 * Write at most \a count bytes to the file referred to by the given file
 * descriptor \a _fd. Writing is done from \a buf.
 *
 * @param   _fd         file descriptor
 * @param   buf         buffer to write data from
 * @param   count       maximum amount of bytes to write from \a buf
 * @param   copied      number of bytes written is returned here
 *
 * @return zero on success, -(errno) on failure
 */
int syscall_write(int _fd, unsigned char *buf, size_t count, ssize_t *copied);

/**
 * @brief Handler for syscall pwrite().
 *
 * Write at most \a count bytes to the file referred to by the given file
 * descriptor \a _fd. Writing is done from \a buf, starting at the given
 * \a offset in file.
 *
 * @param   _fd         file descriptor
 * @param   buf         buffer to write data from
 * @param   count       maximum amount of bytes to write from \a buf
 * @param   offset      offset in file to begin writing to
 * @param   copied      number of bytes written is returned here
 *
 * @return zero on success, -(errno) on failure
 */
int syscall_pwrite(int _fd, void *buf, size_t count, off_t offset, 
                   ssize_t *copied);

/**
 * @brief Handler for syscall writev().
 *
 * Write at most \a count bytes to the file referred to by the given file
 * descriptor \a _fd. Writing is done from multiple buffers.
 *
 * @param   _fd         file descriptor
 * @param   iov         buffers to write data from (see sys/uio.h)
 * @param   count       maximum amount of bytes to write from buffers
 * @param   copied      number of bytes written is returned here
 *
 * @return zero on success, -(errno) on failure
 */
int syscall_writev(int _fd, struct iovec *iov, int count, ssize_t *copied);

/**
 * @brief Handler for syscall pwritev().
 *
 * Write at most \a count bytes to the file referred to by the given file
 * descriptor \a _fd. Writing is done from multiple buffers, starting at
 * the given \a offset in file.
 *
 * @param   _fd         file descriptor
 * @param   iov         buffers to write data from (see sys/uio.h)
 * @param   count       maximum amount of bytes to write from buffers
 * @param   offset      offset in file to begin writing to
 * @param   copied      number of bytes written is returned here
 *
 * @return zero on success, -(errno) on failure
 */
int syscall_pwritev(int _fd, struct iovec *iov, int count,
                    off_t offset, ssize_t *copied);

#endif      /* __KERNEL_SYSCALL_H__ */
