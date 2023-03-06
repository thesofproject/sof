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
 * \file include/ipc4/header.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_GATEWAY_H__
#define __SOF_IPC4_GATEWAY_H__

#include <stdint.h>

/**< Type of the gateway. */
enum ipc4_connector_node_id_type {
	/**< HD/A host output (-> DSP). */
	ipc4_hda_host_output_class = 0,
	/**< HD/A host input (<- DSP). */
	ipc4_hda_host_input_class = 1,
	/**< HD/A host input/output (rsvd for future use). */
	ipc4_hda_host_inout_class = 2,

	/**< HD/A link output (DSP ->). */
	ipc4_hda_link_output_class = 8,
	/**< HD/A link input (DSP <-). */
	ipc4_hda_link_input_class = 9,
	/**< HD/A link input/output (rsvd for future use). */
	ipc4_hda_link_inout_class = 10,

	/**< DMIC link input (DSP <-). */
	ipc4_dmic_link_input_class = 11,

	/**< I2S link output (DSP ->). */
	ipc4_i2s_link_output_class = 12,
	/**< I2S link input (DSP <-). */
	ipc4_i2s_link_input_class = 13,

	/**< ALH link output, legacy for SNDW (DSP ->). */
	ipc4_alh_link_output_class = 16,
	/**< ALH link input, legacy for SNDW (DSP <-). */
	ipc4_alh_link_input_class = 17,

	/**< SNDW link output (DSP ->). */
	ipc4_alh_snd_wire_stream_link_output_class = 16,
	/**< SNDW link input (DSP <-). */
	ipc4_alh_snd_wire_stream_link_input_class = 17,

	/**< UAOL link output (DSP ->). */
	ipc4_alh_uaol_stream_link_output_class = 18,
	/**< UAOL link input (DSP <-). */
	ipc4_alh_uaol_stream_link_input_class = 19,

	/**< IPC output (DSP ->). */
	ipc4_ipc_output_class = 20,
	/**< IPC input (DSP <-). */
	ipc4_ipc_input_class = 21,

	/**< I2S Multi gtw output (DSP ->). */
	ipc4_i2s_multi_link_output_class = 22,
	/**< I2S Multi gtw input (DSP <-). */
	ipc4_i2s_multi_link_input_class = 23,
	/**< GPIO */
	ipc4_gpio_class = 24,
	/**< SPI */
	ipc4_spi_output_class = 25,
	ipc4_spi_input_class = 26,
	ipc4_max_connector_node_id_type
};

/**< Invalid raw node id (to indicate uninitialized node id). */
#define IPC4_INVALID_NODE_ID	0xffffffff

/**< all bits of v_index and dma_type */
#define IPC4_NODE_ID_MASK	0x1fff

/**< Base top-level structure of an address of a gateway. */
/*!
 * The virtual index value, presented on the top level as raw 8 bits,
 * is expected to be encoded in a gateway specific way depending on
 * the actual type of gateway.
 */
union ipc4_connector_node_id {

	/**< Raw 32-bit value of node id. */
	uint32_t dw;

	/**< Bit fields */
	struct {
		/**< Index of the virtual DMA at the gateway. */
		uint32_t v_index : 8;

		/**< Type of the gateway, one of ConnectorNodeId::Type values. */
		uint32_t dma_type : 5;

		/**< Rsvd field. */
		uint32_t _rsvd : 19;
	} f; /**<< Bits */
} __attribute__((packed, aligned(4)));

#define IPC4_HW_HOST_OUTPUT_NODE_ID_BASE	0x00
#define IPC4_HW_CODE_LOADER_NODE_ID		0x0F
#define IPC4_HW_LINK_INPUT_NODE_ID_BASE		0x10

/*!
 * Attributes are usually provided along with the gateway configuration
 * BLOB when the FW is requested to instantiate that gateway.
 *
 * There are flags which requests FW to allocate gateway related data
 * (buffers and other items used while transferring data, like linked list)
 *  to be allocated from a special memory area, e.g low power memory.
 */
union ipc4_gateway_attributes {

	/**< Raw value */
	uint32_t dw;

	/**< Access to the fields */
	struct {
		/**< Gateway data requested in low power memory. */
		uint32_t lp_buffer_alloc : 1;

		/**< Gateway data requested in register file memory. */
		uint32_t alloc_from_reg_file : 1;

		/**< Reserved field */
		uint32_t _rsvd : 30;
	} bits; /**<< Bits */
} __attribute__((packed, aligned(4)));

/**< Gateway configuration BLOB structure. */
/*!
 * Actual config_blob content depends on the specific target gateway type.
 */
struct ipc4_gateway_config_data {
	/**< Gateway attributes */
	union ipc4_gateway_attributes gtw_attributes;

	/**< Configuration BLOB */
	uint32_t config_blob[0];
} __attribute__((packed, aligned(4)));

/**< Configuration for the IPC Gateway */
struct ipc4_ipc_gateway_config_blob {

	/**< Size of the gateway buffer, specified in bytes */
	uint32_t buffer_size;

	/**< Flags */
	union flags {
		struct bits {
			/**< Activates high threshold notification */
			/*!
			 * Indicates whether notification should be sent to the host
			 * when the size of data in the buffer reaches the high threshold
			 * specified by threshold_high parameter.
			 */
			uint32_t notif_high : 1;

			/**< Activates low threshold notification */
			/*!
			 * Indicates whether notification should be sent to the host
			 * when the size of data in the buffer reaches the low threshold
			 * specified by threshold_low parameter.
			 */
			uint32_t notif_low : 1;

			/**< Reserved field */
			uint32_t rsvd : 30;
		} f; /**<< Bits */
		/**< Raw value of flags */
		uint32_t flags_raw;
	} u; /**<< Flags */

	/**< High threshold */
	/*!
	 * Specifies the high threshold (in bytes) for notifying the host
	 * about the buffered data level.
	 */
	uint32_t threshold_high;

	/**< Low threshold */
	/*!
	 * Specifies the low threshold (in bytes) for notifying the host
	 * about the buffered data level.
	 */
	uint32_t threshold_low;
} __attribute__((packed, aligned(4)));

#define ALH_MULTI_GTW_BASE	0x50

enum ipc4_gateway_type {
	ipc4_gtw_none	= BIT(0),
	ipc4_gtw_host	= BIT(1),
	ipc4_gtw_dmic	= BIT(2),
	ipc4_gtw_link	= BIT(3),
	ipc4_gtw_alh	= BIT(4),
	ipc4_gtw_ssp	= BIT(5),
	ipc4_gtw_all	= BIT(6) - 1
};

enum ipc4_direction_type {
	ipc4_playback = BIT(0),
	ipc4_capture = BIT(1),
	ipc4_bidirection = BIT(0) | BIT(1)
};

#define IPC4_DIRECTION(x) BIT(x)
#endif
