/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file platform/intel/cavs/lib/power_down.S
 * \brief Power gating memory banks - implementation specific for platforms
 *  with cAVS 1.8 (i.e. CannonLake) and cAVS 2.0 (i.e. IceLake)
  * \author Lech Betlej <lech.betlej@linux.intel.com>
 */


#include <cavs/version.h>
#include <cavs/drivers/sideband-ipc.h>
#include <cavs/lib/asm_ldo_management.h>
#include <cavs/lib/asm_memory_management.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>

#if CAVS_VERSION >= CAVS_VERSION_1_8

	.section .text, "ax"
	.align 64
power_down_literals:
	.literal_position
#if CONFIG_IPC_MAJOR_3
ipc_flag:
	.word IPC_DIPCTDR_BUSY
#elif CONFIG_IPC_MAJOR_4
set_dx_reply:
	/* BUSY (bit31), MODULE_MSG (bit30), reply (bit29), SET_DX (bit 24-28: 7) */
	.word 0xE7000000

#endif
sram_dis_loop_cnt:
	.word 4096

	.global power_down
	.type power_down, @function

/**
 * Perform power down.
 *
 * Depending on arguments, memories are switched off.
 * A2 - argument for LPSRAM
 * A3 - pointer to array containing power gating mask.
 *Size of array is determined by MEMORY_SEGMENTS define.
 * A4 - platform type
 * A5 - response_to_ipc
 */

#define b_enable_lpsram              a2
#define pu32_hpsram_mask             a3
#define temp_reg0                    a6
#define temp_reg1                    a7
#define temp_reg2                    a8
#define temp_reg3                    a9
#define host_base		     a10
#define pfl_reg                      a15

power_down:
	entry sp, 32
	// effectively executes:
	// xthal_dcache_region_lock(&literals, 128);
	// xthal_dcache_region_lock(&powerdown, 256);
	// xthal_dcache_region_lock(&pu32_hpsram_mask, 64);
	movi pfl_reg, power_down_literals
	dpfl pfl_reg, 0
	dpfl pfl_reg, 64

	movi pfl_reg, power_down
	ipfl pfl_reg, 0
	ipfl pfl_reg, 64
	ipfl pfl_reg, 128
	ipfl pfl_reg, 192

	mov  pfl_reg, pu32_hpsram_mask
	dpfl pfl_reg, 0

_PD_DISABLE_LPSRAM:
/* effectively executes:
 * if (b_enable_lpsram){
 *  cavs_lpsram_power_down_entire();
 * }
 */
	movi host_base, IPC_HOST_BASE

	beqz b_enable_lpsram, _PD_DISABLE_HPSRAM
	m_cavs_lpsram_power_down_entire temp_reg0, temp_reg1, temp_reg2,\
					sram_dis_loop_cnt
	j _PD_DISABLE_HPSRAM

_PD_DISABLE_HPSRAM:
 /* if value in memory pointed by pu32_hpsram_mask = 0
	 (hpsram_pwrgating_mask) - do not disable hpsram. */
	beqz pu32_hpsram_mask, _PD_SEND_IPC

/* mandatory sequence for LDO ON - effectively executes:
 * m_cavs_s_set_ldo_hpsram_on_state();
 * WAIT_300NS();
 */
	movi temp_reg0, SHIM_LDOCTL_HPSRAM_LDO_ON
	m_cavs_set_hpldo_state temp_reg0, temp_reg1, temp_reg2
	movi temp_reg0, 128
1 :
	addi temp_reg0, temp_reg0, -1
	bnez temp_reg0, 1b


/* effectively executes:
 * for (size_t seg_index = (MAX_MEMORY_SEGMENTS - 1); seg_index >= 0;
 * --seg_index) {
 * cavs_hpsram_power_change(seg_index, mask[seg_index]);
 * }
 * where mask is given in pu32_hpsram_mask register
 */

	.set seg_index, MAX_MEMORY_SEGMENTS - 1
	.rept MAX_MEMORY_SEGMENTS
	l32i temp_reg0, pu32_hpsram_mask, 4 * seg_index
	m_cavs_hpsram_power_change\
	/*segment_index=*/	seg_index,\
	/*mask=*/	temp_reg0,\
	temp_reg1,\
	temp_reg2,\
	temp_reg3
	.set seg_index, seg_index - 1
	.endr


/* mandatory sequence for LDO OFF - effectively executes:
 * WAIT_300NS();
 * m_cavs_set_ldo_hpsram_on_state()
 */
	movi temp_reg0, 128
1 :
	addi temp_reg0, temp_reg0, -1
	bnez temp_reg0, 1b

	movi temp_reg0, SHIM_LDOCTL_HPSRAM_LDO_OFF
	m_cavs_set_hpldo_state temp_reg0, temp_reg1, temp_reg2

_PD_SEND_IPC:
#if CONFIG_IPC_MAJOR_3
/* Send IPC to host informing of PD completion - Clear BUSY
 * bit by writing IPC_DIPCTDR_BUSY to IPC_DIPCTDR
 * and writing IPC_DIPCTDA_DONE to IPC_DIPCTDA
 */
	l32i temp_reg1, host_base, IPC_DIPCTDR
	movi temp_reg2, ipc_flag
	l32i temp_reg2, temp_reg2, 0
	or temp_reg1, temp_reg1, temp_reg2
	s32i temp_reg1, host_base, IPC_DIPCTDR

	l32i temp_reg1, host_base, IPC_DIPCTDA
	or temp_reg1, temp_reg1, temp_reg2
	s32i temp_reg1, host_base, IPC_DIPCTDA
#elif CONFIG_IPC_MAJOR_4
/* Send IPC reply for SET_DX message */
	movi temp_reg1, 0
	s32i temp_reg1, host_base, IPC_DIPCIDD

	movi temp_reg1, set_dx_reply
	l32i temp_reg1, temp_reg1, 0
	s32i temp_reg1, host_base, IPC_DIPCIDR
#else
#error "Support for this IPC version is not implemented"
#endif

_PD_SLEEP:
/* effecfively executes:
 * xmp_spin()
 * waiti 5
 */
	movi temp_reg0, 128
loop:
	addi temp_reg0, temp_reg0, -1
	bnez temp_reg0, loop

    extw
    extw
    waiti 5
    1:
    j 1b

.size power_down , . - power_down

#endif /* CAVS_VERSION >= CAVS_VERSION_1_8 */

