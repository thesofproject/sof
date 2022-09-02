// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component_ext.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <rtos/interrupt.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <rtos/string.h>
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

	return 0;
}

void sys_comp_init(struct sof *sof)
{
	sof->comp_drivers = platform_shared_get(&cd, sizeof(cd));

	list_init(&sof->comp_drivers->list);
	k_spinlock_init(&sof->comp_drivers->lock);
}

void comp_get_copy_limits(struct comp_buffer __sparse_cache *source,
			  struct comp_buffer __sparse_cache *sink,
			  struct comp_copy_limits *cl)
{
	cl->frames = audio_stream_avail_frames(&source->stream, &sink->stream);
	cl->source_frame_bytes = audio_stream_frame_bytes(&source->stream);
	cl->sink_frame_bytes = audio_stream_frame_bytes(&sink->stream);
	cl->source_bytes = cl->frames * cl->source_frame_bytes;
	cl->sink_bytes = cl->frames * cl->sink_frame_bytes;
}

void comp_get_copy_limits_frame_aligned(const struct comp_buffer __sparse_cache *source,
					const struct comp_buffer __sparse_cache *sink,
					struct comp_copy_limits *cl)
{
	cl->frames = audio_stream_avail_frames_aligned(&source->stream, &sink->stream);
	cl->source_frame_bytes = audio_stream_frame_bytes(&source->stream);
	cl->sink_frame_bytes = audio_stream_frame_bytes(&sink->stream);
	cl->source_bytes = cl->frames * cl->source_frame_bytes;
	cl->sink_bytes = cl->frames * cl->sink_frame_bytes;
}

int audio_stream_copy(const struct audio_stream __sparse_cache *source, uint32_t ioffset,
		      struct audio_stream __sparse_cache *sink, uint32_t ooffset, uint32_t samples)
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

void audio_stream_copy_from_linear(const void *linear_source, int ioffset,
				   struct audio_stream __sparse_cache *sink, int ooffset,
				   unsigned int samples)
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

void audio_stream_copy_to_linear(const struct audio_stream __sparse_cache *source, int ioffset,
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
