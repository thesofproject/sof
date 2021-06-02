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
 * \file include/ipc4/dmic.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_DMIC_H__
#define __SOF_IPC4_DMIC_H__

#include <stdint.h>
#include <ipc4/gateway.h>

/*
 * If there is only one PDM controller configuration passed,
 * the other (missing) one is configured by the driver just by
 * clearing CIC_CONTROL.SOFT_RESET bit.
 * The driver needs to make sure that all mics are disabled
 * before starting to program PDM controllers.
 */
struct ipc4_dmic_fir_config {
	uint32_t fir_control;
	uint32_t fir_config;
	uint32_t dc_offset_left;
	uint32_t dc_offset_right;
	uint32_t out_gain_left;
	uint32_t out_gain_right;
	uint32_t rsvd_2[2];
} __attribute__((packed, aligned(4)));

/*
 * Note that FIR array may be provided in either packed or unpacked format.
 * see FIR_COEFFS_PACKED_TO_24_BITS.
 * Since in many cases all PDMs are programmed with the same FIR settings,
 * it is possible to provide it in a single copy inside the BLOB and refer
 * to that from other PDM configurations \see reuse_fir_from_pdm.
 */
struct ipc4_dmic_pdm_ctrl_cfg {
	uint32_t cic_control;
	uint32_t cic_config;
	uint32_t rsvd_0;
	uint32_t mic_control;
	/* this field is used on platforms with SoundWire, otherwise ignored */
	uint32_t pdmsm;
	//! Index of another PDMCtrlCfg to be used as a source of FIR coefficients.
	/*!
	  \note The index is 1-based, value of 0 means that FIR coefficients
	     array fir_coeffs is provided by this item.
	  This is a very common case that the same FIR coefficients are used
	  to program more than one PDM controller. In this case, fir_coeffs array
	  may be provided in a single copy following PdmCtrlCfg #0 and be reused
	  by PdmCtrlCfg #1 by setting reuse_fir_from_pdm to 1 (1-based index).
	*/
	uint32_t reuse_fir_from_pdm;
	uint32_t rsvd_1[2];
	struct ipc4_dmic_fir_config fir_config[2];

	/*
	 * Array of FIR coefficients, channel A goes first, then channel B.
	 * Actual size of the array depends on the number of active taps of the
	 * FIR filter for channel A plus the number of active taps of the FIR filter
	 * for channel B (see FIR_CONFIG) as well as on the form (packed/unpacked)
	 * of values.
	 */
	uint32_t fir_coeffs[0];
} __attribute__((packed, aligned(4)));

struct ipc4_dmic_config_blob {
	/* time-slot mappings */
	uint32_t ts_group[4];

	/* expected value is 1-3ms. typical value is 1ms */
	uint32_t clock_on_delay;

	/*
	 * PDM channels to be programmed using data from channel_cfg array.
	 * i'th bit = 1 means that configuration for PDM channel # i is provided
	 */
	uint32_t channel_ctrl_mask;

	//! PDM channel configuration settings
	/*!
	  Actual number of items depends on channel_ctrl_mask (# of 1's).
	*/
	uint32_t channel_cfg;

	//! PDM controllers to be programmed using data from pdm_ctrl_cfg array.
	/*!
	  i'th bit = 1 means that configuration for PDM controller # i is provided.
	*/
	uint32_t pdm_ctrl_mask;

	//! PDM controller configuration settings
	/*!
	  Actual number of items depends on pdm_ctrl_mask (# of 1's).
	*/
	struct ipc4_dmic_pdm_ctrl_cfg pdm_ctrl_cfg[0];
} __attribute__((packed, aligned(4)));

//DMIC gateway configuration data
struct ipc4_dmic_config_data {
	union ipc4_gateway_attributes gtw_attributes;
	struct ipc4_dmic_config_blob dmic_config_blob;
} __attribute__((packed, aligned(4)));

#endif
