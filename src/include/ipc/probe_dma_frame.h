/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/**
 * \file include/ipc/probe_dma_frame.h
 * \brief Audio probe extraction stream packet frame headers and format bitfield encoding.
 * \ingroup audio_probe_api
 */

#ifndef __IPC_PROBE_DMA_FRAME_H__
#define __IPC_PROBE_DMA_FRAME_H__

#include <rtos/bit.h>

/**
 * \brief Header for data packets sent via compressed PCM from extraction probes.
 * \ingroup audio_probe_api
 */
struct probe_data_packet {
	uint32_t sync_word;		/**< Magic synchronization marker (PROBE_EXTRACT_SYNC_WORD) */
	uint32_t buffer_id;		/**< Buffer ID from which data was extracted */
	uint32_t format;		/**< Encoded 32-bit audio data format */
	uint32_t timestamp_low;		/**< Low 32 bits of hardware timestamp in microseconds */
	uint32_t timestamp_high;	/**< High 32 bits of hardware timestamp in microseconds */
	uint32_t data_size_bytes;	/**< Byte length of extracted payload in data[] */
	uint8_t data[];			/**< Extracted audio samples */
} __attribute__((packed, aligned(4)));

/** Extraction packet sync marker (0xBABEBEBA) */
#define PROBE_EXTRACT_SYNC_WORD		0xBABEBEBA

/** \name Probe Extraction Audio Format Bitfield Shifts & Masks
 *  Bitfield encoding for probe extraction stream audio formats.
 *  @{
 */
#define PROBE_SHIFT_FMT_TYPE		31	/**< Type encoding shift (1 = standard) */
#define PROBE_SHIFT_STANDARD_TYPE	27	/**< Standard type shift (0 = audio) */
#define PROBE_SHIFT_AUDIO_FMT		23	/**< Audio format shift (0 = PCM) */
#define PROBE_SHIFT_SAMPLE_RATE		19	/**< Sample rate enumeration index shift */
#define PROBE_SHIFT_NB_CHANNELS		14	/**< Channel count minus 1 shift */
#define PROBE_SHIFT_SAMPLE_SIZE		12	/**< Valid sample bytes minus 1 shift */
#define PROBE_SHIFT_CONTAINER_SIZE	10	/**< Container bytes minus 1 shift */
#define PROBE_SHIFT_SAMPLE_FMT		9	/**< Sample format (0 = integer, 1 = float) */
#define PROBE_SHIFT_SAMPLE_END		8	/**< Endianness (0 = little-endian) */
#define PROBE_SHIFT_INTERLEAVING_ST	7	/**< Interleaving (1 = sample interleaved) */

#define PROBE_MASK_FMT_TYPE		MASK(31, 31)	/**< Type encoding mask */
#define PROBE_MASK_STANDARD_TYPE	MASK(30, 27)	/**< Standard type mask */
#define PROBE_MASK_AUDIO_FMT		MASK(26, 23)	/**< Audio format mask */
#define PROBE_MASK_SAMPLE_RATE		MASK(22, 19)	/**< Sample rate mask */
#define PROBE_MASK_NB_CHANNELS		MASK(18, 14)	/**< Channel count mask */
#define PROBE_MASK_SAMPLE_SIZE		MASK(13, 12)	/**< Sample size mask */
#define PROBE_MASK_CONTAINER_SIZE	MASK(11, 10)	/**< Container size mask */
#define PROBE_MASK_SAMPLE_FMT		MASK(9, 9)	/**< Sample format mask */
#define PROBE_MASK_SAMPLE_END		MASK(8, 8)	/**< Endianness mask */
#define PROBE_MASK_INTERLEAVING_ST	MASK(7, 7)	/**< Interleaving mask */
/** @} */

#endif /* __IPC_PROBE_DMA_FRAME_H__ */
