/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: module.c
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
 *  \file module.c
 *
 * ACPICA module entry and exit points.
 */

#define MODULE
#include <kernel/modules.h>
#include <kernel/acpi.h>
//#include <acpi/actypes.h>
#include <kernel/io.h>
#include <kernel/pci.h>
#include <kernel/smp.h>
#include <kernel/apic.h>
#include <kernel/ioapic.h>
#include "acpi.h"

MODULE_NAME("ACPICA")
MODULE_DESCRIPTION("ACPICA interface module")
MODULE_AUTHOR("Mohammed Isam <mohammed_isam1984@yahoo.com>")

/* definition from include/acpi/actypes.h */
typedef unsigned int                    UINT32;
typedef UINT32                          ACPI_STATUS;    /* All ACPI Exceptions */


/* stuff for resetting the machine */
struct GenericAddressStructure ResetReg;
uint8_t  ResetValue;

/* cached table copies */
void *rsdp_copy = 0;
void *rsdt_copy = 0;
void *xsdt_copy = 0;


/*
ACPI_STATUS battery_identify(ACPI_HANDLE obj, UINT32 nesting, void *context, void **res);


ACPI_STATUS battery_identify(ACPI_HANDLE obj, UINT32 nesting, void *context, void **res)
{
	printk("ACPI: found a battery device\n");
	printk("      0x%lx, 0x%x, 0x%lx, 0x%lx\n", obj, nesting, context, res);
	return AE_OK;
}
*/


static ACPI_STATUS acpi_ex_handler(uint32_t function, 
                                   ACPI_PHYSICAL_ADDRESS address, 
                                   uint32_t bits, uint64_t *value, 
                                   void *handler_context, void *region_context)
{
    (void)function;
    (void)address;
    (void)bits;
    (void)value;
    (void)handler_context;
    (void)region_context;

    return AE_OK;
}


int init_module(void)
{
    ACPI_STATUS	rv;
    ACPI_TABLE_HEADER *FADT_Header;
    physical_addr phys;
    virtual_addr virt;
    size_t sz;

    printk("Loading ACPICA..\n");

	rv = AcpiInitializeSubsystem();

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiInitializeSubsystem: error %i\n", rv);
		return rv;
	}
	
	rv = AcpiInitializeTables(NULL, 16, FALSE);

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiInitializeTables: error %i\n", rv);
		AcpiTerminate();
		return rv;
	}

	rv = AcpiLoadTables();

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiLoadTables: error %i\n", rv);
		AcpiTerminate();
		return rv;
	}

    // This fix for real hardware is taken from the following post:
    //    https://forum.osdev.org/viewtopic.php?t=33640
    rv = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_EC,
                                        &acpi_ex_handler, NULL, NULL);

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiInstallAddressSpaceHandler: error %i\n", rv);
	}

	rv = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiEnableSubsystem: error %i\n", rv);
		AcpiTerminate();
		return rv;
	}

    /*
    void *res;

	//rv = AcpiGetDevices("PNP0C0A", battery_identify, NULL, &res);
	rv = AcpiGetDevices("ACPI0003", battery_identify, NULL, &res);


	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: failed to get battery info: error %i\n", rv);
	}
	*/

	ResetReg.Address = 0;
	rv = AcpiGetTable("FACP", 1, &FADT_Header);

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: failed to get FADT: error %i\n", rv);
	}
	else
	{
	    struct FADT *FADT_Table = (struct FADT *)FADT_Header;
	    char *sig = FADT_Table->h.Signature;

        /*
	    printk("ACPI: FADT signature '%c%c%c%c'\n", 
	            sig[0], sig[1], sig[2], sig[3]);
        printk("Length %x, Revision %x, Checksum %x\n", 
                FADT_Table->h.Length, FADT_Table->h.Revision, FADT_Table->h.Checksum);
        printk("OEMID '%c%c%c%c%c%c'\n",
                FADT_Table->h.OEMID[0], FADT_Table->h.OEMID[1],
                FADT_Table->h.OEMID[2], FADT_Table->h.OEMID[3],
                FADT_Table->h.OEMID[4], FADT_Table->h.OEMID[5]);
        printk("OEMTableID '%c%c%c%c%c%c%c%c'\n",
                FADT_Table->h.OEMTableID[0], FADT_Table->h.OEMTableID[1],
                FADT_Table->h.OEMTableID[2], FADT_Table->h.OEMTableID[3],
                FADT_Table->h.OEMTableID[4], FADT_Table->h.OEMTableID[5],
                FADT_Table->h.OEMTableID[6], FADT_Table->h.OEMTableID[7]);
        printk("OEMRevision %x, CreatorID %x, CreatorRevision %x\n",
                FADT_Table->h.OEMRevision, FADT_Table->h.CreatorID,
                FADT_Table->h.CreatorRevision);

        printk("FirmwareCtrl %x, Dsdt %x, Reserved %x\n",
                FADT_Table->FirmwareCtrl, FADT_Table->Dsdt, FADT_Table->Reserved);
        printk("PreferredPowerManagementProfile %x\n",
                 FADT_Table->PreferredPowerManagementProfile);
        printk("Reserved2 %x\n", FADT_Table->Reserved2);
        printk("  Flags %x\n", FADT_Table->Flags);
        */
	    
	    if(sig[0] == 'F' &&
	       sig[1] == 'A' &&
	       sig[2] == 'C' &&
	       sig[3] == 'P')
	    {
	        printk("ACPI: checking the Reset Register:\n");
	        /*
	        printk("  AddressSpace %x\n", FADT_Table->ResetReg.AddressSpace);
	        printk("  BitWidth %x\n", FADT_Table->ResetReg.BitWidth);
	        printk("  BitOffset %x\n", FADT_Table->ResetReg.BitOffset);
	        printk("  AccessSize %x\n", FADT_Table->ResetReg.AccessSize);
	        printk("  Address %lx\n", FADT_Table->ResetReg.Address);
	        printk("  ResetValue %x\n", FADT_Table->ResetValue);
	        */

            // Validate values according to the ACPI specification:
            //    https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/04_ACPI_Hardware_Specification/ACPI_Hardware_Specification.html#reset-register

#define PRINTERR(str, val)  \
    printk("ACPI: %s (0x%x)\n", str, FADT_Table->ResetReg. val);

            if(FADT_Table->ResetReg.AddressSpace != ACPI_ADDRESS_SPACE_SYSTEM_MEMORY &&
               FADT_Table->ResetReg.AddressSpace != ACPI_ADDRESS_SPACE_SYSTEM_IO &&
               FADT_Table->ResetReg.AddressSpace != ACPI_ADDRESS_SPACE_SYSTEM_PCI_CONFIG)
            {
    	        PRINTERR("invalid address space", AddressSpace);
            }
            else if(FADT_Table->ResetReg.BitWidth != 8)
            {
    	        PRINTERR("invalid bit width", BitWidth);
            }
            else if(FADT_Table->ResetReg.BitOffset != 0)
            {
    	        PRINTERR("invalid bit offset", BitOffset);
            }
            else
            {
                printk("ACPI: address " _XPTR_ ", value 0x%x\n",
                        FADT_Table->ResetReg.Address,
                        FADT_Table->ResetValue);
                ResetReg.AddressSpace = FADT_Table->ResetReg.AddressSpace;
                ResetReg.BitWidth = FADT_Table->ResetReg.BitWidth;
                ResetReg.BitOffset = FADT_Table->ResetReg.BitOffset;
                ResetReg.AccessSize = FADT_Table->ResetReg.AccessSize;
                ResetReg.Address = FADT_Table->ResetReg.Address;
                ResetValue = FADT_Table->ResetValue;

                // If the address is in system memory space, map the physical
                // address to a virtual address for later use
                if(ResetReg.AddressSpace == ACPI_ADDRESS_SPACE_SYSTEM_MEMORY)
                {
                    // This will unfortunately map the whole page
                    ResetReg.Address = mmio_map(ResetReg.Address, ResetReg.Address + 1);
                }
            }

#undef PRINTERR

	    }
	    else
	    {
	        printk("ACPI: skipping FADT with invalid signature\n");
	    }
	}

    // Keep cache copies of some of the tables that might be used later
    if((phys = AcpiOsGetRootPointer()))
    {
        // map RSDP to a virtual address
        if((virt = 
                phys_to_virt_off(phys, phys + PAGE_SIZE,
                                          PTE_FLAGS_PW, REGION_ACPI)))
        {
            sz = (((struct RSDPDescriptor *)virt)->Revision == 2) ?
                    ((struct RSDPDescriptor20 *)virt)->Length :
                    sizeof(struct RSDPDescriptor);

            if((rsdp_copy = kmalloc(sz)))
            {
                A_memcpy(rsdp_copy, (void *)virt, sz);
            }

            // free this as we will not use it anymore
            vmmngr_free_pages(virt, PAGE_SIZE);

            // if this is ACPI 2.0 and we have an XSDT, grab it
            if(((struct RSDPDescriptor20 *)rsdp_copy)->firstPart.Revision == 2 &&
               ((struct RSDPDescriptor20 *)rsdp_copy)->XsdtAddress != 0)
            {
                phys = ((struct RSDPDescriptor20 *)rsdp_copy)->XsdtAddress;

                if((virt = 
                        phys_to_virt_off(phys, phys + PAGE_SIZE,
                                          PTE_FLAGS_PW, REGION_ACPI)))
                {
                    sz = ((struct XSDT *)virt)->h.Length;

                    if((xsdt_copy = kmalloc(sz)))
                    {
                        A_memcpy(xsdt_copy, (void *)virt, sz);
                    }

                    // free this as we will not use it anymore
                    vmmngr_free_pages(virt, PAGE_SIZE);
                }
            }

            // if this is ACPI 2.0 and we don't have an XSDT, or if this
            // is ACPI 1.0, grab the RSDT
            if(((struct RSDPDescriptor *)rsdp_copy)->RsdtAddress != 0)
            {
                phys = ((struct RSDPDescriptor *)rsdp_copy)->RsdtAddress;

                if((virt = 
                        phys_to_virt_off(phys, phys + PAGE_SIZE,
                                          PTE_FLAGS_PW, REGION_ACPI)))
                {
                    sz = ((struct RSDT *)virt)->h.Length;

                    if((rsdt_copy = kmalloc(sz)))
                    {
                        A_memcpy(rsdt_copy, (void *)virt, sz);
                    }

                    // free this as we will not use it anymore
                    vmmngr_free_pages(virt, PAGE_SIZE);
                }
            }
        }
    }

    printk("Finished loading ACPICA..\n");
    //__asm__ __volatile("xchg %%bx, %%bx"::);
    //__asm__ __volatile__("cli\nhlt"::);
    //for(;;);

    return 0;
}


void *acpi_get_table(char *signature)
{
	ACPI_STATUS	rv;
	ACPI_TABLE_HEADER *TableHeader;

    // these two tables are found via pointers in the RSDP, and we should
    // have cached copies of them
    if(memcmp(signature, "XSDT", 4) == 0)
    {
        return xsdt_copy;
    }

    if(memcmp(signature, "RSDT", 4) == 0)
    {
        return rsdt_copy;
    }

    // for other tables, ask ACPICA for a copy
	rv = AcpiGetTable(signature, 1, &TableHeader);

	if(ACPI_FAILURE(rv))
	{
	    printk("ACPI: could not find table '%s'\n", signature);
	    return NULL;
	}

    return TableHeader;
}


void acpi_parse_madt(void)
{
    physical_addr phys;
    virtual_addr virt;
    void *table;
    char *p, *p2;
    int i, entries, is_xsdt;

    if((table = acpi_get_table("XSDT")))
    {
        is_xsdt = 1;
        entries = (((struct XSDT *)table)->h.Length - 
                            sizeof(((struct XSDT *)table)->h)) / 8;
    }
    else if((table = acpi_get_table("RSDT")))
    {
        is_xsdt = 0;
        entries = (((struct RSDT *)table)->h.Length - 
                            sizeof(((struct RSDT *)table)->h)) / 4;
    }
    else
    {
        printk("ACPI: cannot find XSDT or RSDT..\n");
        processor_count = 1;
        return;
    }

    processor_count = 0;

    /*
     * Iterate through table entries to find the MADT table, which has a
     * signature 'APIC'.
     *
     * See: https://wiki.osdev.org/MADT
     */
    for(i = 0; i < entries; i++)
    {
        phys = (physical_addr)(is_xsdt ? 
                            ((struct XSDT *)table)->PointerToOtherSDT[i] :
                            ((struct RSDT *)table)->PointerToOtherSDT[i]);

        if(!(virt = 
                phys_to_virt_off(phys, phys + PAGE_SIZE,
                                          PTE_FLAGS_PW, REGION_ACPI)))
        {
            kpanic("ACPI: failed to map table\n");
        }

        p = (char *)virt;

        if(p[0] == 'A' && p[1] == 'P' && p[2] == 'I' && p[3] == 'C')
        {
            struct MADT *madt = (struct MADT *)virt;

            lapic_phys = madt->LocalAPICAddress;

            for(p2 = (char *)madt->Entries; p2 < p + madt->h.Length; p2 += p2[1])
            {
                switch(p2[0])
                {
                    case MADT_ENTRY_PROC_LOCAL_APIC:
                    {
                        struct MADT_LAPIC *lapic = (struct MADT_LAPIC *)p2;

                        if(lapic->Flags & 0x1)
                        {
                            if(processor_count >= MAX_CORES)
                            {
                                printk("ACPI: too many cores (max %d)\n", MAX_CORES);
                                vmmngr_free_pages(virt, PAGE_SIZE);
                                goto skip;
                            }

                            printk("ACPI: found core #%d (lapic id %d)\n", 
                                    processor_count, p2[3]);
                            processor_local_data[processor_count].cpuid = processor_count;
                            processor_local_data[processor_count].lapicid = lapic->APICID;
                            processor_count++;
                        }
                        break;
                    }

                    case MADT_ENTRY_IOAPIC:
                    {
                        struct MADT_IOAPIC *ioapic = (struct MADT_IOAPIC *)p2;

                        printk("ACPI: found I/O APIC id %d\n", ioapic->IOAPICID);
                        ioapic_add(ioapic->GlobalSysIntBase, ioapic->IOAPICAddress);
                        break;
                    }

                    case MADT_ENTRY_IOAPIC_INT_SRC_OVERRIDE:
                    {
                        struct MADT_IOPIC_ISO *ioapic = (struct MADT_IOPIC_ISO *)p2;

                        printk("ACPI: found Interrupt Source Override\n");
                        printk("ACPI:   bus %d, IRQ %d -> %d, flags 0x%x\n",
                                ioapic->BusSource, ioapic->IRQSource,
                                ioapic->GlobalSysInt, ioapic->Flags);

                        irq_redir[ioapic->IRQSource].gsi = ioapic->GlobalSysInt;
                        irq_redir[ioapic->IRQSource].flags = ioapic->Flags;
                        break;
                    }

                    default:
                        /*
                         * TODO: parse other entry types.
                         */
                        break;
                }
            }
        }

        vmmngr_free_pages(virt, PAGE_SIZE);
    }

skip:

    if(processor_count == 0)
    {
        processor_count = 1;
    }
}


void cleanup_module(void)
{
    printk("Unloading ACPICA..\n");
    AcpiOsTerminate();
}


void acpi_sleep(int state)
{
    if(AcpiEnterSleepStatePrep(state) == AE_OK)
    {
        AcpiEnterSleepState(state);
    }
}


/*
 * This function is not tested except for the system I/O reset code, which 
 * is tested on Oracle VM VirtualBox.
 */
void acpi_reset(void)
{
    if(ResetReg.Address == 0)
    {
        return;
    }

    switch(ResetReg.AddressSpace)
    {
        case ACPI_ADDRESS_SPACE_SYSTEM_MEMORY:
            mmio_outb(ResetReg.Address, ResetValue);
            break;

        case ACPI_ADDRESS_SPACE_SYSTEM_IO:
            outb(ResetReg.Address, ResetValue);
            break;

        case ACPI_ADDRESS_SPACE_SYSTEM_PCI_CONFIG:
            pci_config_write_byte(0, (ResetReg.Address >> 32) & 0xff,
                                     (ResetReg.Address >> 16) & 0xff, 
                                     (ResetReg.Address & 0xff), ResetValue);
            break;

        default:
            break;
    }
}

