/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: vbox.h
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
 *  \file vbox.h
 *
 *  Functions and macros for enabling the kernel to run smoothly under
 *  Oracle VM VirtualBox.
 */

#ifndef VBOX_DRIVER_H
#define VBOX_DRIVER_H

#define VBOX_VENDOR_ID              0x80EE
#define VBOX_DEVICE_ID              0xCAFE

/**
 * @brief Initialise VirtualBox device.
 *
 * Probe the given \a pci device, which should refer to the Oracle VirtualBox
 * controller, to initialize the device for kernel use.
 *
 * @param   pci     PCI structure of the VirtualBox device to initialize
 *
 * @return  nothing.
 */
void vbox_init(struct pci_dev_t *pci);

#endif      /* VBOX_DRIVER_H */
