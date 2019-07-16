/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file platform/cannonlake/include/platform/asm_memory_management.h
 * \brief Macros for power gating memory banks specific for cAVS 1.8
 * \(CannonLake)
 * \author Lech Betlej <lech.betlej@linux.intel.com>
 */

#ifndef __PLATFORM_ASM_MEMORY_MANAGEMENT_H__
#define __PLATFORM_ASM_MEMORY_MANAGEMENT_H__

#ifndef ASSEMBLY
#warning "ASSEMBLY macro not defined. Header can't be inluded in C files"
#warning "The file is intended to be includded in assembly files only."
#endif

#include <sof/lib/memory.h>
#include <sof/lib/shim.h>

/**
 * powers down entire hpsram. on entry lirerals and code for section from
 * where this code is executed needs to be placed in memory which is not
 * HPSRAM (in case when this code is located in HPSRAM, lock memory in L1$ or
 * L1 SRAM)
 */
.macro m_cavs_hpsram_power_down_entire ax, ay, az
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
.endm

.macro m_cavs_hpsram_power_change segment_index, mask, ax, ay, az
	movi \ax, SHIM_HSPGCTL(\segment_index)
	movi \ay, SHIM_HSPGISTS(\segment_index)
	s32i \mask, \ax, 0
	memw
	// assumed that HDA shared dma buffer will be in LPSRAM
1 :
	l32i \ax, \ay, 0
	bne \ax, \mask, 1b
.endm

.macro m_cavs_lpsram_power_down_entire ax, ay, az
	movi \az, LSPGISTS
	movi \ax, LSPGCTL
	movi \ay, LPSRAM_MASK()
	s32i \ay, \ax, 0
	memw
  // assumed that HDA shared dma buffer will be in LPSRAM
	movi \ax, 4096
1 :
	addi \ax, \ax, -1
	bnez \ax, 1b
.endm

#endif /* __PLATFORM_ASM_MEMORY_MANAGEMENT_H__ */
