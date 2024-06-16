/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: pci.h
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
 *  \file pci.h
 *
 *  Functions to work with PCI (Peripheral Component Interconnect) devices.
 */

#ifndef __PCI_H__
#define __PCI_H__

#include <stddef.h>
#include <stdint.h>
#include <kernel/irq.h>


/**
 * @struct pci_dev_t
 * @brief The pci_dev_t structure.
 *
 * A structure to represent a PCI device.
 */
struct pci_dev_t
{
    uint8_t base_class;     /**< device base class */
    uint8_t sub_class;      /**< device subclass */
    uint8_t bus;            /**< bus number */
    uint8_t dev;            /**< device number */
    uint8_t function;       /**< function number */
    uint16_t vendor;        /**< device vendor id */
    uint16_t dev_id;        /**< device id */
    uint8_t irq[2];         /**< IRQ numbers */
    uint8_t prog_if;        /**< programming interface */
    uint8_t rev;            /**< revision number */
    unsigned int bar[6];    /**< Base Address Registers (BARs) */
    struct pci_dev_t *next; /**< next PCI device on kernel list */
    
    struct handler_t irq_handler;   /**< IRQ handler */
    int unit;               /**< unit number (used by device drivers to
                                 identify different devices) */

//#define PCIDEV_FLAG_ALIVE           1
    int flags;              /**< flags (currently unused) */
};


/**
 * @struct pci_bus_t
 * @brief The pci_bus_t structure.
 *
 * A structure to represent a PCI device bus.
 */
struct pci_bus_t
{
    uint8_t bus;    /**< bus number */
    int count;      /**< number of devices on the bus */
    struct pci_dev_t *first,    /**< pointer to first device on bus */
                     *last;     /**< pointer to last device on bus */
    struct pci_bus_t *next;     /**< pointer to next bus struct */
};


/**
 * @var first_pci
 * @brief first PCI device.
 *
 * This is a pointer to the first PCI device on the kernel list.
 */
extern struct pci_dev_t *first_pci;

/**
 * @var total_pci_dev
 * @brief PCI device count.
 *
 * This variable contains the count of PCI devices found on the system.
 */
extern int total_pci_dev;


/***********************
 * Function prototypes
 ***********************/

/**
 * @brief Get device count on bus.
 *
 * Return the number of devices found on the given \a bus.
 *
 * @param   bus         PCI bus
 *
 * @return  PCI device count.
 */
int devices_on_bus(struct pci_bus_t *bus);

/**
 * @brief Get bus count.
 *
 * Return the number of PCI device buses.
 *
 * @return  PCI bus count.
 */
int active_pci_bus_count(void);

/**
 * @brief Get bus numbers and count.
 *
 * Return the number of PCI device buses and the bus numbers.
 *
 * @param   buses       an array containing bus numbers is returned here
 * @param   bus_count   bus count is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int active_pci_buses(char **buses, int *bus_count);

/**
 * @brief Enable busmastering.
 *
 * Enable busmastering for the given \a pci device.
 *
 * @param   pci         PCI device
 *
 * @return  nothing.
 */
void pci_enable_busmastering(struct pci_dev_t *pci);

/**
 * @brief Enable interrupts.
 *
 * Enable interrupts for the given \a pci device.
 *
 * @param   pci         PCI device
 *
 * @return  nothing.
 */
void pci_enable_interrupts(struct pci_dev_t *pci);

/**
 * @brief Enable memory space.
 *
 * Enable memory space for the given \a pci device.
 *
 * @param   pci         PCI device
 *
 * @return  nothing.
 */
void pci_enable_memoryspace(struct pci_dev_t *pci);

/**
 * @brief Enable I/O space.
 *
 * Enable I/O space for the given \a pci device.
 *
 * @param   pci         PCI device
 *
 * @return  nothing.
 */
void pci_enable_iospace(struct pci_dev_t *pci);

/**
 * @brief Write long word to PCI I/O space.
 *
 * Write a 32-bit word to the given \a pci device's I/O space at the given
 * \a offset.
 *
 * @param   bus         bus number
 * @param   slot        device number
 * @param   func        function number
 * @param   offset      offet into I/O space
 * @param   val         32-bit value to write
 *
 * @return  nothing.
 */
void pci_config_write_long(uint8_t bus, uint8_t slot, uint8_t func, 
                           uint8_t offset, uint32_t val);

/**
 * @brief Write byte to PCI I/O space.
 *
 * Write an 8-bit value to the given \a pci device's I/O space at the given
 * \a offset.
 *
 * @param   bus         bus number
 * @param   slot        device number
 * @param   func        function number
 * @param   offset      offet into I/O space
 * @param   val         8-bit value to write
 *
 * @return  nothing.
 */
void pci_config_write_byte(uint8_t bus, uint8_t slot, uint8_t func, 
                           uint8_t offset, uint8_t val);

/**
 * @brief Write word to PCI I/O space.
 *
 * Write a 16-bit word to the given \a pci device's I/O space at the given
 * \a offset.
 *
 * @param   bus         bus number
 * @param   slot        device number
 * @param   func        function number
 * @param   offset      offet into I/O space
 * @param   val         16-bit value to write
 *
 * @return  nothing.
 */
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, 
                      uint8_t offset, uint16_t val);

/**
 * @brief Read long word from PCI I/O space.
 *
 * Read a 32-bit word from the given \a pci device's I/O space from the given
 * \a offset.
 *
 * @param   bus         bus number
 * @param   slot        device number
 * @param   func        function number
 * @param   offset      offet into I/O space
 *
 * @return  32-bit value.
 */
uint32_t pci_config_read_long(uint8_t bus, uint8_t slot, uint8_t func, 
                              uint8_t offset);

/**
 * @brief Read byte from PCI I/O space.
 *
 * Read an 8-bit word from the given \a pci device's I/O space from the given
 * \a offset.
 *
 * @param   bus         bus number
 * @param   slot        device number
 * @param   func        function number
 * @param   offset      offet into I/O space
 *
 * @return  8-bit value.
 */
uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, 
                             uint8_t offset);

/**
 * @brief Read word from PCI I/O space.
 *
 * Read a 16-bit word from the given \a pci device's I/O space from the given
 * \a offset.
 *
 * @param   bus         bus number
 * @param   slot        device number
 * @param   func        function number
 * @param   offset      offet into I/O space
 *
 * @return  16-bit value.
 */
uint16_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, 
                         uint8_t offset);

/**
 * @brief Check PCI buses.
 *
 * Check all PCI buses on the system to find attached PCI devices.
 * This function is called during boot to discover PCI devices.
 *
 * @return  nothing.
 */
void pci_check_all_buses(void);

/**
 * @brief Register PCI IRQ handler.
 *
 * Different device drivers call this function to register their IRQ handlers.
 *
 * @param   pci         PCI device
 * @param   handler     the IRQ handler to register
 *
 * @return  nothing.
 */
void pci_register_irq_handler(struct pci_dev_t *pci, 
                              int (*handler)(struct regs *, int),
                              char *name);

#endif      /* __PCI_H__ */
