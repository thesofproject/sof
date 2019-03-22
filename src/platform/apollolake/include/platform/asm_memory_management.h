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
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file platform/apollolake/include/platform/asm_memory_management.h
 * \brief Macros for power gating memory banks specific for Apollolake
 * \author Lech Betlej <lech.betlej@linux.intel.com>
 */
#ifndef ASM_MEMORY_MANAGEMENT_H
#define ASM_MEMORY_MANAGEMENT_H

#ifndef ASSEMBLY
#warning "ASSEMBLY macro not defined."
#endif

#include <sof/bit.h>
#include <platform/shim.h>
#include <platform/platform.h>

	/* Macro powers down entire hpsram. on entry literals and code for
	 * section from where this code is executed needs to be placed in
	 * memory which is not HPSRAM (in case when this code is located in
	 * HPSRAM lock memory in L1$ or L1 SRAM)
	 */
	.macro m_cavs_hpsram_power_off ax, ay, az
	// SEGMENT #0
	movi \az, (SHIM_BASE + SHIM_HSPGISTS)
	movi \ax, (SHIM_BASE + SHIM_HSPGCTL)
	movi \ay, HPSRAM_MASK()
	s32i \ay, \ax, 0
	memw
	/* since HPSRAM EBB bank #0 might be used as buffer for legacy
	 * streaming, should not be checked in status
	 */
	movi \ax, 0xfffffffe
	and \ay, \ay, \ax
	1 :
	l32i \ax, \az, 0
	and \ax, \ax, \ay
	bne \ax, \ay, 1b
	/* there is no possibility to check from DSP whether EBB #0 is actually
	 * in use therefore wait additional 4K DSP cycles as chicken check -
	 * after that time EBB #0 should be already power gated unless is used
	 * by other HW components (like HD-A)
	 */
	l32i \ax, \az, 0
	beq \ax, \ay, m_cavs_hpsram_power_off_end
	movi \ax, 4096
	1 :
	addi \ax, \ax, -1
	bnez \ax, 1b
	m_cavs_hpsram_power_off_end :
	.endm

	.macro m_cavs_lpsram_power_off ax, ay, az
	movi \az, (SHIM_BASE + SHIM_LSPGISTS)
	movi \ax, (SHIM_BASE + SHIM_LSPGCTL)
	movi \ay, LPSRAM_MASK()
	s32i \ay, \ax, 0
	memw
	1 :
	l32i \ax, \az, 0
	bne \ax, \ay, 1b
	.endm

#endif /* ASM_MEMORY_MANAGEMENT_H */
