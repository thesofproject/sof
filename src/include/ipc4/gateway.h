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
 * \file include/ipc4/gateway.h
 * \brief IPC4 audio gateway attributes, ALH, HDA, and SoundWire gateway definitions.
 * \ingroup ipc4_gateway_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_GATEWAY_H__
#define __SOF_IPC4_GATEWAY_H__

#include <stdint.h>
#include <rtos/bit.h>

/**
 * \brief Hardware and virtual audio gateway connector types.
 * \ingroup ipc4_gateway_api
 */
enum ipc4_connector_node_id_type {
	/** HD/A host output stream (Host to DSP playback). */
	ipc4_hda_host_output_class = 0,
	/** HD/A host input stream (DSP to Host capture). */
	ipc4_hda_host_input_class = 1,
	/** HD/A host bidirectional stream (reserved for future use). */
	ipc4_hda_host_inout_class = 2,

	/** HD/A link output (DSP to external codec playback). */
	ipc4_hda_link_output_class = 8,
	/** HD/A link input (external codec to DSP capture). */
	ipc4_hda_link_input_class = 9,
	/** HD/A link bidirectional stream (reserved for future use). */
	ipc4_hda_link_inout_class = 10,

	/** Digital Microphone (DMIC) link input (microphones to DSP capture). */
	ipc4_dmic_link_input_class = 11,

	/** I2S/SSP link output (DSP to I2S codec playback). */
	ipc4_i2s_link_output_class = 12,
	/** I2S/SSP link input (I2S codec to DSP capture). */
	ipc4_i2s_link_input_class = 13,

	/** ALH link output, legacy alias for SoundWire (DSP to SoundWire link). */
	ipc4_alh_link_output_class = 16,
	/** ALH link input, legacy alias for SoundWire (SoundWire link to DSP). */
	ipc4_alh_link_input_class = 17,

	/** SoundWire link output stream (DSP to SoundWire codec playback). */
	ipc4_alh_snd_wire_stream_link_output_class = 16,
	/** SoundWire link input stream (SoundWire codec to DSP capture). */
	ipc4_alh_snd_wire_stream_link_input_class = 17,

	/** USB Audio Offload (UAOL) link output (DSP to UAOL controller). */
	ipc4_alh_uaol_stream_link_output_class = 18,
	/** USB Audio Offload (UAOL) link input (UAOL controller to DSP). */
	ipc4_alh_uaol_stream_link_input_class = 19,

	/** Inter-Processor Communication (IPC) output (DSP to host/peer). */
	ipc4_ipc_output_class = 20,
	/** Inter-Processor Communication (IPC) input (host/peer to DSP). */
	ipc4_ipc_input_class = 21,

	/** I2S aggregated multi-gateway output. */
	ipc4_i2s_multi_link_output_class = 22,
	/** I2S aggregated multi-gateway input. */
	ipc4_i2s_multi_link_input_class = 23,
	/** GPIO hardware gateway. */
	ipc4_gpio_class = 24,
	/** SPI hardware gateway output. */
	ipc4_spi_output_class = 25,
	/** SPI hardware gateway input. */
	ipc4_spi_input_class = 26,
	/** Sentinel marking maximum connector node ID type. */
	ipc4_max_connector_node_id_type
};

/** \brief Invalid raw node identifier sentinel. */
#define IPC4_INVALID_NODE_ID	0xffffffff

/** \brief Bitmask covering virtual index and DMA type bits. */
#define IPC4_NODE_ID_MASK	0x1fff

/**
 * \brief Audio gateway addressing descriptor (connector node ID).
 * \ingroup ipc4_gateway_api
 *
 * Encodes the gateway type and virtual DMA channel index for HDA, ALH,
 * DMIC, and SSP gateways.
 */
union ipc4_connector_node_id {
	uint32_t dw; /**< Raw 32-bit value of node ID. */

	struct {
		uint32_t v_index : 8;  /**< Virtual DMA channel or FIFO index. */
		uint32_t dma_type : 5; /**< Gateway type (enum ipc4_connector_node_id_type). */
		uint32_t _rsvd : 19;   /**< Reserved bits. */
	} f; /**< Bitfield access structure. */
} __attribute__((packed, aligned(4)));

/** \brief Base node ID for host output DMA channels. */
#define IPC4_HW_HOST_OUTPUT_NODE_ID_BASE	0x00
/** \brief Node ID assigned to firmware code loader DMA. */
#define IPC4_HW_CODE_LOADER_NODE_ID		0x0F
/** \brief Base node ID for link input DMA channels. */
#define IPC4_HW_LINK_INPUT_NODE_ID_BASE		0x10

/**
 * \brief Gateway buffer allocation attributes.
 * \ingroup ipc4_gateway_api
 *
 * Configures special memory placement for gateway FIFOs and DMA buffers.
 */
union ipc4_gateway_attributes {
	uint32_t dw; /**< Raw 32-bit attributes value. */

	struct {
		uint32_t lp_buffer_alloc : 1;     /**< Allocate gateway buffers in Low Power (LP) SRAM. */
		uint32_t alloc_from_reg_file : 1; /**< Allocate gateway structures from DSP register file. */
		uint32_t _rsvd : 30;              /**< Reserved bits. */
	} bits; /**< Bitfield access structure. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Generic gateway configuration blob container.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_gateway_config_data {
	union ipc4_gateway_attributes gtw_attributes; /**< Memory allocation attributes. */
	uint32_t config_blob[]; /**< Variable-length gateway-specific configuration payload. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Configuration parameters for IPC Host Gateway.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_ipc_gateway_config_blob {
	uint32_t buffer_size; /**< Size of gateway circular buffer in bytes. */

	union {
		struct {
			uint32_t notif_high : 1; /**< Send notification when buffer exceeds high threshold. */
			uint32_t notif_low : 1;  /**< Send notification when buffer falls below low threshold. */
			uint32_t rsvd : 30;      /**< Reserved bits. */
		} f; /**< Bitfield access structure. */
		uint32_t flags_raw; /**< Raw 32-bit flags value. */
	} u; /**< Notification threshold flags union. */

	uint32_t threshold_high; /**< High buffer fill watermark in bytes. */
	uint32_t threshold_low;  /**< Low buffer fill watermark in bytes. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Gateway peripheral category bitmask.
 * \ingroup ipc4_gateway_api
 */
enum ipc4_gateway_type {
	ipc4_gtw_none	= BIT(0), /**< No gateway specified. */
	ipc4_gtw_host	= BIT(1), /**< Host DMA gateway. */
	ipc4_gtw_dmic	= BIT(2), /**< Digital microphone gateway. */
	ipc4_gtw_link	= BIT(3), /**< HD/A link gateway. */
	ipc4_gtw_alh	= BIT(4), /**< Audio Hub Link / SoundWire gateway. */
	ipc4_gtw_ssp	= BIT(5), /**< SSP / I2S gateway. */
	ipc4_gtw_all	= BIT(6) - 1 /**< Bitmask covering all gateway types. */
};

/**
 * \brief Audio data stream direction flags.
 * \ingroup ipc4_gateway_api
 */
enum ipc4_direction_type {
	ipc4_playback = BIT(0),                 /**< Playback stream direction (host/memory to peripheral). */
	ipc4_capture = BIT(1),                  /**< Capture stream direction (peripheral to host/memory). */
	ipc4_bidirection = BIT(0) | BIT(1)      /**< Bidirectional stream direction. */
};

/** \brief Helper macro to construct direction bitmask from index. */
#define IPC4_DIRECTION(x) BIT(x)
#endif
