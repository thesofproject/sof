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
 * \file platform/apollolake/include/platform/asm_ldo_management.h
 * \brief Macros for controlling LDO state specific for cAVS 1.5
 * \author Lech Betlej <lech.betlej@linux.intel.com>
 */
#ifndef ASM_LDO_MANAGEMENT_H
#define ASM_LDO_MANAGEMENT_H

#ifndef ASSEMBLY
#warning "Header can only be used by assembly sources."
#endif

#include <platform/shim.h>

.macro m_cavs_set_ldo_state state, ax
movi \ax, (SHIM_BASE + SHIM_LDOCTL)
s32i \state, \ax, 0
memw
// wait loop > 300ns (min 100ns required)
movi \ax, 128
1 :
addi \ax, \ax, -1
nop
bnez \ax, 1b
.endm

.macro m_cavs_set_hpldo_state state, ax, ay
movi \ax, (SHIM_BASE + SHIM_LDOCTL)
l32i \ay, \ax, 0

movi \ax, ~(SHIM_LDOCTL_HP_SRAM_MASK)
and \ay, \ax, \ay
or \state, \ay, \state

m_cavs_set_ldo_state \state, \ax
.endm

.macro m_cavs_set_lpldo_state state, ax, ay
movi \ax, (SHIM_BASE + SHIM_LDOCTL)
l32i \ay, \ax, 0
// LP SRAM mask
movi \ax, ~(SHIM_LDOCTL_LP_SRAM_MASK)
and \ay, \ax, \ay
or \state, \ay, \state

m_cavs_set_ldo_state \state, \ax
.endm

.macro m_cavs_set_ldo_on_state ax, ay, az
movi \ay, (SHIM_BASE + SHIM_LDOCTL)
l32i \az, \ay, 0

movi \ax, ~(SHIM_LDOCTL_HP_SRAM_MASK | SHIM_LDOCTL_LP_SRAM_MASK)
and \az, \ax, \az
movi \ax, (SHIM_LDOCTL_HP_SRAM_LDO_ON | SHIM_LDOCTL_LP_SRAM_LDO_ON)
or \ax, \az, \ax

m_cavs_set_ldo_state \ax, \ay
.endm

.macro m_cavs_set_ldo_off_state ax, ay, az
// wait loop > 300ns (min 100ns required)
movi \ax, 128
1 :
		addi \ax, \ax, -1
		nop
		bnez \ax, 1b
movi \ay, (SHIM_BASE + SHIM_LDOCTL)
l32i \az, \ay, 0

movi \ax, ~(SHIM_LDOCTL_HP_SRAM_MASK | SHIM_LDOCTL_LP_SRAM_MASK)
and \az, \az, \ax

movi \ax, (SHIM_LDOCTL_HP_SRAM_LDO_OFF | SHIM_LDOCTL_LP_SRAM_LDO_OFF)
or \ax, \ax, \az

s32i \ax, \ay, 0
l32i \ax, \ay, 0
.endm

.macro m_cavs_set_ldo_bypass_state ax, ay, az
// wait loop > 300ns (min 100ns required)
movi \ax, 128
1 :
		addi \ax, \ax, -1
		nop
		bnez \ax, 1b
movi \ay, (SHIM_BASE + SHIM_LDOCTL)
l32i \az, \ay, 0

movi \ax, ~(SHIM_LDOCTL_HP_SRAM_MASK | SHIM_LDOCTL_LP_SRAM_MASK)
and \az, \az, \ax

movi \ax, (SHIM_LDOCTL_HP_SRAM_LDO_BYPASS | SHIM_LDOCTL_LP_SRAM_LDO_BYPASS)
or \ax, \ax, \az

s32i \ax, \ay, 0
l32i \ax, \ay, 0
.endm

#endif /* ASM_LDO_MANAGEMENT_H */
