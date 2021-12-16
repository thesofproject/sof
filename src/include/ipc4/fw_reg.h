/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/**
 * \file include/ipc4/fw_reg.h
 * \brief IPC4 fw registers in mailbox for host
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __IPC4_FW_REG_H__
#define __IPC4_FW_REG_H__

#include <stdint.h>
#include <ipc4/module.h>
#include <platform/lib/cpu.h>
#include <platform/lib/dma.h>

struct ipc4_fw_status_reg {
	uint32_t state      : 28;
	/* Last module ID updated FSR */
	uint32_t module     : 3;
	/* State of DSP core */
	uint32_t running    : 1;
} __attribute__((packed, aligned(4)));

typedef uint32_t ipc4_last_error;

struct ipc4_fw_pwr_status {
	/* currently set astate */
	uint32_t curr_astate : 4;
	/* cached IMR usage status for previous D0i3 */
	uint32_t cached_imr_usage_status  : 1;
	uint32_t curr_fstate : 3;
	uint32_t wake_tick_period : 5;
	uint32_t active_pipelines_count : 6;
	uint32_t rsvd0 : 13;
} __attribute__((packed, aligned(4)));

struct ipc4_pipeline_registers {
	/**
	 * Stream start offset (LPIB) reported by mixin module allocated on pipeline attached
	 * to Host Output Gateway when first data is being mixed to mixout module. When data
	 * is not mixed (right after creation/after reset) value "(uint64_t)-1" is reported.
	 * In number of bytes.
	 * */
	uint64_t stream_start_offset;
	/**
	 * Stream end offset (LPIB) reported by mixin module allocated on pipeline attached
	 * to Host Output Gateway during transition from RUNNING to PAUSED. When data
	 * is not mixed (right after creation/after reset) value "(uint64_t)-1" is reported. When
	 * first data is mixed then value "0"is reported.
	 * In number of bytes.
	 * */
	uint64_t stream_end_offset;
} __attribute__((packed, aligned(8)));

#define IPC4_PV_MAX_SUPPORTED_CHANNELS 8
struct ipc4_peak_volume_regs {
	uint32_t peak_meter_[IPC4_PV_MAX_SUPPORTED_CHANNELS];
	uint32_t current_volume_[IPC4_PV_MAX_SUPPORTED_CHANNELS];
	uint32_t target_volume_[IPC4_PV_MAX_SUPPORTED_CHANNELS];
} __attribute__((packed, aligned(4)));

struct ipc4_llp_reading {
	/* lower part of 64-bit LLP */
	uint32_t llp_l;
	/* upper part of 64-bit LLP */
	uint32_t llp_u;
	/* lower part of 64-bit Wallclock */
	uint32_t wclk_l;
	/* upper part of 64-bit Wallclock */
	uint32_t wclk_u;
} __attribute__((packed, aligned(4)));

struct ipc4_llp_reading_extended {
	struct ipc4_llp_reading llp_reading;
	/* total processed data (low part) */
	uint32_t tpd_low;
	/* total processed data (high part) */
	uint32_t tpd_high;
} __attribute__((packed, aligned(4)));

struct ipc4_llp_reading_slot {
	uint32_t node_id;
	struct ipc4_llp_reading reading;
} __attribute__((packed, aligned(4)));

union ipc4_rom_info {
	struct {
		uint32_t fuse_values: 8;
		uint32_t load_method: 1;
		uint32_t downlink_IPC_use_DMA: 1;
		uint32_t load_method_reserved: 2;
		uint32_t implementation_revision_min: 4;
		uint32_t implementation_revision_maj: 4;
		uint32_t implementation_version_min: 4;
		uint32_t implementation_version_maj: 4;
		uint32_t reserved : 4;
	} bits;
	struct {
		uint32_t rsvd1: 16;
		uint32_t type: 12;
		uint32_t rsvd2: 4;
	} platform;
} __attribute__((packed, aligned(4)));

struct ipc4_fw_registers {
	struct ipc4_fw_status_reg fsr;
	ipc4_last_error lec;
	struct ipc4_fw_pwr_status fps;
	uint32_t mem_status;
	uint32_t ltr;
	uint32_t rsvd0;
	union ipc4_rom_info rom_info;
	uint32_t rsvd1[5];
	uint32_t dbg_log_wp[CONFIG_MAX_CORE_COUNT];
	uint32_t rsvd2[4 - CONFIG_MAX_CORE_COUNT];

	struct ipc4_pipeline_registers pipeline_regs[PLATFORM_MAX_DMA_CHAN];
	struct ipc4_peak_volume_regs peak_vol_regs[10];
	struct ipc4_llp_reading_slot llp_gpdma_reading_slots[MAX_GPDMA_COUNT * 8];
	struct ipc4_llp_reading_slot llp_sndw_reading_slots[MAX_GPDMA_COUNT * 8];
	uint32_t memory_window_2_slot_desc[IPC4_MAX_SUPPORTED_LIBRARIES];
} __attribute__((packed, aligned(4)));

#endif
