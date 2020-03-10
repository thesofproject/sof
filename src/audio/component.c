// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <sof/string.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static SHARED_DATA struct comp_driver_list cd;

static const struct comp_driver *get_drv(uint32_t type)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;
	const struct comp_driver *drv = NULL;
	struct comp_driver_info *info;
	uint32_t flags;

	irq_local_disable(flags);

	/* search driver list for driver type */
	list_for_item(clist, &drivers->list) {
		info = container_of(clist, struct comp_driver_info, list);
		if (info->drv->type == type) {
			drv = info->drv;
			platform_shared_commit(info, sizeof(*info));
			goto out;
		}

		platform_shared_commit(info, sizeof(*info));
	}

out:
	platform_shared_commit(drivers, sizeof(*drivers));
	irq_local_enable(flags);
	return drv;
}

struct comp_dev *comp_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *cdev;
	const struct comp_driver *drv;

	/* find the driver for our new component */
	drv = get_drv(comp->type);
	if (!drv) {
		trace_error(TRACE_CLASS_COMP, "comp_new() error: driver not found, comp->type = %u",
			    comp->type);
		return NULL;
	}

	/* validate size of ipc config */
	if (IPC_IS_SIZE_INVALID(*comp_config(comp))) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_COMP, *comp_config(comp));
		return NULL;
	}

	trace_event(TRACE_CLASS_COMP, "comp new %s type %d pipe_id %d id %d",
		    drv->uid, comp->type, comp->pipeline_id, comp->id);

	/* create the new component */
	cdev = drv->ops.new(drv, comp);
	if (!cdev) {
		comp_cl_err(drv, "comp_new() error: unable to create the new component");
		return NULL;
	}

	list_init(&cdev->bsource_list);
	list_init(&cdev->bsink_list);

	return cdev;
}

int comp_register(struct comp_driver_info *drv)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	uint32_t flags;

	irq_local_disable(flags);
	list_item_prepend(&drv->list, &drivers->list);
	platform_shared_commit(drv, sizeof(*drv));
	platform_shared_commit(drivers, sizeof(*drivers));
	irq_local_enable(flags);

	return 0;
}

void comp_unregister(struct comp_driver_info *drv)
{
	uint32_t flags;

	irq_local_disable(flags);
	list_item_del(&drv->list);
	platform_shared_commit(drv, sizeof(*drv));
	irq_local_enable(flags);
}

/* NOTE: Keep the component state diagram up to date:
 * sof-docs/developer_guides/firmware/components/images/comp-dev-states.pu
 */

int comp_set_state(struct comp_dev *dev, int cmd)
{
	int requested_state = comp_get_requested_state(cmd);
	int ret = 0;

	if (dev->state == requested_state) {
		comp_info(dev, "comp_set_state(), state already set to %u",
			  dev->state);
		return COMP_STATUS_STATE_ALREADY_SET;
	}

	switch (cmd) {
	case COMP_TRIGGER_START:
		if (dev->state == COMP_STATE_PREPARE) {
			dev->state = COMP_STATE_ACTIVE;
		} else {
			comp_err(dev, "comp_set_state() error: wrong state = %u, COMP_TRIGGER_START",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_RELEASE:
		if (dev->state == COMP_STATE_PAUSED) {
			dev->state = COMP_STATE_ACTIVE;
		} else {
			comp_err(dev, "comp_set_state() error: wrong state = %u, COMP_TRIGGER_RELEASE",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_STOP:
		if (dev->state == COMP_STATE_ACTIVE ||
		    dev->state == COMP_STATE_PAUSED) {
			dev->state = COMP_STATE_PREPARE;
		} else {
			comp_err(dev, "comp_set_state() error: wrong state = %u, COMP_TRIGGER_STOP",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_XRUN:
		/* reset component status to ready at xrun */
		dev->state = COMP_STATE_READY;
		break;
	case COMP_TRIGGER_PAUSE:
		/* only support pausing for running */
		if (dev->state == COMP_STATE_ACTIVE) {
			dev->state = COMP_STATE_PAUSED;
		} else {
			comp_err(dev, "comp_set_state() error: wrong state = %u, COMP_TRIGGER_PAUSE",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_RESET:
		/* reset always succeeds */
		if (dev->state == COMP_STATE_ACTIVE ||
		    dev->state == COMP_STATE_PAUSED) {
			comp_err(dev, "comp_set_state() error: wrong state = %u, COMP_TRIGGER_RESET",
				 dev->state);
			ret = 0;
		}
		dev->state = COMP_STATE_READY;
		break;
	case COMP_TRIGGER_PREPARE:
		if (dev->state == COMP_STATE_READY) {
			dev->state = COMP_STATE_PREPARE;
		} else {
			comp_err(dev, "comp_set_state() error: wrong state = %u, COMP_TRIGGER_PREPARE",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	default:
		break;
	}

	return ret;
}

void sys_comp_init(struct sof *sof)
{
	sof->comp_drivers = platform_shared_get(&cd, sizeof(cd));

	list_init(&sof->comp_drivers->list);

	platform_shared_commit(sof->comp_drivers, sizeof(*sof->comp_drivers));
}

void comp_get_copy_limits(struct comp_buffer *source, struct comp_buffer *sink,
			  struct comp_copy_limits *cl)
{
	cl->frames = audio_stream_avail_frames(&source->stream, &sink->stream);
	cl->source_frame_bytes = audio_stream_frame_bytes(&source->stream);
	cl->sink_frame_bytes = audio_stream_frame_bytes(&sink->stream);
	cl->source_bytes = cl->frames * cl->source_frame_bytes;
	cl->sink_bytes = cl->frames * cl->sink_frame_bytes;
}

/* Function overwrites PCM parameters (frame_fmt, buffer_fmt, channels, rate)
 * with buffer parameters when specific flag is set.
 */
static void comp_update_params(uint32_t flag,
			       struct sof_ipc_stream_params *params,
			       struct comp_buffer *buffer)
{
	if (flag & BUFF_PARAMS_FRAME_FMT)
		params->frame_fmt = buffer->stream.frame_fmt;

	if (flag & BUFF_PARAMS_BUFFER_FMT)
		params->buffer_fmt = buffer->buffer_fmt;

	if (flag & BUFF_PARAMS_CHANNELS)
		params->channels = buffer->stream.channels;

	if (flag & BUFF_PARAMS_RATE)
		params->rate = buffer->stream.rate;
}

int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params)
{
	struct list_item *buffer_list;
	struct list_item *source_list;
	struct list_item *sink_list;
	struct list_item *clist;
	struct list_item *curr;
	struct comp_buffer *sinkb;
	struct comp_buffer *buf;
	int dir = dev->direction;
	uint32_t flags = 0;

	if (!params) {
		comp_err(dev, "comp_verify_params() error: !params");
		return -EINVAL;
	}

	source_list = comp_buffer_list(dev, PPL_DIR_UPSTREAM);
	sink_list = comp_buffer_list(dev, PPL_DIR_DOWNSTREAM);

	/* searching for endpoint component e.g. HOST, DETECT_TEST, which
	 * has only one sink or one source buffer.
	 */
	if (list_is_empty(source_list) != list_is_empty(sink_list)) {
		if (!list_is_empty(source_list))
			buf = list_first_item(&dev->bsource_list,
					      struct comp_buffer,
					      sink_list);
		else
			buf = list_first_item(&dev->bsink_list,
					      struct comp_buffer,
					      source_list);

		buffer_lock(buf, &flags);

		/* update specific pcm parameter with buffer parameter if
		 * specific flag is set.
		 */
		comp_update_params(flag, params, buf);

		/* overwrite buffer parameters with modified pcm
		 * parameters
		 */
		buffer_set_params(buf, params, BUFFER_UPDATE_FORCE);

		/* set component period frames */
		component_set_period_frames(dev, buf->stream.rate);

		buffer_unlock(buf, flags);
	} else {
		/* for other components we iterate over all downstream buffers
		 * (for playback) or upstream buffers (for capture).
		 */
		buffer_list = comp_buffer_list(dev, dir);
		clist = buffer_list->next;

		while (clist != buffer_list) {
			curr = clist;

			buf = buffer_from_list(curr, struct comp_buffer, dir);

			buffer_lock(buf, &flags);

			clist = clist->next;

			comp_update_params(flag, params, buf);

			buffer_set_params(buf, params, BUFFER_UPDATE_FORCE);

			buffer_unlock(buf, flags);
		}

		/* fetch sink buffer in order to calculate period frames */
		sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
					source_list);

		buffer_lock(sinkb, &flags);

		component_set_period_frames(dev, sinkb->stream.rate);

		buffer_unlock(sinkb, flags);
	}

	return 0;
}

struct comp_dev *comp_make_shared(struct comp_dev *dev)
{
	struct comp_dev *old = dev;

	dev = rrealloc(dev, SOF_MEM_ZONE_RUNTIME, SOF_MEM_FLAG_SHARED,
		       SOF_MEM_CAPS_RAM, dev->size);
	if (!dev) {
		trace_error(TRACE_CLASS_COMP, "comp_make_shared() error: unable to realloc component");
		return NULL;
	}

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);
	dev->is_shared = true;

	platform_shared_commit(dev, sizeof(*dev));

	/* clear cache to avoid later random flushes */
	dcache_invalidate_region(old, sizeof(*old));

	return dev;
}
