/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: acpi.h
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
 *  \file acpi.h
 *
 *  Macros and function declarations for the kernel ACPI layer.
 */

#ifndef KERNEL_ACPI_H
#define KERNEL_ACPI_H

#include <stdint.h>

struct RSDPDescriptor
{
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} __attribute__ ((packed));


struct RSDPDescriptor20
{
    struct RSDPDescriptor firstPart;

    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} __attribute__ ((packed));


struct ACPISDTHeader
{
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__ ((packed));


struct RSDT
{
    struct ACPISDTHeader h;
    uint32_t PointerToOtherSDT[];
} __attribute__ ((packed));


struct XSDT
{
    struct ACPISDTHeader h;
    uint64_t PointerToOtherSDT[];
} __attribute__ ((packed));


/*
 * EntryType types.
 * See: https://wiki.osdev.org/MADT
 */
#define MADT_ENTRY_PROC_LOCAL_APIC              0
#define MADT_ENTRY_IOAPIC                       1
#define MADT_ENTRY_IOAPIC_INT_SRC_OVERRIDE      2
#define MADT_ENTRY_IOAPIC_NMI_SRC               3
#define MADT_ENTRY_LOCALAPIC_NMI                4
#define MADT_ENTRY_LOCALAPIC_ADDR_OVERRIDE      5
#define MADT_ENTRY_PROC_LOCAL_X2APIC            9

struct MADTEntryHeader
{
    uint8_t EntryType;
    uint8_t RecordLength;
} __attribute__ ((packed));


struct MADT
{
    struct ACPISDTHeader h;
    uint32_t LocalAPICAddress;
    uint32_t Flags;
    struct MADTEntryHeader Entries[];
} __attribute__ ((packed));


struct MADT_LAPIC
{
    struct MADTEntryHeader h;
    uint8_t ProcessorID;
    uint8_t APICID;
    uint32_t Flags;
} __attribute__ ((packed));


struct MADT_IOAPIC
{
    struct MADTEntryHeader h;
    uint8_t IOAPICID;
    uint8_t Reserved;
    uint32_t IOAPICAddress;
    uint32_t GlobalSysIntBase;
} __attribute__ ((packed));


struct MADT_IOPIC_ISO
{
    struct MADTEntryHeader h;
    uint8_t BusSource;
    uint8_t IRQSource;
    uint32_t GlobalSysInt;
    uint16_t Flags;
} __attribute__ ((packed));


/*
 * AddressSpace types.
 * See: https://wiki.osdev.org/FADT
 */
#define ACPI_ADDRESS_SPACE_SYSTEM_MEMORY        0
#define ACPI_ADDRESS_SPACE_SYSTEM_IO            1
#define ACPI_ADDRESS_SPACE_SYSTEM_PCI_CONFIG    2
#define ACPI_ADDRESS_SPACE_SYSTEM_EMBEDDED      3
#define ACPI_ADDRESS_SPACE_SYSTEM_SMBUS         4
#define ACPI_ADDRESS_SPACE_SYSTEM_CMOS          5
#define ACPI_ADDRESS_SPACE_SYSTEM_PCI_BAR       6
#define ACPI_ADDRESS_SPACE_SYSTEM_IPMI          7
#define ACPI_ADDRESS_SPACE_SYSTEM_GPIO          8
#define ACPI_ADDRESS_SPACE_SYSTEM_SERIAL        9

struct GenericAddressStructure
{
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  uint8_t AccessSize;
  uint64_t Address;
} __attribute__ ((packed));


struct FADT
{
    struct   ACPISDTHeader h;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  Reserved;

    uint8_t  PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t  PM1EventLength;
    uint8_t  PM1ControlLength;
    uint8_t  PM2ControlLength;
    uint8_t  PMTimerLength;
    uint8_t  GPE0Length;
    uint8_t  GPE1Length;
    uint8_t  GPE1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t BootArchitectureFlags;

    uint8_t  Reserved2;
    uint32_t Flags;

    // 12 byte structure; see below for details
    struct GenericAddressStructure ResetReg;

    uint8_t  ResetValue;
    uint8_t  Reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                X_FirmwareControl;
    uint64_t                X_Dsdt;

    struct GenericAddressStructure X_PM1aEventBlock;
    struct GenericAddressStructure X_PM1bEventBlock;
    struct GenericAddressStructure X_PM1aControlBlock;
    struct GenericAddressStructure X_PM1bControlBlock;
    struct GenericAddressStructure X_PM2ControlBlock;
    struct GenericAddressStructure X_PMTimerBlock;
    struct GenericAddressStructure X_GPE0Block;
    struct GenericAddressStructure X_GPE1Block;
} __attribute__ ((packed));


/************************************
 * Function declarations
 ************************************/

void acpi_init(void);
void acpi_sleep(int state);
void acpi_reset(void);
void *acpi_get_table(char *signature);
void acpi_parse_madt(void);

#endif      /* KERNEL_ACPI_H */
