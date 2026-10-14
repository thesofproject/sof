/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>
 */

/**
 * \file include/kernel/mailbox.h
 * \brief SRAM Window 0 firmware status registers and telemetry offset definitions.
 * \ingroup telemetry_perf_api
 */

#ifndef __KERNEL_MAILBOX_H__
#define __KERNEL_MAILBOX_H__

#if CONFIG_IPC_MAJOR_4
#include <ipc4/fw_reg.h>
#endif

/** \addtogroup telemetry_perf_api
 *  Common SRAM Window 0 firmware status and telemetry register offsets.
 *  @{
 */
#if CONFIG_IPC_MAJOR_4
#define SRAM_REG_FW_STATUS               offsetof(struct ipc4_fw_registers, fsr)	/**< Offset to Firmware Status Register (FSR) */
#define SRAM_REG_LAST_ERROR_CODE         offsetof(struct ipc4_fw_registers, lec)	/**< Offset to Last Error Code (LEC) */
#define SRAM_REG_FW_PWR_STATUS           offsetof(struct ipc4_fw_registers, fps)	/**< Offset to Firmware Power Status (FPS) */
#define SRAM_REG_LNEC                    offsetof(struct ipc4_fw_registers, lnec)	/**< Offset to Last Native Error Code (LNEC) */
#define SRAM_REG_LTR                     offsetof(struct ipc4_fw_registers, ltr)	/**< Offset to Latency Tolerance Reporting (LTR) */
#define SRAM_REG_ROM_INFO                offsetof(struct ipc4_fw_registers, rom_info)	/**< Offset to ROM bootstrap information */
#define SRAM_REG_ABI_VER                 offsetof(struct ipc4_fw_registers, abi_ver)	/**< Offset to ABI version field */
#define SRAM_REG_SLAVE_CORE_STS          offsetof(struct ipc4_fw_registers, slave_core_sts) /**< Offset to Secondary core power/run status */
#define SRAM_REG_PIPELINE_REGS           offsetof(struct ipc4_fw_registers, pipeline_regs) /**< Offset to pipeline state register array */
#define SRAM_REG_PEAK_VOL_REGS           offsetof(struct ipc4_fw_registers, peak_vol_regs) /**< Offset to peak volume meter register array */
#define SRAM_REG_LLP_GPDMA_READING_SLOTS offsetof(struct ipc4_fw_registers, llp_gpdma_reading_slots) /**< Offset to GPDMA linear link position reading slots */
#define SRAM_REG_LLP_SNDW_READING_SLOTS  offsetof(struct ipc4_fw_registers, llp_sndw_reading_slots)  /**< Offset to SoundWire link position reading slots */
#define SRAM_REG_LLP_EVAD_SLOTS          offsetof(struct ipc4_fw_registers, llp_evad_reading_slot)   /**< Offset to EVAD position reading slot */
#define SRAM_REG_FW_END                  sizeof(struct ipc4_fw_registers)		/**< Total size boundary of Window 0 firmware register block */
#else /* CONFIG_IPC_MAJOR_4 */
#define SRAM_REG_ROM_STATUS			0x0	/**< ROM bootstrap status */
#define SRAM_REG_FW_STATUS			0x4	/**< Firmware run status */
#define SRAM_REG_FW_TRACEP			0x8	/**< Primary core trace buffer pointer */
#define SRAM_REG_FW_IPC_RECEIVED_COUNT		0xc	/**< Total IPC messages received from host */
#define SRAM_REG_FW_IPC_PROCESSED_COUNT		0x10	/**< Total IPC messages dispatched */
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_BASE	0x14	/**< Base offset for secondary core trace pointers */
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_1		SRAM_REG_FW_TRACEP_SECONDARY_CORE_BASE	/**< Core 1 trace pointer */
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_2		0x18	/**< Core 2 trace pointer */
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_3		0x1C	/**< Core 3 trace pointer */
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_4		0x20	/**< Core 4 trace pointer */
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_5		0x24	/**< Core 5 trace pointer */
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_6		0x28	/**< Core 6 trace pointer */
#define SRAM_REG_FW_TRACEP_SECONDARY_CORE_7		0x2C	/**< Core 7 trace pointer */
#define SRAM_REG_R_STATE_TRACE_BASE		0x30	/**< Base offset for power state trace */
#define SRAM_REG_R0_STATE_TRACE			SRAM_REG_R_STATE_TRACE_BASE	/**< Core 0 power state trace */
#define SRAM_REG_R1_STATE_TRACE			0x38	/**< Core 1 power state trace */
#define SRAM_REG_R2_STATE_TRACE			0x40	/**< Core 2 power state trace */
#define SRAM_REG_FW_END				(SRAM_REG_R2_STATE_TRACE + 0x8)	/**< End of register block */
#endif

/** @}*/

#endif /* __KERNEL_MAILBOX_H__ */
