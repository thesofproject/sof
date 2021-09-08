/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 */

#include <arch/debug/offset-defs.h>
#include <xtensa/config/core-isa.h>
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

	/* store PS */
	rsr a3, PS
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
#if XCHAL_INTLEVEL5_MASK
	rsr  a6, EPC5
	s32i a6, a2, REG_OFFSET_EPC5
#endif
#if XCHAL_INTLEVEL6_MASK
	rsr  a6, EPC6
	s32i a6, a2, REG_OFFSET_EPC6
#endif
#if XCHAL_INTLEVEL7_MASK
	rsr  a6, EPC7
	s32i a6, a2, REG_OFFSET_EPC7
#endif
	rsr a6, EPS2
	s32i a6, a2, REG_OFFSET_EPS2
	rsr a6, EPS3
	s32i a6, a2, REG_OFFSET_EPS3
	rsr a6, EPS4
	s32i a6, a2, REG_OFFSET_EPS4
#if XCHAL_INTLEVEL5_MASK
	rsr a6, EPS5
	s32i a6, a2, REG_OFFSET_EPS5
#endif
#if XCHAL_INTLEVEL6_MASK
	rsr a6, EPS6
	s32i a6, a2, REG_OFFSET_EPS6
#endif
#if XCHAL_INTLEVEL7_MASK
	rsr a6, EPS7
	s32i a6, a2, REG_OFFSET_EPS7
#endif
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
