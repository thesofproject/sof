/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Sound Open Firmware (SOF) Remote Server Protocol & Shared UAPI Definitions
 */

#ifndef _TESTBENCH_SOF_REMOTE_PROTO_H
#define _TESTBENCH_SOF_REMOTE_PROTO_H

#include <stdint.h>

#define SOF_CDEV_ABI_VERSION		0x00010000U

/*
 * SRAM Memory Window Selectors
 */
#define SOF_CDEV_WIN_FW_REGS		0	/* Window 0: FW Regs (0x0..0xfff) + Inbox (+0x1000) */
#define SOF_CDEV_WIN_OUTBOX		1	/* Window 1: Host Downlink Outbox */
#define SOF_CDEV_WIN_DEBUG		2	/* Window 2: Debug Window (16 x 4 KB pages) */
#define SOF_CDEV_WIN_PERF		3	/* Window 3: Performance Measurements Window (ADSP_PMW) */
#define SOF_CDEV_WIN_BY_SLOT_TYPE	0xFF	/* Select Window 2 slot by slot_type + slot_index */

#define SOF_CDEV_MAX_WINDOWS		4
#define SOF_CDEV_MAX_DEBUG_SLOTS	16

/* Standard IPC4 Window 2 Debug Slot Types */
#define SOF_CDEV_SLOT_UNUSED		0x00000000U
#define SOF_CDEV_SLOT_CRITICAL_LOG	0x54524300U
#define SOF_CDEV_SLOT_DEBUG_LOG		0x474f4c00U
#define SOF_CDEV_SLOT_GDB_STUB		0x42444700U
#define SOF_CDEV_SLOT_TELEMETRY		0x4c455400U
#define SOF_CDEV_SLOT_BROKEN		0x44414544U
#define SOF_CDEV_SLOT_SHELL		0x73686c6cU /* 'shll' Zephyr winstream shell */
#define SOF_CDEV_SLOT_LLEXT_LOAD	0x4c454c44U /* 'LELD' Shell LLEXT load shm */

struct sof_cdev_slot_desc {
	uint32_t slot_index;
	uint32_t res_id;
	uint32_t type;
	uint32_t vma;
	uint32_t win_offset;
	uint32_t bar_offset;
	uint32_t size;
	uint32_t reserved;
};

struct sof_cdev_win_desc {
	uint32_t win_id;
	uint32_t bar_offset;
	uint32_t size;
	uint32_t flags;
};

struct sof_cdev_info {
	uint32_t abi_version;
	uint32_t ipc_type;		/* 0 = IPC3, 1 = IPC4 */
	uint32_t fw_state;		/* enum sof_fw_state (7 = BOOT_COMPLETE) */
	uint32_t dsp_power_state;	/* 0=U0, 1=D0, 2=D1, 3=D2, 4=D3 */
	uint32_t pm_runtime_active;	/* 1 if active (D0) */
	uint32_t num_cores;		/* Total DSP cores on platform */
	uint32_t enabled_cores_mask;	/* Bitmask of currently enabled DSP cores */
	uint32_t max_payload_size;	/* Max IPC mailbox payload size in bytes */
	uint16_t fw_ver_major;
	uint16_t fw_ver_minor;
	uint16_t fw_ver_micro;
	uint16_t fw_ver_build;
	uint32_t hw_ip_version;
	char     platform[32];
	char     fw_filename[64];
	struct sof_cdev_win_desc windows[SOF_CDEV_MAX_WINDOWS];
	struct sof_cdev_slot_desc debug_slots[SOF_CDEV_MAX_DEBUG_SLOTS];
};

/*
 * Power Management Control Commands
 */
#define SOF_PM_OP_QUERY		0
#define SOF_PM_OP_WAKE_HOLD	1
#define SOF_PM_OP_RELEASE_HOLD	2
#define SOF_PM_OP_BOOT_DSP	3
#define SOF_PM_OP_CORE_GET	4
#define SOF_PM_OP_CORE_PUT	5

struct sof_cdev_pm_ctl {
	uint32_t op;
	uint32_t core_id;
	uint32_t fw_state;
	uint32_t dsp_power_state;
	uint32_t pm_runtime_active;
	uint32_t enabled_cores_mask;
	uint32_t fd_holding_d0;
	uint32_t reserved;
};

#define SOF_CDEV_IPC_FL_NO_REPLY	(1U << 0)

#define SOF_CDEV_LARGE_CFG_GET		0
#define SOF_CDEV_LARGE_CFG_SET		1

#define SOF_CDEV_WIN_FL_WAKE_DSP	(1U << 0)

struct sof_cdev_win_info {
	struct sof_cdev_win_desc windows[SOF_CDEV_MAX_WINDOWS];
	struct sof_cdev_slot_desc debug_slots[SOF_CDEV_MAX_DEBUG_SLOTS];
};

#define SOF_LLEXT_MODE_SHELL_SHM	0
#define SOF_LLEXT_MODE_IPC4_DIRECT	1

#define SOF_PROBE_OP_INIT		1
#define SOF_PROBE_OP_DEINIT		2
#define SOF_PROBE_OP_INFO_ACTIVE	3
#define SOF_PROBE_OP_INFO_AVAILABLE	4
#define SOF_PROBE_OP_POINTS_ADD		5
#define SOF_PROBE_OP_POINTS_REMOVE	6
#define SOF_PROBE_OP_READ_BUFFER	7

struct sof_cdev_probe_point {
	uint32_t buffer_id;
	uint32_t purpose;
	uint32_t stream_tag;
	uint32_t reserved;
	char     widget_name[32];
};

/*
 * Network Protocol Definitions (TCP port 9998)
 */
#define SOF_NET_MAGIC			0x534F4652U /* 'SOFR' */
#define SOF_NET_VERSION			1
#define SOF_NET_DEFAULT_PORT		9998

enum sof_net_opcode {
	SOF_NET_OP_PING			= 0x0000,
	SOF_NET_OP_GET_INFO		= 0x0001,
	SOF_NET_OP_PM_CTL		= 0x0002,
	SOF_NET_OP_IPC_TX		= 0x0010,
	SOF_NET_OP_IPC_LARGE_CFG	= 0x0011,
	SOF_NET_OP_WIN_INFO		= 0x0020,
	SOF_NET_OP_WIN_READ		= 0x0021,
	SOF_NET_OP_WIN_WRITE		= 0x0022,
	SOF_NET_OP_WINSTREAM_IO		= 0x0023,
	SOF_NET_OP_TELEM_SNAPSHOT	= 0x0028,
	SOF_NET_OP_WORKLOAD_SIM_CFG	= 0x0029,
	SOF_NET_OP_SCHED_TRACE		= 0x002B,
	SOF_NET_OP_LLEXT_LOAD		= 0x0030,
	SOF_NET_OP_SHELL_EXEC		= 0x0040,
	SOF_NET_OP_PROBE_CTL		= 0x0050,
	SOF_NET_OP_PCM_CTL		= 0x0060,
	/* Extended Audio Card, PCM/Compress Capabilities, DAPM Graph & Kcontrol Opcodes */
	SOF_NET_OP_CARD_LIST		= 0x0070,
	SOF_NET_OP_PCM_CAPS		= 0x0071,
	SOF_NET_OP_DAPM_GRAPH		= 0x0072,
	SOF_NET_OP_COMPR_CTL		= 0x0073,
	SOF_NET_OP_KCTL_LIST		= 0x0074,
	SOF_NET_OP_KCTL_GET		= 0x0075,
	SOF_NET_OP_KCTL_SET		= 0x0076,
};

/* Selectable Userspace Audio API Backend */
enum sof_audio_api {
	SOF_AUDIO_API_AUTO		= 0,
	SOF_AUDIO_API_ALSA		= 1,
	SOF_AUDIO_API_TINYALSA		= 2,
	SOF_AUDIO_API_TINYCOMPRESS	= 3,
};

/* Sample Format IDs across ALSA and TinyALSA */
enum sof_pcm_fmt_id {
	SOF_PCM_FMT_S16_LE		= 0,
	SOF_PCM_FMT_S24_LE		= 1,
	SOF_PCM_FMT_S24_3LE		= 2,
	SOF_PCM_FMT_S32_LE		= 3,
	SOF_PCM_FMT_S8			= 4,
	SOF_PCM_FMT_FLOAT_LE		= 5,
};

struct sof_net_hdr {
	uint32_t magic;
	uint16_t version;
	uint16_t opcode;
	uint32_t req_id;
	uint32_t payload_len;
	int32_t  status;
} __attribute__((packed));

struct sof_net_ipc_tx_req {
	uint32_t primary;
	uint32_t extension;
	uint32_t tx_size;
	uint32_t rx_size;
	uint32_t flags;
} __attribute__((packed));

struct sof_net_ipc_tx_rsp {
	uint32_t reply_primary;
	uint32_t reply_extension;
	int32_t  ipc_status;
	uint32_t rx_size;
} __attribute__((packed));

struct sof_net_large_cfg_req {
	uint32_t module_id;
	uint32_t instance_id;
	uint32_t param_id;
	uint32_t set_get;
	uint32_t data_size;
	uint32_t flags;
} __attribute__((packed));

struct sof_net_win_io_req {
	uint32_t win_id;
	uint32_t slot_type;
	uint32_t slot_index;
	uint32_t offset;
	uint32_t size;
	uint32_t flags;
} __attribute__((packed));

struct sof_net_winstream_req {
	uint32_t slot_type;
	uint32_t rx_win_size;
	uint32_t tx_len;
	uint32_t rx_buf_len;
	uint32_t seq_in;
} __attribute__((packed));

struct sof_net_winstream_rsp {
	uint32_t rx_bytes_read;
	uint32_t seq_out;
} __attribute__((packed));

#define SOF_TELEM_FL_FW_REGS		(1U << 0)
#define SOF_TELEM_FL_TELEM_SLOT		(1U << 1)
#define SOF_TELEM_FL_PERF_WIN3		(1U << 2)
#define SOF_TELEM_FL_MEM_STATE_TLV	(1U << 3)
#define SOF_TELEM_FL_PWR_STATE_TLV	(1U << 4)
#define SOF_TELEM_FL_EXT_PERF_TLV	(1U << 5)
#define SOF_TELEM_FL_MODULES_INFO_TLV	(1U << 6)
#define SOF_TELEM_FL_SYS_HOST_STATS	(1U << 7)
#define SOF_TELEM_FL_WAKE_DSP		(1U << 15)

struct sof_net_telem_req {
	uint32_t query_mask;
	uint32_t flags;
} __attribute__((packed));

struct sof_net_telem_snapshot {
	struct sof_cdev_info dev_info;
	uint32_t flags_present;
	uint32_t active_pcm_streams;
	uint8_t  fw_regs[512];
	uint8_t  telem_slot[512];
	uint8_t  perf_win3[4096];
	uint32_t mem_state_len;
	uint8_t  mem_state_tlv[2048];
	uint32_t pwr_state_len;
	uint8_t  pwr_state_tlv[256];
	uint32_t ext_perf_len;
	uint8_t  ext_perf_tlv[4096];
	uint32_t modules_info_len;
	uint8_t  modules_info_tlv[4096];
};

struct sof_net_llext_req {
	uint32_t mode;
	uint32_t lib_id;
	uint32_t data_size;
	uint32_t issue_shell_cmd;
	char     mod_name[64];
} __attribute__((packed));

struct sof_net_llext_rsp {
	uint32_t lib_id;
	int32_t  fw_state;
	char     shell_out[512];
} __attribute__((packed));

struct sof_net_shell_req {
	uint32_t timeout_ms;
	uint32_t cmd_len;
} __attribute__((packed));

struct sof_net_probe_req {
	uint32_t op;
	uint32_t stream_tag;
	uint32_t dma_buffer_size;
	uint32_t num_points;
} __attribute__((packed));

#define SOF_PCM_OP_STATUS		0
#define SOF_PCM_OP_START_TONE		1
#define SOF_PCM_OP_STOP_TONE		2
#define SOF_PCM_OP_PLAY_BUFFER		3
#define SOF_PCM_OP_CAPTURE_BUFFER	4

struct sof_net_pcm_req {
	uint32_t op;
	char     device[32];
	uint32_t rate;
	uint32_t channels;
	uint32_t freq_hz;
	uint32_t duration_ms;
	uint32_t data_size;
	/* Optional extended fields (if payload_len >= sizeof(struct sof_net_pcm_req)) */
	uint32_t api_backend;
	uint32_t format;
	uint32_t period_size;
	uint32_t period_count;
} __attribute__((packed));

struct sof_net_pcm_rsp {
	uint32_t tone_running;
	char     active_device[32];
	uint32_t rate;
	uint32_t channels;
	uint32_t freq_hz;
	uint64_t frames_played;
	uint32_t rms_level;
	uint32_t peak_level;
	uint32_t data_size;
	uint32_t api_used;
	uint32_t format;
	uint32_t xruns;
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * SOF_NET_OP_CARD_LIST: Sound Cards, PCM Streams & Compress Streams
 * -------------------------------------------------------------------------
 */
#define SOF_STREAM_DIR_PLAYBACK		0
#define SOF_STREAM_DIR_CAPTURE		1

struct sof_net_card_list_hdr {
	uint32_t num_cards;
	uint32_t num_pcm_nodes;
} __attribute__((packed));

struct sof_net_card_info {
	uint32_t card_id;
	char     id[32];
	char     driver[32];
	char     name[64];
	char     longname[96];
	char     mixername[64];
	char     tplg_file[96];
	uint32_t num_pcm_streams;
	uint32_t num_compr_streams;
	uint32_t num_kcontrols;
	uint32_t active_streams;
} __attribute__((packed));

struct sof_net_pcm_node_info {
	uint32_t card_id;
	uint32_t device_id;
	uint32_t direction;
	uint32_t is_compress;
	char     id[64];
	char     name[64];
	uint32_t subdevices_count;
	uint32_t subdevices_avail;
	char     state_str[16];
	uint32_t is_running;
	uint32_t owner_pid;
	char     active_format[16];
	uint32_t active_rate;
	uint32_t active_channels;
	uint32_t period_size;
	uint32_t buffer_size;
	uint64_t hw_ptr;
	uint64_t appl_ptr;
	uint32_t delay_frames;
	uint32_t avail_frames;
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * SOF_NET_OP_PCM_CAPS: Per-PCM & Compress Capabilities
 * -------------------------------------------------------------------------
 */
#define SOF_CAPS_DIR_PLAYBACK		(1U << 0)
#define SOF_CAPS_DIR_CAPTURE		(1U << 1)
#define SOF_CAPS_DIR_COMPRESS		(1U << 2)
#define SOF_CAPS_DIR_ALL		0x07U

struct sof_net_pcm_caps_req {
	uint32_t card_id;
	uint32_t device_id;
	uint32_t dir_mask;
	uint32_t api_backend;
} __attribute__((packed));

struct sof_net_pcm_caps_hdr {
	uint32_t num_entries;
	uint32_t api_used;
} __attribute__((packed));

#define SOF_FMTBIT_S8			(1ULL << 0)
#define SOF_FMTBIT_U8			(1ULL << 1)
#define SOF_FMTBIT_S16_LE		(1ULL << 2)
#define SOF_FMTBIT_S16_BE		(1ULL << 3)
#define SOF_FMTBIT_S24_LE		(1ULL << 6)
#define SOF_FMTBIT_S24_BE		(1ULL << 7)
#define SOF_FMTBIT_S32_LE		(1ULL << 10)
#define SOF_FMTBIT_S32_BE		(1ULL << 11)
#define SOF_FMTBIT_FLOAT_LE		(1ULL << 14)
#define SOF_FMTBIT_S24_3LE		(1ULL << 32)
#define SOF_FMTBIT_S24_3BE		(1ULL << 33)

#define SOF_RATEBIT_8000		(1U << 0)
#define SOF_RATEBIT_11025		(1U << 1)
#define SOF_RATEBIT_16000		(1U << 2)
#define SOF_RATEBIT_22050		(1U << 3)
#define SOF_RATEBIT_32000		(1U << 4)
#define SOF_RATEBIT_44100		(1U << 5)
#define SOF_RATEBIT_48000		(1U << 6)
#define SOF_RATEBIT_88200		(1U << 7)
#define SOF_RATEBIT_96000		(1U << 8)
#define SOF_RATEBIT_176400		(1U << 9)
#define SOF_RATEBIT_192000		(1U << 10)

struct sof_net_pcm_caps {
	uint32_t card_id;
	uint32_t device_id;
	uint32_t direction;
	uint32_t is_compress;
	uint32_t api_used;
	int32_t  status;
	char     id[64];
	char     name[64];
	uint64_t formats_mask;
	uint32_t access_mask;
	uint32_t rates_mask;
	uint32_t rate_min;
	uint32_t rate_max;
	uint32_t channels_min;
	uint32_t channels_max;
	uint32_t sample_bits_min;
	uint32_t sample_bits_max;
	uint32_t period_size_min;
	uint32_t period_size_max;
	uint32_t period_bytes_min;
	uint32_t period_bytes_max;
	uint32_t periods_min;
	uint32_t periods_max;
	uint32_t buffer_size_min;
	uint32_t buffer_size_max;
	uint32_t buffer_bytes_min;
	uint32_t buffer_bytes_max;
	uint32_t min_fragment_size;
	uint32_t max_fragment_size;
	uint32_t min_fragments;
	uint32_t max_fragments;
	uint32_t num_codecs;
	uint32_t codecs[16];
	char     formats_str[96];
	char     rates_str[96];
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * SOF_NET_OP_DAPM_GRAPH: DAPM Nodes, Directed Connections, Layout & DPCM Status
 * -------------------------------------------------------------------------
 */
struct sof_net_dapm_req {
	uint32_t card_id;
	uint32_t flags;
} __attribute__((packed));

struct sof_net_dapm_graph_hdr {
	uint32_t card_id;
	char     card_name[64];
	char     tplg_file[96];
	uint32_t num_components;
	uint32_t num_nodes;
	uint32_t num_powered_nodes;
	uint32_t num_edges;
	uint32_t num_active_edges;
	uint32_t num_dpcm_links;
} __attribute__((packed));

struct sof_net_dapm_comp_status {
	char     name[48];
	char     bias_str[16];
	uint32_t bias_level;
	uint32_t widget_count;
	uint32_t powered_count;
} __attribute__((packed));

struct sof_net_dapm_node {
	uint32_t node_id;
	char     name[64];
	char     component[48];
	char     widget_type[24];
	uint32_t power;
	uint32_t in_ep;
	uint32_t out_ep;
	uint32_t stream_active;
	char     stream_name[48];
	int32_t  reg;
	uint32_t mask;
	int32_t  pipeline_id;
	uint16_t layout_col;
	uint16_t layout_row;
	uint16_t num_kcontrols;
	uint16_t reserved;
	uint32_t kcontrol_numids[4];
	char     kcontrol_names[2][48];
} __attribute__((packed));

struct sof_net_dapm_edge {
	uint32_t edge_id;
	int32_t  src_node_id;
	int32_t  dst_node_id;
	char     src_name[48];
	char     dst_name[48];
	char     src_comp[32];
	char     dst_comp[32];
	char     control_name[48];
	uint32_t kcontrol_numid;
	uint32_t active;
} __attribute__((packed));

struct sof_net_dpcm_link_state {
	char     fe_name[48];
	uint32_t direction;
	char     state_str[16];
	char     fe_format[16];
	uint32_t fe_channels;
	uint32_t fe_rate;
	uint32_t num_backends;
	char     be_names[4][32];
	char     be_states[4][16];
	char     be_formats[4][16];
	uint32_t be_channels[4];
	uint32_t be_rates[4];
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * SOF_NET_OP_COMPR_CTL: Compressed PCM Playback & Capture
 * -------------------------------------------------------------------------
 */
#define SOF_COMPR_OP_GET_CAPS		0
#define SOF_COMPR_OP_PLAY_BUFFER	1
#define SOF_COMPR_OP_CAPTURE_BUFFER	2

struct sof_net_compr_req {
	uint32_t op;
	uint32_t card_id;
	uint32_t device_id;
	uint32_t direction;
	uint32_t codec_id;
	uint32_t rate;
	uint32_t channels;
	uint32_t format;
	uint32_t fragment_size;
	uint32_t fragments;
	uint32_t duration_ms;
	uint32_t freq_hz;
	uint32_t data_size;
} __attribute__((packed));

struct sof_net_compr_rsp {
	int32_t  status;
	uint32_t card_id;
	uint32_t device_id;
	uint32_t direction;
	uint32_t codec_id;
	uint32_t fragment_size;
	uint32_t fragments;
	uint32_t min_fragment_size;
	uint32_t max_fragment_size;
	uint32_t num_codecs;
	uint32_t codecs[16];
	uint32_t bytes_transferred;
	uint64_t rendered_samples;
	uint32_t sampling_rate;
	uint32_t data_size;
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * SOF_NET_OP_KCTL_LIST / KCTL_GET / KCTL_SET
 * -------------------------------------------------------------------------
 */
enum sof_kctl_type {
	SOF_KCTL_TYPE_NONE		= 0,
	SOF_KCTL_TYPE_BOOLEAN		= 1,
	SOF_KCTL_TYPE_INTEGER		= 2,
	SOF_KCTL_TYPE_ENUMERATED	= 3,
	SOF_KCTL_TYPE_BYTES		= 4,
	SOF_KCTL_TYPE_IEC958		= 5,
	SOF_KCTL_TYPE_INTEGER64		= 6,
};

enum sof_kctl_anchor_type {
	SOF_KCTL_ANCHOR_CARD		= 0,
	SOF_KCTL_ANCHOR_WIDGET		= 1,
	SOF_KCTL_ANCHOR_EDGE		= 2,
	SOF_KCTL_ANCHOR_PCM_FE		= 3,
	SOF_KCTL_ANCHOR_DAI_BE		= 4,
};

#define SOF_KCTL_ACC_READ		(1U << 0)
#define SOF_KCTL_ACC_WRITE		(1U << 1)
#define SOF_KCTL_ACC_VOLATILE		(1U << 2)
#define SOF_KCTL_ACC_TLV_READ		(1U << 3)
#define SOF_KCTL_ACC_TLV_WRITE		(1U << 4)
#define SOF_KCTL_ACC_INACTIVE		(1U << 5)
#define SOF_KCTL_ACC_LOCK		(1U << 6)

struct sof_net_kctl_list_req {
	uint32_t card_id;
	uint32_t api_backend;
} __attribute__((packed));

struct sof_net_kctl_list_hdr {
	uint32_t card_id;
	uint32_t api_used;
	uint32_t num_kcontrols;
} __attribute__((packed));

struct sof_net_kctl_info {
	uint32_t card_id;
	uint32_t numid;
	uint32_t iface;
	char     iface_str[12];
	uint32_t device;
	uint32_t subdevice;
	uint32_t index;
	char     name[64];
	uint32_t type;
	char     type_str[12];
	uint32_t access_mask;
	char     access_str[12];
	uint32_t count;
	int64_t  min_val;
	int64_t  max_val;
	int64_t  step_val;
	uint32_t has_db_scale;
	int32_t  db_min_cdb;
	int32_t  db_step_cdb;
	uint32_t db_mute;
	uint32_t num_enums;
	char     enum_names[8][24];
	int64_t  values[8];
	uint32_t anchor_type;
	int32_t  target_node_id;
	int32_t  target_edge_id;
	char     target_widget[64];
	char     target_component[48];
	int32_t  pipeline_id;
	uint16_t layout_col;
	uint16_t layout_row;
} __attribute__((packed));

struct sof_net_kctl_io_req {
	uint32_t card_id;
	uint32_t api_backend;
	uint32_t numid;
	uint32_t iface;
	uint32_t device;
	uint32_t index;
	char     name[64];
	uint32_t count;
	int64_t  values[16];
	uint32_t bytes_len;
	uint8_t  bytes_data[512];
} __attribute__((packed));

struct sof_net_kctl_io_rsp {
	int32_t  status;
	uint32_t api_used;
	struct sof_net_kctl_info info;
	uint32_t bytes_len;
	uint8_t  bytes_data[512];
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * SOF_NET_OP_WORKLOAD_SIM_CFG (0x0029)
 * Configure and query accurate CPC/Frequency/SRAM-DRAM workload simulation
 * -------------------------------------------------------------------------
 */
#define SOF_WORKLOAD_SIM_OP_GET			0
#define SOF_WORKLOAD_SIM_OP_SET			1
#define SOF_WORKLOAD_SIM_OP_MOD_SET		2
#define SOF_WORKLOAD_SIM_OP_RESET_OVERRIDES	3

struct sof_net_workload_sim_req {
	uint32_t op;			/* SOF_WORKLOAD_SIM_OP_* */
	uint32_t mode;			/* 0=OFF, 1=VIRTUAL, 2=REALTIME */
	uint32_t default_mem_domain;	/* 0=SRAM, 1=DRAM */
	uint32_t dram_factor_x100;	/* e.g. 160 = 1.60x */
	uint32_t dram_cycles_per_word;	/* e.g. 2 cycles per 32-bit word */
	uint32_t core_freq_hz[8];	/* 0 = unchanged, else target core frequency in Hz */
	/* For SOF_WORKLOAD_SIM_OP_MOD_SET */
	uint32_t target_comp_id;
	uint32_t cpc_override;
	uint32_t mem_domain_override;	/* 0=SRAM, 1=DRAM, 0xFF=AUTO */
} __attribute__((packed));

struct sof_net_workload_core_sim_stat {
	uint32_t active;
	uint32_t freq_hz;
	uint32_t total_util_x100;
	uint32_t ll_deadline_violations;
	uint32_t ll_avg_kcps;
	uint32_t ll_peak_kcps;
	uint64_t ll_avg_exec_ns;
	uint64_t ll_peak_exec_ns;
	uint32_t dp_avg_kcps;
	uint32_t dp_peak_kcps;
	uint64_t dp_avg_exec_ns;
	uint64_t dp_peak_exec_ns;
} __attribute__((packed));

struct sof_net_workload_sim_rsp {
	int32_t  status;
	uint32_t mode;
	uint32_t default_mem_domain;
	uint32_t sram_factor_x100;
	uint32_t dram_factor_x100;
	uint32_t dram_cycles_per_word;
	uint32_t num_cores;
	uint32_t num_overrides;
	struct sof_net_workload_core_sim_stat cores[8];
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * SOF_NET_OP_SCHED_TRACE (0x002B)
 * Query per-iteration LL/DP module scheduling timeline (start/complete/deadline)
 * -------------------------------------------------------------------------
 */
#define SOF_SCHED_TRACE_OP_READ		0
#define SOF_SCHED_TRACE_OP_CLEAR	1

struct sof_net_sched_trace_req {
	uint32_t op;			/* SOF_SCHED_TRACE_OP_READ or CLEAR */
	uint32_t max_events;		/* Max events to return (0 = default 256, max 4096) */
	uint32_t since_seq_id;		/* Return only events with seq_id > since_seq_id */
	uint32_t filter_core_id;	/* 0xFFFFFFFF = all cores, else DSP core 0..7 */
	uint32_t filter_pipe_id;	/* 0xFFFFFFFF = all pipelines, else pipeline ID */
	uint32_t filter_comp_id;	/* 0xFFFFFFFF = all modules, else IPC4 comp_id */
	uint32_t filter_domain;		/* 0xFFFFFFFF = all, 0 = LL_COPY, 1 = DP_SYNC, 2 = DP_RUN */
} __attribute__((packed));

struct sof_net_sched_trace_hdr {
	int32_t  status;
	uint32_t total_recorded_events;	/* Total scheduling events emitted since reset */
	uint32_t avail_in_ring;		/* Events currently stored in trace ring buffer */
	uint32_t returned_events;	/* Number of sof_net_sched_trace_entry items following */
	uint32_t current_ll_tick;	/* Total 1ms LL ticks executed in current run */
	uint32_t num_cores;		/* Configured DSP core count */
	uint32_t sim_mode;		/* 0=HOST-MEASURED, 1=VIRTUAL-SIM, 2=REALTIME-PACED */
	uint32_t core_freq_hz[8];	/* Active core clock frequencies in Hz */
	char     platform_name[32];	/* Platform preset name (e.g. ptl, arl, tgl, nvl) */
} __attribute__((packed));

struct sof_net_sched_trace_entry {
	uint32_t seq_id;
	uint32_t ll_tick_nr;
	uint32_t mod_iter_nr;
	uint32_t comp_id;
	uint16_t pipeline_id;
	uint8_t  core_id;
	uint8_t  event_type;		/* 0=LL_COPY, 1=DP_SYNC, 2=DP_RUN */
	uint8_t  mem_domain;		/* 0=SRAM, 1=DRAM */
	uint8_t  cpc_violation;
	uint16_t period_us;
	uint64_t start_cycles;
	uint64_t complete_cycles;
	uint32_t duration_cycles;
	uint32_t cpc_budget;
	uint64_t start_ns;
	uint64_t complete_ns;
	uint64_t deadline_ns;
	uint64_t wall_start_ns;
	uint64_t wall_complete_ns;
	uint32_t bytes_in;
	uint32_t bytes_out;
	int32_t  ret_status;
	char     widget_name[32];
} __attribute__((packed));

#endif /* _TESTBENCH_SOF_REMOTE_PROTO_H */
