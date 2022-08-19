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
	uint32_t data_size_bytes;	/**< Size of following audio data */
	uint32_t data[];		/**< Audio data extracted from buffer */
} __attribute__((packed, aligned(4)));

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
