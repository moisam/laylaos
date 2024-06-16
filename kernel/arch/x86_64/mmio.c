/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: mmio.c
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
 *  \file mmio.c
 *
 *  Helper functions for performing memory-mapped I/O operations.
 */

#include <stddef.h>
#include <stdint.h>
#include <kernel/io.h>

uint8_t mmio_inb(uintptr_t _addr)
{
    volatile uint8_t *addr = (uint8_t *)_addr;

    return *addr;
}

void mmio_outb(uintptr_t _addr, uint8_t command)
{
    volatile uint8_t *addr = (uint8_t *)_addr;

    *addr = command;
}

uint16_t mmio_inw(uintptr_t _addr)
{
    volatile uint16_t *addr = (uint16_t *)_addr;

    return *addr;
}

void mmio_outw(uintptr_t _addr, uint16_t command)
{
    volatile uint16_t *addr = (uint16_t *)_addr;
    
    *addr = command;
}

uint32_t mmio_inl(uintptr_t _addr)
{
    volatile uint32_t *addr = (uint32_t *)_addr;
    
    return *addr;
}

void mmio_outl(uintptr_t _addr, uint32_t command)
{
    volatile uint32_t *addr = (uint32_t *)_addr;
    
    *addr = command;
}

