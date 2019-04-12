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
 */

#include <arch/offset-defs.h>
#include <xtensa/corebits.h>

	.section .text, "ax"
	.align 64
literals:
	.literal_position
	.global arch_dump_regs_a
	.type arch_dump_regs_a, @function

/*
 * params:
 * a2 - ptr to dump mem buffer
 * a3 - original PS
 */
arch_dump_regs_a:
	entry a1, 16

	/* all accessible physical registers */

	s32i a0, a2, REG_OFFSET_AR_BEGIN
	s32i a1, a2, REG_OFFSET_AR_BEGIN + 0x4
	s32i a2, a2, REG_OFFSET_AR_BEGIN + 0x8
	s32i a3, a2, REG_OFFSET_AR_BEGIN + 0xc
	s32i a4, a2, REG_OFFSET_AR_BEGIN + 0x10
	s32i a5, a2, REG_OFFSET_AR_BEGIN + 0x14
	s32i a6, a2, REG_OFFSET_AR_BEGIN + 0x18
	s32i a7, a2, REG_OFFSET_AR_BEGIN + 0x1c
	memw

	/* store original PS */
	s32i a3, a2, REG_OFFSET_PS

	/*
	 * copy original a2 to a3 as we will increment it in the loop with
	 * offset to AR registers with addition of 8 registers already read.
	 * It will be the base for next reg dump
	 */
	movi a3, REG_OFFSET_AR_BEGIN + 8*4
	add  a3, a2, a3

	/*
	 * storing rest of AREGS starts here
	 * a4 - number of 8-reg chunks to save (a0-a7 already done)
	 */
	movi a4, XCHAL_NUM_AREGS / 8 - 1

	/* set exception mode if we are on core 0 */
	rsr a6, PRID
	bnez a6, mask_interrupts_on_core0
	movi a5, PS_EXCM | PS_INTLEVEL(0x5)
	wsr a5, PS
	rsync

	/* exception mode set so no need to set interrupt mask */
	j store_register_loop

mask_interrupts_on_core0:
	/*
	 * if we are in core context different than 0
	 * disable interrupts on core 0
	 * only level 2 interrupts disabled for now on
	 */
	/* TODO */

store_register_loop:
	s32i a8, a3, 0
	s32i a9, a3, 4
	s32i a10, a3, 8
	s32i a11, a3, 0xc
	s32i a12, a3, 0x10
	s32i a13, a3, 0x14
	s32i a14, a3, 0x18
	s32i a15, a3, 0x1c

	addi a11, a3, 32 // after rotation a11 will be next a3
	addi a12, a4, -1 // after rotation a12 will be next a4 - iter decrement
	/*
	 * restore registers from current window to preserve backtrace
	 * upon return
	 */
	addi a3, a3, -20
	l32i a4, a3, 4
	l32i a3, a3, 0
	rotw 2
	bnez a4, store_register_loop
	rotw 2
	memw

dump_special_registers:
	rsr  a6, EPC2
	s32i a6, a2, REG_OFFSET_EPC2
	rsr  a6, EPC3
	s32i a6, a2, REG_OFFSET_EPC3
	rsr  a6, EPC4
	s32i a6, a2, REG_OFFSET_EPC4
	rsr  a6, EPC5
	s32i a6, a2, REG_OFFSET_EPC5
	rsr  a6, EPC6
	s32i a6, a2, REG_OFFSET_EPC6
	rsr  a6, EPC7
	s32i a6, a2, REG_OFFSET_EPC7

	rsr a6, EPS2
	s32i a6, a2, REG_OFFSET_EPS2
	rsr a6, EPS3
	s32i a6, a2, REG_OFFSET_EPS3
	rsr a6, EPS4
	s32i a6, a2, REG_OFFSET_EPS4
	rsr a6, EPS5
	s32i a6, a2, REG_OFFSET_EPS5
	rsr a6, EPS6
	s32i a6, a2, REG_OFFSET_EPS6
	rsr a6, EPS7
	s32i a6, a2, REG_OFFSET_EPS7

	rsr  a6, DEPC
	s32i a6, a2, REG_OFFSET_DEPC
	rsr  a6, DEBUGCAUSE
	s32i a6, a2, REG_OFFSET_DEBUGCAUSE
	rsr  a6, EXCCAUSE
	s32i a6, a2, REG_OFFSET_EXCCAUSE
	rsr  a6, INTERRUPT
	s32i a6, a2, REG_OFFSET_INTERRUPT
	rsr  a6, EXCVADDR
	s32i a6, a2, REG_OFFSET_EXCVADDR
	rsr  a6, EXCSAVE1
	s32i a6, a2, REG_OFFSET_EXCSAVE1
	rsr a6, WINDOWBASE
	s32i a6, a2, REG_OFFSET_WINDOWBASE
	rsr a6, WINDOWSTART
	s32i a6, a2, REG_OFFSET_WINDOWSTART

	/* restore processor_state */
restore_processor_state:
	/* restore previously saved PS before return */
	l32i a3, a2, REG_OFFSET_PS
	wsr a3, PS
	rsync
	retw
