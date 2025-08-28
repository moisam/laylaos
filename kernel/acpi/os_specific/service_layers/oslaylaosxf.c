/******************************************************************************
 *
 * Module Name: oslaylaosxf - LaylaOS OSL interfaces
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2022, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code. No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

/*
 * These interfaces are required in order to compile the ASL compiler and the
 * various ACPICA tools under Linux or other Unix-like system.
 */
#include "acpi.h"
#include "accommon.h"
#include "amlcode.h"
#include "acparser.h"
#include "acdebug.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
//#define __USE_XOPEN2K           // sem_timedwait
//#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/clock.h>
#include <kernel/timer.h>
#include <kernel/irq.h>
#include <kernel/pic.h>
#include <kernel/io.h>
#include <kernel/mutex.h>
#include <mm/mmap.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <mm/kstack.h>

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("osunixxf")

typedef struct
{
    int volatile value;
} acpi_sem_t;

extern int acpi_sem_init(acpi_sem_t *sem, int pshared, unsigned int value);
extern int acpi_sem_destroy(acpi_sem_t *sem);
extern acpi_sem_t *acpi_sem_open(const char *name, int oflag, ...);
extern int acpi_sem_close(acpi_sem_t *sem);
extern int acpi_sem_unlink(const char *name);
extern int acpi_sem_wait(acpi_sem_t *sem);
extern int acpi_sem_timedwait(acpi_sem_t *__restrict sem,
                       const struct timespec *__restrict abstime);
extern int acpi_sem_trywait(acpi_sem_t *sem);
extern int acpi_sem_post(acpi_sem_t *sem);
extern int acpi_sem_getvalue(acpi_sem_t *__restrict sem, int *__restrict sval);

//struct kernel_mutex_t acpi_lock = { 0, };

ACPI_PHYSICAL_ADDRESS UefiRootPointer = 0;

// defined in kernel.c
extern uintptr_t rsdp_phys_addr;


#undef getchar

int getchar(void)
{
    unsigned char buf[4];
    ssize_t copied;
    
    if(syscall_read(0, buf, 1, &copied) == 0)
    //if(syscall_read(0, buf, 1) == 1)
    {
        return (int)buf[0];
    }
    
    return EOF;
}


/* Upcalls to AcpiExec */

void
AeTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable);

typedef void* (*PTHREAD_CALLBACK) (void *);

/* Buffer used by AcpiOsVprintf */

#define ACPI_VPRINTF_BUFFER_SIZE    512
#define _ASCII_NEWLINE              '\n'

/* Terminal support for AcpiExec only */

#ifdef ACPI_EXEC_APP
#include <termios.h>

struct termios              OriginalTermAttributes;
int                         TermAttributesWereSet = 0;

ACPI_STATUS
AcpiUtReadLine (
    char                    *Buffer,
    UINT32                  BufferLength,
    UINT32                  *BytesRead);

static void
OsEnterLineEditMode (
    void);

static void
OsExitLineEditMode (
    void);


/******************************************************************************
 *
 * FUNCTION:    OsEnterLineEditMode, OsExitLineEditMode
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Enter/Exit the raw character input mode for the terminal.
 *
 * Interactive line-editing support for the AML debugger. Used with the
 * common/acgetline module.
 *
 * readline() is not used because of non-portability. It is not available
 * on all systems, and if it is, often the package must be manually installed.
 *
 * Therefore, we use the POSIX tcgetattr/tcsetattr and do the minimal line
 * editing that we need in AcpiOsGetLine.
 *
 * If the POSIX tcgetattr/tcsetattr interfaces are unavailable, these
 * calls will also work:
 *     For OsEnterLineEditMode: system ("stty cbreak -echo")
 *     For OsExitLineEditMode:  system ("stty cooked echo")
 *
 *****************************************************************************/

static void
OsEnterLineEditMode (
    void)
{
    struct termios          LocalTermAttributes;


    TermAttributesWereSet = 0;

    /* STDIN must be a terminal */

    if (!isatty (STDIN_FILENO))
    {
        return;
    }

    /* Get and keep the original attributes */

    if (tcgetattr (STDIN_FILENO, &OriginalTermAttributes))
    {
        fprintf (stderr, "Could not get terminal attributes!\n");
        return;
    }

    /* Set the new attributes to enable raw character input */

    memcpy (&LocalTermAttributes, &OriginalTermAttributes,
        sizeof (struct termios));

    LocalTermAttributes.c_lflag &= ~(ICANON | ECHO);
    LocalTermAttributes.c_cc[VMIN] = 1;
    LocalTermAttributes.c_cc[VTIME] = 0;

    if (tcsetattr (STDIN_FILENO, TCSANOW, &LocalTermAttributes))
    {
        fprintf (stderr, "Could not set terminal attributes!\n");
        return;
    }

    TermAttributesWereSet = 1;
}


static void
OsExitLineEditMode (
    void)
{

    if (!TermAttributesWereSet)
    {
        return;
    }

    /* Set terminal attributes back to the original values */

    if (tcsetattr (STDIN_FILENO, TCSANOW, &OriginalTermAttributes))
    {
        fprintf (stderr, "Could not restore terminal attributes!\n");
    }
}


#else

/* These functions are not needed for other ACPICA utilities */

#define OsEnterLineEditMode()
#define OsExitLineEditMode()
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInitialize, AcpiOsTerminate
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize and terminate this module.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsInitialize (
    void)
{
    //ACPI_STATUS            Status;


    /*
    AcpiGbl_OutputFile = stdout;

    OsEnterLineEditMode ();

    Status = AcpiOsCreateLock (&AcpiGbl_PrintLock);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    */

    return (AE_OK);
}

ACPI_STATUS
AcpiOsTerminate (
    void)
{

    //OsExitLineEditMode ();
    return (AE_OK);
}


#ifndef ACPI_USE_NATIVE_RSDP_POINTER
/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetRootPointer
 *
 * PARAMETERS:  None
 *
 * RETURN:      RSDP physical address
 *
 * DESCRIPTION: Gets the ACPI root pointer (RSDP)
 *
 *****************************************************************************/

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer (
    void)
{
	ACPI_PHYSICAL_ADDRESS Ret = 0;

    /*
     * Check if GRUB Multiboot2 passed an RSDP table pointer.
     */
    if(UefiRootPointer != 0)
    {
        return UefiRootPointer;
    }

	if(rsdp_phys_addr != 0)
    {
        UefiRootPointer = rsdp_phys_addr;
        return UefiRootPointer;
    }

	AcpiFindRootPointer(&Ret);
	return Ret;
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsPredefinedOverride
 *
 * PARAMETERS:  InitVal             - Initial value of the predefined object
 *              NewVal              - The new value for the object
 *
 * RETURN:      Status, pointer to value. Null pointer returned if not
 *              overriding.
 *
 * DESCRIPTION: Allow the OS to override predefined names
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsPredefinedOverride (
    const ACPI_PREDEFINED_NAMES *InitVal,
    ACPI_STRING                 *NewVal)
{

    if (!InitVal || !NewVal)
    {
        return (AE_BAD_PARAMETER);
    }

    *NewVal = NULL;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsTableOverride
 *
 * PARAMETERS:  ExistingTable       - Header of current table (probably
 *                                    firmware)
 *              NewTable            - Where an entire new table is returned.
 *
 * RETURN:      Status, pointer to new table. Null pointer returned if no
 *              table is available to override
 *
 * DESCRIPTION: Return a different version of a table if one is available
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable)
{

    if (!ExistingTable || !NewTable)
    {
        return (AE_BAD_PARAMETER);
    }

    *NewTable = NULL;

#ifdef ACPI_EXEC_APP

    AeTableOverride (ExistingTable, NewTable);
    return (AE_OK);
#else

    return (AE_NO_ACPI_TABLES);
#endif
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsPhysicalTableOverride
 *
 * PARAMETERS:  ExistingTable       - Header of current table (probably firmware)
 *              NewAddress          - Where new table address is returned
 *                                    (Physical address)
 *              NewTableLength      - Where new table length is returned
 *
 * RETURN:      Status, address/length of new table. Null pointer returned
 *              if no table is available to override.
 *
 * DESCRIPTION: Returns AE_SUPPORT, function not used in user space.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsPhysicalTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_PHYSICAL_ADDRESS   *NewAddress,
    UINT32                  *NewTableLength)
{
    UNUSED(ExistingTable);
    UNUSED(NewAddress);
    UNUSED(NewTableLength);
    
    return (AE_SUPPORT);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsEnterSleep
 *
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *              RegaValue           - Register A value
 *              RegbValue           - Register B value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: A hook before writing sleep registers to enter the sleep
 *              state. Return AE_CTRL_TERMINATE to skip further sleep register
 *              writes.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsEnterSleep (
    UINT8                   SleepState,
    UINT32                  RegaValue,
    UINT32                  RegbValue)
{
    UNUSED(SleepState);
    UNUSED(RegaValue);
    UNUSED(RegbValue);

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsRedirectOutput
 *
 * PARAMETERS:  Destination         - An open file handle/pointer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Causes redirect of AcpiOsPrintf and AcpiOsVprintf
 *
 *****************************************************************************/

void
AcpiOsRedirectOutput (
    void                    *Destination)
{
    UNUSED(Destination);

    //AcpiGbl_OutputFile = Destination;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsPrintf
 *
 * PARAMETERS:  fmt, ...            - Standard printf format
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output. Note: very similar to AcpiOsVprintf
 *              (performance), changes should be tracked in both functions.
 *
 *****************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf (
    const char              *Fmt,
    ...)
{
    va_list                 Args;
    UINT8                   Flags;


    Flags = AcpiGbl_DbOutputFlags;

#if 0

    if (Flags & ACPI_DB_REDIRECTABLE_OUTPUT)
    {
        /* Output is directable to either a file (if open) or the console */

        if (AcpiGbl_DebugFile)
        {
            /* Output file is open, send the output there */

            va_start (Args, Fmt);
            //printk (Fmt, Args);
            vfprintf (AcpiGbl_DebugFile, Fmt, Args);
            va_end (Args);
        }
        else
        {
            /* No redirection, send output to console (once only!) */

            Flags |= ACPI_DB_CONSOLE_OUTPUT;
        }
    }

#endif

    if (Flags & ACPI_DB_CONSOLE_OUTPUT)
    {
        va_start (Args, Fmt);
        vprintk (Fmt, Args);
        //vfprintf (AcpiGbl_OutputFile, Fmt, Args);
        va_end (Args);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsVprintf
 *
 * PARAMETERS:  fmt                 - Standard printf format
 *              args                - Argument list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output with argument list pointer. Note: very
 *              similar to AcpiOsPrintf, changes should be tracked in both
 *              functions.
 *
 *****************************************************************************/

void
AcpiOsVprintf (
    const char              *Fmt,
    va_list                 Args)
{
    UINT8                   Flags;
    //char                    Buffer[ACPI_VPRINTF_BUFFER_SIZE];


    /*
     * We build the output string in a local buffer because we may be
     * outputting the buffer twice. Using vfprintf is problematic because
     * some implementations modify the args pointer/structure during
     * execution. Thus, we use the local buffer for portability.
     *
     * Note: Since this module is intended for use by the various ACPICA
     * utilities/applications, we can safely declare the buffer on the stack.
     * Also, This function is used for relatively small error messages only.
     */
    //vsnprintf (Buffer, ACPI_VPRINTF_BUFFER_SIZE, Fmt, Args);

    Flags = AcpiGbl_DbOutputFlags;

#if 0

    if (Flags & ACPI_DB_REDIRECTABLE_OUTPUT)
    {
        /* Output is directable to either a file (if open) or the console */

        if (AcpiGbl_DebugFile)
        {
            /* Output file is open, send the output there */

            fputs (Buffer, AcpiGbl_DebugFile);
            //twritestr(Buffer);
        }
        else
        {
            /* No redirection, send output to console (once only!) */

            Flags |= ACPI_DB_CONSOLE_OUTPUT;
        }
    }

#endif

    if (Flags & ACPI_DB_CONSOLE_OUTPUT)
    {
        ////fputs (Buffer, AcpiGbl_OutputFile);
        //printk("%s", Buffer);
        vprintk (Fmt, Args);
    }
}


#ifndef ACPI_EXEC_APP
/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetLine
 *
 * PARAMETERS:  Buffer              - Where to return the command line
 *              BufferLength        - Maximum length of Buffer
 *              BytesRead           - Where the actual byte count is returned
 *
 * RETURN:      Status and actual bytes read
 *
 * DESCRIPTION: Get the next input line from the terminal. NOTE: For the
 *              AcpiExec utility, we use the acgetline module instead to
 *              provide line-editing and history support.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetLine (
    char                    *Buffer,
    UINT32                  BufferLength,
    UINT32                  *BytesRead)
{
    int                     InputChar;
    UINT32                  EndOfLine;


    /* Standard AcpiOsGetLine for all utilities except AcpiExec */

    for (EndOfLine = 0; ; EndOfLine++)
    {
        if (EndOfLine >= BufferLength)
        {
            return (AE_BUFFER_OVERFLOW);
        }

        if ((InputChar = getchar ()) == EOF)
        {
            return (AE_ERROR);
        }

        if (!InputChar || InputChar == _ASCII_NEWLINE)
        {
            break;
        }

        Buffer[EndOfLine] = (char) InputChar;
    }

    /* Null terminate the buffer */

    Buffer[EndOfLine] = 0;

    /* Return the number of bytes in the string */

    if (BytesRead)
    {
        *BytesRead = EndOfLine;
    }

    return (AE_OK);
}
#endif


#ifndef ACPI_USE_NATIVE_MEMORY_MAPPING
/******************************************************************************
 *
 * FUNCTION:    AcpiOsMapMemory
 *
 * PARAMETERS:  where               - Physical address of memory to be mapped
 *              length              - How much memory to map
 *
 * RETURN:      Pointer to mapped memory. Null on error.
 *
 * DESCRIPTION: Map physical memory into caller's address space
 *
 *****************************************************************************/

void *
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   where,
    ACPI_SIZE               length)
{
    physical_addr pstart = align_down(where);
    physical_addr pend = align_up(where + length);

    size_t sz = pend - pstart;
    size_t i, j, pages = sz / PAGE_SIZE;
    virtual_addr v, addr = 0;
    
    // NOTE: we don't need this as sz is page-aligned

    /*
    if(sz % PAGE_SIZE)
    {
        pages++;
    }
    */

    // try and get consecutive virtual address pages
    for(i = ACPI_MEMORY_START, j = 0; i < ACPI_MEMORY_END; i += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *) i);

        // we've got an unused address
        if(PTE_FRAME(*pt) == 0)
        //if(pt_entry_get_frame(pt) == 0)
        {
            // we've got our pages
            if(++j == pages)
            {
                addr = i - ((pages - 1) * PAGE_SIZE);
                break;
            }
        }
        else
        {
            // reset our counter
            j = 0;
        }
    }

    if(j != pages)
    {
        kpanic("Insufficient memory in AcpiOsMapMemory()");
        return 0;
    }

    /*
     * TODO: We should check if the requested physical memory is actually
     *       mapped by another process. If so, return error. Otherwise,
     *       call the physical mem manager to mark the physical frames as used.
     */
    pmmngr_deinit_region(pstart, sz);

    for(i = 0, v = addr;
        i < pages;
        i++, pstart += PAGE_SIZE, v += PAGE_SIZE)
    {
        vmmngr_map_page((void *)pstart, (void *)v, PTE_FLAGS_PW);
        vmmngr_flush_tlb_entry(v);
        
        /*
         * If the requested memory page is in the lower 1MiB, increase the
         * frame shares so that subsequent calls to AcpiOsUnmapMemory() do not
         * free the frame and make it available for use.
         */
        if(pstart < 0x100000)
        {
            inc_frame_shares(pstart);
        }
    }
    
    return (ACPI_TO_POINTER ((ACPI_SIZE) (addr + (where - align_down(where)))));
    //return (ACPI_TO_POINTER ((ACPI_SIZE) where));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsUnmapMemory
 *
 * PARAMETERS:  where               - Logical address of memory to be unmapped
 *              length              - How much memory to unmap
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete a previously created mapping. Where and Length must
 *              correspond to a previous mapping exactly.
 *
 *****************************************************************************/

void
AcpiOsUnmapMemory (
    void                    *where,
    ACPI_SIZE               length)
{
    virtual_addr vstart = align_down((virtual_addr)where);
    virtual_addr vend = align_up((virtual_addr)where + length);
    //virtual_addr i = vstart;
    
    if(vstart < ACPI_MEMORY_START || vend > ACPI_MEMORY_END)
    {
        kpanic("Invalid memory address in AcpiOsUnmapMemory()");
        return;
    }
    
    /*
     * TODO: we should call vmmngr_free_pages() to actually release the
     *       physical memory frames, as well as unmap memory.
     */
    vmmngr_free_pages(vstart, vend - vstart);

    /*
    while (i < vend)
    {
        vmmngr_free_page(get_page_entry((void *) i));
        vmmngr_flush_tlb_entry(i);
        i += PAGE_SIZE;
    }
    */
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsAllocate
 *
 * PARAMETERS:  Size                - Amount to allocate, in bytes
 *
 * RETURN:      Pointer to the new allocation. Null on error.
 *
 * DESCRIPTION: Allocate memory. Algorithm is dependent on the OS.
 *
 *****************************************************************************/

void *
AcpiOsAllocate (
    ACPI_SIZE               size)
{
    void                    *Mem;

     //printk("AcpiOsAllocate: sz %u\n", size);
    Mem = (void *) kmalloc ((size_t) size);
     //printk("AcpiOsAllocate: 0x%x\n", Mem);
    return (Mem);
}


#ifdef USE_NATIVE_ALLOCATE_ZEROED
/******************************************************************************
 *
 * FUNCTION:    AcpiOsAllocateZeroed
 *
 * PARAMETERS:  Size                - Amount to allocate, in bytes
 *
 * RETURN:      Pointer to the new allocation. Null on error.
 *
 * DESCRIPTION: Allocate and zero memory. Algorithm is dependent on the OS.
 *
 *****************************************************************************/

void *
AcpiOsAllocateZeroed (
    ACPI_SIZE               size)
{
    void                    *Mem;

     //printk("AcpiOsAllocateZeroed: sz %u\n", size);
    Mem = (void *) kcalloc (1, (size_t) size);
     //printk("AcpiOsAllocateZeroed: 0x%x\n", Mem);
    
    /*
    Mem = (void *) kmalloc ((size_t) size);
    
    if(Mem)
    {
        memset(Mem, 0, (size_t) size);
    }
    */
    
    return (Mem);
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsFree
 *
 * PARAMETERS:  mem                 - Pointer to previously allocated memory
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Free memory allocated via AcpiOsAllocate
 *
 *****************************************************************************/

void
AcpiOsFree (
    void                    *mem)
{
     //printk("AcpiOsFree: 0x%x\n", mem);
    if(mem)
    {
        kfree (mem);
    }
}


#ifdef ACPI_SINGLE_THREADED
/******************************************************************************
 *
 * FUNCTION:    Semaphore stub functions
 *
 * DESCRIPTION: Stub functions used for single-thread applications that do
 *              not require semaphore synchronization. Full implementations
 *              of these functions appear after the stubs.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateSemaphore (
    UINT32              MaxUnits,
    UINT32              InitialUnits,
    ACPI_HANDLE         *OutHandle)
{
    *OutHandle = (ACPI_HANDLE) 1;
    return (AE_OK);
}

ACPI_STATUS
AcpiOsDeleteSemaphore (
    ACPI_HANDLE         Handle)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiOsWaitSemaphore (
    ACPI_HANDLE         Handle,
    UINT32              Units,
    UINT16              Timeout)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiOsSignalSemaphore (
    ACPI_HANDLE         Handle,
    UINT32              Units)
{
    return (AE_OK);
}

#else
/******************************************************************************
 *
 * FUNCTION:    AcpiOsCreateSemaphore
 *
 * PARAMETERS:  InitialUnits        - Units to be assigned to the new semaphore
 *              OutHandle           - Where a handle will be returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create an OS semaphore
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateSemaphore (
    UINT32              MaxUnits,
    UINT32              InitialUnits,
    ACPI_HANDLE         *OutHandle)
{
    UNUSED(MaxUnits);
    acpi_sem_t               *Sem;


    if (!OutHandle)
    {
        return (AE_BAD_PARAMETER);
    }

    Sem = AcpiOsAllocate (sizeof (acpi_sem_t));
    if (!Sem)
    {
        return (AE_NO_MEMORY);
    }

    if (acpi_sem_init (Sem, 0, InitialUnits) < 0)
    //if (sem_init (Sem, 0, InitialUnits) == -1)
    {
        AcpiOsFree (Sem);
        return (AE_BAD_PARAMETER);
    }

    *OutHandle = (ACPI_HANDLE) Sem;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsDeleteSemaphore
 *
 * PARAMETERS:  Handle              - Handle returned by AcpiOsCreateSemaphore
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete an OS semaphore
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsDeleteSemaphore (
    ACPI_HANDLE         Handle)
{
    acpi_sem_t               *Sem = (acpi_sem_t *) Handle;


    if (!Sem)
    {
        return (AE_BAD_PARAMETER);
    }

    if (acpi_sem_destroy (Sem) < 0)
    //if (sem_destroy (Sem) == -1)
    {
        return (AE_BAD_PARAMETER);
    }

    // our sem_destroy() is a no-op, so release the memory here
    AcpiOsFree (Sem);

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitSemaphore
 *
 * PARAMETERS:  Handle              - Handle returned by AcpiOsCreateSemaphore
 *              Units               - How many units to wait for
 *              MsecTimeout         - How long to wait (milliseconds)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Wait for units
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWaitSemaphore (
    ACPI_HANDLE         Handle,
    UINT32              Units,
    UINT16              MsecTimeout)
{
    UNUSED(Units);

    ACPI_STATUS         Status = AE_OK;
    acpi_sem_t               *Sem = (acpi_sem_t *) Handle;
    int                 RetVal;
#ifndef ACPI_USE_ALTERNATE_TIMEOUT
    struct timespec     Time;
#endif


    if (!Sem)
    {
        return (AE_BAD_PARAMETER);
    }

    switch (MsecTimeout)
    {
    /*
     * No Wait:
     * --------
     * A zero timeout value indicates that we shouldn't wait - just
     * acquire the semaphore if available otherwise return AE_TIME
     * (a.k.a. 'would block').
     */
    case 0:

        if (acpi_sem_trywait(Sem) < 0)
        //if (sem_trywait(Sem) == -1)
        {
            Status = (AE_TIME);
        }
        break;

    /* Wait Indefinitely */

    case ACPI_WAIT_FOREVER:

        while (((RetVal = acpi_sem_wait (Sem)) == -EINTR))
        //while (((RetVal = sem_wait (Sem)) == -1) && (errno == EINTR))
        {
            continue;   /* Restart if interrupted */
        }
        if (RetVal != 0)
        {
            Status = (AE_TIME);
        }
        break;


    /* Wait with MsecTimeout */

    default:

#ifdef ACPI_USE_ALTERNATE_TIMEOUT
        /*
         * Alternate timeout mechanism for environments where
         * sem_timedwait is not available or does not work properly.
         */
        while (MsecTimeout)
        {
            if (acpi_sem_trywait (Sem) == 0)
            {
                /* Got the semaphore */
                return (AE_OK);
            }

            if (MsecTimeout >= 10)
            {
                MsecTimeout -= 10;
                usleep (10 * ACPI_USEC_PER_MSEC); /* ten milliseconds */
            }
            else
            {
                MsecTimeout--;
                usleep (ACPI_USEC_PER_MSEC); /* one millisecond */
            }
        }
        Status = (AE_TIME);
#else
        /*
         * The interface to sem_timedwait is an absolute time, so we need to
         * get the current time, then add in the millisecond Timeout value.
         */

#include <kernel/clock.h>
        Time.tv_sec  = monotonic_time.tv_sec + startup_time;
        Time.tv_nsec = monotonic_time.tv_nsec;

        /*
        if (syscall_clock_gettime (CLOCK_REALTIME, &Time) == -1)
        {
            printk ("clock_gettime: %s\n", strerror(-RetVal));
            //perror ("clock_gettime");
            return (AE_TIME);
        }
        */

        Time.tv_sec += (MsecTimeout / ACPI_MSEC_PER_SEC);
        Time.tv_nsec += ((MsecTimeout % ACPI_MSEC_PER_SEC) * ACPI_NSEC_PER_MSEC);

        /* Handle nanosecond overflow (field must be less than one second) */

        if (Time.tv_nsec >= ACPI_NSEC_PER_SEC)
        {
            Time.tv_sec += (Time.tv_nsec / ACPI_NSEC_PER_SEC);
            Time.tv_nsec = (Time.tv_nsec % ACPI_NSEC_PER_SEC);
        }

        while (((RetVal = acpi_sem_timedwait (Sem, &Time)) == -EINTR))
        //while (((RetVal = sem_timedwait (Sem, &Time)) == -1) && (errno == EINTR))
        {
            continue;   /* Restart if interrupted */

        }

        if (RetVal != 0)
        {
            if (RetVal != -ETIMEDOUT)
            //if (errno != ETIMEDOUT)
            {
                printk ("sem_timedwait: %s\n", strerror(-RetVal));
                //perror ("sem_timedwait");
            }
            Status = (AE_TIME);
        }
#endif
        break;
    }

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsSignalSemaphore
 *
 * PARAMETERS:  Handle              - Handle returned by AcpiOsCreateSemaphore
 *              Units               - Number of units to send
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Send units
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsSignalSemaphore (
    ACPI_HANDLE         Handle,
    UINT32              Units)
{
    UNUSED(Units);

    acpi_sem_t               *Sem = (acpi_sem_t *)Handle;


    if (!Sem)
    {
        return (AE_BAD_PARAMETER);
    }

    if (acpi_sem_post (Sem) < 0)
    //if (sem_post (Sem) == -1)
    {
        return (AE_LIMIT);
    }

    return (AE_OK);
}

#endif /* ACPI_SINGLE_THREADED */


/******************************************************************************
 *
 * FUNCTION:    Spinlock interfaces
 *
 * DESCRIPTION: Map these interfaces to semaphore interfaces
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateLock (
    ACPI_SPINLOCK           *OutHandle)
{

    return (AcpiOsCreateSemaphore (1, 1, OutHandle));
}


void
AcpiOsDeleteLock (
    ACPI_SPINLOCK           Handle)
{
    AcpiOsDeleteSemaphore (Handle);
}


ACPI_CPU_FLAGS
AcpiOsAcquireLock (
    ACPI_HANDLE             Handle)
{
    AcpiOsWaitSemaphore (Handle, 1, 0xFFFF);
    return (0);
}


void
AcpiOsReleaseLock (
    ACPI_SPINLOCK           Handle,
    ACPI_CPU_FLAGS          Flags)
{
    UNUSED(Flags);

    AcpiOsSignalSemaphore (Handle, 1);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInstallInterruptHandler
 *
 * PARAMETERS:  InterruptNumber     - Level handler should respond to.
 *              Isr                 - Address of the ACPI interrupt handler
 *              ExceptPtr           - Where status is returned
 *
 * RETURN:      Handle to the newly installed handler.
 *
 * DESCRIPTION: Install an interrupt handler. Used to install the ACPI
 *              OS-independent handler.
 *
 *****************************************************************************/

/* define some variables and a trampoline function for our kernel */
ACPI_OSD_HANDLER ServiceRout = NULL;
void *ServiceRoutArg = NULL;
int acpi_irq_callback(struct regs *unused, int arg);

struct handler_t acpi_irq_handler =
{
    .handler = acpi_irq_callback,
    .handler_arg = 0,
    .short_name = "acpi",
};

int acpi_irq_callback(struct regs *unused, int arg)
{
    UINT32 res;
    UNUSED(unused);

    //printk("acpi_irq_callback: 1 - irq %d\n", unused->int_no);
    
    if(ServiceRout)
    {
        res = ServiceRout((void *)(uintptr_t)arg /* ServiceRoutArg */);
        printk("acpi_irq_callback: 2 - res %d (%d, %d)\n", res, ACPI_INTERRUPT_HANDLED, ACPI_INTERRUPT_NOT_HANDLED);
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        
        return (res == ACPI_INTERRUPT_HANDLED) ? 1 : 0;
    }

    return 0;
}


UINT32
AcpiOsInstallInterruptHandler (
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine,
    void                    *Context)
{
    ServiceRout = ServiceRoutine;
    //acpi_irq_handler.handler_arg = Context;
    ServiceRoutArg = Context;
    register_irq_handler(InterruptNumber, &acpi_irq_handler);
    enable_irq(InterruptNumber);
    
    //printk("AcpiOsInstallInterruptHandler: irq %d\n", InterruptNumber);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsRemoveInterruptHandler
 *
 * PARAMETERS:  Handle              - Returned when handler was installed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Uninstalls an interrupt handler.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsRemoveInterruptHandler (
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine)
{
    UNUSED(ServiceRoutine);
    
    ServiceRout = NULL;
    ServiceRoutArg = NULL;
    unregister_irq_handler(InterruptNumber, &acpi_irq_handler);

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsStall
 *
 * PARAMETERS:  microseconds        - Time to sleep
 *
 * RETURN:      Blocks until sleep is completed.
 *
 * DESCRIPTION: Sleep at microsecond granularity
 *
 *****************************************************************************/

void
AcpiOsStall (
    UINT32                  microseconds)
{
    volatile time_t aticks = ticks;
    time_t nticks;
    
    nticks = (microseconds / ACPI_USEC_PER_SEC) * PIT_FREQUENCY;
    nticks += ticks;

    if(microseconds % ACPI_USEC_PER_SEC)
    {
        nticks++;
    }
    
    while(aticks < nticks)
    {
        aticks = ticks;
    }

    /*
    if (microseconds)
    {
        usleep (microseconds);
    }
    */
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsSleep
 *
 * PARAMETERS:  milliseconds        - Time to sleep
 *
 * RETURN:      Blocks until sleep is completed.
 *
 * DESCRIPTION: Sleep at millisecond granularity
 *
 *****************************************************************************/

void
AcpiOsSleep (
    UINT64                  milliseconds)
{
    /*
    register struct task_t *ct = get_cur_task();
    time_t nticks;
    
    nticks = (milliseconds / ACPI_MSEC_PER_SEC) * PIT_FREQUENCY;
    
    if(milliseconds % ACPI_MSEC_PER_SEC)
    {
        nticks++;
    }

    ct->alarm = ticks + nticks;
    block_task(ct, 1);
    ct->alarm = 0;
    */

    struct timespec rqtp;
    
    rqtp.tv_sec = 0;
    rqtp.tv_nsec = milliseconds * 1000000;
    syscall_nanosleep(&rqtp, NULL);


#if 0

    /* Sleep for whole seconds */

    sleep (milliseconds / ACPI_MSEC_PER_SEC);

    /*
     * Sleep for remaining microseconds.
     * Arg to usleep() is in usecs and must be less than 1,000,000 (1 second).
     */
    usleep ((milliseconds % ACPI_MSEC_PER_SEC) * ACPI_USEC_PER_MSEC);

#endif

}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTimer
 *
 * PARAMETERS:  None
 *
 * RETURN:      Current time in 100 nanosecond units
 *
 * DESCRIPTION: Get the current system time
 *
 *****************************************************************************/

UINT64
AcpiOsGetTimer (
    void)
{
    struct timeval          time;


    /* This timer has sufficient resolution for user-space application code */

    //gettimeofday (&time, NULL);

    time_t t = now();
    
    time.tv_sec  = t / 1e6;
    time.tv_usec = t % (time_t)1e6;


    /* (Seconds * 10^7 = 100ns(10^-7)) + (Microseconds(10^-6) * 10^1 = 100ns) */

    return (((UINT64) time.tv_sec * ACPI_100NSEC_PER_SEC) +
            ((UINT64) time.tv_usec * ACPI_100NSEC_PER_USEC));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadPciConfiguration
 *
 * PARAMETERS:  PciId               - Seg/Bus/Dev
 *              PciRegister         - Device Register
 *              Value               - Buffer where value is placed
 *              Width               - Number of bits
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read data from PCI configuration space
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadPciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  PciRegister,
    UINT64                  *Value,
    UINT32                  Width)
{
    UNUSED(PciId);
    UNUSED(PciRegister);
    UNUSED(Width);

    *Value = 0;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritePciConfiguration
 *
 * PARAMETERS:  PciId               - Seg/Bus/Dev
 *              PciRegister         - Device Register
 *              Value               - Value to be written
 *              Width               - Number of bits
 *
 * RETURN:      Status.
 *
 * DESCRIPTION: Write data to PCI configuration space
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWritePciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  PciRegister,
    UINT64                  Value,
    UINT32                  Width)
{
    UNUSED(PciId);
    UNUSED(PciRegister);
    UNUSED(Width);
    UNUSED(Value);

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadPort
 *
 * PARAMETERS:  Address             - Address of I/O port/register to read
 *              Value               - Where value is placed
 *              Width               - Number of bits
 *
 * RETURN:      Value read from port
 *
 * DESCRIPTION: Read data from an I/O port or register
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadPort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width)
{

    switch (Width)
    {
    case 8:

        *Value = inb(Address);  // 0xFF;
        break;

    case 16:

        *Value = inw(Address);  // 0xFFFF;
        break;

    case 32:

        *Value = inl(Address);  // 0xFFFFFFFF;
        break;

    default:

        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritePort
 *
 * PARAMETERS:  Address             - Address of I/O port/register to write
 *              Value               - Value to write
 *              Width               - Number of bits
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write data to an I/O port or register
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWritePort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  Value,
    UINT32                  Width)
{

    switch (Width)
    {
    case 8:

        outb(Address, Value);
        break;

    case 16:

        outw(Address, Value);
        break;

    case 32:

        outl(Address, Value);
        break;

    default:

        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadMemory
 *
 * PARAMETERS:  Address             - Physical Memory Address to read
 *              Value               - Where value is placed
 *              Width               - Number of bits (8,16,32, or 64)
 *
 * RETURN:      Value read from physical memory address. Always returned
 *              as a 64-bit integer, regardless of the read width.
 *
 * DESCRIPTION: Read data from a physical memory address
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  *Value,
    UINT32                  Width)
{
    physical_addr phys = align_down(Address);
    virtual_addr virt;

    switch (Width)
    {
    case 8:
    case 16:
    case 32:
    case 64:
        *Value = 0;

        // temporarily map the physical frame to a virtual address
        if((virt = phys_to_virt(phys, PTE_FLAGS_PW, REGION_ACPI)) == (virtual_addr)-1)
        /*
        if((virt = phys_to_virt(phys, ACPI_MEMORY_START, ACPI_MEMORY_END,
                                &acpi_lock, PTE_FLAGS_PW)) == (virtual_addr)-1)
        */
        {
            return (AE_NO_MEMORY);
        }

        // get the requested value
        if(Width == 8)
        {
            uint8_t val = *((uint8_t *)virt + (Address - phys));
            *Value = (uint64_t)val;
        }
        else if(Width == 16)
        {
            uint16_t val = *((uint16_t *)virt + (Address - phys));
            *Value = (uint64_t)val;
        }
        else if(Width == 32)
        {
            uint32_t val = *((uint32_t *)virt + (Address - phys));
            *Value = (uint64_t)val;
        }
        else
        {
            uint64_t val = *((uint64_t *)virt + (Address - phys));
            *Value = (uint64_t)val;
        }

        // unmap the temporary virtual address
        pt_entry *page = get_page_entry((void *) virt);

        *page = 0;
        vmmngr_flush_tlb_entry(virt);

        break;

    default:

        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWriteMemory
 *
 * PARAMETERS:  Address             - Physical Memory Address to write
 *              Value               - Value to write
 *              Width               - Number of bits (8,16,32, or 64)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write data to a physical memory address
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWriteMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  Value,
    UINT32                  Width)
{
    physical_addr phys = align_down(Address);
    virtual_addr virt;

    switch (Width)
    {
    case 8:
    case 16:
    case 32:
    case 64:

        // temporarily map the physical frame to a virtual address
        if((virt = phys_to_virt(phys, PTE_FLAGS_PW, REGION_ACPI)) == (virtual_addr)-1)
        /*
        if((virt = phys_to_virt(phys, ACPI_MEMORY_START, ACPI_MEMORY_END,
                                &acpi_lock, PTE_FLAGS_PW)) == (virtual_addr)-1)
        */
        {
            return (AE_NO_MEMORY);
        }

        // get the requested value
        if(Width == 8)
        {
            *((uint8_t *)virt + (Address - phys)) = (uint8_t)Value;
        }
        else if(Width == 16)
        {
            *((uint16_t *)virt + (Address - phys)) = (uint16_t)Value;
        }
        else if(Width == 32)
        {
            *((uint32_t *)virt + (Address - phys)) = (uint32_t)Value;
        }
        else
        {
            *((uint64_t *)virt + (Address - phys)) = Value;
        }

        // unmap the temporary virtual address
        pt_entry *page = get_page_entry((void *) virt);

        *page = 0;
        vmmngr_flush_tlb_entry(virt);

        break;

    default:

        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadable
 *
 * PARAMETERS:  Pointer             - Area to be verified
 *              Length              - Size of area
 *
 * RETURN:      TRUE if readable for entire length
 *
 * DESCRIPTION: Verify that a pointer is valid for reading
 *
 *****************************************************************************/

BOOLEAN
AcpiOsReadable (
    void                    *Pointer,
    ACPI_SIZE               Length)
{
    virtual_addr vstart = align_down((virtual_addr)Pointer);
    virtual_addr vend = align_up((virtual_addr)Pointer + Length);
    virtual_addr i;

    for(i = vstart; i < vend; i += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)i);

        if(PTE_FRAME(*pt) == 0)
        //if(pt_entry_get_frame(pt) == 0)
        {
            return (FALSE);
        }
        
        if(!PTE_PRESENT(*pt))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritable
 *
 * PARAMETERS:  Pointer             - Area to be verified
 *              Length              - Size of area
 *
 * RETURN:      TRUE if writable for entire length
 *
 * DESCRIPTION: Verify that a pointer is valid for writing
 *
 *****************************************************************************/

BOOLEAN
AcpiOsWritable (
    void                    *Pointer,
    ACPI_SIZE               Length)
{
    virtual_addr vstart = align_down((virtual_addr)Pointer);
    virtual_addr vend = align_up((virtual_addr)Pointer + Length);
    virtual_addr i;

    for(i = vstart; i < vend; i += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)i);

        if(PTE_FRAME(*pt) == 0)
        //if(pt_entry_get_frame(pt) == 0)
        {
            return (FALSE);
        }
        
        if(!PTE_PRESENT(*pt) || !PTE_WRITABLE(*pt))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsSignal
 *
 * PARAMETERS:  Function            - ACPI A signal function code
 *              Info                - Pointer to function-dependent structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Miscellaneous functions. Example implementation only.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsSignal (
    UINT32                  Function,
    void                    *Info)
{
    UNUSED(Info);

    switch (Function)
    {
    case ACPI_SIGNAL_FATAL:

        break;

    case ACPI_SIGNAL_BREAKPOINT:

        break;

    default:

        break;
    }

    return (AE_OK);
}

/* Optional multi-thread support */

#ifndef ACPI_SINGLE_THREADED
/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetThreadId
 *
 * PARAMETERS:  None
 *
 * RETURN:      Id of the running thread
 *
 * DESCRIPTION: Get the ID of the current (running) thread
 *
 *****************************************************************************/

ACPI_THREAD_ID
AcpiOsGetThreadId (
    void)
{
    volatile struct task_t *ct = get_cur_task();
    //struct task_t *ct = cur_task;
    
    if(!ct)
    {
        return 1;
    }
    
    //return (ACPI_THREAD_ID)(ct->tid);
    return (ACPI_THREAD_ID)(ct->pid);
    
    /*
    return (ACPI_CAST_PTHREAD_T (ct->tid));
    
    pthread_t               thread;


    thread = pthread_self();
    return (ACPI_CAST_PTHREAD_T (thread));
    */
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsExecute
 *
 * PARAMETERS:  Type                - Type of execution
 *              Function            - Address of the function to execute
 *              Context             - Passed as a parameter to the function
 *
 * RETURN:      Status.
 *
 * DESCRIPTION: Execute a new thread
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsExecute (
    ACPI_EXECUTE_TYPE       Type,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context)
{
    UNUSED(Type);
    void (*func)(void *) = (void (*)(void *))Function;
    
    if(start_kernel_task("acpi", func, Context, NULL, 0) <= 0)
    {
        kpanic("Failed to create thread in AcpiOsExecute()");
    }

    /*
    pthread_t               thread;
    int                     ret;


    ret = pthread_create (&thread, NULL, (PTHREAD_CALLBACK) Function, Context);
    if (ret)
    {
        AcpiOsPrintf("Create thread failed");
    }
    */
    
    return (0);
}

#else /* ACPI_SINGLE_THREADED */
ACPI_THREAD_ID
AcpiOsGetThreadId (
    void)
{
    return (1);
}

ACPI_STATUS
AcpiOsExecute (
    ACPI_EXECUTE_TYPE       Type,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context)
{

    Function (Context);

    return (AE_OK);
}

#endif /* ACPI_SINGLE_THREADED */


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitEventsComplete
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Wait for all asynchronous events to complete. This
 *              implementation does nothing.
 *
 *****************************************************************************/

void
AcpiOsWaitEventsComplete (
    void)
{
    return;
}


/******************************************************************************
 *
 * FUNCTION:    Local cache interfaces
 *
 * DESCRIPTION: Implements cache interfaces via malloc/free for testing
 *              purposes only.
 *
 *****************************************************************************/

#ifndef ACPI_USE_LOCAL_CACHE

ACPI_STATUS
AcpiOsCreateCache (
    char                    *CacheName,
    UINT16                  ObjectSize,
    UINT16                  MaxDepth,
    ACPI_CACHE_T            **ReturnCache)
{
    ACPI_MEMORY_LIST        *NewCache;


    NewCache = malloc (sizeof (ACPI_MEMORY_LIST));
    if (!NewCache)
    {
        return (AE_NO_MEMORY);
    }

    memset (NewCache, 0, sizeof (ACPI_MEMORY_LIST));
    NewCache->ListName = CacheName;
    NewCache->ObjectSize = ObjectSize;
    NewCache->MaxDepth = MaxDepth;

    *ReturnCache = (ACPI_CACHE_T) NewCache;
    return (AE_OK);
}

ACPI_STATUS
AcpiOsDeleteCache (
    ACPI_CACHE_T            *Cache)
{
    free (Cache);
    return (AE_OK);
}

ACPI_STATUS
AcpiOsPurgeCache (
    ACPI_CACHE_T            *Cache)
{
    return (AE_OK);
}

void *
AcpiOsAcquireObject (
    ACPI_CACHE_T            *Cache)
{
    void                    *NewObject;

    NewObject = malloc (((ACPI_MEMORY_LIST *) Cache)->ObjectSize);
    memset (NewObject, 0, ((ACPI_MEMORY_LIST *) Cache)->ObjectSize);

    return (NewObject);
}

ACPI_STATUS
AcpiOsReleaseObject (
    ACPI_CACHE_T            *Cache,
    void                    *Object)
{
    free (Object);
    return (AE_OK);
}

#endif /* ACPI_USE_LOCAL_CACHE */


#if 0

inline ACPI_STATUS
AcpiOsInitializeDebugger (
    void)
{
    return AE_OK;
}

inline void
AcpiOsTerminateDebugger (
    void)
{
    return;
}

#endif

