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
 * \file platform/cannonlake/include/platform/asm_memory_management.h
 * \brief Macros for power gating memory banks specific for cAVS 1.8
 * \(CannonLake)
 * \author Lech Betlej <lech.betlej@linux.intel.com>
 */
#ifndef ASM_MEMORY_MANAGEMENT_H
#define ASM_MEMORY_MANAGEMENT_H

#ifndef ASSEMBLY
#warning "ASSEMBLY macro not defined. Header can't be inluded in C files"
#warning "The file is intended to be includded in assembly files only."
#endif

#include <platform/shim.h>
#include <platform/platcfg.h>

#define MAX_EBB_BANKS_IN_SEGMENT	32
#define HPSRAM_MASK(seg_idx)\
	((1 << (PLATFORM_HPSRAM_EBB_COUNT\
	 - MAX_EBB_BANKS_IN_SEGMENT * seg_idx)) - 1)
#define LPSRAM_MASK	((1 << PLATFORM_LPSRAM_EBB_COUNT) - 1)
#define MAX_MEMORY_SEGMENTS ((PLATFORM_HPSRAM_EBB_COUNT + \
	MAX_EBB_BANKS_IN_SEGMENT - 1) / MAX_EBB_BANKS_IN_SEGMENT)

/**
 * powers down entire hpsram. on entry lirerals and code for section from
 * where this code is executed needs to be placed in memory which is not
 * HPSRAM (in case when this code is located in HPSRAM, lock memory in L1$ or
 * L1 SRAM)
 */
.macro m_cavs_hpsram_power_down_entire ax, ay, az
	//TODO: add LDO control
	// SEGMENT #0
	movi \az, SHIM_HSPGCTL(0)
	movi \ax, SHIM_HSPGISTS(0)
	movi \ay, HPSRAM_MASK(0)
	s32i \ay, \ax, 0
	memw
1 :
	l32i \ax, \az, 0
	bne \ax, \ay, 1b

	// SEGMENT #1
	movi \az, SHIM_HSPGCTL(1)
	movi \ax, SHIM_HSPGISTS(1)
	movi \ay, HPSRAM_MASK(1)
	s32i \ay, \ax, 0
	memw
1 :
	l32i \ax, \az, 0
	bne \ax, \ay, 1b
	// TODO: Add LDO control
.endm

.macro m_cavs_hpsram_power_change segment_index, mask, ax, ay, az
	// TODO: Add LDO Control
	movi \ax, SHIM_HSPGCTL(\segment_index)
	movi \ay, SHIM_HSPGISTS(\segment_index)
	s32i \mask, \ax, 0
	memw
	// assumed that HDA shared dma buffer will be in LPSRAM
1 :
	l32i \ax, \ay, 0
	bne \ax, \mask, 1b
	// TODO: Add LDO Control
.endm

.macro m_cavs_lpsram_power_down_entire ax, ay, az
	movi \az, LSPGISTS
	movi \ax, LSPGCTL
	movi \ay, LPSRAM_MASK
	s32i \ay, \ax, 0
	memw
  // assumed that HDA shared dma buffer will be in LPSRAM
	movi \ax, 4096
1 :
	addi \ax, \ax, -1
	bnez \ax, 1b
.endm

#endif /* ASM_MEMORY_MANAGEMENT_H */
