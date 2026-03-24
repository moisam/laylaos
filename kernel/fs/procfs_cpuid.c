/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024, 2025, 2026 (c)
 * 
 *    file: procfs_cpuid.c
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
 *  \file procfs_cpuid.c
 *
 *  This file implements the functions needed to read cpuid info from the
 *  /proc/cpuid file. Values are populated early on during SMP initialization.
 */

#include <string.h>
#include <kernel/asm.h>
#include <mm/kheap.h>
#include <fs/procfs.h>

#define BUFSZ                   4096

#define COPY_BYTES(b, i, val)       \
    b[i] = val & 0xff;              \
    b[i + 1] = (val >> 8) & 0xff;   \
    b[i + 2] = (val >> 16) & 0xff;  \
    b[i + 3] = (val >> 24) & 0xff;

static char *edx_features[] =
{
    "fpu", "vme", "de", "pse", "tsc", "msr", "pae", "mce", "cx8",
    "apic", NULL, "sep", "mtrr", "pge", "mca", "cmov", "pat", "pse-36",
    "psn", "clfsh", "(nx)", "ds", "acpi", "mmx", "fxsr", "sse", "sse2",
    "ss", "htt", "tm", "ia64", "pbe"
};

static char *ecx_features[] =
{
    "sse3", "pclmulqdq", "dtes64", "monitor", "ds-cpl", "vmx", "smx",
    "est", "tm2", "ssse3", "cnxt-id", "sdbg", "fma", "cx16", "xtpr",
    "pdcm", NULL, "pcid", "dca", "sse4.1", "sse4.2", "x2apic", "movbe",
    "popcnt", "tsc-deadline", "aes-ni", "xsave", "osxsave", "avx", "f16c",
    "rdrnd", "hypervisor"
};

static unsigned long long cpu_speed(void)
{
    unsigned int slow, shigh, elow, ehigh;
    unsigned long long start = 0, end = 0;

    __asm__ __volatile__("rdtsc" : "=a"(slow), "=d"(shigh));
    //block_task2(ecx_features, 20);
    set_task_waking_signal(this_core->cur_task, 0);
    __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_SELECT_EVENT);
    block_task_timeout(this_core->cur_task, 20);
    __asm__ __volatile__("rdtsc" : "=a"(elow), "=d"(ehigh));

    start |= shigh;
    start <<= 32;
    start |= slow;

    end |= ehigh;
    end <<= 32;
    end |= elow;

    return ((end - start) / 1000000) * 5;
}


size_t detect_cpu(char **buf)
{
	unsigned long long cpuspeed;
	int i, j;
	char *p;

	if(!(*buf = kmalloc(BUFSZ)))
	{
	    return 0;
	}

    p = *buf;
    cpuspeed = cpu_speed();

    for(i = 0; i < processor_count; i++)
    {
        ksprintf(p, BUFSZ, "processor     : %d\n", i);
        p += strlen(p);

        ksprintf(p, BUFSZ, "vendor_id     : %s\n", processor_local_data[i].vendorid);
        p += strlen(p);

        ksprintf(p, BUFSZ, "cpu family    : %u\n", processor_local_data[i].family);
        p += strlen(p);

        ksprintf(p, BUFSZ, "model         : %u\n", processor_local_data[i].model);
        p += strlen(p);

        if(processor_local_data[i].modelname[0])
        {
            ksprintf(p, BUFSZ, "model name    : %s\n", processor_local_data[i].modelname);
            p += strlen(p);
        }

        ksprintf(p, BUFSZ, "stepping      : %u\n", processor_local_data[i].stepping);
        p += strlen(p);

        ksprintf(p, BUFSZ, "cpu MHz       : %llu\n", cpuspeed);
        p += strlen(p);

        /*
         * TODO: cache size
         */

        ksprintf(p, BUFSZ, "physical id   : %u\n", processor_local_data[i].cpuid);
        p += strlen(p);

        ksprintf(p, BUFSZ, "core id       : %u\n", processor_local_data[i].cpuid);
        p += strlen(p);

        /*
         * TODO: fill this with proper values
         */
        ksprintf(p, BUFSZ, "cpu cores     : %u\n", 1);
        p += strlen(p);

        ksprintf(p, BUFSZ, "apicid        : %u\n", processor_local_data[i].lapicid);
        p += strlen(p);

        ksprintf(p, BUFSZ, "initial apicid: %u\n", processor_local_data[i].lapicid);
        p += strlen(p);

        ksprintf(p, BUFSZ, "fpu           : %s\n", 
                 (processor_local_data[i].edx_features & 0x01) ? "yes" : "no");
        p += strlen(p);

        // features and flags
        ksprintf(p, BUFSZ, "flags         : ");
        p += strlen(p);

        // check the features returned in edx
        for(j = 0; j < 32; j++)
        {
            if(((processor_local_data[i].edx_features >> j) & 0x01) && edx_features[j])
            {
                ksprintf(p, BUFSZ, "%s ", edx_features[j]);
                p += strlen(p);
            }
        }

        // check the features returned in ecx
        for(j = 0; j < 32; j++)
        {
            if(((processor_local_data[i].ecx_features >> j) & 0x01) && ecx_features[j])
            {
                ksprintf(p, BUFSZ, "%s ", ecx_features[j]);
                p += strlen(p);
            }
        }

        *p++ = '\n';
        *p = '\0';

        if(processor_local_data[i].clflush_size)
        {
            ksprintf(p, BUFSZ, "clflush size  : %u\n", processor_local_data[i].clflush_size);
            p += strlen(p);
        }

        if(processor_local_data[i].bits_phys)
        {
            ksprintf(p, BUFSZ, "address sizes : %u bits physical, %u bits virtual\n",
                        processor_local_data[i].bits_phys,
                        processor_local_data[i].bits_virt);
            p += strlen(p);
        }

        *p++ = '\n';
        *p = '\0';
    }

    return (p - *buf);
}

