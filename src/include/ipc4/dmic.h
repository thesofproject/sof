/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOF_IPC4_DMIC_H__
#define __SOF_IPC4_DMIC_H__

#include <stdint.h>
#include <stddef.h>
#include <sof/drivers/dmic.h>
#include "gateway.h"

/**
 * \file include/ipc4/dmic.h
 * \brief IPC4 Digital Microphone (DMIC) gateway and PDM controller configuration.
 * \ingroup ipc4_gateway_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

/** \brief IOCTL command ID for DMIC set gain coefficients. */
#define DMIC_SET_GAIN_COEFFICIENTS 2

/** \brief Maximum number of DMIC gain coefficients. */
#define DMIC_MAX_GAIN_COEFFS_CNT   4

/**
 * \brief Global hardware configuration for DMIC controller.
 * \ingroup ipc4_gateway_api
 */
union dmic_global_cfg {
	/** Raw 32-bit clock on delay value. */
	uint32_t clock_on_delay;

	/** Extended global configuration bitfields. */
	struct {
		/**
		 * Specifies the period in milliseconds to override data with silence after
		 * DMA transfer is started.
		 */
		uint32_t silence_period : 16;

		/**
		 * Specifies the period in milliseconds for fade-in to apply on input data
		 * (following silence_period if applied).
		 */
		uint32_t fade_in_period : 16;
	} ext_global_cfg;
} __packed __aligned(4);

/**
 * \brief Configuration parameters for a single DMIC channel.
 * \ingroup ipc4_gateway_api
 */
struct dmic_channel_cfg {
	uint32_t out_control; /**< Output control register value. */
};

/**
 * \brief Finite Impulse Response (FIR) decimation filter configuration.
 * \ingroup ipc4_gateway_api
 */
struct dmic_fir_cfg {
	uint32_t fir_control;    /**< FIR filter control register value. */
	uint32_t fir_config;     /**< FIR filter configuration register value. */
	uint32_t dc_offset_left;  /**< DC offset compensation value for left channel. */
	uint32_t dc_offset_rigth; /**< DC offset compensation value for right channel. */
	uint32_t out_gain_left;   /**< Output gain value for left channel. */
	uint32_t out_gain_rigth;  /**< Output gain value for right channel. */
	uint32_t rsvd_2[2];       /**< Reserved padding. */
} __packed __aligned(4);

/**
 * \brief Pulse Density Modulation (PDM) controller configuration for DMIC.
 * \ingroup ipc4_gateway_api
 *
 * Defines the configuration parameters for the PDM control of the DMIC
 * module, including CIC decimation filter, microphone gain/control,
 * SoundWire pin mapping, and dual FIR filter cascades.
 */
struct dmic_pdm_ctrl_cfg {
	uint32_t cic_control;        /**< CIC filter control register value. */
	uint32_t cic_config;         /**< CIC filter configuration register value. */
	uint32_t rsvd_0;             /**< Reserved padding. */
	uint32_t mic_control;        /**< Microphone control register value. */
	uint32_t pdmsm;              /**< SoundWire PDM stream mapping (ignored on non-SDW). */
	uint32_t reuse_fir_from_pdm; /**< Index of another PDM controller to reuse FIR coefficients from. */
	uint32_t rsvd_1[2];          /**< Reserved padding. */
	struct dmic_fir_cfg fir_config[2]; /**< Cascaded FIR decimation filter configurations. */
	uint32_t fir_coeffs[0];      /**< Variable-length array of FIR coefficients (channel A then B). */
} __packed __aligned(4);

/**
 * \brief Complete configuration blob for Digital Microphone (DMIC) settings.
 * \ingroup ipc4_gateway_api
 *
 * Contains time-slot mappings, global clock/ramp settings, channel routing,
 * and per-PDM controller decimation parameters.
 */
struct dmic_config_blob {
	uint32_t ts_group[4]; /**< Time-slot grouping mappings. */
	union dmic_global_cfg global_cfg; /**< Global clock delay and fade parameters. */
	uint32_t channel_ctrl_mask : 8;  /**< PDM channels to program from channel_cfg. */
	uint32_t clock_source : 8;       /**< Clock source selection for DMIC. */
	uint32_t rsvd : 16;              /**< Reserved padding. */
	struct dmic_channel_cfg channel_cfg[0]; /**< Variable-length channel configurations. */
	uint32_t pdm_ctrl_mask;          /**< Bitmask of active PDM controllers. */
	struct dmic_pdm_ctrl_cfg pdm_ctrl_cfg[0]; /**< Variable-length PDM controller configurations. */
} __packed __aligned(4);

/**
 * \brief Top-level DMIC configuration data container.
 * \ingroup ipc4_gateway_api
 */
struct dmic_config_data {
	union ipc4_gateway_attributes gtw_attributes; /**< Gateway memory allocation attributes. */
	struct dmic_config_blob dmic_blob; /**< Hardware DMIC configuration blob. */
}  __packed __aligned(4);

#endif /* __SOF_IPC4_DMIC_H__ */
