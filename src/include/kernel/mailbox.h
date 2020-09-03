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
#define SRAM_REG_FW_END				(SRAM_REG_R2_STATE_TRACE + 0x8)

/** @}*/

#endif /* __KERNEL_MAILBOX_H__ */
