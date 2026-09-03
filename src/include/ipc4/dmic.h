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
 * \brief IPC4 DMIC definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

/* IOCTL ID of DMIC Set Gain Coefficients */
#define DMIC_SET_GAIN_COEFFICIENTS 2

/* Maximum number of dmic gain coefficients */
#define DMIC_MAX_GAIN_COEFFS_CNT   4

/**
 * @brief Structure representing the global configuration for DMIC (Digital Microphone) module.
 */
union dmic_global_cfg {
	/**
	 * @brief Raw 32-bit value of Global Cfg.
	 */
	uint32_t clock_on_delay;

	/**
	 * @brief Bitfields of Extended Global Config.
	 */
	struct {
		/**
		 * @brief Specifies the period in milliseconds to override data with silence after
		 * DMA transfer is started.
		 */
		uint32_t silence_period : 16;

		/**
		 * @brief Specifies the period in milliseconds for fade-in to apply on input data
		 * (following silence_period if applied).
		 */
		uint32_t fade_in_period : 16;
	} ext_global_cfg;
} __packed __aligned(4);

/**
 * @brief Structure representing the configuration of a DMIC channel.
 */
struct dmic_channel_cfg {
	/**
	 * @brief Outcontrol
	 */
	uint32_t out_control;
};

/**
 * @brief Structure representing FIR (Finite Impulse Response) configuration.
 */
struct dmic_fir_cfg {
	/**
	 * @brief FIR_CONTROL
	 * Control register for FIR configuration.
	 */
	uint32_t fir_control;

	/**
	 * @brief FIR_CONFIG
	 * Configuration register for FIR filter.
	 */
	uint32_t fir_config;

	/**
	 * @brief DC_OFFSET_LEFT
	 * DC offset value for the left channel.
	 */
	uint32_t dc_offset_left;

	/**
	 * @brief DC_OFFSET_RIGHT
	 * DC offset value for the right channel.
	 */
	uint32_t dc_offset_rigth;

	/**
	 * @brief OUT_GAIN_LEFT
	 * Output gain value for the left channel.
	 */
	uint32_t out_gain_left;

	/**
	 * @brief OUT_GAIN_RIGHT
	 * Output gain value for the right channel.
	 */
	uint32_t out_gain_rigth;

	/**
	 * @brief rsvd_2
	 * Reserved field.
	 */
	uint32_t rsvd_2[2];
} __packed __aligned(4);

/**
 * @brief Structure representing the configuration of the PDM control for DMIC.
 *
 * This structure defines the configuration parameters for the PDM control of the DMIC
 * (Digital Microphone) module. It includes fields for controlling the CIC (Cascaded
 * Integrator-Comb) filter, MIC (Microphone) control, SoundWire mapping, FIR (Finite
 * Impulse Response) configurations, and FIR coefficients.
 */
struct dmic_pdm_ctrl_cfg {
	/**
	 * @brief CIC_CONTROL
	 * Control register for CIC configuration.
	 */
	uint32_t cic_control;
	/**
	 * @brief CIC_CONFIG
	 * Configuration register for CIC filter.
	 */
	uint32_t cic_config;
	/**
	 * @brief Reserved field
	 */
	uint32_t rsvd_0;
	/**
	 * @brief MIC_CONTROL
	 * Control register for MIC configuration.
	 */
	uint32_t mic_control;
	/**
	 * @brief
	 * This field is used on platforms with SoundWire, otherwise ignored.
	 */
	uint32_t pdmsm;
	/**
	 * @brief Index of another PDMCtrlCfg to be used as a source of FIR coefficients.
	 */
	uint32_t reuse_fir_from_pdm;
	/**
	 * @brief Reserved field
	 */
	uint32_t rsvd_1[2];
	/**
	 * @brief FIR configurations
	 */
	struct dmic_fir_cfg fir_config[2];
	/**
	 * @brief Array of FIR coefficients, channel A goes first, then channel B.
	 */
	uint32_t fir_coeffs[0];
} __packed __aligned(4);

/**
 * @brief Structure representing the configuration blob for DMIC (Digital Microphone) settings.
 *
 * This structure contains various configuration settings for DMIC, including time-slot mappings,
 * global configuration, PDM channel configuration, and PDM controller configuration.
 */
struct dmic_config_blob {
	/**
	 * @brief Time-slot mappings.
	 */
	uint32_t ts_group[4];

	/**
	 * @brief DMIC global configuration.
	 */
	union dmic_global_cfg global_cfg;

	/**
	 * @brief PDM channels to be programmed using data from channel_cfg array.
	 */
	uint32_t channel_ctrl_mask : 8;

	/**
	 * @brief Clock source for DMIC.
	 */
	uint32_t clock_source : 8;

	/**
	 * @brief Reserved field.
	 */
	uint32_t rsvd : 16;

	/**
	 * @brief PDM channel configuration settings.
	 */
	struct dmic_channel_cfg channel_cfg[0];

	/**
	 * @brief PDM controllers to be programmed using data from pdm_ctrl_cfg array.
	 */
	uint32_t pdm_ctrl_mask;

	/**
	 * @brief PDM controller configuration settings.
	 */
	struct dmic_pdm_ctrl_cfg pdm_ctrl_cfg[0];
} __packed __aligned(4);

/**
 * @brief Structure representing the configuration data for DMIC.
 */
struct dmic_config_data {
	/**< Gateway attributes */
	union ipc4_gateway_attributes gtw_attributes;
	/**< DMIC Configuration BLOB */
	struct dmic_config_blob dmic_blob;
}  __packed __aligned(4);

#endif /* __SOF_IPC4_DMIC_H__ */
