/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: acct.c
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
 *  \file acct.c
 *
 *  Functions for working with task accounting.
 */

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/acct.h>
#include <sys/wait.h>
#include <kernel/syscall.h>
#include <kernel/vfs.h>


static struct fs_node_t *acctnode = NULL;
static struct file_t acctfile;
static volatile struct kernel_mutex_t acctlock = { 0, };

/*
 * Handler for syscall acct().
 *
 * Turn accounting on if 'filename' is an existing file. The system will then 
 * write a record for each process as it terminates, to this file. If 
 * 'filename' is NULL, turn accounting off.
 *
 * This call is restricted to the super-user.
 *
 * See: https://man7.org/linux/man-pages/man2/acct.2.html
 */
long syscall_acct(char *filename)
{
    long res;
    static int flags = O_RDWR | O_APPEND;
    struct fs_node_t *node;

    if(!suser(this_core->cur_task))
    {
        return -EPERM;
    }

    // turn off
    if(!filename)
    {
        kernel_mutex_lock(&acctlock);

        if(acctnode)
        {
            release_node(acctnode);
            acctnode = NULL;
        }
        
        kernel_mutex_unlock(&acctlock);
        return 0;
    }
    
    // turn on
    if((res = vfs_open(filename, flags, 0666, AT_FDCWD, &node,
                       OPEN_USER_CALLER | OPEN_CREATE_DENTRY)) != 0)
    {
        return res;
    }
    
    if(S_ISDIR(node->mode))
    {
        release_node(node);
        return -EISDIR;
    }

    // can't write if the filesystem was mount readonly
    /*
    if((dinfo = get_mount_info(node->dev)) && (dinfo->mountflags & MS_RDONLY))
    {
        release_node(node);
        return -EROFS;
    }
    */
    
    kernel_mutex_lock(&acctlock);

    if(acctnode)
    {
        release_node(acctnode);
    }

    acctnode = node;
    acctfile.flags = flags;
    acctfile.pos = 0;

    kernel_mutex_unlock(&acctlock);
    
    return 0;
}


/*
 * Write task accounting information.
 */
void task_account(struct task_t *task)
{
    struct acct_v3 acct;
    size_t sz;
    off_t fpos;
    
    if(!acctnode)
    {
        return;
    }
    
    // always append to this file
    fpos = acctnode->size;
    
    /*
     * TODO: Handle the AFORK & ASU flags (see sys/acct.h).
     */
    acct.ac_flag = (WIFSIGNALED(task->exit_status) ? AXSIG : 0) |
                   (WCOREDUMP(task->exit_status) ? ACORE : 0);

    acct.ac_version = ACCT_VERSION;
    acct.ac_tty = task->ctty;
    acct.ac_exitcode = task->exit_status;
    acct.ac_uid = task->uid;
    acct.ac_gid = task->gid;
    acct.ac_pid = task->pid;
    acct.ac_ppid = task->parent ? task->parent->pid : 1;

    // TODO: process creation time
    acct.ac_btime = 0;

    // TODO: process elapsed time
    acct.ac_etime = 0;
    
    acct.ac_utime = task->user_time + task->children_user_time;
    acct.ac_stime = task->sys_time  + task->children_sys_time ;

    // TODO: process average memory usage
    acct.ac_mem = 0;

    acct.ac_io = 0;
    acct.ac_rw = 0;
    acct.ac_minflt = task->minflt + task->children_minflt;
    acct.ac_majflt = task->majflt + task->children_majflt;
    acct.ac_swaps = 0;

    sz = (TASK_COMM_LEN > ACCT_COMM) ? ACCT_COMM : TASK_COMM_LEN;
    memcpy(acct.ac_comm, task->command, sz);
    acct.ac_comm[sz - 1] = '\0';
    
    kernel_mutex_lock(&acctlock);
    vfs_write_node(acctnode, &fpos, (unsigned char *)&acct, sizeof(acct), 1);
    kernel_mutex_unlock(&acctlock);
}

