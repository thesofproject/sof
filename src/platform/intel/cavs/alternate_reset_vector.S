/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 */


#include <rtos/bit.h>
#include <sof/common.h>
#include <rtos/interrupt.h>
#if CONFIG_XT_BOOT_LOADER && !CONFIG_VM_ROM
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#endif
#include <xtensa/cacheattrasm.h>
#include <xtensa/cacheasm.h>
#include <xtensa/coreasm.h>
#include <xtensa/config/system.h>
#include <xtensa/xdm-regs.h>
#include <xtensa/xtensa-xer.h>
#include <xtensa/xtruntime-core-state.h>

#include "xtos-internal.h"

.type   secondary_core_init, @function
.type   shared_vecbase_ptr, @object

.section        .AlternateResetVector.text, "ax"

	/* Alignemt and padding for 4 dwords is in order to meet
	 * lpsram header requiremetns  */
_LpsramHeader:
	j    _AltResetVector
	.align 4
	/* Magic value. */
	.long 0
	/* Lp-restore vector address. */
	.long 0
	/* Reserved. */
	.long 0

	.size    _LpsramHeader, . - _LpsramHeader

.align 4
_AltResetVectorLiterals:
	.literal_position

	.size    _AltResetVectorLiterals, . - _AltResetVectorLiterals

.align 4
.global _AltResetVector

.align 4
_AltResetVector:
	movi a0, 0
	movi a1, 1
	wsr  a1, WINDOWSTART
	wsr  a0, WINDOWBASE
	rsync

	get_prid a5
	movi a6, PLATFORM_PRIMARY_CORE_ID
	bne a5, a6, secondary_init //core0 handle restore, core1 always starts from scratch
	/* Currently no implementation for core 0:
	   - shutoff restore should be put either here on in L1SRAM */
	j alt_boot_error_loop

secondary_init:
	/* Block interrupt but not enable WOE */
	movi a3, PS_UM | PS_INTLEVEL(5)
	wsr a3, PS
	rsync

	movi a2, 0
	call0 _l1_cache_init
	movi a0, 0

alt_boot_secondary_core_idc_receiver:
	get_prid a5
	movi a3, IPC_DSP_BASE(0)
	movi a4, IPC_DSP_SIZE
	mull a4, a4, a5
	add a3, a3, a4

	l32i a2, a3, 0
	bbsi a2, 31, alt_boot_validate_idc_msg

alt_boot_secondary_core_enter_waiti:

	/* Setup int vector to be local */
	movi a2, _LpsramHeader
	wsr a2, vecbase

	get_prid a5
	// Get core interrupt reg
	// a5 should still contain processor id
	movi a3, IRQ_CPU_OFFSET
	mull a3, a3, a5
	movi a2, IRQ_BASE
	add  a3, a2, a3

	movi a2, 0xffffffff & ~(BIT(IRQ_BIT_LVL2_IDC))
	s32i a2, a3, 0
	movi a2, 0xffffffff
	s32i a2, a3, 0x10
	movi a2, 0xffffffff
	s32i a2, a3, 0x20
	movi a2, 0xffffffff
	s32i a2, a3, 0x30
	memw // at this point use l32i to read
	//Unmask Tensilica L2 interrupt
	movi a2, IRQ_MASK_EXT_LEVEL2

	/* Enable L2 level trigger (external) interrupt */
	movi a2, BIT(6)
	wsr a2, INTENABLE

	movi a2, 128

alt_boot_secondary_wait_for_waiti:
	addi a2, a2, -1
	nop
	bnez a2, alt_boot_secondary_wait_for_waiti

alt_boot_secondary_enter_waiti:
	isync
	extw
	waiti 1
	j alt_boot_secondary_core_idc_receiver

alt_boot_validate_idc_msg:
	/*  Core wake version: bits 0-8 (9 - bits) - core wake version must be 0x2. */
	l32i a2, a3, 0
	movi a4, 0x1FF
	and a2, a2, a4
	bnei a2, 0x2, alt_boot_handle_incorrect_idc
	/* ROM Control Version bits 24-28 (5 bits) - rom control version must be 0x1. */
	l32i a2, a3, 0
	srli a2, a2, 24
	movi a4, 0x1F
	and a2, a2, a4
	bnei a2, 0x1, alt_boot_handle_incorrect_idc

	.macro	init_vector	level
	  .if GREATERTHAN(XCHAL_NUM_INTLEVELS+1,\level)
	    .if XCHAL_DEBUGLEVEL-\level
	      .weak   _Level&level&FromVector
	      movi    a4, _Level&level&FromVector
	      writesr excsave \level a4
	      .if GREATERTHAN(\level,XCHAL_EXCM_LEVEL)
		movi    a5, _Pri_&level&_HandlerAddress
		s32i    a4, a5, 0
		/*  If user provides their own handler, that handler might
		 *  not provide its own _Pri_<n>_HandlerAddress variable for
		 *  linking handlers.  In that case, the reference below
		 *  would pull in the XTOS handler anyway, causing a conflict.
		 *  To avoid that, provide a weak version of it here:
		 */
		.pushsection .data, "aw"
		.global  _Pri_&level&_HandlerAddress
		.weak   _Pri_&level&_HandlerAddress
		.align	4
		_Pri_&level&_HandlerAddress: .space 4
		.popsection
	      .endif
	    .endif
	  .endif
	.endm


lp_reset_setup_vecbase:
	movi a2, shared_vecbase_ptr
	l32i a2, a2, 0
	beqz a2, alt_boot_error_loop
	/*  Apply alternate vector base given from ldscripts. */
	wsr a2, vecbase

	init_vector    2
	init_vector    3
	init_vector    4
	init_vector    5

alt_boot_secondary_core_wakeup:
	movi a2, 0
	wsr a2, INTENABLE
	rsync

	// Compute address to jump
	l32i a2, a3, 4
	slli a2, a2, 2
	// Clear busy bit
	l32i a4, a3, 0
	s32i a4, a3, 0
	memw

	jx a2
alt_boot_handle_incorrect_idc:
	// Clear BUSY
	l32i a2, a3, 0
	s32i a2, a3, 0
	memw
	// HW limitation read the register
	l32i a4, a3, 0
	j alt_boot_secondary_core_idc_receiver

alt_boot_error_loop:
	// TODO: consider some kind of status reporting here.
	j alt_boot_error_loop

	.size _AltResetVector, . - _AltResetVector

.section        .AlternateResetL2IntVector.text, "ax"

/* Note: at this moment it is essential that this is linked on
 * _LpsramHeader + 0x180 */
.align 4
.global _AltResetL2IntHandler
_AltResetL2IntHandler:
	xsr a2, excsave2
	xor a2, a2, a2
	wsr a2, intenable
	xsr a2, excsave2
	rfi 2
	.size _AltResetL2IntHandler, . - _AltResetL2IntHandler

.section        .LpsramCode.text, "ax"

.literal_position

.global    _l1_cache_init
.align 4

_l1_cache_init:
	mov a9, a2
	movi a3, CxL1CCFG
	bnez a9, l1_cache_enable_one_way

l1_cache_way_enable_all:
	movi a2, L1_CACHE_ALL_WAY_ENABLED_MASK
	movi a4, L1_CACHE_ALL_WAY_ACTIVE_MASK
	j l1_cache_enable_write

l1_cache_enable_one_way:
	movi a2, L1_CACHE_ONE_WAY_ENABLED_MASK
	movi a4, L1_CACHE_ONE_WAY_ACTIVE_MASK

l1_cache_enable_write:
	s32i a2, a3, 0

l1_cache_wait_for_way_enable_loop:
	l32i a2, a3, 0
	and a2, a2, a4
	bne a2, a4, l1_cache_wait_for_way_enable_loop

#if XCHAL_HAVE_PREFETCH
l1_cache_pref_ebb_enble:
	movi a3, CxL1PCFG
	movi a2, L1_CACHE_PREFETCHER_ENABLED
	movi a4, L1_CACHE_PREFETCHER_ACTIVE
	s32i a2, a3, 0

l1_cache_wait_for_prefetcher:
	l32i a2, a3, 0
	and a2, a2, a4
	bne a4, a2, l1_cache_wait_for_prefetcher
#endif

l1_cache_inv_unlock:
#if ! XCHAL_HAVE_ICACHE_DYN_WAYS
	icache_reset    a2, a3
#endif

#if ! XCHAL_HAVE_DCACHE_DYN_WAYS
	dcache_reset    a2, a3
#endif

l1_cache_set_prefctl:
#if XCHAL_HAVE_PREFETCH
	/* Enable cache prefetch if present.  */
	movi a2, L1_CACHE_PREFCTL_VALUE
	wsr a2, PREFCTL
#endif

l1_cache_setup_memprotection:
	movi    a2, _memmap_cacheattr_reset
	/* NOTE: CLOBBERS a2 - a8 !!! */
	cacheattr_set
#if XCHAL_USE_MEMCTL
	bnez a9, l1_cache_init_program_memctl_one_way

l1_cache_init_program_memctl_all_ways:
	movi a3, ((~MEMCTL_SNOOP_EN))
	j l1_cache_init_program_memctl

l1_cache_init_program_memctl_one_way:
	movi a3, ((1 << 8) | (1 << 13) | (1 << 18) | MEMCTL_L0IBUF_EN | 1 << 23)

l1_cache_init_program_memctl:
	wsr a3, MEMCTL
	rsync
	/*  Enable zero-overhead loop instr buffer, and snoop responses, if configured.  */
	movi    a3, (MEMCTL_SNOOP_EN | MEMCTL_L0IBUF_EN)
	rsr a2, MEMCTL
	or  a2, a2, a3
	wsr a2, MEMCTL
	rsync
#endif
	ret

	.size _l1_cache_init, . - _l1_cache_init
