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
 * \brief IPC4 Synchronous Serial Port (SSP / I2S) gateway and TDM configuration.
 * \ingroup ipc4_gateway_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_SSP_H__
#define __SOF_IPC4_SSP_H__

#include <stdint.h>
#include <ipc4/gateway.h>

/** \brief Invalid TDM slot mapping sentinel value. */
#define I2S_TDM_INVALID_SLOT_MAP1 0xF
/** \brief Maximum number of audio channels supported over SSP TDM. */
#define I2S_TDM_MAX_CHANNEL_COUNT 8
/** \brief Maximum number of TDM slot mapping entries. */
#define I2S_TDM_MAX_SLOT_MAP_COUNT 8

/**
 * \brief Hardware register configuration values for Intel SSP/I2S controller.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_ssp_config {
	uint32_t ssc0;   /**< Synchronous Serial Port Control register 0 (SSCR0: mode, data size). */
	uint32_t ssc1;   /**< Synchronous Serial Port Control register 1 (SSCR1: clocks, phase, polarity). */
	uint32_t sscto;  /**< Synchronous Serial Port Control Timeout register (SSCTO). */
	uint32_t sspsp;  /**< Synchronous Serial Port Programmable Serial Protocol register (SSPSP: frame sync timing). */
	uint32_t sstsa;  /**< Synchronous Serial Port Tx Time Slot Active register (SSTSA). */
	uint32_t ssrsa;  /**< Synchronous Serial Port Rx Time Slot Active register (SSRSA). */
	uint32_t ssc2;   /**< Synchronous Serial Port Control register 2 (SSCR2: extended FIFO control). */
	uint32_t sspsp2; /**< Synchronous Serial Port Programmable Serial Protocol register 2 (SSPSP2). */
	uint32_t ssc3;   /**< Synchronous Serial Port Control register 3 (SSCR3). */
	uint32_t ssioc;  /**< Synchronous Serial Port I/O Control register (SSIOC: pin directions). */
} __attribute__((packed, aligned(4)));

/**
 * \brief Master Clock (MCLK) divider hardware configuration.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_ssp_mclk_config {
	uint32_t mdivc; /**< Master clock divider control register. */
	uint32_t mdivr; /**< Master clock divider ratio register. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Combined SSP hardware and clock configuration structure.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_ssp_driver_config {
	struct ipc4_ssp_config i2s_config;       /**< SSP controller register values. */
	struct ipc4_ssp_mclk_config mclk_config; /**< Master clock generator configuration. */
} __attribute__((packed, aligned(4)));

/**
 * \brief SSP interface clock start control parameters.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_ssp_start_control {
	/**
	 * Delay in milliseconds between enabling interface (when Copier is attached)
	 * and actual interface start. Value of 0 means no delay.
	 */
	uint32_t clock_warm_up    : 16;

	/** Target clock: 1 for MCLK, 0 for SCLK. */
	uint32_t mclk             : 1;

	/**
	 * 1 means start clock immediately even if no Copier is attached.
	 */
	uint32_t warm_up_ovr      : 1;
	uint32_t rsvd0            : 14; /**< Reserved padding. */
} __attribute__((packed, aligned(4)));

/**
 * \brief SSP interface clock stop control parameters.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_ssp_stop_control {
	/**
	 * Delay in milliseconds between stopping interface (when Copier detaches)
	 * and stopping interface clocks. 0 means no delay.
	 */
	uint32_t clock_stop_delay : 16;

	/** 1 means keep clock running continuously after Copier detaches. */
	uint32_t keep_running     : 1;

	/** 1 means stop clock immediately upon detachment. */
	uint32_t clock_stop_ovr   : 1;
	uint32_t rsvd1            : 14; /**< Reserved padding. */
} __attribute__((packed, aligned(4)));

/**
 * \brief SSP dynamic DMA and clock control union.
 * \ingroup ipc4_gateway_api
 */
union ipc4_ssp_dma_control {
	/** Start and stop clock delay controls. */
	struct ipc4_ssp_control {
		struct ipc4_ssp_start_control start_control; /**< Clock warm-up parameters. */
		struct ipc4_ssp_stop_control stop_control;   /**< Clock stop delay parameters. */
	} control_data; /**< Clock lifecycle controls. */

	/** Fractional M/N clock divider settings. */
	struct ipc4_mn_div_config {
		uint32_t mval; /**< M numerator value. */
		uint32_t nval; /**< N denominator value. */
	} mndiv_control_data; /**< M/N divider ratio. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Full SSP/I2S gateway configuration blob structure.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_ssp_configuration_blob {
	union ipc4_gateway_attributes gw_attr; /**< Gateway memory allocation attributes. */
	uint32_t tdm_ts_group[I2S_TDM_MAX_SLOT_MAP_COUNT]; /**< TDM time slot channel mappings. */
	struct ipc4_ssp_driver_config i2s_driver_config;   /**< SSP controller and MCLK registers. */
	union ipc4_ssp_dma_control i2s_dma_control[];      /**< Optional dynamic DMA/clock controls. */
} __attribute__((packed, aligned(4)));

#endif
