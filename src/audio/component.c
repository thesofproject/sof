// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component_ext.h>
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

/* 7c42ce8b-0108-43d0-9137-56d660478c5f */
DECLARE_SOF_UUID("component", comp_uuid, 0x7c42ce8b, 0x0108, 0x43d0,
		 0x91, 0x37, 0x56, 0xd6, 0x60, 0x47, 0x8c, 0x5f);

DECLARE_TR_CTX(comp_tr, SOF_UUID(comp_uuid), LOG_LEVEL_INFO);

static const struct comp_driver *get_drv(struct sof_ipc_comp *comp)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;
	const struct comp_driver *drv = NULL;
	struct comp_driver_info *info;
	struct sof_ipc_comp_ext *comp_ext;
	uint32_t flags;

	irq_local_disable(flags);

	/* do we have extended data ? */
	if (!comp->ext_data_length)
		goto comp_type_match;

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
	list_for_item(clist, &drivers->list) {
		info = container_of(clist, struct comp_driver_info,
				    list);
		if (!memcmp(info->drv->uid, comp_ext->uuid,
			    UUID_SIZE)) {
			tr_dbg(&comp_tr,
			       "get_drv_from_uuid(), found driver type %d, uuid %s",
			       info->drv->type,
			       info->drv->tctx->uuid_p);
			drv = info->drv;
			goto out;
		}
	}

	tr_err(&comp_tr, "get_drv(): the provided UUID (%8x%8x%8x%8x) doesn't match to any driver!",
	       *(uint32_t *)(&comp_ext->uuid[0]),
	       *(uint32_t *)(&comp_ext->uuid[4]),
	       *(uint32_t *)(&comp_ext->uuid[8]),
	       *(uint32_t *)(&comp_ext->uuid[12]));

	goto out;

comp_type_match:
	/* search driver list for driver type */
	list_for_item(clist, &drivers->list) {
		info = container_of(clist, struct comp_driver_info, list);
		if (info->drv->type == comp->type) {
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
	drv = get_drv(comp);
	if (!drv) {
		tr_err(&comp_tr, "comp_new(): driver not found, comp->type = %u",
		       comp->type);
		return NULL;
	}

	/* validate size of ipc config */
	if (IPC_IS_SIZE_INVALID(*comp_config(comp))) {
		IPC_SIZE_ERROR_TRACE(&comp_tr, *comp_config(comp));
		return NULL;
	}

	tr_info(&comp_tr, "comp new %s type %d id %d.%d",
		drv->tctx->uuid_p, comp->type, comp->pipeline_id, comp->id);

	/* create the new component */
	cdev = drv->ops.create(drv, comp);
	if (!cdev) {
		comp_cl_err(drv, "comp_new(): unable to create the new component");
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
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_START",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_RELEASE:
		if (dev->state == COMP_STATE_PAUSED) {
			dev->state = COMP_STATE_ACTIVE;
		} else {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_RELEASE",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_STOP:
		if (dev->state == COMP_STATE_ACTIVE ||
		    dev->state == COMP_STATE_PAUSED) {
			dev->state = COMP_STATE_PREPARE;
		} else {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_STOP",
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
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_PAUSE",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_RESET:
		/* reset always succeeds */
		if (dev->state == COMP_STATE_ACTIVE ||
		    dev->state == COMP_STATE_PAUSED) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_RESET",
				 dev->state);
			ret = 0;
		}
		dev->state = COMP_STATE_READY;
		break;
	case COMP_TRIGGER_PREPARE:
		if (dev->state == COMP_STATE_READY) {
			dev->state = COMP_STATE_PREPARE;
		} else {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_PREPARE",
				 dev->state);
			ret = -EINVAL;
		}
		break;
	default:
		break;
	}

	comp_writeback(dev);

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
		comp_err(dev, "comp_verify_params(): !params");
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

void comp_free_model_data(struct comp_dev *dev, struct comp_model_data *model)
{
	if (!model->data)
		return;

	rfree(model->data);
	model->data = NULL;
	model->data_size = 0;
	model->crc = 0;
	model->data_pos = 0;
}

int  comp_alloc_model_data(struct comp_dev *dev, struct comp_model_data *model,
			   uint32_t size)
{
	comp_free_model_data(dev, model);

	if (!size)
		return 0;

	model->data = rballoc(0, SOF_MEM_CAPS_RAM, size);

	if (!model->data) {
		comp_err(dev, "comp_alloc_model_data(): model->data rballoc failed");
		return -ENOMEM;
	}

	bzero(model->data, size);
	model->data_size = size;
	model->data_pos = 0;
	model->crc = 0;

	return 0;
}

int comp_set_model(struct comp_dev *dev, struct comp_model_data *model,
		   struct sof_ipc_ctrl_data *cdata)
{
	bool done = false;
	int ret = 0;

	comp_dbg(dev, "comp_set_model() msg_index = %d, num_elems = %d, remaining = %d ",
		 cdata->msg_index, cdata->num_elems,
		 cdata->elems_remaining);

	/* in case when the current package is the first, we should allocate
	 * memory for whole model data
	 */
	if (!cdata->msg_index) {
		ret = comp_alloc_model_data(dev, model, cdata->data->size);
		/* in case when required model size is equal to zero we do not
		 * allocate memory and should just return 0
		 */
		if (ret < 0 || !cdata->data->size)
			return ret;
	}

	/* return an error in case when we do not have allocated memory for
	 * model data
	 */
	if (!model->data) {
		comp_err(dev, "comp_set_model(): buffer not allocated");
		return -ENOMEM;
	}

	if (!cdata->elems_remaining) {
		/* when we receive the last package and do not fill the whole
		 * allocated buffer, we return an error
		 */
		if (cdata->num_elems + model->data_pos < model->data_size) {
			comp_err(dev, "comp_set_model(): not enough data to fill the buffer");
			// TODO: handle this situation

			return -EINVAL;
		}

		/* the whole data were received properly */
		done = true;
		comp_dbg(dev, "comp_set_model(): final package received");
	}

	/* return an error in case when received data exceed allocated
	 * memory
	 */
	if (cdata->num_elems > model->data_size - model->data_pos) {
		comp_err(dev, "comp_set_model(): too much data");
		return -EINVAL;
	}

	ret = memcpy_s((char *)model->data + model->data_pos,
		       model->data_size - model->data_pos,
		       cdata->data->data, cdata->num_elems);
	assert(!ret);

	/* update data_pos variable with received number of elements (num_elem)
	 */
	model->data_pos += cdata->num_elems;

	/* Update crc value when done */
	if (done) {
		model->crc = crc32(0, model->data, model->data_size);
		comp_dbg(dev, "comp_set_model() done, memory_size = 0x%x, crc = 0x%08x",
			 model->data_size, model->crc);
	}

	return 0;
}

int comp_get_model(struct comp_dev *dev, struct comp_model_data *model,
		   struct sof_ipc_ctrl_data *cdata, int size)
{
	size_t bs;
	int ret = 0;

	comp_dbg(dev, "comp_get_model() msg_index = %d, num_elems = %d, remaining = %d ",
		 cdata->msg_index, cdata->num_elems,
		 cdata->elems_remaining);

	/* Copy back to user space */
	if (model->data) {
		/* reset data_pos variable in case of copying first element */
		if (!cdata->msg_index) {
			model->data_pos = 0;
			comp_dbg(dev, "comp_get_model() model data_size = 0x%x",
				 model->data_size);
		}

		bs = cdata->num_elems;

		/* return an error in case of mismatch between num_elems and
		 * required size
		 */
		if (bs > size) {
			comp_err(dev, "comp_get_model(): invalid size %d", bs);
			return -EINVAL;
		}

		/* copy required size of data */
		ret = memcpy_s(cdata->data->data, size,
			       (char *)model->data + model->data_pos,
			       bs);
		assert(!ret);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = model->data_size;
		model->data_pos += bs;

	} else {
		comp_warn(dev, "comp_get_model(): model->data not allocated yet.");
		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = 0;
	}

	return ret;
}

struct comp_dev *comp_make_shared(struct comp_dev *dev)
{
	struct list_item *old_bsource_list = &dev->bsource_list;
	struct list_item *old_bsink_list = &dev->bsink_list;

	/* flush cache to share */
	dcache_writeback_region(dev, dev->size);

	dev = platform_shared_get(dev, dev->size);

	/* re-link lists with the new heads addresses, init would cut
	 * links to existing items, local already connected buffers
	 */
	list_relink(&dev->bsource_list, old_bsource_list);
	list_relink(&dev->bsink_list, old_bsink_list);
	dev->is_shared = true;

	platform_shared_commit(dev, sizeof(*dev));

	return dev;
}
