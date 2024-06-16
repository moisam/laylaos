/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
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
int syscall_reboot(int cmd)
{
    struct task_t *ct = cur_task;
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
            //printk("System halted.\n");
            break;
            
        case KERNEL_REBOOT_POWEROFF:
            add_task_signal(init, SIGINT, NULL, 1);
            //printk("Power down.\n");
            break;
            
        case KERNEL_REBOOT_RESTART:
            add_task_signal(init, SIGHUP, NULL, 1);
            //printk("Restarting system.\n");
            break;
            
        case KERNEL_REBOOT_SUSPEND:
            printk("System suspend is not yet implemented!\n");
            return -ENOSYS;
    }
    
    return 0;
}


void handle_init_exit(int code)
{
    //printk("handle_init_exit: 0\n");
    //printk("handle_init_exit: code %d (%d, %d)\n", code, WIFSIGNALED(code), WEXITSTATUS(code));

    // ensure we are on the system console (i.e. tty0 == the 1st tty)
    switch_tty(1);

    printk("kernel: init exited with code %d\n", code);
    screen_refresh(NULL);

    // reboot (init was killed, or exited with SIGHUP)
    if(WIFSIGNALED(code) || WEXITSTATUS(code) == 1)
    {
        cli();
        kbd_reset_system();
    }

    __asm__ __volatile__("1: cli\n"
                         "   hlt\n"
                         "   jmp 1b" ::);
    //empty_loop();
}

