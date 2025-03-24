// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/telemetry/performance_monitor.h>
#include <rtos/idc.h>
#include <rtos/interrupt.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <rtos/symbol.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

static bool valid_ipc_buffer_desc(const struct sof_ipc_buffer *desc)
{
	if (desc->caps >= SOF_MEM_CAPS_LOWEST_INVALID)
		return false;

	/* TODO: check desc->size and maybe other things */
	return true;
}

/* create a new component in the pipeline */
struct comp_buffer *buffer_new(const struct sof_ipc_buffer *desc, bool is_shared)
{
	struct comp_buffer *buffer;

	tr_info(&buffer_tr, "buffer new size 0x%x id %d.%d flags 0x%x",
		desc->size, desc->comp.pipeline_id, desc->comp.id, desc->flags);

	if (!valid_ipc_buffer_desc(desc)) {
		tr_err(&buffer_tr, "Invalid buffer desc! New size 0x%x id %d.%d caps 0x%x",
		       desc->size, desc->comp.pipeline_id, desc->comp.id, desc->caps);
		return NULL;
	}

	/* allocate buffer */
	buffer = buffer_alloc(desc->size, desc->caps, desc->flags, PLATFORM_DCACHE_ALIGN,
			      is_shared);
	if (buffer) {
		buffer->stream.runtime_stream_params.id = desc->comp.id;
		buffer->stream.runtime_stream_params.pipeline_id = desc->comp.pipeline_id;
		buffer->core = desc->comp.core;

		memcpy_s(&buffer->tctx, sizeof(struct tr_ctx),
			 &buffer_tr, sizeof(struct tr_ctx));
	}

	return buffer;
}

int32_t ipc_comp_pipe_id(const struct ipc_comp_dev *icd)
{
	switch (icd->type) {
	case COMP_TYPE_COMPONENT:
		return dev_comp_pipe_id(icd->cd);
	case COMP_TYPE_BUFFER:
		return buffer_pipeline_id(icd->cb);
	case COMP_TYPE_PIPELINE:
		return icd->pipeline->pipeline_id;
	default:
		tr_err(&ipc_tr, "Unknown ipc component type %u", icd->type);
		return -EINVAL;
	};

	return 0;
}

/* Function overwrites PCM parameters (frame_fmt, buffer_fmt, channels, rate)
 * with buffer parameters when specific flag is set.
 */
static void comp_update_params(uint32_t flag,
			       struct sof_ipc_stream_params *params,
			       struct comp_buffer *buffer)
{
	if (flag & BUFF_PARAMS_FRAME_FMT)
		params->frame_fmt = audio_stream_get_frm_fmt(&buffer->stream);

	if (flag & BUFF_PARAMS_BUFFER_FMT)
		params->buffer_fmt = audio_stream_get_buffer_fmt(&buffer->stream);

	if (flag & BUFF_PARAMS_CHANNELS)
		params->channels = audio_stream_get_channels(&buffer->stream);

	if (flag & BUFF_PARAMS_RATE)
		params->rate = audio_stream_get_rate(&buffer->stream);
}

int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params)
{
	struct list_item *source_list;
	struct list_item *sink_list;
	struct comp_buffer *sinkb;
	struct comp_buffer *buf;
	int dir = dev->direction;

	if (!params) {
		comp_err(dev, "comp_verify_params(): !params");
		return -EINVAL;
	}

	source_list = comp_buffer_list(dev, PPL_DIR_UPSTREAM);
	sink_list = comp_buffer_list(dev, PPL_DIR_DOWNSTREAM);

	/* searching for endpoint component e.g. HOST, DETECT_TEST, which
	 * has only one sink or one source buffer.
	 */
	if (list_is_empty(source_list) != list_is_empty(sink_list)) {
		if (list_is_empty(sink_list))
			buf = comp_dev_get_first_data_producer(dev);
		else
			buf = comp_dev_get_first_data_consumer(dev);

		/* update specific pcm parameter with buffer parameter if
		 * specific flag is set.
		 */
		comp_update_params(flag, params, buf);

		/* overwrite buffer parameters with modified pcm
		 * parameters
		 */
		buffer_set_params(buf, params, BUFFER_UPDATE_FORCE);

		/* set component period frames */
		component_set_nearest_period_frames(dev, audio_stream_get_rate(&buf->stream));
	} else {
		/* for other components we iterate over all downstream buffers
		 * (for playback) or upstream buffers (for capture).
		 */
		if (dir == PPL_DIR_DOWNSTREAM) {
			comp_dev_for_each_consumer(dev, buf) {
				comp_update_params(flag, params, buf);
				buffer_set_params(buf, params,
						  BUFFER_UPDATE_FORCE);
			}
		} else {
			comp_dev_for_each_producer(dev, buf) {
				comp_update_params(flag, params, buf);
				buffer_set_params(buf, params,
						  BUFFER_UPDATE_FORCE);
			}
		}

		/* fetch sink buffer in order to calculate period frames */
		sinkb = comp_dev_get_first_data_consumer(dev);
		component_set_nearest_period_frames(dev, audio_stream_get_rate(&sinkb->stream));
	}

	return 0;
}
EXPORT_SYMBOL(comp_verify_params);

int comp_buffer_connect(struct comp_dev *comp, uint32_t comp_core,
			struct comp_buffer *buffer, uint32_t dir)
{
	/* check if it's a connection between cores */
	if (buffer->core != comp_core) {
#if CONFIG_INCOHERENT
		/* buffer must be shared */
		assert(audio_buffer_is_shared(&buffer->audio_buffer));
#else
		buffer->audio_buffer.is_shared = true;
#endif
		if (!comp->is_shared)
			comp_make_shared(comp);
	}

	return pipeline_connect(comp, buffer, dir);
}

int ipc_pipeline_complete(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	struct ipc_comp_dev *icd;
	struct pipeline *p;
	uint32_t pipeline_id;
	struct ipc_comp_dev *ipc_ppl_source;
	struct ipc_comp_dev *ipc_ppl_sink;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, comp_id);
	if (!ipc_pipe) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipe component id 0x%x failed",
		       comp_id);
		return -EINVAL;
	}

	/* check core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc_process_on_core(ipc_pipe->core, false);

	p = ipc_pipe->pipeline;

	/* get pipeline source component */
	ipc_ppl_source = ipc_get_ppl_src_comp(ipc, p->pipeline_id);
	if (!ipc_ppl_source) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipeline source failed");
		return -EINVAL;
	}

	/* get pipeline sink component */
	ipc_ppl_sink = ipc_get_ppl_sink_comp(ipc, p->pipeline_id);
	if (!ipc_ppl_sink) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipeline sink failed");
		return -EINVAL;
	}

	/* find the scheduling component */
	icd = ipc_get_comp_by_id(ipc, p->sched_id);
	if (!icd) {
		tr_warn(&ipc_tr, "ipc_pipeline_complete(): no scheduling component specified, use comp 0x%x",
			ipc_ppl_sink->id);

		icd = ipc_ppl_sink;
	}

	if (icd->core != ipc_pipe->core) {
		tr_err(&ipc_tr, "ipc_pipeline_complete(): icd->core (%d) != ipc_pipe->core (%d) for pipeline scheduling component icd->id 0x%x",
		       icd->core, ipc_pipe->core, icd->id);
		return -EINVAL;
	}

	p->sched_comp = icd->cd;

	pipeline_id = ipc_pipe->pipeline->pipeline_id;

	tr_dbg(&ipc_tr, "ipc: pipe %d -> complete on comp 0x%x", pipeline_id,
	       comp_id);

	return pipeline_complete(ipc_pipe->pipeline, ipc_ppl_source->cd,
				 ipc_ppl_sink->cd);
}

int ipc_comp_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *icd;
	struct comp_buffer *buffer;
	struct comp_buffer *safe;
	uint32_t flags;

	/* check whether component exists */
	icd = ipc_get_comp_by_id(ipc, comp_id);
	if (!icd) {
		tr_err(&ipc_tr, "ipc_comp_free(): comp id: 0x%x is not found",
		       comp_id);
		return -ENODEV;
	}

	/* check core */
	if (!cpu_is_me(icd->core))
		return ipc_process_on_core(icd->core, false);

	/* check state */
	if (icd->cd->state != COMP_STATE_READY) {
		tr_err(&ipc_tr, "ipc_comp_free(): comp id: 0x%x state is %d cannot be freed",
		       comp_id, icd->cd->state);
		return -EINVAL;
	}

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
	free_performance_data(icd->cd->perf_data.perf_data_item);
#endif

	if (!icd->cd->bsource_list.next || !icd->cd->bsink_list.next) {
		/* Unfortunate: the buffer list node gets initialized
		 * at the component level and thus can contain NULLs
		 * (which is an invalid list!) if the component's
		 * lifecycle hasn't reached that point.  There's no
		 * single place to ensure a valid/empty list, so we
		 * have to do it here and eat the resulting memory
		 * leak on error.  Bug-free host drivers won't do
		 * this, this was found via fuzzing.
		 */
		tr_err(&ipc_tr, "ipc_comp_free(): uninitialized buffer lists on comp 0x%x\n",
		       icd->id);
		return -EINVAL;
	}

	irq_local_disable(flags);
	comp_dev_for_each_producer_safe(icd->cd, buffer, safe) {
		comp_buffer_set_sink_component(buffer, NULL);
		/* This breaks the list, but we anyway delete all buffers */
		comp_buffer_reset_sink_list(buffer);
	}

	comp_dev_for_each_consumer_safe(icd->cd, buffer, safe) {
		comp_buffer_set_source_component(buffer, NULL);
		/* This breaks the list, but we anyway delete all buffers */
		comp_buffer_reset_source_list(buffer);
	}

	irq_local_enable(flags);

	/* free component and remove from list */
	comp_free(icd->cd);

	icd->cd = NULL;

	list_item_del(&icd->list);
	rfree(icd);

	return 0;
}
