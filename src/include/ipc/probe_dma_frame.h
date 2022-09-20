/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __IPC_PROBE_DMA_FRAME_H__
#define __IPC_PROBE_DMA_FRAME_H__

#include <rtos/bit.h>

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
	uint8_t data[];			/**< Audio data extracted from buffer */
} __attribute__((packed, aligned(4)));

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

#endif
