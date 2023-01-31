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
	IPC4_FS_8000HZ = 8000,
	IPC4_FS_11025HZ = 11025,
	IPC4_FS_12000HZ = 12000, /**< Mp3, AAC, SRC only */
	IPC4_FS_16000HZ = 16000,
	IPC4_FS_18900HZ = 18900, /**< SRC only for 44100 */
	IPC4_FS_22050HZ = 22050,
	IPC4_FS_24000HZ = 24000, /**< Mp3, AAC, SRC only */
	IPC4_FS_32000HZ = 32000,
	IPC4_FS_37800HZ = 37800, /**< SRC only for 44100 */
	IPC4_FS_44100HZ = 44100,
	IPC4_FS_48000HZ = 48000, /**< Default */
	IPC4_FS_64000HZ = 64000, /**< AAC, SRC only */
	IPC4_FS_88200HZ = 88200, /**< AAC, SRC only */
	IPC4_FS_96000HZ = 96000, /**< AAC, SRC only */
	IPC4_FS_176400HZ = 176400, /**< SRC only */
	IPC4_FS_192000HZ = 192000, /**< SRC only */
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
	IPC4_CHANNEL_CONFIG_MONO = 0,		/**< one channel only */
	IPC4_CHANNEL_CONFIG_STEREO = 1,		/**< L & R */
	IPC4_CHANNEL_CONFIG_2_POINT_1 = 2,	/**< L, R & LFE; PCM only */
	IPC4_CHANNEL_CONFIG_3_POINT_0 = 3,	/**< L, C & R; MP3 & AAC only */
	IPC4_CHANNEL_CONFIG_3_POINT_1 = 4,	/**< L, C, R & LFE; PCM only */
	IPC4_CHANNEL_CONFIG_QUATRO = 5,		/**< L, R, Ls & Rs; PCM only */
	IPC4_CHANNEL_CONFIG_4_POINT_0 = 6,	/**< L, C, R & Cs; MP3 & AAC only */
	IPC4_CHANNEL_CONFIG_5_POINT_0 = 7,	/**< L, C, R, Ls & Rs */
	IPC4_CHANNEL_CONFIG_5_POINT_1 = 8,	/**< L, C, R, Ls, Rs & LFE */
	IPC4_CHANNEL_CONFIG_DUAL_MONO = 9,	/**< one channel replicated in two */
	/**< Stereo (L,R) in 4 slots, 1st stream: [ L, R, -, - ] */
	IPC4_CHANNEL_CONFIG_I2S_DUAL_STEREO_0 = 10,
	/**< Stereo (L,R) in 4 slots, 2nd stream: [ -, -, L, R ] */
	IPC4_CHANNEL_CONFIG_I2S_DUAL_STEREO_1 = 11,
	IPC4_CHANNEL_CONFIG_7_POINT_1 = 12,	/**< L, C, R, Ls, Rs & LFE., LS, RS */
	IPC4_CHANNEL_CONFIG_INVALID
};

enum ipc4_channel_index {
	CHANNEL_LEFT            = 0,
	CHANNEL_CENTER          = 1,
	CHANNEL_RIGHT           = 2,
	CHANNEL_LEFT_SURROUND   = 3,
	CHANNEL_CENTER_SURROUND = 3,
	CHANNEL_RIGHT_SURROUND  = 4,
	CHANNEL_LEFT_SIDE       = 5,
	CHANNEL_RIGHT_SIDE      = 6,
	CHANNEL_LFE             = 7,
	CHANNEL_INVALID         = 0xF,
};

enum ipc4_interleaved_style {
	IPC4_CHANNELS_INTERLEAVED = 0,
	IPC4_CHANNELS_NONINTERLEAVED = 1,
};

enum ipc4_sample_type {
	IPC4_TYPE_MSB_INTEGER = 0,	/**< integer with Most Significant Byte first */
	IPC4_TYPE_LSB_INTEGER = 1,	/**< integer with Least Significant Byte first */
	IPC4_TYPE_SIGNED_INTEGER = 2,
	IPC4_TYPE_UNSIGNED_INTEGER = 3,
	IPC4_TYPE_FLOAT = 4
};

enum ipc4_stream_type {
	IPC4_STREAM_PCM = 0,	/**< PCM stream */
	IPC4_STREAM_MP3 = 1,	/**< MP3 encoded stream */
	IPC4_STREAM_AAC = 2,	/**< AAC encoded stream */
	/* TODO: revisit max stream type count. Currently
	 * it aligns with windows audio driver and we will
	 * update all when more types are supported
	 */
	IPC4_STREAM_COUNT = 3,
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
	uint32_t pin_index;	/**< index of the pin */
	uint32_t ibs;		/**< specifies input frame size (in bytes) */
	struct ipc4_audio_format audio_fmt; /**< format of the input data */
} __attribute__((packed, aligned(4)));

struct ipc4_output_pin_format {
	uint32_t pin_index;	/**< index of the pin */
	uint32_t obs;		/**< specifies output frame size (in bytes) */
	struct ipc4_audio_format audio_fmt; /**< format of the output data */
} __attribute__((packed, aligned(4)));

struct ipc4_base_module_cfg_ext {
	/* specifies number of items in input_pins array. Maximum size is 8 */
	uint16_t nb_input_pins;
	/* specifies number of items in output_pins array. Maximum size is 8 */
	uint16_t nb_output_pins;
	uint8_t reserved[12];
	/* Specifies format of input pins followed by output pins.
	 * Pin format arrays may be non-continuous i.e. may contain pin #0 format
	 * followed by pin #2 format in case pin #1 will not be in use.
	 * FW assigned format of the pin based on pin_index, not on a position of
	 * the item in the array. Applies to both input and output pins.
	 */
	uint8_t pin_formats[];
} __attribute__((packed, aligned(4)));

#define ipc4_calc_base_module_cfg_ext_size(in_pins, out_pins)		\
		(sizeof(struct ipc4_base_module_cfg_ext) +		\
		 (in_pins) * sizeof(struct ipc4_input_pin_format) +	\
		 (out_pins) * sizeof(struct ipc4_output_pin_format))

/* Struct to combine the base_cfg and base_cfg_ext for easier parsing */
struct ipc4_base_module_extended_cfg {
	struct ipc4_base_module_cfg base_cfg;
	struct ipc4_base_module_cfg_ext base_cfg_ext;
} __attribute__((packed, aligned(4)));

/* This enum defines short 16bit parameters common for all modules.
 * Value of module specific parameters have to be less than 0x3000.
 */
enum ipc4_base_module_params {
	/* handled inside LargeConfigGet of module instance */
	IPC4_MOD_INST_PROPS  = 0xFE,
	/* handled inside ConfigSet of module instance */
	IPC4_MOD_INST_ENABLE = 0x3000
};

struct ipc4_pin_props {
	/* type of the connected stream. */
	enum ipc4_stream_type stream_type;

	/* audio format of the stream. The content is valid in case of ePcm stream_type. */
	struct ipc4_audio_format format;

	/* unique ID of the physical queue connected to the pin.
	 * If there is no queue connected, then -1 (invalid queue ID) is set
	 */
	uint32_t phys_queue_id;
} __attribute__((packed, aligned(4)));

struct ipc4_pin_list_info {
	uint32_t pin_count;
	struct ipc4_pin_props pin_info[1];
} __attribute__((packed, aligned(4)));

/* structure describing module instance properties used in response
 * to module LargeConfigGet with MOD_INST_PROPS parameter.
 */
struct ipc4_module_instance_props {
	uint32_t  id;
	uint32_t  dp_queue_type;
	uint32_t  queue_alignment;
	uint32_t  cp_usage_mask;
	uint32_t  stack_bytes;
	uint32_t  bss_total_bytes;
	uint32_t  bss_used_bytes;
	uint32_t  ibs_bytes;
	uint32_t  obs_bytes;
	uint32_t  cpc;
	uint32_t  cpc_peak;
	struct ipc4_pin_list_info input_queues;
	struct ipc4_pin_list_info output_queues;
	uint32_t  input_gateway;
	uint32_t  output_gateway;
} __attribute__((packed, aligned(4)));

/* Reflects the last two entries in ModuleInstanceProps sttructure */
struct ipc4_in_out_gateway {
	uint32_t  input_gateway;
	uint32_t  output_gateway;
} __attribute__((packed, aligned(4)));

/* this structure may be used by modules to carry
 * short 16bit parameters as part of the IxC register content.
 */
union ipc4_cfg_param_id_data {
	uint32_t dw;
	struct {
		uint32_t data16 : 16;   /* Input/Output small config data */
		uint32_t id     : 14;   /* input parameter ID */
		uint32_t _rsvd  : 2;
	} f;
} __attribute__((packed, aligned(4)));

#endif
