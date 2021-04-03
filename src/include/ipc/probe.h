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
#include <sof/bit.h>
#include <stdint.h>

#define PROBE_PURPOSE_EXTRACTION	0x1
#define PROBE_PURPOSE_INJECTION		0x2

#define PROBE_EXTRACT_SYNC_WORD		0xBABEBEBA

/**
 * \brief Definitions of shifts and masks for format encoding in probe
 *	  extraction stream
 *
 * Audio format from extraction probes is encoded as 32 bit value. Following
 * graphic explains encoding.
 *
 * A|BBBB|CCCC|DDDD|EEEEE|FF|GG|H|I|J|XXXXXXX
 * A - 1 bit - Specifies Type Encoding - 1 for Standard encoding
 * B - 4 bits - Specify Standard Type - 0 for Audio
 * C - 4 bits - Specify Audio format - 0 for PCM
 * D - 4 bits - Specify Sample Rate - value enumerating standard sample rates:
 *				      8000 Hz		= 0x0
 *				      11025 Hz		= 0x1
 *				      12000 Hz		= 0x2
 *				      16000 Hz		= 0x3
 *				      22050 Hz		= 0x4
 *				      24000 Hz		= 0x5
 *				      32000 Hz		= 0x6
 *				      44100 Hz		= 0x7
 *				      48000 Hz		= 0x8
 *				      64000 Hz		= 0x9
 *				      88200 Hz		= 0xA
 *				      96000 Hz		= 0xB
 *				      128000 Hz		= 0xC
 *				      176400 Hz		= 0xD
 *				      192000 Hz		= 0xE
 *				      none of the above = 0xF
 * E - 5 bits - Specify Number of Channels minus 1
 * F - 2 bits - Specify Sample Size, number of valid sample bytes minus 1
 * G - 2 bits - Specify Container Size, number of container bytes minus 1
 * H - 1 bit - Specifies Sample Format - 0 for Integer, 1 for Floating point
 * I - 1 bit - Specifies Sample Endianness - 0 for LE
 * J - 1 bit - Specifies Interleaving - 1 for Sample Interleaving
 */
#define PROBE_SHIFT_FMT_TYPE		31
#define PROBE_SHIFT_STANDARD_TYPE	27
#define PROBE_SHIFT_AUDIO_FMT		23
#define PROBE_SHIFT_SAMPLE_RATE		19
#define PROBE_SHIFT_NB_CHANNELS		14
#define PROBE_SHIFT_SAMPLE_SIZE		12
#define PROBE_SHIFT_CONTAINER_SIZE	10
#define PROBE_SHIFT_SAMPLE_FMT		9
#define PROBE_SHIFT_SAMPLE_END		8
#define PROBE_SHIFT_INTERLEAVING_ST	7

#define PROBE_MASK_FMT_TYPE		MASK(31, 31)
#define PROBE_MASK_STANDARD_TYPE	MASK(30, 27)
#define PROBE_MASK_AUDIO_FMT		MASK(26, 23)
#define PROBE_MASK_SAMPLE_RATE		MASK(22, 19)
#define PROBE_MASK_NB_CHANNELS		MASK(18, 14)
#define PROBE_MASK_SAMPLE_SIZE		MASK(13, 12)
#define PROBE_MASK_CONTAINER_SIZE	MASK(11, 10)
#define PROBE_MASK_SAMPLE_FMT		MASK(9, 9)
#define PROBE_MASK_SAMPLE_END		MASK(8, 8)
#define PROBE_MASK_INTERLEAVING_ST	MASK(7, 7)

/**
 * Header for data packets sent via compressed PCM from extraction probes
 */
struct probe_data_packet {
	uint32_t sync_word;		/**< PROBE_EXTRACT_SYNC_WORD */
	uint32_t buffer_id;		/**< Buffer ID from which data was extracted */
	uint32_t format;		/**< Encoded data format */
	uint32_t timestamp_low;		/**< Low 32 bits of timestamp in us */
	uint32_t timestamp_high;	/**< High 32 bits of timestamp in us */
	uint32_t checksum;		/**< CRC32 of header and payload */
	uint32_t data_size_bytes;	/**< Size of following audio data */
	uint32_t data[];		/**< Audio data extracted from buffer */
} __attribute__((packed, aligned(4)));

/**
 * Description of probe dma
 */
struct probe_dma {
	uint32_t stream_tag;		/**< Stream tag associated with this DMA */
	uint32_t dma_buffer_size;	/**< Size of buffer associated with this DMA */
} __attribute__((packed, aligned(4)));

/**
 * Description of probe point
 */
struct probe_point {
	uint32_t buffer_id;	/**< ID of buffer to which probe is attached */
	uint32_t purpose;	/**< PROBE_PURPOSE_EXTRACTION or PROBE_PURPOSE_INJECTION */
	uint32_t stream_tag;	/**< Stream tag of DMA via which data will be provided for injection.
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
