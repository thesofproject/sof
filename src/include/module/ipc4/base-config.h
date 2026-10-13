/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2021 - 2023 Intel Corporation. All rights reserved.
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
 * \file include/module/ipc4/base-config.h
 * \brief IPC4 audio formats, sampling frequencies, bit depths, and pin descriptors.
 * \ingroup ipc4_format_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __MODULE_IPC4_BASE_CONFIG_H__
#define __MODULE_IPC4_BASE_CONFIG_H__

#include <stdint.h>

/**
 * \brief Standard PCM and encoded audio sampling frequencies supported by IPC4.
 * \ingroup ipc4_format_api
 */
enum ipc4_sampling_frequency {
	IPC4_FS_8000HZ = 8000,   /**< 8.0 kHz sampling rate. */
	IPC4_FS_11025HZ = 11025, /**< 11.025 kHz sampling rate. */
	IPC4_FS_12000HZ = 12000, /**< 12.0 kHz (MP3, AAC, SRC only). */
	IPC4_FS_16000HZ = 16000, /**< 16.0 kHz sampling rate (standard voice/telephony). */
	IPC4_FS_18900HZ = 18900, /**< 18.9 kHz (SRC only for 44.1 kHz fractional rates). */
	IPC4_FS_22050HZ = 22050, /**< 22.05 kHz sampling rate. */
	IPC4_FS_24000HZ = 24000, /**< 24.0 kHz (MP3, AAC, SRC only). */
	IPC4_FS_32000HZ = 32000, /**< 32.0 kHz sampling rate. */
	IPC4_FS_37800HZ = 37800, /**< 37.8 kHz (SRC only for 44.1 kHz fractional rates). */
	IPC4_FS_44100HZ = 44100, /**< 44.1 kHz CD audio sampling rate. */
	IPC4_FS_48000HZ = 48000, /**< 48.0 kHz standard DSP audio rate (Default). */
	IPC4_FS_64000HZ = 64000, /**< 64.0 kHz (AAC, SRC only). */
	IPC4_FS_88200HZ = 88200, /**< 88.2 kHz high-resolution audio. */
	IPC4_FS_96000HZ = 96000, /**< 96.0 kHz high-resolution audio. */
	IPC4_FS_176400HZ = 176400, /**< 176.4 kHz studio master rate. */
	IPC4_FS_192000HZ = 192000, /**< 192.0 kHz studio master rate. */
	IPC4_FS_INVALID          /**< Sentinel for invalid or uninitialized sampling rate. */
};

/**
 * \brief Container bit depth for sample storage in memory.
 * \ingroup ipc4_format_api
 */
enum ipc4_bit_depth {
	IPC4_DEPTH_8BIT	= 8,   /**< 8-bit sample container depth. */
	IPC4_DEPTH_16BIT = 16, /**< 16-bit sample container depth. */
	IPC4_DEPTH_24BIT = 24, /**< 24-bit sample container depth (Default). */
	IPC4_DEPTH_32BIT = 32, /**< 32-bit sample container depth. */
	IPC4_DEPTH_64BIT = 64, /**< 64-bit sample container depth. */
	IPC4_DEPTH_INVALID     /**< Sentinel for invalid bit depth. */
};

/**
 * \brief Audio channel configurations and speaker mappings.
 * \ingroup ipc4_format_api
 */
enum ipc4_channel_config {
	IPC4_CHANNEL_CONFIG_MONO = 0,		/**< Single mono channel. */
	IPC4_CHANNEL_CONFIG_STEREO = 1,		/**< Standard stereo (Left & Right). */
	IPC4_CHANNEL_CONFIG_2_POINT_1 = 2,	/**< 2.1 Audio: Left, Right & LFE (PCM only). */
	IPC4_CHANNEL_CONFIG_3_POINT_0 = 3,	/**< 3.0 Audio: Left, Center & Right (MP3 & AAC only). */
	IPC4_CHANNEL_CONFIG_3_POINT_1 = 4,	/**< 3.1 Audio: Left, Center, Right & LFE (PCM only). */
	IPC4_CHANNEL_CONFIG_QUATRO = 5,		/**< 4.0 Surround: Left, Right, Left Surround & Right Surround. */
	IPC4_CHANNEL_CONFIG_4_POINT_0 = 6,	/**< 4.0 Cinema: Left, Center, Right & Center Surround. */
	IPC4_CHANNEL_CONFIG_5_POINT_0 = 7,	/**< 5.0 Surround: Left, Center, Right, Left Surround & Right Surround. */
	IPC4_CHANNEL_CONFIG_5_POINT_1 = 8,	/**< 5.1 Surround: Left, Center, Right, Left Surround, Right Surround & LFE. */
	IPC4_CHANNEL_CONFIG_DUAL_MONO = 9,	/**< Dual mono: single logical channel duplicated into two slots. */
	IPC4_CHANNEL_CONFIG_I2S_DUAL_STEREO_0 = 10, /**< Stereo (L,R) in 4 slots, 1st stream: [ L, R, -, - ]. */
	IPC4_CHANNEL_CONFIG_I2S_DUAL_STEREO_1 = 11, /**< Stereo (L,R) in 4 slots, 2nd stream: [ -, -, L, R ]. */
	IPC4_CHANNEL_CONFIG_7_POINT_1 = 12,	/**< 7.1 Surround: L, C, R, Ls, Rs, LFE, Left Side & Right Side. */
	IPC4_CHANNEL_CONFIG_INVALID         /**< Sentinel for unsupported channel configuration. */
};

/**
 * \brief Logical channel position indices used in channel maps.
 * \ingroup ipc4_format_api
 */
enum ipc4_channel_index {
	CHANNEL_LEFT            = 0,   /**< Front Left channel. */
	CHANNEL_CENTER          = 1,   /**< Front Center channel. */
	CHANNEL_RIGHT           = 2,   /**< Front Right channel. */
	CHANNEL_LEFT_SURROUND   = 3,   /**< Left Surround channel. */
	CHANNEL_CENTER_SURROUND = 3,   /**< Center Surround channel. */
	CHANNEL_RIGHT_SURROUND  = 4,   /**< Right Surround channel. */
	CHANNEL_LEFT_SIDE       = 5,   /**< Left Side Surround channel. */
	CHANNEL_RIGHT_SIDE      = 6,   /**< Right Side Surround channel. */
	CHANNEL_LFE             = 7,   /**< Low Frequency Effects (subwoofer) channel. */
	CHANNEL_INVALID         = 0xF, /**< Unmapped or inactive channel slot. */
};

/**
 * \brief Sample interleaving styles.
 * \ingroup ipc4_format_api
 */
enum ipc4_interleaved_style {
	IPC4_CHANNELS_INTERLEAVED = 0,    /**< Channels interleaved sample-by-sample (L0, R0, L1, R1...). */
	IPC4_CHANNELS_NONINTERLEAVED = 1, /**< Planar non-interleaved channels (all Left samples, then all Right). */
};

/**
 * \brief Audio sample representation and arithmetic type.
 * \ingroup ipc4_format_api
 */
enum ipc4_sample_type {
	IPC4_TYPE_MSB_INTEGER = 0,	/**< Integer with Most Significant Byte first. */
	IPC4_TYPE_LSB_INTEGER = 1,	/**< Integer with Least Significant Byte first. */
	IPC4_TYPE_SIGNED_INTEGER = 2,   /**< Standard 2's complement signed integer (e.g. Q1.15, Q1.23, Q1.31). */
	IPC4_TYPE_UNSIGNED_INTEGER = 3, /**< Unsigned integer PCM. */
	IPC4_TYPE_FLOAT = 4,            /**< 32-bit IEEE 754 floating point. */
	IPC4_TYPE_A_LAW = 5,            /**< ITU-T G.711 A-law companded audio. */
	IPC4_TYPE_MU_LAW = 6,           /**< ITU-T G.711 mu-law companded audio. */
};

/**
 * \brief Stream container encoding type.
 * \ingroup ipc4_format_api
 */
enum ipc4_stream_type {
	IPC4_STREAM_PCM = 0,	/**< Linear Pulse Code Modulated (PCM) uncompressed audio. */
	IPC4_STREAM_MP3 = 1,	/**< MPEG-1 Audio Layer III encoded bitstream. */
	IPC4_STREAM_AAC = 2,	/**< Advanced Audio Coding encoded bitstream. */
	IPC4_STREAM_COUNT = 3,  /**< Total count of defined stream types. */
	IPC4_STREAM_INVALID = 0xFF /**< Sentinel for invalid stream type. */
};

/**
 * \brief Standard audio format descriptor across all IPC4 modules and gateways.
 * \ingroup ipc4_format_api
 */
struct ipc4_audio_format {
	uint32_t sampling_frequency;	/**< Sampling rate in Hz (one of enum ipc4_sampling_frequency). */
	uint32_t depth;			/**< Container bit depth in bits (one of enum ipc4_bit_depth). */
	uint32_t ch_map;		/**< 32-bit channel map nibbles (CHANNEL_LEFT, CHANNEL_RIGHT, etc.). */
	uint32_t ch_cfg;		/**< Channel topology configuration (one of enum ipc4_channel_config). */
	uint32_t interleaving_style;	/**< Sample layout (one of enum ipc4_interleaved_style). */
	uint32_t channels_count : 8;	/**< Active channel count (1 to 8). */
	uint32_t valid_bit_depth : 8;	/**< Number of significant valid bits within container (e.g. 24). */
	uint32_t s_type : 8;		/**< Sample representation type (one of enum ipc4_sample_type). */
	uint32_t reserved : 8;		/**< Reserved padding bits; must be set to 0. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Base module configuration parameters provided upon module instantiation.
 * \ingroup ipc4_format_api
 */
struct ipc4_base_module_cfg {
	uint32_t cpc;		/**< Maximum Cycles Per Chunk (CPC) compute budget required by module. */
	uint32_t ibs;		/**< Input Buffer Size (IBS) in bytes required per processing frame. */
	uint32_t obs;		/**< Output Buffer Size (OBS) in bytes produced per processing frame. */
	uint32_t is_pages;	/**< Number of physical 4KB memory pages allocated for module state. */
	struct ipc4_audio_format audio_fmt; /**< Primary audio stream format of the module. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Input pin format specification for multi-pin modules.
 * \ingroup ipc4_format_api
 */
struct ipc4_input_pin_format {
	uint32_t pin_index;	/**< Zero-based logical index of the input pin. */
	uint32_t ibs;		/**< Required input frame buffer size in bytes for this pin. */
	struct ipc4_audio_format audio_fmt; /**< Negotiated audio data format for this input pin. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Output pin format specification for multi-pin modules.
 * \ingroup ipc4_format_api
 */
struct ipc4_output_pin_format {
	uint32_t pin_index;	/**< Zero-based logical index of the output pin. */
	uint32_t obs;		/**< Output frame buffer size in bytes for this pin. */
	struct ipc4_audio_format audio_fmt; /**< Negotiated audio data format for this output pin. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Extended module configuration header describing dynamic input and output pins.
 * \ingroup ipc4_format_api
 */
struct ipc4_base_module_cfg_ext {
	/**< Number of input pin format entries following in pin_formats[] (maximum 8). */
	uint16_t nb_input_pins;
	/**< Number of output pin format entries following in pin_formats[] (maximum 8). */
	uint16_t nb_output_pins;
	uint8_t reserved[12]; /**< Reserved padding; must be set to 0. */
	/**<
	 * Variable-length array of input pin formats followed immediately by output pin formats.
	 * Formats are matched by pin_index rather than array index.
	 */
	uint8_t pin_formats[];
} __attribute__((packed, aligned(4)));

/**
 * \brief Helper macro to calculate serialized byte size of struct ipc4_base_module_cfg_ext.
 * \ingroup ipc4_format_api
 */
#define ipc4_calc_base_module_cfg_ext_size(in_pins, out_pins)		\
		(sizeof(struct ipc4_base_module_cfg_ext) +		\
		 (in_pins) * sizeof(struct ipc4_input_pin_format) +	\
		 (out_pins) * sizeof(struct ipc4_output_pin_format))

/**
 * \brief Combined base and extended module configuration payload for easy parsing.
 * \ingroup ipc4_format_api
 */
struct ipc4_base_module_extended_cfg {
	struct ipc4_base_module_cfg base_cfg;         /**< Standard single-pin base module config. */
	struct ipc4_base_module_cfg_ext base_cfg_ext; /**< Extended multi-pin format descriptors. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Common 16-bit parameter IDs supported across all IPC4 module instances.
 * \ingroup ipc4_format_api
 */
enum ipc4_base_module_params {
	/**< Retrieve module instance runtime memory and queue properties via LargeConfigGet. */
	IPC4_MOD_INST_PROPS  = 0xFE,
	/**< Enable or disable module instance processing via ConfigSet (0 = disable, 1 = enable). */
	IPC4_MOD_INST_ENABLE = 0x3000
};

/**
 * \brief Runtime properties of a module pin connection.
 * \ingroup ipc4_format_api
 */
struct ipc4_pin_props {
	/**< Stream type (one of enum ipc4_stream_type). */
	uint32_t stream_type;

	/**< Active audio format of the stream (valid when stream_type == IPC4_STREAM_PCM). */
	struct ipc4_audio_format format;

	/**< Physical DMA queue ID connected to this pin (-1 if unassigned). */
	uint32_t phys_queue_id;
} __attribute__((packed, aligned(4)));

/**
 * \brief List of active pin properties for a module instance.
 * \ingroup ipc4_format_api
 */
struct ipc4_pin_list_info {
	uint32_t pin_count;                  /**< Number of active pin property descriptors. */
	struct ipc4_pin_props pin_info[1];   /**< Array of pin properties. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Detailed module instance runtime state, memory consumption, and queue alignment.
 * \ingroup ipc4_format_api
 *
 * Returned in response to LargeConfigGet with parameter ID #IPC4_MOD_INST_PROPS.
 */
struct ipc4_module_instance_props {
	uint32_t  id;              /**< Unique module instance identifier. */
	uint32_t  dp_queue_type;   /**< Data processing queue type. */
	uint32_t  queue_alignment; /**< Buffer memory alignment in bytes required by DSP. */
	uint32_t  cp_usage_mask;   /**< Coprocessor / hardware accelerator usage bitmask. */
	uint32_t  stack_bytes;     /**< Stack memory allocated for module instance thread. */
	uint32_t  bss_total_bytes; /**< Total BSS data section size in bytes. */
	uint32_t  bss_used_bytes;  /**< Currently utilized BSS memory in bytes. */
	uint32_t  ibs_bytes;       /**< Input buffer size in bytes. */
	uint32_t  obs_bytes;       /**< Output buffer size in bytes. */
	uint32_t  cpc;             /**< Nominal Cycles Per Chunk compute requirement. */
	uint32_t  cpc_peak;        /**< Peak measured Cycles Per Chunk compute requirement. */
	struct ipc4_pin_list_info input_queues;  /**< Connected input queue descriptors. */
	struct ipc4_pin_list_info output_queues; /**< Connected output queue descriptors. */
	uint32_t  input_gateway;   /**< Input gateway node ID if attached directly to gateway. */
	uint32_t  output_gateway;  /**< Output gateway node ID if attached directly to gateway. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Direct gateway attachment identifiers for a module instance.
 * \ingroup ipc4_format_api
 */
struct ipc4_in_out_gateway {
	uint32_t  input_gateway;  /**< Node ID of attached input gateway (e.g. DMA stream). */
	uint32_t  output_gateway; /**< Node ID of attached output gateway (e.g. DMA stream). */
} __attribute__((packed, aligned(4)));

/**
 * \brief 16-bit small parameter register encoding for MOD_CONFIG_SET/GET.
 * \ingroup ipc4_format_api
 */
union ipc4_cfg_param_id_data {
	uint32_t dw; /**< Raw 32-bit register value. */
	struct {
		uint32_t data16 : 16;   /**< 16-bit parameter data payload. */
		uint32_t id     : 14;   /**< 14-bit parameter identifier. */
		uint32_t _rsvd  : 2;    /**< Reserved bits. */
	} f;
} __attribute__((packed, aligned(4)));

#endif /* __MODULE_IPC4_BASE_CONFIG_H__ */
