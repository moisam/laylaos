/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: procfs.c
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
 *  \file procfs.c
 *
 *  This file implements procfs filesystem functions, which provide access to
 *  the procfs virtual filesystem.
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref procfs_ops structure.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/clock.h>
#include <kernel/dev.h>
#include <kernel/pci.h>
#include <kernel/fio.h>
#include <fs/tmpfs.h>
#include <fs/procfs.h>
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/devpts.h>
#include <fs/options.h>
#include <fs/magic.h>
#include <mm/kheap.h>

/*
 * The procfs filesystem, traditionally mounted on /proc.
 *
 * This is a pseudo-filesystem, which means it doesn't reside on disk. All
 * information is stored in memory and lost on reboot. It is intended as a
 * way for user programs to get information about kernel memory and
 * internal data structures. Linux also allows user programs to write to
 * some procfs files, which effectively modifies kernel behaviour in real-
 * time. We currently don't support this, i.e. procfs is read-only.
 *
 * The procfs filesystem doesn't have inode structures or disk blocks. To
 * enable it to work with our VFS interface, we give each file/directory a
 * madeup inode number. Each inode number encodes the file/directory it
 * refers to, so that reads (and in the future, writes) lead to the right
 * file. An inode number is generated using the following formula:
 *
 *     inode = (((file) << 16) | ((subdir) << 8) | (dir))
 *
 * The inode number consists of the following fields, which are interpreted
 * according to the file/directory the inode refers to:
 *
 * file/dir                         dir         subdir          file
 * -------------------------        ---         -----------     ----
 * /proc                            1           0               0
 *   files under /proc              1           0               [1+]
 * /proc/bus                        2           0               0
 *   files under /proc/bus          2           0               [1+]
 * /proc/bus/pci                    3           0               0
 *   files under /proc/bus/pci      3           0               [1+]
 *   dirs under /proc/bus/pci       3           [1+]            0
 * /proc/sys                        4           0               0
 *   files under /proc/sys          4           TODO            TODO
 * /proc/tty                        5           0               0
 *   files under /proc/tty          5           TODO            TODO
 * /proc/net                        6           0               0
 *   files under /proc/net          6           0               [1+]
 * /proc/[pid]                      7           task-index*     0
 *   files under /proc/[pid]        7           task-index*     [1+]
 * /proc/[pid]/fd                   8           task-index*     0
 *   files under /proc/[pid]/fd     8           task-index*     [1+]
 * /proc/[pid]/task                 9           task-index*     0
 *   dirs under /proc/[pid]/task    9           task-index*     [1+]
 *
 * The task-index field is the task index within the global task table, when
 * it is accessed as an array. So the first task in the array has a task-index
 * of 0, and the last of NR_TASKS - 1. Note that a task's task-index is not
 * the same as its pid, as it refers to the task's slot in the task table, not
 * its identity. This was chosen as the task table is of finite and limited
 * size, currently 1024, which can be represented in 2 bytes, whereas pids can
 * reach high numbers and need more storage space (pid_t is 4-bytes long on x86).
 */

// defined in drivers/pci.c
extern struct pci_bus_t *first_pci_bus;


#define PROCFS_BLOCK_SIZE               512
#define PROCFS_ROOT_INODE               MAKE_PROCFS_INODE(DIR_PROC, 0, 0)

#define PROCFS_DEV_MIN                  0
#define PROCFS_DEV_MAJ                  243

// device id for procfs
dev_t PROCFS_DEVID = TO_DEVID(PROCFS_DEV_MAJ, PROCFS_DEV_MIN);

// make sure procfs is init'ed only once
static int procfs_inited = 0;

struct fs_node_t *procfs_root;

// filesystem operations
struct fs_ops_t procfs_ops =
{
    // inode operations
    .read_inode = procfs_read_inode,
    .write_inode = procfs_write_inode,
    //.trunc_inode = NULL,
    .alloc_inode = NULL,
    .free_inode = NULL,
    .bmap = NULL,

    .read_symlink = procfs_read_symlink,
    .write_symlink = procfs_write_symlink,
    
    // directory operations
    .finddir = procfs_finddir,
    .finddir_by_inode = procfs_finddir_by_inode,
    //.readdir = procfs_readdir,
    .addir = NULL,
    .mkdir = NULL,
    .deldir = NULL,
    .dir_empty = NULL,
    .getdents = procfs_getdents,
    
    //.read = procfs_read,
    //.write = procfs_write,
    
    // device operations
    .mount = procfs_mount,
    .umount = NULL,
    .read_super = procfs_read_super,
    .write_super = NULL,
    .put_super = procfs_put_super,
    .ustat = procfs_ustat,
    .statfs = procfs_statfs,
};


struct procfs_entry_t
{
    char *name;
    mode_t mode;
    time_t atime, mtime, ctime;
    size_t (*read_file)(char **);  // function to read proc file contents
};

struct procfs_pid_entry_t
{
    char *name;
    mode_t mode;
    time_t atime, mtime, ctime;
    size_t (*read_file)(struct task_t *, char **);  // function to read proc
                                                    //   file contents
};

#define arr_count(a)        (int)(sizeof(a) / sizeof(a[0]))

struct procfs_entry_t procfs_root_entries[] =
{
    { "."               , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
    { ".."              , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_BUS_DIR        2
    { "bus"             , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_SYS_DIR        3
    { "sys"             , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_TTY_DIR        4
    { "tty"             , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_NET_DIR        5
    { "net"             , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_BUFFERS        6
    { "buffers"         , PROCFS_FILE_MODE, 0, 0, 0, get_buffer_info, },
#define PROC_CMDLINE        7
    { "cmdline"         , PROCFS_FILE_MODE, 0, 0, 0, NULL, },
#define PROC_CPUINFO        8
    { "cpuinfo"         , PROCFS_FILE_MODE, 0, 0, 0, detect_cpu, },
#define PROC_DEVICES        9
    { "devices"         , PROCFS_FILE_MODE, 0, 0, 0, get_device_list, },
#define PROC_FILESYSTEMS    10
    { "filesystems"     , PROCFS_FILE_MODE, 0, 0, 0, get_fs_list, },
#define PROC_INTERRUPTS     11
    { "interrupts"      , PROCFS_FILE_MODE, 0, 0, 0, get_interrupt_info, },
#define PROC_LOADAVG        12
    { "loadavg"         , PROCFS_FILE_MODE, 0, 0, 0, get_loadavg, },
#define PROC_MEMINFO        13
    { "meminfo"         , PROCFS_FILE_MODE, 0, 0, 0, get_meminfo, },
#define PROC_MODULES        14
    { "modules"         , PROCFS_FILE_MODE, 0, 0, 0, get_modules, },
#define PROC_MOUNTS         15
    { "mounts"          , PROCFS_FILE_MODE, 0, 0, 0, get_mounts, },
#define PROC_PARTITIONS     16
    { "partitions"      , PROCFS_FILE_MODE, 0, 0, 0, get_partitions, },
#define PROC_STAT           17
    { "stat"            , PROCFS_FILE_MODE, 0, 0, 0, get_sysstat, },
#define PROC_TIMER_LIST     18
    { "timer_list"      , PROCFS_FILE_MODE, 0, 0, 0, NULL, },
#define PROC_UPTIME         19
    { "uptime"          , PROCFS_FILE_MODE, 0, 0, 0, get_uptime, },
#define PROC_VERSION        20
    { "version"         , PROCFS_FILE_MODE, 0, 0, 0, get_version, },
#define PROC_VMSTAT         21
    { "vmstat"          , PROCFS_FILE_MODE, 0, 0, 0, get_vmstat, },
#define PROC_KSYMS          22
    { "ksyms"           , PROCFS_FILE_MODE, 0, 0, 0, get_ksyms, },
#define PROC_SYSCALLS       23
    { "syscalls"        , PROCFS_FILE_MODE, 0, 0, 0, get_syscalls, },
#define PROC_SELF           24
    { "self"            , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
#define PROC_THREAD_SELF    25
    { "thread-self"     , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
};

#define procfs_root_entry_count     arr_count(procfs_root_entries)

struct procfs_entry_t procfs_bus_entries[] =
{
    { "."               , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
    { ".."              , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
    { "pci"             , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
};

#define procfs_bus_entry_count      arr_count(procfs_bus_entries)

struct procfs_entry_t procfs_bus_pci_entries[] =
{
    { "."               , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
    { ".."              , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_BUS_PCI_DEVICES    2
    { "devices"         , PROCFS_FILE_MODE, 0, 0, 0, get_pci_device_list, },
};

#define procfs_bus_pci_entry_count  arr_count(procfs_bus_pci_entries)

struct procfs_entry_t procfs_net_entries[] =
{
    { "."               , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
    { ".."              , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_NET_ARP            2
    { "arp"             , PROCFS_FILE_MODE, 0, 0, 0, get_arp_list, },
#define PROC_NET_DEV            3
    { "dev"             , PROCFS_FILE_MODE, 0, 0, 0, get_net_dev_stats, },
#define PROC_NET_TCP            4
    { "tcp"             , PROCFS_FILE_MODE, 0, 0, 0, get_net_tcp, },
#define PROC_NET_UDP            5
    { "udp"             , PROCFS_FILE_MODE, 0, 0, 0, get_net_udp, },
#define PROC_NET_UNIX           6
    { "unix"            , PROCFS_FILE_MODE, 0, 0, 0, get_net_unix, },
#define PROC_NET_RAW            7
    { "raw"             , PROCFS_FILE_MODE, 0, 0, 0, get_net_raw, },
#define PROC_NET_RESOLV         8
    { "resolv.conf"     , PROCFS_FILE_MODE, 0, 0, 0, get_dns_list, },
};

#define procfs_net_entry_count      arr_count(procfs_net_entries)

struct procfs_entry_t procfs_tty_entries[] =
{
    { "."               , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
    { ".."              , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_TTY_DRIVERS        2
    { "drivers"         , PROCFS_FILE_MODE, 0, 0, 0, get_tty_driver_list, },
};

#define procfs_tty_entry_count      arr_count(procfs_tty_entries)

struct procfs_pid_entry_t procfs_pid_entries[] =
{
    { "."               , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
    { ".."              , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_PID_CMDLINE    2
    { "cmdline"         , PROCFS_FILE_MODE, 0, 0, 0, NULL, },
#define PROC_PID_COMM       3
    { "comm"            , PROCFS_FILE_MODE, 0, 0, 0, NULL, },
#define PROC_PID_CWD        4
    { "cwd"             , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
#define PROC_PID_ENVIRON    5
    { "environ"         , PROCFS_FILE_MODE, 0, 0, 0, NULL, },
#define PROC_PID_EXE        6
    { "exe"             , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
#define PROC_PID_FD         7
    { "fd"              , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_PID_IO         8
    { "io"              , PROCFS_FILE_MODE, 0, 0, 0, get_task_io, },
#define PROC_PID_LIMITS     9
    { "limits"          , PROCFS_FILE_MODE, 0, 0, 0, get_task_rlimits, },
#define PROC_PID_MAPS       10
    { "maps"            , PROCFS_FILE_MODE, 0, 0, 0, get_task_mmaps, },
#define PROC_PID_MEM        11
    { "mem"             , PROCFS_FILE_MODE, 0, 0, 0, NULL, },
#define PROC_PID_MOUNTS     12
    { "mounts"          , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
#define PROC_PID_ROOT       13
    { "root"            , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
#define PROC_PID_STAT       14
    { "stat"            , PROCFS_FILE_MODE, 0, 0, 0, get_task_stat, },
#define PROC_PID_STATM      15
    { "statm"           , PROCFS_FILE_MODE, 0, 0, 0, get_task_statm, },
#define PROC_PID_STATUS     16
    { "status"          , PROCFS_FILE_MODE, 0, 0, 0, get_task_status, },
#define PROC_PID_TASK       17
    { "task"            , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_PID_TIMERS     18
    { "timers"          , PROCFS_FILE_MODE, 0, 0, 0, get_task_posix_timers, },
};

#define procfs_pid_entry_count      arr_count(procfs_pid_entries)


/*
 * Initialize procfs.
 */
void procfs_init(void)
{
    fs_register("procfs", &procfs_ops);
    //procfs_create();
    
    int maj = MAJOR(PROCFS_DEVID);
    
    //bdev_tab[maj].strategy = procfs_strategy;
    bdev_tab[maj].ioctl = procfs_ioctl;
    bdev_tab[maj].select = devfs_select;
    bdev_tab[maj].poll = devfs_poll;
}


/*
 * Create the procfs virtual filesystem.
 * Should be called once, on system startup.
 *
 * Returns:
 *    root node of procfs
 */
struct fs_node_t *procfs_create(void)
{
    size_t i;
    time_t t = now();
    
    if(procfs_inited)
    {
        printk("procfs: trying to re-init procfs\n");
        return procfs_root;
    }
    
    if(!(procfs_root = get_empty_node()))
    {
        printk("procfs: failed to create procfs\n");
        return NULL;
    }

    procfs_root->ops = &procfs_ops;
    procfs_root->mode = S_IFDIR | 0555;
    procfs_root->links = procfs_root_entry_count;
    procfs_root->refs = 1;
    procfs_root->inode = PROCFS_ROOT_INODE;
    procfs_root->ctime = t;
    procfs_root->mtime = t;
    procfs_root->atime = t;

    // some user programs that call getdents() don't read past the directory's
    // size, so we estimate a size large enough to ensure someone who reads
    // the root directory gets all the entries they need (we use an average of
    // 8 chars per entry name just for approximation).
    procfs_root->size = (sizeof(struct dirent) + 8) *
                            (procfs_root_entry_count + NR_TASKS);

    // use one of the reserved dev ids
    procfs_root->dev = PROCFS_DEVID;
    
#define set_times(entries, count, t)        \
    for(i = 0; i < count; i++)              \
    {                                       \
        entries[i].ctime = t;               \
        entries[i].atime = t;               \
        entries[i].mtime = t;               \
    }
    
    set_times(procfs_root_entries, procfs_root_entry_count, t);
    set_times(procfs_bus_entries, procfs_bus_entry_count, t);
    set_times(procfs_bus_pci_entries, procfs_bus_pci_entry_count, t);
    set_times(procfs_pid_entries, procfs_pid_entry_count, t);
    set_times(procfs_net_entries, procfs_net_entry_count, t);

#undef set_times

    procfs_inited = 1;
    
    return procfs_root;
}


/*
 * Mount the procfs filesystem.
 *
 * Inputs:
 *    d => pointer to the mount info struct on which we'll mount procfs
 *    flags => currently not used
 *    options => a string of options that MIGHT include the following comma-
 *               separated options and their values:
 *                   inode_count, block_count, block_size
 *               e.g.
 *                   "inode_count=64,block_count=16,block_size=512"
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long procfs_mount(struct mount_info_t *d, int flags, char *options)
{
    UNUSED(flags);
    UNUSED(options);

    struct fs_node_t *root = procfs_create();
    
    if(root)
    {
        d->dev = root->dev;
        return 0;
    }
    
    return -EIO;
}


/*
 * Read the filesystem's superblock and root inode.
 * This function fills in the mount info struct's block_size, super,
 * and root fields.
 */
long procfs_read_super(dev_t dev, struct mount_info_t *d,
                       size_t bytes_per_sector)
{
    UNUSED(bytes_per_sector);
    
    if(dev != PROCFS_DEVID || procfs_root == NULL)
    {
        return -EINVAL;
    }
    
    d->block_size = PROCFS_BLOCK_SIZE;
    d->super = NULL;
    d->root = procfs_root;
    
    return 0;
}


/*
 * Release the filesystem's superblock and its buffer.
 * For procfs, we also release the virtual disk's memory, as we expect no one
 * will be using them anymore after this call.
 * Called when unmounting the filesystem.
 */
void procfs_put_super(dev_t dev, struct superblock_t *sb)
{
    UNUSED(sb);

    if(dev != PROCFS_DEVID || procfs_root == NULL)
    {
        return;
    }
}


static inline int valid_procfs_node(struct fs_node_t *node)
{
    if(!node || node->dev != PROCFS_DEVID || !procfs_root)
    {
        return 0;
    }
    
    return 1;
}


void copy_root_node_attribs(struct fs_node_t *node,
                            struct procfs_entry_t *e, int file)
{
    node->mode = e[file].mode;
    node->atime = e[file].atime;
    node->mtime = e[file].mtime;
    node->ctime = e[file].ctime;
    node->uid = 0;
    node->gid = 0;
    node->size = S_ISDIR(node->mode) ? PROCFS_BLOCK_SIZE : 0;
    node->links = S_ISDIR(node->mode) ? 2 : 1;
}

void copy_pid_node_attribs(struct fs_node_t *node,
                           volatile struct task_t *task, mode_t mode)
{
    time_t t = startup_time + (task->start_time / PIT_FREQUENCY);
    node->mode = mode;
    node->atime = t;
    node->mtime = t;
    node->ctime = t;
    node->uid = task->euid;
    node->gid = task->egid;
    node->size = S_ISDIR(node->mode) ? PROCFS_BLOCK_SIZE : 0;
    node->links = S_ISDIR(node->mode) ? 2 : 1;
}

volatile struct task_t *get_task_by_index(int i)
{
    if(i < 0 || i >= NR_TASKS)
    {
        return NULL;
    }
    
    return task_table[i];
}

int get_index_for_task(volatile struct task_t *task)
{
    for_each_taskptr(t)
    {
        if(*t && (*t)->pid == task->pid)
        {
            return t - task_table;
        }
    }
    
    return -1;
}


ino_t procfs_root_entry_inode(int offset)
{
    ino_t ino;

    if(offset == 0 || offset == 1)
    {
        ino = MAKE_PROCFS_INODE(DIR_PROC, 0, 0);
    }
    else if(offset == PROC_BUS_DIR)
    {
        ino = MAKE_PROCFS_INODE(DIR_BUS, 0, 0);
    }
    else if(offset == PROC_SYS_DIR)
    {
        ino = MAKE_PROCFS_INODE(DIR_SYS, 0, 0);
    }
    else if(offset == PROC_TTY_DIR)
    {
        ino = MAKE_PROCFS_INODE(DIR_TTY, 0, 0);
    }
    else if(offset == PROC_NET_DIR)
    {
        ino = MAKE_PROCFS_INODE(DIR_NET, 0, 0);
    }
    else
    {
        ino = MAKE_PROCFS_INODE(DIR_PROC, 0, offset);
    }
    
    return ino;
}


ino_t procfs_bus_entry_inode(int offset)
{
    ino_t ino;

    if(offset == 0)
    {
        ino = MAKE_PROCFS_INODE(DIR_BUS, 0, 0);
    }
    else if(offset == 1)
    {
        ino = MAKE_PROCFS_INODE(DIR_PROC, 0, 0);
    }
    else if(offset == 2)
    {
        ino = MAKE_PROCFS_INODE(DIR_BUS_PCI, 0, 0);
    }
    else
    {
        ino = MAKE_PROCFS_INODE(DIR_BUS, 0, offset);
    }
    
    return ino;
}


ino_t procfs_nettty_entry_inode(int dir, int offset)
{
    ino_t ino;

    if(offset == 0)
    {
        ino = MAKE_PROCFS_INODE(dir, 0, 0);
    }
    else if(offset == 1)
    {
        ino = MAKE_PROCFS_INODE(DIR_PROC, 0, 0);
    }
    else
    {
        ino = MAKE_PROCFS_INODE(dir, 0, offset);
    }
    
    return ino;
}


ino_t procfs_pid_entry_inode(int subdir, int offset)
{
    ino_t ino;

    if(offset == 0)
    {
        ino = MAKE_PROCFS_INODE(DIR_PID, subdir, 0);
    }
    else if(offset == 1)
    {
        ino = MAKE_PROCFS_INODE(DIR_PROC, 0, 0);
    }
    else if(offset == PROC_PID_FD)
    {
        ino = MAKE_PROCFS_INODE(DIR_PID_FD, subdir, 0);
    }
    else if(offset == PROC_PID_TASK)
    {
        ino = MAKE_PROCFS_INODE(DIR_PID_TASK, subdir, 0);
    }
    else
    {
        /*
        if(!task_table[subdir])
        {
            break;
        }
        */

        ino = MAKE_PROCFS_INODE(DIR_PID, subdir, offset);
    }

    return ino;
}


struct pci_bus_t *bus_from_number(int n)
{
    struct pci_bus_t *bus;

    for(bus = first_pci_bus; bus != NULL; bus = bus->next)
    {
        if(--n == 0)
        {
            return bus;
        }
    }

    return NULL;
}


struct pci_dev_t *dev_from_number(struct pci_bus_t *bus, int n)
{
    struct pci_dev_t *pci;

    for(n -= 1, pci = bus->first; pci != NULL; pci = pci->next)
    {
        if(--n == 0)
        {
            return pci;
        }
    }

    return NULL;
}


#define assert_not_bigger_than(f, n, e)     if(f < 0 || f >= (int)n) return -e;

/*
 * Reads inode data structure from disk.
 */
long procfs_read_inode(struct fs_node_t *node)
{
    if(!valid_procfs_node(node))
    {
        return -EINVAL;
    }

    int dir = INODE_DIR_BITS(node->inode);
    int subdir = INODE_SUBDIR_BITS(node->inode);
    int file = INODE_FILE_BITS(node->inode);
    volatile struct task_t *task, *task2;
    
    KDEBUG("procfs_read_inode: dir %d, subdir %d, file %d\n", dir, subdir, file);
    
    switch(dir)
    {
        case DIR_PROC:
            assert_not_bigger_than(subdir, 1, ENOENT);

            if(file < procfs_root_entry_count)
            {
                copy_root_node_attribs(node, procfs_root_entries, file);
                return 0;
            }
            else
            {
                file -= procfs_root_entry_count;
                
                if(file < NR_TASKS && task_table[file])
                {
                    copy_pid_node_attribs(node, task_table[file],
                                            PROCFS_DIR_MODE);
                    return 0;
                }
            }

            return -ENOENT;

        case DIR_BUS:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, procfs_bus_entry_count, ENOENT);
            copy_root_node_attribs(node, procfs_bus_entries, file);
            return 0;

        case DIR_BUS_PCI:
            if(subdir == 0)
            {
                assert_not_bigger_than(file, procfs_bus_pci_entry_count,
                                            ENOENT);
                copy_root_node_attribs(node, procfs_bus_pci_entries, file);
                return 0;
            }
            else
            {
                struct pci_bus_t *bus;
                struct pci_dev_t *pci;

                if(!(bus = bus_from_number(subdir)))
                {
                    break;
                }

                if(file == 0)
                {
                    copy_root_node_attribs(node, procfs_bus_pci_entries, 0);
                    return 0;
                }

                if((pci = dev_from_number(bus, file)))
                {
                    copy_root_node_attribs(node, procfs_bus_pci_entries, 2);
                    return 0;
                }
            }

            break;

        case DIR_PID:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            assert_not_bigger_than(file, procfs_pid_entry_count, ENOENT);
            copy_pid_node_attribs(node, task, procfs_pid_entries[file].mode);
            return 0;

        case DIR_PID_FD:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }
            
            if(!file)       // '.'
            {
                copy_pid_node_attribs(node, task, PROCFS_DIR_MODE);
                return 0;
            }
            
            if(!validfd(file - 1, task))
            {
                return -ENOENT;
            }

            copy_pid_node_attribs(node, task, PROCFS_LINK_MODE);
            return 0;

        case DIR_PID_TASK:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            if(!file)       // '.'
            {
                copy_pid_node_attribs(node, task, PROCFS_DIR_MODE);
                return 0;
            }

            if(!(task2 = get_task_by_index(file - 1)))
            {
                return -ENOENT;
            }

            copy_pid_node_attribs(node, task2, PROCFS_DIR_MODE);
            return 0;

        case DIR_NET:
            assert_not_bigger_than(subdir, 1, ENOENT);  // XXX - for now
            assert_not_bigger_than(file, procfs_net_entry_count, ENOENT);
            copy_root_node_attribs(node, procfs_net_entries, file);
            return 0;

        case DIR_TTY:
            assert_not_bigger_than(subdir, 1, ENOENT);  // XXX - for now
            assert_not_bigger_than(file, procfs_tty_entry_count, ENOENT);
            copy_root_node_attribs(node, procfs_tty_entries, file);
            return 0;

        case DIR_SYS:
            copy_root_node_attribs(node, procfs_root_entries, 0);
            return 0;
    }

    return -ENOENT;
}


/*
 * Writes inode data structure to disk.
 */
long procfs_write_inode(struct fs_node_t *node)
{
    if(!valid_procfs_node(node))
    {
        return -EINVAL;
    }

    return 0;
}


STATIC_INLINE 
struct dirent *procfs_entry_to_dirent(ino_t ino,
                                      mode_t mode, char *name, int off)
{
    int namelen = strlen(name);
    unsigned int reclen = GET_DIRENT_LEN(namelen);

    struct dirent *entry = kmalloc(reclen);

    if(!entry)
    {
        return NULL;
    }
    
    entry->d_reclen = reclen;
    entry->d_ino = ino;
    entry->d_off = off;
    entry->d_type = S_ISDIR(mode) ? DT_DIR :
                    (S_ISLNK(mode) ? DT_LNK : DT_REG);
    strcpy(entry->d_name, name);
    
    return entry;
}


#define ishex(d)    (((d) >= '0' && (d) <= '9') || \
                     ((d) >= 'a' && (d) <= 'f'))

#define gethex(d)   (((d) >= '0' && (d) <= '9') ? (d) - '0' : (d) - 'a')


/*
 * Find the given filename in the parent directory.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    filename => the searched-for filename
 *
 * Outputs:
 *    entry => if the filename is found, its entry is converted to a kmalloc'd
 *             dirent struct, and the result is stored in this field
 *    dbuf => the disk buffer representing the disk block containing the found
 *            filename, this is useful if the caller wants to delete the file
 *            after finding it (vfs_unlink(), for example)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                file's entry
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long procfs_finddir(struct fs_node_t *dirnode, char *filename,
                    struct dirent **entry,
                    struct cached_page_t **dbuf, size_t *dbuf_off)
{
    if(!valid_procfs_node(dirnode))
    {
        return -EINVAL;
    }

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    int dir = INODE_DIR_BITS(dirnode->inode);
    int subdir = INODE_SUBDIR_BITS(dirnode->inode);
    int file = INODE_FILE_BITS(dirnode->inode);
    volatile int i, j;
    volatile ino_t ino;
    volatile mode_t mode;
    char tmp[16];
    volatile struct task_t *task, *thread;

    KDEBUG("%s: d %d, s %d, f %d\n", __func__, dir, subdir, file);
    
    switch(dir)
    {
        case DIR_PROC:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOENT);
            
            // search the standard entries first
            for(i = 0; i < (int)procfs_root_entry_count; i++)
            {
                if(strcmp(procfs_root_entries[i].name, filename) == 0)
                {
                    ino = procfs_root_entry_inode(i);
                    *entry = procfs_entry_to_dirent(ino,
                                            procfs_root_entries[i].mode,
                                            procfs_root_entries[i].name, i);
                    return *entry ? 0 : -ENOMEM;
                }
            }
            
            // not found, search for a [pid] dir
            if(*filename >= '0' && *filename <= '9')
            {
                i = atoi(filename);
                
                if(i <= 0 /* || i >= NR_TASKS */)
                {
                    return -ENOENT;
                }

                for_each_taskptr(t)
                {
                    if(*t && tgid(*t) == i && (*t)->pid == tgid(*t))
                    {
                        KDEBUG("%s: found pid %d\n", __func__, i);
                        ino = MAKE_PROCFS_INODE(DIR_PID, t - task_table, 0);
                        //sprintf(tmp, "%d", tgid(*t));
                        ksprintf(tmp, sizeof(tmp), "%d", tgid(*t));
                        *entry = procfs_entry_to_dirent(ino,
                                        PROCFS_DIR_MODE,
                                        tmp, procfs_root_entry_count + 
                                                (t - task_table));
                        return *entry ? 0 : -ENOMEM;
                    }
                }
            }

            break;

        case DIR_BUS:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);

            for(i = 0; i < procfs_bus_entry_count; i++)
            {
                if(strcmp(procfs_bus_entries[i].name, filename) == 0)
                {
                    ino = procfs_bus_entry_inode(i);
                    *entry = procfs_entry_to_dirent(ino,
                                            procfs_bus_entries[i].mode,
                                            procfs_bus_entries[i].name, i);
                    return *entry ? 0 : -ENOMEM;
                }
            }

            break;

        case DIR_BUS_PCI:
            assert_not_bigger_than(file, 1, ENOTDIR);

            if(subdir == 0)
            {
                char *buses;
                int bus_count;

                if(active_pci_buses(&buses, &bus_count) != 0)
                {
                    return -ENOMEM;
                }

                if(strcmp(filename, ".") == 0)
                {
                    ino = MAKE_PROCFS_INODE(dir, 0, 0);
                    mode = PROCFS_DIR_MODE;
                    i = 0;
                }
                else if(strcmp(filename, "..") == 0)
                {
                    ino = MAKE_PROCFS_INODE(DIR_BUS, 0, 0);
                    mode = PROCFS_DIR_MODE;
                    i = 0;
                }
                else if(strcmp(filename, "devices") == 0)
                {
                    ino = MAKE_PROCFS_INODE(dir, 0, 2);
                    mode = PROCFS_FILE_MODE;
                    i = 1;
                }
                else
                {
                    for(j = 0; j < bus_count; j++)
                    {
                        //sprintf(tmp, "%02x", buses[j]);
                        ksprintf(tmp, sizeof(tmp), "%02x", buses[j]);
                            
                        if(strcmp(tmp, filename) == 0)
                        {
                            ino = MAKE_PROCFS_INODE(dir, j + 1, 0);
                            mode = PROCFS_DIR_MODE;
                            break;
                        }
                    }
                        
                    if(j == bus_count)
                    {
                        kfree(buses);
                        break;
                    }

                    i = j + 1;
                }

                kfree(buses);
                *entry = procfs_entry_to_dirent(ino, mode,
                                                filename, i);
                return *entry ? 0 : -ENOMEM;
            }
            else
            {
                struct pci_bus_t *bus;
                struct pci_dev_t *pci;

                if(!(bus = bus_from_number(subdir)))
                {
                    break;
                }
                
                if(strcmp(filename, ".") == 0)
                {
                    ino = MAKE_PROCFS_INODE(dir, subdir, 0);
                    mode = PROCFS_DIR_MODE;
                    i = 0;
                }
                else if(strcmp(filename, "..") == 0)
                {
                    ino = MAKE_PROCFS_INODE(DIR_BUS_PCI, 0, 0);
                    mode = PROCFS_DIR_MODE;
                    i = 0;
                }
                else
                {
                    for(i = 2, pci = bus->first;
                        pci != NULL;
                        pci = pci->next, i++)
                    {
                        //sprintf(tmp, "%02x.%02x", pci->dev, pci->function);
                        ksprintf(tmp, sizeof(tmp), "%02x.%02x",
                                    pci->dev, pci->function);

                        if(strcmp(tmp, filename) == 0)
                        {
                            ino = MAKE_PROCFS_INODE(dir, subdir, i);
                            mode = PROCFS_FILE_MODE;
                            break;
                        }
                    }
                    
                    if(!pci)
                    {
                        break;
                    }

                    *entry = procfs_entry_to_dirent(ino, mode,
                                                    filename, i);
                    return *entry ? 0 : -ENOMEM;
                }
            }

            break;

        case DIR_PID:
            if(subdir < 0 || subdir >= NR_TASKS)
            {
                break;
            }
            
            assert_not_bigger_than(file, 1, ENOTDIR);

            for(i = 0; i < (int)procfs_pid_entry_count; i++)
            {
                if(strcmp(filename, procfs_pid_entries[i].name) == 0)
                {
                    ino = procfs_pid_entry_inode(subdir, i);

                    *entry = procfs_entry_to_dirent(ino,
                                            procfs_pid_entries[i].mode,
                                            procfs_pid_entries[i].name, i);
                    return *entry ? 0 : -ENOMEM;
                }
            }
            
            break;

        case DIR_PID_FD:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            assert_not_bigger_than(file, 1, ENOTDIR);
            
            if(strcmp(filename, ".") == 0)
            {
                ino = MAKE_PROCFS_INODE(dir, subdir, 0);
                mode = PROCFS_DIR_MODE;
                i = 0;
            }
            else if(strcmp(filename, "..") == 0)
            {
                ino = MAKE_PROCFS_INODE(DIR_PID, subdir, 0);
                mode = PROCFS_DIR_MODE;
                i = 0;
            }
            else
            {
                for(ino = 0, i = 0; i < NR_OPEN; i++)
                {
                    if(!task->ofiles->ofile[i])
                    {
                        continue;
                    }

                    //sprintf(tmp, "%d", i);
                    ksprintf(tmp, sizeof(tmp), "%d", i);

                    //if(task->pid == 30 && i == 31) __asm__ __volatile__("xchg %%bx, %%bx"::);
                
                    if(strcmp(tmp, filename) == 0)
                    {
                        ino = MAKE_PROCFS_INODE(dir, subdir, ++i);
                        mode = PROCFS_LINK_MODE;
                        break;
                    }
                }
                
                if(ino == 0)
                //if(i == NR_OPEN)
                {
                    break;
                }
            }

            *entry = procfs_entry_to_dirent(ino, mode, filename, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_PID_TASK:
            if(!(task = get_task_by_index(subdir)))
            {
                break;
            }

            assert_not_bigger_than(file, 1, ENOTDIR);

            kernel_mutex_lock(&(task->threads->mutex));
            thread = NULL;

            if(strcmp(filename, ".") == 0)
            {
                ino = MAKE_PROCFS_INODE(dir, subdir, 0);
                i = 0;
            }
            else if(strcmp(filename, "..") == 0)
            {
                ino = MAKE_PROCFS_INODE(DIR_PID, subdir, 0);
                i = 0;
            }
            else
            {
                for_each_thread(thread, task)
                {
                    //sprintf(tmp, "%d", thread->pid);
                    ksprintf(tmp, sizeof(tmp), "%d", thread->pid);
                    
                    if(strcmp(tmp, filename) == 0)
                    {
                        ino = MAKE_PROCFS_INODE(DIR_PID,
                                        get_index_for_task(thread), 0);
                        i = 2;
                        break;
                    }
                }

                if(!thread)
                {
                    break;
                }
            }

            kernel_mutex_unlock(&(task->threads->mutex));
            *entry = procfs_entry_to_dirent(ino, PROCFS_DIR_MODE,
                                            filename, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_NET:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);

            for(i = 0; i < procfs_net_entry_count; i++)
            {
                if(strcmp(procfs_net_entries[i].name, filename) == 0)
                {
                    ino = procfs_nettty_entry_inode(DIR_NET, i);
                    *entry = procfs_entry_to_dirent(ino,
                                            procfs_net_entries[i].mode,
                                            procfs_net_entries[i].name, i);
                    return *entry ? 0 : -ENOMEM;
                }
            }

            break;

        case DIR_TTY:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);

            for(i = 0; i < procfs_tty_entry_count; i++)
            {
                if(strcmp(procfs_tty_entries[i].name, filename) == 0)
                {
                    ino = procfs_nettty_entry_inode(DIR_TTY, i);
                    *entry = procfs_entry_to_dirent(ino,
                                            procfs_tty_entries[i].mode,
                                            procfs_tty_entries[i].name, i);
                    return *entry ? 0 : -ENOMEM;
                }
            }

            break;

        case DIR_SYS:
        default:
            break;
    }

    return -ENOENT;
}


/*
 * Find the given inode in the parent directory.
 * Called during pathname resolution when constructing the absolute pathname
 * of a given inode.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    node => the searched-for inode
 *
 * Outputs:
 *    entry => if the node is found, its entry is converted to a kmalloc'd
 *             dirent struct, and the result is stored in this field
 *    dbuf => the disk buffer representing the disk block containing the found
 *            filename, this is useful if the caller wants to delete the file
 *            after finding it (vfs_unlink(), for example)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                file's entry
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long procfs_finddir_by_inode(struct fs_node_t *dirnode, struct fs_node_t *node,
                             struct dirent **entry,
                             struct cached_page_t **dbuf, size_t *dbuf_off)
{
    if(!valid_procfs_node(dirnode))
    {
        return -EINVAL;
    }

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;

    int dir = INODE_DIR_BITS(dirnode->inode);
    int subdir = INODE_SUBDIR_BITS(dirnode->inode);
    int file = INODE_FILE_BITS(dirnode->inode);
    volatile int child_dir, child_subdir, child_file;
    volatile int i;
    //volatile ino_t ino;
    char tmp[16];
    volatile struct task_t *task, *thread;

    child_dir = INODE_DIR_BITS(node->inode);
    child_subdir = INODE_SUBDIR_BITS(node->inode);
    child_file = INODE_FILE_BITS(node->inode);

    KDEBUG("%s: d %d, s %d, f %d (cd %d, cs %d, cf %d)\n", __func__, dir, subdir, file, child_dir, child_subdir, child_file);
    
    switch(dir)
    {
        case DIR_PROC:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOENT);
            
            if(child_dir == DIR_PID)
            {
                if(child_file)
                {
                    break;
                }

                if(child_subdir >= 0 && child_subdir < NR_TASKS &&
                   task_table[child_subdir])
                {
                    //sprintf(tmp, "%d", tgid(task_table[i]));
                    ksprintf(tmp, sizeof(tmp), "%d",
                                tgid(task_table[child_subdir]));
                    *entry = procfs_entry_to_dirent(node->inode,
                                                    PROCFS_DIR_MODE, tmp,
                                                    procfs_root_entry_count + 
                                                            child_subdir);
                    return *entry ? 0 : -ENOMEM;
                }
                
                break;
            }
            
            if(child_subdir)
            {
                break;
            }
            
            if(!child_file)
            {
                if(child_dir == DIR_BUS)
                {
                    i = PROC_BUS_DIR;
                }
                else if(child_dir == DIR_PROC)
                {
                    i = 0;
                }
                else if(child_dir == DIR_SYS)
                {
                    i = PROC_SYS_DIR;
                }
                else if(child_dir == DIR_TTY)
                {
                    i = PROC_TTY_DIR;
                }
                else
                {
                    break;
                }
            }
            else if(child_file > 0 &&
                    child_file < (int)procfs_root_entry_count)
            {
                i = child_file;
            }
            else
            {
                break;
            }

            *entry = procfs_entry_to_dirent(node->inode,
                                            procfs_root_entries[i].mode,
                                            procfs_root_entries[i].name, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_BUS:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);
            i = child_file;
            
            if(child_dir != dir || child_subdir != 0)
            {
                break;
            }
            
            if(i < 0 || i >= (int)procfs_bus_entry_count)
            {
                break;
            }

            *entry = procfs_entry_to_dirent(node->inode,
                                            procfs_bus_entries[i].mode,
                                            procfs_bus_entries[i].name, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_BUS_PCI:
            assert_not_bigger_than(file, 1, ENOTDIR);

            if(child_dir != dir)
            {
                break;
            }

            if(subdir == 0)
            {
                char *buses;
                int bus_count;

                if(child_subdir == 0)
                {
                    if(child_file == 0 || child_file == 2)
                    {
                        i = child_file;
                        *entry = procfs_entry_to_dirent(node->inode,
                                            procfs_bus_pci_entries[i].mode,
                                            procfs_bus_pci_entries[i].name, i);
                        return *entry ? 0 : -ENOMEM;
                    }
                    
                    break;
                }

                if(active_pci_buses(&buses, &bus_count) != 0)
                {
                    return -ENOMEM;
                }
                
                if(child_subdir > 0 && child_subdir <= bus_count)
                {
                    //sprintf(tmp, "%02x", buses[child_subdir - 1]);
                    ksprintf(tmp, sizeof(tmp), "%02x",
                                buses[child_subdir - 1]);
                    kfree(buses);
                    *entry = procfs_entry_to_dirent(node->inode,
                                                    PROCFS_DIR_MODE,
                                                    tmp, child_subdir);
                    return *entry ? 0 : -ENOMEM;
                }

                kfree(buses);
                break;
            }
            else
            {
                struct pci_bus_t *bus;
                struct pci_dev_t *pci;

                if(!(bus = bus_from_number(subdir)))
                {
                    break;
                }

                if(child_file == 0)
                {
                    *entry = procfs_entry_to_dirent(node->inode,
                                                    PROCFS_DIR_MODE,
                                                    ".", 0);
                    return *entry ? 0 : -ENOMEM;
                }

                if((pci = dev_from_number(bus, child_file)))
                {
                    //sprintf(tmp, "%02x.%02x", pci->dev, pci->function);
                    ksprintf(tmp, sizeof(tmp), "%02x.%02x",
                                pci->dev, pci->function);
                    *entry = procfs_entry_to_dirent(node->inode,
                                                    PROCFS_FILE_MODE,
                                                    tmp, child_file);
                    return *entry ? 0 : -ENOMEM;
                }
            }

            break;

        case DIR_PID:
            if(subdir < 0 || subdir >= NR_TASKS)
            {
                break;
            }
            
            assert_not_bigger_than(file, 1, ENOTDIR);
            
            // /proc/[pid]/fd/
            if(node->inode == (ino_t)MAKE_PROCFS_INODE(DIR_PID_FD, subdir, 0))
            {
                child_dir = dir;
                child_subdir = subdir;
                child_file = PROC_PID_FD;
            }
            // /proc/[pid]/task/
            else if(node->inode == (ino_t)MAKE_PROCFS_INODE(PROC_PID_TASK,
                                                            subdir, 0))
            {
                child_dir = dir;
                child_subdir = subdir;
                child_file = PROC_PID_FD;
            }
            
            // other files under /proc/[pid]/
            if(child_dir != dir || child_subdir != subdir)
            {
                break;
            }

            i = child_file;
            
            if(i < 0 || i >= (int)procfs_pid_entry_count)
            {
                break;
            }

            *entry = procfs_entry_to_dirent(node->inode,
                                            procfs_pid_entries[i].mode,
                                            procfs_pid_entries[i].name, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_PID_FD:
            if(!(task = get_task_by_index(subdir)))
            {
                break;
            }

            assert_not_bigger_than(file, 1, ENOTDIR);
            i = child_file;
            
            if(child_dir != dir || child_subdir != subdir)
            {
                break;
            }
            
            if(i == 0)
            {
                *entry = procfs_entry_to_dirent(node->inode,
                                                PROCFS_DIR_MODE,
                                                ".", i);
                return *entry ? 0 : -ENOMEM;
            }
            
            if(i > NR_OPEN)
            {
                break;
            }

            if(!task->ofiles->ofile[i - 1])
            {
                break;
            }

            //sprintf(tmp, "%d", i - 1);
            ksprintf(tmp, sizeof(tmp), "%d", i - 1);
            *entry = procfs_entry_to_dirent(node->inode,
                                            PROCFS_LINK_MODE, tmp, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_PID_TASK:
            if(!(task = get_task_by_index(subdir)))
            {
                break;
            }

            assert_not_bigger_than(file, 1, ENOTDIR);
            
            if(dir == child_dir || dir == DIR_PID)
            {
                if(subdir != child_subdir)
                {
                    break;
                }
                
                //sprintf(tmp, (dir == DIR_PID) ? ".." : ".");
                ksprintf(tmp, sizeof(tmp), (dir == DIR_PID) ? ".." : ".");
                i = 0;
            }
            else if(child_dir == DIR_PROC)
            {
                kernel_mutex_lock(&(task->threads->mutex));

                for_each_thread(thread, task)
                {
                    if(--child_subdir == 0)
                    {
                        break;
                    }
                }

                kernel_mutex_unlock(&(task->threads->mutex));
                
                if(!thread)
                {
                    break;
                }

                //sprintf(tmp, "%d", thread->pid);
                ksprintf(tmp, sizeof(tmp), "%d", thread->pid);
                i = 2;
            }
            else
            {
                break;
            }

            *entry = procfs_entry_to_dirent(node->inode,
                                            PROCFS_DIR_MODE, tmp, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_NET:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);
            i = child_file;
            
            if(child_dir != dir || child_subdir != 0)
            {
                break;
            }
            
            if(i < 0 || i >= (int)procfs_net_entry_count)
            {
                break;
            }

            *entry = procfs_entry_to_dirent(node->inode,
                                            procfs_net_entries[i].mode,
                                            procfs_net_entries[i].name, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_TTY:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);
            i = child_file;
            
            if(child_dir != dir || child_subdir != 0)
            {
                break;
            }
            
            if(i < 0 || i >= (int)procfs_tty_entry_count)
            {
                break;
            }

            *entry = procfs_entry_to_dirent(node->inode,
                                            procfs_tty_entries[i].mode,
                                            procfs_tty_entries[i].name, i);
            return *entry ? 0 : -ENOMEM;

        case DIR_SYS:
        default:
            break;
    }

    return -ENOENT;
}


#define copy_dent(name)                                 \
{                                                       \
    namelen = strlen((char *)name);                     \
    reclen = GET_DIRENT_LEN(namelen);                   \
    /* ALIGN_WORD(reclen); */                           \
    if((count + reclen) > (size_t)bufsz) break;         \
    dent = (struct dirent *)b;                          \
    dent->d_ino = ino;                                  \
    dent->d_off = offset;                               \
    dent->d_type = S_ISDIR(mode) ? DT_DIR :             \
                    (S_ISLNK(mode) ? DT_LNK : DT_REG);  \
    strcpy((char *)dent->d_name, (char *)name);         \
    dent->d_reclen = reclen;                            \
    b += reclen;                                        \
    count += reclen;                                    \
}


/*
 * Get dir entries.
 *
 * Inputs:
 *     dir => node of dir to read from
 *     pos => byte position to start reading entries from
 *     dp => buffer in which to store dir entries
 *     count => max number of bytes to read (i.e. size of dp)
 *
 * Returns:
 *     number of bytes read on success, -errno on failure
 */
long procfs_getdents(struct fs_node_t *dirnode, off_t *pos,
                     void *buf, int bufsz)
{
    if(!valid_procfs_node(dirnode))
    {
        return -EINVAL;
    }

    int dir = INODE_DIR_BITS(dirnode->inode);
    int subdir = INODE_SUBDIR_BITS(dirnode->inode);
    int file = INODE_FILE_BITS(dirnode->inode);
    char *b = (char *)buf;

    KDEBUG("%s: dir %d, subdir %d, file %d\n", __func__, dir, subdir, file);
    
    volatile size_t offset, count = 0, entry_count = 0;
    volatile int i;
    volatile size_t reclen, namelen;
    volatile char *name;
    volatile mode_t mode;
    volatile ino_t ino;
    volatile struct dirent *dent;
    volatile struct task_t *task, *thread;
    char tmp[16];

    offset = *pos;
    
    switch(dir)
    {
        case DIR_PROC:
            //entry_count = procfs_root_entry_count + total_tasks;
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);

            //while(offset < entry_count)
            while(1)
            {
                // [pid] dirs
                if(offset >= procfs_root_entry_count)
                {
                    i = offset - procfs_root_entry_count;
                    volatile int found = 0;
                    
                    for_each_taskptr(t)
                    {
                        if(!*t || (*t)->pid != tgid(*t))
                        {
                            //__asm__ __volatile__ ("xchg %%bx, %%bx"::);
                            continue;
                        }

                        if(i-- == 0)
                        {
                            ino = MAKE_PROCFS_INODE(DIR_PID,
                                                    t - task_table, 0);
                            //sprintf(tmp, "%d", tgid(*t));
                            ksprintf(tmp, sizeof(tmp), "%d", tgid(*t));
                            mode = PROCFS_DIR_MODE;
                            found = 1;
                            copy_dent(tmp);
                            offset++;
                            break;
                        }
                    }
                    
                    if(!found)
                    {
                        break;
                    }

                    if((count + reclen) > (size_t)bufsz)
                    {
                        break;
                    }
                    
                    continue;
                }
                
                ino = procfs_root_entry_inode(offset);
                name = procfs_root_entries[offset].name;
                mode = procfs_root_entries[offset].mode;
                copy_dent(name);
                offset++;
            }

            *pos = offset;
            return count;

        case DIR_BUS:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);

            while(offset < procfs_bus_entry_count)
            {
                ino = procfs_bus_entry_inode(offset);
                name = procfs_bus_entries[offset].name;
                mode = procfs_bus_entries[offset].mode;
                copy_dent(name);
                offset++;
            }

            *pos = offset;
            return count;

        case DIR_BUS_PCI:
            assert_not_bigger_than(file, 1, ENOTDIR);

            if(subdir == 0)
            {
                char *buses;
                int bus_count;

                if(active_pci_buses(&buses, &bus_count) != 0)
                {
                    return -ENOMEM;
                }

                while(offset < (size_t)(bus_count + 3))
                {
                    if(offset == 0)
                    {
                        ino = MAKE_PROCFS_INODE(dir, 0, 0);
                        //sprintf(tmp, ".");
                        ksprintf(tmp, sizeof(tmp), ".");
                    }
                    else if(offset == 1)
                    {
                        ino = MAKE_PROCFS_INODE(DIR_BUS, 0, 0);
                        //sprintf(tmp, "..");
                        ksprintf(tmp, sizeof(tmp), "..");
                    }
                    else if(offset == 2)
                    {
                        ino = MAKE_PROCFS_INODE(dir, 0, offset);
                        //sprintf(tmp, "devices");
                        ksprintf(tmp, sizeof(tmp), "devices");
                    }
                    else
                    {
                        ino = MAKE_PROCFS_INODE(dir, offset - 2, 0);
                        //sprintf(tmp, "%02x", buses[offset - 3]);
                        ksprintf(tmp, sizeof(tmp), "%02x", buses[offset - 3]);
                    }

                    mode = (offset == 2) ? PROCFS_FILE_MODE : PROCFS_DIR_MODE;
                    copy_dent(tmp);
                    offset++;
                }

                kfree(buses);
            }
            else
            {
                struct pci_bus_t *bus;
                struct pci_dev_t *pci;

                if(!(bus = bus_from_number(subdir)))
                {
                    return -ENOENT;
                }
                
                entry_count = devices_on_bus(bus);

                while(offset < entry_count + 2)
                {
                    if(offset == 0)
                    {
                        ino = MAKE_PROCFS_INODE(dir, subdir, 0);
                        mode = PROCFS_DIR_MODE;
                        //sprintf(tmp, ".");
                        ksprintf(tmp, sizeof(tmp), ".");
                    }
                    else if(offset == 1)
                    {
                        ino = MAKE_PROCFS_INODE(DIR_BUS_PCI, 0, 0);
                        //sprintf(tmp, "..");
                        ksprintf(tmp, sizeof(tmp), "..");
                    }
                    else
                    {
                        if(!(pci = dev_from_number(bus, offset)))
                        {
                            return -ENOENT;
                        }

                        ino = MAKE_PROCFS_INODE(dir, subdir, offset);
                        mode = PROCFS_FILE_MODE;
                        //sprintf(tmp, "%02x.%02x", pci->dev, pci->function);
                        ksprintf(tmp, sizeof(tmp), "%02x.%02x",
                                    pci->dev, pci->function);
                    }

                    copy_dent(tmp);
                    offset++;
                }
            }

            *pos = offset;
            return count;

        case DIR_PID:
            if(subdir < 0 || subdir >= NR_TASKS)
            {
                return -ENOENT;
            }
            
            assert_not_bigger_than(file, 1, ENOTDIR);

            while(offset < procfs_pid_entry_count)
            {
                ino = procfs_pid_entry_inode(subdir, offset);
                name = procfs_pid_entries[offset].name;
                mode = procfs_pid_entries[offset].mode;
                copy_dent(name);
                offset++;
            }

            *pos = offset;
            return count;

        case DIR_PID_FD:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            assert_not_bigger_than(file, 1, ENOTDIR);

            while(offset < NR_OPEN + 2)
            {
                if(offset == 0)
                {
                    ino = MAKE_PROCFS_INODE(dir, subdir, 0);
                    //sprintf(tmp, ".");
                    ksprintf(tmp, sizeof(tmp), ".");
                    mode = PROCFS_DIR_MODE;
                }
                else if(offset == 1)
                {
                    ino = MAKE_PROCFS_INODE(DIR_PID, subdir, 0);
                    //sprintf(tmp, "..");
                    ksprintf(tmp, sizeof(tmp), "..");
                    mode = PROCFS_DIR_MODE;
                }
                else
                {
                    if(!task_table[subdir] || !task_table[subdir]->ofiles)
                    {
                        break;
                    }
                    
                    if(!task_table[subdir]->ofiles->ofile[offset - 2])
                    {
                        offset++;
                        continue;
                    }

                    ino = MAKE_PROCFS_INODE(dir, subdir, offset - 1);
                    //sprintf(tmp, "%ld", offset - 2);
                    ksprintf(tmp, sizeof(tmp), "%ld", offset - 2);
                    KDEBUG("procfs_getdents: [%d] %s\n", offset, tmp);
                    mode = PROCFS_LINK_MODE;
                }

                copy_dent(tmp);
                offset++;
            }

            *pos = offset;
            return count;

        case DIR_PID_TASK:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            assert_not_bigger_than(file, 1, ENOTDIR);

            kernel_mutex_lock(&(task->threads->mutex));
            thread = NULL;
            entry_count = task->threads->thread_count + 2;

            while(offset < entry_count)
            {
                if(offset == 0)
                {
                    ino = MAKE_PROCFS_INODE(dir, subdir, 0);
                    //sprintf(tmp, ".");
                    ksprintf(tmp, sizeof(tmp), ".");
                }
                else if(offset == 1)
                {
                    ino = MAKE_PROCFS_INODE(DIR_PID, subdir, 0);
                    //sprintf(tmp, "..");
                    ksprintf(tmp, sizeof(tmp), "..");
                }
                else if(thread == NULL)
                {
                    if(!(thread = task->threads->thread_group_leader))
                    {
                        break;
                    }

                    ino = MAKE_PROCFS_INODE(DIR_PROC,
                                        procfs_root_entry_count +
                                            get_index_for_task(thread), 0);
                    //sprintf(tmp, "%d", thread->pid);
                    ksprintf(tmp, sizeof(tmp), "%d", thread->pid);
                }
                else
                {
                    if(!(thread = thread->thread_group_next))
                    {
                        break;
                    }

                    ino = MAKE_PROCFS_INODE(DIR_PROC,
                                        procfs_root_entry_count +
                                            get_index_for_task(thread), 0);
                    //sprintf(tmp, "%d", thread->pid);
                    ksprintf(tmp, sizeof(tmp), "%d", thread->pid);
                }
                
                mode = PROCFS_DIR_MODE;
                copy_dent(tmp);
                offset++;
            }

            kernel_mutex_unlock(&(task->threads->mutex));
            *pos = offset;
            return count;

        case DIR_NET:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);

            while(offset < procfs_net_entry_count)
            {
                ino = procfs_nettty_entry_inode(DIR_NET, offset);
                name = procfs_net_entries[offset].name;
                mode = procfs_net_entries[offset].mode;
                copy_dent(name);
                offset++;
            }

            *pos = offset;
            return count;

        case DIR_TTY:
            assert_not_bigger_than(subdir, 1, ENOENT);
            assert_not_bigger_than(file, 1, ENOTDIR);

            while(offset < procfs_tty_entry_count)
            {
                ino = procfs_nettty_entry_inode(DIR_TTY, offset);
                name = procfs_tty_entries[offset].name;
                mode = procfs_tty_entries[offset].mode;
                copy_dent(name);
                offset++;
            }

            *pos = offset;
            return count;

        case DIR_SYS:
        default:
            return -ENOENT;
    }
}


/*
 * General block device control function.
 */
long procfs_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel)
{
    if(dev != PROCFS_DEVID)
    {
        return 0;
    }
    
    switch(cmd)
    {
        case BLKSSZGET:
            // get the block size in bytes
            RETURN_IOCTL_RES(int, arg, PROCFS_BLOCK_SIZE, kernel);

        case BLKGETSIZE:
            // get disk size in 512-blocks
            RETURN_IOCTL_RES(long, arg, 0, kernel);

        case BLKGETSIZE64:
        {
            // get disk size in bytes
            RETURN_IOCTL_RES(unsigned long long, arg, 0, kernel);
        }
    }
    
    return -EINVAL;
}


/*
 * Return filesystem statistics.
 */
long procfs_ustat(struct mount_info_t *d, struct ustat *ubuf)
{
    if(!d || d->dev != PROCFS_DEVID)
    {
        return -EINVAL;
    }
    
    if(!ubuf)
    {
        return -EFAULT;
    }
    
    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_ustat() function).
     */
    ubuf->f_tfree = 0;
    ubuf->f_tinode = 0;

    return 0;
}


/*
 * Return detailed filesystem statistics.
 */
long procfs_statfs(struct mount_info_t *d, struct statfs *statbuf)
{
    if(!d || d->dev != PROCFS_DEVID)
    {
        return -EINVAL;
    }

    if(!statbuf)
    {
        return -EFAULT;
    }

    /*
     * NOTE: we copy directly as we're called from kernel space (the
     *       syscall_statfs() function).
     */
    statbuf->f_type = PROC_SUPER_MAGIC;
    statbuf->f_bsize = PROCFS_BLOCK_SIZE;
    statbuf->f_blocks = 0;
    statbuf->f_bfree = 0;
    statbuf->f_bavail = 0;
    statbuf->f_files = 0;
    statbuf->f_ffree = 0;
    //statbuf->f_fsid = 0;
    statbuf->f_frsize = 0;
    statbuf->f_namelen = EXT2_MAX_FILENAME_LEN;
    statbuf->f_flags = d->mountflags;

    return 0;
}


long copy_string_internal(char *dest, char *src, size_t destsz, int kernel)
{
    return copy_internal(dest, src, destsz, strlen(src) + 1, kernel);
}


long copy_internal(char *__dest, char *__src, size_t destsz,
                   size_t srcsz, int kernel)
{
    size_t i;
    long res;
    void *dest, *src;

    dest = (void *)__dest;
    src = (void *)__src;
    
    if(!dest || !src)
    {
        return -EINVAL;
    }
    
    i = (destsz < srcsz) ? destsz : srcsz;
    
    if(kernel)
    {
        A_memcpy(dest, src, i);
        return i;
    }

    res = copy_to_user(dest, src, i);

    // copy_to_user() returns 0 on success, -errno on failure
    return res ? res : (long)i;
}


/*
 * Read the contents of a symbolic link. As different filesystems might have
 * different ways of storing symlinks (e.g. ext2 stores links < 60 chars in
 * length in the inode struct itself), so we hand over this task to the
 * filesystem.
 *
 * Inputs:
 *    link => the symlink's inode
 *    buf => the buffer in which we will read and store the symlink's target
 *    bufsz => size of buffer above 
 *    kernel => set if the caller is a kernel function (i.e. 'buf' address
 *              is in kernel memory), 0 if 'buf' is a userspace address
 *
 * Returns:
 *    number of chars read on success, -errno on failure
 */
long procfs_read_symlink(struct fs_node_t *link, char *buf,
                         size_t bufsz, int kernel)
{
    if(!valid_procfs_node(link))
    {
        return -EINVAL;
    }
    
    if(!buf || !bufsz)
    {
        return -EINVAL;
    }
    
    int dir = INODE_DIR_BITS(link->inode);
    int subdir = INODE_SUBDIR_BITS(link->inode);
    int file = INODE_FILE_BITS(link->inode);

    volatile struct task_t *task;
    struct fs_node_t *node;
    struct file_t *f;
    char *p = NULL;
    int res = 0;
    struct dentry_t *dent = NULL;

    switch(dir)
    {
        case DIR_PROC:
            assert_not_bigger_than(subdir, 1, ENOENT);

            switch(file)
            {
                case PROC_SELF       :   /* /proc/self */
                    if(!(p = (char *)kmalloc(32)))
                    {
                        return -ENOMEM;
                    }

                    ksprintf(p, 32, "/proc/%u", tgid(this_core->cur_task));
                    res = copy_string_internal(buf, p, bufsz, kernel);
                    kfree(p);
                    return res;

                case PROC_THREAD_SELF:   /* /proc/thread-self */
                    if(!(p = (char *)kmalloc(32)))
                    {
                        return -ENOMEM;
                    }

                    ksprintf(p, 32, "/proc/%u/task/%u", 
                            tgid(this_core->cur_task), this_core->cur_task->pid);
                    res = copy_string_internal(buf, p, bufsz, kernel);
                    kfree(p);
                    return res;

                default:
                    return -EINVAL;
            }
            
            break;

        case DIR_BUS:
            return -EINVAL;

        case DIR_BUS_PCI:
            return -EINVAL;

        case DIR_PID:
            if(!(task = get_task_by_index(subdir)))
            {
                return -EINVAL;
            }

            switch(file)
            {
                case PROC_PID_CWD       :   /* /proc/[pid]/cwd */
                    if(!task->fs || !task->fs->cwd)
                    {
                        *buf = '\0';
                        return 0;
                    }

                    return copy_task_dirpath(task->fs->cwd->dev,
                                             task->fs->cwd->inode,
                                             buf, bufsz, kernel);

                case PROC_PID_EXE       :   /* /proc/[pid]/exe */
                    if(!task->exe_dev || !task->exe_inode)
                    {
                        *buf = '\0';
                        return 0;
                    }

                    return copy_task_dirpath(task->exe_dev, task->exe_inode,
                                             buf, bufsz, kernel);

                case PROC_PID_ROOT      :   /* /proc/[pid]/root */
                    if(!task->fs || !task->fs->root)
                    {
                        *buf = '\0';
                        return 0;
                    }

                    return copy_task_dirpath(task->fs->root->dev,
                                             task->fs->root->inode,
                                             buf, bufsz, kernel);

                case PROC_PID_MOUNTS    :   /* /proc/[pid]/mounts */
                    //sprintf(buf, "/proc/mounts");
                    ksprintf(buf, bufsz, "/proc/mounts");
                    return strlen(buf);

                default:
                    return -EINVAL;
            }
            
            break;

        case DIR_PID_FD:
            if(file <= 0 || file > NR_OPEN)
            {
                return -EINVAL;
            }

            if(!(task = get_task_by_index(subdir)))
            {
                return -EINVAL;
            }

            /* /proc/[pid]/fd/[0]..[NR_OPEN-1] */
            if(!(task->ofiles) ||
               !(f = task->ofiles->ofile[file - 1]) ||
               !(node = f->node))
            {
                KDEBUG("%s: no file\n", __func__);
                return -EINVAL;
            }

            KDEBUG("%s: dev 0x%x, inode 0x%x\n", __func__, node->dev, node->inode);

            //__asm__ __volatile__("xchg %%bx, %%bx"::);

            //if(S_ISSOCK(node->mode))
            if(IS_SOCKET(node))
            {
                ksprintf(buf, bufsz, "socket:[%d]", node->inode);
                return strlen(buf);
            }

            if(IS_PIPE(node))
            {
                ksprintf(buf, bufsz, "pipe:[%d]", node->inode);
                return strlen(buf);
            }
            
            if(S_ISCHR(node->mode) && MAJOR(node->blocks[0]) == PTY_MASTER_MAJ)
            {
                /*
                 * TODO: fix this to return a link to the proper /dev/ptmx.
                 */
                ksprintf(buf, bufsz, "/dev/ptmx");
                return strlen(buf);
            }

            if((node = get_node(node->dev, node->inode, GETNODE_FOLLOW_MPOINTS)) == NULL)
            {
                __asm__ __volatile__("xchg %%bx, %%bx":::);
                KDEBUG("%s: no node %d\n", __func__, 1);
                return -EINVAL;
            }

            if((res = get_dentry(node, &dent)) < 0)
            {
                __asm__ __volatile__("xchg %%bx, %%bx":::);
                KDEBUG("%s: no dent %d\n", __func__, 2);
                release_node(node);
                return res;
            }

            KDEBUG("%s: path %s\n", __func__, dent->path ? dent->path : "null");

            if(!dent->path)
            {
                __asm__ __volatile__("xchg %%bx, %%bx":::);
                KDEBUG("%s: no path %d\n", __func__, 3);
                release_dentry(dent);
                release_node(node);
                return -ENOENT;
            }
    
            res = copy_string_internal(buf, dent->path, bufsz, kernel);
            release_dentry(dent);
            release_node(node);

            KDEBUG("%s: buf %s\n", __func__, buf);

            return res;

        case DIR_PID_TASK:
        case DIR_SYS:
        case DIR_TTY:
        case DIR_NET:
        default:
            return -EINVAL;
    }
}


/*
 * Write the contents of a symbolic link. As different filesystems might have
 * different ways of storing symlinks (e.g. ext2 stores links < 60 chars in
 * length in the inode struct itself), so we hand over this task to the
 * filesystem.
 *
 * Inputs:
 *    link => the symlink's inode
 *    target => the buffer containing the symlink's target to be saved
 *    len => size of buffer above
 *    kernel => set if the caller is a kernel function (i.e. 'target' address
 *              is in kernel memory), 0 if 'target' is a userspace address
 *
 * Returns:
 *    number of chars written on success, -errno on failure
 */
size_t procfs_write_symlink(struct fs_node_t *link, char *target,
                            size_t len, int kernel)
{
    if(!valid_procfs_node(link))
    {
        return -EINVAL;
    }
    
    if(!target)
    {
        return -EINVAL;
    }

    UNUSED(target);
    UNUSED(len);
    UNUSED(kernel);
    
    return -ENOSYS;
}


/*
 * Read /proc/[pid]/cmdline
 *      /proc/[pid]/environ
 */
static size_t procfs_get_task_args(volatile struct task_t *task, int which, char **buf)
{
    virtual_addr memstart = (virtual_addr)((which == PROC_PID_CMDLINE) ?
                                                task->arg_start :
                                                task->env_start);
    virtual_addr memend = (virtual_addr)((which == PROC_PID_CMDLINE) ?
                                                task->arg_end :
                                                task->env_end);

    size_t count = memend - memstart;
    PR_MALLOC(*buf, count);
    return read_other_taskmem((struct task_t *)task, 0, memstart, memend, *buf, count);
}


/*
 * Read from a procfs file.
 *
 * Ideally, this function should be in procfs_file.c, but sadly
 * it depends on a lot of the data structures and macros we defined at
 * the beginning of this file. Moving this function out of this file
 * means moving those definitions into procfs.h, which will make it more
 * bloated than what it already is.
 */
ssize_t procfs_read_file(struct fs_node_t *node, off_t *pos,
                         unsigned char *buf, size_t count)
{
    if(!node || !pos || !buf)
    {
        return -EINVAL;
    }
    
    int dir = INODE_DIR_BITS(node->inode);
    int subdir = INODE_SUBDIR_BITS(node->inode);
    int file = INODE_FILE_BITS(node->inode);
    size_t buflen = 0, j, i = *pos;
    char *procbuf = NULL;
    volatile struct task_t *task;
    
    //KDEBUG("procfs_read_file: pid 0x%x, file 0x%x\n", pid, file);
    KDEBUG("procfs_read_inode: dir %d, subdir %d, file %d\n", dir, subdir, file);

    switch(dir)
    {
        case DIR_PROC:
            assert_not_bigger_than(subdir, 1, ENOENT);

            switch(file)
            {
                case PROC_CMDLINE    :   /* /proc/cmdline */
                    PR_MALLOC(procbuf, strlen(kernel_cmdline) + 2);
                    //sprintf(procbuf, "%s\n", kernel_cmdline);
                    ksprintf(procbuf, strlen(kernel_cmdline) + 2, "%s\n",
                                kernel_cmdline);
                    buflen = strlen(procbuf);
                    break;

                case PROC_CPUINFO    :   /* /proc/cpuinfo     */
                case PROC_BUFFERS    :   /* /proc/buffers     */
                case PROC_DEVICES    :   /* /proc/devices     */
                case PROC_FILESYSTEMS:   /* /proc/filesystems */
                case PROC_KSYMS      :   /* /proc/ksyms       */
                case PROC_INTERRUPTS :   /* /proc/interrupts  */
                case PROC_LOADAVG    :   /* /proc/loadavg     */
                case PROC_MEMINFO    :   /* /proc/meminfo     */
                case PROC_MODULES    :   /* /proc/modules     */
                case PROC_MOUNTS     :   /* /proc/mounts      */
                case PROC_PARTITIONS :   /* /proc/partitions  */
                case PROC_STAT       :   /* /proc/stat        */
                case PROC_UPTIME     :   /* /proc/uptime      */
                case PROC_VERSION    :   /* /proc/version     */
                case PROC_VMSTAT     :   /* /proc/vmstat      */
                case PROC_SYSCALLS   :   /* /proc/syscalls    */
                    buflen = procfs_root_entries[file].read_file(&procbuf);
                    break;

                case PROC_TIMER_LIST :   /* /proc/timer_list */
                    /*
                     * TODO:
                     */
                    break;

                case PROC_SELF       :   /* /proc/self */
                    PR_MALLOC(procbuf, 16);
                    //sprintf(procbuf, "/proc/%u", tgid(ct));
                    ksprintf(procbuf, 16, "/proc/%u", tgid(this_core->cur_task));
                    buflen = strlen(procbuf);
                    break;

                case PROC_THREAD_SELF:   /* /proc/thread-self */
                    PR_MALLOC(procbuf, 32);
                    //sprintf(procbuf, "/proc/%u/task/%u", tgid(ct), ct->pid);
                    ksprintf(procbuf, 32, "/proc/%u/task/%u",
                                tgid(this_core->cur_task), this_core->cur_task->pid);
                    buflen = strlen(procbuf);
                    break;

                default:
                    break;
            }
            
            break;

        case DIR_BUS:
            break;

        case DIR_BUS_PCI:
            if(subdir == 0)
            {
                //assert_not_bigger_than(subdir, 1, ENOENT);

                switch(file)
                {
                    case PROC_BUS_PCI_DEVICES:  /* /proc/bus/pci/devices */
                        //buflen = get_pci_device_list(&procbuf);
                        buflen = procfs_bus_pci_entries[file].read_file(&procbuf);
                        break;
                }
            }
            else
            {
                if(file < 2)
                {
                    break;
                }

                struct pci_bus_t *bus;
                struct pci_dev_t *pci;

                if(!(bus = bus_from_number(subdir)))
                {
                    break;
                }

                if((pci = dev_from_number(bus, file)))
                {
                    buflen = get_pci_device_config_space(pci, &procbuf);

                    /*
                    printk("procfs_read_inode: [");
                    for(int i = 0; i < 256; i++) printk("%02x", procbuf[i]);
                    printk("]\n");
                    */

                    break;
                }
            }
            
            break;

        case DIR_PID:
            if(!(task = get_task_by_index(subdir)))
            {
                break;
            }

            switch(file)
            {
                case PROC_PID_CMDLINE   :   /* /proc/[pid]/cmdline */
                    if(task->state == TASK_ZOMBIE)
                    {
                        break;
                    }

                    buflen = procfs_get_task_args(task, file, &procbuf);
                    break;

                case PROC_PID_COMM      :   /* /proc/[pid]/comm */
                    PR_MALLOC(procbuf, strlen((char *)task->command) + 2);
                    //sprintf(procbuf, "%s\n", task->command);
                    ksprintf(procbuf, strlen((char *)task->command) + 2, "%s\n",
                                task->command);
                    buflen = strlen(procbuf);
                    break;

                case PROC_PID_ENVIRON   :   /* /proc/[pid]/environ */
                    buflen = procfs_get_task_args(task, file, &procbuf);
                    break;

                case PROC_PID_EXE       :   /* /proc/[pid]/exe */
                    if(!task->exe_dev || !task->exe_inode)
                    {
                        break;
                    }

                    PR_MALLOC(procbuf, 2048);
                    buflen = copy_task_dirpath(task->exe_dev,
                                               task->exe_inode, procbuf,
                                               2048, 1);
                    break;

                case PROC_PID_IO        :   /* /proc/[pid]/io */
                case PROC_PID_LIMITS    :   /* /proc/[pid]/limits */
                case PROC_PID_MAPS      :   /* /proc/[pid]/maps */
                case PROC_PID_STAT      :   /* /proc/[pid]/stat */
                case PROC_PID_STATM     :   /* /proc/[pid]/statm */
                case PROC_PID_STATUS    :   /* /proc/[pid]/status */
                case PROC_PID_TIMERS    :   /* /proc/[pid]/timers */
                    buflen = procfs_pid_entries[file].read_file((struct task_t *)task, &procbuf);
                    break;

                case PROC_PID_MEM       :   /* /proc/[pid]/mem */
                    /*
                     * TODO:
                     */
                    break;

                case PROC_PID_CWD       :   /* /proc/[pid]/cwd */
                    if(!task->fs || !task->fs->cwd)
                    {
                        break;
                    }

                    PR_MALLOC(procbuf, 2048);
                    buflen = copy_task_dirpath(task->fs->cwd->dev,
                                               task->fs->cwd->inode, procbuf,
                                               2048, 1);
                    break;

                case PROC_PID_ROOT      :   /* /proc/[pid]/root */
                    if(!task->fs || !task->fs->root)
                    {
                        break;
                    }

                    PR_MALLOC(procbuf, 2048);
                    buflen = copy_task_dirpath(task->fs->root->dev,
                                               task->fs->root->inode, procbuf,
                                               2048, 1);
                    break;

                default:
                    break;
            }
            
            break;

        case DIR_NET:
            if(subdir == 0)
            {
                //assert_not_bigger_than(subdir, 1, ENOENT);

                switch(file)
                {
                    case PROC_NET_RESOLV:  /* /proc/net/resolv.conf */
                    case PROC_NET_ARP:     /* /proc/net/arp */
                    case PROC_NET_DEV:     /* /proc/net/dev */
                    case PROC_NET_TCP:     /* /proc/net/tcp */
                    case PROC_NET_UDP:     /* /proc/net/udp */
                    case PROC_NET_UNIX:    /* /proc/net/unix */
                    case PROC_NET_RAW:     /* /proc/net/raw */
                        buflen = procfs_net_entries[file].read_file(&procbuf);
                        break;
                }
            }

            break;

        case DIR_TTY:
            if(subdir == 0)
            {
                //assert_not_bigger_than(subdir, 1, ENOENT);

                switch(file)
                {
                    case PROC_TTY_DRIVERS:  /* /proc/tty/drivers */
                        buflen = procfs_tty_entries[file].read_file(&procbuf);
                        break;
                }
            }

            break;

        case DIR_PID_FD:
        case DIR_PID_TASK:
        case DIR_SYS:
        default:
            break;
    }

    if((buflen == 0) || (i >= buflen))
    {
        if(procbuf)
        {
            kfree(procbuf);
        }
        
        return 0;
    }
    
    j = MIN((buflen - i), count);
    
    if(copy_to_user(buf, procbuf + i, j) != 0)
    {
        if(procbuf)
        {
            kfree(procbuf);
        }
        
        return -EFAULT;
    }

    (*pos) += j;

    if(procbuf)
    {
        kfree(procbuf);
    }

    update_atime(node);
    return j;
}

