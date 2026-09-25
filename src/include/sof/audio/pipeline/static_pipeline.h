/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

/**
 * \file include/sof/audio/pipeline/static_pipeline.h
 * \brief Declarative Static Audio Pipeline Engine API
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * \defgroup static_pipeline_api Static Audio Pipeline Engine
 * @{
 *
 * The Static Audio Pipeline subsystem provides compile-time and runtime
 * declarative topology construction for hostless, microcontroller, and
 * standalone embedded platforms running Sound Open Firmware.
 *
 * It allows platforms to construct complete SOF processing graphs (pipelines,
 * modules, DAIs, intermediate buffers, routes, and kcontrols) directly at boot
 * without requiring dynamic IPC topology commands from a host operating system.
 */

#ifndef __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_H__
#define __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_H__

#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <ipc/topology.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/dai.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Audio Format & Pipeline Capabilities
 * ========================================================================= */

/** \brief Supported sample rate bitmasks for static capability negotiation */
#define SOF_STATIC_RATE_8K      SOF_RATE_8000     /**< 8000 Hz */
#define SOF_STATIC_RATE_11K     SOF_RATE_11025    /**< 11025 Hz */
#define SOF_STATIC_RATE_12K     SOF_RATE_12000    /**< 12000 Hz */
#define SOF_STATIC_RATE_16K     SOF_RATE_16000    /**< 16000 Hz */
#define SOF_STATIC_RATE_22K     SOF_RATE_22050    /**< 22050 Hz */
#define SOF_STATIC_RATE_24K     SOF_RATE_24000    /**< 24000 Hz */
#define SOF_STATIC_RATE_32K     SOF_RATE_32000    /**< 32000 Hz */
#define SOF_STATIC_RATE_44K1    SOF_RATE_44100    /**< 44100 Hz */
#define SOF_STATIC_RATE_48K     SOF_RATE_48000    /**< 48000 Hz */
#define SOF_STATIC_RATE_64K     SOF_RATE_64000    /**< 64000 Hz */
#define SOF_STATIC_RATE_88K2    SOF_RATE_88200    /**< 88200 Hz */
#define SOF_STATIC_RATE_96K     SOF_RATE_96000    /**< 96000 Hz */
#define SOF_STATIC_RATE_176K4   SOF_RATE_176400   /**< 176400 Hz */
#define SOF_STATIC_RATE_192K    SOF_RATE_192000   /**< 192000 Hz */
#define SOF_STATIC_RATE_384K    (1 << 14)         /**< 384000 Hz */

/** \brief Supported PCM frame format bitmasks (matching enum sof_ipc_frame) */
#define SOF_STATIC_FMT_S16_LE       BIT(SOF_IPC_FRAME_S16_LE)
#define SOF_STATIC_FMT_S24_4LE      BIT(SOF_IPC_FRAME_S24_4LE)
#define SOF_STATIC_FMT_S32_LE       BIT(SOF_IPC_FRAME_S32_LE)
#define SOF_STATIC_FMT_FLOAT        BIT(SOF_IPC_FRAME_FLOAT)
#define SOF_STATIC_FMT_S24_3LE      BIT(SOF_IPC_FRAME_S24_3LE)
#define SOF_STATIC_FMT_S24_4LE_MSB  BIT(SOF_IPC_FRAME_S24_4LE_MSB)
#define SOF_STATIC_FMT_U8           BIT(SOF_IPC_FRAME_U8)
#define SOF_STATIC_FMT_S16_4LE      BIT(SOF_IPC_FRAME_S16_4LE)
#define SOF_STATIC_FMT_A_LAW        BIT(SOF_IPC_FRAME_A_LAW)
#define SOF_STATIC_FMT_MU_LAW       BIT(SOF_IPC_FRAME_MU_LAW)

/** Mask covering all supported sample rates */
#define SOF_STATIC_RATE_MASK_ALL \
	(SOF_STATIC_RATE_8K | SOF_STATIC_RATE_11K | SOF_STATIC_RATE_12K | \
	 SOF_STATIC_RATE_16K | SOF_STATIC_RATE_22K | SOF_STATIC_RATE_24K | \
	 SOF_STATIC_RATE_32K | SOF_STATIC_RATE_44K1 | SOF_STATIC_RATE_48K | \
	 SOF_STATIC_RATE_64K | SOF_STATIC_RATE_88K2 | SOF_STATIC_RATE_96K | \
	 SOF_STATIC_RATE_176K4 | SOF_STATIC_RATE_192K | SOF_STATIC_RATE_384K)

/** Mask covering all supported frame formats */
#define SOF_STATIC_FMT_MASK_ALL \
	(SOF_STATIC_FMT_S16_LE | SOF_STATIC_FMT_S24_4LE | SOF_STATIC_FMT_S32_LE | \
	 SOF_STATIC_FMT_FLOAT | SOF_STATIC_FMT_S24_3LE | SOF_STATIC_FMT_S24_4LE_MSB | \
	 SOF_STATIC_FMT_U8 | SOF_STATIC_FMT_S16_4LE | SOF_STATIC_FMT_A_LAW | \
	 SOF_STATIC_FMT_MU_LAW)

/**
 * \brief Audio stream format and sample rate capability descriptor.
 */
struct sof_static_caps {
	uint32_t formats;       /**< Bitmask of supported frame formats (SOF_STATIC_FMT_*) */
	uint32_t rates;         /**< Bitmask of supported sample rates (SOF_STATIC_RATE_*) */
	uint16_t min_channels;  /**< Minimum channel count */
	uint16_t max_channels;  /**< Maximum channel count */
	uint32_t default_rate;  /**< Default sample rate in Hz (e.g. 48000) */
	uint32_t sink_rate;     /**< Target sink sample rate for converters (SRC/ASRC, 0 = same as default_rate) */
	enum sof_ipc_frame default_fmt; /**< Default frame format */
};

/**
 * \brief Helper macro to initialize a static capability structure.
 * \param _fmt Frame format (e.g. SOF_IPC_FRAME_S16_LE or SOF_IPC_FRAME_FLOAT).
 * \param _rate Default sample rate in Hz (e.g. 48000).
 * \param _ch Number of audio channels (e.g. 2).
 */
#define SOF_STATIC_CAPS(_fmt, _rate, _ch) \
	{ \
		.formats = BIT(_fmt), \
		.rates = (_rate == 8000) ? SOF_STATIC_RATE_8K : \
			 (_rate == 11025) ? SOF_STATIC_RATE_11K : \
			 (_rate == 12000) ? SOF_STATIC_RATE_12K : \
			 (_rate == 16000) ? SOF_STATIC_RATE_16K : \
			 (_rate == 22050) ? SOF_STATIC_RATE_22K : \
			 (_rate == 24000) ? SOF_STATIC_RATE_24K : \
			 (_rate == 32000) ? SOF_STATIC_RATE_32K : \
			 (_rate == 44100) ? SOF_STATIC_RATE_44K1 : \
			 (_rate == 48000) ? SOF_STATIC_RATE_48K : \
			 (_rate == 64000) ? SOF_STATIC_RATE_64K : \
			 (_rate == 88200) ? SOF_STATIC_RATE_88K2 : \
			 (_rate == 96000) ? SOF_STATIC_RATE_96K : \
			 (_rate == 176400) ? SOF_STATIC_RATE_176K4 : \
			 (_rate == 192000) ? SOF_STATIC_RATE_192K : \
			 (_rate == 384000) ? SOF_STATIC_RATE_384K : SOF_STATIC_RATE_48K, \
		.min_channels = (_ch), \
		.max_channels = (_ch), \
		.default_rate = (_rate), \
		.default_fmt = (_fmt), \
	}

/* =========================================================================
 * Components / Processing Modules
 * ========================================================================= */

/**
 * \brief Category of a static pipeline component.
 */
enum sof_static_comp_type {
	SOF_STATIC_COMP_MODULE = 0, /**< Audio processing module (Volume, EQ, DRC, etc.) */
	SOF_STATIC_COMP_HOST,       /**< Host or USB streaming endpoint */
	SOF_STATIC_COMP_DAI,        /**< Hardware Digital Audio Interface (I2S, PDM, SAI, SPDIF) */
};

/**
 * \brief Hardware endpoint binding type.
 */
enum sof_static_ep_type {
	SOF_STATIC_EP_NONE = 0,     /**< Standard internal processing module */
	SOF_STATIC_EP_USB_TERMINAL, /**< USB Audio Class 2 (UAC2) streaming terminal */
	SOF_STATIC_EP_DAI,          /**< Physical hardware DAI controller */
};

/**
 * \brief Declarative component descriptor.
 */
struct sof_static_comp {
	uint32_t id;                     /**< Unique component identifier */
	uint32_t pipeline_id;            /**< Owning pipeline identifier */
	const char *name;                /**< Human-readable diagnostic name (e.g. "VOL_PB") */
	enum sof_static_comp_type type;  /**< Component category: Module, Host, or DAI */
	const struct sof_uuid *uuid;     /**< Registered driver UUID (NULL for standard Host or DAI) */
	uint32_t direction;              /**< Audio stream direction: SOF_IPC_STREAM_PLAYBACK or CAPTURE */
	struct sof_static_caps caps;     /**< Format and sample rate capabilities */

	enum sof_static_ep_type ep_type; /**< Endpoint binding type */
	union {
		struct {
			uint32_t terminal_id;    /**< Bound UAC2 USB Terminal Entity ID */
		} usb;
		struct {
			uint32_t dai_type;       /**< DAI type (e.g. SOF_DAI_ESP32_I2S, SOF_DAI_IMX_SAI) */
			uint32_t dai_index;      /**< DAI hardware instance index */
			uint32_t format;         /**< Protocol/clock format mask (SOF_DAI_FMT_*) */
			int (*configure)(struct comp_dev *dev,
					 struct ipc_config_dai *dai_cfg,
					 struct sof_ipc_dai_config *spec_cfg); /**< Optional platform DAI endpoint config hook */
		} dai;
	} ep;

	const void *init_blob;           /**< Optional initial ABI configuration blob */
	size_t init_blob_size;           /**< Size of initial configuration blob in bytes */
};

/**
 * \brief Declare a processing module component.
 */
#define SOF_STATIC_COMP_MODULE(...) \
	{ .type = SOF_STATIC_COMP_MODULE, __VA_ARGS__ }

/**
 * \brief Declare a USB/Host streaming endpoint component.
 */
#define SOF_STATIC_COMP_HOST(...) \
	{ .type = SOF_STATIC_COMP_HOST, .ep_type = SOF_STATIC_EP_USB_TERMINAL, __VA_ARGS__ }

/**
 * \brief Declare a hardware DAI endpoint component.
 */
#define SOF_STATIC_COMP_DAI(...) \
	{ .type = SOF_STATIC_COMP_DAI, .ep_type = SOF_STATIC_EP_DAI, __VA_ARGS__ }

/* =========================================================================
 * Module Constructor Macros
 * ========================================================================= */

/**
 * \brief Generic constructor macro for an audio processing module component.
 *
 * \param _id Unique component ID.
 * \param _ppl Owning pipeline ID.
 * \param _name Human-readable component name string.
 * \param _dir Stream direction (SOF_IPC_STREAM_PLAYBACK or SOF_IPC_STREAM_CAPTURE).
 * \param _uuid Pointer to component driver UUID (e.g. &volume_uuid).
 * \param _fmt Default PCM frame format (enum sof_ipc_frame).
 * \param _rate Default stream sample rate in Hz.
 * \param _ch Default channel count.
 * \param _blob Optional pointer to initial configuration blob (or NULL).
 * \param _blob_sz Size of initial configuration blob in bytes (or 0).
 */
#define SOF_STATIC_MODULE(_id, _ppl, _name, _dir, _uuid, _fmt, _rate, _ch, _blob, _blob_sz) \
	{ \
		.id = (_id), \
		.pipeline_id = (_ppl), \
		.name = (_name), \
		.type = SOF_STATIC_COMP_MODULE, \
		.uuid = (_uuid), \
		.direction = (_dir), \
		.caps = SOF_STATIC_CAPS((_fmt), (_rate), (_ch)), \
		.init_blob = (_blob), \
		.init_blob_size = (_blob_sz), \
	}

/**
 * \brief Generic constructor macro for a sample rate converter component (SRC / ASRC).
 *
 * \param _id Unique component ID.
 * \param _ppl Owning pipeline ID.
 * \param _name Human-readable component name string.
 * \param _dir Stream direction (SOF_IPC_STREAM_PLAYBACK or SOF_IPC_STREAM_CAPTURE).
 * \param _uuid Pointer to component driver UUID (e.g. &src_uuid, &asrc_uuid).
 * \param _fmt Default PCM frame format (enum sof_ipc_frame).
 * \param _in_rate Source/input sample rate in Hz.
 * \param _out_rate Sink/output sample rate in Hz.
 * \param _ch Channel count.
 */
#define SOF_STATIC_MODULE_RATE_CONV(_id, _ppl, _name, _dir, _uuid, _fmt, _in_rate, _out_rate, _ch) \
	{ \
		.id = (_id), \
		.pipeline_id = (_ppl), \
		.name = (_name), \
		.type = SOF_STATIC_COMP_MODULE, \
		.uuid = (_uuid), \
		.direction = (_dir), \
		.caps = { \
			.formats = BIT(_fmt), \
			.rates = SOF_STATIC_RATE_MASK_ALL, \
			.min_channels = (_ch), \
			.max_channels = (_ch), \
			.default_rate = (_in_rate), \
			.sink_rate = (_out_rate), \
			.default_fmt = (_fmt), \
		}, \
	}

/* =========================================================================
 * Endpoint Constructor Macros
 * ========================================================================= */

/**
 * \brief Simple constructor macro for a USB / UAC2 streaming endpoint.
 *
 * \param _id Unique component ID.
 * \param _ppl Owning pipeline ID.
 * \param _name Human-readable component name.
 * \param _dir Stream direction (SOF_IPC_STREAM_PLAYBACK or SOF_IPC_STREAM_CAPTURE).
 * \param _fmt Default PCM frame format (enum sof_ipc_frame).
 * \param _rate Stream sample rate in Hz.
 * \param _ch Channel count.
 * \param _term_id USB Audio Class Terminal ID.
 */
#define SOF_STATIC_ENDPOINT_USB(_id, _ppl, _name, _dir, _fmt, _rate, _ch, _term_id) \
	{ \
		.id = (_id), \
		.pipeline_id = (_ppl), \
		.name = (_name), \
		.type = SOF_STATIC_COMP_HOST, \
		.direction = (_dir), \
		.caps = SOF_STATIC_CAPS((_fmt), (_rate), (_ch)), \
		.ep_type = SOF_STATIC_EP_USB_TERMINAL, \
		.ep.usb.terminal_id = (_term_id), \
	}

/**
 * \brief Generic constructor macro for a hardware Digital Audio Interface (DAI) endpoint.
 *
 * \param _id Unique component ID.
 * \param _ppl Owning pipeline ID.
 * \param _name Human-readable component name.
 * \param _dir Stream direction (SOF_IPC_STREAM_PLAYBACK or SOF_IPC_STREAM_CAPTURE).
 * \param _fmt Default PCM frame format (enum sof_ipc_frame).
 * \param _rate Stream sample rate in Hz.
 * \param _ch Channel count.
 * \param _type Hardware DAI type (e.g. SOF_DAI_INTEL_SSP, SOF_DAI_ESP32_I2S, etc.).
 * \param _idx Hardware DAI controller instance index.
 * \param _clk_fmt DAI clock format flags (e.g. SOF_DAI_FMT_I2S | SOF_DAI_FMT_CBC_CFC).
 */
#define SOF_STATIC_ENDPOINT_DAI(_id, _ppl, _name, _dir, _fmt, _rate, _ch, _type, _idx, _clk_fmt) \
	{ \
		.id = (_id), \
		.pipeline_id = (_ppl), \
		.name = (_name), \
		.type = SOF_STATIC_COMP_DAI, \
		.direction = (_dir), \
		.caps = SOF_STATIC_CAPS((_fmt), (_rate), (_ch)), \
		.ep_type = SOF_STATIC_EP_DAI, \
		.ep.dai.dai_type = (_type), \
		.ep.dai.dai_index = (_idx), \
		.ep.dai.format = (_clk_fmt), \
	}

/**
 * \brief Constructor macro for a hardware DAI endpoint with custom platform configuration callback.
 *
 * \param _id Unique component ID.
 * \param _ppl Owning pipeline ID.
 * \param _name Human-readable component name.
 * \param _dir Stream direction (SOF_IPC_STREAM_PLAYBACK or SOF_IPC_STREAM_CAPTURE).
 * \param _fmt Default PCM frame format (enum sof_ipc_frame).
 * \param _rate Stream sample rate in Hz.
 * \param _ch Channel count.
 * \param _type Hardware DAI type (e.g. SOF_DAI_INTEL_SSP, SOF_DAI_ESP32_I2S, etc.).
 * \param _idx Hardware DAI controller instance index.
 * \param _clk_fmt DAI clock format flags (e.g. SOF_DAI_FMT_I2S | SOF_DAI_FMT_CBC_CFC).
 * \param _cfg_fn Endpoint configuration callback function pointer.
 */
#define SOF_STATIC_ENDPOINT_DAI_CFG(_id, _ppl, _name, _dir, _fmt, _rate, _ch, _type, _idx, _clk_fmt, _cfg_fn) \
	{ \
		.id = (_id), \
		.pipeline_id = (_ppl), \
		.name = (_name), \
		.type = SOF_STATIC_COMP_DAI, \
		.direction = (_dir), \
		.caps = SOF_STATIC_CAPS((_fmt), (_rate), (_ch)), \
		.ep_type = SOF_STATIC_EP_DAI, \
		.ep.dai.dai_type = (_type), \
		.ep.dai.dai_index = (_idx), \
		.ep.dai.format = (_clk_fmt), \
		.ep.dai.configure = (_cfg_fn), \
	}

/* =========================================================================
 * Static Module Operations
 * ========================================================================= */

/**
 * \brief Static pipeline module driver operations.
 *
 * Audio processing modules register these operations to handle static
 * instantiation (synthesizing any module-specific IPC init configuration)
 * and runtime kcontrol manipulation (volume, switch/mute, enum) without
 * requiring the generic pipeline loader to contain module-specific logic.
 */
struct sof_static_module_ops {
	/** UUID of target audio component */
	const struct sof_uuid *uuid;

	/**
	 * \brief Instantiate and initialize the module component.
	 * \param[in] drv SOF component driver.
	 * \param[in] cfg IPC component configuration.
	 * \param[in] cdesc Static component descriptor.
	 * \param[in] period_us Owning pipeline scheduling period in microseconds.
	 * \return Created component device pointer, or NULL on error.
	 */
	struct comp_dev *(*create)(const struct comp_driver *drv,
				   struct comp_ipc_config *cfg,
				   const struct sof_static_comp *cdesc,
				   uint32_t period_us);

	/**
	 * \brief Apply linear volume gain to component.
	 * \param[in] dev Component device pointer.
	 * \param[in] channels Channel count to apply volume across.
	 * \param[in] val Linear volume value (0 to INT32_MAX).
	 * \return 0 on success, negative error code on failure.
	 */
	int (*apply_volume)(struct comp_dev *dev, uint32_t channels, int32_t val);

	/**
	 * \brief Apply binary switch/mute state to component.
	 * \param[in] dev Component device pointer.
	 * \param[in] channels Channel count.
	 * \param[in] val Binary switch value (0 = muted/bypass, 1 = unmuted/active).
	 * \return 0 on success, negative error code on failure.
	 */
	int (*apply_switch)(struct comp_dev *dev, uint32_t channels, int32_t val);

	/**
	 * \brief Apply enumerated route/value to component.
	 * \param[in] dev Component device pointer.
	 * \param[in] channel Channel index.
	 * \param[in] val Selected enumeration value.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*apply_enum)(struct comp_dev *dev, uint32_t channel, int32_t val);

	/** List node for registered ops */
	struct list_item list;
};

/**
 * \brief Macro to automatically register static module operations at system startup.
 *
 * In Zephyr RTOS environments, registers using SYS_INIT at APPLICATION level.
 * In non-Zephyr environments (e.g. POSIX tests), registers using constructor attribute.
 *
 * \param name Unique identifier suffix for initialization routine.
 * \param ops Pointer to struct sof_static_module_ops.
 */
#if defined(__ZEPHYR__)
#include <zephyr/init.h>
#define DECLARE_STATIC_MODULE_OPS(name, ops) \
	static int _static_ops_init_##name(void) \
	{ \
		return sof_static_register_module_ops(ops); \
	} \
	SYS_INIT(_static_ops_init_##name, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY)
#else
#define DECLARE_STATIC_MODULE_OPS(name, ops) \
	__attribute__((constructor)) __used \
	static void _static_ops_init_##name(void) \
	{ \
		sof_static_register_module_ops(ops); \
	}
#endif

/* =========================================================================
 * Intermediate Audio Buffers & Pipeline Routing
 * ========================================================================= */

/**
 * \brief Declarative intermediate audio buffer descriptor.
 */
struct sof_static_buffer {
	uint32_t id;                     /**< Unique buffer identifier */
	size_t size;                     /**< Total buffer allocation size in bytes */
	enum sof_ipc_frame fmt;          /**< Buffer audio frame format */
	uint32_t rate;                   /**< Sample rate in Hz (0 = inherit from producer component) */
	uint16_t channels;               /**< Channel count (0 = inherit from producer component) */
	uint32_t flags;                  /**< Memory allocation flags (e.g. SOF_MEM_FLAG_DMA) */
};

/**
 * \brief Helper macro to declare an intermediate audio buffer with standard flags.
 */
#define SOF_STATIC_BUFFER(...) \
	{ .flags = SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, __VA_ARGS__ }

/**
 * \brief Declarative graph route connecting an upstream component to a downstream component.
 */
struct sof_static_route {
	uint32_t src_comp_id;            /**< Upstream producer component ID */
	uint32_t buffer_id;              /**< Intermediate buffer ID linking the components */
	uint32_t sink_comp_id;           /**< Downstream consumer component ID */
};

/**
 * \brief Helper macro to declare a connection route.
 */
#define SOF_STATIC_ROUTE(...) \
	{ __VA_ARGS__ }

/* =========================================================================
 * PCMs (Pulse Code Modulated Endpoints)
 * ========================================================================= */

/**
 * \brief Declarative PCM endpoint descriptor.
 */
struct sof_static_pcm {
	uint32_t pcm_id;                 /**< Logical PCM stream index */
	const char *name;                /**< Human-readable PCM name (e.g. "Speaker Playback") */
	uint32_t direction;              /**< Audio direction: SOF_IPC_STREAM_PLAYBACK or CAPTURE */
	uint32_t pipeline_id;            /**< Associated pipeline ID */
	uint32_t host_comp_id;           /**< Host/USB endpoint component ID */
	struct sof_static_caps caps;     /**< Supported PCM capabilities */
};

/**
 * \brief Helper macro to declare a static PCM stream.
 */
#define SOF_STATIC_PCM(...) \
	{ __VA_ARGS__ }

/* =========================================================================
 * Kcontrols (Volume, Mute, Bypass, Presets, Coefficients)
 * ========================================================================= */

/**
 * \brief Type of static kcontrol.
 */
enum sof_static_ctrl_type {
	SOF_STATIC_CTRL_VOLUME = 0,      /**< Linear or dB volume fader */
	SOF_STATIC_CTRL_SWITCH,          /**< Boolean mute or module bypass switch */
	SOF_STATIC_CTRL_ENUM,            /**< Multi-value enumeration selector */
	SOF_STATIC_CTRL_BINARY,          /**< Raw binary parameter or coefficient blob */
};

/**
 * \brief Declarative static kcontrol descriptor.
 */
struct sof_static_kcontrol {
	uint32_t id;                     /**< Unique control identifier */
	const char *name;                /**< Human-readable control name (e.g. "Master Volume") */
	enum sof_static_ctrl_type type;  /**< Control type */
	uint32_t target_comp_id;         /**< Target component ID affected by this control (0 for custom) */
	uint32_t param_id;               /**< Optional parameter or command index */

	int32_t min;                     /**< Minimum valid value */
	int32_t max;                     /**< Maximum valid value */
	int32_t def;                     /**< Default initial value applied at boot */
	uint32_t channels;               /**< Number of channels affected by this control */

	uint8_t uac2_entity_id;          /**< Bound external entity ID (e.g. UAC2 Feature Unit) */
};

/** \brief Helper macro to declare a volume kcontrol */
#define SOF_STATIC_KCONTROL_VOLUME(...) \
	{ .type = SOF_STATIC_CTRL_VOLUME, __VA_ARGS__ }

/** \brief Helper macro to declare a boolean switch/bypass kcontrol */
#define SOF_STATIC_KCONTROL_SWITCH(...) \
	{ .type = SOF_STATIC_CTRL_SWITCH, __VA_ARGS__ }

/** \brief Helper macro to declare an enumeration selector kcontrol */
#define SOF_STATIC_KCONTROL_ENUM(...) \
	{ .type = SOF_STATIC_CTRL_ENUM, __VA_ARGS__ }

/** \brief Helper macro to declare a binary configuration kcontrol */
#define SOF_STATIC_KCONTROL_BINARY(...) \
	{ .type = SOF_STATIC_CTRL_BINARY, __VA_ARGS__ }

/* =========================================================================
 * Pipeline Descriptors & Top-Level Topology
 * ========================================================================= */

/**
 * \brief Declarative audio pipeline descriptor.
 */
struct sof_static_pipeline_desc {
	uint32_t pipeline_id;            /**< Unique pipeline identifier */
	const char *name;                /**< Human-readable pipeline name */
	uint32_t direction;              /**< Audio direction: SOF_IPC_STREAM_PLAYBACK or CAPTURE */
	uint32_t priority;               /**< Scheduling priority (0 = normal) */
	uint32_t core;                   /**< Core affinity index (0, 1, ...) */
	uint32_t period;                 /**< Scheduling period in microseconds (e.g. 1000) */
	uint32_t frames_per_sched;       /**< Number of frames processed per schedule tick */
	uint32_t time_domain;            /**< Scheduling time domain (SOF_TIME_DOMAIN_TIMER or DMA) */
	uint32_t default_rate;           /**< Default stream rate in Hz (e.g. 48000, 0 = auto) */
	uint16_t default_channels;       /**< Default channel count (e.g. 2, 0 = auto) */
	uint32_t sched_comp_id;          /**< Component driving pipeline scheduling */
	uint32_t source_comp_id;         /**< Primary source endpoint component ID */
	uint32_t sink_comp_id;           /**< Primary sink endpoint component ID */
};

/**
 * \brief Custom kcontrol callback handler type.
 *
 * Allows platforms or applications to register custom kcontrol dispatch
 * logic for non-module controls (e.g. clock modes, routes, injector switches).
 *
 * \param[in] ctl Pointer to the static kcontrol descriptor.
 * \param[in] val Value to write.
 * \param[in,out] priv Private user/platform context.
 * \return 0 on success, negative errno on failure.
 */
typedef int (*sof_static_kcontrol_handler_fn)(const struct sof_static_kcontrol *ctl,
					      int32_t val, void *priv);

/**
 * \brief Top-level static audio topology descriptor.
 */
struct sof_static_topology {
	const char *name;                                   /**< Human-readable topology name */
	size_t num_pipelines;                               /**< Number of pipelines in this topology */
	const struct sof_static_pipeline_desc *pipelines;   /**< Array of pipeline descriptors */
	size_t num_comps;                                   /**< Number of components */
	const struct sof_static_comp *comps;                /**< Array of component descriptors */
	size_t num_buffers;                                 /**< Number of intermediate buffers */
	const struct sof_static_buffer *buffers;            /**< Array of buffer descriptors */
	size_t num_routes;                                  /**< Number of graph connection routes */
	const struct sof_static_route *routes;              /**< Array of connection routes */
	size_t num_pcms;                                    /**< Number of logical PCMs */
	const struct sof_static_pcm *pcms;                  /**< Array of PCM descriptors */
	size_t num_controls;                                /**< Number of kcontrols */
	const struct sof_static_kcontrol *controls;         /**< Array of kcontrol descriptors */

	sof_static_kcontrol_handler_fn custom_control_handler; /**< Optional platform control hook */
	void *custom_control_data;                             /**< Platform context passed to hook */
};

/* =========================================================================
 * Generic Engine Public APIs
 * ========================================================================= */

/**
 * \brief Initialize and instantiate the static topology graph.
 *
 * Allocates pipelines, instantiates components from the driver registry,
 * initializes intermediate buffers, establishes graph connections, negotiates
 * component stream parameters, and prepares all pipelines for streaming.
 *
 * \param[in] topo Pointer to the target static topology definition.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_topology_init(const struct sof_static_topology *topo);

/**
 * \brief Retrieve the currently active static topology descriptor.
 * \return Pointer to active topology descriptor, or NULL if uninitialized.
 */
const struct sof_static_topology *sof_static_topology_get(void);

/**
 * \brief Retrieve a pipeline instance by its pipeline ID.
 * \param[in] pipeline_id Unique pipeline ID.
 * \return Pointer to struct pipeline, or NULL if not found.
 */
struct pipeline *sof_static_pipeline_get(uint32_t pipeline_id);

/**
 * \brief Retrieve a component device instance by its component ID.
 * \param[in] comp_id Unique component ID.
 * \return Pointer to struct comp_dev, or NULL if not found.
 */
struct comp_dev *sof_static_comp_get(uint32_t comp_id);

/**
 * \brief Retrieve an intermediate buffer instance by its buffer ID.
 * \param[in] buffer_id Unique buffer ID.
 * \return Pointer to struct comp_buffer, or NULL if not found.
 */
struct comp_buffer *sof_static_buffer_get(uint32_t buffer_id);

/**
 * \brief Register static module operations for an audio component.
 *
 * Audio processing modules register static operations at boot or module load
 * time to handle static component instantiation and kcontrol dispatch without
 * hardcoded component logic in the generic pipeline engine.
 *
 * \param[in,out] ops Pointer to module operations structure.
 * \return 0 on success, negative error code on failure.
 */
int sof_static_register_module_ops(struct sof_static_module_ops *ops);

/**
 * \brief Find registered static module operations by component UUID.
 * \param[in] uuid UUID of target component.
 * \return Pointer to registered ops, or NULL if not found.
 */
const struct sof_static_module_ops *sof_static_find_module_ops(const struct sof_uuid *uuid);

struct ipc4_base_module_cfg;

/**
 * \brief Synthesize standard IPC4 base module configuration structure.
 * \param[out] base_cfg Target base configuration to populate.
 * \param[in] cdesc Static component descriptor with format and rate capabilities.
 * \param[in] period_us Pipeline scheduling period in microseconds.
 */
void sof_static_init_base_cfg(struct ipc4_base_module_cfg *base_cfg,
			      const struct sof_static_comp *cdesc,
			      uint32_t period_us);

/**
 * \brief Default fallback creation for standard IPC4 module adapters.
 * \param[in] drv SOF component driver.
 * \param[in] cfg IPC component configuration.
 * \param[in] cdesc Static component descriptor.
 * \param[in] period_us Owning pipeline scheduling period in microseconds.
 * \return Created comp_dev pointer, or NULL on failure.
 */
struct comp_dev *sof_static_module_create_default(const struct comp_driver *drv,
						  struct comp_ipc_config *cfg,
						  const struct sof_static_comp *cdesc,
						  uint32_t period_us);

/**
 * \brief Set kcontrol value by control ID.
 *
 * Dispatches to the target module via its registered static operations
 * (struct sof_static_module_ops), or invokes the topology's custom_control_handler.
 *
 * \param[in] ctrl_id Unique control ID.
 * \param[in] val Value to apply.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_set(uint32_t ctrl_id, int32_t val);

/**
 * \brief Retrieve cached kcontrol value by control ID.
 * \param[in] ctrl_id Unique control ID.
 * \param[out] val Pointer to store retrieved control value.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_get(uint32_t ctrl_id, int32_t *val);

/**
 * \brief Find kcontrol ID by human-readable name.
 * \param[in] name Control name string to search for.
 * \return Control ID >= 0 if found, -ENOENT if not found.
 */
int sof_static_kcontrol_find_by_name(const char *name);

/**
 * \brief Dispatch UAC2 feature unit control to registered kcontrols.
 * \param[in] entity_id UAC2 Feature Unit Entity ID.
 * \param[in] channel Audio channel index (0 = master/all).
 * \param[in] val Value from USB request (e.g. 8.8 fixed-point dB volume or boolean mute).
 * \param[in] is_volume True for volume command, false for mute command.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_set_by_uac2(uint8_t entity_id, uint8_t channel, int32_t val, bool is_volume);

/**
 * \brief Retrieve kcontrol value formatted for UAC2 feature unit response.
 * \param[in] entity_id UAC2 Feature Unit Entity ID.
 * \param[in] channel Audio channel index.
 * \param[out] val Pointer to store retrieved value.
 * \param[in] is_volume True for volume query, false for mute query.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_get_by_uac2(uint8_t entity_id, uint8_t channel, int32_t *val, bool is_volume);

/**
 * \brief Trigger pipeline start or stop associated with a UAC2 terminal ID.
 * \param[in] terminal_id Bound UAC2 Terminal Entity ID.
 * \param[in] start True to start pipeline, false to pause/stop.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_trigger_by_uac2_term(uint8_t terminal_id, bool start);

/**
 * \brief Start a static pipeline by pipeline ID.
 *
 * Prepares the pipeline and constituent components if needed, propagates
 * start triggers, transitions state to COMP_STATE_ACTIVE, and activates
 * scheduling copy tasks.
 *
 * \param[in] pipeline_id Target pipeline ID.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_start(uint32_t pipeline_id);

/**
 * \brief Stop a static pipeline by pipeline ID.
 *
 * Propagates stop triggers, cancels scheduling copy tasks, and transitions
 * pipeline state to COMP_STATE_PAUSED.
 *
 * \param[in] pipeline_id Target pipeline ID.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_stop(uint32_t pipeline_id);

/**
 * \brief Directly trigger a static pipeline start or stop by pipeline ID.
 *
 * Synchronously transitions the pipeline state machine, propagates triggers
 * to all constituent components, and manages scheduling task execution.
 * Dispatches directly to sof_static_pipeline_start() or sof_static_pipeline_stop().
 *
 * \param[in] pipeline_id Target pipeline ID.
 * \param[in] start True to start pipeline, false to stop.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_trigger(uint32_t pipeline_id, bool start);

/**
 * \brief Set global sample rate across active static pipelines.
 *
 * In pipelines with sample rate converters (SRC / ASRC), individual pipeline
 * sections maintain their distinct sample rates according to their component
 * capabilities and buffer configurations.
 *
 * \param[in] rate Target primary sample rate in Hz (e.g. 48000).
 * \return 0 on success.
 */
int sof_static_set_sample_rate(uint32_t rate);

/**
 * \brief Retrieve current primary sample rate.
 * \return Current sample rate in Hz.
 */
uint32_t sof_static_get_sample_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_H__ */

/** @} */
