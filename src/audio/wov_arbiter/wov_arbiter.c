// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation
//
// WOV Arbiter -- routes the drain output of one-of-N KPB host sinks to the
// single host PCM copier, while silencing the idle inputs and coordinating
// pause/resume of sibling WOV detectors via the SOF notifier system.
//
// Topology connectivity (per KPB slot i):
//   KPB_i host_sink (output pin 1) --> wov_arbiter input pin i
//   wov_arbiter output pin 0       --> host copier
//
// Notifier events:
//   Subscribes to NOTIFIER_ID_WOV_DETECT  (detector -> arbiter on keyword)
//   Publishes   NOTIFIER_ID_WOV_CTRL      (arbiter -> detectors: pause/resume)

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/wov_arbiter.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/base-config.h>
#include <ipc4/header.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(wov_arbiter, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(wov_arbiter);
DECLARE_TR_CTX(wov_arbiter_tr, SOF_UUID(wov_arbiter_uuid), LOG_LEVEL_INFO);

/* Private runtime data. */
struct wov_arb_data {
	struct ipc4_base_module_cfg base_cfg;
	/*
	 * Index of the currently active KPB slot (0..num_slots-1).
	 * WOV_ARB_NO_ACTIVE when no drain is in progress.
	 * Protected by the scheduler; no extra lock needed.
	 */
	uint8_t active_slot;
	/* Number of input pins (= KPB slots); read from nb_input_pins at init. */
	uint8_t num_slots;
};

/* -------------------------------------------------------------------------
 * Notifier callbacks
 * ---------------------------------------------------------------------- */

/* Notifier callback: VAD gate entered sustained silence.  Resets the active
 * slot and resumes all detectors so they can re-arm for the next trigger. */
static void arb_on_vad_silence(void *arg, enum notify_id id, void *data)
{
	struct comp_dev *dev = arg;
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	if (cd->active_slot == WOV_ARB_NO_ACTIVE)
		return; /* no drain in progress, nothing to re-arm */

	comp_info(dev, "wov_arb: VAD silence, re-arming all slots (was active=%u)",
		  cd->active_slot);
	cd->active_slot = WOV_ARB_NO_ACTIVE;

	struct wov_ctrl_notif ctrl = { .cmd = WOV_ARB_CMD_RESUME, .slot_id = WOV_SLOT_INVALID };
	notifier_event(dev, NOTIFIER_ID_WOV_CTRL, NOTIFIER_TARGET_CORE_ALL_MASK,
		       &ctrl, sizeof(ctrl));
}

/* Notifier callback: a WOV detector has fired.  Activates the winning slot
 * and broadcasts PAUSE to all detectors via NOTIFIER_ID_WOV_CTRL. */
static void arb_on_detect(void *arg, enum notify_id id, void *data)
{
	struct comp_dev *dev = arg;
	struct wov_arb_data *cd = comp_get_drvdata(dev);
	const struct wov_detect_notif *det = data;

	if (det->slot_id >= cd->num_slots) {
		comp_err(dev, "wov_arb: bad slot_id %u num_slots=%u",
			 det->slot_id, cd->num_slots);
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

	/* Broadcast PAUSE to all detectors. The winning slot continues draining
	 * its KPB history buffer; all others suspend detection until RESUME. */
	struct wov_ctrl_notif ctrl = { .cmd = WOV_ARB_CMD_PAUSE, .slot_id = det->slot_id };
	notifier_event(dev, NOTIFIER_ID_WOV_CTRL, NOTIFIER_TARGET_CORE_ALL_MASK,
		       &ctrl, sizeof(ctrl));
}

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

	const struct ipc4_base_module_cfg *base_cfg = spec;
	memcpy_s(&cd->base_cfg, sizeof(cd->base_cfg), base_cfg, sizeof(*base_cfg));

	/* Read num_slots from nb_input_pins in the extended config when available.
	 * The old-API spec pointer carries no explicit size, so we validate the
	 * value and fall back to WOV_ARB_MAX_SLOTS when ext is absent/malformed. */
	const struct ipc4_base_module_cfg_ext *ext =
		(const struct ipc4_base_module_cfg_ext *)
		((const uint8_t *)spec + sizeof(*base_cfg));
	if (ext->nb_input_pins > 0 && ext->nb_input_pins <= WOV_ARB_MAX_SLOTS)
		cd->num_slots = ext->nb_input_pins;
	else
		cd->num_slots = WOV_ARB_MAX_SLOTS;

	/* Start with no active slot; first WOV_DETECT notifier will activate one. */
	cd->active_slot = WOV_ARB_NO_ACTIVE;

	comp_set_drvdata(dev, cd);
	/* Arbiter produces a capture stream that feeds the host PCM copier. */
	dev->direction = SOF_IPC_STREAM_CAPTURE;
	dev->direction_set = true;
	dev->state = COMP_STATE_READY;

	comp_info(dev, "wov_arb_new: num_slots=%u", cd->num_slots);

	return dev;
}

static void wov_arb_free(struct comp_dev *dev)
{
	comp_info(dev, "wov_arb_free");

	notifier_unregister(dev, NULL, NOTIFIER_ID_WOV_DETECT);
	notifier_unregister(dev, NULL, NOTIFIER_ID_VAD_SILENCE);

	struct wov_arb_data *cd = comp_get_drvdata(dev);
	rfree(cd);
	comp_free_device(dev);
}

static int wov_arb_prepare(struct comp_dev *dev)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "wov_arb_prepare");

	cd->active_slot = WOV_ARB_NO_ACTIVE;

	/* Subscribe to keyword-detected events from any WOV detector. */
	notifier_register(dev, NULL, NOTIFIER_ID_WOV_DETECT, arb_on_detect, 0);
	/* Subscribe to VAD gate silence events for no-reset re-arm. */
	notifier_register(dev, NULL, NOTIFIER_ID_VAD_SILENCE, arb_on_vad_silence, 0);

	/* Broadcast RESUME so all WOV detector slots start unpaused. */
	struct wov_ctrl_notif ctrl = { .cmd = WOV_ARB_CMD_RESUME };
	notifier_event(dev, NOTIFIER_ID_WOV_CTRL, NOTIFIER_TARGET_CORE_ALL_MASK,
		       &ctrl, sizeof(ctrl));

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static int wov_arb_reset(struct comp_dev *dev)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "wov_arb_reset");

	cd->active_slot = WOV_ARB_NO_ACTIVE;

	notifier_unregister(dev, NULL, NOTIFIER_ID_WOV_DETECT);
	notifier_unregister(dev, NULL, NOTIFIER_ID_VAD_SILENCE);

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
	 * Stream stopped or paused: deactivate the active slot and resume
	 * all detectors so they return to listening mode.
	 */
	if (cmd == COMP_TRIGGER_STOP || cmd == COMP_TRIGGER_PAUSE) {
		if (cd->active_slot != WOV_ARB_NO_ACTIVE) {
			comp_info(dev, "wov_arb: stream stopped, resuming all slots");
			cd->active_slot = WOV_ARB_NO_ACTIVE;
			struct wov_ctrl_notif c = { .cmd = WOV_ARB_CMD_RESUME };
			notifier_event(dev, NOTIFIER_ID_WOV_CTRL,
				       NOTIFIER_TARGET_CORE_ALL_MASK,
				       &c, sizeof(c));
		}
	}

	return 0;
}

static int wov_arb_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	/* Translate IPC4 base_cfg audio_fmt to IPC3-style stream params. */
	memset(params, 0, sizeof(*params));
	params->channels = cd->base_cfg.audio_fmt.channels_count;
	params->rate     = cd->base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->base_cfg.audio_fmt.depth / 8;
	params->sample_valid_bytes =
		cd->base_cfg.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = cd->base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = cd->base_cfg.ibs;

	return comp_verify_params(dev, 0, params);
}

/* -------------------------------------------------------------------------
 * IPC4 large-config: allow host to force-select a slot (debug/test use).
 * ---------------------------------------------------------------------- */

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

	/* Accept initial SET from the wov_trigger_id bytes kcontrol; ignore data. */
	if (param_id == IPC4_WOV_ARB_GET_ACTIVE_SLOT)
		return 0;

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

/* -------------------------------------------------------------------------
 * copy() -- main audio processing
 *
 * For the active input pin: forward frames to the output.
 * For all other input pins: consume and discard to prevent buffer stalls.
 *
 * Input buffers are ordered by connection order in bsource_list.
 * Slot 0 = first connected source, etc.
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
		if (++slot >= cd->num_slots)
			break;
	}

	copy_bytes = MIN(active_avail, sink_free);

	/* Second pass: copy active slot, silently drain idle slots. */
	slot = 0;
	list_for_item(src_item, &dev->bsource_list) {
		source = list_item(src_item, struct comp_buffer, sink_list);

		if (slot == cd->active_slot && copy_bytes > 0) {
			uint32_t frame_bytes = audio_stream_frame_bytes(&source->stream);

			/* Round down to whole frames to avoid splitting a sample. */
			if (copy_bytes >= frame_bytes) {
				uint32_t aligned = (copy_bytes / frame_bytes) * frame_bytes;

				buffer_stream_invalidate(source, aligned);
				audio_stream_copy(&source->stream, 0,
						  &sink->stream, 0,
						  aligned / audio_stream_sample_bytes(&source->stream));
				comp_update_buffer_consume(source, aligned);
				buffer_stream_writeback(sink, aligned);
				comp_update_buffer_produce(sink, aligned);
			}
		} else {
			uint32_t avail = audio_stream_get_avail_bytes(&source->stream);

			if (avail > 0)
				comp_update_buffer_consume(source, avail);
		}

		if (++slot >= cd->num_slots)
			break;
	}

	if (cd->active_slot == WOV_ARB_NO_ACTIVE && sink_free > 0) {
		/* No active drain: push silence so the host copier always has data.
		 * Split the memset at the circular-buffer wrap point if needed. */
		uint32_t fill_bytes = sink_free;
		void *wptr = audio_stream_get_wptr(&sink->stream);
		uint32_t bytes_to_end = audio_stream_bytes_without_wrap(&sink->stream, wptr);

		if (fill_bytes <= bytes_to_end) {
			memset(wptr, 0, fill_bytes);
		} else {
			/* Wrap: zero to end of buffer then continue from the start. */
			memset(wptr, 0, bytes_to_end);
			memset(audio_stream_get_addr(&sink->stream), 0, fill_bytes - bytes_to_end);
		}
		buffer_stream_writeback(sink, fill_bytes);
		comp_update_buffer_produce(sink, fill_bytes);
	}

	return 0;
}

/* -------------------------------------------------------------------------
 * IPC4 large-config (get): expose active slot to userspace as a volatile
 * RO enum kcontrol; the host reads this via GET_MODULE_LARGE_CONFIG.
 * ---------------------------------------------------------------------- */

static int wov_arb_get_large_config(struct comp_dev *dev,
				    uint32_t param_id,
				    bool first_block,
				    bool last_block,
				    uint32_t *data_offset,
				    char *data)
{
	struct wov_arb_data *cd = comp_get_drvdata(dev);

	/* param_id=2: return active_slot as uint32_t for the volatile bytes kcontrol. */
	if (param_id == IPC4_WOV_ARB_GET_ACTIVE_SLOT) {
		*(uint32_t *)data = (uint32_t)cd->active_slot;
		*data_offset = sizeof(uint32_t);
		return 0;
	}

	return -EINVAL;
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
		.set_large_config  = wov_arb_set_large_config,
		.get_large_config  = wov_arb_get_large_config,
		.get_attribute     = wov_arb_get_attribute,
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
