/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>
 */

/**
  * \file include/kernel/mailbox.h
  * \brief FW Regs API definition
  * \author Adrian Bonislawski <adrian.bonislawski@linux.intel.com>
  */

#ifndef __KERNEL_MAILBOX_H__
#define __KERNEL_MAILBOX_H__

/** \addtogroup fw_regs_api FW Regs API
 *  This is a common SRAM window 0 FW "registers" layout
 *  used in the platfrom-defined SRAM window 0 region
 *  @{
 */
#define SRAM_REG_ROM_STATUS			0x0
#define SRAM_REG_FW_STATUS			0x4
#define SRAM_REG_FW_TRACEP			0x8
#define SRAM_REG_FW_IPC_RECEIVED_COUNT		0xc
#define SRAM_REG_FW_IPC_PROCESSED_COUNT		0x10
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_BASE	0x14
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_1		SRAM_REG_FW_TRACEP_SECONDARY_CORE_BASE
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_2		0x18
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_3		0x1C
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_4		0x20
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_5		0x24
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_6		0x28
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_7		0x2C
#define SRAM_REG_R_STATE_TRACE_BASE		0x30
#define SRAM_REG_R0_STATE_TRACE			SRAM_REG_R_STATE_TRACE_BASE
#define SRAM_REG_R1_STATE_TRACE			0x38
#define SRAM_REG_R2_STATE_TRACE			0x40
#define SRAM_FOO_START 0x48
#define SRAM_FOO_NUM_TASKS 0x4C
#define SRAM_FOO_TOTAL_TASKS 0x50
#define SRAM_FOO_CLIENTS 0x54
#define SRAM_FOO_REGISTERED 0x58
#define SRAM_FOO_ENABLED 0x5C
#define SRAM_FOO_LAST_TICK 0x60
#define SRAM_FOO_CLK 0x68
#define SRAM_FOO_LINE 0x6C
#define SRAM_FOO_CUR_TIME 0x70
#define SRAM_FOO_REQ_TIME 0x78
#define SRAM_FOO_LL_TASK_SCHED 0x80
#define SRAM_FOO_LL_TASK_STATE 0x84
#define SRAM_FOO_LL_PENDING 0x88
#define SRAM_FOO_LL_SET_FAIL 0x8C
#define SRAM_FOO_LL_CANCEL 0x90
#define SRAM_FOO_LL_RECALC 0x94
#define SRAM_FOO_LL_TASK_LINE 0x98
#define SRAM_REG_FW_END				(SRAM_FOO_LL_TASK_LINE + 0x4)

/** @}*/

#endif /* __KERNEL_MAILBOX_H__ */
