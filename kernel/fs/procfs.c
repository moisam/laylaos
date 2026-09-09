/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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
 *  the procfs virtual filesystem. The procfs filesystem has been rewritten
 *  in kernel version 0.0.6 as the previous version was limited and not
 *  easy to extend, adding new directories was a pain in the bum.
 *
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
#include <kernel/common.h>
#include <kernel/acpi.h>
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
 *     inode = ((file) | ((subdir) << 16) | ((dir) << 24))
 *
 * The inode number consists of the following fields, which are interpreted
 * according to the file/directory the inode refers to:
 *
 * file/dir                         dir         subdir          file
 * -------------------------        ---         -----------     ----
 * /proc/[pid]                      1           task-index*     0
 *   files under /proc/[pid]        1           task-index*     [1+]
 * /proc/[pid]/fd                   2           task-index*     0
 *   files under /proc/[pid]/fd     2           task-index*     [1+]
 * /proc/[pid]/task                 3           task-index*     0
 *   dirs under /proc/[pid]/task    3           task-index*     [1+]
 * all other files under /proc      0           0               [2+]
 *
 * The task-index field is the task index within the global task table, when
 * it is accessed as an array. So the first task in the array has a task-index
 * of 0, and the last of NR_TASKS - 1. Note that a task's task-index is not
 * the same as its pid, as it refers to the task's slot in the task table, not
 * its identity. This was chosen as the task table is of finite and limited
 * size, currently 1024, which can be represented in 2 bytes, whereas pids can
 * reach high numbers and need more storage space (pid_t is 4-bytes long on x86).
 */

#define PROCFS_BLOCK_SIZE               512
#define PROCFS_ROOT_INODE               2
#define PROCFS_DEV_MIN                  0
#define PROCFS_DEV_MAJ                  243

#define CREATE_DIR_NODE(parent, name, inode, time)  \
    create_procfs_node(parent, name, NULL, NULL, inode, PROCFS_DIR_MODE, time);

#define CREATE_FILE_NODE(parent, name, func, funcarg, time)  \
    create_procfs_node(parent, name, func, funcarg, 0, PROCFS_FILE_MODE, time);

#define CREATE_LINK_NODE(parent, name, func, funcarg, time)  \
    create_procfs_node(parent, name, func, funcarg, 0, PROCFS_LINK_MODE, time);

// defined in drivers/pci.c
extern struct pci_bus_t *first_pci_bus;

// defined in fs/devfs.c
extern struct fs_node_t *devfs_root;

// struct to represent procfs nodes internally
struct procfs_node_t
{
    struct fs_node_t node;
    char name[32];
    struct procfs_node_t *next_sibling;
    struct procfs_node_t *parent;
    struct procfs_node_t *first_child, *last_child;
    size_t children;

#define PROCFS_NODE_FLAG_IS_PCI         0x01
    int flags;

    void *read_file_arg;

    union
    {
        size_t (*read_file)(char **, void *);   // function to read proc file contents
        struct pci_dev_t *pci;                  // pointer to pci device for nodes 
                                                // under /proc/bus/pci
    };
};


// device id for procfs
dev_t PROCFS_DEVID = TO_DEVID(PROCFS_DEV_MAJ, PROCFS_DEV_MIN);

// make sure procfs is init'ed only once
static int procfs_inited = 0;

struct procfs_node_t *procfs_root;


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


struct procfs_pid_entry_t
{
    char *name;
    mode_t mode;
    time_t atime, mtime, ctime;
    size_t (*read_file)(struct task_t *, char **);  // function to read proc
                                                    //   file contents
};

#define arr_count(a)        (int)(sizeof(a) / sizeof(a[0]))

struct procfs_pid_entry_t procfs_pid_entries[] =
{
    { "."               , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
    { ".."              , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_PID_CMDLINE    2
    { "cmdline"         , PROCFS_FILE_MODE, 0, 0, 0, NULL, },
#define PROC_PID_COMM       3
    { "comm"            , PROCFS_FILE_MODE, 0, 0, 0, get_task_comm, },
#define PROC_PID_CWD        4
    { "cwd"             , PROCFS_LINK_MODE, 0, 0, 0, get_task_cwd, },
#define PROC_PID_ENVIRON    5
    { "environ"         , PROCFS_FILE_MODE, 0, 0, 0, NULL, },
#define PROC_PID_EXE        6
    { "exe"             , PROCFS_LINK_MODE, 0, 0, 0, get_task_exe, },
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
#define PROC_PID_MOUNTINFO  12
    { "mountinfo"       , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
#define PROC_PID_MOUNTSTATS 13
    { "mountstats"      , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
#define PROC_PID_MOUNTS     14
    { "mounts"          , PROCFS_LINK_MODE, 0, 0, 0, NULL, },
#define PROC_PID_ROOT       15
    { "root"            , PROCFS_LINK_MODE, 0, 0, 0, get_task_root, },
#define PROC_PID_STAT       16
    { "stat"            , PROCFS_FILE_MODE, 0, 0, 0, get_task_stat, },
#define PROC_PID_STATM      17
    { "statm"           , PROCFS_FILE_MODE, 0, 0, 0, get_task_statm, },
#define PROC_PID_STATUS     18
    { "status"          , PROCFS_FILE_MODE, 0, 0, 0, get_task_status, },
#define PROC_PID_SMAPS      19
    { "smaps"           , PROCFS_FILE_MODE, 0, 0, 0, get_task_smaps, },
#define PROC_PID_TASK       20
    { "task"            , PROCFS_DIR_MODE , 0, 0, 0, NULL, },
#define PROC_PID_TIMERS     21
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


static struct procfs_node_t *create_procfs_node(struct procfs_node_t *parent,
                                                char *name,
                                                size_t (*read_file)(char **, void *),
                                                void *read_file_arg,
                                                ino_t n, mode_t mode, time_t t)
{
    // inodes 0 and 1 are unused
    // root inode is 2
    // inodes from 3 to (NR_TASKS + 2) are reserved for task entries
    // rest of procfs inode numbers start from (NR_TASKS + 3)
    static volatile int next_inode = PROCFS_ROOT_INODE + NR_TASKS + 1;
    struct procfs_node_t *node;

    if(!(node = kmalloc(sizeof(struct procfs_node_t))))
    {
        kpanic("procfs: failed to create node\n");
    }

    A_memset(node, 0, sizeof(struct procfs_node_t));
    node->node.ops = &procfs_ops;
    node->node.mode = mode;
    node->node.atime = t;
    node->node.mtime = t;
    node->node.ctime = t;
    node->node.uid = 0;
    node->node.gid = 0;
    node->node.size = S_ISDIR(mode) ? PROCFS_BLOCK_SIZE : 0;
    node->node.links = S_ISDIR(mode) ? 2 : 1;
    node->node.refs = 1;

    // use one of the reserved dev ids
    node->node.dev = PROCFS_DEVID;

    if(n == 0)
    {
        node->node.inode = next_inode++;
    }
    else
    {
        node->node.inode = n;
    }

    node->read_file = read_file;
    node->read_file_arg = read_file_arg;
    node->parent = parent;
    node->children = 0;
    node->next_sibling = NULL;
    node->first_child = NULL;
    node->last_child = NULL;
    node->flags = 0;

    if(name)
    {
        strcpy(node->name, name);
    }

    if(parent)
    {
        if(parent->last_child)
        {
            parent->last_child->next_sibling = node;
            parent->last_child = node;
        }
        else
        {
            parent->first_child = node;
            parent->last_child = node;
        }

        parent->children++;
    }

    return node;
}


static void add_acpi_nodes(struct procfs_node_t *nacpi, time_t t)
{
    struct procfs_node_t *nbat, *ngpe0;
    void *arg;
    char tmp[8];
    int i;
    uint32_t j;

    // if we have battery info, add a /proc/acpi/BAT0 subdirectory and
    // populate it
    for(i = 0; i < sys_batinfo.count; i++)
    {
        ksprintf(tmp, sizeof(tmp), "BAT%d", i);
        arg = &sys_batinfo.bat[i];

        nbat = CREATE_DIR_NODE(nacpi, tmp, 0, t);

        CREATE_DIR_NODE(nbat, ".", nbat->node.inode, t);
        CREATE_DIR_NODE(nbat, "..", nacpi->node.inode, t);

        CREATE_FILE_NODE(nbat, "present", get_bat_present, arg, t);
        CREATE_FILE_NODE(nbat, "capacity", get_bat_capacity, arg, t);
        CREATE_FILE_NODE(nbat, "info", get_bat_info, arg, t);
        CREATE_FILE_NODE(nbat, "status", get_bat_status, arg, t);
        CREATE_FILE_NODE(nbat, "model_name", get_bat_model_name, arg, t);
        CREATE_FILE_NODE(nbat, "serial_number", get_bat_serial_number, arg, t);
        CREATE_FILE_NODE(nbat, "type", get_bat_type, arg, t);
        CREATE_FILE_NODE(nbat, "manufacturer", get_bat_manufacturer, arg, t);
        CREATE_FILE_NODE(nbat, "charge_full", get_bat_charge_full, arg, t);
        CREATE_FILE_NODE(nbat, "charge_full_design", get_bat_charge_full_design, arg, t);
        CREATE_FILE_NODE(nbat, "charge_now", get_bat_charge_now, arg, t);
        CREATE_FILE_NODE(nbat, "technology", get_bat_technology, arg, t);
        CREATE_FILE_NODE(nbat, "cycle_count", get_bat_cycle_count, arg, t);
    }

    // if we have GPE info, add a /proc/acpi/interrupts subdirectory and
    // populate it
    if(gpe0_count)
    {
        ngpe0 = CREATE_DIR_NODE(nacpi, "interrupts", 0, t);

        CREATE_DIR_NODE(ngpe0, ".", ngpe0->node.inode, t);
        CREATE_DIR_NODE(ngpe0, "..", nacpi->node.inode, t);

        for(j = 0; j < gpe0_count; j++)
        {
            ksprintf(tmp, sizeof(tmp), "gpe%02X", j);
            CREATE_FILE_NODE(ngpe0, tmp, get_gpe, (void *)(uintptr_t)j, t);
        }

        if(gpe1_base && gpe1_count)
        {
            for(j = 0; j < gpe1_count; j++)
            {
                ksprintf(tmp, sizeof(tmp), "gpe%02X", gpe1_base + j);
                CREATE_FILE_NODE(ngpe0, tmp, get_gpe, (void *)(uintptr_t)(gpe1_base + j), t);
            }
        }
    }
}


static void add_pci_bus_nodes(struct procfs_node_t *nbuspci, time_t t)
{
    struct procfs_node_t *busnode, *devnode;
    volatile struct pci_bus_t *bus;
    volatile struct pci_dev_t *pci;
    char tmp[8];

    for(bus = first_pci_bus; bus != NULL; bus = bus->next)
    {
        // for each bus, create a subdirectory under /proc/bus/pci
        ksprintf(tmp, sizeof(tmp), "%02x", bus->bus);
        busnode = CREATE_DIR_NODE(nbuspci, tmp, 0, t);

        CREATE_DIR_NODE(busnode, ".", busnode->node.inode, t);
        CREATE_DIR_NODE(busnode, "..", nbuspci->node.inode, t);

        // then create a node for each pci device under the new subdir
        for(pci = bus->first; pci != NULL; pci = pci->next)
        {
            ksprintf(tmp, sizeof(tmp), "%02x.%02x", pci->dev, pci->function);
            devnode = CREATE_FILE_NODE(busnode, tmp, NULL, NULL, t);
            devnode->pci = (struct pci_dev_t *)pci;
            devnode->flags = PROCFS_NODE_FLAG_IS_PCI;
        }
    }
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
    struct procfs_node_t *nbus, *npci, *nsys, *ntty, *nnet, *nacpi;
    time_t t = now();
    
    if(procfs_inited)
    {
        printk("procfs: trying to re-init procfs\n");
        return (struct fs_node_t *)procfs_root;
    }

    // create root inode
    procfs_root = CREATE_DIR_NODE(NULL, NULL, PROCFS_ROOT_INODE, t);
    procfs_root->node.ops = &procfs_ops;
    procfs_root->node.refs++;

    // create all the static inodes (i.e. everything not under /proc/[pid])

    // files and dirs under /proc
    CREATE_DIR_NODE(procfs_root, ".", PROCFS_ROOT_INODE, t);
    CREATE_DIR_NODE(procfs_root, "..", PROCFS_ROOT_INODE, t);
    nacpi = CREATE_DIR_NODE(procfs_root, "acpi", 0, t);
    nbus = CREATE_DIR_NODE(procfs_root, "bus", 0, t);
    nsys = CREATE_DIR_NODE(procfs_root, "sys", 0, t);
    ntty = CREATE_DIR_NODE(procfs_root, "tty", 0, t);
    nnet = CREATE_DIR_NODE(procfs_root, "net", 0, t);

    CREATE_FILE_NODE(procfs_root, "buffers", get_buffer_info, NULL, t);
    CREATE_FILE_NODE(procfs_root, "cmdline", get_cmdline, NULL, t);
    CREATE_FILE_NODE(procfs_root, "cpuinfo", detect_cpu, NULL, t);
    CREATE_FILE_NODE(procfs_root, "devices", get_device_list, NULL, t);
    CREATE_FILE_NODE(procfs_root, "filesystems", get_fs_list, NULL, t);
    CREATE_FILE_NODE(procfs_root, "interrupts", get_interrupt_info, NULL, t);
    CREATE_FILE_NODE(procfs_root, "loadavg", get_loadavg, NULL, t);
    CREATE_FILE_NODE(procfs_root, "meminfo", get_meminfo, NULL, t);
    CREATE_FILE_NODE(procfs_root, "modules", get_modules, NULL, t);
    CREATE_FILE_NODE(procfs_root, "mountinfo", get_mountinfo, NULL, t);
    CREATE_FILE_NODE(procfs_root, "mountstats", get_mountstats, NULL, t);
    CREATE_FILE_NODE(procfs_root, "mounts", get_mounts, NULL, t);
    CREATE_FILE_NODE(procfs_root, "partitions", get_partitions, NULL, t);
    CREATE_FILE_NODE(procfs_root, "stat", get_sysstat, NULL, t);
    CREATE_FILE_NODE(procfs_root, "timer_list", NULL, NULL, t);
    CREATE_FILE_NODE(procfs_root, "uptime", get_uptime, NULL, t);
    CREATE_FILE_NODE(procfs_root, "version", get_version, NULL, t);
    CREATE_FILE_NODE(procfs_root, "vmstat", get_vmstat, NULL, t);
    CREATE_FILE_NODE(procfs_root, "ksyms", get_ksyms, NULL, t);
    CREATE_FILE_NODE(procfs_root, "syscalls", get_syscalls, NULL, t);

    CREATE_LINK_NODE(procfs_root, "self", get_self, NULL, t);
    CREATE_LINK_NODE(procfs_root, "thread-self", get_thread_self, NULL, t);

    // files and dirs under /proc/acpi
    CREATE_DIR_NODE(nacpi, ".", nacpi->node.inode, t);
    CREATE_DIR_NODE(nacpi, "..", PROCFS_ROOT_INODE, t);
    add_acpi_nodes(nacpi, t);

    // files and dirs under /proc/bus
    CREATE_DIR_NODE(nbus, ".", nbus->node.inode, t);
    CREATE_DIR_NODE(nbus, "..", PROCFS_ROOT_INODE, t);
    npci = CREATE_DIR_NODE(nbus, "pci", 0, t);

    // files and dirs under /proc/bus/pci
    CREATE_DIR_NODE(npci, ".", npci->node.inode, t);
    CREATE_DIR_NODE(npci, "..", nbus->node.inode, t);
    CREATE_FILE_NODE(npci, "devices", get_pci_device_list, NULL, t);
    add_pci_bus_nodes(npci, t);

    // files and dirs under /proc/sys
    CREATE_DIR_NODE(nsys, ".", nsys->node.inode, t);
    CREATE_DIR_NODE(nsys, "..", PROCFS_ROOT_INODE, t);

    // files and dirs under /proc/net
    CREATE_DIR_NODE(nnet, ".", nnet->node.inode, t);
    CREATE_DIR_NODE(nnet, "..", PROCFS_ROOT_INODE, t);
    CREATE_FILE_NODE(nnet, "arp", get_arp_list, NULL, t);
    CREATE_FILE_NODE(nnet, "dev", get_net_dev_stats, NULL, t);
    CREATE_FILE_NODE(nnet, "tcp", get_net_tcp, NULL, t);
    CREATE_FILE_NODE(nnet, "udp", get_net_udp, NULL, t);
    CREATE_FILE_NODE(nnet, "unix", get_net_unix, NULL, t);
    CREATE_FILE_NODE(nnet, "raw", get_net_raw, NULL, t);
    CREATE_FILE_NODE(nnet, "resolv.conf", get_dns_list, NULL, t);

    // files and dirs under /proc/tty
    CREATE_DIR_NODE(ntty, ".", ntty->node.inode, t);
    CREATE_DIR_NODE(ntty, "..", PROCFS_ROOT_INODE, t);
    CREATE_FILE_NODE(ntty, "drivers", get_tty_driver_list, NULL, t);

    // some user programs that call getdents() don't read past the directory's
    // size, so we estimate a size large enough to ensure someone who reads
    // the root directory gets all the entries they need (we use an average of
    // 8 chars per entry name just for approximation).
    procfs_root->node.size = (sizeof(struct dirent) + 8) *
                            (procfs_root->children + NR_TASKS);
    procfs_root->node.links = procfs_root->children;

    procfs_inited = 1;

    return (struct fs_node_t *)procfs_root;
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
    d->root = (struct fs_node_t *)procfs_root;

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


static struct procfs_node_t *search_tree_for_inode(struct procfs_node_t *parent, int ino)
{
    struct procfs_node_t *pnode, *res;

    for(pnode = parent->first_child; pnode != NULL; pnode = pnode->next_sibling)
    {
        if(pnode->name[0] == '.')   // ignore '.' and '..'
        {
            continue;
        }

        if((int)pnode->node.inode == ino)
        {
            return pnode;
        }

        if((res = search_tree_for_inode(pnode, ino)))
        {
            return res;
        }
    }

    return NULL;
}


static struct procfs_node_t *find_child_by_name(struct procfs_node_t *parent, 
                                                char *name, volatile int *index)
{
    volatile struct procfs_node_t *pnode;
    volatile int i = 0;

    *index = 0;

    for(pnode = parent->first_child; pnode != NULL; pnode = pnode->next_sibling, i++)
    {
        if(strcmp((void *)pnode->name, name) == 0)
        {
            *index = i;
            return (struct procfs_node_t *)pnode;
        }
    }

    return NULL;
}


static struct procfs_node_t *find_child_by_inode(struct procfs_node_t *parent, 
                                                 int ino, volatile int *index)
{
    volatile struct procfs_node_t *pnode;
    volatile int i = 0;

    *index = 0;

    for(pnode = parent->first_child; pnode != NULL; pnode = pnode->next_sibling, i++)
    {
        if((int)pnode->node.inode == ino)
        {
            *index = i;
            return (struct procfs_node_t *)pnode;
        }
    }

    return NULL;
}


void copy_procfs_node_attribs(struct fs_node_t *node,
                              struct procfs_node_t *pnode)
{
    node->mode = pnode->node.mode;
    node->atime = pnode->node.atime;
    node->mtime = pnode->node.mtime;
    node->ctime = pnode->node.ctime;
    node->uid = pnode->node.uid;
    node->gid = pnode->node.gid;
    node->size = pnode->node.size;
    node->links = pnode->node.links;
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


/*
 * This will mark the task struct as busy. Caller must unset the flag!
 */
volatile struct task_t *get_task_by_index(int i)
{
    if(i < 0 || i >= NR_TASKS)
    {
        return NULL;
    }

    if(task_table[i] != NULL)
    {
        __sync_or_and_fetch(&task_table[i]->properties, PROPERTY_STRUCT_BUSY);
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


ino_t procfs_pid_entry_inode(int subdir, int offset)
{
    ino_t ino;

    if(offset == 0)
    {
        ino = MAKE_PROCFS_INODE(DIR_PID, subdir, 0);
    }
    else if(offset == 1)
    {
        ino = PROCFS_ROOT_INODE;
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
        ino = MAKE_PROCFS_INODE(DIR_PID, subdir, offset);
    }

    return ino;
}


#define assert_not_bigger_than(f, n, e, t)                  \
    if(f < 0 || f >= (int)n) {                              \
        if(t) __sync_and_and_fetch(&t->properties,          \
                                      ~PROPERTY_STRUCT_BUSY);\
        return -e;                                          \
    }

#define assert_not_bigger_than2(f, n, e)                    \
    if(f < 0 || f >= (int)n) {                              \
        return -e;                                          \
    }

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
    struct procfs_node_t *pnode;
    volatile struct task_t *task, *task2;
    
    KDEBUG("procfs_read_inode: dir %d, subdir %d, file %d\n", dir, subdir, file);

    // 1 - check special directories /proc/pid, /proc/pid/fd and /proc/pid/task
    switch(dir)
    {
        case DIR_PID:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            assert_not_bigger_than(file, procfs_pid_entry_count, ENOENT, task);
            copy_pid_node_attribs(node, task, procfs_pid_entries[file].mode);
            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);

            return 0;

        case DIR_PID_FD:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }
            
            if(!file)       // '.'
            {
                copy_pid_node_attribs(node, task, PROCFS_DIR_MODE);
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return 0;
            }

            if(!validfd(file - 1, task))
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return -ENOENT;
            }

            copy_pid_node_attribs(node, task, PROCFS_LINK_MODE);
            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
            return 0;

        case DIR_PID_TASK:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            if(!file)       // '.'
            {
                copy_pid_node_attribs(node, task, PROCFS_DIR_MODE);
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return 0;
            }

            if(!(task2 = get_task_by_index(file - 1)))
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return -ENOENT;
            }

            copy_pid_node_attribs(node, task2, PROCFS_DIR_MODE);
            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
            return 0;
    }

    // all "non-special" inodes should have dir == 0 && subdir == 0
    // inodes 0 and 1 do not exist
    if(dir || subdir || file < 2)
    {
        return -ENOENT;
    }

    // 2 - check the root node
    if(file == 2)
    {
        copy_procfs_node_attribs(node, procfs_root);
        return 0;
    }

    // 3 - check /proc/[pid] subdirs
    if(file < PROCFS_ROOT_INODE + NR_TASKS + 1)
    {
        file -= PROCFS_ROOT_INODE;

        if(task_table[file])
        {
            copy_pid_node_attribs(node, task_table[file], PROCFS_DIR_MODE);
            return 0;
        }

        return -ENOENT;
    }

    // 4 - check the rest of /procfs inodes
    if((pnode = search_tree_for_inode(procfs_root, file)))
    {
        copy_procfs_node_attribs(node, pnode);
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
struct dirent *procfs_entry_to_dirent(ino_t ino, mode_t mode, char *name,
                                      int off, struct dirent *__ent)
{
    int namelen = strlen(name);
    unsigned int reclen = GET_DIRENT_LEN(namelen);
    struct dirent *entry = __ent ? __ent : kmalloc(reclen);

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


/*
 * Find the given filename in the parent directory.
 *
 * Inputs:
 *    dirnode => the parent directory's node
 *    filename => the searched-for filename
 *
 * Outputs:
 *    entry => if the filename is found, its entry is converted to a kmalloc'd
 *             dirent struct, and the result is stored in this field
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long procfs_finddir(struct fs_node_t *dirnode, char *filename,
                    struct dirent **entry)
{
    if(!valid_procfs_node(dirnode))
    {
        return -EINVAL;
    }

    // for safety
    /*
    *entry = NULL;
    */

    int dir = INODE_DIR_BITS(dirnode->inode);
    int subdir = INODE_SUBDIR_BITS(dirnode->inode);
    int file = INODE_FILE_BITS(dirnode->inode);
    volatile int i;
    volatile ino_t ino;
    volatile mode_t mode;
    char tmp[16];
    struct procfs_node_t *pnode, *cpnode;
    volatile struct task_t *task, *thread;

    KDEBUG("%s: d %d, s %d, f %d\n", __func__, dir, subdir, file);
    
    // 1 - check special directories /proc/pid, /proc/pid/fd and /proc/pid/task
    switch(dir)
    {
        case DIR_PID:
            if(subdir < 0 || subdir >= NR_TASKS)
            {
                return -ENOENT;
            }
            
            assert_not_bigger_than2(file, 1, ENOTDIR);

            for(i = 0; i < (int)procfs_pid_entry_count; i++)
            {
                if(strcmp(filename, procfs_pid_entries[i].name) == 0)
                {
                    ino = procfs_pid_entry_inode(subdir, i);

                    *entry = procfs_entry_to_dirent(ino,
                                            procfs_pid_entries[i].mode,
                                            procfs_pid_entries[i].name, i, *entry);
                    return *entry ? 0 : -ENOMEM;
                }
            }
            
            return -ENOENT;

        case DIR_PID_FD:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            assert_not_bigger_than(file, 1, ENOTDIR, task);

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

                    ksprintf(tmp, sizeof(tmp), "%d", i);
                
                    if(strcmp(tmp, filename) == 0)
                    {
                        ino = MAKE_PROCFS_INODE(dir, subdir, ++i);
                        mode = PROCFS_LINK_MODE;
                        break;
                    }
                }
                
                if(ino == 0)
                {
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    return -ENOENT;
                }
            }

            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);

            *entry = procfs_entry_to_dirent(ino, mode, filename, i, *entry);
            return *entry ? 0 : -ENOMEM;

        case DIR_PID_TASK:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            assert_not_bigger_than(file, 1, ENOTDIR, task);

            if(!task->threads)
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return -ENOENT;
            }

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
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    return -ENOENT;
                }
            }

            kernel_mutex_unlock(&(task->threads->mutex));
            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);

            *entry = procfs_entry_to_dirent(ino, PROCFS_DIR_MODE,
                                            filename, i, *entry);
            return *entry ? 0 : -ENOMEM;
    }

    // all "non-special" inodes should have dir == 0 && subdir == 0
    // inodes 0 and 1 do not exist
    if(dir || subdir || file < 2)
    {
        return -ENOENT;
    }

    // get parent node
    pnode = (file == 2) ? procfs_root : search_tree_for_inode(procfs_root, file);

    if(!pnode)
    {
        return -ENOENT;
    }

    // 2 - check the rest of /procfs inodes
    if((cpnode = find_child_by_name(pnode, filename, &i)))
    {
        *entry = procfs_entry_to_dirent(cpnode->node.inode,
                                        cpnode->node.mode,
                                        cpnode->name, i, *entry);
        return *entry ? 0 : -ENOMEM;
    }

    // 3 - check [pid] dirs under the root node
    if(file == 2 && *filename >= '0' && *filename <= '9')
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
                ino = MAKE_PROCFS_INODE(DIR_PID, t - task_table, 0);
                ksprintf(tmp, sizeof(tmp), "%d", tgid(*t));
                *entry = procfs_entry_to_dirent(ino,
                                        PROCFS_DIR_MODE, tmp, 
                                        procfs_root->children + (t - task_table),
                                        *entry);
                return *entry ? 0 : -ENOMEM;
            }
        }
    }

    return -ENOENT;
}


/*
 * Find the given inode in the parent directory.
 * Called during pathname resolution when constructing the absolute pathname
 * of a given inode.
 *
 * Inputs:
 *    dirnode => the parent directory's node
 *    node => the searched-for inode
 *
 * Outputs:
 *    entry => if the node is found, its entry is converted to a kmalloc'd
 *             dirent struct, and the result is stored in this field
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long procfs_finddir_by_inode(struct fs_node_t *dirnode, struct fs_node_t *node,
                             struct dirent **entry)
{
    if(!valid_procfs_node(dirnode))
    {
        return -EINVAL;
    }

    // for safety
    *entry = NULL;

    int dir = INODE_DIR_BITS(dirnode->inode);
    int subdir = INODE_SUBDIR_BITS(dirnode->inode);
    int file = INODE_FILE_BITS(dirnode->inode);
    volatile int child_dir, child_subdir, child_file;
    volatile int i;
    //volatile ino_t ino;
    char tmp[16];
    struct procfs_node_t *pnode, *cpnode;
    volatile struct task_t *task, *thread;

    child_dir = INODE_DIR_BITS(node->inode);
    child_subdir = INODE_SUBDIR_BITS(node->inode);
    child_file = INODE_FILE_BITS(node->inode);

    KDEBUG("%s: d %d, s %d, f %d (cd %d, cs %d, cf %d)\n", __func__, dir, subdir, file, child_dir, child_subdir, child_file);
    
    // 1 - check special directories /proc/pid, /proc/pid/fd and /proc/pid/task
    switch(dir)
    {
        case DIR_PID:
            if(subdir < 0 || subdir >= NR_TASKS)
            {
                break;
            }
            
            assert_not_bigger_than2(file, 1, ENOTDIR);

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
                                            procfs_pid_entries[i].name, i, NULL);
            return *entry ? 0 : -ENOMEM;

        case DIR_PID_FD:
            if(!(task = get_task_by_index(subdir)))
            {
                break;
            }

            assert_not_bigger_than(file, 1, ENOTDIR, task);
            i = child_file;

            if(child_dir != dir || child_subdir != subdir)
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                break;
            }
            
            if(i == 0)
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                *entry = procfs_entry_to_dirent(node->inode,
                                                PROCFS_DIR_MODE,
                                                ".", i, NULL);
                return *entry ? 0 : -ENOMEM;
            }
            
            if(i > NR_OPEN)
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                break;
            }

            if(!task->ofiles->ofile[i - 1])
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                break;
            }

            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
            //sprintf(tmp, "%d", i - 1);
            ksprintf(tmp, sizeof(tmp), "%d", i - 1);
            *entry = procfs_entry_to_dirent(node->inode,
                                            PROCFS_LINK_MODE, tmp, i, NULL);
            return *entry ? 0 : -ENOMEM;

        case DIR_PID_TASK:
            if(!(task = get_task_by_index(subdir)))
            {
                break;
            }

            assert_not_bigger_than(file, 1, ENOTDIR, task);

            if(dir == child_dir || dir == DIR_PID)
            {
                if(subdir != child_subdir)
                {
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    break;
                }
                
                //sprintf(tmp, (dir == DIR_PID) ? ".." : ".");
                ksprintf(tmp, sizeof(tmp), (dir == DIR_PID) ? ".." : ".");
                i = 0;
            }
            else if(child_dir == 0)
            {
                pid_t pid = 0;

                if(!task->threads)
                {
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    break;
                }

                kernel_mutex_lock(&(task->threads->mutex));

                for_each_thread(thread, task)
                {
                    if(--child_subdir == 0)
                    {
                        break;
                    }
                }

                if(thread)
                {
                    pid = thread->pid;
                }

                kernel_mutex_unlock(&(task->threads->mutex));
                
                if(!thread)
                {
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    break;
                }

                //sprintf(tmp, "%d", thread->pid);
                ksprintf(tmp, sizeof(tmp), "%d", pid);
                i = 2;
            }
            else
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                break;
            }

            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);

            *entry = procfs_entry_to_dirent(node->inode,
                                            PROCFS_DIR_MODE, tmp, i, NULL);
            return *entry ? 0 : -ENOMEM;
    }

    // all "non-special" inodes should have dir == 0 && subdir == 0
    // inodes 0 and 1 do not exist
    if(dir || subdir || file < 2)
    {
        return -ENOENT;
    }

    // get parent node
    pnode = (file == 2) ? procfs_root : search_tree_for_inode(procfs_root, file);

    if(!pnode)
    {
        return -ENOENT;
    }

    // 2 - check the rest of /procfs inodes
    if((cpnode = find_child_by_inode(pnode, node->inode, &i)))
    {
        *entry = procfs_entry_to_dirent(cpnode->node.inode,
                                        cpnode->node.mode,
                                        cpnode->name, i, NULL);
        return *entry ? 0 : -ENOMEM;
    }

    // 3 - check [pid] dirs under the root node
    if(file == 2 && child_dir == DIR_PID && child_file == 0)
    {
        if(child_subdir >= 0 && child_subdir < NR_TASKS &&
           task_table[child_subdir])
        {
            ksprintf(tmp, sizeof(tmp), "%d",
                                tgid(task_table[child_subdir]));
            *entry = procfs_entry_to_dirent(node->inode,
                                            PROCFS_DIR_MODE, tmp,
                                            procfs_root->children + child_subdir, NULL);
            return *entry ? 0 : -ENOMEM;
        }
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
    struct procfs_node_t *pnode, *cpnode;
    char tmp[16];

    offset = *pos;
    
    switch(dir)
    {
        case DIR_PID:
            if(subdir < 0 || subdir >= NR_TASKS)
            {
                return -ENOENT;
            }
            
            assert_not_bigger_than2(file, 1, ENOTDIR);

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

            assert_not_bigger_than(file, 1, ENOTDIR, task);

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

            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
            *pos = offset;
            return count;

        case DIR_PID_TASK:
            if(!(task = get_task_by_index(subdir)))
            {
                return -ENOENT;
            }

            assert_not_bigger_than(file, 1, ENOTDIR, task);

            if(!task->threads)
            {
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return -ENOENT;
            }

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

                    ino = MAKE_PROCFS_INODE(0, procfs_root->children +
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

                    ino = MAKE_PROCFS_INODE(0, procfs_root->children +
                                               get_index_for_task(thread), 0);
                    //sprintf(tmp, "%d", thread->pid);
                    ksprintf(tmp, sizeof(tmp), "%d", thread->pid);
                }
                
                mode = PROCFS_DIR_MODE;
                copy_dent(tmp);
                offset++;
            }

            kernel_mutex_unlock(&(task->threads->mutex));
            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);

            *pos = offset;
            return count;
    }

    // all "non-special" inodes should have dir == 0 && subdir == 0
    // inodes 0 and 1 do not exist
    if(dir || subdir || file < 2)
    {
        return -ENOENT;
    }

    // get parent node
    pnode = (file == 2) ? procfs_root : search_tree_for_inode(procfs_root, file);

    if(!pnode)
    {
        return -ENOENT;
    }

    // skip to the entry at the given offset
    for(count = 0, cpnode = pnode->first_child; 
        cpnode != NULL; 
        cpnode = cpnode->next_sibling, count++)
    {
        if(count == offset)
        {
            break;
        }
    }

    count = 0;

    // 2 - check the root node
    if(file == 2)
    {
        while(1)
        {
            // "normal" entries
            if(offset < pnode->children)
            {
                if(!cpnode)
                {
                    return -ENOENT;
                }

                ino = cpnode->node.inode;
                name = cpnode->name;
                mode = cpnode->node.mode;
                copy_dent(name);
                offset++;
                cpnode = cpnode->next_sibling;
            }
            // [pid] dirs
            else
            {
                i = offset - pnode->children;
                volatile int found = 0;

                for_each_taskptr(t)
                {
                    if(!*t || (*t)->pid != tgid(*t))
                    {
                        continue;
                    }

                    if(i-- == 0)
                    {
                        ino = MAKE_PROCFS_INODE(DIR_PID, t - task_table, 0);
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
            }
        }

        *pos = offset;
        return count;
    }

    // 2 - check the rest of /procfs inodes
    while(offset < pnode->children)
    {
        if(!cpnode)
        {
            return -ENOENT;
        }

        ino = cpnode->node.inode;
        name = cpnode->name;
        mode = cpnode->node.mode;
        copy_dent(name);
        offset++;
        cpnode = cpnode->next_sibling;
    }

    *pos = offset;
    return count;
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


static long get_devfs_path(struct fs_node_t *node, char *buf, size_t bufsz)
{
    struct dirent *entry = NULL;
    int res;

    if((res = devfs_finddir_by_inode(devfs_root, node, &entry)) < 0)
    {
        return res;
    }

    ksprintf(buf, bufsz, "/dev/%s", entry->d_name);
    kfree(entry);
    return strlen(buf);
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

    struct procfs_node_t *pnode;
    volatile struct task_t *task;
    struct fs_node_t *node;
    struct file_t *f;
    char *p = NULL;
    long res = 0;
    struct dentry_t *dent = NULL;

    switch(dir)
    {
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
                        __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                        return 0;
                    }

                    res = copy_task_dirpath(task->fs->cwd->dev,
                                            task->fs->cwd->inode,
                                            buf, bufsz, kernel);
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    return res;

                case PROC_PID_EXE       :   /* /proc/[pid]/exe */
                    if(!task->exe_path)
                    {
                        *buf = '\0';
                        __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                        return 0;
                    }

                    ksprintf(buf, bufsz, "%s", task->exe_path);
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    return strlen(buf);

                case PROC_PID_ROOT      :   /* /proc/[pid]/root */
                    if(!task->fs || !task->fs->root)
                    {
                        *buf = '\0';
                        __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                        return 0;
                    }

                    res = copy_task_dirpath(task->fs->root->dev,
                                            task->fs->root->inode,
                                            buf, bufsz, kernel);
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    return res;

                case PROC_PID_MOUNTS    :   /* /proc/[pid]/mounts */
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    ksprintf(buf, bufsz, "/proc/mounts");
                    return strlen(buf);

                case PROC_PID_MOUNTSTATS:   /* /proc/[pid]/mountstats */
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    ksprintf(buf, bufsz, "/proc/mountstats");
                    return strlen(buf);

                case PROC_PID_MOUNTINFO :   /* /proc/[pid]/mountinfo */
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    ksprintf(buf, bufsz, "/proc/mountinfo");
                    return strlen(buf);

                default:
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
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
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return -EINVAL;
            }

            KDEBUG("%s: dev 0x%x, inode 0x%x\n", __func__, node->dev, node->inode);

            /*
             * First handle special files, e.g. sockets, pipes, ...
             */

            //if(S_ISSOCK(node->mode))
            if(IS_SOCKET(node))
            {
                /*
                 * TODO: fix this when we implement socket node numbers.
                 */
                ksprintf(buf, bufsz, "socket:[%d]", node->inode);
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return strlen(buf);
            }

            if(IS_PIPE(node))
            {
                /*
                 * TODO: fix this when we implement pipe node numbers.
                 */
                ksprintf(buf, bufsz, "pipe:[%d]", node->inode);
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return strlen(buf);
            }

            if(S_ISCHR(node->mode))
            {
                if(MAJOR(node->blocks[0]) == PTY_MASTER_MAJ)
                {
                    ksprintf(buf, bufsz, "/dev/ptmx");
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    return strlen(buf);
                }

                if(MAJOR(node->blocks[0]) == PTY_SLAVE_MAJ)
                {
                    ksprintf(buf, bufsz, "/dev/pts/%d", MINOR(node->blocks[0]));
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    return strlen(buf);
                }
            }

            if(node->dev == DEV_DEVID)
            {
                res = get_devfs_path(node, buf, bufsz);
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return res;
            }

            /*
             * Next handle normal files
             */

            if((node = get_node(node->dev, node->inode, GETNODE_FOLLOW_MPOINTS)) == NULL)
            {
                __asm__ __volatile__("xchg %%bx, %%bx":::);
                KDEBUG("%s: no node, dev 0x%x, ino 0x%x\n", __func__, node->dev, node->inode);
                __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                return -EINVAL;
            }

            __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);

            if((res = get_dentry(node, &dent)) < 0)
            {
                __asm__ __volatile__("xchg %%bx, %%bx":::);
                KDEBUG("%s: no dent %d, dev 0x%x, ino 0x%x\n", __func__, res, node->dev, node->inode);
                release_node(node);
                return res;
            }

            KDEBUG("%s: path %s\n", __func__, dent->path ? dent->path : "null");

            if(!dent->path)
            {
                __asm__ __volatile__("xchg %%bx, %%bx":::);
                KDEBUG("%s: no path, dev 0x%x, ino 0x%x\n", __func__, node->dev, node->inode);
                release_dentry(dent);
                release_node(node);
                return -ENOENT;
            }
    
            res = copy_string_internal(buf, dent->path, bufsz, kernel);
            release_dentry(dent);
            release_node(node);

            KDEBUG("%s: buf %s\n", __func__, buf);

            return res;
    }

    // all "non-special" inodes should have dir == 0 && subdir == 0
    // inodes 0 and 1 do not exist
    if(dir || subdir || file < 2)
    {
        return -EINVAL;
    }

    // get inode
    pnode = (file == 2) ? procfs_root : search_tree_for_inode(procfs_root, file);

    if(!pnode || !S_ISLNK(pnode->node.mode) || !pnode->read_file)
    {
        return -EINVAL;
    }

    pnode->read_file(&p, pnode->read_file_arg);
    res = copy_string_internal(buf, p, bufsz, kernel);

    if(p)
    {
        kfree(p);
    }

    return res;
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
    struct procfs_node_t *pnode;
    volatile struct task_t *task;

    switch(dir)
    {
        case DIR_PID:
            if(!(task = get_task_by_index(subdir)))
            {
                break;
            }

            switch(file)
            {
                case PROC_PID_CMDLINE   :   /* /proc/[pid]/cmdline */
                    if(get_task_state(task) == TASK_ZOMBIE)
                    {
                        __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                        break;
                    }

                    buflen = procfs_get_task_args(task, file, &procbuf);
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    break;

                case PROC_PID_ENVIRON   :   /* /proc/[pid]/environ */
                    buflen = procfs_get_task_args(task, file, &procbuf);
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    break;

                case PROC_PID_MEM       :   /* /proc/[pid]/mem */
                    /*
                     * TODO: check this works.
                     */
                    j = read_other_taskmem((struct task_t *)task, *pos,
                                           0, KERNEL_MEM_START,
                                           (char *)buf, count);

                    if(j != 0)
                    {
                        (*pos) += j;
                    }

                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);

                    return j;

                case PROC_PID_EXE       :   /* /proc/[pid]/exe */
                case PROC_PID_COMM      :   /* /proc/[pid]/comm */
                case PROC_PID_IO        :   /* /proc/[pid]/io */
                case PROC_PID_LIMITS    :   /* /proc/[pid]/limits */
                case PROC_PID_MAPS      :   /* /proc/[pid]/maps */
                case PROC_PID_SMAPS     :   /* /proc/[pid]/smaps */
                case PROC_PID_STAT      :   /* /proc/[pid]/stat */
                case PROC_PID_STATM     :   /* /proc/[pid]/statm */
                case PROC_PID_STATUS    :   /* /proc/[pid]/status */
                case PROC_PID_TIMERS    :   /* /proc/[pid]/timers */
                case PROC_PID_CWD       :   /* /proc/[pid]/cwd */
                case PROC_PID_ROOT      :   /* /proc/[pid]/root */
                    buflen = procfs_pid_entries[file].read_file((struct task_t *)task, &procbuf);
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    break;

                default:
                    __sync_and_and_fetch(&task->properties, ~PROPERTY_STRUCT_BUSY);
                    break;
            }
            
            break;

        default:
            // all "non-special" inodes should have dir == 0 && subdir == 0
            // inodes 0 and 1 do not exist
            if(dir || subdir || file < 2)
            {
                break;
            }

            // get inode
            pnode = (file == 2) ? procfs_root : search_tree_for_inode(procfs_root, file);

            if(!pnode || !pnode->read_file)
            {
                break;
            }

            // handle PCI devices first
            if(pnode->flags & PROCFS_NODE_FLAG_IS_PCI)
            {
                buflen = get_pci_device_config_space(pnode->pci, &procbuf);

                /*
                printk("procfs_read_inode: [");
                for(int i = 0; i < 256; i++) printk("%02x", procbuf[i]);
                printk("]\n");
                */

                break;
            }

            // handle all other inodes
            buflen = pnode->read_file(&procbuf, pnode->read_file_arg);
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

