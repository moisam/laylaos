/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: task_offsets.h
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
 *  \file task_offsets.h
 *
 *  Offsets into the task struct, for use in assembly code.
 */

.extern NR_SYSCALLS
.extern cur_task

/* as defined in task.h */
.set TASK_RUNNING,              1
//.set PROPERTY_VM86,             (1 << 5)
.set PROPERTY_HANDLING_SIG,     (1 << 11)
.set PROPERTY_IN_SYSCALL,       (1 << 12)

/* offsets into the task struct */
.set TASK_STATE_FIELD,          104
.set TASK_TIME_LEFT_FIELD,      108
.set TASK_PROPERTIES_FIELD,     112
.set TASK_IN_KERNEL_MODE_FIELD, 8
//.set TASK_KSTACK_VIRT_FIELD,     16
//.set ESP_FIELD,             56
//.set USERESP_FIELD,         96
//.set EIP_FIELD,             84

.set TASK_GS_FIELD,             28
.set TASK_FS_FIELD,             32
.set TASK_ES_FIELD,             36
.set TASK_DS_FIELD,             40
.set TASK_EDI_FIELD,            44
.set TASK_ESI_FIELD,            48
.set TASK_EBP_FIELD,            52
.set TASK_ESP_FIELD,            56
.set TASK_EBX_FIELD,            60
.set TASK_EDX_FIELD,            64
.set TASK_ECX_FIELD,            68
.set TASK_EAX_FIELD,            72
.set TASK_INTNO_FIELD,          76
.set TASK_ERRCODE_FIELD,        80
.set TASK_EIP_FIELD,            84
.set TASK_CS_FIELD,             88
.set TASK_EFLAGS_FIELD,         92
.set TASK_USERESP_FIELD,        96
.set TASK_SS_FIELD,             100

