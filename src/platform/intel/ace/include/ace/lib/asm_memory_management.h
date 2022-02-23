/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file
 * \brief Macros for power gating memory banks specific for ACE 1.0
 * \author Lech Betlej <lech.betlej@linux.intel.com>
 */

#ifndef __ACE_LIB_ASM_MEMORY_MANAGEMENT_H__
#define __ACE_LIB_ASM_MEMORY_MANAGEMENT_H__

#ifndef ASSEMBLY
#warning "ASSEMBLY macro not defined. Header can't be included in C files"
#warning "The file is intended to be included in assembly files only."
#endif /* ASSEMBLY */

#include <ace/version.h>

#include <sof/lib/memory.h>
#include <sof/lib/shim.h>

/**
 * Macro powers down entire HPSRAM. On entry literals and code for section from
 * where this code is executed need to be placed in memory which is not
 * HPSRAM (in case when this code is located in HPSRAM, lock memory in L1$ or
 * L1 SRAM)
 */
.macro m_ace_hpsram_power_down_entire ax, ay, az
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

.macro m_ace_hpsram_power_change segment_index, mask, ax, ay, az, au, aw
	.if \segment_index == 0
		.if EBB_SEGMENT_SIZE > PLATFORM_HPSRAM_EBB_COUNT
			.set i_end, PLATFORM_HPSRAM_EBB_COUNT
		.else
			.set i_end, EBB_SEGMENT_SIZE
		.endif
	.elseif PLATFORM_HPSRAM_EBB_COUNT >= EBB_SEGMENT_SIZE
		.set i_end, PLATFORM_HPSRAM_EBB_COUNT - EBB_SEGMENT_SIZE
	.else
		.err
	.endif
	.set ebb_index, \segment_index << 5
	.set i, 0               /* i = bank bit in segment */

	rsr.sar \aw             /* store old sar value */

	movi \az, SHIM_HSPGCTL(ebb_index)
	movi \au, i_end - 1     /* au = banks count in segment */
2 :
	/* au = current bank in segment */
	mov \ax, \mask          /* ax = mask */
	ssr \au
	srl \ax, \ax            /* ax >>= current bank */
	extui \ax, \ax, 0, 1    /* ax &= BIT(1) */
	s8i \ax, \az, 0         /* HSxPGCTL.l2lmpge = ax */
	memw
	1 :
		l8ui \ay, \az, 4    /* ax=HSxPGISTS.l2lmpgis */
		bne \ax, \ay, 1b    /* wait till status==request */

	addi \az, \az, 8
	addi \au, \au, -1
	bnez \au, 2b

	wsr.sar \aw
.endm

.macro m_ace_lpsram_power_down_entire ax, ay, az, au
	movi \au, 8
	movi \az, LSPGCTL

	movi \ay, 1
2 :
	s8i \ay, \az, 0
	memw

1 :
	l8ui \ax, \az, 4
	bne \ax, \ay, 1b

	addi \az, \az, 8
	addi \au, \au, -1
	bnez \au, 2b
.endm

#endif /* __ACE_LIB_ASM_MEMORY_MANAGEMENT_H__ */
