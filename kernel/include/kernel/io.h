/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: io.h
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
 *  \file io.h
 *
 *  Helper functions for performing port-based and memory-mapped I/O 
 *  operations.
 */

#ifndef __KERNEL_IO_H__
#define __KERNEL_IO_H__

#include <stdint.h>


/****************************
 * Port-based input/output.
 ****************************/

static inline uint8_t inb(uint16_t port)
{
  uint8_t res;
  __asm__ __volatile__ (
   "in %%dx, %%al"
   :"=a"(res)
   :"d"(port)
   :);
  return (uint8_t)res;
}

static inline void outb(uint16_t port, uint8_t command)
{
  __asm__ __volatile__ (
   "out %%al, %%dx"
   :
   :"d"(port),"a"(command)
   :);
  return;
}

static inline uint16_t inw(uint16_t port)
{
  uint8_t res;
  __asm__ __volatile__ (
   "in %%dx, %%ax"
   :"=a"(res)
   :"d"(port)
   :);
  return (uint16_t)res;
}

static inline void outw(uint16_t port, uint16_t command)
{
  __asm__ __volatile__ (
   "out %%ax, %%dx"
   :
   :"d"(port),"a"(command)
   :);
  return;
}

static inline uint32_t inl(uint16_t port)
{
  uint32_t res;
  __asm__ __volatile__ (
   "in %%dx, %%eax"
   :"=a"(res)
   :"d"(port)
   :);
  return (uint32_t)res;
}

static inline void outl(uint16_t port, uint32_t command)
{
  __asm__ __volatile__ (
   "out %%eax, %%dx"
   :
   :"d"(port),"a"(command)
   :);
  return;
}

static inline void insl(unsigned short port, void *addr, unsigned long count)
{
  __asm__ __volatile__ (
    "rep ; insl"
    : "=D"(addr),"=c"(count) : "d"(port),"0"(addr),"1"(count));
}

static inline void insw(unsigned short port, void *addr, unsigned long count)
{
  __asm__ __volatile__ (
    "rep ; insw"
    : "=D"(addr),"=c"(count) : "d"(port),"0"(addr),"1"(count));
}

static inline void outsw(unsigned short port, void *addr, unsigned long count)
{
  __asm__ __volatile__ (
    "rep ; outsw"
    : "=S"(addr),"=c"(count) : "d"(port),"0"(addr),"1"(count));
}


/************************************
 * Memory-mapped input/output.
 * Functions are defined in mmio.c.
 ************************************/

uint8_t mmio_inb(uintptr_t _addr);
void mmio_outb(uintptr_t _addr, uint8_t command);
uint16_t mmio_inw(uintptr_t _addr);
void mmio_outw(uintptr_t _addr, uint16_t command);
uint32_t mmio_inl(uintptr_t _addr);
void mmio_outl(uintptr_t _addr, uint32_t command);

#endif      /* __KERNEL_IO_H__ */
