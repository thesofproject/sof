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
#include <xtensa/corebits.h>
#include <xtensa/config/core-isa-boot.h>

	.type   boot_pri_core, @function
	.type   boot_sec_core, @function

	.begin	literal_prefix	.boot_entry
	.section .boot_entry.text, "ax"

	.align	4
	.global	boot_entry

boot_entry:
	entry	a1, 48
	j boot_init

	.align 4
	.literal_position
l2_mecs:
	.word SHIM_L2_MECS

boot_init:
	.align 4
#if defined(CONFIG_CANNONLAKE)
	/* reset memory hole */
	l32r a3, l2_mecs
	movi a5, 0
	s32i a5, a3, 0
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
	rsr a2, PRID
	beqz a2, 1f

	/* we are seconadry core, so boot it */
	call8 boot_sec_core
	j dead

1:
	/* we are primary core so boot it */
	call8 boot_pri_core

dead:
	/* should never get here - we are dead */
	j dead

	.size	boot_entry, . - boot_entry

	.end	literal_prefix
