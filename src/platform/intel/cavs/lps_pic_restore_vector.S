/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#include <xtensa/coreasm.h>
#include <xtensa/cacheattrasm.h>
#include <cavs/lps_ctx.h>
#include <sof/lib/memory.h>

#define MEMCTL_INIT_BIT 23
#define MEMCTL_INIT_VAL (MEMCTL_L0IBUF_EN | 1 << MEMCTL_INIT_BIT)
#define SW_INT_NUM 7
#define SW_INT_MASK (1<<SW_INT_NUM)

	.section .lps_vector, "ax"
lps_pic_restore_vector_literals:
	.literal_position
	.align 4

	.macro restore_vector	level
		l32i	a4, a3, lps_ctx_vector_level_&level
		wsr	a4, EXCSAVE+\level
	.endm

	.global lps_pic_restore_vector_literals
	.global lps_pic_restore_vector
	.global lps_pic_restore_vector_end

lps_pic_restore_vector:
	movi a0, 0
	movi a1, 1
	wsr  a1, WINDOWSTART
	wsr  a0, WINDOWBASE
	rsync

	movi a3, MEMCTL_INIT_VAL
	wsr a3, MEMCTL
	rsync

	movi    a3, 0x15	/* RCW for non-coherent dcache */
	wsr     a3, ATOMCTL

	movi a2, L1_CACHE_PREFCTL_VALUE
	wsr  a2, PREFCTL

	movi a3, PS_UM|PS_WOE
	wsr a3, PS
	rsync

	movi    a2, _memmap_cacheattr_reset
	cacheattr_set

	movi a3, (~MEMCTL_SNOOP_EN)
	wsr a3, MEMCTL
	rsync

	movi    a3, (MEMCTL_SNOOP_EN | MEMCTL_L0IBUF_EN)
	rsr a2, MEMCTL
	or  a2, a2, a3
	wsr a2, MEMCTL
	rsync

	/* setup the stack */
	movi sp, lps_boot_stack
	movi a2, 0x1000
	add sp, sp, a2

	/* Restore things saved by the platform code before sleep */

	movi a3, lps_restore

	l32i a2, a3, lps_ctx_threadptr
	wur a2, THREADPTR

	l32i a4, a3, lps_ctx_memmap_vecbase_reset
	wsr a4, vecbase
	rsync

	restore_vector 2
	restore_vector 3
	restore_vector 4
	restore_vector 5

	/* generate sw interrupt (# 7)*/
	movi a3, SW_INT_MASK
	rsr a4, intenable
	or a4, a4, a3
	wsr a4, intenable
	rsync
	wsr a3, interrupt
	rsync

loop:
    j loop
    /* should never go here */
_HaltLoop:
    j _HaltLoop

    /* alignment is needed to ensure that vector
     * will be copied with proper alignment
     */
    .align 64
lps_pic_restore_vector_end:

    .size   lps_pic_restore_vector, . - lps_pic_restore_vector
