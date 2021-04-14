// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/msg.h>
#include <sof/list.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * This flag disables firmware-side xrun recovery.
 * It should remain enabled in the situation when the
 * recovery is delegated to the outside of firmware.
 */
#define NO_XRUN_RECOVERY 1

static int pipeline_comp_xrun(struct comp_dev *current,
			      struct comp_buffer *calling_buf,
			      struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;

	if (dev_comp_type(current) == SOF_COMP_HOST) {
		/* get host timestamps */
		platform_host_timestamp(current, ppl_data->posn);

		/* send XRUN to host */
		mailbox_stream_write(ppl_data->p->posn_offset, ppl_data->posn,
				     sizeof(*ppl_data->posn));
		ipc_msg_send(ppl_data->p->msg, ppl_data->posn, true);
	}

	return pipeline_for_each_comp(current, ctx, dir);
}

#if NO_XRUN_RECOVERY
/* recover the pipeline from a XRUN condition */
int pipeline_xrun_recover(struct pipeline *p)
{
	return -EINVAL;
}

#else
/* recover the pipeline from a XRUN condition */
int pipeline_xrun_recover(struct pipeline *p)
{
	int ret;

	pipe_err(p, "pipeline_xrun_recover()");

	/* prepare the pipeline */
	ret = pipeline_prepare(p, p->source_comp);
	if (ret < 0) {
		pipe_err(p, "pipeline_xrun_recover(): pipeline_prepare() failed, ret = %d",
			 ret);
		return ret;
	}

	/* reset xrun status as we already in prepared */
	p->xrun_bytes = 0;

	/* restart pipeline comps */
	ret = pipeline_trigger(p, p->source_comp, COMP_TRIGGER_START);
	if (ret < 0) {
		pipe_err(p, "pipeline_xrun_recover(): pipeline_trigger() failed, ret = %d",
			 ret);
		return ret;
	}

	return 0;
}
#endif

int pipeline_xrun_set_limit(struct pipeline *p, uint32_t xrun_limit_usecs)
{
	/* TODO: these could be validated against min/max permissible values */
	p->xrun_limit_usecs = xrun_limit_usecs;
	return 0;
}

/*
 * trigger handler for pipelines in xrun, used for recovery from host only.
 * return values:
 *	0 -- success, further trigger in caller needed.
 *	PPL_STATUS_PATH_STOP -- done, no more further trigger needed.
 *	minus -- failed, caller should return failure.
 */
int pipeline_xrun_handle_trigger(struct pipeline *p, int cmd)
{
	int ret = 0;

	/* it is expected in paused status for xrun pipeline */
	if (!p->xrun_bytes || p->status != COMP_STATE_PAUSED)
		return 0;

	/* in xrun, handle start/stop trigger */
	switch (cmd) {
	case COMP_TRIGGER_START:
		/* in xrun, prepare before trigger start needed */
		pipe_info(p, "in xrun, prepare it first");
		/* prepare the pipeline */
		ret = pipeline_prepare(p, p->source_comp);
		if (ret < 0) {
			pipe_err(p, "prepare: ret = %d", ret);
			return ret;
		}
		/* now ready for start, clear xrun_bytes */
		p->xrun_bytes = 0;
		break;
	case COMP_TRIGGER_STOP:
		/* in xrun, suppose pipeline is already stopped, ignore it */
		pipe_info(p, "already stopped in xrun");
		/* no more further trigger stop needed */
		ret = PPL_STATUS_PATH_STOP;
		break;
	}

	return ret;
}

/* Send an XRUN to each host for this component. */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev,
		   int32_t bytes)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_xrun,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	struct sof_ipc_stream_posn posn;
	int ret;

	/* don't flood host */
	if (p->xrun_bytes)
		return;

	/* only send when we are running */
	if (dev->state != COMP_STATE_ACTIVE)
		return;

	/* notify all pipeline comps we are in XRUN, and stop copying */
	ret = pipeline_trigger(p, p->source_comp, COMP_TRIGGER_XRUN);
	if (ret < 0)
		pipe_err(p, "pipeline_xrun(): Pipelines notification about XRUN failed, ret = %d",
			 ret);

	memset(&posn, 0, sizeof(posn));
	ipc_build_stream_posn(&posn, SOF_IPC_STREAM_TRIG_XRUN,
			      dev_comp_id(dev));
	p->xrun_bytes = bytes;
	posn.xrun_size = bytes;
	posn.xrun_comp_id = dev_comp_id(dev);
	data.posn = &posn;
	data.p = p;

	walk_ctx.comp_func(dev, NULL, &walk_ctx, dev->direction);
}
