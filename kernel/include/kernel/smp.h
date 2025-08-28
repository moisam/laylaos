/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: smp.h
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
 *  \file smp.h
 *
 *  Macros and function declarations for the kernel SMP layer.
 */

#ifndef KERNEL_SMP_H
#define KERNEL_SMP_H

/*
 * Maximum supported cores on this kernel.
 */
#define MAX_CORES                       32

struct task_t;

// NOTE: DON'T change the order of the fields in this struct as their offsets
//       are used in assembly code in restore_context() and other asm functions
struct processor_local_t
{
    void *tss_pointer;                  // offset 0
    void *_cur_directory_virt;          // offset 8
    void *_cur_directory_phys;          // offset 16
    volatile struct task_t *cur_task;   // offset 24
    volatile struct task_t *idle_task;  // offset 32
    int32_t cpuid;                      // offset 40
    int32_t lapicid;                    // offset 44
    char *printk_buf;                   // offset 48

#define SMP_FLAG_ONLINE                 0x01
#define SMP_FLAG_SCHEDULER_BUSY         0x02
    volatile int flags;                 // offset 56

    // cpu features obtained from cpuid
    char vendorid[16];
    char modelname[68];
	int family, model, stepping, clflush_size;
	int bits_phys, bits_virt;
	//unsigned long long cpuspeed;
	unsigned long edx_features, ecx_features;
};

extern struct processor_local_t processor_local_data[];
extern int processor_count;

/*
 * Processor local data pointer, relative to %gs.
 */
#ifndef NO_PROCESSOR_LOCAL
static struct processor_local_t __seg_gs *const this_core = 0;
#endif


static inline int __set_cpu_flag(int flag)
{
    int res;
    __asm__ __volatile__("movl %%gs:56, %%ecx\n"
                         "lock orl %%eax, %%gs:56"
                         : "=c"(res) : "a"(flag) : "memory");
    return res;
}


static inline void __clear_cpu_flag(int flag)
{
    __asm__ __volatile__("lock andl %%eax, %%gs:56" : : "a"(~flag) : "memory");
}


/************************************
 * Function declarations
 ************************************/

void smp_init(void);
void wakeup_other_processors(void);
void tlb_shootdown(uintptr_t vaddr);
void halt_other_processors(void);

#endif      /* KERNEL_SMP_H */
