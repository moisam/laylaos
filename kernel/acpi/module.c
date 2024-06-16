/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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
#include "acpi.h"

MODULE_NAME("ACPICA")
MODULE_DESCRIPTION("ACPICA interface module")
MODULE_AUTHOR("Mohammed Isam <mohammed_isam1984@yahoo.com>")

/* definition from acpi/actypes.h */
typedef unsigned int                    UINT32;
typedef UINT32                          ACPI_STATUS;    /* All ACPI Exceptions */

//ACPI_STATUS AcpiOsInitialize(void);
//ACPI_STATUS AcpiOsTerminate(void);


/*
ACPI_STATUS battery_identify(ACPI_HANDLE obj, UINT32 nesting, void *context, void **res);


ACPI_STATUS battery_identify(ACPI_HANDLE obj, UINT32 nesting, void *context, void **res)
{
	printk("ACPI: found a battery device\n");
	printk("      0x%lx, 0x%x, 0x%lx, 0x%lx\n", obj, nesting, context, res);
	return AE_OK;
}
*/


int init_module(void)
{
    printk("Loading ACPICA..\n");
    //AcpiOsInitialize();

	ACPI_STATUS	rv;

    //printk("AcpiOsInitialize: 1\n");
    //__asm__ __volatile("xchg %%bx, %%bx"::);

	rv = AcpiInitializeSubsystem();

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiInitializeSubsystem: error %i\n", rv);
		return rv;
	}

    //printk("AcpiOsInitialize: 2\n");
    //__asm__ __volatile("xchg %%bx, %%bx"::);
	
	rv = AcpiInitializeTables(NULL, 16, FALSE);

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiInitializeTables: error %i\n", rv);
		AcpiTerminate();
		return rv;
	}

    //printk("AcpiOsInitialize: 3\n");
    //__asm__ __volatile("xchg %%bx, %%bx"::);

	rv = AcpiLoadTables();

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiLoadTables: error %i\n", rv);
		AcpiTerminate();
		return rv;
	}

    //printk("AcpiOsInitialize: 4\n");
    //__asm__ __volatile("xchg %%bx, %%bx"::);
	
	rv = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: AcpiEnableSubsystem: error %i\n", rv);
		AcpiTerminate();
		return rv;
	}

    //printk("AcpiOsInitialize: 5\n");
    //__asm__ __volatile("xchg %%bx, %%bx"::);

    /*
    void *res;

	//rv = AcpiGetDevices("PNP0C0A", battery_identify, NULL, &res);
	rv = AcpiGetDevices("ACPI0003", battery_identify, NULL, &res);


	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: failed to get battery info: error %i\n", rv);
	}
	*/
	
	
	/*
	ACPI_TABLE_HEADER *FADT_Header;
	
	rv = AcpiGetTable("FACP", 1, &FADT_Header);

	if(ACPI_FAILURE(rv))
	{
		printk("ACPI: failed to get FADT: error %i\n", rv);
	}
	else
	{
	    struct FADT *FADT_Table = (struct FADT *)FADT_Header;
	    char *sig = FADT_Table->h.Signature;


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


	    
	    //printk("ACPI: FADT signature '%c%c%c%c'\n", 
	    //        sig[0], sig[1], sig[2], sig[3]);

        printk("  Flags %x\n", FADT_Table->Flags);
	    
	    if(sig[0] == 'F' &&
	       sig[1] == 'A' &&
	       sig[2] == 'C' &&
	       sig[3] == 'P')
	    {
	        printk("ACPI: Reset Register:\n");
	        printk("  AddressSpace %x\n", FADT_Table->ResetReg.AddressSpace);
	        printk("  BitWidth %x\n", FADT_Table->ResetReg.BitWidth);
	        printk("  BitOffset %x\n", FADT_Table->ResetReg.BitOffset);
	        printk("  AccessSize %x\n", FADT_Table->ResetReg.AccessSize);
	        printk("  Address %lx\n", FADT_Table->ResetReg.Address);
	        printk("  ResetValue %x\n", FADT_Table->ResetValue);
	    }
	    else
	    {
	        printk("ACPI: skipping FADT with invalid signature\n");
	    }
	}
	*/


    printk("Finished loading ACPICA..\n");
    //__asm__ __volatile("xchg %%bx, %%bx"::);
    //__asm__ __volatile__("cli\nhlt"::);
    //for(;;);

    return 0;
}

void cleanup_module(void)
{
    printk("Unloading ACPICA..\n");
    AcpiOsTerminate();
}

