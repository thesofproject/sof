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
#include <kernel/header.h>
#include <module/ipc4/base-config.h>
#include <rtos/sof.h>
#include <rtos/alloc.h>
#include <rtos/task.h>
#include <sof/schedule/schedule.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "static_pipeline_modules.h"

LOG_MODULE_REGISTER(static_pipeline_loader, CONFIG_SOF_LOG_LEVEL);

/*
 * Maximum capacity limits for static topology tracking tables.
 * Sized generously to support complex embedded multi-channel topologies
 * while avoiding dynamic heap overhead during runtime lookups.
 */
#define MAX_STATIC_PIPELINES    16
#define MAX_STATIC_COMPS        64
#define MAX_STATIC_BUFFERS      64
#define MAX_STATIC_CONTROLS     64

/* Active declarative topology descriptor */
static const struct sof_static_topology *s_active_topo;

/* Static tracking tables for fast ID-to-pointer lookups */
static struct pipeline *s_pipelines[MAX_STATIC_PIPELINES];
static uint32_t s_pipeline_ids[MAX_STATIC_PIPELINES];
static size_t s_num_pipelines;

static struct comp_dev *s_comps[MAX_STATIC_COMPS];
static uint32_t s_comp_ids[MAX_STATIC_COMPS];
static size_t s_num_comps;

static struct comp_buffer *s_buffers[MAX_STATIC_BUFFERS];
static uint32_t s_buffer_ids[MAX_STATIC_BUFFERS];
static size_t s_num_buffers;

/* Cached kcontrol values */
static int32_t s_control_vals[MAX_STATIC_CONTROLS];

/*
 * Primary stream sample rate in Hz. Initialized to 0 (unassigned) and
 * dynamically resolved from the declarative topology descriptor at boot
 * (or updated at runtime via sof_static_set_sample_rate()).
 */
static uint32_t s_sample_rate;

/**
 * \brief Send initial binary configuration blob (e.g. EQ coefficients, DRC tuning) to a component.
 *
 * In hostless environments, modules requiring configuration tables (such as equalizer
 * filter coefficients or DRC curves) must be configured directly at boot. This routine
 * dispatches the initial configuration blob via the module adapter's set_configuration ops.
 *
 * \param[in] dev Target component device pointer.
 * \param[in] abi_blob Pointer to ABI configuration blob starting with struct sof_abi_hdr.
 * \param[in] abi_blob_total_size Total size of blob in bytes including header.
 * \return 0 on success, negative errno on failure.
 */
static int send_comp_config(struct comp_dev *dev, const void *abi_blob, size_t abi_blob_total_size)
{
	/* Validate component pointer and configuration blob payload */
	if (!dev || !abi_blob || abi_blob_total_size == 0)
		return -EINVAL;

	const struct sof_abi_hdr *hdr = (const struct sof_abi_hdr *)abi_blob;
	struct processing_module *mod = comp_mod(dev);

	/* Verify the component's module adapter driver provides set_configuration */
	if (!mod || !mod->dev || !mod->dev->drv || !mod->dev->drv->adapter_ops ||
	    !mod->dev->drv->adapter_ops->set_configuration) {
		LOG_ERR("No adapter_ops set_configuration for comp %d", dev->ipc_config.id);
		return -EINVAL;
	}

#if CONFIG_IPC_MAJOR_4
	/*
	 * In IPC4 mode, pass the raw configuration payload directly following the
	 * ABI header to the module adapter's set_configuration callback.
	 */
	const uint8_t *raw_data = (const uint8_t *)abi_blob + sizeof(struct sof_abi_hdr);
	int ret = mod->dev->drv->adapter_ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE,
								hdr->size, raw_data,
								hdr->size, NULL, 0);
#else
	/*
	 * In IPC3 mode, wrap the configuration payload in a struct sof_ipc_ctrl_data
	 * with command SOF_CTRL_CMD_BINARY as expected by IPC3 processing components.
	 */
	size_t cdata_size = sizeof(struct sof_ipc_ctrl_data) + sizeof(struct sof_abi_hdr) + hdr->size;
	struct sof_ipc_ctrl_data *cdata = rzalloc(SOF_MEM_FLAG_USER, cdata_size);
	if (!cdata) {
		LOG_ERR("Failed to allocate ctrl_data size %zu", cdata_size);
		return -ENOMEM;
	}

	cdata->cmd = SOF_CTRL_CMD_BINARY;
	cdata->num_elems = hdr->size;
	cdata->data[0].magic = hdr->magic;
	cdata->data[0].type = hdr->type;
	cdata->data[0].size = hdr->size;
	cdata->data[0].abi = hdr->abi;
	memcpy_s(cdata->data[0].data, hdr->size, (const uint8_t *)abi_blob + sizeof(struct sof_abi_hdr), hdr->size);

	int ret = mod->dev->drv->adapter_ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE,
								hdr->size, (const uint8_t *)cdata,
								hdr->size, NULL, 0);
	rfree(cdata);
#endif

	/* Log configuration status for diagnostics */
	if (ret < 0) {
		LOG_ERR("set_configuration failed for comp %d: ret %d", dev->ipc_config.id, ret);
	} else {
		LOG_INF("Configuration blob loaded for comp %d (%u bytes)", dev->ipc_config.id, hdr->size);
	}

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
	uint32_t valid_bytes = get_sample_bitdepth(fmt) / 8;

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
 * \brief Resolve the primary stream sample rate from declarative topology.
 *
 * Scans pipelines and components for declared sample rates, falling back to
 * standard 48 kHz if completely unspecified.
 *
 * \param[in] topo Topology descriptor pointer.
 */
static void sof_static_topology_resolve_rate(const struct sof_static_topology *topo)
{
	if (s_sample_rate)
		return;

	/* First check if the primary pipeline declares a default rate */
	for (size_t p = 0; p < topo->num_pipelines; p++) {
		if (topo->pipelines[p].default_rate) {
			s_sample_rate = topo->pipelines[p].default_rate;
			break;
		}
	}

	/* Fallback to checking the first component with a valid default rate */
	if (!s_sample_rate) {
		for (size_t c = 0; c < topo->num_comps; c++) {
			if (topo->comps[c].caps.default_rate) {
				s_sample_rate = topo->comps[c].caps.default_rate;
				break;
			}
		}
	}

	/* If completely unspecified in topology, log and fall back to standard 48 kHz */
	if (!s_sample_rate) {
		LOG_INF("Primary sample rate unspecified in topology, falling back to 48000 Hz");
		s_sample_rate = 48000;
	}
}

/**
 * \brief Stage 1: Allocate top-level pipelines.
 *
 * Allocate pipeline objects first because components and buffers require an
 * owning pipeline context for scheduling, buffer allocation, and clock domains.
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
		pipe->period = pdesc->period ? pdesc->period : 1000;
		pipe->frames_per_sched = pdesc->frames_per_sched ? pdesc->frames_per_sched : 48;
		pipe->time_domain = pdesc->time_domain ? pdesc->time_domain : SOF_TIME_DOMAIN_TIMER;

		/* Record pipeline instance in static tracking array */
		s_pipelines[s_num_pipelines] = pipe;
		s_pipeline_ids[s_num_pipelines] = pdesc->pipeline_id;
		s_num_pipelines++;

		LOG_INF("Pipeline %u ('%s') initialized (period %u us, %u frames)",
			pdesc->pipeline_id, pdesc->name ? pdesc->name : "", pipe->period, pipe->frames_per_sched);
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
						 const struct comp_ipc_config *cfg)
{
	/* Derive DAI sampling frequency from component caps or resolved rate */
	uint32_t dai_rate = cdesc->caps.default_rate ? cdesc->caps.default_rate : s_sample_rate;
	struct ipc_config_dai dai_cfg = {
		.type = cdesc->ep.dai.dai_type,
		.dai_index = cdesc->ep.dai.dai_index,
		.direction = cdesc->direction,
		.sampling_frequency = dai_rate,
		.dma_buffer_size = 1024,
		.format = cdesc->ep.dai.format,
	};
	struct comp_dev *dev = drv->ops.create(drv, cfg, &dai_cfg);

	if (!dev)
		return NULL;

	struct sof_ipc_dai_config spec_cfg = {
		.type = cdesc->ep.dai.dai_type,
		.dai_index = cdesc->ep.dai.dai_index,
		.format = cdesc->ep.dai.format,
	};

	/* Invoke optional platform/endpoint-specific configuration callback */
	if (cdesc->ep.dai.configure) {
		int ret_cfg = cdesc->ep.dai.configure(dev, &dai_cfg, &spec_cfg);
		if (ret_cfg < 0) {
			LOG_ERR("DAI endpoint configure callback failed %d for comp %u",
				ret_cfg, cdesc->id);
			return NULL;
		}
	}

	/* Apply DAI configuration to driver data */
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dd) {
		comp_dai_config(dd, dev, &dai_cfg, &spec_cfg);
		if (!dd->dai_spec_config) {
			dd->dai_spec_config = sof_heap_alloc(dd->alloc_ctx.heap,
							     SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
							     sizeof(struct sof_ipc_dai_config), 0);
			if (dd->dai_spec_config)
				memcpy(dd->dai_spec_config, &spec_cfg, sizeof(struct sof_ipc_dai_config));
		}
	}

	/* Configure underlying physical DAI controller instance */
	struct dai *dai = dai_get(cdesc->ep.dai.dai_type, cdesc->ep.dai.dai_index, DAI_CREAT);

	if (dai) {
		dai_set_config(dai, &dai_cfg, &spec_cfg, sizeof(spec_cfg));
		dai_put(dai);
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
						    const struct comp_ipc_config *cfg,
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

		/* Populate standard SOF component IPC configuration */
		struct comp_ipc_config cfg = {
			.id = cdesc->id,
			.pipeline_id = cdesc->pipeline_id,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = cdesc->caps.default_fmt,
			.type = drv_type,
		};

		struct comp_dev *dev;

		if (cdesc->type == SOF_STATIC_COMP_DAI)
			dev = sof_static_init_dai_comp(cdesc, drv, &cfg);
		else
			dev = sof_static_init_module_comp(cdesc, drv, &cfg, topo);

		if (!dev) {
			LOG_ERR("Failed to create component %u ('%s')", cdesc->id, cdesc->name);
			return -ENOMEM;
		}

		/* Set component stream direction and owning pipeline */
		dev->direction = cdesc->direction;
		dev->pipeline = sof_static_pipeline_get(cdesc->pipeline_id);

		/* Find owning pipeline descriptor to resolve scheduling period */
		const struct sof_static_pipeline_desc *pdesc = NULL;
		for (size_t p = 0; p < topo->num_pipelines; p++) {
			if (topo->pipelines[p].pipeline_id == cdesc->pipeline_id) {
				pdesc = &topo->pipelines[p];
				break;
			}
		}
		dev->period = (pdesc && pdesc->period) ? pdesc->period : 1000;

		/* Derive component frames per scheduling period */
		uint32_t comp_rate = cdesc->caps.default_rate ? cdesc->caps.default_rate :
				     ((pdesc && pdesc->default_rate) ? pdesc->default_rate :
				      s_sample_rate);
		dev->frames = (comp_rate * dev->period) / 1000000;
		if (dev->frames == 0)
			dev->frames = 48;

		/* Send initial configuration blob if specified (EQ, DRC, etc.) */
		if (cdesc->init_blob && cdesc->init_blob_size > 0)
			send_comp_config(dev, cdesc->init_blob, cdesc->init_blob_size);

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
			if (!fmt)
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

		/* Default fallbacks if unassigned */
		if (!rate)
			rate = s_sample_rate;
		if (!channels)
			channels = 2;
		if (!fmt)
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
		pipeline_connect(src, buf, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(sink, buf, PPL_CONN_DIR_BUFFER_TO_COMP);
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
		if (!pipe)
			continue;

		/* Bind primary source, sink, and scheduling components */
		pipe->source_comp = sof_static_comp_get(pdesc->source_comp_id);
		pipe->sink_comp = sof_static_comp_get(pdesc->sink_comp_id);
		pipe->sched_comp = sof_static_comp_get(pdesc->sched_comp_id);

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

			/* Determine stream sampling rate */
			uint32_t rate = pdesc->default_rate;
			if (!rate && cdesc)
				rate = cdesc->caps.default_rate;
			if (!rate)
				rate = s_sample_rate;

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
			uint32_t valid_bytes = get_sample_bitdepth(fmt) / 8;
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
			int ret_prep = pipeline_prepare(pipe, host_or_sched);
			LOG_INF("Pipeline %u params (rate %u, ch %u, fmt %u) ret=%d, prepare ret=%d",
				pdesc->pipeline_id, rate, channels, fmt, ret_prms, ret_prep);
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
			const struct sof_static_pipeline_desc *pdesc = NULL;
			for (size_t p = 0; p < topo->num_pipelines; p++) {
				if (topo->pipelines[p].pipeline_id == cdesc->pipeline_id) {
					pdesc = &topo->pipelines[p];
					break;
				}
			}

			/* Determine component-specific sample rate */
			uint32_t rate = cdesc->caps.default_rate;
			if (!rate && pdesc)
				rate = pdesc->default_rate;
			if (!rate)
				rate = s_sample_rate;

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
			uint32_t valid_bytes = get_sample_bitdepth(fmt) / 8;
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
			int ret_prep = comp_prepare(dev);
			LOG_INF("Component %u ('%s') explicit prepare (rate %u, ch %u, fmt %u): prms=%d prep=%d state=%d",
				dev_comp_id(dev), cdesc->name, rate, channels, fmt, ret_prms, ret_prep, dev->state);
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
 * \return 0 on success.
 */
static int sof_static_topology_init_controls(const struct sof_static_topology *topo)
{
	for (size_t i = 0; i < topo->num_controls && i < MAX_STATIC_CONTROLS; i++) {
		const struct sof_static_kcontrol *ctl = &topo->controls[i];
		s_control_vals[i] = ctl->def;
		sof_static_kcontrol_set(ctl->id, ctl->def);
		LOG_INF("Kcontrol [%u] '%s' (comp %u, type %d, def %d)",
			ctl->id, ctl->name, ctl->target_comp_id, ctl->type, ctl->def);
	}

	return 0;
}

/**
 * \brief Load and instantiate a declarative static audio topology.
 *
 * Executes the complete multi-stage construction pipeline:
 *   Stage 0: Resolve primary stream sample rate
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
int sof_static_topology_init(const struct sof_static_topology *topo)
{
	int ret;

	/* Validate input topology pointer */
	if (!topo) {
		LOG_ERR("Invalid topology descriptor");
		return -EINVAL;
	}

	LOG_INF("=== Loading Static Audio Topology: '%s' ===", topo->name ? topo->name : "Unnamed");
	s_active_topo = topo;

	/* Stage 0: Dynamically resolve stream sample rate */
	sof_static_topology_resolve_rate(topo);

	/* Stage 1: Allocate top-level pipelines */
	ret = sof_static_topology_init_pipelines(topo);
	if (ret < 0)
		return ret;

	/* Stage 2: Instantiate components */
	ret = sof_static_topology_init_comps(topo);
	if (ret < 0)
		return ret;

	/* Stage 3: Allocate intermediate audio buffers */
	ret = sof_static_topology_init_buffers(topo);
	if (ret < 0)
		return ret;

	/* Stage 4: Connect pipeline graph routes */
	ret = sof_static_topology_init_routes(topo);
	if (ret < 0)
		return ret;

	/* Stage 5: Bind pipeline endpoints and propagate stream parameters */
	ret = sof_static_topology_init_pipeline_endpoints(topo);
	if (ret < 0)
		return ret;

	/* Stage 6: Explicitly configure and prepare all components */
	ret = sof_static_topology_prepare_comps(topo);
	if (ret < 0)
		return ret;

	/* Stage 7: Initialize kcontrols with default values */
	ret = sof_static_topology_init_controls(topo);
	if (ret < 0)
		return ret;

	LOG_INF("Static audio topology initialized successfully (%zu pipelines, %zu comps, %zu buffers, %zu controls)",
		topo->num_pipelines, topo->num_comps, topo->num_buffers, topo->num_controls);
	return 0;
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

	/* Look up component's registered static module operations */
	const struct sof_static_module_ops *ops = dev->drv ? sof_static_find_module_ops(dev->drv->uid) : NULL;

	/* Dispatch control value to target component using registered operations */
	if (ops) {
		switch (ctl->type) {
		case SOF_STATIC_CTRL_VOLUME:
			/* Linear volume fader control */
			if (ops->apply_volume)
				ops->apply_volume(dev, ctl->channels, val);
			break;

		case SOF_STATIC_CTRL_SWITCH:
			/* Boolean mute or module bypass switch */
			if (ops->apply_switch)
				ops->apply_switch(dev, ctl->channels, val);
			break;

		case SOF_STATIC_CTRL_ENUM:
			/* Enumerated route or channel selector */
			if (ops->apply_enum)
				ops->apply_enum(dev, 0, val);
			break;

		default:
			LOG_ERR("sof_static_kcontrol_set: invalid control type %d for ctl %u",
				ctl->type, ctl->id);
			break;
		}
	}

	/* Cache updated control value and log confirmation */
	s_control_vals[ctl_idx] = val;
	LOG_INF("Kcontrol [%u] '%s' set to %d", ctl->id, ctl->name, val);
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

		/* Propagate PRE_START trigger across the pipeline graph */
		int ret = pipeline_trigger_run(pipe, dev, COMP_TRIGGER_PRE_START);
		if (ret < 0) {
			LOG_ERR("Pipeline %u pre-start trigger failed: %d", pipeline_id, ret);
			return ret;
		}

		/* Propagate START trigger across the pipeline graph */
		ret = pipeline_trigger_run(pipe, dev, COMP_TRIGGER_START);
		if (ret < 0) {
			LOG_ERR("Pipeline %u start trigger failed: %d", pipeline_id, ret);
			return ret;
		}

		/* Explicitly notify all constituent components and mark state ACTIVE */
		if (s_active_topo) {
			for (size_t j = 0; j < s_active_topo->num_comps; j++) {
				if (s_active_topo->comps[j].pipeline_id == pipeline_id) {
					struct comp_dev *c = sof_static_comp_get(s_active_topo->comps[j].id);
					if (c) {
						comp_trigger(c, COMP_TRIGGER_PRE_START);
						comp_trigger(c, COMP_TRIGGER_START);
						c->state = COMP_STATE_ACTIVE;
					}
				}
			}
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

		/* Explicitly notify all constituent components and mark state PAUSED */
		if (s_active_topo) {
			for (size_t j = 0; j < s_active_topo->num_comps; j++) {
				if (s_active_topo->comps[j].pipeline_id == pipeline_id) {
					struct comp_dev *c = sof_static_comp_get(s_active_topo->comps[j].id);
					if (c) {
						comp_trigger(c, COMP_TRIGGER_STOP);
						c->state = COMP_STATE_PAUSED;
					}
				}
			}
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
 * \brief Override the primary stream sample rate across active static pipelines.
 * \param[in] rate Target sample rate in Hz (e.g. 48000).
 * \return 0 on success.
 */
int sof_static_set_sample_rate(uint32_t rate)
{
	s_sample_rate = rate;
	return 0;
}

/**
 * \brief Retrieve the current primary stream sample rate.
 * \return Current sample rate in Hz.
 */
uint32_t sof_static_get_sample_rate(void)
{
	/*
	 * Return the active primary sample rate. If topology init has run,
	 * this is derived from the declarative topology or runtime override;
	 * if queried before init, fall back to 48 kHz.
	 */
	return s_sample_rate ? s_sample_rate : 48000;
}
