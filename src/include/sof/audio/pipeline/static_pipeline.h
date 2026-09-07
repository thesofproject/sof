/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_H__
#define __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_H__

#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <ipc/topology.h>
#include <ipc/control.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Audio Format & Pipeline Capabilities
 * ========================================================================= */

#define SOF_STATIC_RATE_8K      BIT(0)
#define SOF_STATIC_RATE_16K     BIT(1)
#define SOF_STATIC_RATE_32K     BIT(2)
#define SOF_STATIC_RATE_44K1    BIT(3)
#define SOF_STATIC_RATE_48K     BIT(4)
#define SOF_STATIC_RATE_96K     BIT(5)
#define SOF_STATIC_RATE_192K    BIT(6)

#define SOF_STATIC_FMT_S16_LE   BIT(SOF_IPC_FRAME_S16_LE)
#define SOF_STATIC_FMT_S24_4LE  BIT(SOF_IPC_FRAME_S24_4LE)
#define SOF_STATIC_FMT_S32_LE   BIT(SOF_IPC_FRAME_S32_LE)
#define SOF_STATIC_FMT_FLOAT    BIT(SOF_IPC_FRAME_FLOAT)

struct sof_static_caps {
	uint32_t formats;       /* Supported frame format mask */
	uint32_t rates;         /* Supported sample rate mask */
	uint16_t min_channels;  /* Minimum channel count */
	uint16_t max_channels;  /* Maximum channel count */
	uint32_t default_rate;  /* Default sample rate in Hz (e.g. 48000) */
	enum sof_ipc_frame default_fmt; /* Default frame format */
};

#define SOF_STATIC_CAPS(_fmt, _rate, _ch) \
	{ \
		.formats = BIT(_fmt), \
		.rates = SOF_STATIC_RATE_48K, \
		.min_channels = (_ch), \
		.max_channels = (_ch), \
		.default_rate = (_rate), \
		.default_fmt = (_fmt), \
	}

/* =========================================================================
 * Components / Processing Modules
 * ========================================================================= */

enum sof_static_comp_type {
	SOF_STATIC_COMP_MODULE = 0, /* Volume, EQ, DRC, TDFB, etc. */
	SOF_STATIC_COMP_HOST,       /* USB Audio / Host streaming endpoint */
	SOF_STATIC_COMP_DAI,        /* Hardware I2S / PDM DAI */
};

enum sof_static_ep_type {
	SOF_STATIC_EP_NONE = 0,
	SOF_STATIC_EP_USB_TERMINAL,
	SOF_STATIC_EP_DAI,
};

struct sof_static_comp {
	uint32_t id;                     /* Unique component ID */
	uint32_t pipeline_id;            /* Owning pipeline ID */
	const char *name;                /* Human-readable name (e.g. "VOL_PB") */
	enum sof_static_comp_type type;  /* Host, Module, or DAI */
	const struct sof_uuid *uuid;     /* Component UUID */
	uint32_t direction;              /* SOF_IPC_STREAM_PLAYBACK or CAPTURE */
	struct sof_static_caps caps;     /* Audio format capabilities */

	/* Hardware endpoint configuration */
	enum sof_static_ep_type ep_type;
	union {
		struct {
			uint32_t terminal_id;    /* UAC2 USB Terminal ID */
		} usb;
		struct {
			uint32_t dai_type;       /* e.g. SOF_DAI_ESP32_I2S */
			uint32_t dai_index;      /* DAI index (0, 1) */
			uint32_t format;         /* SOF_DAI_FMT_I2S */
		} dai;
	} ep;

	/* Initial configuration blob (ABI header + coefficients) */
	const void *init_blob;
	size_t init_blob_size;
};

#define SOF_STATIC_COMP_MODULE(...) \
	{ .type = SOF_STATIC_COMP_MODULE, __VA_ARGS__ }

#define SOF_STATIC_COMP_HOST(...) \
	{ .type = SOF_STATIC_COMP_HOST, .ep_type = SOF_STATIC_EP_USB_TERMINAL, __VA_ARGS__ }

#define SOF_STATIC_COMP_DAI(...) \
	{ .type = SOF_STATIC_COMP_DAI, .ep_type = SOF_STATIC_EP_DAI, __VA_ARGS__ }

/* =========================================================================
 * Intermediate Audio Buffers & Pipeline Routing
 * ========================================================================= */

struct sof_static_buffer {
	uint32_t id;                     /* Unique buffer ID */
	size_t size;                     /* Buffer byte size */
	enum sof_ipc_frame fmt;          /* Buffer frame format */
	uint32_t flags;                  /* Memory allocation flags */
};

#define SOF_STATIC_BUFFER(...) \
	{ .flags = SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, __VA_ARGS__ }

struct sof_static_route {
	uint32_t src_comp_id;            /* Upstream module ID */
	uint32_t buffer_id;              /* Intermediate buffer ID */
	uint32_t sink_comp_id;           /* Downstream module ID */
};

#define SOF_STATIC_ROUTE(...) \
	{ __VA_ARGS__ }

/* =========================================================================
 * PCMs (Pulse Code Modulated Endpoints)
 * ========================================================================= */

struct sof_static_pcm {
	uint32_t pcm_id;                 /* PCM index */
	const char *name;                /* e.g. "Speaker Playback" */
	uint32_t direction;              /* SOF_IPC_STREAM_PLAYBACK / CAPTURE */
	uint32_t pipeline_id;            /* Associated pipeline ID */
	uint32_t host_comp_id;           /* Host/endpoint component ID */
	struct sof_static_caps caps;     /* Supported stream capabilities */
};

#define SOF_STATIC_PCM(...) \
	{ __VA_ARGS__ }

/* =========================================================================
 * Kcontrols (Volume, Mute, Bypass, Presets, Coefficients)
 * ========================================================================= */

enum sof_static_ctrl_type {
	SOF_STATIC_CTRL_VOLUME = 0,      /* Linear / dB Volume fader */
	SOF_STATIC_CTRL_SWITCH,          /* Boolean Mute / Bypass switch */
	SOF_STATIC_CTRL_ENUM,            /* Multi-value selection */
	SOF_STATIC_CTRL_BINARY,          /* ABI data blob */
};

struct sof_static_kcontrol {
	uint32_t id;                     /* Control ID */
	const char *name;                /* Control name (e.g. "Master Volume") */
	enum sof_static_ctrl_type type;  /* Control type */
	uint32_t target_comp_id;         /* Component affected by this control */
	uint32_t param_id;               /* Parameter or command index */

	int32_t min;                     /* Minimum control value */
	int32_t max;                     /* Maximum control value */
	int32_t def;                     /* Default control value */
	uint32_t channels;               /* Number of channels */

	/* Bound external entity (e.g. UAC2 Feature Unit Entity ID) */
	uint8_t uac2_entity_id;
};

#define SOF_STATIC_KCONTROL_VOLUME(...) \
	{ .type = SOF_STATIC_CTRL_VOLUME, __VA_ARGS__ }

#define SOF_STATIC_KCONTROL_SWITCH(...) \
	{ .type = SOF_STATIC_CTRL_SWITCH, __VA_ARGS__ }

#define SOF_STATIC_KCONTROL_ENUM(...) \
	{ .type = SOF_STATIC_CTRL_ENUM, __VA_ARGS__ }

#define SOF_STATIC_KCONTROL_BINARY(...) \
	{ .type = SOF_STATIC_CTRL_BINARY, __VA_ARGS__ }

/* =========================================================================
 * Pipeline Descriptors & Top-Level Topology
 * ========================================================================= */

struct sof_static_pipeline_desc {
	uint32_t pipeline_id;            /* Pipeline ID */
	const char *name;                /* e.g. "Playback Pipeline" */
	uint32_t direction;              /* SOF_IPC_STREAM_PLAYBACK / CAPTURE */
	uint32_t priority;               /* Pipeline priority (0 = normal) */
	uint32_t core;                   /* Core affinity (0 or 1) */
	uint32_t period;                 /* Period in microseconds (e.g. 1000) */
	uint32_t frames_per_sched;       /* e.g. 48 */
	uint32_t time_domain;            /* e.g. SOF_TIME_DOMAIN_TIMER */
	uint32_t sched_comp_id;          /* Component driving scheduling */
	uint32_t source_comp_id;         /* Source component ID */
	uint32_t sink_comp_id;           /* Sink component ID */
};

struct sof_static_topology {
	const char *name;
	size_t num_pipelines;
	const struct sof_static_pipeline_desc *pipelines;
	size_t num_comps;
	const struct sof_static_comp *comps;
	size_t num_buffers;
	const struct sof_static_buffer *buffers;
	size_t num_routes;
	const struct sof_static_route *routes;
	size_t num_pcms;
	const struct sof_static_pcm *pcms;
	size_t num_controls;
	const struct sof_static_kcontrol *controls;
};

/* =========================================================================
 * Generic Engine Public APIs
 * ========================================================================= */

/**
 * @brief Initialize and instantiate the static topology graph.
 *
 * @param topo Pointer to the target static topology definition.
 * @return 0 on success, negative errno on failure.
 */
int sof_static_topology_init(const struct sof_static_topology *topo);

/**
 * @brief Retrieve the active static topology descriptor.
 */
const struct sof_static_topology *sof_static_topology_get(void);

/**
 * @brief Get pipeline by ID.
 */
struct pipeline *sof_static_pipeline_get(uint32_t pipeline_id);

/**
 * @brief Get component device by ID.
 */
struct comp_dev *sof_static_comp_get(uint32_t comp_id);

/**
 * @brief Get intermediate buffer by ID.
 */
struct comp_buffer *sof_static_buffer_get(uint32_t buffer_id);

/**
 * @brief Set kcontrol value by control ID.
 */
int sof_static_kcontrol_set(uint32_t ctrl_id, int32_t val);

/**
 * @brief Get kcontrol value by control ID.
 */
int sof_static_kcontrol_get(uint32_t ctrl_id, int32_t *val);

/**
 * @brief Find kcontrol ID by name.
 * @return Control ID >= 0 if found, negative errno if not found.
 */
int sof_static_kcontrol_find_by_name(const char *name);

/**
 * @brief Dispatch UAC2 feature unit control to registered kcontrols.
 */
int sof_static_kcontrol_set_by_uac2(uint8_t entity_id, uint8_t channel, int32_t val, bool is_volume);
int sof_static_kcontrol_get_by_uac2(uint8_t entity_id, uint8_t channel, int32_t *val, bool is_volume);

/**
 * @brief Trigger pipeline start/stop associated with a UAC2 terminal ID.
 */
int sof_static_pipeline_trigger_by_uac2_term(uint8_t terminal_id, bool start);

/**
 * @brief Set sample rate across all active static pipelines and endpoints.
 */
int sof_static_set_sample_rate(uint32_t rate);

/**
 * @brief Get current sample rate.
 */
uint32_t sof_static_get_sample_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_H__ */
