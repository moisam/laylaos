/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: dma.h
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
 *  \file dma.h
 *
 *  Helper functions for performing Direct Memory Access (DMA) operations.
 */

#ifndef __KERNEL_DMA_H__
#define __KERNEL_DMA_H__

#include <mm/mmngr_phys.h>
#include <mm/mmngr_virtual.h>
#include <kernel/ata.h>

/**
 * @brief Initialize DMA disk access.
 *
 * Called during ATA device initialization to initialize the DMA driver
 * for use by the device (if the device supports DMA operations).
 *
 * @param   dev     ATA device
 *
 * @return  zero on success, -(errno) on failure.
 */
int ata_dma_init(struct ata_dev_s *dev);

/**
 * @brief Prepare DMA table.
 *
 * Prepare a DMA PRDT table for reading/writing sectors from an ATA device.
 *
 * @param   dev     ATA device
 * @param   sz      data size
 *
 * @return  zero on success, -(errno) on failure.
 */
int ata_dma_prepare(struct ata_dev_s *dev, size_t sz);

#endif      /* __KERNEL_DMA_H__ */
