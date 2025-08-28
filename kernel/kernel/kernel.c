/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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
#ifdef USE_MULTIBOOT2
#include <kernel/multiboot2.h>
#else
#include <kernel/multiboot.h>
#endif
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
#include <kernel/kparam.h>
#include <kernel/smp.h>
#include <kernel/apic.h>
#include <kernel/ksymtab.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kheap.h>
#include <fs/procfs.h>
#include <kernel/net/protocol.h>
#include <gui/vbe.h>
#include <gui/fb.h>

// kernel size in bytes
size_t kernel_size = 0;

// the physical address of the RSDP table if we have it from the bootloader
uintptr_t rsdp_phys_addr = 0;


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
    // calc kernel size
    physical_addr e = (physical_addr)&kernel_end;
    physical_addr s = (physical_addr)&kernel_start;
    kernel_size = e - s;

    console_init();
    printk("System console initialized..\n");

#ifdef MULTIBOOT2_BOOTLOADER_MAGIC

    struct multiboot_tag *tag;
    unsigned size;

    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    {
        printk("Invalid magic number: 0x%x\n", (unsigned)magic);
        return;
    }

    if(addr & 7)
    {
        printk("Unaligned multiboot2 header: 0x%x\n", addr);
        return;
    }

    // Prefer the ACPI 2.0 RSDP table if present. Otherwise use the ACPI 1.0
    // version if present
    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_ACPI_NEW)))
    {
        struct multiboot_tag_new_acpi *acpitag = 
                            (struct multiboot_tag_new_acpi *)tag;
        rsdp_phys_addr = (uintptr_t)acpitag->rsdp;
    }
    else if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_ACPI_OLD)))
    {
        struct multiboot_tag_old_acpi *acpitag = 
                            (struct multiboot_tag_old_acpi *)tag;
        rsdp_phys_addr = (uintptr_t)acpitag->rsdp;
    }


    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_EFI_MMAP)))
    {
    }

    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_EFI_BS)))
    {
    }

    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_EFI32)))
    {
    }

    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_EFI64)))
    {
        //AcpiOsSetUefiRootPointer
    }


    size = *(unsigned *)addr;
    printk("Announced multboot2 header size: 0x%x\n", size);

    if(find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_VBE) ||
       find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER))
    {
        get_vbe_info(addr);
    }

    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_BOOTDEV)))
    {
        printk("Boot device = 0x%x,%u,%u\n", 
               ((struct multiboot_tag_bootdev *)tag)->biosdev,
               ((struct multiboot_tag_bootdev *)tag)->slice,
               ((struct multiboot_tag_bootdev *)tag)->part);
    }

    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_CMDLINE)))
    {
        struct multiboot_tag_string *str = (struct multiboot_tag_string *)tag;

        printk("Command line = '%s'\n", str->string);
        
        if(strlen(str->string) < 256)
        {
            strcpy(kernel_cmdline, str->string);
        }
        else
        {
            memcpy(kernel_cmdline, str->string, 256 - 4);
            kernel_cmdline[256 - 4] = '.';
            kernel_cmdline[256 - 3] = '.';
            kernel_cmdline[256 - 2] = '.';
            kernel_cmdline[256 - 1] = '\0';
        }
    }

    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_BASIC_MEMINFO)))
    {
        struct multiboot_tag_basic_meminfo *m = 
                                    (struct multiboot_tag_basic_meminfo *)tag;

        // read contiguous memory size
        printk("\nReading memory:\n");
        printk("    Low mem = %luKB\n", m->mem_lower);
        printk("   High mem = %luKB\n\n", m->mem_upper);
    }

#else       /* !MULTIBOOT2_BOOTLOADER_MAGIC */

    multiboot_info_t *mbd = (multiboot_info_t *)addr;

    if(magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        printk("Invalid magic number: 0x%x\n", (unsigned)magic);
        return;
    }

    if(BIT_SET(mbd->flags, 11))
    {
        get_vbe_info(addr);
    }

    if(BIT_SET(mbd->flags, 1))
    {
        printk("Boot device = 0x%x\n", mbd->boot_device);
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

    if(BIT_SET(mbd->flags, 0))
    {
        // read contiguous memory size
        printk("\nReading memory:\n");
        printk("    Low mem = %luKB\n", (unsigned)mbd->mem_lower);
        printk("   High mem = %luKB\n\n", (unsigned)mbd->mem_upper);
    }

#endif      /* MULTIBOOT2_BOOTLOADER_MAGIC */

    printk("Initializing the GDT..\n");
    gdt_init();

    printk("Initializing interrupts..\n");
    idt_init();
    
    printk("Initializing system clock..\n");
    init_clock();
    

#ifdef __x86_64__
    fpu_init();
#else
    sse_init();
#endif

    printk("Initializing physical memory manager..\n");

    pmmngr_init(addr, e);

    printk("\nInitializing virtual memory manager..\n");
    vmmngr_initialize(/* addr */);
    sti();

    // after the call to vmmngr_initialize(), the first 4MB of memory is no
    // longer identity-mapped, so don't try to access things like the
    // multiboot info struct! If we need to use it, we have to copy it
    // somewhere in higher memory before we call vmmngr_initialize().
    // The only exception is the loaded modules, as we marked their memory
    // as 'used' when we called pmmngr_init(). Still, we need to map the
    // modules into higher memory before we can access them.

    printk("kernel_start 0x%lx\n", s);
    printk("kernel_end   0x%lx\n", e);
    printk("kernel_size  0x%lx\n", kernel_size);

    printk("Initializing kernel modules..\n");
    boot_module_init();

    // init APIC and start up other cores if present
    printk("Parsing the MADT table..\n");

    /*
     * Force gcc to ignore the "void * to function pointer cast" warning
     */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

    void (*acpifunc)();

    if((acpifunc = ksym_value("acpi_parse_madt")))
    {
        acpifunc();
    }

#pragma GCC diagnostic pop

    printk("Initializing the scheduler..\n");
    tasking_init();
    
    printk("Initializing the syscall interface..\n");
    syscall_init();

    printk("Initializing APICs..\n");
    apic_init();

    printk("Initializing SMP..\n");
    smp_init();
    
    //sse_init();
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    printk("Kernel cmdline: %s\n", kernel_cmdline);

    printk("Forking init task..\n");

    volatile struct task_t *idle = this_core->cur_task /* get_cur_task() */;
    struct regs r;
    
    fpu_state_save(idle);
    //idle->syscall_regs = &r;

    save_context(idle);
    memcpy(&r, (void *)&idle->saved_context, sizeof(struct regs));

#ifdef __x86_64__
    r.rip = (uintptr_t)do_init;
    r.rflags |= 0x200;
    r.rax = __NR_fork;
#else
    r.eip = (uintptr_t)do_init;
    r.eflags |= 0x200;
    r.eax = __NR_fork;
#endif

    syscall_fork(&r);

    syscall_idle();
}


void do_init(void)
{
    volatile struct task_t *ct = this_core->cur_task /* get_cur_task() */;

    sti();
    
    printk("cpu[%d]: %d\n", this_core->cpuid, scheduler_holding_cpu);
    // init the soft interrupt table before anyone schedules a softint
    //printk("Initializing soft interrupts..\n");
    //softint_init();

    printk("cpu[%d]: Initializing internal queues..\n", this_core->cpuid);
    init_clock_waiters();
    //init_itimers();
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

    printk("cpu[%d]: Initializing the network layer..\n", this_core->cpuid);
    network_init();

    printk("cpu[%d]: Checking PCI buses..\n", this_core->cpuid);
    pci_check_all_buses();
    printk("cpu[%d]: Finished checking PCI buses..\n", this_core->cpuid);
    screen_refresh(NULL);
    
    // fork the disk read/write task AFTER enumerating PCI buses to avoid
    // intervening with IRQs that disks/cdrom devices might need in order
    // to initialize
    (void)start_kernel_task("disk", disk_task_func, NULL,
                            &disk_task, KERNEL_TASK_ELEVATED_PRIORITY);

    printk("cpu[%d]: Initializing filesystems..\n", this_core->cpuid);
    init_fstab();

    printk("cpu[%d]: Initializing dentry cache..\n", this_core->cpuid);
    init_dentries();

    printk("cpu[%d]: Initializing ttys..\n", this_core->cpuid);
    tty_init();

    printk("cpu[%d]: Initializing the random number generator..\n", this_core->cpuid);
    init_genrand(now());
    
    // init groups AFTER init'ing rootfs and making sure we have a working
    // filesystem where we can read /etc/groups
    printk("cpu[%d]: Initializing groups..\n", this_core->cpuid);
    kgroups_init();
    
    printk("cpu[%d]: Initializing SysV IPC queues..\n", this_core->cpuid);
    ipc_init();

    printk("cpu[%d]: Finished boot initialization..\n", this_core->cpuid);

    // finally, execute init
    static char name[] = "/bin/init";
    static char target[16];
    static char *arg[] = { "/bin/init", target, NULL };
    static char *env[] = { "PATH=/bin", /* "LD_DEBUG=1", */ NULL };
    char *path;
    int res = 0;

    ct->user = 1;

    // decide what target to pass to init:
    //   - if a target was passed to us on the boot cmdline, use this
    //   - if we are running from initrd, assume filesystem setup failed for
    //     some reason and boot into "rescue-mode"
    //   - otherwise boot into "single-user" mode
    if(has_cmdline_param("target") && (path = get_cmdline_param_val("target")))
    {
        res = strlen(path);
        memcpy(target, path, res < 16 ? res + 1 : 15);
        target[15] = '\0';
        kfree(path);
    }
    else if(system_root_node->dev == ROOT_DEVID)
    {
        memcpy(target, "rescue-mode", 12);
    }
    else
    {
        memcpy(target, "single-user", 12);
    }

    // set up standard streams
    if((res = syscall_open("/dev/tty1", O_RDONLY /* O_RDWR */, 0)) < 0)
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

    printk("cpu[%d]: Executing init (pid %d)..\n", this_core->cpuid, ct->pid);
    screen_refresh(NULL);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    res = syscall_execve(name, arg, env);

    printk("cpu[%d]: Failed to exec init (%d)\n", this_core->cpuid, res);
    screen_refresh(NULL);
    kpanic("no init!\n");
    empty_loop();
}

