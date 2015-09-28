/* 
 * BSD See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Intel IPC ABI
 */

#ifndef __INCLUDE_UAPI_INTEL_IPC_H__
#define __INCLUDE_UAPI_INTEL_IPC_H__

#include <stdint.h>

#define IPC_INTEL_NO_CHANNELS		4
#define IPC_INTEL_MAX_DX_REGIONS		14
#define IPC_INTEL_DX_CONTEXT_SIZE        (640 * 1024)
#define IPC_INTEL_CHANNELS_ALL		0xffffffff

#define IPC_INTEL_FW_LOG_CONFIG_DWORDS	12
#define IPC_INTEL_GLOBAL_LOG		15

/**
 * Upfront defined maximum message size that is
 * expected by the in/out communication pipes in FW.
 */
#define IPC_INTEL_IPC_MAX_PAYLOAD_SIZE	400
#define IPC_INTEL_MAX_INFO_SIZE		64
#define IPC_INTEL_BUILD_HASH_LENGTH	40
#define IPC_INTEL_IPC_MAX_SHORT_PARAMETER_SIZE	500
#define WAVES_PARAM_COUNT		128
#define WAVES_PARAM_LINES		160

/* Stream Allocate Path ID */
enum ipc_intel_stream_path_id {
	IPC_INTEL_STREAM_PATH_SSP0_OUT = 0,
	IPC_INTEL_STREAM_PATH_SSP0_IN = 1,
	IPC_INTEL_STREAM_PATH_MAX_PATH_ID = 2,
};

/* Stream Allocate Stream Type */
enum ipc_intel_stream_type {
	IPC_INTEL_STREAM_TYPE_RENDER = 0,
	IPC_INTEL_STREAM_TYPE_SYSTEM = 1,
	IPC_INTEL_STREAM_TYPE_CAPTURE = 2,
	IPC_INTEL_STREAM_TYPE_LOOPBACK = 3,
	IPC_INTEL_STREAM_TYPE_MAX_STREAM_TYPE = 4,
};

/* Stream Allocate Stream Format */
enum ipc_intel_stream_format {
	IPC_INTEL_STREAM_FORMAT_PCM_FORMAT = 0,
	IPC_INTEL_STREAM_FORMAT_MP3_FORMAT = 1,
	IPC_INTEL_STREAM_FORMAT_AAC_FORMAT = 2,
	IPC_INTEL_STREAM_FORMAT_MAX_FORMAT_ID = 3,
};

/* Device ID */
enum ipc_intel_device_id {
	IPC_INTEL_DEVICE_SSP_0   = 0,
	IPC_INTEL_DEVICE_SSP_1   = 1,
};

/* Device Master Clock Frequency */
enum ipc_intel_device_mclk {
	IPC_INTEL_DEVICE_MCLK_OFF         = 0,
	IPC_INTEL_DEVICE_MCLK_FREQ_6_MHZ  = 1,
	IPC_INTEL_DEVICE_MCLK_FREQ_12_MHZ = 2,
	IPC_INTEL_DEVICE_MCLK_FREQ_24_MHZ = 3,
};

/* Device Clock Master */
enum ipc_intel_device_mode {
	IPC_INTEL_DEVICE_CLOCK_SLAVE   = 0,
	IPC_INTEL_DEVICE_CLOCK_MASTER  = 1,
	IPC_INTEL_DEVICE_TDM_CLOCK_MASTER = 2,
};

/* DX Power State */
enum ipc_intel_dx_state {
	IPC_INTEL_DX_STATE_D0     = 0,
	IPC_INTEL_DX_STATE_D1     = 1,
	IPC_INTEL_DX_STATE_D3     = 3,
	IPC_INTEL_DX_STATE_MAX	= 3,
};

/* Audio stream stage IDs */
enum ipc_intel_fx_stage_id {
	IPC_INTEL_STAGE_ID_WAVES = 0,
	IPC_INTEL_STAGE_ID_DTS   = 1,
	IPC_INTEL_STAGE_ID_DOLBY = 2,
	IPC_INTEL_STAGE_ID_BOOST = 3,
	IPC_INTEL_STAGE_ID_MAX_FX_ID
};

/* DX State Type */
enum ipc_intel_dx_type {
	IPC_INTEL_DX_TYPE_FW_IMAGE = 0,
	IPC_INTEL_DX_TYPE_MEMORY_DUMP = 1
};

/* Volume Curve Type*/
enum ipc_intel_volume_curve {
	IPC_INTEL_VOLUME_CURVE_NONE = 0,
	IPC_INTEL_VOLUME_CURVE_FADE = 1
};

/* Sample ordering */
enum ipc_intel_interleaving {
	IPC_INTEL_INTERLEAVING_PER_CHANNEL = 0,
	IPC_INTEL_INTERLEAVING_PER_SAMPLE  = 1,
};

/* Channel indices */
enum ipc_intel_channel_index {
	IPC_INTEL_CHANNEL_LEFT            = 0,
	IPC_INTEL_CHANNEL_CENTER          = 1,
	IPC_INTEL_CHANNEL_RIGHT           = 2,
	IPC_INTEL_CHANNEL_LEFT_SURROUND   = 3,
	IPC_INTEL_CHANNEL_CENTER_SURROUND = 3,
	IPC_INTEL_CHANNEL_RIGHT_SURROUND  = 4,
	IPC_INTEL_CHANNEL_LFE             = 7,
	IPC_INTEL_CHANNEL_INVALID         = 0xF,
};

/* List of supported channel maps. */
enum ipc_intel_channel_config {
	IPC_INTEL_CHANNEL_CONFIG_MONO      = 0, /* mono only. */
	IPC_INTEL_CHANNEL_CONFIG_STEREO    = 1, /* L & R. */
	IPC_INTEL_CHANNEL_CONFIG_2_POINT_1 = 2, /* L, R & LFE; PCM only. */
	IPC_INTEL_CHANNEL_CONFIG_3_POINT_0 = 3, /* L, C & R; MP3 & AAC only. */
	IPC_INTEL_CHANNEL_CONFIG_3_POINT_1 = 4, /* L, C, R & LFE; PCM only. */
	IPC_INTEL_CHANNEL_CONFIG_QUATRO    = 5, /* L, R, Ls & Rs; PCM only. */
	IPC_INTEL_CHANNEL_CONFIG_4_POINT_0 = 6, /* L, C, R & Cs; MP3 & AAC only. */
	IPC_INTEL_CHANNEL_CONFIG_5_POINT_0 = 7, /* L, C, R, Ls & Rs. */
	IPC_INTEL_CHANNEL_CONFIG_5_POINT_1 = 8, /* L, C, R, Ls, Rs & LFE. */
	IPC_INTEL_CHANNEL_CONFIG_DUAL_MONO = 9, /* One channel replicated in two. */
	IPC_INTEL_CHANNEL_CONFIG_INVALID,
};

/* List of supported bit depths. */
enum ipc_intel_bitdepth {
	IPC_INTEL_DEPTH_8BIT  = 8,
	IPC_INTEL_DEPTH_16BIT = 16,
	IPC_INTEL_DEPTH_24BIT = 24, /* Default. */
	IPC_INTEL_DEPTH_32BIT = 32,
	IPC_INTEL_DEPTH_INVALID = 33,
};

enum ipc_intel_module_id {
	IPC_INTEL_MODULE_BASE_FW = 0x0,
	IPC_INTEL_MODULE_MP3     = 0x1,
	IPC_INTEL_MODULE_AAC_5_1 = 0x2,
	IPC_INTEL_MODULE_AAC_2_0 = 0x3,
	IPC_INTEL_MODULE_SRC     = 0x4,
	IPC_INTEL_MODULE_WAVES   = 0x5,
	IPC_INTEL_MODULE_DOLBY   = 0x6,
	IPC_INTEL_MODULE_BOOST   = 0x7,
	IPC_INTEL_MODULE_LPAL    = 0x8,
	IPC_INTEL_MODULE_DTS     = 0x9,
	IPC_INTEL_MODULE_PCM_CAPTURE = 0xA,
	IPC_INTEL_MODULE_PCM_SYSTEM = 0xB,
	IPC_INTEL_MODULE_PCM_REFERENCE = 0xC,
	IPC_INTEL_MODULE_PCM = 0xD,
	IPC_INTEL_MODULE_BLUETOOTH_RENDER_MODULE = 0xE,
	IPC_INTEL_MODULE_BLUETOOTH_CAPTURE_MODULE = 0xF,
	IPC_INTEL_MAX_MODULE_ID,
};

enum ipc_intel_performance_action {
	IPC_INTEL_PERF_START = 0,
	IPC_INTEL_PERF_STOP = 1,
};

struct ipc_intel_transfer_info {
	uint32_t destination;       /* destination address */
	uint32_t reverse:1;         /* if 1 data flows from destination */
	uint32_t size:31;           /* transfer size in bytes.*/
	uint16_t first_page_offset; /* offset to data in the first page. */
	uint8_t  packed_pages;   /* page addresses. Each occupies 20 bits */
} __attribute__((packed));

struct ipc_intel_transfer_list {
	uint32_t transfers_count;
	struct ipc_intel_transfer_info transfers;
} __attribute__((packed));

struct ipc_intel_transfer_parameter {
	uint32_t parameter_id;
	uint32_t data_size;
	union {
		uint8_t data[1];
		struct ipc_intel_transfer_list transfer_list;
	};
} __attribute__((packed));

/* SST firmware module info */
struct ipc_intel_module_info {
	u8 name[IPC_INTEL_MAX_INFO_SIZE];
	u8 version[IPC_INTEL_MAX_INFO_SIZE];
} __attribute__((packed));

/* Module entry point */
struct ipc_intel_module_entry {
	enum ipc_intel_module_id module_id;
	u32 entry_point;
} __attribute__((packed));

/* Module map - alignement matches DSP */
struct ipc_intel_module_map {
	u8 module_entries_count;
	struct ipc_intel_module_entry module_entries[1];
} __attribute__((packed));

struct ipc_intel_memory_info {
	u32 offset;
	u32 size;
} __attribute__((packed));

struct ipc_intel_fx_enable {
	struct ipc_intel_module_map module_map;
	struct ipc_intel_memory_info persistent_mem;
} __attribute__((packed));

struct ipc_intel_ipc_module_config {
	struct ipc_intel_module_map map;
	struct ipc_intel_memory_info persistent_mem;
	struct ipc_intel_memory_info scratch_mem;
} __attribute__((packed));

struct ipc_intel_get_fx_param {
	u32 parameter_id;
	u32 param_size;
} __attribute__((packed));

struct ipc_intel_perf_action {
	u32 action;
} __attribute__((packed));

struct ipc_intel_perf_data {
	u64 timestamp;
	u64 cycles;
	u64 datatime;
} __attribute__((packed));

/* FW version */
struct ipc_intel_ipc_fw_version {
	u8 build;
	u8 minor;
	u8 major;
	u8 type;
	u8 fw_build_hash[IPC_INTEL_BUILD_HASH_LENGTH];
	u32 fw_log_providers_hash;
} __attribute__((packed));

/* Stream ring info */
struct ipc_intel_ipc_stream_ring {
	u32 ring_pt_address;
	u32 num_pages;
	u32 ring_size;
	u32 ring_offset;
	u32 ring_first_pfn;
} __attribute__((packed));

/* Debug Dump Log Enable Request */
struct ipc_intel_ipc_debug_log_enable_req {
	struct ipc_intel_ipc_stream_ring ringinfo;
	u32 config[IPC_INTEL_FW_LOG_CONFIG_DWORDS];
} __attribute__((packed));

/* Debug Dump Log Reply */
struct ipc_intel_ipc_debug_log_reply {
	u32 log_buffer_begining;
	u32 log_buffer_size;
} __attribute__((packed));

/* Stream glitch position */
struct ipc_intel_ipc_stream_glitch_position {
	u32 glitch_type;
	u32 present_pos;
	u32 write_pos;
} __attribute__((packed));

/* Stream get position */
struct ipc_intel_ipc_stream_get_position {
	u32 position;
	u32 fw_cycle_count;
} __attribute__((packed));

/* Stream set position */
struct ipc_intel_ipc_stream_set_position {
	u32 position;
	u32 end_of_buffer;
} __attribute__((packed));

/* Stream Free Request */
struct ipc_intel_ipc_stream_free_req {
	u8 stream_id;
	u8 reserved[3];
} __attribute__((packed));

/* Set Volume Request */
struct ipc_intel_ipc_volume_req {
	u32 channel;
	u32 target_volume;
	u64 curve_duration;
	u32 curve_type;
} __attribute__((packed));

/* Device Configuration Request */
struct ipc_intel_ipc_device_config_req {
	u32 ssp_interface;
	u32 clock_frequency;
	u32 mode;
	u16 clock_divider;
	u8 channels;
	u8 reserved;
} __attribute__((packed));

/* Audio Data formats */
struct ipc_intel_audio_data_format_ipc {
	u32 frequency;
	u32 bitdepth;
	u32 map;
	u32 config;
	u32 style;
	u8 ch_num;
	u8 valid_bit;
	u8 reserved[2];
} __attribute__((packed));

/* Stream Allocate Request */
struct ipc_intel_ipc_stream_alloc_req {
	u8 path_id;
	u8 stream_type;
	u8 format_id;
	u8 reserved;
	struct ipc_intel_audio_data_format_ipc format;
	struct ipc_intel_ipc_stream_ring ringinfo;
	struct ipc_intel_module_map map;
	struct ipc_intel_memory_info persistent_mem;
	struct ipc_intel_memory_info scratch_mem;
	u32 number_of_notifications;
} __attribute__((packed));

/* Stream Allocate Reply */
struct ipc_intel_ipc_stream_alloc_reply {
	u32 stream_hw_id;
	u32 mixer_hw_id; // returns rate ????
	u32 read_position_register_address;
	u32 presentation_position_register_address;
	u32 peak_meter_register_address[IPC_INTEL_NO_CHANNELS];
	u32 volume_register_address[IPC_INTEL_NO_CHANNELS];
} __attribute__((packed));

/* Get Mixer Stream Info */
struct ipc_intel_ipc_stream_info_reply {
	u32 mixer_hw_id;
	u32 peak_meter_register_address[IPC_INTEL_NO_CHANNELS];
	u32 volume_register_address[IPC_INTEL_NO_CHANNELS];
} __attribute__((packed));

/* DX State Request */
struct ipc_intel_ipc_dx_req {
	u8 state;
	u8 reserved[3];
} __attribute__((packed));

/* DX State Reply Memory Info Item */
struct ipc_intel_ipc_dx_memory_item {
	u32 offset;
	u32 size;
	u32 source;
} __attribute__((packed));

/* DX State Reply */
struct ipc_intel_ipc_dx_reply {
	u32 entries_no;
	struct ipc_intel_ipc_dx_memory_item mem_info[IPC_INTEL_MAX_DX_REGIONS];
} __attribute__((packed));

#endif
