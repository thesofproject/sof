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

/**
 * \file include/ipc4/probe.h
 * \brief IPC4 audio probe injection, extraction, and DMA definitions.
 * \ingroup ipc4_telemetry_probe_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_PROBE_H__
#define __SOF_IPC4_PROBE_H__

#include <sof/compiler_attributes.h>
#include "base-config.h"
#include <rtos/bit.h>
#include <stdint.h>

/**
 * \brief Buffer ID used in probe output stream headers for logging data packet.
 */
#define PROBE_LOGGING_BUFFER_ID		0x01000000

/** \brief Probe extraction mode: extract audio from pipeline to host. */
#define PROBE_PURPOSE_EXTRACTION	0
/** \brief Probe injection mode: inject host audio into pipeline. */
#define PROBE_PURPOSE_INJECTION		1

/** \brief Probe point attached to module input pin. */
#define PROBE_TYPE_INPUT		0
/** \brief Probe point attached to module output pin. */
#define PROBE_TYPE_OUTPUT		1
/** \brief Probe point attached to internal module buffer. */
#define PROBE_TYPE_INTERNAL		2

/** \brief Probe module parameter: add injection DMA channel. */
#define IPC4_PROBE_MODULE_INJECTION_DMA_ADD	  1
/** \brief Probe module parameter: detach injection DMA channel. */
#define IPC4_PROBE_MODULE_INJECTION_DMA_DETACH	  2
/** \brief Probe module parameter: connect probe points. */
#define IPC4_PROBE_MODULE_PROBE_POINTS_ADD	  3
/** \brief Probe module parameter: disconnect probe points. */
#define IPC4_PROBE_MODULE_DISCONNECT_PROBE_POINTS 4
/** \brief Probe module parameter: query active/available probe points. */
#define IPC4_PROBE_MODULE_AVAILABLE_PROBE_POINTS  5

/**
 * \brief Probe DMA channel descriptor.
 * \ingroup ipc4_telemetry_probe_api
 */
struct probe_dma {
	uint32_t stream_tag;		/**< Node ID / stream tag associated with this DMA. */
	uint32_t dma_buffer_size;	/**< Size of buffer in bytes associated with this DMA. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Unique identifier for a probe attachment point.
 * \ingroup ipc4_telemetry_probe_api
 */
typedef union probe_point_id {
	uint32_t full_id; /**< Raw 32-bit probe point identifier. */
	struct {
		uint32_t  module_id   : 16;	/**< Target module ID. */
		uint32_t  instance_id : 8;	/**< Target module instance ID. */
		uint32_t  type        : 2;	/**< Probe point type (PROBE_TYPE_xxx). */
		uint32_t  index       : 6;	/**< Pin / queue index inside target module. */
	} fields; /**< Bitfield access structure. */
} __attribute__((packed, aligned(4))) probe_point_id_t;

/**
 * \brief Probe point attachment descriptor.
 * \ingroup ipc4_telemetry_probe_api
 */
struct probe_point {
	probe_point_id_t buffer_id;	/**< ID of module buffer/pin to which probe is attached. */
	uint32_t purpose;	/**< Extraction or injection (PROBE_PURPOSE_xxx). */
	uint32_t stream_tag;	/**< Stream tag of DMA providing injection data (for injection probes); ignored for extraction where firmware automatically binds the configured extraction DMA tag. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Discovery response listing active probe points or DMA channels.
 * \ingroup ipc4_telemetry_probe_api
 */
struct sof_ipc_probe_info_params {
	uint32_t num_elems;				/**< Count of elements in payload array. */
	union {
		struct probe_dma probe_dma[0];		/**< Array of active DMA descriptors. */
		struct probe_point probe_point[0];	/**< Array of active probe point descriptors. */
	};
} __attribute__((packed, aligned(4)));

/**
 * \brief Probe module configuration structure.
 * \ingroup ipc4_telemetry_probe_api
 */
struct ipc4_probe_module_cfg {
	struct ipc4_base_module_cfg base_cfg; /**< Base module configuration parameters. */
	struct probe_dma gtw_cfg;             /**< Associated probe gateway DMA configuration. */
} __packed __aligned(4);

#endif /* __SOF_IPC4_PROBE_H__ */
