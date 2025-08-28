/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: pciio.h
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
 *  \file pciio.h
 *
 *  Functions and macros for I/O on PCI devices.
 */

#ifndef PCIIO_H
#define PCIIO_H

#include <kernel/io.h>

__attribute__((unused)) static uint8_t __inb(uintptr_t port) { return inb(port); }
__attribute__((unused)) static uint16_t __inw(uintptr_t port) { return inw(port); }
__attribute__((unused)) static uint32_t __inl(uintptr_t port) { return inl(port); }
__attribute__((unused)) static void __outb(uintptr_t port, uint8_t command) { outb(port, command); }
__attribute__((unused)) static void __outw(uintptr_t port, uint16_t command) { outw(port, command); }
__attribute__((unused)) static void __outl(uintptr_t port, uint32_t command) { outl(port, command); }

#define pcidev_inb(dev, p)     \
    ((dev->mmio) ? mmio_inb(dev->iobase + p) : __inb(dev->iobase + p))

#define pcidev_inw(dev, p)     \
    ((dev->mmio) ? mmio_inw(dev->iobase + p) : __inw(dev->iobase + p))

#define pcidev_inl(dev, p)     \
    ((dev->mmio) ? mmio_inl(dev->iobase + p) : __inl(dev->iobase + p))

#define pcidev_outb(dev, p, c) \
    ((dev->mmio) ? mmio_outb(dev->iobase + p, c) : __outb(dev->iobase + p, c))

#define pcidev_outw(dev, p, c) \
    ((dev->mmio) ? mmio_outw(dev->iobase + p, c) : __outw(dev->iobase + p, c))

#define pcidev_outl(dev, p, c) \
    ((dev->mmio) ? mmio_outl(dev->iobase + p, c) : __outl(dev->iobase + p, c))

#endif      /* PCIIO_H */
