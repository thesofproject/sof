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
 */

/**
 * \file include/ipc4/ssp.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_SSP_H__
#define __SOF_IPC4_SSP_H__

#include <stdint.h>
#include <ipc4/gateway.h>

#define I2S_TDM_INVALID_SLOT_MAP1 0xF
#define I2S_TDM_MAX_CHANNEL_COUNT 8
#define I2S_TDM_MAX_SLOT_MAP_COUNT 8

/* i2s Configuration BLOB building blocks */

/* i2s registers for i2s Configuration */
struct ipc4_ssp_config {
	uint32_t ssc0;
	uint32_t ssc1;
	uint32_t sscto;
	uint32_t sspsp;
	uint32_t sstsa;
	uint32_t ssrsa;
	uint32_t ssc2;
	uint32_t sspsp2;
	uint32_t ssc3;
	uint32_t ssioc;
} __attribute__((packed, aligned(4)));

struct ipc4_ssp_mclk_config {
	/* master clock divider control register */
	uint32_t mdivc;

	/* master clock divider ratio register */
	uint32_t mdivr;
} __attribute__((packed, aligned(4)));

struct ipc4_ssp_driver_config {
	struct ipc4_ssp_config i2s_config;
	struct ipc4_ssp_mclk_config mclk_config;
} __attribute__((packed, aligned(4)));

struct ipc4_ssp_start_control {
	/*
	  delay in msec between enabling interface (moment when Copier
	  instance is being attached to the interface) and actual
	  interface start. Value of 0 means no delay.
	*/
	uint32_t clock_warm_up    : 16;

	/* specifies if parameters target MCLK (1) or SCLK (0) */
	uint32_t mclk             : 1;

	/*
	  value of 1 means that clock should be started immediately
	  even if no Copier instance is currently attached to the interface.
	*/
	uint32_t warm_up_ovr      : 1;
	uint32_t rsvd0            : 14;
} __attribute__((packed, aligned(4)));

struct ipc4_ssp_stop_control {
	/*
	  delay in msec between stopping the interface
	  (moment when Copier instance is being detached from the interface)
	  and interface clock stop. Value of 0 means no delay.
	*/
	uint32_t clock_stop_delay : 16;

	/*
	  value of 1 means that clock should be kept running (infinite
	  stop delay) after Copier instance detaches from the interface.
	*/
	uint32_t keep_running     : 1;

	/*
	  value of 1 means that clock should be stopped immediately.
	*/
	uint32_t clock_stop_ovr   : 1;
	uint32_t rsvd1            : 14;
} __attribute__((packed, aligned(4)));

union ipc4_ssp_dma_control {
	struct ipc4_ssp_control {
		struct ipc4_ssp_start_control start_control;
		struct ipc4_ssp_stop_control stop_control;
	} control_data;

	struct ipc4_mn_div_config {
		uint32_t mval;
		uint32_t nval;
	} mndiv_control_data;
} __attribute__((packed, aligned(4)));

struct ipc4_ssp_configuration_blob {
	union ipc4_gateway_attributes gw_attr;

	/* TDM time slot mappings */
	uint32_t tdm_ts_group[I2S_TDM_MAX_SLOT_MAP_COUNT];

	/* i2s port configuration */
	struct ipc4_ssp_driver_config i2s_driver_config;

	/* optional configuration parameters */
	union ipc4_ssp_dma_control i2s_dma_control[0];
} __attribute__((packed, aligned(4)));

#endif
