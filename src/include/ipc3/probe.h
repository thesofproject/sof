/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

/**
 * \file include/ipc/probe.h
 * \brief Probe IPC definitions
 * \author Adrian Bonislawski <adrian.bonislawski@linux.intel.com>
 * \author Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __IPC_PROBE_H__
#define __IPC_PROBE_H__

#include <ipc/header.h>
#include <rtos/bit.h>
#include <stdint.h>

/*
 * Buffer id used in the probe output stream headers for
 * logging data packet.
 */
#define PROBE_LOGGING_BUFFER_ID 0

#define PROBE_PURPOSE_EXTRACTION	0x1
#define PROBE_PURPOSE_INJECTION		0x2
#define PROBE_PURPOSE_LOGGING		0x3
#define PROBE_PURPOSE_TRACING		0x4

/**
 * Description of probe dma
 */
struct probe_dma {
	uint32_t stream_tag;		/**< Stream tag associated with this DMA */
	uint32_t dma_buffer_size;	/**< Size of buffer associated with this DMA */
} __attribute__((packed, aligned(4)));

/**
 * Description of probe point id
 */
typedef struct probe_point_id {
	uint32_t full_id;
} __attribute__((packed, aligned(4))) probe_point_id_t;

/**
 * Description of probe point
 */
struct probe_point {
	probe_point_id_t buffer_id;	/**< ID of buffer to which probe is attached */
	uint32_t purpose;		/**< PROBE_PURPOSE_xxx */
	uint32_t stream_tag;		/**< Stream tag of DMA via which data will be provided for injection.
					 *   For extraction purposes, stream tag is ignored when received,
					 *   but returned actual extraction stream tag via INFO function.
					 */
} __attribute__((packed, aligned(4)));

/**
 * \brief DMA ADD for probes.
 *
 * Used as payload for IPCs: SOF_IPC_PROBE_INIT, SOF_IPC_PROBE_DMA_ADD.
 */
struct sof_ipc_probe_dma_add_params {
	struct sof_ipc_cmd_hdr hdr;	/**< Header */
	uint32_t num_elems;		/**< Count of DMAs specified in array */
	struct probe_dma probe_dma[];	/**< Array of DMAs to be added */
} __attribute__((packed, aligned(4)));

/**
 * \brief Reply to INFO functions.
 *
 * Used as payload for IPCs: SOF_IPC_PROBE_DMA_INFO, SOF_IPC_PROBE_POINT_INFO.
 */
struct sof_ipc_probe_info_params {
	struct sof_ipc_reply rhdr;			/**< Header */
	uint32_t num_elems;				/**< Count of elements in array */
	union {
		struct probe_dma probe_dma[0];		/**< DMA info */
		struct probe_point probe_point[0];	/**< Probe Point info */
	};
} __attribute__((packed, aligned(4)));

/**
 * \brief Probe DMA remove.
 *
 * Used as payload for IPC: SOF_IPC_PROBE_DMA_REMOVE
 */
struct sof_ipc_probe_dma_remove_params {
	struct sof_ipc_cmd_hdr hdr;	/**< Header */
	uint32_t num_elems;		/**< Count of stream tags specified in array */
	uint32_t stream_tag[];		/**< Array of stream tags associated with DMAs to remove */
} __attribute__((packed, aligned(4)));

/**
 * \brief Add probe points.
 *
 * Used as payload for IPC: SOF_IPC_PROBE_POINT_ADD
 */
struct sof_ipc_probe_point_add_params {
	struct sof_ipc_cmd_hdr hdr;		/**< Header */
	uint32_t num_elems;			/**< Count of Probe Points specified in array */
	struct probe_point probe_point[];	/**< Array of Probe Points to add */
} __attribute__((packed, aligned(4)));

/**
 * \brief Remove probe point.
 *
 * Used as payload for IPC: SOF_IPC_PROBE_POINT_REMOVE
 */
struct sof_ipc_probe_point_remove_params {
	struct sof_ipc_cmd_hdr hdr;	/**< Header */
	uint32_t num_elems;		/**< Count of buffer IDs specified in array */
	uint32_t buffer_id[];		/**< Array of buffer IDs from which Probe Points should be removed */
} __attribute__((packed, aligned(4)));

#endif /* __IPC_PROBE_H__ */
