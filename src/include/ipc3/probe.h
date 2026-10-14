/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

/**
 * \file include/ipc3/probe.h
 * \brief IPC3 Audio probe points, DMA descriptors, and configuration commands.
 * \ingroup audio_probe_api
 */

#ifndef __IPC_PROBE_H__
#define __IPC_PROBE_H__

#include <ipc/header.h>
#include <rtos/bit.h>
#include <stdint.h>

/** Buffer ID used in probe stream headers for extraction logging packets */
#define PROBE_LOGGING_BUFFER_ID 0

/** \name Probe Tap Point Purpose
 *  Intended direction and role of the probe connection.
 *  @{
 */
#define PROBE_PURPOSE_EXTRACTION	0x1	/**< Extract audio stream from buffer to host */
#define PROBE_PURPOSE_INJECTION		0x2	/**< Inject host audio stream into buffer */
#define PROBE_PURPOSE_LOGGING		0x3	/**< Route probe packets to logging backend */
#define PROBE_PURPOSE_TRACING		0x4	/**< Real-time pipeline trace capture */
/** @} */

/**
 * \brief Host DMA channel descriptor allocated for audio probes.
 * \ingroup audio_probe_api
 */
struct probe_dma {
	uint32_t stream_tag;		/**< Stream tag associated with this DMA channel */
	uint32_t dma_buffer_size;	/**< Size of host DMA buffer in bytes */
} __attribute__((packed, aligned(4)));

/**
 * \brief Packed identifier identifying target audio buffer for probe attachment.
 * \ingroup audio_probe_api
 */
typedef struct probe_point_id {
	uint32_t full_id;		/**< Packed buffer identifier */
} __attribute__((packed, aligned(4))) probe_point_id_t;

/**
 * \brief Probe tap point descriptor binding a pipeline buffer to a probe purpose.
 * \ingroup audio_probe_api
 */
struct probe_point {
	probe_point_id_t buffer_id;	/**< ID of pipeline buffer to which probe is attached */
	uint32_t purpose;		/**< Probe role (PROBE_PURPOSE_*) */
	uint32_t stream_tag;		/**< Stream tag of DMA for injection; ignored on extraction request and populated on reply */
} __attribute__((packed, aligned(4)));

/**
 * \brief DMA ADD for probes payload (SOF_IPC_PROBE_INIT, SOF_IPC_PROBE_DMA_ADD).
 * \ingroup audio_probe_api
 */
struct sof_ipc_probe_dma_add_params {
	struct sof_ipc_cmd_hdr hdr;	/**< Header */
	uint32_t num_elems;		/**< Count of DMAs specified in array */
	struct probe_dma probe_dma[];	/**< Array of DMAs to be added */
} __attribute__((packed, aligned(4)));

/**
 * \brief Reply to INFO queries (SOF_IPC_PROBE_DMA_INFO, SOF_IPC_PROBE_POINT_INFO).
 * \ingroup audio_probe_api
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
 * \brief Probe DMA remove payload (SOF_IPC_PROBE_DMA_REMOVE).
 * \ingroup audio_probe_api
 */
struct sof_ipc_probe_dma_remove_params {
	struct sof_ipc_cmd_hdr hdr;	/**< Header */
	uint32_t num_elems;		/**< Count of stream tags specified in array */
	uint32_t stream_tag[];		/**< Array of stream tags associated with DMAs to remove */
} __attribute__((packed, aligned(4)));

/**
 * \brief Add probe points payload (SOF_IPC_PROBE_POINT_ADD).
 * \ingroup audio_probe_api
 */
struct sof_ipc_probe_point_add_params {
	struct sof_ipc_cmd_hdr hdr;		/**< Header */
	uint32_t num_elems;			/**< Count of Probe Points specified in array */
	struct probe_point probe_point[];	/**< Array of Probe Points to add */
} __attribute__((packed, aligned(4)));

/**
 * \brief Remove probe points payload (SOF_IPC_PROBE_POINT_REMOVE).
 * \ingroup audio_probe_api
 */
struct sof_ipc_probe_point_remove_params {
	struct sof_ipc_cmd_hdr hdr;	/**< Header */
	uint32_t num_elems;		/**< Count of buffer IDs specified in array */
	uint32_t buffer_id[];		/**< Array of buffer IDs from which Probe Points should be removed */
} __attribute__((packed, aligned(4)));

#endif /* __IPC_PROBE_H__ */
