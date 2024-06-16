/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kernel.c
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
 *  \file kernel.c
 *
 *  Main kernel function.
 */

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/param.h>
#include <kernel/laylaos.h>
#include <kernel/tty.h>
#include <kernel/multiboot.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/irq.h>
#include <kernel/pic.h>
#include <kernel/asm.h>
#include <kernel/clock.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/pci.h>
#include <kernel/modules.h>
#include <kernel/reboot.h>
#include <kernel/ksignal.h>
#include <kernel/dev.h>
#include <kernel/softint.h>
#include <kernel/ata.h>
#include <kernel/kgroups.h>
#include <kernel/ipc.h>
#include <kernel/kbd.h>
#include <kernel/mouse.h>
#include <kernel/acpi.h>
#include <kernel/fpu.h>
#include <kernel/pcache.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kheap.h>
#include <fs/procfs.h>
#include <kernel/net/protocol.h>
#include <gui/vbe.h>
#include <gui/fb.h>

size_t kernel_size = 0;

void do_init(void);

// defined in dev/chr/rand.c
extern void init_genrand(unsigned long s);

// defined in network/network.c
extern void network_init(void);


/*
 * Multiboot info: https://www.gnu.org/software/grub/manual/multiboot/html_node/Boot-information-format.html
 */


void kernel_main(unsigned long magic, unsigned long addr)
{
    multiboot_info_t *mbd = (multiboot_info_t *)addr;

    // calc kernel size
    physical_addr e = (physical_addr)&kernel_end;
    physical_addr s = (physical_addr)&kernel_start;
    kernel_size = e - s;

    console_init();
    printk("System console initialized..\n");

    if(BIT_SET(mbd->flags, 11))
    {
        get_vbe_info(mbd);
    }

    if(magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        printk("Invalid magic number: 0x%x\n", (unsigned)magic);
        return;
    }

    if(BIT_SET(mbd->flags, 1))
    {
        printk("Boot device = %x\n", mbd->boot_device);
    }

    if(BIT_SET(mbd->flags, 2))
    {
        printk("Command line = '%s'\n", (char *)(uintptr_t)mbd->cmdline);
        
        if(strlen((char *)(uintptr_t)mbd->cmdline) < 256)
        {
            strcpy(kernel_cmdline, (char *)(uintptr_t)mbd->cmdline);
        }
        else
        {
            memcpy(kernel_cmdline, (char *)(uintptr_t)mbd->cmdline, 256 - 4);
            kernel_cmdline[256 - 4] = '.';
            kernel_cmdline[256 - 3] = '.';
            kernel_cmdline[256 - 2] = '.';
            kernel_cmdline[256 - 1] = '\0';
        }
    }

    printk("Initializing the GDT..\n");
    gdt_init();

    printk("Initializing interrupts..\n");
    idt_init();
    
    sti();
    printk("Initializing system clock..\n");
    init_clock();
    

#ifdef __x86_64__
    fpu_init();
#else
    sse_init();
#endif

  
    if(BIT_SET(mbd->flags, 0))
    {
        // read contiguous memory size
        printk("\nReading memory:\n");
        printk("    Low mem = %luKB\n", (unsigned)mbd->mem_lower);
        printk("   High mem = %luKB\n\n", (unsigned)mbd->mem_upper);
    }

    printk("Initializing physical memory manager..\n");

    pmmngr_init(mbd, e);

    printk("\nInitializing virtual memory manager..\n");
    vmmngr_initialize(mbd);
    
    // after the call to vmmngr_initialize(), the first 4MB of memory is no
    // longer identity-mapped, so don't try to access things like the
    // multiboot info struct! If we need to use it, we have to copy it
    // somewhere in higher memory before we call vmmngr_initialize().
    // The only exception is the loaded modules, as we marked their memory
    // as 'used' when we called pmmngr_init(). Still, we need to map the
    // modules into higher memory before we can access them.

    printk("Initializing kernel modules..\n");
    boot_module_init();

    printk("Initializing the scheduler..\n");
    tasking_init();
    
    printk("Initializing the syscall interface..\n");
    syscall_init();
    
    //sse_init();
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    printk("Kernel cmdline: %s\n", kernel_cmdline);

    printk("Forking the init task..\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    struct task_t *idle = get_cur_task();
    struct regs r;
    
    fpu_state_save(idle);
    idle->regs = &r;
    
    save_context(idle);
    memcpy(&r, &idle->saved_context, sizeof(struct regs));

#ifdef __x86_64__
    r.rip = (uintptr_t)do_init;
    r.rflags |= 0x200;
    r.rax = __NR_fork;
#else
    r.eip = (uintptr_t)do_init;
    r.eflags |= 0x200;
    r.eax = __NR_fork;
#endif

    syscall_fork();

    syscall_idle();
}


void do_init(void)
{
    register struct task_t *ct = get_cur_task();

    sti();
    
    // init the soft interrupt table before anyone schedules a softint
    //printk("Initializing soft interrupts..\n");
    //softint_init();

    printk("Initializing internal queues..\n");
    init_clock_waiters();
    init_itimers();
    init_seltab();
    init_pcache();
    
    // fork the soft interrupts task
    //(void)start_kernel_task("softint", softint_task_func, NULL,
    //                        &softint_task, 0);

    // fork the keyboard interrupt task
    (void)start_kernel_task("kbd", kbd_task_func, NULL,
                            &kbd_task, KERNEL_TASK_ELEVATED_PRIORITY);

    // fork the mouse interrupt task
    (void)start_kernel_task("mouse", mouse_task_func, NULL,
                            &mouse_task, KERNEL_TASK_ELEVATED_PRIORITY);

    fb_init_screen();
    
    // Init protocols and block reception of incoming packets until
    // everything is ready. We do this before checking PCI devices as some
    // devices (e.g. a NE2K ethernet driver) might need to send a packet,
    // for example a DHCP discover packet.

    printk("Initializing the network layer..\n");
    network_init();

    printk("Checking PCI buses..\n");
    pci_check_all_buses();
    printk("Finished checking PCI buses..\n");
    screen_refresh(NULL);
    
    // fork the disk read/write task AFTER enumerating PCI buses to avoid
    // intervening with IRQs that disks/cdrom devices might need in order
    // to initialize
    (void)start_kernel_task("disk", disk_task_func, NULL,
                            &disk_task, KERNEL_TASK_ELEVATED_PRIORITY);

    printk("Initializing filesystems..\n");
    init_fstab();

    printk("Initializing dentry cache..\n");
    init_dentries();

    printk("Initializing ttys..\n");
    tty_init();

    printk("Initializing the random number generator..\n");
    init_genrand(now());
    
    // init groups AFTER init'ing rootfs and making sure we have a working
    // filesystem where we can read /etc/groups
    printk("Initializing groups..\n");
    kgroups_init();
    
    printk("Initializing SysV IPC queues..\n");
    ipc_init();

    printk("Finished boot initialization..\n");

    // finally, execute init
    static char name[] = "/bin/init";
    static char *arg[] = { "/bin/init", "single-user", NULL };
    static char *env[] = { "PATH=/bin", /* "LD_DEBUG=1", */ NULL };
    int res = 0;

    ct->user = 1;

    if((res = syscall_open("/dev/tty0", O_RDONLY /* O_RDWR */, 0)) < 0)
    {
        printk("  Failed to open terminal for init (errno %d)\n", -res);
    }
    else
    {
        ct->ofiles->ofile[0]->flags = O_RDWR;
    }
    
    if((res = syscall_dup(0)) < 0)
    {
        printk("  Failed to open terminal for init (errno %d)\n", -res);
    }
    
    if((res = syscall_dup(0)) < 0)
    {
        printk("  Failed to open terminal for init (errno %d)\n", -res);
    }

    printk("Executing init (pid %d)..\n", ct->pid);
    screen_refresh(NULL);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    res = syscall_execve(name, arg, env);

    printk("Failed to exec init (%d)\n", res);
    screen_refresh(NULL);
    kpanic("no init!\n");
    empty_loop();
}

