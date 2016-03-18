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
#include <reef/mailbox.h>

#define IPC_INTEL_NO_CHANNELS		4
#define IPC_INTEL_MAX_DX_REGIONS		14
#define IPC_INTEL_DX_CONTEXT_SIZE        (640 * 1024)
#define IPC_INTEL_CHANNELS_ALL		0xffffffff

#define IPC_INTEL_FW_LOG_CONFIG_DWORDS	12
#define IPC_INTEL_GLOBAL_LOG		15

#define IPC_INTEL_PAGE_SIZE		4096
#define IPC_INTEL_PAGE_TABLE_SIZE	IPC_INTEL_PAGE_SIZE

/**
 * Upfront defined maximum message size that is
 * expected by the in/out communication pipes in FW.
 */
#define IPC_INTEL_IPC_MAX_PAYLOAD_SIZE	400
#define IPC_INTEL_MAX_INFO_SIZE		64
#define IPC_INTEL_BUILD_HASH_LENGTH	40
#define IPC_INTEL_IPC_MAX_SHORT_PARAMETER_SIZE	500

/* Global Message - Generic */
#define IPC_INTEL_GLB_TYPE_SHIFT	24
#define IPC_INTEL_GLB_TYPE_MASK	(0x1f << IPC_INTEL_GLB_TYPE_SHIFT)
#define IPC_INTEL_GLB_TYPE(x)		(x << IPC_INTEL_GLB_TYPE_SHIFT)

/* Global Message - Reply */
#define IPC_INTEL_GLB_REPLY_SHIFT	0
#define IPC_INTEL_GLB_REPLY_MASK	(0x1f << IPC_INTEL_GLB_REPLY_SHIFT)
#define IPC_INTEL_GLB_REPLY_TYPE(x)	(x << IPC_INTEL_GLB_REPLY_TYPE_SHIFT)

/* Stream Message - Generic */
#define IPC_INTEL_STR_TYPE_SHIFT	20
#define IPC_INTEL_STR_TYPE_MASK	(0xf << IPC_INTEL_STR_TYPE_SHIFT)
#define IPC_INTEL_STR_TYPE(x)		(x << IPC_INTEL_STR_TYPE_SHIFT)
#define IPC_INTEL_STR_ID_SHIFT	16
#define IPC_INTEL_STR_ID_MASK		(0xf << IPC_INTEL_STR_ID_SHIFT)
#define IPC_INTEL_STR_ID(x)		(x << IPC_INTEL_STR_ID_SHIFT)

/* Stream Message - Reply */
#define IPC_INTEL_STR_REPLY_SHIFT	0
#define IPC_INTEL_STR_REPLY_MASK	(0x1f << IPC_INTEL_STR_REPLY_SHIFT)

/* Stream Stage Message - Generic */
#define IPC_INTEL_STG_TYPE_SHIFT	12
#define IPC_INTEL_STG_TYPE_MASK	(0xf << IPC_INTEL_STG_TYPE_SHIFT)
#define IPC_INTEL_STG_TYPE(x)		(x << IPC_INTEL_STG_TYPE_SHIFT)
#define IPC_INTEL_STG_ID_SHIFT	10
#define IPC_INTEL_STG_ID_MASK		(0x3 << IPC_INTEL_STG_ID_SHIFT)
#define IPC_INTEL_STG_ID(x)		(x << IPC_INTEL_STG_ID_SHIFT)

/* Stream Stage Message - Reply */
#define IPC_INTEL_STG_REPLY_SHIFT	0
#define IPC_INTEL_STG_REPLY_MASK	(0x1f << IPC_INTEL_STG_REPLY_SHIFT)

/* Debug Log Message - Generic */
#define IPC_INTEL_LOG_OP_SHIFT	20
#define IPC_INTEL_LOG_OP_MASK		(0xf << IPC_INTEL_LOG_OP_SHIFT)
#define IPC_INTEL_LOG_OP_TYPE(x)	(x << IPC_INTEL_LOG_OP_SHIFT)
#define IPC_INTEL_LOG_ID_SHIFT	16
#define IPC_INTEL_LOG_ID_MASK		(0xf << IPC_INTEL_LOG_ID_SHIFT)
#define IPC_INTEL_LOG_ID(x)		(x << IPC_INTEL_LOG_ID_SHIFT)

/* Module Message */
#define IPC_INTEL_MODULE_OPERATION_SHIFT	20
#define IPC_INTEL_MODULE_OPERATION_MASK	(0xf << IPC_INTEL_MODULE_OPERATION_SHIFT)
#define IPC_INTEL_MODULE_OPERATION(x)	(x << IPC_INTEL_MODULE_OPERATION_SHIFT)

#define IPC_INTEL_MODULE_ID_SHIFT	16
#define IPC_INTEL_MODULE_ID_MASK	(0xf << IPC_INTEL_MODULE_ID_SHIFT)
#define IPC_INTEL_MODULE_ID(x)	(x << IPC_INTEL_MODULE_ID_SHIFT)

/* IPC message timeout (msecs) */
#define IPC_INTEL_TIMEOUT_MSECS	300


/* Firmware Ready Message */
#define IPC_INTEL_FW_READY		(0x1 << 29)
#define IPC_INTL_STATUS_MASK		(0x3 << 30)

#define IPC_EMPTY_LIST_SIZE	8
#define IPC_MAX_STREAMS		4

#define IPC_INTEL_INVALID_STREAM	0xffffffff

/* Global Message - Types and Replies */
enum ipc_glb_type {
	IPC_INTEL_GLB_GET_FW_VERSION = 0,		/* Retrieves firmware version */
	IPC_INTEL_GLB_PERFORMANCE_MONITOR = 1,	/* Performance monitoring actions */
	IPC_INTEL_GLB_ALLOCATE_STREAM = 3,		/* Request to allocate new stream */
	IPC_INTEL_GLB_FREE_STREAM = 4,		/* Request to free stream */
	IPC_INTEL_GLB_GET_FW_CAPABILITIES = 5,	/* Retrieves firmware capabilities */
	IPC_INTEL_GLB_STREAM_MESSAGE = 6,		/* Message directed to stream or its stages */
	/* Request to store firmware context during D0->D3 transition */
	IPC_INTEL_GLB_REQUEST_DUMP = 7,
	/* Request to restore firmware context during D3->D0 transition */
	IPC_INTEL_GLB_RESTORE_CONTEXT = 8,
	IPC_INTEL_GLB_GET_DEVICE_FORMATS = 9,		/* Set device format */
	IPC_INTEL_GLB_SET_DEVICE_FORMATS = 10,	/* Get device format */
	IPC_INTEL_GLB_SHORT_REPLY = 11,
	IPC_INTEL_GLB_ENTER_DX_STATE = 12,
	IPC_INTEL_GLB_GET_MIXER_STREAM_INFO = 13,	/* Request mixer stream params */
	IPC_INTEL_GLB_DEBUG_LOG_MESSAGE = 14,		/* Message to or from the debug logger. */
	IPC_INTEL_GLB_MODULE_OPERATION = 15,		/* Message to loadable fw module */
	IPC_INTEL_GLB_REQUEST_TRANSFER = 16, 		/* < Request Transfer for host */
	IPC_INTEL_GLB_MAX_IPC_MESSAGE_TYPE = 17,	/* Maximum message number */
};

enum ipc_glb_reply {
	IPC_INTEL_GLB_REPLY_SUCCESS = 0,		/* The operation was successful. */
	IPC_INTEL_GLB_REPLY_ERROR_INVALID_PARAM = 1,	/* Invalid parameter was passed. */
	IPC_INTEL_GLB_REPLY_UNKNOWN_MESSAGE_TYPE = 2,	/* Uknown message type was resceived. */
	IPC_INTEL_GLB_REPLY_OUT_OF_RESOURCES = 3,	/* No resources to satisfy the request. */
	IPC_INTEL_GLB_REPLY_BUSY = 4,			/* The system or resource is busy. */
	IPC_INTEL_GLB_REPLY_PENDING = 5,		/* The action was scheduled for processing.  */
	IPC_INTEL_GLB_REPLY_FAILURE = 6,		/* Critical error happened. */
	IPC_INTEL_GLB_REPLY_INVALID_REQUEST = 7,	/* Request can not be completed. */
	IPC_INTEL_GLB_REPLY_STAGE_UNINITIALIZED = 8,	/* Processing stage was uninitialized. */
	IPC_INTEL_GLB_REPLY_NOT_FOUND = 9,		/* Required resource can not be found. */
	IPC_INTEL_GLB_REPLY_SOURCE_NOT_STARTED = 10,	/* Source was not started. */
};

enum ipc_module_operation {
	IPC_INTEL_MODULE_NOTIFICATION = 0,
	IPC_INTEL_MODULE_ENABLE = 1,
	IPC_INTEL_MODULE_DISABLE = 2,
	IPC_INTEL_MODULE_GET_PARAMETER = 3,
	IPC_INTEL_MODULE_SET_PARAMETER = 4,
	IPC_INTEL_MODULE_GET_INFO = 5,
	IPC_INTEL_MODULE_MAX_MESSAGE
};

/* Stream Message - Types */
enum ipc_str_operation {
	IPC_INTEL_STR_RESET = 0,
	IPC_INTEL_STR_PAUSE = 1,
	IPC_INTEL_STR_RESUME = 2,
	IPC_INTEL_STR_STAGE_MESSAGE = 3,
	IPC_INTEL_STR_NOTIFICATION = 4,
	IPC_INTEL_STR_MAX_MESSAGE
};

/* Stream Stage Message Types */
enum ipc_stg_operation {
	IPC_INTEL_STG_GET_VOLUME = 0,
	IPC_INTEL_STG_SET_VOLUME,
	IPC_INTEL_STG_SET_WRITE_POSITION,
	IPC_INTEL_STG_SET_FX_ENABLE,
	IPC_INTEL_STG_SET_FX_DISABLE,
	IPC_INTEL_STG_SET_FX_GET_PARAM,
	IPC_INTEL_STG_SET_FX_SET_PARAM,
	IPC_INTEL_STG_SET_FX_GET_INFO,
	IPC_INTEL_STG_MUTE_LOOPBACK,
	IPC_INTEL_STG_MAX_MESSAGE
};

/* Stream Stage Message Types For Notification*/
enum ipc_stg_operation_notify {
	IPC_POSITION_CHANGED = 0,
	IPC_INTEL_STG_GLITCH,
	IPC_INTEL_STG_MAX_NOTIFY
};

enum ipc_glitch_type {
	IPC_GLITCH_UNDERRUN = 1,
	IPC_GLITCH_DECODER_ERROR,
	IPC_GLITCH_DOUBLED_WRITE_POS,
	IPC_GLITCH_MAX
};

/* Debug Control */
enum ipc_debug_operation {
	IPC_DEBUG_ENABLE_LOG = 0,
	IPC_DEBUG_DISABLE_LOG = 1,
	IPC_DEBUG_REQUEST_LOG_DUMP = 2,
	IPC_DEBUG_NOTIFY_LOG_DUMP = 3,
	IPC_DEBUG_MAX_DEBUG_LOG
};

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
	IPC_INTEL_DEVICE_SSP_2   = 2,
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
	uint8_t name[IPC_INTEL_MAX_INFO_SIZE];
	uint8_t version[IPC_INTEL_MAX_INFO_SIZE];
} __attribute__((packed));

/* Module entry point */
struct ipc_intel_module_entry {
	enum ipc_intel_module_id module_id;
	uint32_t entry_point;
} __attribute__((packed));

/* Module map - alignement matches DSP */
struct ipc_intel_module_map {
	uint8_t module_entries_count;
	struct ipc_intel_module_entry module_entries[1];
} __attribute__((packed));

struct ipc_intel_memory_info {
	uint32_t offset;
	uint32_t size;
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
	uint32_t parameter_id;
	uint32_t param_size;
} __attribute__((packed));

struct ipc_intel_perf_action {
	uint32_t action;
} __attribute__((packed));

struct ipc_intel_perf_data {
	uint64_t timestamp;
	uint64_t cycles;
	uint64_t datatime;
} __attribute__((packed));

/* FW version */
struct ipc_intel_ipc_fw_version {
	uint8_t build;
	uint8_t minor;
	uint8_t major;
	uint8_t type;
	uint8_t fw_build_hash[IPC_INTEL_BUILD_HASH_LENGTH];
	uint32_t fw_log_providers_hash;
} __attribute__((packed));

/* Stream ring info */
struct ipc_intel_ipc_stream_ring {
	uint32_t ring_pt_address;
	uint32_t num_pages;
	uint32_t ring_size;
	uint32_t ring_offset;
	uint32_t ring_first_pfn;
} __attribute__((packed));

/* Debug Dump Log Enable Request */
struct ipc_intel_ipc_debug_log_enable_req {
	struct ipc_intel_ipc_stream_ring ringinfo;
	uint32_t config[IPC_INTEL_FW_LOG_CONFIG_DWORDS];
} __attribute__((packed));

/* Debug Dump Log Reply */
struct ipc_intel_ipc_debug_log_reply {
	uint32_t log_buffer_begining;
	uint32_t log_buffer_size;
} __attribute__((packed));

/* Stream glitch position */
struct ipc_intel_ipc_stream_glitch_position {
	uint32_t glitch_type;
	uint32_t present_pos;
	uint32_t write_pos;
} __attribute__((packed));

/* Stream get position */
struct ipc_intel_ipc_stream_get_position {
	uint32_t position;
	uint32_t fw_cycle_count;
} __attribute__((packed));

/* Stream set position */
struct ipc_intel_ipc_stream_set_position {
	uint32_t position;
	uint32_t end_of_buffer;
} __attribute__((packed));

/* Stream Free Request */
struct ipc_intel_ipc_stream_free_req {
	uint8_t stream_id;
	uint8_t reserved[3];
} __attribute__((packed));

/* Set Volume Request */
struct ipc_intel_ipc_volume_req {
	uint32_t channel;
	uint32_t target_volume;
	uint64_t curve_duration;
	uint32_t curve_type;
} __attribute__((packed));

/* Device Configuration Request */
struct ipc_intel_ipc_device_config_req {
	uint32_t ssp_interface;
	uint32_t clock_frequency;
	uint32_t mode;
	uint16_t clock_divider;
	uint8_t channels;
	uint8_t reserved;
} __attribute__((packed));

/* Audio Data formats */
struct ipc_intel_audio_data_format_ipc {
	uint32_t frequency;
	uint32_t bitdepth;
	uint32_t map;
	uint32_t config;
	uint32_t style;
	uint8_t ch_num;
	uint8_t valid_bit;
	uint16_t period_pages;
} __attribute__((packed));

/* Stream Allocate Request */
struct ipc_intel_ipc_stream_alloc_req {
	uint8_t path_id;
	uint8_t stream_type;
	uint8_t format_id;
	uint8_t reserved;
	struct ipc_intel_audio_data_format_ipc format;
	struct ipc_intel_ipc_stream_ring ringinfo;
	struct ipc_intel_module_map map;
	struct ipc_intel_memory_info persistent_mem;
	struct ipc_intel_memory_info scratch_mem;
	uint32_t number_of_notifications;
} __attribute__((packed));

/* Stream Allocate Reply */
struct ipc_intel_ipc_stream_alloc_reply {
	uint32_t stream_hw_id;
	uint32_t mixer_hw_id; // returns rate ????
	uint32_t read_position_register_address;
	uint32_t presentation_position_register_address;
	uint32_t peak_meter_register_address[IPC_INTEL_NO_CHANNELS];
	uint32_t volume_register_address[IPC_INTEL_NO_CHANNELS];
} __attribute__((packed));

/* Get Mixer Stream Info */
struct ipc_intel_ipc_stream_info_reply {
	uint32_t mixer_hw_id;
	uint32_t peak_meter_register_address[IPC_INTEL_NO_CHANNELS];
	uint32_t volume_register_address[IPC_INTEL_NO_CHANNELS];
} __attribute__((packed));

/* DX State Request */
struct ipc_intel_ipc_dx_req {
	uint8_t state;
	uint8_t reserved[3];
} __attribute__((packed));

/* DX State Reply Memory Info Item */
struct ipc_intel_ipc_dx_memory_item {
	uint32_t offset;
	uint32_t size;
	uint32_t source;
} __attribute__((packed));

/* DX State Reply */
struct ipc_intel_ipc_dx_reply {
	uint32_t entries_no;
	struct ipc_intel_ipc_dx_memory_item mem_info[IPC_INTEL_MAX_DX_REGIONS];
} __attribute__((packed));


/* FW info */
struct fw_info {
	uint8_t name[5];
	uint8_t date[11];
	uint8_t time[8];
} __attribute__((packed));

/* Firmware Ready */
#define IPC_INTEL_FW_RDY_RSVD	32

struct sst_intel_ipc_fw_ready {
	uint32_t inbox_offset;
	uint32_t outbox_offset;
	uint32_t inbox_size;
	uint32_t outbox_size;
	uint32_t fw_info_size;
	union {
		uint8_t rsvd[IPC_INTEL_FW_RDY_RSVD];
		struct fw_info info;
	};
} __attribute__((packed));

/* Stream data */
struct sst_intel_ipc_stream_vol {
	uint32_t peak;
	uint32_t vol;
} __attribute__((packed));

struct sst_intel_ipc_stream_data {
	uint32_t read_posn;
	uint32_t presentation_posn;
	struct sst_intel_ipc_stream_vol vol[IPC_INTEL_NO_CHANNELS];
} __attribute__((packed));
#endif
