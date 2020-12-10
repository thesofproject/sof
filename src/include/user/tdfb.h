/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __USER_TDFB_H__
#define __USER_TDFB_H__

#include <stdint.h>

#define SOF_TDFB_MAX_SIZE 4096		/* Max size for coef data in bytes */
#define SOF_TDFB_FIR_MAX_LENGTH 256	/* Max length for individual filter */
#define SOF_TDFB_FIR_MAX_COUNT 16	/* A blob can define max 16 FIR EQs */
#define SOF_TDFB_MAX_STREAMS 8		/* Support 1..8 sinks */
#define SOF_TDFB_MAX_ANGLES 360		/* Up to 1 degree precision for 360 degrees coverage */
#define SOF_TDFB_MAX_MICROPHONES 16	/* Up to 16 microphone locations */

/*
 * sof_tdfb_config data[]

 * int16_t fir_filter1[length_filter1];  Multiple of 4 taps and 32 bit align
 * int16_t fir_filter2[length_filter2];  Multiple of 4 taps and 32 bit align
 *		...
 * int16_t fir_filterN[length_filterN];  Multiple of 4 taps and 32 bit align
 * int16_t input_channel_select[num_filters];  0 = ch0, 1 = 1ch1, ..
 * int16_t output_channel_mix[num_filters];
 * int16_t output_stream_mix[num_filters];
 *
 */

struct sof_tdfb_config {
	uint32_t size;			/* Size of entire struct */
	uint16_t num_filters;		/* Total number of filters */
	uint16_t num_output_channels;   /* Total number of output channels */
	uint16_t num_output_streams;	/* one source, N output sinks */
	uint16_t reserved16;		/* To keep data 32 bit aligned */

	/* Since ABI version 3.X.1 */
	uint16_t num_mic_locations;	/* Number of microphones locations entries */
	uint16_t num_angles;		/* Number of steer angles in data, not counting beam off */
	uint16_t beam_off_defined;	/* Set if a beam off filters configuration is present */
	uint16_t track_doa;		/* Track direction of arrival angle */
	int16_t angle_enum_mult;	/* Multiply enum value (0..15) to get angle in degrees */
	int16_t angle_enum_offs;	/* After multiplication add this degrees offset to angle */

	/* reserved */
	uint32_t reserved32[1];		/* For future */

	int16_t data[];
} __attribute__((packed));

struct sof_tdfb_angle {
	int16_t azimuth;
	int16_t elevation;
	int16_t filter_index;
	int16_t reserved;
} __attribute__((packed));

struct sof_tdfb_mic_location {
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t reserved;
} __attribute__((packed));

#endif /* __USER_TDFB_H__ */
