// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

/**
 * \file audio/pipeline/static_pipeline_loader.c
 * \brief Generic Static Audio Topology Loader Engine
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Implements the core runtime engine for declarative static audio pipelines.
 * In microcontroller, hostless, and standalone embedded environments, this engine
 * instantiates SOF components, buffers, routes, and kcontrols directly at boot
 * without dynamic IPC topology commands from a host operating system.
 */

#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/lib/dai.h>
#include <ipc/topology.h>
#include <ipc/control.h>
#include <ipc/dai.h>
#include <ipc4/gateway.h>
#include <kernel/header.h>
#include <module/ipc4/base-config.h>
#include <rtos/sof.h>
#include <rtos/alloc.h>
#include <rtos/task.h>
#include <rtos/userspace_helper.h>
#include <sof/schedule/schedule.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "../copier/copier.h"
#include "static_pipeline_modules.h"

LOG_MODULE_REGISTER(static_pipeline_loader, CONFIG_SOF_LOG_LEVEL);

/*
 * Capacity limits for static topology tracking tables.
 * Sourced from Kconfig to allow memory-constrained microcontrollers
 * and embedded DSPs to scale tracking buffers appropriately.
 */
#ifndef CONFIG_STATIC_PIPELINE_MAX_PIPELINES
#define CONFIG_STATIC_PIPELINE_MAX_PIPELINES 16
#endif
#ifndef CONFIG_STATIC_PIPELINE_MAX_COMPS
#define CONFIG_STATIC_PIPELINE_MAX_COMPS 64
#endif
#ifndef CONFIG_STATIC_PIPELINE_MAX_BUFFERS
#define CONFIG_STATIC_PIPELINE_MAX_BUFFERS 64
#endif
#ifndef CONFIG_STATIC_PIPELINE_MAX_CONTROLS
#define CONFIG_STATIC_PIPELINE_MAX_CONTROLS 64
#endif
#ifndef CONFIG_STATIC_PIPELINE_MAX_ROUTES
#define CONFIG_STATIC_PIPELINE_MAX_ROUTES 128
#endif

#define MAX_STATIC_PIPELINES    CONFIG_STATIC_PIPELINE_MAX_PIPELINES
#define MAX_STATIC_COMPS        CONFIG_STATIC_PIPELINE_MAX_COMPS
#define MAX_STATIC_BUFFERS      CONFIG_STATIC_PIPELINE_MAX_BUFFERS
#define MAX_STATIC_CONTROLS     CONFIG_STATIC_PIPELINE_MAX_CONTROLS
#define MAX_STATIC_ROUTES       CONFIG_STATIC_PIPELINE_MAX_ROUTES

/* Active declarative topology descriptor */
static APP_SYSUSER_DATA const struct sof_static_topology *s_active_topo;

/* Static tracking tables for fast ID-to-pointer lookups */
static APP_SYSUSER_BSS struct pipeline *s_pipelines[MAX_STATIC_PIPELINES];
static APP_SYSUSER_BSS uint32_t s_pipeline_ids[MAX_STATIC_PIPELINES];
static APP_SYSUSER_BSS uint32_t s_pipeline_rates[MAX_STATIC_PIPELINES];
static APP_SYSUSER_BSS size_t s_num_pipelines;

static APP_SYSUSER_BSS struct comp_dev *s_comps[MAX_STATIC_COMPS];
static APP_SYSUSER_BSS uint32_t s_comp_ids[MAX_STATIC_COMPS];
static APP_SYSUSER_BSS size_t s_num_comps;

static APP_SYSUSER_BSS struct comp_buffer *s_buffers[MAX_STATIC_BUFFERS];
static APP_SYSUSER_BSS uint32_t s_buffer_ids[MAX_STATIC_BUFFERS];
static APP_SYSUSER_BSS size_t s_num_buffers;

/* Cached kcontrol values */
static APP_SYSUSER_BSS int32_t s_control_vals[MAX_STATIC_CONTROLS];

/**
 * \brief Calculate valid sample byte size for a given PCM frame format.
 *
 * Handles special case SOF_IPC_FRAME_S16_4LE where 16 valid bits reside in a
 * 32-bit container, returning 2 valid bytes rather than container size 4.
 *
 * \param[in] fmt PCM frame format enum.
 * \return Valid sample bytes (e.g. 2 for S16/S16_4LE, 3 for S24_3LE, 4 for S24_4LE/S32/FLOAT).
 */
static inline uint32_t static_get_sample_valid_bytes(enum sof_ipc_frame fmt)
{
	if (fmt == SOF_IPC_FRAME_S16_4LE)
		return 2;
	return get_sample_bitdepth(fmt) / 8;
}

/**
 * \brief Convert sample rate in Hz to capability bitmask.
 * \param[in] rate Sample rate in Hz.
 * \return Matching SOF_STATIC_RATE_* bitmask or 0.
 */
static inline uint32_t sof_static_rate_to_mask(uint32_t rate)
{
	/* Map standard audio sampling rates to static capability bitmasks */
	switch (rate) {
	case 8000:   return SOF_STATIC_RATE_8K;
	case 11025:  return SOF_STATIC_RATE_11K;
	case 12000:  return SOF_STATIC_RATE_12K;
	case 16000:  return SOF_STATIC_RATE_16K;
	case 22050:  return SOF_STATIC_RATE_22K;
	case 24000:  return SOF_STATIC_RATE_24K;
	case 32000:  return SOF_STATIC_RATE_32K;
	case 44100:  return SOF_STATIC_RATE_44K1;
	case 48000:  return SOF_STATIC_RATE_48K;
	case 64000:  return SOF_STATIC_RATE_64K;
	case 88200:  return SOF_STATIC_RATE_88K2;
	case 96000:  return SOF_STATIC_RATE_96K;
	case 176400: return SOF_STATIC_RATE_176K4;
	case 192000: return SOF_STATIC_RATE_192K;
	case 384000: return SOF_STATIC_RATE_384K;
	default:     return 0;
	}
}

/**
 * \brief Helper to find declarative component descriptor by unique component ID.
 * \param[in] topo Active topology pointer.
 * \param[in] comp_id Target component identifier.
 * \return Pointer to matched struct sof_static_comp or NULL.
 */
static const struct sof_static_comp *
sof_static_find_comp_desc(const struct sof_static_topology *topo, uint32_t comp_id)
{
	if (!topo)
		return NULL;

	/* Linear search across declared components */
	for (size_t c = 0; c < topo->num_comps; c++) {
		if (topo->comps[c].id == comp_id)
			return &topo->comps[c];
	}
	return NULL;
}

/**
 * \brief Helper to find declarative pipeline descriptor by unique pipeline ID.
 * \param[in] topo Active topology pointer.
 * \param[in] pipeline_id Target pipeline identifier.
 * \return Pointer to matched struct sof_static_pipeline_desc or NULL.
 */
static const struct sof_static_pipeline_desc *
sof_static_find_pipeline_desc(const struct sof_static_topology *topo, uint32_t pipeline_id)
{
	if (!topo)
		return NULL;

	for (size_t p = 0; p < topo->num_pipelines; p++) {
		if (topo->pipelines[p].pipeline_id == pipeline_id)
			return &topo->pipelines[p];
	}
	return NULL;
}

/**
 * \brief Send initial binary configuration blob (e.g. EQ coefficients, DRC tuning) to a component.
 *
 * In hostless environments, modules requiring configuration tables (such as equalizer
 * filter coefficients or DRC curves) must be configured directly at boot. This routine
 * dispatches the initial configuration blob via the IPC4 module adapter's set_configuration ops.
 *
 * \param[in] dev Target component device pointer.
 * \param[in] abi_blob Pointer to ABI configuration blob starting with struct sof_abi_hdr.
 * \param[in] abi_blob_total_size Total size of blob in bytes including header.
 * \return 0 on success, negative errno on failure.
 */
static int send_comp_config(struct comp_dev *dev, const void *abi_blob, size_t abi_blob_total_size)
{
	/* Validate component pointer and verify minimum blob size covers ABI header */
	if (!dev || !abi_blob || abi_blob_total_size < sizeof(struct sof_abi_hdr)) {
		LOG_ERR("send_comp_config: invalid parameter or truncated blob (size %zu)",
			abi_blob_total_size);
		return -EINVAL;
	}

	const struct sof_abi_hdr *hdr = (const struct sof_abi_hdr *)abi_blob;

	/* Verify payload length fits within supplied total size */
	if (hdr->size > abi_blob_total_size - sizeof(struct sof_abi_hdr)) {
		LOG_ERR("send_comp_config: payload size %u exceeds total size %zu",
			hdr->size, abi_blob_total_size);
		return -EINVAL;
	}

	struct processing_module *mod = comp_mod(dev);

	/* Verify the component's module adapter driver provides set_configuration */
	if (!mod || !mod->dev || !mod->dev->drv || !mod->dev->drv->adapter_ops ||
	    !mod->dev->drv->adapter_ops->set_configuration) {
		LOG_ERR("No adapter_ops set_configuration for comp %d", dev->ipc_config.id);
		return -EINVAL;
	}

	/* Pass raw configuration payload directly following ABI header to module adapter */
	const uint8_t *raw_data = (const uint8_t *)abi_blob + sizeof(struct sof_abi_hdr);
	int ret = mod->dev->drv->adapter_ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE,
								hdr->size, raw_data,
								hdr->size, NULL, 0);

	/* Log configuration status for diagnostics */
	if (ret < 0)
		LOG_ERR("set_configuration failed for comp %d: ret %d", dev->ipc_config.id, ret);
	else
		LOG_INF("Configuration blob loaded for comp %d (%u bytes)", dev->ipc_config.id, hdr->size);

	return ret;
}

/**
 * \brief Initialize audio stream and buffer parameters for an intermediate connection buffer.
 *
 * Configures the IPC stream parameters (sampling rate, channel count, frame format,
 * container sizes, valid bit depth, host period bytes, and ALSA channel map) and
 * synchronizes the underlying audio stream (buf->stream) as well as its associated
 * sink and source interfaces.
 *
 * Intermediate circular buffers connect adjacent processing components in the
 * graph. Initializing their stream parameters and reset state ensures that
 * buffer_set_params() and sink/source getters establish identical format
 * expectations prior to component parameter negotiation and streaming.
 *
 * \param[in,out] buf Pointer to allocated intermediate component buffer.
 * \param[in] dir Audio stream direction (SOF_IPC_STREAM_PLAYBACK or SOF_IPC_STREAM_CAPTURE).
 * \param[in] fmt PCM audio frame format (enum sof_ipc_frame).
 * \param[in] rate Audio sampling rate in Hz.
 * \param[in] channels Audio channel count.
 */
static void init_buffer_params(struct comp_buffer *buf, uint32_t dir, enum sof_ipc_frame fmt,
			       uint32_t rate, uint32_t channels)
{
	if (!buf)
		return;

	/* Calculate sample container and valid byte sizes based on the PCM format */
	uint32_t sample_bytes = get_sample_bytes(fmt);
	uint32_t valid_bytes = static_get_sample_valid_bytes(fmt);

	/* Number of audio frames per 1 millisecond scheduling period */
	uint32_t frames = rate / 1000;

	/* Populate standard IPC stream parameters structure */
	struct sof_ipc_stream_params params;
	memset(&params, 0, sizeof(params));
	params.rate = rate;
	params.channels = channels;
	params.frame_fmt = fmt;
	params.sample_container_bytes = sample_bytes;
	params.sample_valid_bytes = valid_bytes;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.host_period_bytes = frames * channels * sample_bytes;
	params.direction = dir;

	/* Populate standard channel mapping for mono, stereo, and multi-channel streams */
	for (uint32_t c = 0; c < SOF_IPC_MAX_CHANNELS; c++) {
		if (c == 0)
			params.chmap[c] = (channels == 1) ? SOF_CHMAP_MONO : SOF_CHMAP_FL;
		else if (c == 1 && channels > 1)
			params.chmap[c] = SOF_CHMAP_FR;
		else if (c < channels)
			params.chmap[c] = c + 1;
		else
			params.chmap[c] = SOF_CHMAP_NA;
	}

	/* Apply stream parameters to buffer structure */
	buffer_set_params(buf, &params, BUFFER_UPDATE_FORCE);

	/* Configure circular audio stream parameters and byte alignment */
	audio_stream_set_valid_fmt(&buf->stream, fmt);
	audio_stream_set_rate(&buf->stream, rate);
	audio_stream_set_channels(&buf->stream, channels);
	audio_stream_set_frm_fmt(&buf->stream, fmt);
	audio_stream_set_align(SOF_FRAME_BYTE_ALIGN, sample_bytes, &buf->stream);
	audio_stream_reset(&buf->stream);

	/* Synchronize sink interface parameters so downstream consumers match */
	struct sof_sink *sink = audio_buffer_get_sink(&buf->audio_buffer);
	if (sink) {
		sink_set_valid_fmt(sink, fmt);
		sink_set_rate(sink, rate);
		sink_set_channels(sink, channels);
		sink_set_frm_fmt(sink, fmt);
		sink_set_alignment_constants(sink, SOF_FRAME_BYTE_ALIGN, sample_bytes);
	}

	/* Synchronize source interface parameters so upstream producers match */
	struct sof_source *source = audio_buffer_get_source(&buf->audio_buffer);
	if (source) {
		source_set_valid_fmt(source, fmt);
		source_set_rate(source, rate);
		source_set_channels(source, channels);
		source_set_frm_fmt(source, fmt);
		source_set_alignment_constants(source, SOF_FRAME_BYTE_ALIGN, sample_bytes);
	}
}

/**
 * \brief Stage 1: Allocate top-level pipelines.
 *
 * Allocate pipeline objects first because components and buffers require an
 * owning pipeline context for scheduling, buffer allocation, and clock domains.
 * Each pipeline maintains its own sampling rate, scheduling period, and time domain.
 *
 * \param[in] topo Topology descriptor pointer.
 * \return 0 on success, negative errno on failure.
 */
static int sof_static_topology_init_pipelines(const struct sof_static_topology *topo)
{
	for (size_t i = 0; i < topo->num_pipelines && i < MAX_STATIC_PIPELINES; i++) {
		const struct sof_static_pipeline_desc *pdesc = &topo->pipelines[i];
		struct pipeline *pipe = pipeline_new(NULL, pdesc->pipeline_id, pdesc->priority,
						     pdesc->pipeline_id, NULL);
		if (!pipe) {
			LOG_ERR("Failed to allocate pipeline %u (%s)", pdesc->pipeline_id, pdesc->name);
			return -ENOMEM;
		}

		/* Configure pipeline scheduling parameters from declarative descriptor */
		pipe->pipeline_id = pdesc->pipeline_id;
		pipe->core = pdesc->core;
		pipe->period = pdesc->period ? pdesc->period : 1000;
		pipe->frames_per_sched = pdesc->frames_per_sched ? pdesc->frames_per_sched : 48;
		pipe->time_domain = pdesc->time_domain;

		/* Record pipeline instance and sample rate in static tracking arrays */
		s_pipelines[s_num_pipelines] = pipe;
		s_pipeline_ids[s_num_pipelines] = pdesc->pipeline_id;
		s_pipeline_rates[s_num_pipelines] = pdesc->default_rate ? pdesc->default_rate : 48000;
		s_num_pipelines++;

		LOG_INF("Pipeline %u ('%s') initialized (period %u us, %u frames, %u Hz)",
			pdesc->pipeline_id, pdesc->name ? pdesc->name : "", pipe->period,
			pipe->frames_per_sched, s_pipeline_rates[s_num_pipelines - 1]);
	}

	return 0;
}

/**
 * \brief Instantiate a hardware DAI endpoint component.
 *
 * \param[in] cdesc Component descriptor.
 * \param[in] drv Component driver pointer.
 * \param[in] cfg Standard component IPC configuration.
 * \return Pointer to created comp_dev, or NULL on failure.
 */
static struct comp_dev *sof_static_init_dai_comp(const struct sof_static_comp *cdesc,
						 const struct comp_driver *drv,
						 const struct comp_ipc_config *cfg,
						 const struct sof_static_topology *topo)
{
	/* Validate component and driver descriptor pointers */
	if (!cdesc || !drv || !cfg) {
		LOG_ERR("sof_static_init_dai_comp: invalid NULL arguments");
		return NULL;
	}

	const struct sof_static_pipeline_desc *pdesc =
		sof_static_find_pipeline_desc(topo, cdesc->pipeline_id);

	/* Derive DAI sampling frequency from component caps or owning pipeline rate */
	uint32_t dai_rate = cdesc->caps.default_rate;
	if (!dai_rate && pdesc)
		dai_rate = pdesc->default_rate;
	if (!dai_rate)
		dai_rate = 48000;

	struct ipc_config_dai dai_cfg = {
		.type = cdesc->ep.dai.dai_type,
		.dai_index = cdesc->ep.dai.dai_index,
		.direction = cdesc->direction,
		.sampling_frequency = dai_rate,
		.dma_buffer_size = 1024,
		.format = cdesc->ep.dai.format,
	};
	struct comp_dev *dev = drv->ops.create(drv, cfg, &dai_cfg);

	if (!dev) {
		LOG_ERR("Failed to create DAI component %u ('%s')", cdesc->id, cdesc->name);
		return NULL;
	}

	/* Synthesize IPC4 copier and gateway configuration expected by IPC4 DAI driver */
	struct ipc4_copier_module_cfg copier_cfg;
	memset(&copier_cfg, 0, sizeof(copier_cfg));
	sof_static_init_base_cfg(&copier_cfg.base, cdesc, pdesc ? pdesc->period : 1000);
	copier_cfg.out_fmt = copier_cfg.base.audio_fmt;

	enum ipc4_connector_node_id_type node_type;
	switch (cdesc->ep.dai.dai_type) {
	case SOF_DAI_INTEL_SSP:
		node_type = (cdesc->direction == SOF_IPC_STREAM_PLAYBACK) ?
			ipc4_i2s_link_output_class : ipc4_i2s_link_input_class;
		break;
	case SOF_DAI_INTEL_DMIC:
		node_type = ipc4_dmic_link_input_class;
		break;
	case SOF_DAI_INTEL_HDA:
		node_type = (cdesc->direction == SOF_IPC_STREAM_PLAYBACK) ?
			ipc4_hda_link_output_class : ipc4_hda_link_input_class;
		break;
	case SOF_DAI_INTEL_ALH:
		node_type = (cdesc->direction == SOF_IPC_STREAM_PLAYBACK) ?
			ipc4_alh_link_output_class : ipc4_alh_link_input_class;
		break;
	default:
		node_type = (cdesc->direction == SOF_IPC_STREAM_PLAYBACK) ?
			ipc4_i2s_link_output_class : ipc4_i2s_link_input_class;
		break;
	}

	copier_cfg.gtw_cfg.node_id.f.dma_type = node_type;
	copier_cfg.gtw_cfg.node_id.f.v_index = cdesc->ep.dai.dai_index;
	copier_cfg.gtw_cfg.dma_buffer_size = 1024;
	copier_cfg.gtw_cfg.config_length = 0;

	/* Invoke optional platform/endpoint-specific configuration callback */
	if (cdesc->ep.dai.configure) {
		int ret_cfg = cdesc->ep.dai.configure(dev, &dai_cfg, &copier_cfg);
		if (ret_cfg < 0) {
			LOG_ERR("DAI endpoint configure callback failed %d for comp %u",
				ret_cfg, cdesc->id);
			return NULL;
		}
	}

	/* Apply DAI configuration to driver data via the component driver framework */
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dd) {
		int ret_dai = comp_dai_config(dd, dev, &dai_cfg, &copier_cfg);
		if (ret_dai < 0) {
			LOG_ERR("comp_dai_config failed %d for comp %u", ret_dai, cdesc->id);
			return NULL;
		}
	}

	return dev;
}

/**
 * \brief Instantiate a host/USB endpoint component.
 *
 * Populates struct ipc_config_host expected by the host component driver
 * (comp_host::create / host_new) rather than using module adapter fallback.
 *
 * \param[in] cdesc Component descriptor.
 * \param[in] drv Component driver pointer.
 * \param[in] cfg Standard component IPC configuration.
 * \return Pointer to created comp_dev, or NULL on failure.
 */
static struct comp_dev *sof_static_init_host_comp(const struct sof_static_comp *cdesc,
						  const struct comp_driver *drv,
						  const struct comp_ipc_config *cfg)
{
	/* Validate component and driver descriptor pointers */
	if (!cdesc || !drv || !cfg) {
		LOG_ERR("sof_static_init_host_comp: invalid NULL arguments");
		return NULL;
	}

	/* Populate host driver configuration */
	struct ipc_config_host host_cfg = {
		.direction = cdesc->direction,
		.no_irq = 0,
		.dmac_config = 0,
		.dma_buffer_size = 1024,
		.feature_mask = 0,
	};

	/* Invoke host driver create callback */
	struct comp_dev *dev = drv->ops.create(drv, cfg, &host_cfg);
	if (!dev) {
		LOG_ERR("Failed to create host component %u ('%s')", cdesc->id, cdesc->name);
		return NULL;
	}

	return dev;
}

/**
 * \brief Instantiate an audio processing module or host endpoint component.
 *
 * \param[in] cdesc Component descriptor.
 * \param[in] drv Component driver pointer.
 * \param[in] cfg Standard component IPC configuration.
 * \param[in] topo Topology descriptor pointer.
 * \return Pointer to created comp_dev, or NULL on failure.
 */
static struct comp_dev *sof_static_init_module_comp(const struct sof_static_comp *cdesc,
						    const struct comp_driver *drv,
						    struct comp_ipc_config *cfg,
						    const struct sof_static_topology *topo)
{
	const struct sof_static_pipeline_desc *pdesc = NULL;

	for (size_t p = 0; p < topo->num_pipelines; p++) {
		if (topo->pipelines[p].pipeline_id == cdesc->pipeline_id) {
			pdesc = &topo->pipelines[p];
			break;
		}
	}
	uint32_t period_us = (pdesc && pdesc->period) ? pdesc->period : 1000;

	/* Delegate instantiation to registered static ops or generic fallback */
	const struct sof_static_module_ops *ops = sof_static_find_module_ops(cdesc->uuid);

	if (ops && ops->create)
		return ops->create(drv, cfg, cdesc, period_us);

	return sof_static_module_create_default(drv, cfg, cdesc, period_us);
}

/**
 * \brief Stage 2: Instantiate components.
 *
 * Create all declared audio processing modules, DAI endpoints, and host/USB
 * interfaces. Look up drivers in the global SOF registry and initialize them.
 *
 * \param[in] topo Topology descriptor pointer.
 * \return 0 on success, negative errno on failure.
 */
static int sof_static_topology_init_comps(const struct sof_static_topology *topo)
{
	for (size_t i = 0; i < topo->num_comps && i < MAX_STATIC_COMPS; i++) {
		const struct sof_static_comp *cdesc = &topo->comps[i];
		uint32_t drv_type = (cdesc->type == SOF_STATIC_COMP_HOST) ? SOF_COMP_HOST :
				    (cdesc->type == SOF_STATIC_COMP_DAI) ? SOF_COMP_DAI :
				    SOF_COMP_MODULE_ADAPTER;

		/* Look up driver from component driver registry using standard API */
		const struct comp_driver *drv = comp_driver_find(cdesc->uuid, drv_type);
		if (!drv) {
			LOG_ERR("Component driver not found for comp %u ('%s')", cdesc->id, cdesc->name);
			return -ENODEV;
		}

		/* Find owning pipeline descriptor to resolve core affinity and scheduling */
		const struct sof_static_pipeline_desc *pdesc =
			sof_static_find_pipeline_desc(topo, cdesc->pipeline_id);
		uint32_t comp_core = pdesc ? pdesc->core : 0;

		/* Populate standard SOF component IPC configuration */
		struct comp_ipc_config cfg = {
			.id = cdesc->id,
			.pipeline_id = cdesc->pipeline_id,
			.core = comp_core,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = cdesc->caps.default_fmt,
			.type = drv_type,
		};

		struct comp_dev *dev;

		if (cdesc->type == SOF_STATIC_COMP_DAI)
			dev = sof_static_init_dai_comp(cdesc, drv, &cfg, topo);
		else if (cdesc->type == SOF_STATIC_COMP_HOST)
			dev = sof_static_init_host_comp(cdesc, drv, &cfg);
		else
			dev = sof_static_init_module_comp(cdesc, drv, &cfg, topo);

		if (!dev) {
			LOG_ERR("Failed to create component %u ('%s')", cdesc->id, cdesc->name);
			return -ENOMEM;
		}

		/* Set component stream direction and owning pipeline */
		dev->direction = cdesc->direction;
		dev->pipeline = sof_static_pipeline_get(cdesc->pipeline_id);

		/* Assign scheduling period from owning pipeline descriptor */
		dev->period = (pdesc && pdesc->period) ? pdesc->period : 1000;

		/* Derive component frames per scheduling period */
		uint32_t comp_rate = cdesc->caps.default_rate ? cdesc->caps.default_rate :
				     ((pdesc && pdesc->default_rate) ? pdesc->default_rate :
				      48000);
		dev->frames = (comp_rate * dev->period) / 1000000;
		if (dev->frames == 0)
			dev->frames = 48;

		/* Send initial configuration blob if specified (EQ, DRC, etc.) */
		if (cdesc->init_blob && cdesc->init_blob_size > 0) {
			int ret_blob = send_comp_config(dev, cdesc->init_blob, cdesc->init_blob_size);
			if (ret_blob < 0) {
				LOG_ERR("Failed to send init blob for component %u: %d", cdesc->id, ret_blob);
				return ret_blob;
			}
		}

		/* Record component instance in static tracking array */
		s_comps[s_num_comps] = dev;
		s_comp_ids[s_num_comps] = cdesc->id;
		s_num_comps++;

		LOG_INF("Component %u ('%s') created in pipeline %u", cdesc->id, cdesc->name, cdesc->pipeline_id);
	}

	return 0;
}

/**
 * \brief Stage 3: Allocate audio buffers.
 *
 * Allocate intermediate circular buffers linking components. Each buffer inherits
 * its rate, channels, format, and stream direction from its connected producer component.
 *
 * \param[in] topo Topology descriptor pointer.
 * \return 0 on success, negative errno on failure.
 */
static int sof_static_topology_init_buffers(const struct sof_static_topology *topo)
{
	for (size_t i = 0; i < topo->num_buffers && i < MAX_STATIC_BUFFERS; i++) {
		const struct sof_static_buffer *bdesc = &topo->buffers[i];
		struct comp_buffer *buf = buffer_alloc(NULL, bdesc->size, bdesc->flags,
						       PLATFORM_DCACHE_ALIGN, false);
		if (!buf) {
			LOG_ERR("Failed to allocate buffer %u (size %zu)", bdesc->id, bdesc->size);
			return -ENOMEM;
		}

		/* Resolve buffer parameters: rate, channels, format, and stream direction */
		uint32_t rate = bdesc->rate;
		uint16_t channels = bdesc->channels;
		enum sof_ipc_frame fmt = bdesc->fmt;
		uint32_t dir = SOF_IPC_STREAM_PLAYBACK;

		/* Look up the producer component connected to this buffer via route table */
		const struct sof_static_comp *prod_comp = NULL;
		for (size_t r = 0; r < topo->num_routes; r++) {
			if (topo->routes[r].buffer_id == bdesc->id) {
				uint32_t src_id = topo->routes[r].src_comp_id;
				for (size_t c = 0; c < topo->num_comps; c++) {
					if (topo->comps[c].id == src_id) {
						prod_comp = &topo->comps[c];
						break;
					}
				}
				break;
			}
		}

		/* Inherit parameters from producer component */
		if (prod_comp) {
			dir = prod_comp->direction;
			if (fmt == SOF_IPC_FRAME_INVALID)
				fmt = prod_comp->caps.default_fmt;
			if (!rate) {
				/* If producer is a rate converter (SRC / ASRC), inherit sink_rate */
				if (prod_comp->caps.sink_rate)
					rate = prod_comp->caps.sink_rate;
				else
					rate = prod_comp->caps.default_rate;
			}
			if (!channels)
				channels = prod_comp->caps.max_channels;
		}

		/* If rate or channels still unassigned, inherit from producer's owning pipeline */
		if (!rate && prod_comp) {
			const struct sof_static_pipeline_desc *pdesc =
				sof_static_find_pipeline_desc(topo, prod_comp->pipeline_id);
			if (pdesc && pdesc->default_rate)
				rate = pdesc->default_rate;
		}

		/* Default fallbacks if unassigned */
		if (!rate)
			rate = 48000;
		if (!channels)
			channels = 2;
		if (fmt == SOF_IPC_FRAME_INVALID)
			fmt = SOF_IPC_FRAME_S16_LE;

		/* Initialize circular buffer stream parameters and reset pointers */
		init_buffer_params(buf, dir, fmt, rate, channels);

		/* Record buffer instance in static tracking array */
		s_buffers[s_num_buffers] = buf;
		s_buffer_ids[s_num_buffers] = bdesc->id;
		s_num_buffers++;
	}

	return 0;
}

/**
 * \brief Stage 4: Connect pipeline graph routes.
 *
 * Establish graph connections: producer comp -> intermediate buffer -> consumer comp.
 *
 * \param[in] topo Topology descriptor pointer.
 * \return 0 on success, negative errno on failure.
 */
static int sof_static_topology_init_routes(const struct sof_static_topology *topo)
{
	for (size_t i = 0; i < topo->num_routes; i++) {
		const struct sof_static_route *r = &topo->routes[i];
		struct comp_dev *src = sof_static_comp_get(r->src_comp_id);
		struct comp_buffer *buf = sof_static_buffer_get(r->buffer_id);
		struct comp_dev *sink = sof_static_comp_get(r->sink_comp_id);

		/* Verify all route endpoints exist */
		if (!src || !buf || !sink) {
			LOG_ERR("Failed route %u -> [buf %u] -> %u", r->src_comp_id, r->buffer_id, r->sink_comp_id);
			return -EINVAL;
		}

		/* Connect producer component to buffer and buffer to consumer component */
		int ret = pipeline_connect(src, buf, PPL_CONN_DIR_COMP_TO_BUFFER);
		if (ret < 0) {
			LOG_ERR("Failed pipeline_connect src %u -> buf %u: %d",
				r->src_comp_id, r->buffer_id, ret);
			return ret;
		}

		ret = pipeline_connect(sink, buf, PPL_CONN_DIR_BUFFER_TO_COMP);
		if (ret < 0) {
			LOG_ERR("Failed pipeline_connect buf %u -> sink %u: %d",
				r->buffer_id, r->sink_comp_id, ret);
			return ret;
		}

		LOG_DBG("Connected: %u -> [buf %u] -> %u", r->src_comp_id, r->buffer_id, r->sink_comp_id);
	}

	return 0;
}

/**
 * \brief Stage 5: Set pipeline endpoints and propagate parameters.
 *
 * Bind source, sink, and scheduling components to each pipeline.
 * Propagate pipeline_params() and pipeline_prepare() to initiate graph-wide
 * parameter negotiation across constituent components and buffers.
 *
 * \param[in] topo Topology descriptor pointer.
 * \return 0 on success, negative errno on failure.
 */
static int sof_static_topology_init_pipeline_endpoints(const struct sof_static_topology *topo)
{
	for (size_t i = 0; i < topo->num_pipelines; i++) {
		const struct sof_static_pipeline_desc *pdesc = &topo->pipelines[i];
		struct pipeline *pipe = sof_static_pipeline_get(pdesc->pipeline_id);
		if (!pipe) {
			LOG_ERR("Pipeline %u not found", pdesc->pipeline_id);
			return -EINVAL;
		}

		/* Bind primary source, sink, and scheduling components */
		pipe->source_comp = sof_static_comp_get(pdesc->source_comp_id);
		pipe->sink_comp = sof_static_comp_get(pdesc->sink_comp_id);
		pipe->sched_comp = sof_static_comp_get(pdesc->sched_comp_id);

		if (!pipe->source_comp || !pipe->sink_comp) {
			LOG_ERR("Pipeline %u missing primary source (ID %u) or sink (ID %u)",
				pdesc->pipeline_id, pdesc->source_comp_id, pdesc->sink_comp_id);
			return -EINVAL;
		}

		/* Resolve the component driving pipeline parameters */
		struct comp_dev *host_or_sched = pipe->sched_comp ? pipe->sched_comp : pipe->source_comp;
		if (host_or_sched) {
			const struct sof_static_comp *cdesc = NULL;
			for (size_t c = 0; c < topo->num_comps; c++) {
				if (topo->comps[c].id == dev_comp_id(host_or_sched)) {
					cdesc = &topo->comps[c];
					break;
				}
			}

			/* Determine stream sampling rate for this pipeline */
			uint32_t rate = pdesc->default_rate;
			if (!rate && cdesc)
				rate = cdesc->caps.default_rate;
			if (!rate)
				rate = 48000;

			/* Determine stream channel count */
			uint32_t channels = pdesc->default_channels;
			if (!channels && cdesc)
				channels = cdesc->caps.max_channels;
			if (!channels)
				channels = 2;

			/* Determine stream PCM frame format */
			enum sof_ipc_frame fmt = (cdesc && cdesc->caps.default_fmt) ?
						 cdesc->caps.default_fmt : host_or_sched->ipc_config.frame_fmt;
			if (!fmt)
				fmt = SOF_IPC_FRAME_S16_LE;

			/* Calculate container bytes, valid bytes, and period frame size */
			uint32_t cont_bytes = get_sample_bytes(fmt);
			uint32_t valid_bytes = static_get_sample_valid_bytes(fmt);
			uint32_t period_us = pdesc->period ? pdesc->period : 1000;
			uint32_t frames = pdesc->frames_per_sched ? pdesc->frames_per_sched :
					  ((rate * period_us) / 1000000);
			if (frames == 0)
				frames = 48;

			/* Populate PCM params structure for the pipeline */
			struct sof_ipc_pcm_params prms;
			memset(&prms, 0, sizeof(prms));
			prms.params.rate = rate;
			prms.params.channels = channels;
			prms.params.frame_fmt = fmt;
			prms.params.sample_container_bytes = cont_bytes;
			prms.params.sample_valid_bytes = valid_bytes;
			prms.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
			prms.params.host_period_bytes = frames * channels * cont_bytes;
			prms.comp_id = dev_comp_id(host_or_sched);
			prms.params.direction = pdesc->direction;
			for (uint32_t c = 0; c < SOF_IPC_MAX_CHANNELS; c++) {
				if (c == 0)
					prms.params.chmap[c] = (channels == 1) ? SOF_CHMAP_MONO : SOF_CHMAP_FL;
				else if (c == 1 && channels > 1)
					prms.params.chmap[c] = SOF_CHMAP_FR;
				else if (c < channels)
					prms.params.chmap[c] = c + 1;
				else
					prms.params.chmap[c] = SOF_CHMAP_NA;
			}

			/* Propagate params and prepare throughout the pipeline walk */
			int ret_prms = pipeline_params(pipe, host_or_sched, &prms);
			if (ret_prms < 0) {
				LOG_ERR("Pipeline %u params failed: %d", pdesc->pipeline_id, ret_prms);
				return ret_prms;
			}
			int ret_prep = pipeline_prepare(pipe, host_or_sched);
			if (ret_prep < 0) {
				LOG_ERR("Pipeline %u prepare failed: %d", pdesc->pipeline_id, ret_prep);
				return ret_prep;
			}
			LOG_INF("Pipeline %u params (rate %u, ch %u, fmt %u) OK, prepare OK",
				pdesc->pipeline_id, rate, channels, fmt);
		}
	}

	return 0;
}

/**
 * \brief Stage 6: Explicitly configure and prepare all components.
 *
 * Ensure every instantiated component transitions to COMP_STATE_PREPARE,
 * even if not reached by the initial pipeline walk. This guarantees processing
 * modules (e.g. Volume, EQ, DRC, Selector) are fully primed and ready for streaming.
 *
 * \param[in] topo Topology descriptor pointer.
 * \return 0 on success, negative errno on failure.
 */
static int sof_static_topology_prepare_comps(const struct sof_static_topology *topo)
{
	for (size_t i = 0; i < topo->num_comps; i++) {
		struct comp_dev *dev = sof_static_comp_get(topo->comps[i].id);
		if (!dev)
			continue;

		/* Only prepare components currently in READY state */
		if (dev->state == COMP_STATE_READY) {
			const struct sof_static_comp *cdesc = &topo->comps[i];

			/* Skip unrouted components that have no buffers connected */
			if (list_is_empty(&dev->bsource_list) && list_is_empty(&dev->bsink_list)) {
				LOG_DBG("Skipping unrouted component %u ('%s')", cdesc->id, cdesc->name);
				continue;
			}
			if (cdesc->type != SOF_STATIC_COMP_HOST && cdesc->type != SOF_STATIC_COMP_DAI) {
				if (list_is_empty(&dev->bsource_list) || list_is_empty(&dev->bsink_list)) {
					LOG_DBG("Skipping unrouted module %u ('%s')", cdesc->id, cdesc->name);
					continue;
				}
			}

			const struct sof_static_pipeline_desc *pdesc =
				sof_static_find_pipeline_desc(topo, cdesc->pipeline_id);

			/* Determine component-specific sample rate */
			uint32_t rate = cdesc->caps.default_rate;
			if (!rate && pdesc)
				rate = pdesc->default_rate;
			if (!rate)
				rate = 48000;

			/* Determine channel count */
			uint32_t channels = cdesc->caps.max_channels;
			if (!channels && pdesc)
				channels = pdesc->default_channels;
			if (!channels)
				channels = 2;

			/* Determine frame format */
			enum sof_ipc_frame fmt = cdesc->caps.default_fmt ? cdesc->caps.default_fmt :
						 dev->ipc_config.frame_fmt;
			if (!fmt)
				fmt = SOF_IPC_FRAME_S16_LE;

			/* Calculate container size, valid bits, and frames */
			uint32_t cont_bytes = get_sample_bytes(fmt);
			uint32_t valid_bytes = static_get_sample_valid_bytes(fmt);
			uint32_t period_us = (pdesc && pdesc->period) ? pdesc->period : 1000;
			uint32_t frames = (pdesc && pdesc->frames_per_sched) ? pdesc->frames_per_sched :
					  ((rate * period_us) / 1000000);
			if (frames == 0)
				frames = 48;

			/* Populate component PCM parameters */
			struct sof_ipc_pcm_params prms;
			memset(&prms, 0, sizeof(prms));
			prms.params.rate = rate;
			prms.params.channels = channels;
			prms.params.frame_fmt = fmt;
			prms.params.sample_container_bytes = cont_bytes;
			prms.params.sample_valid_bytes = valid_bytes;
			prms.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
			prms.params.host_period_bytes = frames * channels * cont_bytes;
			prms.comp_id = dev_comp_id(dev);
			prms.params.direction = dev->direction;
			for (uint32_t c = 0; c < SOF_IPC_MAX_CHANNELS; c++) {
				if (c == 0)
					prms.params.chmap[c] = (channels == 1) ? SOF_CHMAP_MONO : SOF_CHMAP_FL;
				else if (c == 1 && channels > 1)
					prms.params.chmap[c] = SOF_CHMAP_FR;
				else if (c < channels)
					prms.params.chmap[c] = c + 1;
				else
					prms.params.chmap[c] = SOF_CHMAP_NA;
			}

			/* Call component params and prepare callbacks */
			int ret_prms = comp_params(dev, &prms.params);
			if (ret_prms < 0) {
				LOG_ERR("Component %u ('%s') params failed: %d",
					dev_comp_id(dev), cdesc->name, ret_prms);
				return ret_prms;
			}
			int ret_prep = comp_prepare(dev);
			if (ret_prep == -ENOTCONN) {
				LOG_DBG("Component %u ('%s') not connected, skipping prepare",
					dev_comp_id(dev), cdesc->name);
				continue;
			}
			if (ret_prep < 0) {
				LOG_ERR("Component %u ('%s') prepare failed: %d",
					dev_comp_id(dev), cdesc->name, ret_prep);
				return ret_prep;
			}
			LOG_INF("Component %u ('%s') explicit prepare OK (state %d)",
				dev_comp_id(dev), cdesc->name, dev->state);
		}
	}

	return 0;
}

/**
 * \brief Stage 7: Initialize kcontrols.
 *
 * Set up all declared volume, mute, enum, and binary kcontrols with their
 * default boot values so processing modules start in their expected state.
 *
 * \param[in] topo Topology descriptor pointer.
 * \return 0 on success, negative errno on failure.
 */
static int sof_static_topology_init_controls(const struct sof_static_topology *topo)
{
	for (size_t i = 0; i < topo->num_controls && i < MAX_STATIC_CONTROLS; i++) {
		const struct sof_static_kcontrol *ctl = &topo->controls[i];
		s_control_vals[i] = ctl->def;
		int ret = sof_static_kcontrol_set(ctl->id, ctl->def);
		if (ret < 0) {
			LOG_ERR("Failed to set default value for control [%u] '%s': %d",
				ctl->id, ctl->name, ret);
			return ret;
		}
		LOG_INF("Kcontrol [%u] '%s' (comp %u, type %d, def %d)",
			ctl->id, ctl->name, ctl->target_comp_id, ctl->type, ctl->def);
	}

	return 0;
}

/**
 * \brief Load and instantiate a declarative static audio topology.
 *
 * Executes the complete multi-stage construction pipeline:
 *   Stage 1: Allocate top-level pipelines
 *   Stage 2: Instantiate components (modules, DAIs, USB/host)
 *   Stage 3: Allocate intermediate audio buffers
 *   Stage 4: Connect pipeline graph routes
 *   Stage 5: Bind pipeline endpoints and propagate stream parameters
 *   Stage 6: Explicitly configure and prepare all components
 *   Stage 7: Initialize kcontrols with default boot values
 *
 * \param[in] topo Pointer to declarative static topology descriptor.
 * \return 0 on success, negative errno on failure.
 */
/**
 * \brief Free all allocated topology resources and reset tracking tables.
 *
 * Tears down allocated buffers, components, and pipelines in reverse dependency
 * order, clears tracking registries, and resets active topology state.
 */
static void sof_static_topology_unwind(void)
{
	/* Free allocated intermediate buffers */
	for (size_t i = 0; i < s_num_buffers; i++) {
		if (s_buffers[i]) {
			buffer_free(s_buffers[i]);
			s_buffers[i] = NULL;
		}
	}
	s_num_buffers = 0;

	/* Free instantiated components */
	for (size_t i = 0; i < s_num_comps; i++) {
		if (s_comps[i]) {
			comp_free(s_comps[i]);
			s_comps[i] = NULL;
		}
	}
	s_num_comps = 0;

	/* Free allocated pipelines */
	for (size_t i = 0; i < s_num_pipelines; i++) {
		if (s_pipelines[i]) {
			pipeline_free(s_pipelines[i]);
			s_pipelines[i] = NULL;
		}
	}
	s_num_pipelines = 0;

	/* Reset kcontrol cache and unpublish active topology pointer */
	memset(s_control_vals, 0, sizeof(s_control_vals));
	s_active_topo = NULL;
}

int sof_static_topology_init(const struct sof_static_topology *topo)
{
	int ret;

	/* Validate input topology pointer */
	if (!topo) {
		LOG_ERR("Invalid topology descriptor");
		return -EINVAL;
	}

	/* Validate topology descriptor counts against compiled capacity limits */
	if (topo->num_pipelines > MAX_STATIC_PIPELINES ||
	    topo->num_comps > MAX_STATIC_COMPS ||
	    topo->num_buffers > MAX_STATIC_BUFFERS ||
	    topo->num_controls > MAX_STATIC_CONTROLS ||
	    topo->num_routes > MAX_STATIC_ROUTES) {
		LOG_ERR("Topology descriptor counts exceed compiled limits (pipes %zu/%d, comps %zu/%d, bufs %zu/%d, ctls %zu/%d, routes %zu/%d)",
			topo->num_pipelines, MAX_STATIC_PIPELINES,
			topo->num_comps, MAX_STATIC_COMPS,
			topo->num_buffers, MAX_STATIC_BUFFERS,
			topo->num_controls, MAX_STATIC_CONTROLS,
			topo->num_routes, MAX_STATIC_ROUTES);
		return -EINVAL;
	}

	LOG_INF("=== Loading Static Audio Topology: '%s' ===", topo->name ? topo->name : "Unnamed");

	/* Clean up any previously allocated state before constructing new topology */
	sof_static_topology_unwind();

	/* Stage 1: Allocate top-level pipelines */
	ret = sof_static_topology_init_pipelines(topo);
	if (ret < 0) {
		LOG_ERR("sof_static_topology_init: stage 1 (init_pipelines) failed: %d", ret);
		goto err_unwind;
	}

	/* Stage 2: Instantiate components */
	ret = sof_static_topology_init_comps(topo);
	if (ret < 0) {
		LOG_ERR("sof_static_topology_init: stage 2 (init_comps) failed: %d", ret);
		goto err_unwind;
	}

	/* Stage 3: Allocate intermediate audio buffers */
	ret = sof_static_topology_init_buffers(topo);
	if (ret < 0) {
		LOG_ERR("sof_static_topology_init: stage 3 (init_buffers) failed: %d", ret);
		goto err_unwind;
	}

	/* Stage 4: Connect pipeline graph routes */
	ret = sof_static_topology_init_routes(topo);
	if (ret < 0) {
		LOG_ERR("sof_static_topology_init: stage 4 (init_routes) failed: %d", ret);
		goto err_unwind;
	}

	/* Stage 5: Bind pipeline endpoints and propagate stream parameters */
	ret = sof_static_topology_init_pipeline_endpoints(topo);
	if (ret < 0) {
		LOG_ERR("sof_static_topology_init: stage 5 (init_pipeline_endpoints) failed: %d", ret);
		goto err_unwind;
	}

	/* Stage 6: Explicitly configure and prepare all components */
	ret = sof_static_topology_prepare_comps(topo);
	if (ret < 0) {
		LOG_ERR("sof_static_topology_init: stage 6 (prepare_comps) failed: %d", ret);
		goto err_unwind;
	}

	/* Publish topology as active before initializing kcontrols */
	s_active_topo = topo;

	/* Stage 7: Initialize kcontrols with default values */
	ret = sof_static_topology_init_controls(topo);
	if (ret < 0) {
		LOG_ERR("sof_static_topology_init: stage 7 (init_controls) failed: %d", ret);
		goto err_unwind;
	}

	LOG_INF("Static audio topology initialized successfully (%zu pipelines, %zu comps, %zu buffers, %zu controls)",
		topo->num_pipelines, topo->num_comps, topo->num_buffers, topo->num_controls);
	return 0;

err_unwind:
	s_active_topo = NULL;
	sof_static_topology_unwind();
	return ret;
}

/**
 * \brief Retrieve the currently active static topology descriptor.
 * \return Pointer to active struct sof_static_topology, or NULL if uninitialized.
 */
const struct sof_static_topology *sof_static_topology_get(void)
{
	return s_active_topo;
}

/**
 * \brief Retrieve a pipeline instance pointer by its unique pipeline ID.
 * \param[in] pipeline_id Unique pipeline ID to search for.
 * \return Pointer to struct pipeline, or NULL if not found.
 */
struct pipeline *sof_static_pipeline_get(uint32_t pipeline_id)
{
	/* Linear scan of statically tracked pipeline IDs */
	for (size_t i = 0; i < s_num_pipelines; i++) {
		if (s_pipeline_ids[i] == pipeline_id)
			return s_pipelines[i];
	}

	LOG_ERR("sof_static_pipeline_get: pipeline %u not found", pipeline_id);
	return NULL;
}

/**
 * \brief Retrieve a component device pointer by its unique component ID.
 * \param[in] comp_id Unique component ID to search for.
 * \return Pointer to struct comp_dev, or NULL if not found.
 */
struct comp_dev *sof_static_comp_get(uint32_t comp_id)
{
	/* Linear scan of statically tracked component IDs */
	for (size_t i = 0; i < s_num_comps; i++) {
		if (s_comp_ids[i] == comp_id)
			return s_comps[i];
	}

	LOG_ERR("sof_static_comp_get: comp %u not found", comp_id);
	return NULL;
}

/**
 * \brief Retrieve an intermediate audio buffer pointer by its unique buffer ID.
 * \param[in] buffer_id Unique buffer ID to search for.
 * \return Pointer to struct comp_buffer, or NULL if not found.
 */
struct comp_buffer *sof_static_buffer_get(uint32_t buffer_id)
{
	/* Linear scan of statically tracked buffer IDs */
	for (size_t i = 0; i < s_num_buffers; i++) {
		if (s_buffer_ids[i] == buffer_id)
			return s_buffers[i];
	}

	LOG_ERR("sof_static_buffer_get: buffer %u not found", buffer_id);
	return NULL;
}

/**
 * \brief Set kcontrol value by control ID.
 *
 * Dispatches control updates to the custom callback handler if registered,
 * or routes volume, switch, or enum controls to target components via their
 * registered static module operations (struct sof_static_module_ops).
 *
 * \param[in] ctrl_id Unique control identifier.
 * \param[in] val Value to apply.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_set(uint32_t ctrl_id, int32_t val)
{
	if (!s_active_topo)
		return -ENODEV;

	/* Look up control descriptor by unique ID */
	const struct sof_static_kcontrol *ctl = NULL;
	size_t ctl_idx = 0;
	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		if (s_active_topo->controls[i].id == ctrl_id) {
			ctl = &s_active_topo->controls[i];
			ctl_idx = i;
			break;
		}
	}
	if (!ctl) {
		LOG_ERR("sof_static_kcontrol_set: control ID %u not found", ctrl_id);
		return -ENOENT;
	}

	/*
	 * Custom kcontrol callback handler:
	 * Allows board- or platform-specific controls (such as hardware clock
	 * switching, route multiplexers, or DMIC injection) to intercept control events.
	 */
	if (s_active_topo->custom_control_handler) {
		int ret = s_active_topo->custom_control_handler(ctl, val, s_active_topo->custom_control_data);
		if (ret >= 0 || ctl->target_comp_id == 0) {
			s_control_vals[ctl_idx] = val;
			LOG_INF("Kcontrol [%u] '%s' set to %d via custom handler", ctl->id, ctl->name, val);
			return ret;
		}
	}

	/* Virtual / platform controls with no target component */
	if (ctl->target_comp_id == 0) {
		s_control_vals[ctl_idx] = val;
		LOG_INF("Kcontrol [%u] '%s' stored val=%d", ctl->id, ctl->name, val);
		return 0;
	}

	/* Look up target component device */
	struct comp_dev *dev = sof_static_comp_get(ctl->target_comp_id);
	if (!dev)
		return -ENODEV;

	/* Binary controls cannot be updated with a scalar int32_t value */
	if (ctl->type == SOF_STATIC_CTRL_BINARY) {
		LOG_ERR("sof_static_kcontrol_set: ctl [%u] '%s' is binary, use sof_static_kcontrol_set_binary",
			ctl->id, ctl->name);
		return -EINVAL;
	}

	/* Look up component's registered static module operations */
	const struct sof_static_module_ops *ops = dev->drv ? sof_static_find_module_ops(dev->drv->uid) : NULL;
	if (!ops) {
		LOG_ERR("sof_static_kcontrol_set: no static ops for comp %u (ctl [%u] '%s')",
			ctl->target_comp_id, ctl->id, ctl->name);
		return -ENOTSUP;
	}

	int ret_op = -ENOTSUP;

	/* Dispatch control value to target component using registered operations */
	switch (ctl->type) {
	case SOF_STATIC_CTRL_VOLUME:
		/* Linear volume fader control */
		if (ops->apply_volume)
			ret_op = ops->apply_volume(dev, ctl->channels, val);
		break;

	case SOF_STATIC_CTRL_SWITCH:
		/* Boolean mute or module bypass switch */
		if (ops->apply_switch)
			ret_op = ops->apply_switch(dev, ctl->channels, val);
		break;

	case SOF_STATIC_CTRL_ENUM:
		/* Enumerated route or channel selector */
		if (ops->apply_enum)
			ret_op = ops->apply_enum(dev, 0, val);
		break;

	default:
		LOG_ERR("sof_static_kcontrol_set: invalid control type %d for ctl %u",
			ctl->type, ctl->id);
		return -EINVAL;
	}

	if (ret_op < 0) {
		LOG_ERR("sof_static_kcontrol_set: op failed %d for ctl [%u] '%s'",
			ret_op, ctl->id, ctl->name);
		return ret_op;
	}

	/* Cache updated control value and log confirmation */
	s_control_vals[ctl_idx] = val;
	LOG_INF("Kcontrol [%u] '%s' set to %d", ctl->id, ctl->name, val);
	return 0;
}

/**
 * \brief Set kcontrol value for a specific audio channel.
 * \param[in] ctrl_id Unique control identifier.
 * \param[in] channel Channel index (0-based).
 * \param[in] val Value to apply.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_set_chan(uint32_t ctrl_id, uint32_t channel, int32_t val)
{
	/* Ensure an active static topology has been initialized */
	if (!s_active_topo) {
		LOG_ERR("sof_static_kcontrol_set_chan: no active topology");
		return -ENODEV;
	}

	const struct sof_static_kcontrol *ctl = NULL;
	size_t ctl_idx = 0;

	/* Find control descriptor by ID */
	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		if (s_active_topo->controls[i].id == ctrl_id) {
			ctl = &s_active_topo->controls[i];
			ctl_idx = i;
			break;
		}
	}

	if (!ctl) {
		LOG_ERR("sof_static_kcontrol_set_chan: control ID %u not found", ctrl_id);
		return -ENOENT;
	}

	/* Validate channel index against control channel count */
	if (channel >= ctl->channels) {
		LOG_ERR("sof_static_kcontrol_set_chan: channel %u out of range (max %u) for ctl %u",
			channel, ctl->channels, ctrl_id);
		return -EINVAL;
	}

	/*
	 * Custom kcontrol callback handler:
	 * Forward to platform-specific hook if registered, or acknowledge virtual controls.
	 */
	if (s_active_topo->custom_control_handler) {
		int ret = s_active_topo->custom_control_handler(ctl, val, s_active_topo->custom_control_data);
		if (ret >= 0 || ctl->target_comp_id == 0) {
			s_control_vals[ctl_idx] = val;
			LOG_INF("Kcontrol [%u] '%s' ch %u set to %d via custom handler",
				ctl->id, ctl->name, channel, val);
			return ret;
		}
	}

	/* Virtual / platform controls with no target component */
	if (ctl->target_comp_id == 0) {
		s_control_vals[ctl_idx] = val;
		LOG_INF("Kcontrol [%u] '%s' ch %u stored val=%d", ctl->id, ctl->name, channel, val);
		return 0;
	}

	/* Look up target component device */
	struct comp_dev *dev = sof_static_comp_get(ctl->target_comp_id);
	if (!dev) {
		LOG_ERR("sof_static_kcontrol_set_chan: comp %u not found for ctl %u",
			ctl->target_comp_id, ctrl_id);
		return -ENODEV;
	}

	/* Binary controls cannot be updated with a channel scalar value */
	if (ctl->type == SOF_STATIC_CTRL_BINARY) {
		LOG_ERR("sof_static_kcontrol_set_chan: ctl [%u] '%s' is binary, use sof_static_kcontrol_set_binary",
			ctl->id, ctl->name);
		return -EINVAL;
	}

	/* Look up component's registered static module operations */
	const struct sof_static_module_ops *ops = dev->drv ? sof_static_find_module_ops(dev->drv->uid) : NULL;
	if (!ops) {
		LOG_ERR("sof_static_kcontrol_set_chan: no static ops for comp %u (ctl [%u] '%s')",
			ctl->target_comp_id, ctl->id, ctl->name);
		return -ENOTSUP;
	}

	int ret_op = -ENOTSUP;

	/* Dispatch control value to target component using channel-specific ops if available */
	switch (ctl->type) {
	case SOF_STATIC_CTRL_VOLUME:
		/* Linear volume: prefer channel-aware callback when provided */
		if (ops->apply_volume_chan)
			ret_op = ops->apply_volume_chan(dev, channel, val);
		else if (ops->apply_volume)
			ret_op = ops->apply_volume(dev, ctl->channels, val);
		break;

	case SOF_STATIC_CTRL_SWITCH:
		/* Boolean switch / mute: prefer channel-aware callback when provided */
		if (ops->apply_switch_chan)
			ret_op = ops->apply_switch_chan(dev, channel, val);
		else if (ops->apply_switch)
			ret_op = ops->apply_switch(dev, ctl->channels, val);
		break;

	case SOF_STATIC_CTRL_ENUM:
		/* Channel / stream multiplexer selector */
		if (ops->apply_enum)
			ret_op = ops->apply_enum(dev, channel, val);
		break;

	default:
		LOG_ERR("sof_static_kcontrol_set_chan: invalid control type %d for ctl %u",
			ctl->type, ctl->id);
		return -EINVAL;
	}

	if (ret_op < 0) {
		LOG_ERR("sof_static_kcontrol_set_chan: op failed %d for ctl [%u] '%s'",
			ret_op, ctl->id, ctl->name);
		return ret_op;
	}

	/* Cache updated control value and log confirmation */
	s_control_vals[ctl_idx] = val;
	LOG_INF("Kcontrol [%u] '%s' ch %u set to %d", ctl->id, ctl->name, channel, val);
	return 0;
}

/**
 * \brief Set binary raw configuration blob on a static kcontrol by control ID.
 *
 * Dispatches the raw binary configuration blob to the target component associated
 * with the designated kcontrol via apply_binary or module adapter large config.
 *
 * \param[in] ctrl_id Target kcontrol identifier.
 * \param[in] data Pointer to raw configuration blob.
 * \param[in] size Size of data blob in bytes.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_set_binary(uint32_t ctrl_id, const void *data, size_t size)
{
	/* Ensure an active static topology has been initialized */
	if (!s_active_topo) {
		LOG_ERR("sof_static_kcontrol_set_binary: no active topology");
		return -ENODEV;
	}
	if (!data || size == 0) {
		LOG_ERR("sof_static_kcontrol_set_binary: invalid NULL or empty data");
		return -EINVAL;
	}

	const struct sof_static_kcontrol *ctl = NULL;
	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		if (s_active_topo->controls[i].id == ctrl_id) {
			ctl = &s_active_topo->controls[i];
			break;
		}
	}
	if (!ctl) {
		LOG_ERR("sof_static_kcontrol_set_binary: control ID %u not found", ctrl_id);
		return -ENOENT;
	}
	if (ctl->type != SOF_STATIC_CTRL_BINARY) {
		LOG_ERR("sof_static_kcontrol_set_binary: control [%u] '%s' is not binary type",
			ctl->id, ctl->name);
		return -EINVAL;
	}
	if (ctl->target_comp_id == 0) {
		LOG_INF("Kcontrol [%u] '%s' binary blob (%zu bytes) processed", ctl->id, ctl->name, size);
		return 0;
	}

	struct comp_dev *dev = sof_static_comp_get(ctl->target_comp_id);
	if (!dev) {
		LOG_ERR("sof_static_kcontrol_set_binary: target comp %u not found", ctl->target_comp_id);
		return -ENODEV;
	}

	const struct sof_static_module_ops *ops = dev->drv ? sof_static_find_module_ops(dev->drv->uid) : NULL;
	if (ops && ops->apply_binary) {
		int ret_op = ops->apply_binary(dev, data, size);
		if (ret_op < 0) {
			LOG_ERR("sof_static_kcontrol_set_binary: apply_binary failed %d for ctl [%u] '%s'",
				ret_op, ctl->id, ctl->name);
			return ret_op;
		}
		LOG_INF("Kcontrol [%u] '%s' binary set (%zu bytes)", ctl->id, ctl->name, size);
		return 0;
	}

	/* Fallback: dispatch via component driver set_large_config or send_comp_config */
	int ret;
	if (dev->drv && dev->drv->ops.set_large_config)
		ret = dev->drv->ops.set_large_config(dev, 0, true, true, size, (const char *)data);
	else
		ret = send_comp_config(dev, data, size);

	if (ret < 0) {
		LOG_ERR("sof_static_kcontrol_set_binary: binary set failed %d for ctl [%u] '%s'",
			ret, ctl->id, ctl->name);
		return ret;
	}

	LOG_INF("Kcontrol [%u] '%s' binary set (%zu bytes)", ctl->id, ctl->name, size);
	return 0;
}

/**
 * \brief Retrieve cached kcontrol value by control ID.
 * \param[in] ctrl_id Unique control identifier.
 * \param[out] val Pointer to store retrieved value.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_get(uint32_t ctrl_id, int32_t *val)
{
	if (!s_active_topo || !val)
		return -EINVAL;

	/* Search control descriptors for matching ID and return cached value */
	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		if (s_active_topo->controls[i].id == ctrl_id) {
			*val = s_control_vals[i];
			return 0;
		}
	}

	LOG_ERR("sof_static_kcontrol_get: control ID %u not found", ctrl_id);
	return -ENOENT;
}

/**
 * \brief Find kcontrol ID by its human-readable name.
 * \param[in] name Control name string to search for.
 * \return Control ID >= 0 if found, -ENOENT if not found.
 */
int sof_static_kcontrol_find_by_name(const char *name)
{
	if (!s_active_topo || !name)
		return -EINVAL;

	/* Compare name string against declared controls in active topology */
	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		if (s_active_topo->controls[i].name &&
		    strcmp(s_active_topo->controls[i].name, name) == 0) {
			return (int)s_active_topo->controls[i].id;
		}
	}

	LOG_ERR("sof_static_kcontrol_find_by_name: control '%s' not found", name);
	return -ENOENT;
}

/**
 * \brief Resolve the primary trigger component driving a pipeline.
 *
 * For playback pipelines, the trigger component is typically sched_comp or source_comp.
 * For capture pipelines, where data flows into the DSP from an external DAI or source,
 * the trigger component must be the consumer/sink endpoint to pull data through the graph.
 *
 * \param[in] pipe Target pipeline pointer.
 * \return Pointer to resolved trigger component device.
 */
static struct comp_dev *sof_static_pipeline_get_trigger_dev(struct pipeline *pipe)
{
	struct comp_dev *dev = pipe->sched_comp;

	/* For capture pipelines, ensure trigger starts from the sink component */
	if (!dev || (pipe->source_comp && pipe->source_comp->direction == SOF_IPC_STREAM_CAPTURE &&
		     dev == pipe->source_comp)) {
		if (pipe->source_comp && pipe->source_comp->direction == SOF_IPC_STREAM_CAPTURE)
			dev = pipe->sink_comp;
		else
			dev = pipe->source_comp;
	}
	return dev;
}

/**
 * \brief Start a static audio pipeline by its pipeline ID.
 *
 * Prepares the pipeline and constituent components if needed, propagates
 * start triggers, transitions state to COMP_STATE_ACTIVE, and activates
 * scheduling copy tasks.
 *
 * \param[in] pipeline_id Target pipeline ID.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_start(uint32_t pipeline_id)
{
	/* Look up target pipeline */
	struct pipeline *pipe = sof_static_pipeline_get(pipeline_id);
	if (!pipe)
		return -ENOENT;

	/* Resolve trigger component */
	struct comp_dev *dev = sof_static_pipeline_get_trigger_dev(pipe);
	if (!dev)
		return -ENODEV;

	/* If not already active, prepare and trigger components */
	if (pipe->status != COMP_STATE_ACTIVE) {
		/*
		 * Ensure pipeline is prepared before starting.
		 * If prepare fails, abort immediately: triggering a pipeline whose
		 * circular buffers and DMA channels are uninitialized causes DSP faults.
		 */
		if (pipe->status == COMP_STATE_READY ||
		    pipe->status == COMP_STATE_INIT ||
		    pipe->status == COMP_STATE_PAUSED) {
			int ret_prep = pipeline_prepare(pipe, dev);
			if (ret_prep < 0) {
				LOG_ERR("Pipeline %u prepare failed: %d", pipeline_id, ret_prep);
				return ret_prep;
			}
		}

		/* Propagate PRE_START trigger across the pipeline graph (pipeline_trigger_run
		 * automatically executes the follow-up START traversal on all graph components).
		 */
		int ret = pipeline_trigger_run(pipe, dev, COMP_TRIGGER_PRE_START);
		if (ret < 0) {
			LOG_ERR("Pipeline %u pre-start trigger failed: %d", pipeline_id, ret);
			return ret;
		}

		pipe->status = COMP_STATE_ACTIVE;
	}

	/* Schedule periodic copy task if not already running */
	if (pipe->pipe_task && !task_is_active(pipe->pipe_task))
		pipeline_schedule_copy(pipe, 0);

	LOG_INF("Pipeline %u STARTED", pipeline_id);
	return 0;
}

/**
 * \brief Stop a static audio pipeline by its pipeline ID.
 *
 * Propagates stop triggers, cancels scheduling copy tasks, and transitions
 * pipeline and component states to COMP_STATE_PAUSED.
 *
 * \param[in] pipeline_id Target pipeline ID.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_stop(uint32_t pipeline_id)
{
	/* Look up target pipeline */
	struct pipeline *pipe = sof_static_pipeline_get(pipeline_id);
	if (!pipe)
		return -ENOENT;

	/* Resolve trigger component */
	struct comp_dev *dev = sof_static_pipeline_get_trigger_dev(pipe);
	if (!dev)
		return -ENODEV;

	/* Propagate STOP trigger if pipeline is active or paused */
	if (pipe->status == COMP_STATE_ACTIVE || pipe->status == COMP_STATE_PAUSED) {
		int ret = pipeline_trigger_run(pipe, dev, COMP_TRIGGER_STOP);
		if (ret < 0) {
			LOG_ERR("Pipeline %u stop trigger failed: %d", pipeline_id, ret);
			return ret;
		}

		pipe->status = COMP_STATE_PAUSED;
	}

	/* Cancel active periodic copy task */
	if (pipe->pipe_task && task_is_active(pipe->pipe_task))
		schedule_task_cancel(pipe->pipe_task);

	LOG_INF("Pipeline %u STOPPED", pipeline_id);
	return 0;
}

/**
 * \brief Directly trigger a static pipeline start or stop by pipeline ID.
 *
 * Dispatches cleanly to sof_static_pipeline_start() or sof_static_pipeline_stop().
 *
 * \param[in] pipeline_id Target pipeline ID.
 * \param[in] start True to start pipeline, false to stop.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_trigger(uint32_t pipeline_id, bool start)
{
	if (start)
		return sof_static_pipeline_start(pipeline_id);

	return sof_static_pipeline_stop(pipeline_id);
}

/**
 * \brief Re-negotiate the stream sample rate for a specific static pipeline.
 *
 * Re-negotiates the sample rate for the designated pipeline and its constituent
 * components and buffers while the pipeline is in paused or stopped state.
 * Validates that all constituent components support the requested rate according
 * to their capability masks, reconfigures buffer period sizes, and propagates
 * pipeline_params() and pipeline_prepare() through the pipeline graph.
 *
 * \param[in] pipeline_id Target pipeline identifier.
 * \param[in] rate Target sample rate in Hz (e.g. 16000, 44100, 48000, 96000).
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_set_sample_rate(uint32_t pipeline_id, uint32_t rate)
{
	/* Verify target pipeline exists */
	struct pipeline *pipe = sof_static_pipeline_get(pipeline_id);
	if (!pipe) {
		LOG_ERR("sof_static_pipeline_set_sample_rate: pipeline %u not found", pipeline_id);
		return -ENOENT;
	}

	/* Dynamic rate reconfiguration requires the pipeline to be stopped/paused */
	if (pipe->status == COMP_STATE_ACTIVE) {
		LOG_ERR("sof_static_pipeline_set_sample_rate: pipeline %u is active, cannot change rate",
			pipeline_id);
		return -EBUSY;
	}

	/* Validate requested sample rate against component capability masks */
	uint32_t rmask = sof_static_rate_to_mask(rate);
	if (s_active_topo) {
		for (size_t c = 0; c < s_active_topo->num_comps; c++) {
			const struct sof_static_comp *cdesc = &s_active_topo->comps[c];
			if (cdesc->pipeline_id == pipeline_id && cdesc->caps.rates) {
				if (!(cdesc->caps.rates & rmask)) {
					LOG_ERR("sof_static_pipeline_set_sample_rate: rate %u Hz unsupported by comp %u ('%s')",
						rate, cdesc->id, cdesc->name);
					return -EINVAL;
				}
			}
		}
	}

	/* Record updated sample rate */
	for (size_t i = 0; i < s_num_pipelines; i++) {
		if (s_pipeline_ids[i] == pipeline_id) {
			s_pipeline_rates[i] = rate;
			break;
		}
	}

	/* Update buffer parameters for all buffers whose producer belongs to this pipeline */
	if (s_active_topo) {
		for (size_t r = 0; r < s_active_topo->num_routes; r++) {
			const struct sof_static_route *route = &s_active_topo->routes[r];
			const struct sof_static_comp *src_cdesc =
				sof_static_find_comp_desc(s_active_topo, route->src_comp_id);

			if (src_cdesc && src_cdesc->pipeline_id == pipeline_id) {
				struct comp_buffer *buf = sof_static_buffer_get(route->buffer_id);

				if (buf) {
					init_buffer_params(buf, src_cdesc->direction,
							   audio_stream_get_frm_fmt(&buf->stream),
							   rate,
							   audio_stream_get_channels(&buf->stream));
				}
			}
		}
	}

	/* Propagate updated stream parameters through pipeline graph */
	struct comp_dev *host_or_sched = pipe->sched_comp ? pipe->sched_comp : pipe->source_comp;
	if (host_or_sched) {
		const struct sof_static_pipeline_desc *pdesc = NULL;
		if (s_active_topo)
			pdesc = sof_static_find_pipeline_desc(s_active_topo, pipeline_id);

		uint32_t channels = 2;
		enum sof_ipc_frame fmt = host_or_sched->ipc_config.frame_fmt;
		if (!fmt)
			fmt = SOF_IPC_FRAME_S16_LE;

		uint32_t cont_bytes = get_sample_bytes(fmt);
		uint32_t valid_bytes = static_get_sample_valid_bytes(fmt);
		uint32_t period_us = (pdesc && pdesc->period) ? pdesc->period : 1000;
		uint32_t frames = (rate * period_us) / 1000000;
		if (frames == 0)
			frames = 48;

		struct sof_ipc_pcm_params prms;
		memset(&prms, 0, sizeof(prms));
		prms.params.rate = rate;
		prms.params.channels = channels;
		prms.params.frame_fmt = fmt;
		prms.params.sample_container_bytes = cont_bytes;
		prms.params.sample_valid_bytes = valid_bytes;
		prms.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		prms.params.host_period_bytes = frames * channels * cont_bytes;
		prms.comp_id = dev_comp_id(host_or_sched);
		prms.params.direction = host_or_sched->direction;
		for (uint32_t c = 0; c < SOF_IPC_MAX_CHANNELS; c++) {
			if (c == 0)
				prms.params.chmap[c] = (channels == 1) ? SOF_CHMAP_MONO : SOF_CHMAP_FL;
			else if (c == 1 && channels > 1)
				prms.params.chmap[c] = SOF_CHMAP_FR;
			else
				prms.params.chmap[c] = SOF_CHMAP_NA;
		}

		int ret_params = pipeline_params(pipe, host_or_sched, &prms);
		if (ret_params < 0) {
			LOG_ERR("sof_static_pipeline_set_sample_rate: pipeline_params failed %d for pipeline %u",
				ret_params, pipeline_id);
			return ret_params;
		}

		int ret_prep = pipeline_prepare(pipe, host_or_sched);
		if (ret_prep < 0) {
			LOG_ERR("sof_static_pipeline_set_sample_rate: pipeline_prepare failed %d for pipeline %u",
				ret_prep, pipeline_id);
			return ret_prep;
		}
	}

	LOG_INF("Static pipeline %u sample rate reconfigured to %u Hz", pipeline_id, rate);
	return 0;
}

/**
 * \brief Retrieve the current stream sample rate for a specific static pipeline.
 * \param[in] pipeline_id Target pipeline identifier.
 * \return Pipeline sample rate in Hz, or 0 if pipeline not found.
 */
uint32_t sof_static_pipeline_get_sample_rate(uint32_t pipeline_id)
{
	for (size_t i = 0; i < s_num_pipelines; i++) {
		if (s_pipeline_ids[i] == pipeline_id)
			return s_pipeline_rates[i] ? s_pipeline_rates[i] : 48000;
	}

	LOG_ERR("sof_static_pipeline_get_sample_rate: pipeline %u not found", pipeline_id);
	return 0;
}

/**
 * \brief Override the stream sample rate across all active static pipelines.
 * \param[in] rate Target sample rate in Hz (e.g. 48000).
 * \return 0 on success, negative errno on failure.
 */
int sof_static_set_sample_rate(uint32_t rate)
{
	if (s_num_pipelines == 0) {
		LOG_ERR("sof_static_set_sample_rate: no active static pipelines");
		return -ENODEV;
	}

	int last_ret = 0;
	for (size_t i = 0; i < s_num_pipelines; i++) {
		int ret = sof_static_pipeline_set_sample_rate(s_pipeline_ids[i], rate);
		if (ret < 0)
			last_ret = ret;
	}

	LOG_INF("All static pipelines sample rate set to %u Hz", rate);
	return last_ret;
}

/**
 * \brief Retrieve the primary stream sample rate from the first static pipeline.
 * \return Current sample rate in Hz.
 */
uint32_t sof_static_get_sample_rate(void)
{
	if (s_num_pipelines > 0 && s_pipeline_rates[0])
		return s_pipeline_rates[0];

	return 48000;
}
