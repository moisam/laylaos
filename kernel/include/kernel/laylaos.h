/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: laylaos.h
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
 *  \file laylaos.h
 *
 *  General function and macro definitions for kernel use.
 */

#ifndef __LAYLAOS_KERNEL_H__
#define __LAYLAOS_KERNEL_H__

// some helper macros
#define BIT_SET(flags, bit)     ((flags) & (1 << (bit)))
#define UNUSED(x)               (void)(x)

#undef STATIC_INLINE
#define STATIC_INLINE           static inline __attribute__((always_inline))

// define some system-wide upper limits
#define MAX_NR_TASKS            4096
//#define MAX_NR_DISK_BUFFERS     1024

// for debugging
#ifdef __DEBUG
#define KDEBUG(...)             printk(__VA_ARGS__)
#else
#define KDEBUG(...)             if(0) {}
#endif

// pointer format specifier we use with printk()
#ifdef __x86_64__
# define _XPTR_                 "0x%016lx"
#else
# define _XPTR_                 "0x%08x"
#endif


#include <stdarg.h>
#include <stddef.h>                 // size_t

/*
 * Symbols defined in kernel/linker.ld.
 */
extern unsigned int kernel_ro_end;
extern unsigned int kernel_ro_start;
extern unsigned int kernel_end;
extern unsigned int kernel_start;

/*
 * Symbol defined in kernel/kernel.c.
 */
extern size_t kernel_size;

/*
 * Symbols defined in kernel/symbols.c.
 */
extern char machine[], cpu_model[];
extern char ostype[], osrelease[], version[];
extern int osrev;
extern char kernel_cmdline[];
extern unsigned long system_context_switches;
extern unsigned long system_forks;
extern struct utsname myname;

/*
 * Symbol defined in kernel/printk.c.
 */
extern char global_printk_buf[4096];


/***********************
 * Function prototypes
 ***********************/

/**
 * @brief Kernel printf function.
 *
 * This is the kernel equivalent of printf().
 *
 * @param   fmt     format string
 * @param   ...     optional arguments (depends on \a fmt)
 *
 * @return  number of characters printed.
 */
int printk(const char *fmt, ...);

/**
 * @brief Kernel vprintf function.
 *
 * This is the kernel equivalent of vprintf(), which uses a variadic list
 * of arguments.
 *
 * @param   fmt     format string
 * @param   args    variadic list of arguments
 *
 * @return  number of characters printed.
 */
int vprintk(const char *fmt, va_list args);

/**
 * @brief Kernel panic function.
 *
 * Print an error message and halt the machine.
 *
 * @param   s       error message to print
 *
 * @return  never returns.
 */
void kpanic(const char *s);

/**
 * @brief Print kernel stack trace.
 *
 * Print kernel stack trace when a critical error occurs.
 *
 * @return  nothing.
 */
void kernel_stack_trace(void);

/**
 * @brief Kernel abort function.
 *
 * Print an error message and call kpanic() to halt the machine.
 *
 * @param   caller  the caller's name (used in printing the error message)
 *
 * @return  never returns.
 *
 * @see     kpanic()
 */
void kabort(char *caller);

/**
 * @brief Kernel sprintf function.
 *
 * This is the kernel equivalent of sprintf(). Defined in console.c.
 *
 * @param   buf     buffer to print into
 * @param   sz      size of \a buf
 * @param   fmt     format string
 * @param   ...     optional arguments (depends on \a fmt)
 *
 * @return  number of characters printed.
 */
int ksprintf(char *buf, size_t sz, const char *fmt, ...);

/**
 * @brief Empty loop.
 *
 * Run an empty loop. Defined in common.S.
 *
 * @return  never returns.
 */
extern void empty_loop(void);

/*
 * Helper function defined in libk/stdlib/strtoul.c.
 */
extern unsigned long simple_strtoul(const char *__restrict nptr,
                                    char **__restrict endptr, int base);


#ifdef __x86_64__

// memory limits of different kernel structs

// page cache occupies top memory (upto ~122GiB)
#define PCACHE_MEM_START        0xFFFF858000000000  /**< page cache start */
#define PCACHE_MEM_END          0xFFFFFFFFFFFFFFFF  /**< page cache end   */

// 2TiB for tmpfs
#define TMPFS_START             0xFFFF838000000000  /**< tmpfs memory start */
#define TMPFS_END               0xFFFF858000000000  /**< tmpfs memory end   */

// 2TiB for page tables (actually only 8GiB is used here)
#define PAGE_TABLE_START        0xFFFF818000000000  /**< page tables start */
#define PAGE_TABLE_END          0xFFFF818200000000  /**< page tables end   */
//#define PAGE_TABLE_END          0xFFFF838000000000  /**< page tables end   */

// 1TiB for heap
#define KHEAP_START             0xFFFF808000000000  /**< kernel heap start */
#define KHEAP_MAX_ADDR          0xFFFF818000000000  /**< kernel heap end   */

// everything else occupies the lower 256GiB of the upper half
// 1GiB
#define MMIO_START              0xFFFF800340000000  /**< MMIO start */
#define MMIO_END                0xFFFF800380000000  /**< MMIO end   */

// 2GiB
#define INITRD_START            0xFFFF8002C0000000  /**< initrd start */
#define INITRD_END              0xFFFF800340000000  /**< initrd end   */

// 4GiB
#define KMODULE_START           0xFFFF8001C0000000  /**< kernel module start */
#define KMODULE_END             0xFFFF8002C0000000  /**< kernel module end   */

// 1GiB
#define DISK_BUFFER_START       0xFFFF800180000000  /**< disk buffers start */
#define DISK_BUFFER_END         0xFFFF8001C0000000  /**< disk buffers end   */

// 1GiB
#define USER_KSTACK_START       0xFFFF800140000000  /**< kernel stack start */
#define USER_KSTACK_END         0xFFFF800180000000  /**< kernel stack end   */

// 1GiB
#define DMA_BUF_MEM_START       0xFFFF800100000000  /**< DMA memory start */
#define DMA_BUF_MEM_END         0xFFFF800140000000  /**< DMA memory end   */


#define VBE_BACKBUF_START       0xFFFF8000C8000000  /**< VBE backbuf start */
#define VBE_BACKBUF_END         0xFFFF8000D0000000  /**< VBE backbuf end   */

#define VBE_FRONTBUF_START      0xFFFF8000C0000000  /**< VBE frontbuf start */
#define VBE_FRONTBUF_END        0xFFFF8000C8000000  /**< VBE frontbuf end   */

#if 0
#define VBE_BACKBUF_START       0x0000000058000000  /**< VBE backbuf start */
#define VBE_BACKBUF_END         0x000000005D000000  /**< VBE backbuf end   */

// 1GiB
#define VBE_FRONTBUF_START      0xFFFF8000C0000000  /**< VBE frontbuf start */
#define VBE_FRONTBUF_END        0xFFFF800100000000  /**< VBE frontbuf end   */
#endif


// 1GiB
#define PIPE_MEMORY_START       0xFFFF800080000000  /**< pipe memory start */
#define PIPE_MEMORY_END         0xFFFF8000C0000000  /**< pipe memory end   */

// 1GiB
#define ACPI_MEMORY_START       0xFFFF800040000000  /**< ACPI memory start */
#define ACPI_MEMORY_END         0xFFFF800080000000  /**< ACPI memory end   */

// kernel goes in the top half, -128GiB to 0GiB
#define KERNEL_MEM_START        0xFFFF800000000000  /**< kernel memory start */
#define KERNEL_MEM_END          0xFFFFFFFFFFFFFFFF  /**< kernel memory end   */

#define USER_MEM_START          0x0000000000000000  /**< user memory start */
#define USER_MEM_END            0x00007FFFFFFFFFFF  /**< user memory end   */

#define USER_SHM_START          0x00007D8000000000  /**< shared memory start */
#define USER_SHM_END            0x00007E8000000000  /**< shared memory end   */
#define LIB_ADDR_START          0x00007E8000000000  /**< shared libs start */
#define LIB_ADDR_END            0x00007F0000000000  /**< shared libs end   */
#define STACK_START             0x00007F8000000000  /**< user stack start */
//#define USER_ADDR_END           0x00007FFFFFFFFFFF
#define LDSO_MEM_START          0x000000003F000000  /**< ldso start */


#define USERSP(r)               ((r)->userrsp)


#include <stdint.h>


/**
 * @struct regs
 * @brief The 64-bit regs structure.
 *
 * A structure to represent 64-bit CPU registers.
 */
struct regs
{
    uintptr_t r15,      /**< r15 */
              r14,      /**< r14 */
              r13,      /**< r13 */ 
              r12,      /**< r12 */ 
              r11,      /**< r11 */ 
              r10,      /**< r10 */ 
              r9,       /**< r9  */
              r8;       /**< r8  */
    uintptr_t rsp,      /**< rsp */
              rbp,      /**< rbp */
              rdi,      /**< rdi */
              rsi,      /**< rsi */
              rdx,      /**< rdx */
              rcx,      /**< rcx */
              rbx,      /**< rbx */
              rax;      /**< rax */
    uintptr_t int_no,   /**< interrupt number */
              err_code; /**< (optional) error code */
    uintptr_t rip,      /**< rip */
              cs,       /**< cs  */
              rflags,   /**< rflags */
              userrsp,  /**< user rsp */
              ss;       /**< ss */
};


/*
 * Helper function to dump the registers
 */
static inline void dump_regs(struct regs *r)
{
    uint32_t gs1, gs2;
    uint32_t kgs1, kgs2;

    printk("cs 0x%02x\n"
           "rax 0x%016lx    rbx 0x%016lx\n"
           "rcx 0x%016lx    rdx 0x%016lx\n"
           "r8  0x%016lx    r9  0x%016lx\n"
           "r10 0x%016lx    r11 0x%016lx\n"
           "r12 0x%016lx    r13 0x%016lx\n"
           "r14 0x%016lx    r15 0x%016lx\n"
           "rdi 0x%016lx    rsi 0x%016lx\n"
           "rbp 0x%016lx    rsp 0x%016lx\n"
           "userrsp 0x%016lx  ss 0x%02x\n"
           "rip 0x%016lx    rflags 0x%016lx\n"
           "int_no 0x%02x       err_code 0x%02x\n",
           (r->cs & 0xff),
           r->rax, r->rbx, r->rcx, r->rdx,
           r->r8, r->r9, r->r10, r->r11,
           r->r12, r->r13, r->r14, r->r15,
           r->rdi, r->rsi,
           r->rbp, r->rsp,
           r->userrsp, (r->ss & 0xff),
           r->rip, r->rflags,
           r->int_no, r->err_code);

    __asm__ __volatile__("rdmsr" : "=a"(gs1), "=d"(gs2) : "c"(0xc0000101));
    __asm__ __volatile__("rdmsr" : "=a"(kgs1), "=d"(kgs2) : "c"(0xc0000102));

    printk("gs 0x%08x%08x    kerngs 0x%08x%08x\n", gs2, gs1, kgs2, kgs1);
}

#else       /* !__x86_64__ */

// memory limits of different kernel structs

#define PCACHE_MEM_START        0xFF000000      /**< page cache start */
#define PCACHE_MEM_END          0xFFFFFFFF      /**< page cache end   */

#define TMPFS_START             0xFBC00000      /**< tmpfs memory start */
#define TMPFS_END               0xFF000000      /**< tmpfs memory end   */

#define INITRD_START            0xFAC00000      /**< initrd start */
#define INITRD_END              0xFBC00000      /**< initrd end   */

#define KMODULE_START           0xF7C00000      /**< kernel module start */
#define KMODULE_END             0xFAC00000      /**< kernel module end   */

#define DISK_BUFFER_START       0xF7800000      /**< disk buffers start */
#define DISK_BUFFER_END         0xF7C00000      /**< disk buffers end   */

#define USER_KSTACK_START       0xF7400000      /**< kernel stack start */
#define USER_KSTACK_END         0xF7800000      /**< kernel stack end   */

#define DMA_BUF_MEM_START       0xF7000000      /**< DMA memory start */
#define DMA_BUF_MEM_END         0xF7400000      /**< DMA memory end   */

#define PAGE_TABLE_START        0xE7E00000      /**< page tables start */
#define PAGE_TABLE_END          0xF7000000      /**< page tables end   */

/* 26 << 22 */
#define VBE_BACKBUF_START       0x06800000      /**< VBE backbuf start */
#define VBE_BACKBUF_END         0x06D00000      /**< VBE backbuf end   */

#define VBE_FRONTBUF_START      0xE7400000      /**< VBE frontbuf start */
#define VBE_FRONTBUF_END        0xE7900000      /**< VBE frontbuf end   */

#define PIPE_MEMORY_START       0xE7000000      /**< pipe memory start */
#define PIPE_MEMORY_END         0xE7400000      /**< pipe memory end   */

#define ACPI_MEMORY_START       0xE6C00000      /**< ACPI memory start */
#define ACPI_MEMORY_END         0xE7000000      /**< ACPI memory end   */

#define KHEAP_START             0xC0400000      /**< kernel heap start */
#define KHEAP_MAX_ADDR          0xE6BFF000      /**< kernel heap end   */

#define KERNEL_MEM_START        0xC0000000      /**< kernel memory start */
#define KERNEL_MEM_END          0xFFFFFFFF      /**< kernel memory end   */

#define USER_MEM_START          0x00000000      /**< user memory start */
#define USER_MEM_END            KERNEL_MEM_START    /**< user memory end */

#define USER_SHM_START          0x40000000      /**< shared memory start */
#define USER_SHM_END            KERNEL_MEM_START    /**< shared memory end */
#define LIB_ADDR_START          0x60000000      /**< shared libs start */
#define LIB_ADDR_END            0x80000000      /**< shared libs end   */
//#define USER_ADDR_END           KERNEL_MEM_START
#define STACK_START             KERNEL_MEM_START    /**< user stack start */
#define LDSO_MEM_START          0x3F000000      /**< ldso start */


#define USERSP(r)               ((r)->useresp)


/**
 * @struct regs
 * @brief The 32-bit regs structure.
 *
 * A structure to represent 32-bit CPU registers.
 */
struct regs
{
    unsigned int gs,        /**< gs  */
                 fs,        /**< fs  */
                 es,        /**< es  */
                 ds;        /**< ds  */
    unsigned int edi,       /**< edi */
                 esi,       /**< esi */
                 ebp,       /**< ebp */
                 esp,       /**< esp */
                 ebx,       /**< ebx */
                 edx,       /**< edx */
                 ecx,       /**< ecx */
                 eax;       /**< eax */
    unsigned int int_no,    /**< interrupt number */
                 err_code;  /**< (optional) error code */
    unsigned int eip,       /**< eip */
                 cs,        /**< cs  */
                 eflags,    /**< eflags */
                 useresp,   /**< user esp */
                 ss;        /**< ss  */
};


/*
 * Helper function to dump the registers
 */
static inline void dump_regs(struct regs *r)
{
    printk("cs 0x%02x  ds 0x%02x  es 0x%02x  fs 0x%02x  gs 0x%02x\n"
           "eax 0x%08x    ebx 0x%08x\n"
           "ecx 0x%08x    edx 0x%08x\n"
           "edi 0x%08x    esi 0x%08x\n"
           "ebp 0x%08x    esp 0x%08x\n"
           "useresp 0x%08x  ss 0x%2x\n"
           "eip 0x%08x    eflags 0x%08x\n"
           "int_no 0x%02x       err_code 0x%02x\n",
           (r->cs & 0xff), (r->ds & 0xff), (r->es & 0xff), (r->fs & 0xff),
           (r->gs & 0xff),
           r->eax, r->ebx, r->ecx, r->edx,
           r->edi, r->esi,
           r->ebp, r->esp,
           r->useresp, (r->ss & 0xff),
           r->eip, r->eflags,
           r->int_no, r->err_code);
}

#endif      /* !__x86_64__ */


#include <stddef.h>

// prototypes for optimized memory functions from asmlib.a
// Library source can be downloaded from:
//     https://www.agner.org/optimize/#asmlib
void * A_memcpy(void * dest, const void * src, size_t count);
void * A_memset(void * dest, int c, size_t count);


#endif      /* __LAYLAOS_KERNEL_H__ */
