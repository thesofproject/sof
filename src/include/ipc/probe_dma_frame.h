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

#include <stdint.h>

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

#define PROBE_MASK_FMT_TYPE		0x80000000ULL	/**< Type encoding mask */
#define PROBE_MASK_STANDARD_TYPE	0x78000000ULL	/**< Standard type mask */
#define PROBE_MASK_AUDIO_FMT		0x07800000ULL	/**< Audio format mask */
#define PROBE_MASK_SAMPLE_RATE		0x00780000ULL	/**< Sample rate mask */
#define PROBE_MASK_NB_CHANNELS		0x0007c000ULL	/**< Channel count mask */
#define PROBE_MASK_SAMPLE_SIZE		0x00003000ULL	/**< Sample size mask */
#define PROBE_MASK_CONTAINER_SIZE	0x00000c00ULL	/**< Container size mask */
#define PROBE_MASK_SAMPLE_FMT		0x00000200ULL	/**< Sample format mask */
#define PROBE_MASK_SAMPLE_END		0x00000100ULL	/**< Endianness mask */
#define PROBE_MASK_INTERLEAVING_ST	0x00000080ULL	/**< Interleaving mask */
/** @} */

#endif /* __IPC_PROBE_DMA_FRAME_H__ */
