/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file platform/cannonlake/include/platform/asm_memory_management.h
 * \brief Macros for power gating memory banks specific for cAVS 1.8
 * \(CannonLake) and cAVS 2.0 (IceLake)
 * \author Lech Betlej <lech.betlej@linux.intel.com>
 */

#ifndef __CAVS_LIB_ASM_MEMORY_MANAGEMENT_H__
#define __CAVS_LIB_ASM_MEMORY_MANAGEMENT_H__

#ifndef ASSEMBLY
#warning "ASSEMBLY macro not defined. Header can't be included in C files"
#warning "The file is intended to be included in assembly files only."
#endif

#include <cavs/version.h>

#include <sof/lib/memory.h>
#include <sof/lib/shim.h>

#if CAVS_VERSION >= CAVS_VERSION_1_8
/**
 * Macro powers down entire HPSRAM. On entry literals and code for section from
 * where this code is executed need to be placed in memory which is not
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

.macro m_cavs_lpsram_power_down_entire ax, ay, az, loop_cnt_addr
	movi \az, LSPGISTS
	movi \ax, LSPGCTL
	movi \ay, LPSRAM_MASK()
	s32i \ay, \ax, 0
	memw
  // assumed that HDA shared dma buffer will be in LPSRAM
	movi \ax, \loop_cnt_addr
	l32i \ax, \ax, 0
1 :
	addi \ax, \ax, -1
	bnez \ax, 1b
.endm

#endif /* CAVS_VERSION == CAVS_VERSION_1_8 */
#endif /* __CAVS_LIB_ASM_MEMORY_MANAGEMENT_H__ */
