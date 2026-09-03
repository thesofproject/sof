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

#if CONFIG_IPC_MAJOR_4
#include <ipc4/fw_reg.h>
#endif

/** \addtogroup fw_regs_api FW Regs API
 *  This is a common SRAM window 0 FW "registers" layout
 *  used in the platfrom-defined SRAM window 0 region
 *  @{
 */
#if CONFIG_IPC_MAJOR_4
#define SRAM_REG_FW_STATUS               offsetof(struct ipc4_fw_registers, fsr)
#define SRAM_REG_LAST_ERROR_CODE         offsetof(struct ipc4_fw_registers, lec)
#define SRAM_REG_FW_PWR_STATUS           offsetof(struct ipc4_fw_registers, fps)
#define SRAM_REG_LNEC                    offsetof(struct ipc4_fw_registers, lnec)
#define SRAM_REG_LTR                     offsetof(struct ipc4_fw_registers, ltr)
#define SRAM_REG_ROM_INFO                offsetof(struct ipc4_fw_registers, rom_info)
#define SRAM_REG_ABI_VER                 offsetof(struct ipc4_fw_registers, abi_ver)
#define SRAM_REG_SLAVE_CORE_STS          offsetof(struct ipc4_fw_registers, slave_core_sts)
#define SRAM_REG_PIPELINE_REGS           offsetof(struct ipc4_fw_registers, pipeline_regs)
#define SRAM_REG_PEAK_VOL_REGS           offsetof(struct ipc4_fw_registers, peak_vol_regs)
#define SRAM_REG_LLP_GPDMA_READING_SLOTS offsetof(struct ipc4_fw_registers, llp_gpdma_reading_slots)
#define SRAM_REG_LLP_SNDW_READING_SLOTS  offsetof(struct ipc4_fw_registers, llp_sndw_reading_slots)
#define SRAM_REG_LLP_EVAD_SLOTS          offsetof(struct ipc4_fw_registers, llp_evad_reading_slot)
#define SRAM_REG_FW_END                  sizeof(struct ipc4_fw_registers)
#else /* CONFIG_IPC_MAJOR_4 */
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
#endif

/** @}*/

#endif /* __KERNEL_MAILBOX_H__ */
