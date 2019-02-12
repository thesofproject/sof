/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 * Debug context switch.
 *
 */

#include <arch/gdb/xtensa-defs.h>
#define DENSITY_BREAK_INS_IDENT 0x40
#define NEXT_INST_OFFSET 0x03
#define PS_EXCM_EXCEPTION_MODE 0x10
#define PS_EXCM_MODE_MASK (~PS_EXCM_EXCEPTION_MODE)
#define DEBUG_GDB_MEM_LOC 0x9E008060

/*
Save special register designated by 'reg' into
backup space calculated by offset 'loc * 4' from
memory pointed by a3.
*/
.macro	SAVE_	reg, loc
	rsr	a1, \reg
	s32i	a1, a3, \loc * 4
.endm

.macro	SAVE	reg
	SAVE_	\reg, \reg
.endm

/*
Load special register designated by 'reg' from
backup space calculated by offset 'loc * 4' from
memory pointed by a3.
*/
.macro	LOAD_	reg, loc
	l32i	a1, a3, \loc * 4
	wsr	a1, \reg
.endm

.macro	LOAD	reg
	LOAD_	\reg, \reg
.endm



.text

/*
Temporary stack for context switch
TODO: move it to dedicated GDB_STACK section
*/

gdb_stack:
	.fill 0x1000 , 4 , 0
gdb_stack_end:

.global DebugExceptionEntry
.align	4

/*
Backup important special registers plus
all regular ones (whole register file).
Change EXCM field of PS back to normal mode
after an interrupt took place.
*/
DebugExceptionEntry:

	movi	a2, aregs
	s32i	a0, a2, 0
	s32i	a1, a2, 4
	rsr	a1, DEBUG_EXCSAVE
	s32i	a1, a2, 8
	s32i	a3, a2, 12

	movi	a3, sregs
	SAVE	LBEG
	SAVE	LEND
	SAVE	LCOUNT
	SAVE	SAR
	SAVE	WINDOWBASE
	SAVE	WINDOWSTART

	rsr	a1, DEBUG_PC
	l8ui	a2, a1, 1
	movi 	a0, DENSITY_BREAK_INS_IDENT
	bne	a2, a0, 1f
	addi	a1, a1, NEXT_INST_OFFSET
1:
	s32i	a1, a3, DEBUG_PC * 4

	SAVE	EXCSAVE_1
	SAVE_	DEBUG_PS, PS
	SAVE	EXCCAUSE
	SAVE	DEBUGCAUSE
	SAVE	EXCVADDR

	/*
	(XCHAL_NUM_AREGS / 4 - 1) - A number which holds information on how many
	registers are left to backup. Divide by four since we backup registers
	in group of four. Minus one, since one group has already been saved.
	*/
	movi	a1, XCHAL_NUM_AREGS / 4 - 1
	movi	a2, aregs
1:
	s32i	a4, a2, 16
	s32i	a5, a2, 20
	s32i	a6, a2, 24
	s32i	a7, a2, 28

	addi	a6, a2, 16
	addi	a5, a1, -1
	rotw	1
	bnez	a1, 1b

	movi	a1, 1
	wsr	a1, windowstart
	movi	a0, 0
	wsr	a0, windowbase
	rsync

	/* Setup of stack frame with 20 bytes for extra save area */
	movi	a0, 0
	movi	a1, gdb_stack + STACK_SIZE - 20
	rsr	a2, PS

	/* Set exception mode back to normal */
	movi a3, PS_EXCM_MODE_MASK
	and	a2, a2, a3
	wsr	a2, PS
	rsync

	/* reset icountlevel - essential when coming back from single step */
	movi a2, 0x00
	wsr a2, ICOUNTLEVEL

	movi	a4, gdb_handle_exception
	callx4	a4

/*
Restore important special registers plus
all regular ones (whole register file).
Change EXCM field of PS back to exception mode
and return from interrupt.
*/
DebugExceptionExit:
	movi	a2, DebugExceptionEntry
	wsr	a2, DEBUG_EXCSAVE

	rsr	a4, PS
	movi a3, PS_EXCM_EXCEPTION_MODE
	or	a4, a4, a3
	wsr	a4, PS
	rsync

	movi	a3, sregs
	LOAD	LBEG
	LOAD	LEND
	LOAD	LCOUNT
	LOAD	SAR
	LOAD	WINDOWBASE
	rsync

	movi	a3, sregs
	LOAD	WINDOWSTART
	LOAD	DEBUG_PC
	LOAD	EXCSAVE_1
	LOAD	EXCCAUSE
	LOAD	EXCVADDR
	LOAD 	INTENABLE
	rsync

	movi	a6, aregs
	movi	a5, XCHAL_NUM_AREGS / 4 - 2
1:
	l32i	a0, a6, 0
	l32i	a1, a6, 4
	l32i	a2, a6, 8
	l32i	a3, a6, 12

	beqz	a5, 2f
	addi	a10, a6, 16
	addi	a9, a5, -1
	rotw	1
	j	1b
2:
	l32i	a4, a6, 16
	l32i	a5, a6, 20
	l32i	a7, a6, 28
	l32i	a6, a6, 24
	rotw	2

	rfi	XCHAL_DEBUGLEVEL


/*
Put some important interrupt related registers into memory window
pointed by DEBUG_GDB_MEM_LOC
*/
.global gdb_debug_info
.align	4

gdb_debug_info:

	entry	a1, 16
	movi	a3, DEBUG_GDB_MEM_LOC
	l32i	a4, a2, 0 //load 4 bytes of message ID from incoming argv
	rsr 	a4, EPC_1
	rsr 	a5, EPC_2
	rsr 	a6, EXCCAUSE
	rsr 	a7, DEPC
	rsr 	a8, DEBUG_PS

	s32i 	a4, a3, 0
	s32i 	a5, a3, 4
	s32i	a6, a3, 8
	s32i    a7, a3, 12
	s32i	a8, a3, 16

	isync
	rsync
	retw

.size	gdb_debug_info, . -gdb_debug_info
