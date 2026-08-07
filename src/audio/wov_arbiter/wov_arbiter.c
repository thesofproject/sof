// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// WOV Arbiter — routes the drain output of one-of-N KPB host sinks to the
// single host PCM copier, while silencing the idle inputs and coordinating
// pause/resume of sibling WOV detectors via AMS.
//
// Topology connectivity (per KPB slot i):
//   KPB_i host_sink (output pin 1) ──► wov_arbiter input pin i
//   wov_arbiter output pin 0       ──► host copier
//
// AMS:
//   Subscribes to AMS_WOV_DETECT_MSG_UUID  (detector → arbiter on keyword)
//   Publishes   AMS_WOV_CTRL_MSG_UUID      (arbiter → detectors: pause/resume)

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/wov_arbiter.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#if CONFIG_IPC_MAJOR_4
#include <ipc4/base-config.h>
#endif
#if CONFIG_AMS
#include <sof/lib/ams.h>
#include <sof/lib/ams_msg.h>
#include <ipc4/ams_helpers.h>
#endif
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(wov_arbiter, LOG_LEVEL_INF);

SOF_DEFINE_REG_UUID(wov_arbiter);
DECLARE_TR_CTX(wov_arbiter_tr, SOF_UUID(wov_arbiter_uuid), LOG_LEVEL_INFO);

/* Private runtime data. */
struct wov_arb_data {
#if CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
#endif
	/*
	 * Index of the currently active KPB slot (0..WOV_ARB_MAX_SLOTS-1).
	 * WOV_ARB_NO_ACTIVE when no drain is in progress.
	 * Protected by the scheduler; no extra lock needed.
	 */
	uint8_t active_slot;

#if CONFIG_AMS
	uint32_t detect_uuid_id; /* AMS id for AMS_WOV_DETECT_MSG_UUID */
	uint32_t ctrl_uuid_id;   /* AMS id for AMS_WOV_CTRL_MSG_UUID   */
#endif
};

/* -------------------------------------------------------------------------
 * AMS helpers
 * ---------------------------------------------------------------------- */

#if CONFIG_AMS

static const ams_uuid_t ams_wov_detect_uuid = AMS_WOV_DETECT_MSG_UUID;
static const ams_uuid_t ams_wov_ctrl_uuid   = AMS_WOV_CTRL_MSG_UUID;

/* Send a WOV_CTRL AMS message to all registered detectors. */
static void arb_send_ctrl(const struct comp_dev *dev, uint8_t cmd,
			   uint8_t active_slot)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);
	struct ams_message_payload payload;
	struct wov_ctrl_payload ctrl = { .cmd = cmd, .active_slot = active_slot };

	if (cd->ctrl_uuid_id == AMS_INVALID_MSG_TYPE)
		return;

	ams_helper_prepare_payload(dev, &payload, cd->ctrl_uuid_id,
				   (uint8_t *)&ctrl, sizeof(ctrl));
	if (ams_send(&payload))
		comp_warn(dev, "wov_arb: ctrl AMS send failed");
}

/* AMS callback: a WOV detector has fired. */
static void arb_on_detect(const struct ams_message_payload *const p, void *ctx)
{
	struct comp_dev *dev = ctx;
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	if (p->message_length < sizeof(struct wov_detect_payload))
		return;

	const struct wov_detect_payload *det =
		(const struct wov_detect_payload *)p->message;

	if (det->slot_id >= WOV_ARB_MAX_SLOTS) {
		comp_err(dev, "wov_arb: bad slot_id %u", det->slot_id);
		return;
	}

	/* First-wins: ignore if another slot is already draining. */
	if (cd->active_slot != WOV_ARB_NO_ACTIVE) {
		comp_warn(dev, "wov_arb: slot %u detected but slot %u active, ignoring",
			  det->slot_id, cd->active_slot);
		return;
	}

	comp_info(dev, "wov_arb: activating slot %u", det->slot_id);
	cd->active_slot = det->slot_id;

	/* Tell all detectors to pause; each checks active_slot to decide. */
	arb_send_ctrl(dev, WOV_CTRL_CMD_PAUSE, det->slot_id);
}

#endif /* CONFIG_AMS */

/* -------------------------------------------------------------------------
 * Component lifecycle
 * ---------------------------------------------------------------------- */

static struct comp_dev *wov_arb_new(const struct comp_driver *drv,
				    const struct comp_ipc_config *config,
				    const void *spec)
{
	struct comp_dev *dev;
	struct wov_arb_data *cd;

	comp_cl_info(&drv->tctx, "wov_arb_new");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd) {
		comp_free_device(dev);
		return NULL;
	}

#if CONFIG_IPC_MAJOR_4
	const struct ipc4_base_module_cfg *base_cfg = spec;
	memcpy_s(&cd->base_cfg, sizeof(cd->base_cfg), base_cfg, sizeof(*base_cfg));
#endif

	cd->active_slot = WOV_ARB_NO_ACTIVE;
#if CONFIG_AMS
	cd->detect_uuid_id = AMS_INVALID_MSG_TYPE;
	cd->ctrl_uuid_id   = AMS_INVALID_MSG_TYPE;
#endif

	comp_set_drvdata(dev, cd);
	dev->direction = SOF_IPC_STREAM_CAPTURE;
	dev->direction_set = true;
	dev->state = COMP_STATE_READY;

	return dev;
}

static void wov_arb_free(struct comp_dev *dev)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "wov_arb_free");

#if CONFIG_AMS
	if (cd->detect_uuid_id != AMS_INVALID_MSG_TYPE)
		ams_helper_unregister_consumer(dev, cd->detect_uuid_id,
					       arb_on_detect);
	if (cd->ctrl_uuid_id != AMS_INVALID_MSG_TYPE)
		ams_helper_unregister_producer(dev, cd->ctrl_uuid_id);
#endif

	rfree(cd);
	comp_free_device(dev);
}

static int wov_arb_prepare(struct comp_dev *dev)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);
	int ret;

	comp_info(dev, "wov_arb_prepare");

	cd->active_slot = WOV_ARB_NO_ACTIVE;

#if CONFIG_AMS
	/* Register as consumer of WOV_DETECT messages. */
	ret = ams_helper_register_consumer(dev, &cd->detect_uuid_id,
					   (const uint8_t *)&ams_wov_detect_uuid,
					   arb_on_detect);
	if (ret) {
		comp_err(dev, "wov_arb: detect consumer register failed %d", ret);
		return ret;
	}

	/* Register as producer of WOV_CTRL messages. */
	ret = ams_helper_register_producer(dev, &cd->ctrl_uuid_id,
					   (const uint8_t *)&ams_wov_ctrl_uuid);
	if (ret) {
		comp_err(dev, "wov_arb: ctrl producer register failed %d", ret);
		ams_helper_unregister_consumer(dev, cd->detect_uuid_id,
					       arb_on_detect);
		cd->detect_uuid_id = AMS_INVALID_MSG_TYPE;
		return ret;
	}
	/* Broadcast RESUME so all WOV detector slots start unpaused */
	arb_send_ctrl(dev, WOV_CTRL_CMD_RESUME, WOV_ARB_NO_ACTIVE);
#endif

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static int wov_arb_reset(struct comp_dev *dev)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "wov_arb_reset");

	cd->active_slot = WOV_ARB_NO_ACTIVE;

#if CONFIG_AMS
	if (cd->detect_uuid_id != AMS_INVALID_MSG_TYPE) {
		ams_helper_unregister_consumer(dev, cd->detect_uuid_id,
					       arb_on_detect);
		cd->detect_uuid_id = AMS_INVALID_MSG_TYPE;
	}
	if (cd->ctrl_uuid_id != AMS_INVALID_MSG_TYPE) {
		ams_helper_unregister_producer(dev, cd->ctrl_uuid_id);
		cd->ctrl_uuid_id = AMS_INVALID_MSG_TYPE;
	}
#endif

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int wov_arb_trigger(struct comp_dev *dev, int cmd)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);
	int ret;

	comp_info(dev, "wov_arb_trigger cmd %d", cmd);

	ret = comp_set_state(dev, cmd);
	if (ret)
		return ret;

	/*
	 * Host closed the PCM stream: deactivate and resume all detectors so
	 * they return to listening mode.
	 */
	if (cmd == COMP_TRIGGER_STOP || cmd == COMP_TRIGGER_PAUSE) {
		if (cd->active_slot != WOV_ARB_NO_ACTIVE) {
			comp_info(dev, "wov_arb: stream stopped, resuming all slots");
			cd->active_slot = WOV_ARB_NO_ACTIVE;
#if CONFIG_AMS
			arb_send_ctrl(dev, WOV_CTRL_CMD_RESUME, WOV_ARB_NO_ACTIVE);
#endif
		}
	}

	return 0;
}

static int wov_arb_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
#if CONFIG_IPC_MAJOR_4
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	memset(params, 0, sizeof(*params));
	params->channels = cd->base_cfg.audio_fmt.channels_count;
	params->rate     = cd->base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->base_cfg.audio_fmt.depth / 8;
	params->sample_valid_bytes =
		cd->base_cfg.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = cd->base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = cd->base_cfg.ibs;
#endif
	return comp_verify_params(dev, 0, params);
}

/* -------------------------------------------------------------------------
 * IPC4 large-config: allow host to force-select a slot (debug/test use).
 * ---------------------------------------------------------------------- */

#if CONFIG_IPC_MAJOR_4
static int wov_arb_set_large_config(struct comp_dev *dev,
				    uint32_t param_id,
				    bool first_block,
				    bool last_block,
				    uint32_t data_offset,
				    const char *data)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	if (param_id == IPC4_WOV_ARB_SET_ACTIVE_SLOT) {
		if (data_offset < sizeof(uint8_t))
			return -EINVAL;
		cd->active_slot = *(const uint8_t *)data;
		comp_info(dev, "wov_arb: force active_slot=%u", cd->active_slot);
		return 0;
	}

	return -EINVAL;
}

static int wov_arb_get_attribute(struct comp_dev *dev,
				 uint32_t type, void *value)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	if (type == COMP_ATTR_BASE_CONFIG) {
		*(struct ipc4_base_module_cfg *)value = cd->base_cfg;
		return 0;
	}
	return -EINVAL;
}
#endif /* CONFIG_IPC_MAJOR_4 */

/* -------------------------------------------------------------------------
 * copy() — main audio processing
 *
 * For the active input pin: forward frames to the output.
 * For all other input pins: consume and discard to prevent buffer stalls.
 *
 * Input buffers are ordered by connection order in bsource_list (sink_list
 * is the per-buffer link field).  Slot 0 = first connected source, etc.
 * ---------------------------------------------------------------------- */
static int wov_arb_copy(struct comp_dev *dev)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct list_item *src_item;
	uint32_t slot;
	uint32_t sink_free;
	uint32_t active_avail = 0;
	uint32_t copy_bytes;

	comp_dbg(dev, "wov_arb_copy active=%u", cd->active_slot);

	sink = comp_dev_get_first_data_consumer(dev);
	if (!sink)
		return 0;

	sink_free = audio_stream_get_free_bytes(&sink->stream);

	/* First pass: find how many bytes the active source has available. */
	slot = 0;
	list_for_item(src_item, &dev->bsource_list) {
		source = list_item(src_item, struct comp_buffer, sink_list);
		if (slot == cd->active_slot) {
			active_avail = audio_stream_get_avail_bytes(&source->stream);
			break;
		}
		if (++slot >= WOV_ARB_MAX_SLOTS)
			break;
	}

	copy_bytes = MIN(active_avail, sink_free);

	/* Second pass: copy active slot, silently drain idle slots. */
	slot = 0;
	list_for_item(src_item, &dev->bsource_list) {
		source = list_item(src_item, struct comp_buffer, sink_list);
		uint32_t avail = audio_stream_get_avail_bytes(&source->stream);

		if (slot == cd->active_slot && copy_bytes > 0) {
			uint32_t frame_bytes = audio_stream_frame_bytes(&source->stream);
			uint32_t aligned = (copy_bytes / frame_bytes) * frame_bytes;

			if (aligned > 0) {
				buffer_stream_invalidate(source, aligned);
				audio_stream_copy(&source->stream, 0,
						  &sink->stream, 0,
						  aligned / audio_stream_sample_bytes(&source->stream));
				comp_update_buffer_consume(source, aligned);
				buffer_stream_writeback(sink, aligned);
				comp_update_buffer_produce(sink, aligned);
			}
		} else if (avail > 0) {
			comp_update_buffer_consume(source, avail);
		}

		if (++slot >= WOV_ARB_MAX_SLOTS)
			break;
	}

	if (cd->active_slot == WOV_ARB_NO_ACTIVE && sink_free > 0) {
		uint32_t frame_bytes = audio_stream_frame_bytes(&sink->stream);
		uint32_t fill_bytes = MIN(sink_free, 160 * frame_bytes);

		if (fill_bytes > 0) {
			void *wptr = audio_stream_get_wptr(&sink->stream);
			uint32_t bytes_to_end = audio_stream_bytes_without_wrap(&sink->stream, wptr);

			if (fill_bytes <= bytes_to_end) {
				memset(wptr, 0, fill_bytes);
			} else {
				memset(wptr, 0, bytes_to_end);
				memset(audio_stream_get_addr(&sink->stream), 0, fill_bytes - bytes_to_end);
			}
			buffer_stream_writeback(sink, fill_bytes);
			comp_update_buffer_produce(sink, fill_bytes);
		}
	}

	return 0;
}

/* -------------------------------------------------------------------------
 * Component driver registration
 * ---------------------------------------------------------------------- */

static const struct comp_driver wov_arbiter_drv = {
	.type  = SOF_COMP_KEYWORD_DETECT,
	.uid   = SOF_RT_UUID(wov_arbiter_uuid),
	.tctx  = &wov_arbiter_tr,
	.ops   = {
		.create            = wov_arb_new,
		.free              = wov_arb_free,
		.params            = wov_arb_params,
		.trigger           = wov_arb_trigger,
		.copy              = wov_arb_copy,
		.prepare           = wov_arb_prepare,
		.reset             = wov_arb_reset,
#if CONFIG_IPC_MAJOR_4
		.set_large_config  = wov_arb_set_large_config,
		.get_attribute     = wov_arb_get_attribute,
#endif
	},
};

static SHARED_DATA struct comp_driver_info wov_arbiter_info = {
	.drv = &wov_arbiter_drv,
};

UT_STATIC void sys_comp_wov_arbiter_init(void)
{
	comp_register(&wov_arbiter_info);
}

DECLARE_MODULE(sys_comp_wov_arbiter_init);
SOF_MODULE_INIT(wov_arbiter, sys_comp_wov_arbiter_init);
