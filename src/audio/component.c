// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component_ext.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/msg.h>
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

LOG_MODULE_REGISTER(component, CONFIG_SOF_LOG_LEVEL);

static SHARED_DATA struct comp_driver_list cd;

/* 7c42ce8b-0108-43d0-9137-56d660478c5f */
DECLARE_SOF_UUID("component", comp_uuid, 0x7c42ce8b, 0x0108, 0x43d0,
		 0x91, 0x37, 0x56, 0xd6, 0x60, 0x47, 0x8c, 0x5f);

DECLARE_TR_CTX(comp_tr, SOF_UUID(comp_uuid), LOG_LEVEL_INFO);

int comp_register(struct comp_driver_info *drv)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	k_spinlock_key_t key;

	key = k_spin_lock(&drivers->lock);
	list_item_prepend(&drv->list, &drivers->list);
	k_spin_unlock(&drivers->lock, key);

	return 0;
}

void comp_unregister(struct comp_driver_info *drv)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	k_spinlock_key_t key;

	key = k_spin_lock(&drivers->lock);
	list_item_del(&drv->list);
	k_spin_unlock(&drivers->lock, key);
}

/* NOTE: Keep the component state diagram up to date:
 * sof-docs/developer_guides/firmware/components/images/comp-dev-states.pu
 */

int comp_set_state(struct comp_dev *dev, int cmd)
{
	int requested_state = comp_get_requested_state(cmd);

	if (dev->state == requested_state) {
		comp_info(dev, "comp_set_state(), state already set to %u",
			  dev->state);
		return COMP_STATUS_STATE_ALREADY_SET;
	}

	switch (cmd) {
	case COMP_TRIGGER_START:
		if (dev->state != COMP_STATE_PRE_ACTIVE) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_START",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_RELEASE:
		if (dev->state != COMP_STATE_PRE_ACTIVE) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_RELEASE",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_STOP:
		if (dev->state != COMP_STATE_ACTIVE &&
		    dev->state != COMP_STATE_PAUSED) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_STOP",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_PAUSE:
		/* only support pausing for running */
		if (dev->state != COMP_STATE_ACTIVE) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_PAUSE",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_RESET:
		/* reset always succeeds */
		if (dev->state == COMP_STATE_ACTIVE)
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_RESET",
				 dev->state);
		else if (dev->state == COMP_STATE_PAUSED)
			comp_info(dev, "comp_set_state(): state = %u, COMP_TRIGGER_RESET",
				  dev->state);
		break;
	case COMP_TRIGGER_PREPARE:
		if (dev->state != COMP_STATE_READY) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_PREPARE",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_PRE_START:
		if (dev->state != COMP_STATE_PREPARE) {
			comp_err(dev,
				 "comp_set_state(): wrong state = %u, COMP_TRIGGER_PRE_START",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_PRE_RELEASE:
		if (dev->state != COMP_STATE_PAUSED) {
			comp_err(dev,
				 "comp_set_state(): wrong state = %u, COMP_TRIGGER_PRE_RELEASE",
				 dev->state);
			return -EINVAL;
		}
		break;
	default:
		return 0;
	}

	dev->state = requested_state;

	comp_writeback(dev);

	return 0;
}

void sys_comp_init(struct sof *sof)
{
	sof->comp_drivers = platform_shared_get(&cd, sizeof(cd));

	list_init(&sof->comp_drivers->list);
	k_spinlock_init(&sof->comp_drivers->lock);
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

struct comp_dev *comp_make_shared(struct comp_dev *dev)
{
	struct list_item *old_bsource_list = &dev->bsource_list;
	struct list_item *old_bsink_list = &dev->bsink_list;
	struct list_item *buffer_list, *clist;
	struct comp_buffer *buffer;
	int dir;

	/* flush cache to share */
	dcache_writeback_region((__sparse_force void __sparse_cache *)dev, dev->size);

	dev = platform_shared_get(dev, dev->size);

	/* re-link lists with the new heads addresses, init would cut
	 * links to existing items, local already connected buffers
	 */
	list_relink(&dev->bsource_list, old_bsource_list);
	list_relink(&dev->bsink_list, old_bsink_list);
	dev->is_shared = true;

	/* re-link all buffers which are already connected to this
	 * component
	 */
	for (dir = 0; dir <= PPL_CONN_DIR_BUFFER_TO_COMP; dir++) {
		buffer_list = comp_buffer_list(dev, dir);

		if (list_is_empty(buffer_list))
			continue;

		list_for_item(clist, buffer_list) {
			buffer = buffer_from_list(clist, struct comp_buffer, dir);

			buffer_set_comp(buffer, dev, dir);
		}
	}

	return dev;
}

int audio_stream_copy(const struct audio_stream *source, uint32_t ioffset,
		      struct audio_stream *sink, uint32_t ooffset, uint32_t samples)
{
	int ssize = audio_stream_sample_bytes(source); /* src fmt == sink fmt */
	uint8_t *src = audio_stream_wrap(source, (uint8_t *)source->r_ptr + ioffset * ssize);
	uint8_t *snk = audio_stream_wrap(sink, (uint8_t *)sink->w_ptr + ooffset * ssize);
	size_t bytes = samples * ssize;
	size_t bytes_src;
	size_t bytes_snk;
	size_t bytes_copied;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		bytes_copied = MIN(bytes_src, bytes_snk);
		bytes_copied = MIN(bytes, bytes_copied);
		memcpy(snk, src, bytes_copied);
		bytes -= bytes_copied;
		src = audio_stream_wrap(source, src + bytes_copied);
		snk = audio_stream_wrap(sink, snk + bytes_copied);
	}

	return samples;
}

void audio_stream_copy_from_linear(void *linear_source, int ioffset,
				   struct audio_stream *sink, int ooffset, unsigned int samples)
{
	int ssize = audio_stream_sample_bytes(sink); /* src fmt == sink fmt */
	uint8_t *src = (uint8_t *)linear_source + ioffset * ssize;
	uint8_t *snk = audio_stream_wrap(sink, (uint8_t *)sink->w_ptr + ooffset * ssize);
	size_t bytes = samples * ssize;
	size_t bytes_snk;
	size_t bytes_copied;

	while (bytes) {
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		bytes_copied = MIN(bytes, bytes_snk);
		memcpy(snk, src, bytes_copied);
		bytes -= bytes_copied;
		src += bytes_copied;
		snk = audio_stream_wrap(sink, snk + bytes_copied);
	}
}

void audio_stream_copy_to_linear(struct audio_stream *source, int ioffset,
				 void *linear_sink, int ooffset, unsigned int samples)
{
	int ssize = audio_stream_sample_bytes(source); /* src fmt == sink fmt */
	uint8_t *src = audio_stream_wrap(source, (uint8_t *)source->r_ptr + ioffset * ssize);
	uint8_t *snk = (uint8_t *)linear_sink + ooffset * ssize;
	size_t bytes = samples * ssize;
	size_t bytes_src;
	size_t bytes_copied;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		bytes_copied = MIN(bytes, bytes_src);
		memcpy(snk, src, bytes_copied);
		bytes -= bytes_copied;
		src = audio_stream_wrap(source, src + bytes_copied);
		snk += bytes_copied;
	}
}
