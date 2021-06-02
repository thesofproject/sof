/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
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
 * \file include/ipc4/base-config.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_BASE_CONFIG_H__
#define __SOF_IPC4_BASE_CONFIG_H__

#include <stdint.h>

enum ipc4_sampling_frequency {
	FS_8000HZ	= 8000,
	FS_11025HZ	= 11025,
	FS_12000HZ	= 12000, /**< Mp3, AAC, SRC only. */
	FS_16000HZ	= 16000,
	FS_18900HZ	= 18900, /**< SRC only for 44100 */
	FS_22050HZ	= 22050,
	FS_24000HZ	= 24000, /**< Mp3, AAC, SRC only. */
	FS_32000HZ	= 32000,
	FS_37800HZ	= 37800, /**< SRC only for 44100 */
	FS_44100HZ	= 44100,
	FS_48000HZ	= 48000, /**< Default. */
	FS_64000HZ	= 64000, /**< AAC, SRC only. */
	FS_88200HZ	= 88200, /**< AAC, SRC only. */
	FS_96000HZ	= 96000, /**< AAC, SRC only. */
	FS_176400HZ = 176400, /**< SRC only. */
	FS_192000HZ = 192000, /**< SRC only. */
	FS_INVALID
};

enum ipc4_bit_depth {
	DEPTH_8BIT	= 8, /**< 8 bits depth */
	DEPTH_16BIT = 16, /**< 16 bits depth */
	DEPTH_24BIT = 24, /**< 24 bits depth - Default */
	DEPTH_32BIT = 32, /**< 32 bits depth */
	DEPTH_64BIT = 64, /**< 64 bits depth */
	DEPTH_INVALID
};

enum ipc4_channel_config {
	CHANNEL_CONFIG_MONO	= 0, /**< one channel only. */
	CHANNEL_CONFIG_STEREO	 = 1, /**< L & R. */
	CHANNEL_CONFIG_2_POINT_1 = 2, /**< L, R & LFE; PCM only. */
	CHANNEL_CONFIG_3_POINT_0 = 3, /**< L, C & R; MP3 & AAC only. */
	CHANNEL_CONFIG_3_POINT_1 = 4, /**< L, C, R & LFE; PCM only. */
	CHANNEL_CONFIG_QUATRO	 = 5, /**< L, R, Ls & Rs; PCM only. */
	CHANNEL_CONFIG_4_POINT_0 = 6, /**< L, C, R & Cs; MP3 & AAC only. */
	CHANNEL_CONFIG_5_POINT_0 = 7, /**< L, C, R, Ls & Rs. */
	CHANNEL_CONFIG_5_POINT_1 = 8, /**< L, C, R, Ls, Rs & LFE. */
	CHANNEL_CONFIG_DUAL_MONO = 9, /**< one channel replicated in two. */
	/**< Stereo (L,R) in 4 slots, 1st stream: [ L, R, -, - ] */
	CHANNEL_CONFIG_I2S_DUAL_STEREO_0 = 10,
	/**< Stereo (L,R) in 4 slots, 2nd stream: [ -, -, L, R ] */
	CHANNEL_CONFIG_I2S_DUAL_STEREO_1 = 11,
	CHANNEL_CONFIG_7_POINT_1 = 12, /**< L, C, R, Ls, Rs & LFE., LS, RS */
	CHANNEL_CONFIG_INVALID
};

enum ipc4_interleaved_style {
	CHANNELS_INTERLEAVED = 0,
	CHANNELS_NONINTERLEAVED = 1,
};

enum ipc4_sample_type {
	MSB_INTEGER = 0, /**< integer with Most Significant Byte first */
	LSB_INTEGER = 1, /**< integer with Least Significant Byte first */
	SIGNED_INTEGER = 2, /**< signed integer */
	UNSIGNED_INTEGER = 3, /**< unsigned integer */
	FLOAT = 4 /**< unsigned integer */
};

struct ipc4_audio_format {
	enum ipc4_sampling_frequency sampling_frequency;
	enum ipc4_bit_depth depth;
	uint32_t ch_map;
	enum ipc4_channel_config ch_cfg;
	uint32_t interleaving_style;
	uint32_t channels_count : 8;
	uint32_t valid_bit_depth : 8;
	enum ipc4_sample_type s_type : 8;
	uint32_t reserved : 8;
};

struct BaseModuleCfg {
	uint32_t cpc;	/**< the max count of Cycles Per Chunk processing */
	uint32_t ibs;	/**< input Buffer Size (in bytes)  */
	uint32_t obs;  /**< output Buffer Size (in bytes) */
	uint32_t is_pages;	/**< number of physical pages used */
	struct ipc4_audio_format audio_fmt;
};

struct ipc4_input_pin_format {
	uint32_t			pin_index; /*!< \brief Index of the pin.*/
	uint32_t			ibs;	   /*!< \brief Specifies input frame size (in bytes).*/
	struct ipc4_audio_format 		audio_fmt; /*!< \brief Format of the input data.*/
};

struct ipc4_output_pin_format {
	uint32_t			pin_index; /*!< \brief Index of the pin.*/
	uint32_t			obs;	   /*!< \brief Specifies output frame size (in bytes).*/
	struct ipc4_audio_format 		audio_fmt; /*!< \brief Format of the output data.*/
};

enum ipc4_stream_type {
	ipc4_pcm = 0,                   ///< PCM stream
	ipc4_mp3,                       ///< MP3 encoded stream
	ipc4_aac,                       ///< AAC encoded stream
	ipc4_max,
	ipc4_invalid = 0xFF
};
#endif
