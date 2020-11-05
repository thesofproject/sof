/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file platform/apollolake/lib/power_down.S
 * \brief Power gating memory banks - implementation specific for Apollolake
 * \author Lech Betlej <lech.betlej@linux.intel.com>
 */
#include <platform/lib/asm_ldo_management.h>
#include <platform/lib/asm_memory_management.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>

    .section .text, "ax"
    .align 64
literals:
    .literal_position
    .global power_down
    .type power_down, @function
/**
 * Perform power down.
 *
 * Depending on arguments, memories are switched off.
 * A2 - argument for LPSRAM
 * A3 - pointer to array containing power gating mask.
 *
 * Finally, core enters waiti.
 */

#define b_enable_lpsram              a2
#define pu32_hpsram_mask             a3
#define temp_reg0                    a6
#define temp_reg1                    a7
#define temp_reg2                    a8
#define temp_reg3                    a9
#define host_base                    a10
#define pfl_reg                      a15

    .align 64
power_down:
    entry sp, 32
    // effectively executes:
    // xthal_dcache_region_lock(&literals, 128);
    // xthal_dcache_region_lock(&power_down, 384);
    // xthal_dcache_region_lock(&pu32_hpsram_mask, 64);
    movi pfl_reg, literals
    dpfl pfl_reg, 0
    dpfl pfl_reg, 64

    movi pfl_reg, power_down
    ipfl pfl_reg, 0
    ipfl pfl_reg, 64
    ipfl pfl_reg, 128
    ipfl pfl_reg, 192
    addi pfl_reg, pfl_reg, 256
    ipfl pfl_reg, 0
    ipfl pfl_reg, 64

    mov pfl_reg, pu32_hpsram_mask
    dpfl pfl_reg, 0

    // if b_enable_lpsram = 0 (bool disable_lpsram) - do not disable lpsram.
    beqz b_enable_lpsram, _PD_DISABLE_HPSRAM

_PD_DISABLE_LPSRAM:
    movi host_base, IPC_HOST_BASE

    movi temp_reg0, SHIM_LDOCTL_LPSRAM_LDO_ON
    m_cavs_set_lpldo_state temp_reg0, temp_reg1, temp_reg2

		m_cavs_lpsram_power_off temp_reg0, temp_reg1, temp_reg2

    movi temp_reg0, SHIM_LDOCTL_LPSRAM_LDO_OFF
    m_cavs_set_lpldo_state temp_reg0, temp_reg1, temp_reg2

    // DISABLE_HPSRAM is aligned so there can be zeros between
    //it and last instr.
    j _PD_DISABLE_HPSRAM

	// workaround for incidental gnu assembler bug - no alignment here
	// (see comment before IPFL) ...
	// .align 64
_PD_DISABLE_HPSRAM:
    // 	if value in memory pointed by pu32_hpsram_mask = 0
    //	(hpsram_pwrgating_mask) - do not disable hpsram.
    l32i temp_reg0, pu32_hpsram_mask, 0
    beqz temp_reg0, _PD_SLEEP

    movi temp_reg0, SHIM_LDOCTL_HPSRAM_LDO_ON
    m_cavs_set_hpldo_state temp_reg0, temp_reg1, temp_reg2
    // Disable L2 cache in case it would be enabled

    m_cavs_hpsram_power_off temp_reg0, temp_reg1, temp_reg2

    movi temp_reg0, SHIM_LDOCTL_HPSRAM_LDO_OFF
    m_cavs_set_hpldo_state temp_reg0, temp_reg1, temp_reg2

	// 	For BXT-P we need to deassert VNN request and select slow XTAL
	//	as clock source
	// 	APL specific code _PD_SWITCH_TO_XTAL_CLOCK: and _PD_RELEASE_VNN
_PD_SWITCH_TO_XTAL_CLOCK:
    // TODO: move to CLOCK hal macros
    movi temp_reg0, (SHIM_BASE + SHIM_CLKCTL)
    movi temp_reg1, ~(SHIM_CLKCTL_HDOCS | SHIM_CLKCTL_LDOCS)
    movi temp_reg2, (SHIM_CLKCTL_LDCS_XTAL | SHIM_CLKCTL_HDCS_XTAL)
    l32i temp_reg3, temp_reg0, 0
    // Reset LDOCS & HDOCS bits to select XTAL
    and temp_reg3, temp_reg3, temp_reg1
    // Set LDCS & HDCS so clock selection depends on LDOCS & HDOCS
    or  temp_reg3, temp_reg3, temp_reg2
    s32i temp_reg3, temp_reg0, 0

_PD_RELEASE_VNN:
    // TODO: move to VNN/SHIM hal macros
    movi temp_reg0, (SHIM_BASE + SHIM_SPSREQ)
    movi temp_reg1, ~SHIM_SPSREQ_RVNNP
    l32i temp_reg2, temp_reg0, 0
    and  temp_reg2, temp_reg2, temp_reg1
    s32i temp_reg2, temp_reg0, 0
    l32i temp_reg2, temp_reg0, 0
    // We cannot wait for VNN to drop since it can be held by something else
    // and never drop

_PD_SEND_IPC:
	// Send IPC to host informing of PD completion - Clear BUSY bit by
	// writting IPC_DIPCT_BUSY to IPC_DIPCT
	l32i temp_reg1, host_base, IPC_DIPCT
	movi temp_reg2, IPC_DIPCT_BUSY
	or temp_reg1, temp_reg1, temp_reg2
	s32i temp_reg1, host_base, IPC_DIPCT

_PD_SLEEP:
    // effecfively executes:
    // xmp_spin()
    // waiti 5
    movi temp_reg0, 128
loop:
    addi temp_reg0, temp_reg0, -1
    bnez temp_reg0, loop

    extw
    extw
    waiti 5
    j _PD_SLEEP

  .size  power_down , . - power_down


