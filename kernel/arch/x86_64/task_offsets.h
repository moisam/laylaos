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
.set PROPERTY_TRACE_SYSCALLS,   (1 << 0)
.set PROPERTY_TRACE_SYSEMU,     (1 << 1)
//.set PROPERTY_VM86,             (1 << 5)
.set PROPERTY_HANDLING_SIG,     (1 << 11)
.set PROPERTY_IN_SYSCALL,       (1 << 12)

/* offsets into the task struct */
.set TASK_STATE_FIELD,          288
.set TASK_TIME_LEFT_FIELD,      292
.set TASK_PROPERTIES_FIELD,     300

.set TASK_USER_FIELD,           4
.set TASK_IN_KERNEL_MODE_FIELD, 8

.set TASK_KSTACK_PHYS,          16
.set TASK_KSTACK_VIRT,          24
.set TASK_PDIR_PHYS,            32
.set TASK_PDIR_VIRT,            40

.set TASK_R15_FIELD,            48
.set TASK_R14_FIELD,            56
.set TASK_R13_FIELD,            64
.set TASK_R12_FIELD,            72
.set TASK_R11_FIELD,            80
.set TASK_R10_FIELD,            88
.set TASK_R9_FIELD,             96
.set TASK_R8_FIELD,             104

.set TASK_RSP_FIELD,            112
.set TASK_RBP_FIELD,            120
.set TASK_RDI_FIELD,            128
.set TASK_RSI_FIELD,            136
.set TASK_RDX_FIELD,            144
.set TASK_RCX_FIELD,            152
.set TASK_RBX_FIELD,            160
.set TASK_RAX_FIELD,            168

.set TASK_INTNO_FIELD,          176
.set TASK_ERRCODE_FIELD,        184

.set TASK_RIP_FIELD,            192
.set TASK_CS_FIELD,             200
.set TASK_RFLAGS_FIELD,         208
.set TASK_USERRSP_FIELD,        216
.set TASK_SS_FIELD,             224

#.set TASK_SYSCALL_REGS,         232
.set TASK_EXECVE_REGS,          232

