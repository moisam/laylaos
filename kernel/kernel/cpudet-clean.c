/*
 * Copyright (c) 2006-2007 -  http://brynet.biz.tm - <brynet@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Modified by Mohammed Isam (2021) to work with LaylaOS. Main change is to
 * make the code save output to a string, instead of calling printf().
 * Code was also cleaned up a bit :)
 *
 * File was downloaded from OSDev WiKi:
 *     https://forum.osdev.org/viewtopic.php?t=11998
 */

/* You need to include a file with fairly(ish) compliant printf prototype,
 * Decimal and String support like %s and %d and this is truely all you need!
 */
#include <stdio.h> /* for printf() and sprintf() */
#include <string.h>
#include <mm/kheap.h>
#include <kernel/laylaos.h>

/* Required Declarations */
void do_intel(char *buf, size_t bufsz);
void do_amd(char *buf, size_t bufsz);
void printregs(char *buf, int eax, int ebx, int ecx, int edx);

#define cpuid(in, a, b, c, d)   \
    __asm__("cpuid": "=a" (a), "=b" (b), "=c" (c), "=d" (d) : "a" (in));

#define BUFSZ                   2048

/* Simply call this function detect_cpu(); */
/* or main() if your trying to port this as an independant application */
void detect_cpu(char **buf)
{
	//if(!buf)
	if(!(*buf = kmalloc(BUFSZ)))
	{
	    return;
	}
	
	unsigned long ebx, unused;
	cpuid(0, unused, ebx, unused, unused);
	
	switch(ebx)
	{
		case 0x756e6547: /* Intel Magic Code */
    		do_intel(*buf, BUFSZ);
    		break;

		case 0x68747541: /* AMD Magic Code */
    		do_amd(*buf, BUFSZ);
    		break;

		default:
    		//sprintf(*buf, "Unknown x86 CPU Detected\n");
    		ksprintf(*buf, BUFSZ, "Unknown x86 CPU Detected\n");
    		break;
	}
}


/* Intel Specific brand list */
char *Intel[] =
{
	"Brand ID Not Supported.", 
	"Intel(R) Celeron(R) processor", 
	"Intel(R) Pentium(R) III processor", 
	"Intel(R) Pentium(R) III Xeon(R) processor", 
	"Intel(R) Pentium(R) III processor", 
	"Reserved", 
	"Mobile Intel(R) Pentium(R) III processor-M", 
	"Mobile Intel(R) Celeron(R) processor", 
	"Intel(R) Pentium(R) 4 processor", 
	"Intel(R) Pentium(R) 4 processor", 
	"Intel(R) Celeron(R) processor", 
	"Intel(R) Xeon(R) Processor", 
	"Intel(R) Xeon(R) processor MP", 
	"Reserved", 
	"Mobile Intel(R) Pentium(R) 4 processor-M", 
	"Mobile Intel(R) Pentium(R) Celeron(R) processor", 
	"Reserved", 
	"Mobile Genuine Intel(R) processor", 
	"Intel(R) Celeron(R) M processor", 
	"Mobile Intel(R) Celeron(R) processor", 
	"Intel(R) Celeron(R) processor", 
	"Mobile Geniune Intel(R) processor", 
	"Intel(R) Pentium(R) M processor", 
	"Mobile Intel(R) Celeron(R) processor"
};


/* This table is for those brand strings that have two values depending on
 * the processor signature. It should have the same number of entries as the
 * above table.
 */
char *Intel_Other[] =
{
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Intel(R) Celeron(R) processor", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Intel(R) Xeon(R) processor MP", 
	"Reserved", 
	"Reserved", 
	"Intel(R) Xeon(R) processor", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved", 
	"Reserved"
};


/* Intel-specific information */
void do_intel(char *buf, size_t bufsz)
{
	//sprintf(buf, "Intel Specific Features:\n");
	ksprintf(buf, bufsz, "Intel Specific Features:\n");

	unsigned long eax, ebx, ecx, edx, max_eax, signature, unused;
	int model, family, type, brand, stepping, reserved;
	int extended_family = -1;
	char tmp[32];

	cpuid(1, eax, ebx, unused, unused);
	model = (eax >> 4) & 0xf;
	family = (eax >> 8) & 0xf;
	type = (eax >> 12) & 0x3;
	brand = ebx & 0xff;
	stepping = eax & 0xf;
	reserved = eax >> 14;
	signature = eax;

	//sprintf(tmp, "Type %d - ", type);
	ksprintf(tmp, sizeof(tmp), "Type %d - ", type);
	strcat(buf, tmp);

	switch(type)
	{
		case 0:
    		strcat(buf, "Original OEM");
    		break;

		case 1:
    		strcat(buf, "Overdrive");
    		break;

		case 2:
    		strcat(buf, "Dual-capable");
    		break;

		case 3:
    		strcat(buf, "Reserved");
    		break;
	}

	//sprintf(tmp, "\nFamily %d - ", family);
	ksprintf(tmp, sizeof(tmp), "\nFamily %d - ", family);
	strcat(buf, tmp);

	switch(family)
	{
		case 3:
    		strcat(buf, "i386");
    		break;

		case 4:
    		strcat(buf, "i486");
    		break;

		case 5:
    		strcat(buf, "Pentium");
    		break;

		case 6:
    		strcat(buf, "Pentium Pro");
    		break;

		case 15:
    		strcat(buf, "Pentium 4");
	}

	strcat(buf, "\n");

	if(family == 15)
	{
		extended_family = (eax >> 20) & 0xff;
		//sprintf(tmp, "Extended family %d\n", extended_family);
		ksprintf(tmp, sizeof(tmp), "Extended family %d\n", extended_family);
    	strcat(buf, tmp);
	}

	//sprintf(tmp, "Model %d - ", model);
	ksprintf(tmp, sizeof(tmp), "Model %d - ", model);
	strcat(buf, tmp);

	switch(family)
	{
		case 3:
    		break;

		case 4:
    		switch(model)
    		{
    			case 0:
    			case 1:
        			strcat(buf, "DX");
        			break;

    			case 2:
        			strcat(buf, "SX");
        			break;

    			case 3:
        			strcat(buf, "487/DX2");
        			break;

    			case 4:
        			strcat(buf, "SL");
        			break;

    			case 5:
        			strcat(buf, "SX2");
        			break;

    			case 7:
        			strcat(buf, "Write-back enhanced DX2");
        			break;

    			case 8:
        			strcat(buf, "DX4");
        			break;
    		}

    		break;

		case 5:
    		switch(model)
    		{
    			case 1:
        			strcat(buf, "60/66");
        			break;

    			case 2:
        			strcat(buf, "75-200");
        			break;

    			case 3:
        			strcat(buf, "for 486 system");
        			break;

    			case 4:
        			strcat(buf, "MMX");
        			break;
    		}

    		break;

		case 6:
    		switch(model)
    		{
    			case 1:
	        		strcat(buf, "Pentium Pro");
	        		break;
	
    			case 3:
        			strcat(buf, "Pentium II Model 3");
        			break;

    			case 5:
        			strcat(buf, "Pentium II Model 5/Xeon/Celeron");
        			break;

    			case 6:
        			strcat(buf, "Celeron");
        			break;

    			case 7:
        			strcat(buf, "Pentium III/Pentium III Xeon - external L2 cache");
        			break;

    			case 8:
        			strcat(buf, "Pentium III/Pentium III Xeon - internal L2 cache");
        			break;
    		}

    		break;

		case 15:
    		break;
	}

	strcat(buf, "\n");
	cpuid(0x80000000, max_eax, unused, unused, unused);

	/* Quok said: If the max extended eax value is high enough to support the
	 * processor brand string (values 0x80000002 to 0x80000004), then we'll 
	 * use that information to return the brand information. 
	 * Otherwise, we'll refer back to the brand tables above for backwards
	 * compatibility with older processors. 
	 * According to the Sept. 2006 Intel Arch Software Developer's Guide,
	 * if extended eax values are supported, then all 3 values for the 
	 * processor brand string are supported, but we'll test just to make sure
	 * and be safe.
	 */
	if(max_eax >= 0x80000004)
	{
		strcat(buf, "Brand: ");

		if(max_eax >= 0x80000002)
		{
			cpuid(0x80000002, eax, ebx, ecx, edx);
			printregs(buf, eax, ebx, ecx, edx);
		}

		if(max_eax >= 0x80000003)
		{
			cpuid(0x80000003, eax, ebx, ecx, edx);
			printregs(buf, eax, ebx, ecx, edx);
		}

		if(max_eax >= 0x80000004)
		{
			cpuid(0x80000004, eax, ebx, ecx, edx);
			printregs(buf, eax, ebx, ecx, edx);
		}

		strcat(buf, "\n");
	}
	else if(brand > 0)
	{
		//sprintf(tmp, "Brand %d - ", brand);
		ksprintf(tmp, sizeof(tmp), "Brand %d - ", brand);
		strcat(buf, tmp);

		if(brand < 0x18)
		{
			if(signature == 0x000006B1 || signature == 0x00000F13)
			{
				//sprintf(tmp, "%s\n", Intel_Other[brand]);
				ksprintf(tmp, sizeof(tmp), "%s\n", Intel_Other[brand]);
        		strcat(buf, tmp);
			}
			else
			{
				//sprintf(tmp, "%s\n", Intel[brand]);
				ksprintf(tmp, sizeof(tmp), "%s\n", Intel[brand]);
        		strcat(buf, tmp);
			}
		}
		else
		{
			strcat(buf, "Reserved\n");
		}
	}

	//sprintf(tmp, "Stepping: %d Reserved: %d\n", stepping, reserved);
	ksprintf(tmp, sizeof(tmp), "Stepping: %d Reserved: %d\n", stepping, reserved);
	strcat(buf, tmp);
}


/* Print Registers */
void printregs(char *buf, int eax, int ebx, int ecx, int edx)
{
	int j;
	char string[17];
	string[16] = '\0';

	for(j = 0; j < 4; j++)
	{
		string[j] = eax >> (8 * j);
		string[j + 4] = ebx >> (8 * j);
		string[j + 8] = ecx >> (8 * j);
		string[j + 12] = edx >> (8 * j);
	}

	strcat(buf, string);
}


/* AMD-specific information */
void do_amd(char *buf, size_t bufsz)
{
	//sprintf(buf, "AMD Specific Features:\n");
	ksprintf(buf, bufsz, "AMD Specific Features:\n");

	unsigned long extended, eax, ebx, ecx, edx, unused;
	int family, model, stepping, reserved;
	char tmp[40];

	cpuid(1, eax, unused, unused, unused);
	model = (eax >> 4) & 0xf;
	family = (eax >> 8) & 0xf;
	stepping = eax & 0xf;
	reserved = eax >> 12;

	//sprintf(tmp, "Family: %d Model: %d [", family, model);
	ksprintf(tmp, sizeof(tmp), "Family: %d Model: %d [", family, model);
	strcat(buf, tmp);

	switch(family)
	{
		case 4:
    		//sprintf(tmp, "486 Model %d", model);
    		ksprintf(tmp, sizeof(tmp), "486 Model %d", model);
        	strcat(buf, tmp);
    		break;

		case 5:
    		switch(model)
    		{
    			case 0:
	    		case 1:
    			case 2:
	    		case 3:
	    		case 6:
	    		case 7:
        			//sprintf(tmp, "K6 Model %d", model);
        			ksprintf(tmp, sizeof(tmp), "K6 Model %d", model);
                	strcat(buf, tmp);
        			break;

    			case 8:
        			strcat(buf, "K6-2 Model 8");
        			break;

    			case 9:
        			strcat(buf, "K6-III Model 9");
        			break;

    			default:
        			//sprintf(tmp, "K5/K6 Model %d", model);
        			ksprintf(tmp, sizeof(tmp), "K5/K6 Model %d", model);
                	strcat(buf, tmp);
        			break;
    		}

    		break;

		case 6:
    		switch(model)
    		{
    			case 1:
    			case 2:
    			case 4:
        			//sprintf(tmp, "Athlon Model %d", model);
        			ksprintf(tmp, sizeof(tmp), "Athlon Model %d", model);
                	strcat(buf, tmp);
        			break;

    			case 3:
        			strcat(buf, "Duron Model 3");
        			break;

    			case 6:
        			strcat(buf, "Athlon MP/Mobile Athlon Model 6");
        			break;

    			case 7:
        			strcat(buf, "Mobile Duron Model 7");
        			break;

    			default:
        			//sprintf(tmp, "Duron/Athlon Model %d", model);
        			ksprintf(tmp, sizeof(tmp), "Duron/Athlon Model %d", model);
                	strcat(buf, tmp);
        			break;
    		}

    		break;
	}

	strcat(buf, "]\n");
	cpuid(0x80000000, extended, unused, unused, unused);

	if(extended == 0)
	{
		return;
	}

	if(extended >= 0x80000002)
	{
		unsigned int j;
		strcat(buf, "Detected Processor Name: ");

		for(j = 0x80000002; j <= 0x80000004; j++)
		{
			cpuid(j, eax, ebx, ecx, edx);
			printregs(buf, eax, ebx, ecx, edx);
		}

		strcat(buf, "\n");
	}

	if(extended >= 0x80000007)
	{
		cpuid(0x80000007, unused, unused, unused, edx);

		if(edx & 1)
		{
			//sprintf(tmp, "Temperature Sensing Diode Detected!\n");
			ksprintf(tmp, sizeof(tmp), "Temperature Sensing Diode Detected!\n");
           	strcat(buf, tmp);
		}
	}

	//sprintf(tmp, "Stepping: %d Reserved: %d\n", stepping, reserved);
	ksprintf(tmp, sizeof(tmp), "Stepping: %d Reserved: %d\n", stepping, reserved);
   	strcat(buf, tmp);
}

