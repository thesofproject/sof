// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/kpb.h>
#include <sof/ipc/msg.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <module/module/llext.h>
#include <rtos/init.h>
#include <rtos/panic.h>
#include <rtos/string.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <ipc4/base-config.h>
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/notification.h>

#include <sof/audio/mfcc/mfcc_comp.h>
#include "mww_model.h"

#include <stdarg.h>
#include <stdio.h>
#include <zephyr/sys/printk.h>

/* TFLM error strings land here. Route to printk/mtrace so AllocateTensors()
 * and Invoke() failures print their real reason instead of vanishing.
 */
void DebugLog(const char *format, va_list args)
{
	vprintk(format, args);
}

int DebugVsnprintf(char *buffer, size_t buf_size, const char *format,
		   va_list vlist)
{
	return vsnprintk(buffer, buf_size, format, vlist);
}

#include <sof/lib/notifier.h>
#include <sof/audio/wov_arbiter.h>

#if CONFIG_AMS
#include <sof/lib/ams.h>
#include <sof/lib/ams_msg.h>
#include <ipc4/ams_helpers.h>
#endif

/* MFCC's non-compress output prepends a struct mfcc_data_header (24 bytes)
 * to each hop, followed by MWW_FEATURE_SIZE int32_t Q9.23 mel-log values.
 * This must match the frame size configured in the mww capture pipeline.
 */
#define MWW_HOP_BYTES (sizeof(struct mfcc_data_header) + MWW_FEATURE_SIZE * sizeof(int32_t))

/* Pre-roll history in ms that KPB drains to host on wake-word trigger. */
#define MWW_KPB_DRAIN_REQ_MS 2000

/* MFCC delivers one hop every 10 ms; MWW processes 3 hops per inference. */
#define MWW_MFCC_HOP_MS 10

/* Wake-word probability threshold above which KPB draining is triggered. */
#define MWW_DETECT_THRESHOLD 0.65f

/* Consecutive inferences above threshold required to confirm detection (~60 ms). */
#define MWW_CONSECUTIVE_DETECTS_REQUIRED 2

/* Number of startup inferences to warm up the temporal ring buffers before enabling triggers (~1 sec). */
#define MWW_WARMUP_INFERENCES 33

/* Hop interval for live scoring kcontrol updates (50 hops @ 10ms = 500 ms = 2 Hz). */
#define MWW_SCORE_UPDATE_HOPS 50

/* Soft mel-log AGC (units: Q9.23, matches MFCC output). One decade = +10 dB.
 * Target +2.5 dB (+0.25 in Q9.23), floor -20 dB. Attack: instant clamp so peak+gain
 * never exceeds MEL_CLIP_MAX_Q23 (+1.0, +10 dB). Release: dual-rate additive recovery:
 *   - Normal release during silence (VAD == 0): ~0.5 dB/s (100 hops/s).
 *   - Super-slow leak during speech (VAD == 1): ~0.05 dB/s (1/10th speed) to guarantee
 *     the AGC never stays permanently trapped at minimum gain even if VAD gets stuck.
 */
#define MWW_AGC_GAIN_TARGET_Q23        2097152 /* +2.5 dB (0.25 * 2^23) */
#define MWW_AGC_GAIN_FLOOR_Q23         -16777216 /* -20 dB, int32(-20 * 0.1 * 2^23) */
#define MWW_AGC_RELEASE_STEP_Q23       4194 /* 0.5 dB/s, int32((0.5 * 0.1 / 100) * 2^23) */
#define MWW_AGC_RELEASE_STEP_SPEECH_Q23 419 /* 0.05 dB/s, 1/10th normal release */

/* The range -1.0 to +1.0 of Q9.23 Mel values is scaled to +/-1.0 Q1.7. */
#define MEL_OFFSET_Q23          0 /* 0 */
#define MEL_SCALE_Q30           (1 << 30) /* 1.0 in Q30 */
#define MEL_CLIP_MAX_Q23        (1 << 23) /* +1.0 in Q23 */
#define MEL_CLIP_MIN_Q23        (-1 << 23) /* -1.0 in Q23 */
#define MEL_CLIP_MAX_Q7         127
#define MEL_CLIP_MIN_Q7         -128

SOF_DEFINE_REG_UUID(mww);
LOG_MODULE_REGISTER(mww, CONFIG_SOF_LOG_LEVEL);
#if CONFIG_COMP_MWW_MODULE
EXPORT_SYMBOL(mww_uuid);
EXPORT_SYMBOL(log_const_mww);
#endif

#if CONFIG_AMS
/* Key-phrase detected message, shared with src/samples/audio/detect_test.c
 * and consumed by kpb.c's AMS-consumer branch -- no kpb.c changes needed.
 */
static const ams_uuid_t ams_kpd_msg_uuid = AMS_KPD_MSG_UUID;
#endif

struct mww_comp_data {
	struct comp_data_blob_handler *model_handler;
	struct mww_classify mwc;
	struct kpb_client client_data;
	uint32_t drain_req_ms;
	uint8_t wov_slot_id;
	bool paused;
#if CONFIG_AMS
	uint32_t kpd_uuid_id;
#else
	struct kpb_event_data event_data;
#endif
	bool initialized;
	int8_t feature_buf[MWW_FEATURE_ELEM_COUNT];
	int feature_slices_filled;

	/* Bitmask of VAD flags for the MWW_FEATURE_SLICE_COUNT frames */
	uint32_t vad_history;
	/* Persistent AGC gain applied to every Q9.23 mel value */
	int32_t agc_gain_q23;

	/* Hop buffer for assembling contiguous data when circular buffer wraps */
	uint8_t hop_buf[MWW_HOP_BYTES] __aligned(4);

	/* Consecutive inferences with probability >= MWW_DETECT_THRESHOLD */
	uint32_t consecutive_detects;

	/* Live scoring tracking (twice a second = 50 hops @ 10ms) */
	float window_peak_prob;
	uint8_t current_score_idx;
	uint8_t last_notified_score_idx;
	uint16_t score_hop_counter;

	/* Telemetry counters */
	uint32_t total_inferences;
	uint32_t vad_gated_inferences;
	uint32_t detections;
	uint32_t kpb_trigger_events;
} __attribute__((aligned(8)));

#if CONFIG_AMS
static int mww_notify_kpb(struct processing_module *mod)
{
	struct mww_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct ams_message_payload ams_payload;

	comp_info(dev, "MWW keyword trigger -> notifying KPB to begin draining");

	cd->client_data.r_ptr = NULL;
	cd->client_data.sink = NULL;
	cd->client_data.id = 0; /**< TODO: acquire proper id from kpb */
	cd->client_data.drain_req = cd->drain_req_ms;

	dcache_writeback_region(&cd->client_data, sizeof(cd->client_data));

	ams_helper_prepare_payload(dev, &ams_payload, cd->kpd_uuid_id,
				   (uint8_t *)&cd->client_data,
				   sizeof(struct kpb_client));

	return ams_send(&ams_payload);
}
#else
static int mww_notify_kpb(struct processing_module *mod)
{
	struct mww_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "MWW keyword trigger -> notifying KPB to begin draining");

	cd->client_data.r_ptr = NULL;
	cd->client_data.sink = NULL;
	cd->client_data.id = 0;
	cd->client_data.drain_req = cd->drain_req_ms;
	cd->event_data.event_id = KPB_EVENT_BEGIN_DRAINING;
	cd->event_data.client_data = &cd->client_data;

	notifier_event(dev, NOTIFIER_ID_KPB_CLIENT_EVT,
		       NOTIFIER_TARGET_CORE_ALL_MASK, &cd->event_data,
		       sizeof(cd->event_data));
	return 0;
}
#endif /* CONFIG_AMS */

/* Notifier callback: arbiter is broadcasting a PAUSE or RESUME command. */
static void on_wov_ctrl(void *arg, enum notify_id id, void *data)
{
	struct comp_dev *dev = arg;
	struct processing_module *mod = comp_mod(dev);
	struct mww_comp_data *cd = module_get_private_data(mod);
	const struct wov_ctrl_notif *n = data;

	if (n->cmd == WOV_ARB_CMD_PAUSE && n->slot_id != cd->wov_slot_id) {
		comp_info(dev, "mww slot %u: paused (slot %u active)", cd->wov_slot_id, n->slot_id);
		cd->paused = true;
	} else {
		comp_info(dev, "mww slot %u: resumed by arbiter", cd->wov_slot_id);
		cd->paused = false;
		cd->consecutive_detects = 0;
		MWW_Reset(&cd->mwc);
	}
}

__cold static void mww_log_summary_at_shutdown(struct processing_module *mod)
{
	struct mww_comp_data *cd = mod ? module_get_private_data(mod) : NULL;
	struct comp_dev *dev = mod ? mod->dev : NULL;
	char summary_buf[256];

	if (!cd)
		return;

	snprintk(summary_buf, sizeof(summary_buf),
		 "[MWW STREAM SHUTDOWN SUMMARY Slot %u] Total Inferences=%u | VAD Gated=%u | Detections=%u | KPB Triggers=%u | Arena Used=%zu/%zu B",
		 cd->wov_slot_id,
		 cd->total_inferences, cd->vad_gated_inferences,
		 cd->detections, cd->kpb_trigger_events,
		 MWW_ArenaUsedBytes(&cd->mwc), MWW_ArenaCapacity(&cd->mwc));

	if (dev)
		comp_info(dev, "%s", summary_buf);
	printk("%s\n", summary_buf);
}

#if CONFIG_IPC_MAJOR_4
static void mww_notify_score(const struct comp_dev *dev, uint32_t score_idx)
{
	struct sof_ipc4_notify_module_data *msg_module_data;
	struct sof_ipc4_control_msg_payload *msg_payload;
	struct ipc_msg *msg;
	uint32_t data_size = sizeof(struct sof_ipc4_notify_module_data) +
			     sizeof(struct sof_ipc4_control_msg_payload) +
			     sizeof(struct sof_ipc4_ctrl_value_chan);
	struct ipc4_voice_cmd_notification notif;

	memset_s(&notif, sizeof(notif), 0, sizeof(notif));
	notif.primary.r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
	notif.primary.r.type = SOF_IPC4_GLB_NOTIFICATION;
	notif.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	notif.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;

	msg = ipc_msg_w_ext_init(notif.primary.dat, 0, data_size);
	if (!msg)
		return;

	msg_module_data = (struct sof_ipc4_notify_module_data *)msg->tx_data;
	msg_module_data->instance_id = IPC4_INST_ID(dev->ipc_config.id);
	msg_module_data->module_id = IPC4_MOD_ID(dev->ipc_config.id);
	msg_module_data->event_id = SOF_IPC4_NOTIFY_MODULE_EVENTID_ALSA_MAGIC_VAL |
				    SOF_IPC4_ENUM_CONTROL_PARAM_ID;
	msg_module_data->event_data_size = sizeof(struct sof_ipc4_control_msg_payload) +
					   sizeof(struct sof_ipc4_ctrl_value_chan);

	msg_payload = (struct sof_ipc4_control_msg_payload *)msg_module_data->event_data;
	msg_payload->id = 0; /* Index of the enum control on this widget */
	msg_payload->num_elems = 1;
	msg_payload->chanv[0].channel = 0;
	msg_payload->chanv[0].value = score_idx;

	ipc_msg_send(msg, NULL, false);
}
#endif

__cold static int mww_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mww_comp_data *cd;

	assert_can_be_cold();

	comp_info(dev, "entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->model_handler = mod_data_blob_handler_new(mod);
	if (!cd->model_handler) {
		mod_free(mod, cd);
		return -ENOMEM;
	}

	/* Derive slot_id: e.g. pipeline 101 -> slot 0, 102 -> slot 1, 103 -> slot 2 */
	if (dev->ipc_config.pipeline_id >= 101 && dev->ipc_config.pipeline_id <= 103)
		cd->wov_slot_id = (uint8_t)(dev->ipc_config.pipeline_id - 101);
	else
		cd->wov_slot_id = (uint8_t)(IPC4_INST_ID(dev->ipc_config.id) % 3);

	cd->mwc.slot_id = cd->wov_slot_id;
	cd->paused = false;

	cd->drain_req_ms = MWW_KPB_DRAIN_REQ_MS;
	cd->agc_gain_q23 = MWW_AGC_GAIN_TARGET_Q23;
	cd->window_peak_prob = 0.0f;
	cd->current_score_idx = 0;
	cd->last_notified_score_idx = 0;
	cd->score_hop_counter = 0;
#if CONFIG_AMS
	cd->kpd_uuid_id = AMS_INVALID_MSG_TYPE;
#endif

	return 0;
}

static int mww_prepare(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	struct mww_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "entry slot %u", cd->wov_slot_id);

	if (cd->initialized)
		return 0;

	const unsigned char *model_ptr = NULL;
	size_t blob_size = 0;

	model_ptr = comp_get_data_blob(cd->model_handler, &blob_size, NULL);
	if (model_ptr && blob_size > 0) {
		comp_info(dev, "MWW: loaded model from control blob, size=%zu", blob_size);
	} else {
		model_ptr = NULL;
		blob_size = 0;
	}

	ret = MWW_SetModel(&cd->mwc, model_ptr, blob_size);
	if (ret < 0) {
		comp_err(dev, "MWW_SetModel failed: %d (%s)", ret, cd->mwc.error);
		return ret;
	}

	ret = MWW_InitOps(&cd->mwc);
	if (ret < 0) {
		comp_err(dev, "MWW_InitOps failed: %d (%s)", ret, cd->mwc.error);
		return ret;
	}

#if CONFIG_AMS
	/* Register KD as AMS producer */
	ret = ams_helper_register_producer(dev, &cd->kpd_uuid_id, ams_kpd_msg_uuid);
	if (ret)
		return ret;
#endif

	notifier_register(dev, NULL, NOTIFIER_ID_WOV_CTRL, on_wov_ctrl, 0);

	cd->initialized = true;
	cd->feature_slices_filled = 0;
	cd->vad_history = 0;
	cd->consecutive_detects = 0;
	comp_info(dev, "MWW slot %u model initialized: arena_used=%zu / capacity=%zu bytes",
		  cd->wov_slot_id, MWW_ArenaUsedBytes(&cd->mwc), MWW_ArenaCapacity(&cd->mwc));

	return 0;
}

static int mww_process(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	struct mww_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	size_t bytes_to_process;
	const void *data_ptr, *buf_start;
	size_t buf_size;
	int ret = 0;

	if (!cd->initialized) {
		size_t avail = source_get_data_available(sources[0]);

		if (avail > 0) {
			const void *dp, *bs;
			size_t bsz;

			if (source_get_data(sources[0], avail, &dp, &bs, &bsz) == 0)
				source_release_data(sources[0], avail);
		}
		return 0;
	}

	if (cd->paused) {
		cd->window_peak_prob = 0.0f;
		cd->score_hop_counter = 0;
		if (cd->current_score_idx != 0) {
			cd->current_score_idx = 0;
			cd->last_notified_score_idx = 0;
#if CONFIG_IPC_MAJOR_4
			mww_notify_score(dev, 0);
#endif
		}
		size_t avail = source_get_data_available(sources[0]);

		if (avail > 0)
			source_release_data(sources[0], avail);
		return 0;
	}

	bytes_to_process = source_get_data_available(sources[0]);

	while (bytes_to_process >= MWW_HOP_BYTES) {
		const struct mfcc_data_header *hdr;
		const int32_t *mel;
		const uint8_t *hop_src;
		size_t bytes_to_end;
		int8_t *slice;
		int i;

		ret = source_get_data(sources[0], MWW_HOP_BYTES,
				      &data_ptr, &buf_start, &buf_size);
		if (ret != 0 || !data_ptr)
			break;

		/* Assemble hop into contiguous memory if it wraps across the ring buffer boundary */
		bytes_to_end = (const uint8_t *)buf_start + buf_size - (const uint8_t *)data_ptr;
		if (bytes_to_end >= MWW_HOP_BYTES) {
			hop_src = (const uint8_t *)data_ptr;
		} else {
			memcpy(cd->hop_buf, data_ptr, bytes_to_end);
			memcpy(cd->hop_buf + bytes_to_end, buf_start, MWW_HOP_BYTES - bytes_to_end);
			hop_src = cd->hop_buf;
		}

		hdr = (const struct mfcc_data_header *)hop_src;
		mel = (const int32_t *)(hop_src + sizeof(struct mfcc_data_header));
		slice = &cd->feature_buf[cd->feature_slices_filled * MWW_FEATURE_SIZE];

		/* Update VAD history bitmask across MWW_FEATURE_SLICE_COUNT slices */
		cd->vad_history = ((cd->vad_history << 1) | (hdr->vad_flag ? 1U : 0U)) &
				  ((1U << MWW_FEATURE_SLICE_COUNT) - 1);

		/* AGC: attack on this hop's peak (always track energy) */
		int32_t hop_peak_q23 = mel[0];

		for (i = 1; i < MWW_FEATURE_SIZE; i++) {
			if (mel[i] > hop_peak_q23)
				hop_peak_q23 = mel[i];
		}

		int32_t clip_headroom_q23 = MEL_CLIP_MAX_Q23 - hop_peak_q23;

		if (cd->agc_gain_q23 > clip_headroom_q23)
			cd->agc_gain_q23 = clip_headroom_q23;
		if (cd->agc_gain_q23 < MWW_AGC_GAIN_FLOOR_Q23)
			cd->agc_gain_q23 = MWW_AGC_GAIN_FLOOR_Q23;

		int32_t agc_gain_q23 = cd->agc_gain_q23;

		/* Requantize to int8 (Q1.7 range matching training AGC normalization) */
		for (i = 0; i < MWW_FEATURE_SIZE; i++) {
			int32_t mel_c = mel[i] + agc_gain_q23;

			/* Clamp to [-1.0, +1.0] Q9.23 range before Q1.7 conversion */
			if (mel_c > MEL_CLIP_MAX_Q23)
				mel_c = MEL_CLIP_MAX_Q23;
			else if (mel_c < MEL_CLIP_MIN_Q23)
				mel_c = MEL_CLIP_MIN_Q23;

			/* Rescale with offset and gain */
			mel_c = Q_MULTSR_32X32((int64_t)(mel_c + MEL_OFFSET_Q23),
					       MEL_SCALE_Q30, 23, 30, 7);
			if (mel_c > MEL_CLIP_MAX_Q7)
				mel_c = MEL_CLIP_MAX_Q7;
			else if (mel_c < MEL_CLIP_MIN_Q7)
				mel_c = MEL_CLIP_MIN_Q7;

			slice[i] = (int8_t)mel_c;
		}

		/* Release: normal recovery during silence, super-slow leak during speech */
		if (cd->agc_gain_q23 < MWW_AGC_GAIN_TARGET_Q23) {
			int32_t step = hdr->vad_flag ?
				MWW_AGC_RELEASE_STEP_SPEECH_Q23 : MWW_AGC_RELEASE_STEP_Q23;

			cd->agc_gain_q23 += step;
			if (cd->agc_gain_q23 > MWW_AGC_GAIN_TARGET_Q23)
				cd->agc_gain_q23 = MWW_AGC_GAIN_TARGET_Q23;
		}

#if CONFIG_COMP_MWW_DEBUG_TRACE
		{
			static int dbg_hop_count;
			int32_t mel_min = mel[0], mel_max = mel[0];
			int8_t f_min = slice[0], f_max = slice[0];

			dbg_hop_count++;
			for (i = 1; i < MWW_FEATURE_SIZE; i++) {
				if (mel[i] < mel_min) mel_min = mel[i];
				if (mel[i] > mel_max) mel_max = mel[i];
				if (slice[i] < f_min) f_min = slice[i];
				if (slice[i] > f_max) f_max = slice[i];
			}
			comp_info(dev, "[MWW DBG hop %d] vad=%d E=%d Ne=%d mel_min=%d mel_max=%d f_min=%d f_max=%d agc_q23=%d",
				  dbg_hop_count, (int)hdr->vad_flag,
				  (int)hdr->energy, (int)hdr->noise_energy,
				  mel_min, mel_max, f_min, f_max, (int)cd->agc_gain_q23);
		}
#endif

		/* Copy source data to sink so downstream stages keep seeing raw MFCC hops */
		if (num_of_sinks > 0 && sinks[0]) {
			void *snk_ptr, *snk_buf_start;
			size_t snk_buf_size;
			int sret = sink_get_buffer(sinks[0], MWW_HOP_BYTES,
						   &snk_ptr, &snk_buf_start, &snk_buf_size);
			if (sret == 0 && snk_ptr) {
				size_t snk_bytes_to_end = (uint8_t *)snk_buf_start +
							  snk_buf_size - (uint8_t *)snk_ptr;
				if (snk_bytes_to_end >= MWW_HOP_BYTES) {
					memcpy(snk_ptr, hop_src, MWW_HOP_BYTES);
				} else {
					memcpy(snk_ptr, hop_src, snk_bytes_to_end);
					memcpy(snk_buf_start,
					       hop_src + snk_bytes_to_end,
					       MWW_HOP_BYTES - snk_bytes_to_end);
				}
				sink_commit_buffer(sinks[0], MWW_HOP_BYTES);
			}
		}

		source_release_data(sources[0], MWW_HOP_BYTES);
		bytes_to_process -= MWW_HOP_BYTES;
		cd->feature_slices_filled++;

		if (cd->feature_slices_filled >= MWW_FEATURE_SLICE_COUNT) {
			cd->feature_slices_filled = 0;

			cd->total_inferences++;
			cd->mwc.audio_features = cd->feature_buf;
			cd->mwc.audio_data_size = MWW_FEATURE_ELEM_COUNT;

#if CONFIG_COMP_MWW_DEBUG_TRACE
			uint32_t c0 = k_cycle_get_32();
#endif
			ret = MWW_ProcessClassify(&cd->mwc);
#if CONFIG_COMP_MWW_DEBUG_TRACE
			uint32_t c1 = k_cycle_get_32();
#endif
			if (ret < 0) {
				comp_err(dev, "MWW_ProcessClassify failed: %d (%s)",
					 ret, cd->mwc.error);
				continue;
			}

#if CONFIG_COMP_MWW_DEBUG_TRACE
			comp_info(dev, "MWW probability=%d raw=%u (th=%p prio=%d cycles=%u)",
				  (int)(cd->mwc.probability * 100.0f), cd->mwc.raw_output,
				  k_current_get(), k_thread_priority_get(k_current_get()),
				  c1 - c0);
#endif

			if (cd->mwc.probability > cd->window_peak_prob)
				cd->window_peak_prob = cd->mwc.probability;

			if (cd->mwc.probability >= MWW_DETECT_THRESHOLD) {
				cd->consecutive_detects++;
				if (cd->total_inferences > MWW_WARMUP_INFERENCES &&
				    cd->consecutive_detects >= MWW_CONSECUTIVE_DETECTS_REQUIRED) {
					cd->detections++;
					comp_info(dev, "MWW keyword detected (slot %u): probability=%d pct (consecutive=%u, total=%u)",
						  cd->wov_slot_id,
						  (int)(cd->mwc.probability * 100.0f),
						  cd->consecutive_detects, cd->detections);
					cd->kpb_trigger_events++;
					mww_notify_kpb(mod);
					struct wov_detect_notif payload = { .slot_id = cd->wov_slot_id };
					notifier_event(dev, NOTIFIER_ID_WOV_DETECT,
						       NOTIFIER_TARGET_CORE_ALL_MASK,
						       &payload, sizeof(payload));
					cd->consecutive_detects = 0;
				}
			} else {
				cd->consecutive_detects = 0;
			}
		}

		/* Update scoring kcontrol twice a second (every 50 hops = 500 ms @ 10ms/hop) */
		cd->score_hop_counter++;
		if (cd->score_hop_counter >= MWW_SCORE_UPDATE_HOPS) {
			cd->score_hop_counter = 0;
			uint32_t s_idx = (uint32_t)(cd->window_peak_prob * 10.0f + 0.5f);
			if (s_idx > 10)
				s_idx = 10;
			cd->current_score_idx = (uint8_t)s_idx;
			cd->window_peak_prob = 0.0f;

#if CONFIG_IPC_MAJOR_4
			if (cd->current_score_idx != cd->last_notified_score_idx || cd->current_score_idx > 0) {
				cd->last_notified_score_idx = cd->current_score_idx;
				mww_notify_score(dev, cd->current_score_idx);
			}
#endif
		}
	}

	return ret;
}

static int mww_reset(struct processing_module *mod)
{
	struct mww_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "entry slot %u", cd->wov_slot_id);
	mww_log_summary_at_shutdown(mod);
	cd->feature_slices_filled = 0;
	cd->vad_history = 0;
	cd->consecutive_detects = 0;
	cd->total_inferences = 0;
	cd->score_hop_counter = 0;
	cd->window_peak_prob = 0.0f;
	cd->current_score_idx = 0;
	cd->last_notified_score_idx = 0;
	cd->agc_gain_q23 = MWW_AGC_GAIN_TARGET_Q23;
	memset(cd->feature_buf, 0, sizeof(cd->feature_buf));
	MWW_Reset(&cd->mwc);
#if CONFIG_IPC_MAJOR_4
	mww_notify_score(mod->dev, 0);
#endif
	return 0;
}

__cold static int mww_free(struct processing_module *mod)
{
	struct mww_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	comp_dbg(mod->dev, "entry slot %u", cd->wov_slot_id);
	mww_log_summary_at_shutdown(mod);

	notifier_unregister(mod->dev, NULL, NOTIFIER_ID_WOV_CTRL);

#if CONFIG_AMS
	if (cd->kpd_uuid_id != AMS_INVALID_MSG_TYPE) {
		int ret = ams_helper_unregister_producer(mod->dev, cd->kpd_uuid_id);

		if (ret)
			comp_err(mod->dev, "unregister ams error %d", ret);
	}
#endif

	MWW_Free(&cd->mwc);

	mod_data_blob_handler_free(mod, cd->model_handler);
	mod_free(mod, cd);
	return 0;
}

__cold static int mww_set_config(struct processing_module *mod, uint32_t param_id,
	enum module_cfg_fragment_position pos, uint32_t data_offset_size,
	const uint8_t *fragment, size_t fragment_size, uint8_t *response,
	size_t response_size)
{
	struct mww_comp_data *cd = module_get_private_data(mod);

	switch (param_id) {
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		/* Score enum is read-only */
		return 0;
	default:
		if (mod->dev->state != COMP_STATE_INIT && mod->dev->state != COMP_STATE_READY) {
			comp_warn(mod->dev, "mww_set_config(): model update ignored while not idle (state %d)",
				  mod->dev->state);
			return 0;
		}

		return comp_data_blob_set(cd->model_handler, pos, data_offset_size,
					  fragment, fragment_size);
	}
}

__cold static int mww_get_config(struct processing_module *mod,
				  uint32_t config_id, uint32_t *data_offset_size,
				  uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct mww_comp_data *cd = module_get_private_data(mod);
	struct sof_ipc4_control_msg_payload *ctl;

	switch (config_id) {
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		ctl = (struct sof_ipc4_control_msg_payload *)fragment;
		if (ctl->id == 0 && ctl->num_elems == 1) {
			ctl->chanv[0].value = cd->current_score_idx;
			*data_offset_size = sizeof(*ctl) + sizeof(ctl->chanv[0]);
			return 0;
		}
		return -EINVAL;
	default:
		return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
	}
}

static const struct module_interface mww_interface = {
	.init = mww_init,
	.prepare = mww_prepare,
	.process = mww_process,
	.set_configuration = mww_set_config,
	.get_configuration = mww_get_config,
	.reset = mww_reset,
	.free = mww_free
};

/* This controls build of the module. If COMP_MODULE is selected in kconfig
 * this is build as dynamically loadable module.
 */
#if CONFIG_COMP_MWW_MODULE

#include <module/module/api_ver.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("MWW", &mww_interface, 1,
				  SOF_REG_UUID(mww), 40);

SOF_LLEXT_BUILDINFO;

#else

/* Only used for the module adapter trace context, soon to be deprecated */
DECLARE_TR_CTX(mww_tr, SOF_UUID(mww_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(mww_interface, mww_uuid, mww_tr);
SOF_MODULE_INIT(mww, sys_comp_module_mww_interface_init);

#endif
