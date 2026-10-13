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
 * \brief IPC4 firmware registers in mailbox window 0 for host telemetry and status.
 * \ingroup ipc4_notification_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __IPC4_FW_REG_H__
#define __IPC4_FW_REG_H__

#include <stdint.h>
#include <ipc4/error_status.h>
#include <ipc4/module.h>
#include <sof/lib/cpu.h>

/**
 * \brief Firmware Status Register (FSR) located at offset 0x0 in Mailbox Window 0.
 * \ingroup ipc4_notification_api
 */
struct ipc4_fw_status_reg {
	uint32_t state      : 28; /**< Current firmware execution / boot state. */
	uint32_t module     : 3;  /**< Last module ID that updated FSR. */
	uint32_t running    : 1;  /**< DSP core active execution flag (1 = running). */
} __attribute__((packed, aligned(4)));

/**
 * \brief Last error code reported by ROM or Base FW.
 * \ingroup ipc4_notification_api
 */
typedef uint32_t ipc4_last_error;

/**
 * \brief Firmware power and clock status register (offset 0x8 in Mailbox Window 0).
 * \ingroup ipc4_notification_api
 */
struct ipc4_fw_pwr_status {
	uint32_t curr_astate : 4;             /**< Currently active A-state index. */
	uint32_t cached_imr_usage_status  : 1; /**< Cached IMR usage status from previous D0i3. */
	uint32_t curr_fstate : 3;             /**< Currently active F-state index. */
	uint32_t wake_tick_period : 5;        /**< Wake tick period configuration. */
	uint32_t active_pipelines_count : 6;  /**< Number of currently active pipelines. */
	uint32_t rsvd0 : 13;                  /**< Reserved padding bits. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Pipeline DMA tracking registers in Mailbox Window 0.
 * \ingroup ipc4_notification_api
 */
struct ipc4_pipeline_registers {
	/**
	 * Stream start offset (LPIB) reported by mixin module allocated on pipeline attached
	 * to Host Output Gateway when first data is being mixed to mixout module. When data
	 * is not mixed (right after creation/after reset) value "(uint64_t)-1" is reported.
	 * In number of bytes.
	 */
	uint64_t stream_start_offset;
	/**
	 * Stream end offset (LPIB) reported by mixin module allocated on pipeline attached
	 * to Host Output Gateway during transition from RUNNING to PAUSED. When data
	 * is not mixed (right after creation/after reset) value "(uint64_t)-1" is reported. When
	 * first data is mixed then value "0" is reported.
	 * In number of bytes.
	 */
	uint64_t stream_end_offset;
} __attribute__((packed, aligned(8)));

/** \brief Maximum audio channels supported in PeakVol telemetry registers. */
#define IPC4_PV_MAX_SUPPORTED_CHANNELS 8

/**
 * \brief Peak volume and meter registers for low-latency host inspection.
 * \ingroup ipc4_notification_api
 */
struct ipc4_peak_volume_regs {
	uint32_t peak_meter[IPC4_PV_MAX_SUPPORTED_CHANNELS];     /**< Real-time per-channel peak meter values. */
	uint32_t current_volume[IPC4_PV_MAX_SUPPORTED_CHANNELS]; /**< Current attenuated volume levels. */
	uint32_t target_volume[IPC4_PV_MAX_SUPPORTED_CHANNELS];  /**< Target ramping volume levels. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Link Linear Position (LLP) and wallclock snapshot.
 * \ingroup ipc4_notification_api
 */
struct ipc4_llp_reading {
	uint32_t llp_l;  /**< Lower 32 bits of 64-bit Link Linear Position. */
	uint32_t llp_u;  /**< Upper 32 bits of 64-bit Link Linear Position. */
	uint32_t wclk_l; /**< Lower 32 bits of 64-bit DSP wallclock. */
	uint32_t wclk_u; /**< Upper 32 bits of 64-bit DSP wallclock. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Extended Link Linear Position reading with total processed data bytes.
 * \ingroup ipc4_notification_api
 */
struct ipc4_llp_reading_extended {
	struct ipc4_llp_reading llp_reading; /**< Base LLP and wallclock reading. */
	uint32_t tpd_low;                    /**< Lower 32 bits of total processed data bytes. */
	uint32_t tpd_high;                   /**< Upper 32 bits of total processed data bytes. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Mailbox Window 0 reading slot for a single gateway.
 * \ingroup ipc4_notification_api
 */
struct ipc4_llp_reading_slot {
	uint32_t node_id;                 /**< Audio gateway connector node ID. */
	struct ipc4_llp_reading reading;  /**< Corresponding LLP position reading. */
} __attribute__((packed, aligned(4)));

/**
 * \brief ROM build, version, and load capability information.
 * \ingroup ipc4_notification_api
 */
union ipc4_rom_info {
	struct {
		uint32_t fuse_values: 8;                 /**< Hardware fuse configuration values. */
		uint32_t load_method: 1;                 /**< Image loading method (DMA or memory). */
		uint32_t downlink_IPC_use_DMA: 1;        /**< 1 if downlink IPC uses DMA buffer. */
		uint32_t load_method_reserved: 2;        /**< Reserved load bits. */
		uint32_t implementation_revision_min: 4; /**< Minor implementation revision. */
		uint32_t implementation_revision_maj: 4; /**< Major implementation revision. */
		uint32_t implementation_version_min: 4;  /**< Minor implementation version. */
		uint32_t implementation_version_maj: 4;  /**< Major implementation version. */
		uint32_t reserved : 4;                   /**< Reserved padding bits. */
	} bits; /**< Bitfield layout. */
	struct {
		uint32_t rsvd1: 16; /**< Reserved bits. */
		uint32_t type: 12;  /**< Platform architecture type identifier. */
		uint32_t rsvd2: 4;  /**< Reserved bits. */
	} platform; /**< Platform interpretation. */
} __attribute__((packed, aligned(4)));

/** \brief Maximum number of DSP cores supported in firmware register table. */
#define IPC4_MAX_SUPPORTED_ADSP_CORES 8

/** \brief Number of pipeline register slots in firmware register table. */
#define IPC4_MAX_PIPELINE_REG_SLOTS 16

/** \brief Number of PeakVol register slots in firmware register table. */
#define IPC4_MAX_PEAK_VOL_REG_SLOTS 16

/** \brief Number of GPDMA LLP reading slots in firmware register table. */
#define IPC4_MAX_LLP_GPDMA_READING_SLOTS 24

/** \brief Number of Aggregated SoundWire reading slots in firmware register table. */
#define IPC4_MAX_LLP_SNDW_READING_SLOTS 16

/** \brief Current ABI version of the Mailbox Window 0 FwRegisters layout. */
#define IPC4_FW_REGS_ABI_VER 1

/**
 * \brief Mailbox Window 0 firmware register layout exposed to host driver.
 * \ingroup ipc4_notification_api
 */
struct ipc4_fw_registers {
	struct ipc4_fw_status_reg fsr; /**< Current ROM / FW status (at offset 0x0). */
	ipc4_last_error lec;           /**< Last ROM / FW error code (at offset 0x4). */
	struct ipc4_fw_pwr_status fps; /**< Current DSP power / clock status (at offset 0x8). */
	uint32_t lnec;                 /**< Last Native Error Code from external library (at offset 0xC). */
	uint32_t ltr;                  /**< Copy of LTRC hardware register value (at offset 0x10). */
	uint32_t rsvd0;                /**< Reserved padding. */
	union ipc4_rom_info rom_info;  /**< ROM version and load capabilities (at offset 0x18). */
	uint32_t abi_ver;              /**< Layout version (IPC4_FW_REGS_ABI_VER at offset 0x1C). */
	uint8_t slave_core_sts[IPC4_MAX_SUPPORTED_ADSP_CORES]; /**< Status byte per secondary core. */
	uint32_t rsvd2[6];             /**< Reserved padding. */
	struct ipc4_pipeline_registers pipeline_regs[IPC4_MAX_PIPELINE_REG_SLOTS]; /**< Pipeline LPIB offsets. */
	struct ipc4_peak_volume_regs peak_vol_regs[IPC4_MAX_PEAK_VOL_REG_SLOTS];   /**< Peak meter telemetry. */
	struct ipc4_llp_reading_slot llp_gpdma_reading_slots[IPC4_MAX_LLP_GPDMA_READING_SLOTS]; /**< GPDMA LLP readings. */
	struct ipc4_llp_reading_slot llp_sndw_reading_slots[IPC4_MAX_LLP_SNDW_READING_SLOTS - 1]; /**< SoundWire LLP readings. */
	struct ipc4_llp_reading_slot llp_evad_reading_slot; /**< EVAD gateway LLP reading. */
} __attribute__((packed, aligned(4)));

#endif
