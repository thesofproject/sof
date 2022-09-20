/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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

/*
 * \file include/ipc4/probe.h
 * \brief probe ipc4 definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_PROBE_H__
#define __SOF_IPC4_PROBE_H__

#include <sof/compiler_attributes.h>
#include "base-config.h"
#include <rtos/bit.h>
#include <stdint.h>

/*
 * Buffer id used in the probe output stream headers for
 * logging data packet.
 */
#define PROBE_LOGGING_BUFFER_ID		0x01000000

#define PROBE_PURPOSE_EXTRACTION	0
#define PROBE_PURPOSE_INJECTION		1

#define PROBE_TYPE_INPUT		0
#define PROBE_TYPE_OUTPUT		1
#define PROBE_TYPE_INTERNAL		2

#define IPC4_PROBE_MODULE_INJECTION_DMA_ADD	  1
#define IPC4_PROBE_MODULE_INJECTION_DMA_DETACH	  2
#define IPC4_PROBE_MODULE_PROBE_POINTS_ADD	  3
#define IPC4_PROBE_MODULE_DISCONNECT_PROBE_POINTS 4

/**
 * Description of probe dma
 */
struct probe_dma {
	uint32_t stream_tag;		/**< Node_id associated with this DMA */
	uint32_t dma_buffer_size;	/**< Size of buffer associated with this DMA */
} __attribute__((packed, aligned(4)));

/**
 * Description of probe point id
 */
typedef union probe_point_id {
	uint32_t full_id;
	struct {
		uint32_t  module_id   : 16;	/**< Target module ID */
		uint32_t  instance_id : 8;	/**< Target module instance ID */
		uint32_t  type        : 2;	/**< Probe point type as specified by ProbeType enumeration */
		uint32_t  index       : 6;	/**< Queue index inside target module */
	} fields;
} __attribute__((packed, aligned(4))) probe_point_id_t;

/**
 * Description of probe point
 */
struct probe_point {
	probe_point_id_t buffer_id;	/**< ID of buffer to which probe is attached */
	uint32_t purpose;	/**< PROBE_PURPOSE_xxx */
	uint32_t stream_tag;	/**< Stream tag of DMA via which data will be provided for injection.
				 *   For extraction purposes, stream tag is ignored when received,
				 *   but returned actual extraction stream tag via INFO function.
				 */
} __attribute__((packed, aligned(4)));

struct sof_ipc_probe_info_params {
	uint32_t num_elems;				/**< Count of elements in array */
	union {
		struct probe_dma probe_dma[0];		/**< DMA info */
		struct probe_point probe_point[0];	/**< Probe Point info */
	};
} __attribute__((packed, aligned(4)));

struct ipc4_probe_module_cfg {
	struct ipc4_base_module_cfg base_cfg;
	struct probe_dma gtw_cfg;
} __packed __aligned(8);

#endif /* __SOF_IPC4_PROBE_H__ */
