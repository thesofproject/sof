// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/drivers/idc.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern struct tr_ctx comp_tr;

/**
 * Retrieves component config data from component ipc.
 * @param comp Component ipc data.
 * @return Pointer to the component config data.
 */
static inline struct sof_ipc_comp_config *comp_config(struct sof_ipc_comp *comp)
{
	return (struct sof_ipc_comp_config *)(comp + 1);
}

void ipc_build_stream_posn(struct sof_ipc_stream_posn *posn, uint32_t type,
			   uint32_t id)
{
	posn->rhdr.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | type | id;
	posn->rhdr.hdr.size = sizeof(*posn);
	posn->comp_id = id;
}

void ipc_build_comp_event(struct sof_ipc_comp_event *event, uint32_t type,
			  uint32_t id)
{
	event->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_NOTIFICATION |
		id;
	event->rhdr.hdr.size = sizeof(*event);
	event->src_comp_type = type;
	event->src_comp_id = id;
}

void ipc_build_trace_posn(struct sof_ipc_dma_trace_posn *posn)
{
	posn->rhdr.hdr.cmd =  SOF_IPC_GLB_TRACE_MSG |
		SOF_IPC_TRACE_DMA_POSITION;
	posn->rhdr.hdr.size = sizeof(*posn);
}

static const struct comp_driver *get_drv(struct sof_ipc_comp *comp)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;
	const struct comp_driver *drv = NULL;
	struct comp_driver_info *info;
	struct sof_ipc_comp_ext *comp_ext;

	/* do we have extended data ? */
	if (!comp->ext_data_length) {
		/* search driver list for driver type */
		list_for_item(clist, &drivers->list) {
			info = container_of(clist, struct comp_driver_info, list);
			if (info->drv->type == comp->type) {
				drv = info->drv;
				break;
			}
		}

		if (!drv)
			tr_err(&comp_tr, "get_drv(): driver not found, comp->type = %u",
			       comp->type);

		goto out;
	}

	/* Basic sanity check of the total size and extended data
	 * length. A bit lax because in this generic code we don't know
	 * which derived comp we have and how much its specific members
	 * add.
	 */
	if (comp->hdr.size < sizeof(*comp) + comp->ext_data_length) {
		tr_err(&comp_tr, "Invalid size, hdr.size=0x%x, ext_data_length=0x%x\n",
		       comp->hdr.size, comp->ext_data_length);
		goto out;
	}

	comp_ext = (struct sof_ipc_comp_ext *)
		   ((uint8_t *)comp + comp->hdr.size -
		    comp->ext_data_length);

	/* UUID is first item in extended data - check its big enough */
	if (comp->ext_data_length < UUID_SIZE) {
		tr_err(&comp_tr, "UUID is invalid!\n");
		goto out;
	}

	/* search driver list with UUID */
	spin_lock(&drivers->lock);
	list_for_item(clist, &drivers->list) {
		info = container_of(clist, struct comp_driver_info,
				    list);
		if (!memcmp(info->drv->uid, comp_ext->uuid,
			    UUID_SIZE)) {
			drv = info->drv;
			break;
		}
	}

	if (!drv)
		tr_err(&comp_tr,
		       "get_drv(): the provided UUID (%8x%8x%8x%8x) doesn't match to any driver!",
		       *(uint32_t *)(&comp_ext->uuid[0]),
		       *(uint32_t *)(&comp_ext->uuid[4]),
		       *(uint32_t *)(&comp_ext->uuid[8]),
		       *(uint32_t *)(&comp_ext->uuid[12]));

out:
	if (drv)
		tr_dbg(&comp_tr, "get_drv(), found driver type %d, uuid %pU",
		       drv->type, drv->tctx->uuid_p);

	spin_unlock(&drivers->lock);

	return drv;
}

/* build generic IPC data for all components */
static void comp_common_builder(struct sof_ipc_comp *comp,
				struct comp_ipc_config *config)
{
	struct sof_ipc_comp_config *ipc_config;

	/* create the new component */
	memset(config, 0, sizeof(*config));
	config->core = comp->core;
	config->id = comp->id;
	config->pipeline_id = comp->pipeline_id;
	config->type = comp->type;

	/* buffers dont have the following data */
	if (comp->type != SOF_COMP_BUFFER) {
		/* ipc common config is always after sof_ipc_comp */
		ipc_config = (struct sof_ipc_comp_config *)(comp + 1);
		config->frame_fmt = ipc_config->frame_fmt;
		config->periods_sink = ipc_config->periods_sink;
		config->periods_source = ipc_config->periods_source;
		config->xrun_action = ipc_config->xrun_action;
	}
}
/*
 * Stores all the "legacy" init IPC data locally.
 */
union ipc_config_specific {
	struct ipc_config_host host;
	struct ipc_config_dai dai;
	struct ipc_config_volume volume;
	struct ipc_config_src src;
	struct ipc_config_asrc asrc;
	struct ipc_config_tone tone;
	struct ipc_config_process process;
	struct ipc_comp_file file;
} __attribute__((packed, aligned(4)));

/* build component specific data */
static void comp_specific_builder(struct sof_ipc_comp *comp,
				  union ipc_config_specific *config)
{
#if CONFIG_LIBRARY
	struct sof_ipc_comp_file *file = (struct sof_ipc_comp_file *)comp;
#else
	struct sof_ipc_comp_host *host = (struct sof_ipc_comp_host *)comp;
	struct sof_ipc_comp_dai *dai = (struct sof_ipc_comp_dai *)comp;
#endif
	struct sof_ipc_comp_volume *vol = (struct sof_ipc_comp_volume *)comp;
	struct sof_ipc_comp_process *proc = (struct sof_ipc_comp_process *)comp;
	struct sof_ipc_comp_src *src = (struct sof_ipc_comp_src *)comp;
	struct sof_ipc_comp_asrc *asrc = (struct sof_ipc_comp_asrc *)comp;
	struct sof_ipc_comp_tone *tone = (struct sof_ipc_comp_tone *)comp;

	memset(config, 0, sizeof(*config));

	switch (comp->type) {
#if CONFIG_LIBRARY
	/* test bench maps host and DAIs to a file */
	case SOF_COMP_HOST:
	case SOF_COMP_SG_HOST:
	case SOF_COMP_DAI:
	case SOF_COMP_SG_DAI:
		config->file.channels = file->channels;
		config->file.fn = file->fn;
		config->file.frame_fmt = file->frame_fmt;
		config->file.mode = file->mode;
		config->file.rate = file->rate;
		break;
#else
	case SOF_COMP_HOST:
	case SOF_COMP_SG_HOST:
		config->host.direction = host->direction;
		config->host.no_irq = host->no_irq;
		config->host.dmac_config = host->dmac_config;
		break;
	case SOF_COMP_DAI:
	case SOF_COMP_SG_DAI:
		config->dai.dai_index = dai->dai_index;
		config->dai.direction = dai->direction;
		config->dai.type = dai->type;
		break;
#endif
	case SOF_COMP_VOLUME:
		config->volume.channels = vol->channels;
		config->volume.initial_ramp = vol->initial_ramp;
		config->volume.max_value = vol->max_value;
		config->volume.min_value = vol->min_value;
		config->volume.ramp = vol->ramp;
		break;
	case SOF_COMP_SRC:
		config->src.rate_mask = src->rate_mask;
		config->src.sink_rate = src->sink_rate;
		config->src.source_rate = src->source_rate;
		break;
	case SOF_COMP_TONE:
		config->tone.ampl_mult = tone->ampl_mult;
		config->tone.amplitude = tone->amplitude;
		config->tone.freq_mult = tone->freq_mult;
		config->tone.frequency = tone->frequency;
		config->tone.length = tone->length;
		config->tone.period = tone->period;
		config->tone.ramp_step = tone->ramp_step;
		config->tone.repeats = tone->repeats;
		config->tone.sample_rate = tone->sample_rate;
		break;
	case SOF_COMP_ASRC:
		config->asrc.source_rate = src->source_rate;
		config->asrc.sink_rate = src->sink_rate;
		config->asrc.asynchronous_mode = asrc->asynchronous_mode;
		config->asrc.operation_mode = asrc->operation_mode;
		break;
	case SOF_COMP_EQ_IIR:
	case SOF_COMP_EQ_FIR:
	case SOF_COMP_KEYWORD_DETECT:
	case SOF_COMP_KPB:
	case SOF_COMP_SELECTOR:
	case SOF_COMP_DEMUX:
	case SOF_COMP_MUX:
	case SOF_COMP_DCBLOCK:
	case SOF_COMP_SMART_AMP:
	case SOF_COMP_CODEC_ADAPTOR:
	case SOF_COMP_NONE:
		config->process.type = proc->type;
		config->process.size = proc->size;
		config->process.data = proc->data;
		break;
	default:
		break;
	}
}

struct comp_dev *comp_new(struct sof_ipc_comp *comp)
{
	struct comp_ipc_config config;
	union ipc_config_specific spec;
	struct comp_dev *cdev;
	const struct comp_driver *drv;

	/* find the driver for our new component */
	drv = get_drv(comp);
	if (!drv)
		return NULL;

	/* validate size of ipc config */
	if (IPC_IS_SIZE_INVALID(*comp_config(comp))) {
		IPC_SIZE_ERROR_TRACE(&comp_tr, *comp_config(comp));
		return NULL;
	}

	tr_info(&comp_tr, "comp new %pU type %d id %d.%d",
		drv->tctx->uuid_p, comp->type, comp->pipeline_id, comp->id);

	/* build the component */
	comp_common_builder(comp, &config);
	comp_specific_builder(comp, &spec);
	cdev = drv->ops.create(drv, &config, &spec);
	if (!cdev) {
		comp_cl_err(drv, "comp_new(): unable to create the new component");
		return NULL;
	}

	list_init(&cdev->bsource_list);
	list_init(&cdev->bsink_list);

	return cdev;
}

int ipc_pipeline_new(struct ipc *ipc, ipc_pipe_new *_pipe_desc)
{
	struct sof_ipc_pipe_new *pipe_desc = ipc_from_pipe_new(_pipe_desc);
	struct ipc_comp_dev *ipc_pipe;
	struct pipeline *pipe;
	int ret;

	/* check whether the pipeline already exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, pipe_desc->comp_id);
	if (ipc_pipe != NULL) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline already exists, pipe_desc->comp_id = %u",
		       pipe_desc->comp_id);
		return -EINVAL;
	}

	/* check whether pipeline id is already taken */
	ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
					  pipe_desc->pipeline_id);
	if (ipc_pipe) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline id is already taken, pipe_desc->pipeline_id = %u",
		       pipe_desc->pipeline_id);
		return -EINVAL;
	}

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc->pipeline_id, pipe_desc->priority,
			    pipe_desc->comp_id);
	if (!pipe) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline_new() failed");
		return -ENOMEM;
	}

	/* configure pipeline */
	ret = pipeline_schedule_config(pipe, pipe_desc->sched_id,
				       pipe_desc->core, pipe_desc->period,
				       pipe_desc->period_mips,
				       pipe_desc->frames_per_sched,
				       pipe_desc->time_domain);
	if (ret) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline_schedule_config() failed");
		return ret;
	}

	/* set xrun time limit */
	ret = pipeline_xrun_set_limit(pipe, pipe_desc->xrun_limit_usecs);
	if (ret) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline_xrun_set_limit() failed");
		return ret;
	}

	/* allocate the IPC pipeline container */
	ipc_pipe = rballoc(SOF_MEM_FLAG_COHERENT, SOF_MEM_CAPS_RAM,
			   sizeof(struct ipc_comp_dev));
	if (!ipc_pipe) {
		pipeline_free(pipe);
		return -ENOMEM;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;
	ipc_pipe->core = pipe_desc->core;
	ipc_pipe->id = pipe_desc->comp_id;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->comp_list);

	return 0;
}

int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_pipe)
		return -ENODEV;

	/* check core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc_process_on_core(ipc_pipe->core, false);

	/* free buffer and remove from list */
	ret = pipeline_free(ipc_pipe->pipeline);
	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): pipeline_free() failed");
		return ret;
	}
	ipc_pipe->pipeline = NULL;
	list_item_del(&ipc_pipe->list);
	rfree(ipc_pipe);

	return 0;
}

int ipc_buffer_new(struct ipc *ipc, const struct sof_ipc_buffer *desc)
{
	struct ipc_comp_dev *ibd;
	struct comp_buffer *buffer;
	int ret = 0;

	/* check whether buffer already exists */
	ibd = ipc_get_comp_by_id(ipc, desc->comp.id);
	if (ibd != NULL) {
		tr_err(&ipc_tr, "ipc_buffer_new(): buffer already exists, desc->comp.id = %u",
		       desc->comp.id);
		return -EINVAL;
	}

	/* register buffer with pipeline */
	buffer = buffer_new(desc);
	if (!buffer) {
		tr_err(&ipc_tr, "ipc_buffer_new(): buffer_new() failed");
		return -ENOMEM;
	}

	ibd = rballoc(SOF_MEM_FLAG_COHERENT, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (!ibd) {
		buffer_free(buffer);
		return -ENOMEM;
	}
	ibd->cb = buffer;
	ibd->type = COMP_TYPE_BUFFER;
	ibd->core = desc->comp.core;
	ibd->id = desc->comp.id;

	/* add new buffer to the list */
	list_item_append(&ibd->list, &ipc->comp_list);

	return ret;
}

int ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id)
{
	struct ipc_comp_dev *ibd;
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	struct comp_dev *sink = NULL, *source = NULL;
	bool sink_active = false;
	bool source_active = false;

	/* check whether buffer exists */
	ibd = ipc_get_comp_by_id(ipc, buffer_id);
	if (!ibd)
		return -ENODEV;

	/* check core */
	if (!cpu_is_me(ibd->core))
		return ipc_process_on_core(ibd->core, false);

	/* try to find sink/source components to check if they still exists */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		/* check comp state if sink and source are valid */
		if (ibd->cb->sink == icd->cd &&
		    ibd->cb->sink->state != COMP_STATE_READY) {
			sink = ibd->cb->sink;
			sink_active = true;
		}

		if (ibd->cb->source == icd->cd &&
		    ibd->cb->source->state != COMP_STATE_READY) {
			source = ibd->cb->source;
			source_active = true;
		}
	}

	/*
	 * A buffer could be connected to 2 different pipelines. When one pipeline is freed, the
	 * buffer comp that belongs in this pipeline will need to be freed even when the other
	 * pipeline that the buffer is connected to is active. Check if both ends are active before
	 * freeing the buffer.
	 */
	if (sink_active && source_active)
		return -EINVAL;

	/*
	 * Disconnect the buffer from the active component before freeing it.
	 */
	if (sink)
		pipeline_disconnect(sink, ibd->cb, PPL_CONN_DIR_BUFFER_TO_COMP);

	if (source)
		pipeline_disconnect(source, ibd->cb, PPL_CONN_DIR_COMP_TO_BUFFER);

	/* free buffer and remove from list */
	buffer_free(ibd->cb);
	list_item_del(&ibd->list);
	rfree(ibd);

	return 0;
}

static int ipc_comp_to_buffer_connect(struct ipc_comp_dev *comp,
				      struct ipc_comp_dev *buffer)
{
	int ret;

	if (!cpu_is_me(comp->core))
		return ipc_process_on_core(comp->core, false);

	tr_dbg(&ipc_tr, "ipc: comp sink %d, source %d  -> connect", buffer->id,
	       comp->id);

	/* check if it's a connection between cores */
	if (buffer->core != comp->core) {
		dcache_invalidate_region(buffer->cb, sizeof(*buffer->cb));

		buffer->cb->inter_core = true;

		if (!comp->cd->is_shared) {
			comp->cd = comp_make_shared(comp->cd);
			if (!comp->cd)
				return -ENOMEM;
		}
	}

	ret = pipeline_connect(comp->cd, buffer->cb,
			       PPL_CONN_DIR_COMP_TO_BUFFER);

	dcache_writeback_invalidate_region(buffer->cb, sizeof(*buffer->cb));

	return ret;
}

static int ipc_buffer_to_comp_connect(struct ipc_comp_dev *buffer,
				      struct ipc_comp_dev *comp)
{
	int ret;

	if (!cpu_is_me(comp->core))
		return ipc_process_on_core(comp->core, false);

	tr_dbg(&ipc_tr, "ipc: comp sink %d, source %d  -> connect", comp->id,
	       buffer->id);

	/* check if it's a connection between cores */
	if (buffer->core != comp->core) {
		dcache_invalidate_region(buffer->cb, sizeof(*buffer->cb));

		buffer->cb->inter_core = true;

		if (!comp->cd->is_shared) {
			comp->cd = comp_make_shared(comp->cd);
			if (!comp->cd)
				return -ENOMEM;
		}
	}

	ret = pipeline_connect(comp->cd, buffer->cb,
			       PPL_CONN_DIR_BUFFER_TO_COMP);

	dcache_writeback_invalidate_region(buffer->cb, sizeof(*buffer->cb));

	return ret;
}

int ipc_comp_connect(struct ipc *ipc, ipc_pipe_comp_connect *_connect)
{
	struct sof_ipc_pipe_comp_connect *connect = ipc_from_pipe_connect(_connect);
	struct ipc_comp_dev *icd_source;
	struct ipc_comp_dev *icd_sink;

	/* check whether the components already exist */
	icd_source = ipc_get_comp_by_id(ipc, connect->source_id);
	if (!icd_source) {
		tr_err(&ipc_tr, "ipc_comp_connect(): source component does not exist, source_id = %u sink_id = %u",
		       connect->source_id, connect->sink_id);
		return -EINVAL;
	}

	icd_sink = ipc_get_comp_by_id(ipc, connect->sink_id);
	if (!icd_sink) {
		tr_err(&ipc_tr, "ipc_comp_connect(): sink component does not exist, source_id = %d sink_id = %u",
		       connect->sink_id, connect->source_id);
		return -EINVAL;
	}

	/* check source and sink types */
	if (icd_source->type == COMP_TYPE_BUFFER &&
	    icd_sink->type == COMP_TYPE_COMPONENT)
		return ipc_buffer_to_comp_connect(icd_source, icd_sink);
	else if (icd_source->type == COMP_TYPE_COMPONENT &&
		 icd_sink->type == COMP_TYPE_BUFFER)
		return ipc_comp_to_buffer_connect(icd_source, icd_sink);
	else {
		tr_err(&ipc_tr, "ipc_comp_connect(): invalid source and sink types, connect->source_id = %u, connect->sink_id = %u",
		       connect->source_id, connect->sink_id);
		return -EINVAL;
	}
}

int ipc_comp_new(struct ipc *ipc, ipc_comp *_comp)
{
	struct sof_ipc_comp *comp = ipc_from_comp_new(_comp);
	struct comp_dev *cd;
	struct ipc_comp_dev *icd;

	/* check core is valid */
	if (comp->core >= CONFIG_CORE_COUNT) {
		tr_err(&ipc_tr, "ipc_comp_new(): comp->core = %u", comp->core);
		return -EINVAL;
	}

	/* check whether component already exists */
	icd = ipc_get_comp_by_id(ipc, comp->id);
	if (icd != NULL) {
		tr_err(&ipc_tr, "ipc_comp_new(): comp->id = %u", comp->id);
		return -EINVAL;
	}

	/* create component */
	cd = comp_new(comp);
	if (!cd) {
		tr_err(&ipc_tr, "ipc_comp_new(): component cd = NULL");
		return -EINVAL;
	}

	/* allocate the IPC component container */
	icd = rballoc(SOF_MEM_FLAG_COHERENT, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (!icd) {
		tr_err(&ipc_tr, "ipc_comp_new(): alloc failed");
		rfree(cd);
		return -ENOMEM;
	}
	icd->cd = cd;
	icd->type = COMP_TYPE_COMPONENT;
	icd->core = comp->core;
	icd->id = comp->id;

	/* add new component to the list */
	list_item_append(&icd->list, &ipc->comp_list);

	return 0;
}
