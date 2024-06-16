/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: msr.h
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
 *  \file msr.h
 *
 *  Helper functions for reading and writing x86-64 MSRs.
 */

#ifndef x86_MSR_H
#define x86_MSR_H

#ifdef __x86_64__

#define IA32_STAR               0xc0000081
#define IA32_LSTAR              0xc0000082
#define IA32_FMASK              0xc0000084

#define IA32_FS_BASE            0xc0000100
#define IA32_GS_BASE            0xc0000101
#define IA32_KERNGS_BASE        0xc0000102

static inline void wrmsr(uint32_t sel, uint64_t val)
{
	__asm__ __volatile__("wrmsr" : : "c"(sel),
	                                 "d"((uint32_t)(val >> 32)),
	                                 "a"((uint32_t)(val & 0xFFFFFFFF)));
}

static inline uint64_t rdmsr(uint32_t sel)
{
    uint32_t lo, hi;
    
	__asm__ __volatile__("rdmsr" : "=d"(hi), "=a"(lo) : "c"(sel));
	
	return (uint64_t)hi << 32 | lo;
}

#endif      /* __x86_64__ */

#endif      /* x86_MSR_H */
