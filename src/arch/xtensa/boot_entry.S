/*
 * Copyright (c) 2016, Intel Corporation
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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/*
 * Entry point from ROM - assumes :-
 *
 * 1) C runtime environment is initalised by ROM.
 * 2) Stack is in first HPSRAM bank.
 */

#include <config.h>
#include <platform/shim.h>
#include <platform/platcfg.h>
#include <xtensa/corebits.h>
#include <xtensa/config/core-isa.h>

	.type   boot_master_core, @function

	.begin	literal_prefix	.boot_entry
	.section .boot_entry.text, "ax"

	.align	4
	.global	boot_entry

boot_entry:
	entry	a1, 48
	j boot_init

	.align 4
	.literal_position
#if defined(PLATFORM_RESET_MHE_AT_BOOT)
l2_mecs:
	.word SHIM_L2_MECS
#endif

#if defined(PLATFORM_DISABLE_L2CACHE_AT_BOOT)
l2_cache_pref:
	.word SHIM_L2_PREF_CFG
#endif

sof_stack_base:
	.word SOF_STACK_BASE

#if !defined CONFIG_SUECREEK
wnd0_base:
	.word DMWBA(0)

wnd0_size:
	.word DMWLO(0)

wnd0_base_val:
	.word HP_SRAM_WIN0_BASE | DMWBA_READONLY | DMWBA_ENABLE

wnd0_size_val:
	.word HP_SRAM_WIN0_SIZE | 0x7

wnd0_status_address:
	.word HP_SRAM_WIN0_BASE

wnd0_error_address:
	.word HP_SRAM_WIN0_BASE | 0x4
#endif

fw_loaded_status_value:
	.word	0x00000005

fw_no_errors_value:
	.word	0x00000000

boot_init:
	.align 4
#if defined(PLATFORM_DISABLE_L2CACHE_AT_BOOT)
	l32r a3, l2_cache_pref
	movi a5, 0
	s32i a5, a3, 0
	memw
#endif

#if defined(PLATFORM_RESET_MHE_AT_BOOT)
	/* reset memory hole */
	l32r a3, l2_mecs
	movi a5, 0
	s32i a5, a3, 0
#endif

	/* reprogram stack to the area defined by main FW */
	l32r	a3, sof_stack_base
	mov		sp, a3

#if !defined CONFIG_SUECREEK
	/* set status register to 0x00000005 in wnd0 */
	l32r	a3,	fw_loaded_status_value
	l32r	a5,	wnd0_status_address
	s32i	a3, a5, 0

	/* set error register to 0x00 in wnd0 */
	l32r	a3, fw_no_errors_value
	l32r	a5,	wnd0_error_address
	s32i	a3, a5, 0

	/* realloc memory window0 to
	continue reporting boot progress */
	l32r	a3, wnd0_size
	l32r	a5, wnd0_size_val
	s32i	a5, a3, 0
	memw
	l32r	a3, wnd0_base
	l32r	a5, wnd0_base_val
	s32i	a5, a3, 0
	memw
#endif

#if (XCHAL_DCACHE_IS_COHERENT || XCHAL_LOOP_BUFFER_SIZE) && \
	XCHAL_HW_MIN_VERSION >= XTENSA_HWVERSION_RE_2012_0
	/*	Enable zero-overhead loop instr buffer,
		and snoop responses, if configured.  */
	movi	a3, (MEMCTL_SNOOP_EN | MEMCTL_L0IBUF_EN)
	rsr a2, MEMCTL
	or	a2, a2, a3
	wsr a2, MEMCTL
#endif

	/* determine core we are running on */
	rsr.prid	a2
	movi		a3, PLATFORM_MASTER_CORE_ID
	beq			a2, a3, 1f

	/* no core should get here */
	j dead

1:
	/* we are primary core so boot it */
	call8 boot_master_core

dead:
	/* should never get here - we are dead */
	j dead

	.size	boot_entry, . - boot_entry

	.end	literal_prefix
