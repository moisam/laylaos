/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: smp.c
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
 *  \file smp.c
 *
 *  Code to support Symmetric Multiprocessing (SMP) in the kernel.
 */

#include <kernel/laylaos.h>
#include <kernel/ksymtab.h>
#include <kernel/acpi.h>
#include <kernel/pciio.h>
#include <kernel/smp.h>
#include <kernel/kparam.h>
#include <kernel/apic.h>
#include <kernel/pic.h>
#include <kernel/pci.h>
#include <kernel/msr.h>
#include <kernel/tss.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/asm.h>
#include <kernel/fpu.h>
#include <kernel/irq.h>
#include <kernel/ioapic.h>
#include <kernel/syscall.h>
#include <mm/kheap.h>


#define INVLPG_ENTRY_COUNT                  128

struct invlpg_entry_t
{
    volatile virtual_addr addr;
    volatile uint32_t cpus_pending, sending_cpu;
    volatile pid_t sending_task;
};

struct invlpg_entry_t invlpg_entries[INVLPG_ENTRY_COUNT];
struct processor_local_t processor_local_data[MAX_CORES];
int processor_count = 0;
volatile int online_processor_count = 0;
volatile uint32_t online_processor_bitmap = 0;

// defined in ap_bootstrap.S
extern char ap_bootstrap_start[];
extern char ap_bootstrap_end[];
extern char ap_bootstrap_gdtp[];
extern char ap_premain[];

// defined in ioapic.c
extern int ioapic_count;

// id of AP currently undergoing start up
static volatile int ap_current = 0;

// lock to signal that an AP has started up
static volatile int ap_startup_flag = 0;

// stack address for the AP that is being started
uintptr_t ap_stack_base_virt = 0;
uintptr_t ap_stack_base = 0;        // not actually used

volatile int scheduler_holding_cpu = -1;


#define COPY_BYTES(b, i, val)       \
    b[i] = val & 0xff;              \
    b[i + 1] = (val >> 8) & 0xff;   \
    b[i + 2] = (val >> 16) & 0xff;  \
    b[i + 3] = (val >> 24) & 0xff;


static void load_processor_info(void)
{
	unsigned long eax, ebx, ecx, edx;
	unsigned long ebx_save, ecx_save, edx_save, ext_features;
	int i;

    this_core->clflush_size = 0;
    this_core->bits_phys = 0;
    this_core->bits_virt = 0;
	this_core->vendorid[0] = '\0';
    this_core->modelname[0] = '\0';

    if(!has_cpuid())
	{
	    return;
	}

	cpuid(0, eax, ebx, ecx, edx);
	COPY_BYTES(this_core->vendorid, 0, ebx);
	COPY_BYTES(this_core->vendorid, 4, edx);
	COPY_BYTES(this_core->vendorid, 8, ecx);
	this_core->vendorid[12] = '\0';

	cpuid(1, eax, ebx, ecx, edx);
    this_core->family = (eax >> 8) & 0x0f;
    this_core->model = (eax >> 4) & 0x0f;
    this_core->stepping = (eax & 0x0f);

    if(this_core->family == 0x0f)
    {
        this_core->family += ((eax >> 20) & 0xff);
    }

    if(this_core->family == 0x0f || this_core->family == 0x06)
    {
        this_core->model += ((eax >> 16) & 0x0f) << 4;
    }

    // get extended features
    ebx_save = ebx;
    ecx_save = ecx;
    edx_save = edx;
	cpuid(0x80000000, eax, ebx, ecx, edx);
	ext_features = eax;

    if(eax >= 0x80000004)
    {
        for(i = 0; i < 3; i++)
        {
            cpuid(0x80000002 + i, eax, ebx, ecx, edx);
            COPY_BYTES(this_core->modelname, (i * 16) + 0, eax);
            COPY_BYTES(this_core->modelname, (i * 16) + 4, ebx);
            COPY_BYTES(this_core->modelname, (i * 16) + 8, ecx);
            COPY_BYTES(this_core->modelname, (i * 16) + 12, edx);
        }

        this_core->modelname[(i * 16)] = '\0';
    }

    /*
     * TODO: cache size and cpu speed
     */

    this_core->edx_features = edx_save;
    this_core->ecx_features = ecx_save;

    if(edx_save & (1 << 19))
    {
        this_core->clflush_size = (int)(((ebx_save >> 8) & 0xff) * 8);
    }

    if(ext_features >= 0x80000008)
    {
        cpuid(0x80000008, eax, ebx, ecx, edx);
        this_core->bits_phys = (int)(eax & 0xff);
        this_core->bits_virt = (int)((eax >> 8) & 0xff);
    }
}


void ap_main(void)
{
    struct task_t *idle_task;

    // load our processor local data into GS.
    set_gs_base((uintptr_t)&processor_local_data[ap_current]);
    idt_install();

#ifdef __x86_64__
    fpu_init();
#else
    sse_init();
#endif

    printk("smp[%d]: Initializing page directories..\n", ap_current);
    this_core->tss_pointer = &tss_entry[ap_current];
    this_core->_cur_directory_phys = processor_local_data[0]._cur_directory_phys;
    this_core->_cur_directory_virt = processor_local_data[0]._cur_directory_virt;
    this_core->flags |= SMP_FLAG_ONLINE;
    online_processor_count++;
    online_processor_bitmap |= (1 << ap_current);

    load_processor_info();

    printk("smp[%d]: Initializing the scheduler..\n", ap_current);
    //create_idle_task();
    idle_task = get_cpu_idle_task(ap_current);
    idle_task->cpuid = this_core->cpuid;
    this_core->idle_task = idle_task;
    this_core->cur_task = idle_task;
    this_core->prev_ticks = ticks;

    printk("smp[%d]: Initializing the syscall interface..\n", ap_current);
    syscall_init();

    printk("smp[%d]: Initializing local timer..\n", ap_current);
    lapic_timer_init(123);

    //printk("smp[%d]: Enabling interrupts..\n", ap_current);
    //sti();

    printk("smp[%d]: Running idle task..\n", ap_current);

    // let the BSP know it can continue waking up other APs
    ap_startup_flag = 1;

    syscall_idle();
    printk("smp[%d]: We should NOT be here!\n", this_core->cpuid);
    empty_loop();
}


static virtual_addr find_mp_floating_ptr(virtual_addr start, virtual_addr sz)
{
    virtual_addr addr;

    for(addr = start; addr < start + sz; addr += 16)
    {
        if(memcmp((void *)addr, "_MP_", 4) == 0)
        {
            return addr;
        }
    }

    return 0;
}


#define ARRAYCNT(a)             (sizeof(a) / sizeof(a[0]))
#define ISA_TYPE_INDEX          6
#define PCI_TYPE_INDEX          13


void parse_mp_table(void)
{
    static char *enttypes[] = { "Processor                 ", 
                                "Bus                       ", 
                                "I/O APIC                  ", 
                                "I/O Interrupt Assignment  ", 
                                "Local Interrupt Assignment" };
    static char *bustypestr[] = { "UNKWON",
                                  "CBUS  ", "CBUSII", "EISA  ", "FUTURE",
                                  "INTERN", "ISA   ", "MBI   ", "MBII  ",
                                  "MCA   ", "MPI   ", "MPSA  ", "NUBUS ",
                                  "PCI   ", "PCMCIA", "TC    ", "VL    ",
                                  "VME   ", "XPRESS" };
    static char *inttypes[] = { "INT", "NMI", "SMI", "ExtINT" };
    uint8_t bustypes[256] = { 0, };
    uint8_t u8;
    uint16_t u16, i, j, entcnt;
    uint32_t u32;
    virtual_addr mpptr, mptab, ebda, memsz, ent;

    // find the Extended Bios Data Area (EBDA) address
    // See: https://wiki.osdev.org/Memory_Map_(x86)#Extended_BIOS_Data_Area_(EBDA)
    u16 = *(uint16_t *)PHYS_TO_HIMEM(0x40E);
    ebda = PHYS_TO_HIMEM(u16 << 4);

    // search for the MP table
    if(!ebda || !(mpptr = find_mp_floating_ptr(ebda, 0x400)))
    {
        // not found, try the last KB of system base memory
        u16 = *(uint16_t *)PHYS_TO_HIMEM(0x413);
        memsz = PHYS_TO_HIMEM(u16 * 0x400);

        // not found, try the BIOS ROM 0xf0000 - 0xfffff
        if(!memsz || !(mpptr = find_mp_floating_ptr(memsz, 0x400)))
        {
            mpptr = find_mp_floating_ptr(PHYS_TO_HIMEM(0xF0000), 0x10000);
        }
    }

    if(!mpptr)
    {
        printk("smp: could not find MP floating pointer\n");
        return;
    }

    printk("smp: found MP floating pointer at " _XPTR_ "\n", mpptr);
    u32 = *(uint32_t *)(mpptr + 4);

    if(*(uint8_t *)(mpptr + 11) == 0 && u32 != 0)
    {
        mptab = PHYS_TO_HIMEM(u32);
        printk("smp: found MP config table at " _XPTR_ ", size %u\n", 
                mptab, *(uint16_t *)(mptab + 4));
    }
    else
    {
        printk("smp: could not find MP config table\n");
        return;
    }

    if(memcmp((void *)mptab, "PCMP", 4) != 0)
    {
        printk("smp: MP config table has invalid signature\n");
        return;
    }

    // find entries count
    entcnt = *(uint16_t *)(mptab + 34);
    printk("smp: MP config table with %u entries\n", entcnt);

    // loop through the entries
    ent = mptab + 44;

    for(i = 0; i < entcnt; i++)
    {
        u8 = *(uint8_t *)ent;
        printk("[%d] type %d (%s): ", i, u8, enttypes[u8]);

        if(u8 == 1)                      // Bus
        {
            uint8_t *s = (uint8_t *)(ent + 2);      // 6-byte string name
            uint8_t id = *(uint8_t *)(ent + 1);     // bus id

            printk(" %c%c%c%c%c%c (id %d)\n", 
                    s[0], s[1], s[2], s[3], s[4], s[5], id);
            bustypes[id] = 0;

            // store the bus type in an array for use later
            for(j = 0; j < ARRAYCNT(bustypestr); j++)
            {
                if(memcmp(bustypestr[j], s, 6) == 0)
                {
                    bustypes[id] = j;
                    break;
                }
            }
        }
        else if(u8 == 3 ||                  // I/O Interrupt Assignment
                u8 == 4)                    // Local Interrupt Assignment
        {
            printk(" %s (%d)  flag %x  busid %x  busirq %x  apicid %x  apicin %x\n",
                    inttypes[*(uint8_t *)(ent + 1)], *(uint8_t *)(ent + 1),
                    *(uint8_t *)(ent + 2), *(uint8_t *)(ent + 4),
                    *(uint8_t *)(ent + 5), *(uint8_t *)(ent + 6),
                    *(uint8_t *)(ent + 7));

            if(u8 == 3 &&                   // I/O Interrupt Assignment
               *(uint8_t *)(ent + 1) == 0)  // type == INT
            {
                uint8_t busid = *(uint8_t *)(ent + 4);
                uint8_t busirq = *(uint8_t *)(ent + 5);
                uint8_t apicid = *(uint8_t *)(ent + 6);
                uint8_t intin = ioapics[apicid].irq_base + *(uint8_t *)(ent + 7);
                uint8_t flags = 0;

                u8 = *(uint8_t *)(ent + 2);

                flags |= ((u8 & 0x03) == 0x03) ? IOAPIC_ACTIVE_HIGH_LOW : 0;
                flags |= ((u8 & 0x0C) == 0x0C) ? IOAPIC_TRIGGER_EDGE_LOW : 0;

                //printk("(%u, %u).. ", intin, ioapics[apicid].irq_base);

                // for ISA IRQ assignments, only store the assignment if there
                // the IRQ != its corresponding GSI, otherwise we will identity-map
                // the IRQ when we init our I/O APIC
                if(bustypes[busid] == ISA_TYPE_INDEX)
                {
                    if(busirq != intin)
                    {
                        irq_redir[busirq].gsi = intin;
                        irq_redir[busirq].flags = flags;
                    }
                }
                // for PCI IRQ assignments, remember what GSI the device on
                // the given bus is assigned to, so we can use this information
                // later on when we init the PCI device
                else if(bustypes[busid] == PCI_TYPE_INDEX)
                {
                    // the I/O interrupt entry source for PCI devices is 
                    // formatted as follows (Section D.3 of the MP spec):
                    //   1:0          PCI interrupt signal (0 = INTA#, ...)
                    //   6:2          PCI device number
                    //   7            Reserved
                    busirq >>= 2;
                    busirq &= 0x1F;

                    if(intin == 18)
                    {
                    pci_acpi_irq[busid][busirq].gsi = intin;
                    pci_acpi_irq[busid][busirq].flags = flags;
                    }
                }
            }
        }
        else
        {
            printk("\n");
        }

        if(*(uint8_t *)ent == 0)    // processor entry
        {
            ent += 20;
        }
        else                        // other entries
        {
            ent += 8;
        }
    }
}


void smp_init(void)
{
    physical_addr tmp_phys;
    virtual_addr tmp_virt;
    char *page8000;
    int i, j, sz;
    uintptr_t stack_bases[processor_count + 1];

    processor_local_data[0].tss_pointer = &tss_entry[0];
    //processor_local_data[0].printk_buf = global_printk_buf;
    processor_local_data[0].flags |= SMP_FLAG_ONLINE;

    // init the irq map on the BSP
    reserve_irq_range(0, 31);

    for(i = 0; i < ioapic_count; i++)
    {
        reserve_irq_range(ioapics[i].irq_base + 32,
                          ioapics[i].irq_base + ioapics[i].max_redirect + 32);
    }

    online_processor_count++;
    online_processor_bitmap |= (1 << 0);
    load_processor_info();

    // don't both if 'nosmp' was passed in the kernel commandline
    if(has_cmdline_param("nosmp"))
    {
        printk("smp: disabled via the kernel commandline..\n");
        return;
    }

    printk("smp: found %d core(s)\n", processor_count);

    if(processor_count <= 1)
    {
        return;
    }

    // We will load our AP bootstrap code at physical address 0x8000.
    // Allocate a temporary page to copy the contents of this page (in case
    // the bootloader loaded something there), copy the page, then restore
    // it later after we finish initializing APs.
   	if(!(tmp_phys = (physical_addr)pmmngr_alloc_block()))
   	{
        kpanic("smp: could not allocate temporary page\n");
    }

    tmp_virt = PHYS_TO_HIMEM(tmp_phys);

    // Identity map page 0x8000 so when the AP enables paging, it can find
    // the code it needs to run!
    vmmngr_map_page((physical_addr)0x8000, (virtual_addr)0x8000, PTE_FLAGS_PW);

    A_memcpy((void *)tmp_virt, (void *)0x8000, PAGE_SIZE);

    sz = (uintptr_t)&ap_bootstrap_end - (uintptr_t)&ap_bootstrap_start;

    printk("smp: bootstrap code at " _XPTR_ " (sz 0x%x)\n", 
            (uintptr_t)&ap_bootstrap_start, sz);

    // copy the code to page 0x8000
    A_memcpy((void *)0x8000, &ap_bootstrap_start, sz);
    page8000 = (char *)0x8000;

    // fix the bootstrap code XXX
    for(i = 0; i < sz - 4; i++)
    {
        if(page8000[i + 0] == 0x77 && 
           page8000[i + 1] == 0x77 &&
           page8000[i + 2] == 0x77 &&
           page8000[i + 3] == 0x77)
        {
            *(uint32_t *)(&page8000[i]) = 
                (uintptr_t)processor_local_data[0]._cur_directory_phys;
            break;
        }
    }

    for(i = 1; i < processor_count; i++)
    {
       	if(!(ap_stack_base = (physical_addr)pmmngr_alloc_block()))
       	{
            kpanic("smp: could not allocate AP stack page\n");
        }

        stack_bases[i] = PHYS_TO_HIMEM(ap_stack_base) + PAGE_SIZE;
    }

    for(i = 1; i < processor_count; i++)
    {
        ap_startup_flag = 0;
        gdt_copy_to_trampoline(i, (char *)0x8000 + 
                                    ((uintptr_t)&ap_bootstrap_gdtp - 
                                     (uintptr_t)&ap_bootstrap_start));
        tss_install(i, 0x10, (uintptr_t)stack_bases[i]);

        ap_stack_base_virt = stack_bases[i];
        ap_current = i;

        // Send INIT API
        printk("smp: sending INIT IPI to processor %d\n", i);

        // clear APIC errors
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ERR_STATUS)) = 0;

        // select AP
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRH)) = (i << 24);

        // trigger INIT IPI
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) = 
           (*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & 0xfff00000) | 0x00C500;

        // wait for delivery
        do
        {
            __asm__ __volatile__("pause" ::: "memory");
        } while(*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & (1 << 12));

        printk("smp: selecting processor %d\n", i);
        // select AP
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRH)) = (i << 24);

        // deassert
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) = 
           (*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & 0xfff00000) | 0x008500;

        // wait for delivery
        do
        {
            __asm__ __volatile__("pause" ::: "memory");
        } while(*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & (1 << 12));

        printk("smp: delay on processor %d\n", i);
        // wait 10 msec XXX
        tick_delay(2);

        // send STARTUP IPI twice
        printk("smp: sending STARTUP IPI to processor %d\n", i);

        for(j = 0; j < 2; j++)
        {
            // clear APIC errors
            *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ERR_STATUS)) = 0;

            // select AP
            *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRH)) = (i << 24);

            // trigger STARTUP IPI for 0800:0000
            *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) = 
               (*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & 0xfff0f800) | 0x000608;

            // wait 200 usec XXX
            tick_delay(1);

            // wait for delivery
            do
            {
                __asm__ __volatile__("pause" ::: "memory");
            } while(*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & (1 << 12));
        }

        // wait for AP to signal its ready before starting the next AP
        do
        {
            __asm__ __volatile__("pause" ::: "memory");
        } while(!ap_startup_flag);
    }

    // copy page 0x8000 back and free temp memory
    A_memcpy(page8000, (void *)tmp_virt, PAGE_SIZE);
    vmmngr_unmap_page((virtual_addr)0x8000);
    pmmngr_free_block((void *)tmp_phys);

    printk("smp: enabled %d cores\n", processor_count);
}


/*
 * When a task is added to the ready queue, send a soft IPI to all processors
 * (apart from the current one) so they wake up from the idle task and get a
 * chance to run the task. Busy processors ignore this.
 *
 * This function is taken from ToaruOS:
 *   https://github.com/klange/toaruos/blob/a24e4e524a33a2630ceed28d98c3e9e77e8e6abd/kernel/arch/x86_64/smp.c
 */
/*
void wakeup_other_processors(void)
{
    if(lapic_virt == 0 || processor_count <= 1)
    {
        return;
    }

    // clear APIC errors
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ERR_STATUS)) = 0;

    // select AP
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRH)) = 0;

    // trigger IPI
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) = 
           (*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & 0xfff00000) | (3 << 18) | 126;

    // wait for delivery
    do
    {
        __asm__ __volatile__("pause" ::: "memory");
    } while(*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & (1 << 12));
}
*/


#include "task_funcs.c"

static volatile int tlb_holding_cpu = -1;

void handle_tlb_shootdown(void)
{
    volatile struct invlpg_entry_t *ent;
    volatile virtual_addr addr;
    volatile int old_bitmap;
    int bit = (1 << this_core->cpuid);
    unsigned long long oticks = ticks;

    for(ent = invlpg_entries; ent < &invlpg_entries[INVLPG_ENTRY_COUNT]; ent++)
    {
        if(ent->addr == 0)
        {
            continue;
        }

        addr = ent->addr;
        old_bitmap = __sync_fetch_and_and(&ent->cpus_pending, ~bit);

        if(old_bitmap & bit)
        {
            __asm__ __volatile__("invlpg (%0)"::"r"(addr):"memory");
            __asm__ __volatile__("" ::: "memory");
        }
    }

    this_core->irq_count[124]++;
    this_core->irq_ticks[124] += (ticks - oticks);
}


/*
 * Send a TLB shootdown to other processors.
 */
void tlb_shootdown(uintptr_t vaddr)
{
    volatile struct invlpg_entry_t *ent;
    volatile int i;
    volatile int old_flags;
    volatile uint32_t bitmap = online_processor_bitmap & ~(1 << this_core->cpuid);

    old_flags = __set_cpu_flag(SMP_FLAG_SCHEDULER_BUSY);

    if(lapic_virt == 0 || online_processor_count <= 1)
    {
        if(!(old_flags & SMP_FLAG_SCHEDULER_BUSY))
        {
            __clear_cpu_flag(SMP_FLAG_SCHEDULER_BUSY);
        }

        return;
    }

    for(i = 0; i < online_processor_count; i++)
    {
        if(vaddr < KERNEL_MEM_START &&
           processor_local_data[i]._cur_directory_phys != this_core->_cur_directory_phys)
        {
            bitmap &= ~(1 << processor_local_data[i].cpuid);
        }
    }

    if(bitmap == 0)
    {
        if(!(old_flags & SMP_FLAG_SCHEDULER_BUSY))
        {
            __clear_cpu_flag(SMP_FLAG_SCHEDULER_BUSY);
        }

        return;
    }

try:

    for(ent = invlpg_entries; ent < &invlpg_entries[INVLPG_ENTRY_COUNT]; ent++)
    {
        if(__sync_bool_compare_and_swap(&ent->cpus_pending, 0, bitmap))
        {
            __lock_xchg_ptr(&ent->addr, vaddr);
            ent->sending_cpu = this_core->cpuid;
            ent->sending_task = this_core->cur_task ? this_core->cur_task->pid : 0;
            __asm__ __volatile__("":::"memory");
            break;
        }
    }

    if(ent == &invlpg_entries[INVLPG_ENTRY_COUNT])
    {
        __asm__ __volatile__("pause" ::: "memory");
        goto try;
    }

    /*
    // clear APIC errors
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ERR_STATUS)) = 0;

    // select AP
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRH)) = 0;
    */

    // trigger IPI
    *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) = (3 << 18) | 124;

    // wait for delivery
    do
    {
        __asm__ __volatile__("pause" ::: "memory");
    } while(*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & (1 << 12));

    if(!(old_flags & SMP_FLAG_SCHEDULER_BUSY))
    {
        __clear_cpu_flag(SMP_FLAG_SCHEDULER_BUSY);
    }

    //printk("tlb_shootdown[%d]: vaddr 0x%lx - done\n", this_core->cpuid, vaddr);
}


/*
 * Halt all processors (called from kpanic()).
 *
 * This function is taken from ToaruOS:
 *   https://github.com/klange/toaruos/blob/master/kernel/arch/x86_64/user.c#L237
 */
void halt_other_processors(void)
{
    int i;

    if(lapic_virt == 0 || processor_count <= 1)
    {
        return;
    }

    for(i = 0; i < processor_count; i++)
    {
        if(i == this_core->cpuid)
        {
            continue;
        }

        // clear APIC errors
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ERR_STATUS)) = 0;

        // select AP
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRH)) = (i << 24);

        // trigger IPI
        *((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) = 
           (*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & 0xfff00000) | (3 << 18) | 125;

        // wait for delivery
        do
        {
            __asm__ __volatile__("pause" ::: "memory");
        } while(*((volatile uint32_t *)(lapic_virt + LAPIC_REG_ICRL)) & (1 << 12));
    }
}

