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
	IPC4_FS_8000HZ	= 8000,
	IPC4_FS_11025HZ	= 11025,
	IPC4_FS_12000HZ	= 12000, /**< Mp3, AAC, SRC only. */
	IPC4_FS_16000HZ	= 16000,
	IPC4_FS_18900HZ	= 18900, /**< SRC only for 44100 */
	IPC4_FS_22050HZ	= 22050,
	IPC4_FS_24000HZ	= 24000, /**< Mp3, AAC, SRC only. */
	IPC4_FS_32000HZ	= 32000,
	IPC4_FS_37800HZ	= 37800, /**< SRC only for 44100 */
	IPC4_FS_44100HZ	= 44100,
	IPC4_FS_48000HZ	= 48000, /**< Default. */
	IPC4_FS_64000HZ	= 64000, /**< AAC, SRC only. */
	IPC4_FS_88200HZ	= 88200, /**< AAC, SRC only. */
	IPC4_FS_96000HZ	= 96000, /**< AAC, SRC only. */
	IPC4_FS_176400HZ = 176400, /**< SRC only. */
	IPC4_FS_192000HZ = 192000, /**< SRC only. */
	IPC4_FS_INVALID
};

enum ipc4_bit_depth {
	IPC4_DEPTH_8BIT	= 8, /**< 8 bits depth */
	IPC4_DEPTH_16BIT = 16, /**< 16 bits depth */
	IPC4_DEPTH_24BIT = 24, /**< 24 bits depth - Default */
	IPC4_DEPTH_32BIT = 32, /**< 32 bits depth */
	IPC4_DEPTH_64BIT = 64, /**< 64 bits depth */
	IPC4_DEPTH_INVALID
};

enum ipc4_channel_config {
	IPC4_CHANNEL_CONFIG_MONO = 0,		/**< one channel only. */
	IPC4_CHANNEL_CONFIG_STEREO = 1,		/**< L & R. */
	IPC4_CHANNEL_CONFIG_2_POINT_1 = 2,	/**< L, R & LFE; PCM only. */
	IPC4_CHANNEL_CONFIG_3_POINT_0 = 3,	/**< L, C & R; MP3 & AAC only. */
	IPC4_CHANNEL_CONFIG_3_POINT_1 = 4,	/**< L, C, R & LFE; PCM only. */
	IPC4_CHANNEL_CONFIG_QUATRO = 5,		/**< L, R, Ls & Rs; PCM only. */
	IPC4_CHANNEL_CONFIG_4_POINT_0 = 6,	/**< L, C, R & Cs; MP3 & AAC only. */
	IPC4_CHANNEL_CONFIG_5_POINT_0 = 7,	/**< L, C, R, Ls & Rs. */
	IPC4_CHANNEL_CONFIG_5_POINT_1 = 8,	/**< L, C, R, Ls, Rs & LFE. */
	IPC4_CHANNEL_CONFIG_DUAL_MONO = 9,	/**< one channel replicated in two. */
	/**< Stereo (L,R) in 4 slots, 1st stream: [ L, R, -, - ] */
	IPC4_CHANNEL_CONFIG_I2S_DUAL_STEREO_0 = 10,
	/**< Stereo (L,R) in 4 slots, 2nd stream: [ -, -, L, R ] */
	IPC4_CHANNEL_CONFIG_I2S_DUAL_STEREO_1 = 11,
	IPC4_CHANNEL_CONFIG_7_POINT_1 = 12,	/**< L, C, R, Ls, Rs & LFE., LS, RS */
	IPC4_CHANNEL_CONFIG_INVALID
};

enum ipc4_interleaved_style {
	IPC4_CHANNELS_INTERLEAVED = 0,
	IPC4_CHANNELS_NONINTERLEAVED = 1,
};

enum ipc4_sample_type {
	IPC4_TYPE_MSB_INTEGER = 0,	/**< integer with Most Significant Byte first */
	IPC4_TYPE_LSB_INTEGER = 1,	/**< integer with Least Significant Byte first */
	IPC4_TYPE_SIGNED_INTEGER = 2,	/**< signed integer */
	IPC4_TYPE_UNSIGNED_INTEGER = 3, /**< unsigned integer */
	IPC4_TYPE_FLOAT = 4		/**< unsigned integer */
};

enum ipc4_stream_type {
	IPC4_STREAM_PCM = 0,	/*!< PCM stream */
	IPC4_STREAM_MP3,	/*!< MP3 encoded stream */
	IPC4_STREAM_AAC,	/*!< AAC encoded stream */
	IPC4_STREAM_MAX,
	IPC4_STREAM_INVALID = 0xFF
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
} __attribute__((packed, aligned(4)));

struct ipc4_base_module_cfg {
	uint32_t cpc;		/**< the max count of Cycles Per Chunk processing */
	uint32_t ibs;		/**< input Buffer Size (in bytes)  */
	uint32_t obs;		/**< output Buffer Size (in bytes) */
	uint32_t is_pages;	/**< number of physical pages used */
	struct ipc4_audio_format audio_fmt;
} __attribute__((packed, aligned(4)));

struct ipc4_input_pin_format {
	uint32_t pin_index;	/*!< \brief Index of the pin.*/
	uint32_t ibs;		/*!< \brief Specifies input frame size (in bytes).*/
	struct ipc4_audio_format audio_fmt; /*!< \brief Format of the input data.*/
} __attribute__((packed, aligned(4)));

struct ipc4_output_pin_format {
	uint32_t pin_index;	/*!< \brief Index of the pin.*/
	uint32_t obs;		/*!< \brief Specifies output frame size (in bytes).*/
	struct ipc4_audio_format audio_fmt; /*!< \brief Format of the output data.*/
} __attribute__((packed, aligned(4)));

#endif
