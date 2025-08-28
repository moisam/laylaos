/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: reboot.c
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
 *  \file reboot.c
 *
 *  Machine reboot functions.
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/syscall.h>
#include <kernel/reboot.h>
#include <kernel/task.h>
#include <kernel/ksignal.h>
#include <kernel/asm.h>
#include <kernel/kbd.h>
#include <kernel/tty.h>
#include <kernel/ksymtab.h>
#include <gui/vbe.h>


/*
 * To reboot/shutdown the system:
 *    - a task does a syscall_reboot() call
 *    - this sends a signal to the init task: SIGINT (for shutdown) or
 *      SIGHUP (for reboot)
 *    - init brings down the system safely (sync files, kill other tasks, etc.)
 *    - init exits
 *    - init's exit code is passed to handle_init_exit() below, which performs
 *      the actual shutdown/reboot
 *
 * See: https://man7.org/linux/man-pages/man2/reboot.2.html
 */
long syscall_reboot(int cmd)
{
	volatile struct task_t *ct = this_core->cur_task;
    struct task_t *init = get_init_task();
    
    if(!ct || !init)
    {
        return -EINVAL;
    }

    if(!suser(ct))
    {
        return -EPERM;
    }
    
    switch(cmd)
    {
        case KERNEL_REBOOT_HALT:
            add_task_signal(init, SIGINT, NULL, 1);
            break;
            
        case KERNEL_REBOOT_POWEROFF:
            add_task_signal(init, SIGINT, NULL, 1);
            break;
            
        case KERNEL_REBOOT_RESTART:
            add_task_signal(init, SIGHUP, NULL, 1);
            break;
            
        case KERNEL_REBOOT_SUSPEND:
            printk("System suspend is not yet implemented!\n");
            return -ENOSYS;
    }
    
    return 0;
}


static void put_all_supers(void)
{
    struct mount_info_t *d = mounttab;
    struct mount_info_t *ld = &mounttab[NR_SUPER];

    kernel_mutex_lock(&mount_table_mutex);

    for( ; d < ld; d++)
    {
        if(d->dev == 0 || d->fs == NULL)
        {
            continue;
        }

        if(d->fs->ops->put_super)
        {
            d->fs->ops->put_super(d->dev, d->super);
        }

        memset(d, 0, sizeof(struct mount_info_t));
    }

    kernel_mutex_unlock(&mount_table_mutex);
}


void handle_init_exit(int code)
{
    void (*acpifunc)();

    // ensure we are on the system console (i.e. tty0 == the 1st tty)
    switch_tty(1);

    printk("kernel: init exited with code %d\n", code);
    printk("kernel: flushing mounted filesystem superblocks\n");

    /*
     * Release all superblocks. We should technically perform a full unmount
     * operation on each disk, probably in the init task, rather than brute
     * forcing this. Here, we rely on the fact that our init task should
     * have called sync() before exiting.
     */
    put_all_supers();
    printk("kernel: flushing superblocks done\n");
    screen_refresh(NULL);
    __asm__ __volatile__("xchg %%bx, %%bx"::);

    /*
     * Force gcc to ignore the "void * to function pointer cast" warning
     */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

    // reboot (init was killed, or exited with SIGHUP)
    if(WIFSIGNALED(code) || WEXITSTATUS(code) == 1)
    {
        printk("kernel: trying ACPI reset\n");

        if((acpifunc = ksym_value("acpi_reset")))
        {
            printk("kernel: calling ACPI reset function\n");
            screen_refresh(NULL);
            cli();
            acpifunc();
        }
        else
        {
            printk("kernel: failed to find ACPI reset function\n");
        }

        printk("kernel: rebooting via the keyboard driver\n");
        screen_refresh(NULL);
        cli();
        kbd_reset_system();
    }
    // shutdown the system
    // if the ACPI module was loaded, try to use it for shutdown
    else
    {
        printk("kernel: trying ACPI shutdown\n");

        if((acpifunc = ksym_value("acpi_sleep")))
        {
            printk("kernel: calling ACPI sleep function\n");
            screen_refresh(NULL);
            cli();
            acpifunc(5);
        }
        else
        {
            printk("kernel: failed to find ACPI sleep function\n");
        }
    }

#pragma GCC diagnostic pop

    screen_refresh(NULL);
    __asm__ __volatile__("1: cli\n"
                         "   hlt\n"
                         "   jmp 1b" ::);
    //empty_loop();
}

