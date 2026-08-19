// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/pipeline.h>
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
#include <rtos/alloc.h>
#include <rtos/string.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <user/eq.h>
#include <user/trace.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

static void sof_ut_log(const char *msg)
{
	printk("%s\n", msg);
}
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#include <ipc4/base-config.h>
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/notification.h>
#include <sof/audio/kpb.h>
#include <sof/lib/notifier.h>
#if CONFIG_AMS
#include <sof/lib/ams.h>
#include <sof/lib/ams_msg.h>
#include <ipc4/ams_helpers.h>
#endif

#include <rtos/timer.h>
#include <sof/audio/mfcc/mfcc_comp.h>
#include "speech.h"

/* TFLM error strings land here.  Route to printk/mtrace so AllocateTensors()
 * and Invoke() failures print their real reason instead of vanishing.
 */
void DebugLog(const char *format, va_list args)
{
	vprintk(format, args);
}

int DebugVsnprintf(char *buffer, size_t buf_size, const char *format,
		   va_list vlist)
{
	return vsnprintf(buffer, buf_size, format, vlist);
}

/* MFCC's non-compress output prepends a struct mfcc_data_header (24 bytes)
 * to each hop, followed by TFLM_FEATURE_SIZE int32_t Q9.23 mel-log values
 * (mel40.conf: 40 bins, 20ms hop). This must match MFCC_FRAME_BYTES in
 * host-gateway-micsel-mfcc-tflm-capture.conf.
 */
#define MFCC_HOP_BYTES (sizeof(struct mfcc_data_header) + TFLM_FEATURE_SIZE * sizeof(int32_t))

void start_tflm_ut_thread(void);
void run_unit_tests(void);

#if defined(__xtensa__)
#include <xtensa/config/tie.h>
#endif

static inline uint32_t get_ccount(void)
{
	return k_cycle_get_32();
}

SOF_DEFINE_REG_UUID(tflmcly);
LOG_MODULE_REGISTER(tflmcly, CONFIG_SOF_LOG_LEVEL);
#if CONFIG_COMP_TENSORFLOW_MODULE
EXPORT_SYMBOL(tflmcly_uuid);
EXPORT_SYMBOL(log_const_tflmcly);
#endif

static const char * const prediction[] = TFLM_CATEGORY_DATA;

/* Pre-roll history in ms that KPB drains to host on wake-word trigger.
 * Must be <= KPB_MAX_DRAINING_REQ (2000 ms on non-TGL, 2000 ms on TGL).
 */
#define TFLM_KPB_DRAIN_REQ_MS 2000

/* MFCC delivers one hop every 20 ms; run inference every 25 hops = 500 ms. */
#define TFLM_MFCC_HOP_MS            20
#define TFLM_INFERENCE_STRIDE_HOPS  25

/* Soft mel-log AGC (units: Q9.23, matches MFCC output). One decade = +10 dB.
 * Target 0 dB, floor -20 dB. Attack: instant clamp so peak+gain never exceeds
 * MEL_CLIP_MAX_Q23 (+1.0, +10 dB). Release: additive step per hop during active
 * speech (vad_flag == 1), chosen so recovery is ~0.5 dB/sec (hop=20 ms, 50 hops/sec).
 */
#define TFLM_AGC_GAIN_TARGET_Q23    0 /* 0 dB */
#define TFLM_AGC_GAIN_FLOOR_Q23     -16777216 /* -20 dB, int32(-20 * 0.1 * 2^23) */
#define TFLM_AGC_RELEASE_STEP_Q23   8389 /* 0.5 dB/s, int32((0.5 * 0.1 / 50) * 2^23) */

/* This needs to match with sof_tflm_train.py and sof_tflm_verify.py.
 *
 * The range -1.0 to +1.0 of Q9.23 Mel values is scaled to +/-1.0 Q1.7.
 *
 * in = [-1.0 1.0]; out = [-1 1];
 * offset = -(in(1) + in(2)) / 2 = 0
 * scale = (out(2) - out(1)) / (in(2) - in(1)) = 1.0
 */
#define MEL_OFFSET_Q23		0 /* 0 */
#define MEL_SCALE_Q30		(1 << 30) /* 1.0 in Q30 */
#define MEL_CLIP_MAX_Q23	(1 << 23) /* +1.0 in Q23 */
#define MEL_CLIP_MAX_Q7		127
#define MEL_CLIP_MIN_Q7		-128

struct tflm_comp_data {
	struct comp_data_blob_handler *model_handler;
	struct tf_classify tfc;
	struct ipc_msg *msg;
	struct kpb_event_data event_data;
	struct kpb_client client_data;
	uint32_t drain_req_ms;
#if CONFIG_AMS
	uint32_t kpd_uuid_id;
#endif
	/* Per-instance sliding window and inference cadence. */
	int8_t feature_buf[TFLM_FEATURE_ELEM_COUNT];
	int frame_counter;
	/* Bitmask of VAD flags for the TFLM_FEATURE_COUNT frames in feature_buf. */
	uint64_t vad_history;
	/* Persistent AGC gain applied to every Q9.23 mel value (see AGC defines). */
	int32_t agc_gain_q23;
	/* Per-instance shutdown-summary counters. */
	uint32_t category_totals[TFLM_CATEGORY_COUNT];
	uint32_t total_inferences;
	uint32_t kpb_trigger_events;
} __attribute__((aligned(8)));

#if CONFIG_AMS
/* Key-phrase detected AMS message UUID (matches KPB consumer). */
static const ams_uuid_t tflm_ams_kpd_msg_uuid = AMS_KPD_MSG_UUID;
#endif

static void tflm_notify_kpb(struct processing_module *mod)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "TFLM keyword trigger -> notifying KPB to begin draining");

	cd->client_data.r_ptr = NULL;
	cd->client_data.sink = NULL;
	cd->client_data.id = 0;
	cd->client_data.drain_req = cd->drain_req_ms;

#if CONFIG_AMS
	struct ams_message_payload ams_payload;
	int ret;

	ams_helper_prepare_payload(dev, &ams_payload, cd->kpd_uuid_id,
				   (uint8_t *)&cd->client_data,
				   sizeof(cd->client_data));
	ret = ams_send(&ams_payload);
	if (ret)
		comp_err(dev, "ams_send failed %d", ret);
#else
	cd->event_data.event_id = KPB_EVENT_BEGIN_DRAINING;
	cd->event_data.client_data = &cd->client_data;

	notifier_event(dev, NOTIFIER_ID_KPB_CLIENT_EVT,
		       NOTIFIER_TARGET_CORE_ALL_MASK, &cd->event_data,
		       sizeof(cd->event_data));
#endif
}

/* TODO: scaffolding for future IPC4 keyword notification path; unused today. */
static __maybe_unused int tflm_ipc_notification_init(struct processing_module *mod)
{
	return 0;
}

static __maybe_unused void tflm_send_keyword_notification(struct processing_module *mod,
							  uint32_t category_idx)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct sof_ipc4_notify_module_data *msg_module_data;
	struct sof_ipc4_control_msg_payload *msg_payload;

	if (!cd->msg)
		return;

	msg_module_data = (struct sof_ipc4_notify_module_data *)cd->msg->tx_data;
	msg_payload = (struct sof_ipc4_control_msg_payload *)msg_module_data->event_data;
	msg_payload->chanv[0].value = category_idx;
	ipc_msg_send(cd->msg, NULL, false);
}


/* Shared TFLM backend state. speech.cc has one static arena/model/interpreter,
 * so TF_SetModel/TF_InitOps must run only once no matter how many tflmcly
 * instances the topology creates; secondary instances attach to the first-init
 * instance.
 */
static bool g_tflm_initialized;
static int g_tflm_instance_count;
static int g_tflm_shared_categories;

__cold static void tflm_log_summary_at_shutdown(struct processing_module *mod)
{
	struct tflm_comp_data *cd = mod ? module_get_private_data(mod) : NULL;
	struct comp_dev *dev = mod ? mod->dev : NULL;
	char summary_buf[256];
	size_t off;
	int i;

	if (!cd)
		return;

	off = snprintk(summary_buf, sizeof(summary_buf),
		       "[TFLM STREAM SHUTDOWN SUMMARY] Total Inferences=%u | Keyword Events:",
		       cd->total_inferences);
	for (i = 0; i < TFLM_CATEGORY_COUNT && off < sizeof(summary_buf); i++) {
		off += snprintk(summary_buf + off, sizeof(summary_buf) - off,
				"%s %s=%u", i ? "," : "",
				prediction[i], cd->category_totals[i]);
	}
	if (off < sizeof(summary_buf))
		snprintk(summary_buf + off, sizeof(summary_buf) - off,
			 " | Total KPB Triggers=%u", cd->kpb_trigger_events);
	sof_ut_log(summary_buf);
	if (dev)
		comp_info(dev, "%s", summary_buf);
	printk("%s\n", summary_buf);
}

void tflm_yield(void)
{
	k_yield();
}

__cold static int tflm_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct tflm_comp_data *cd;

	assert_can_be_cold();

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd) {
		printk("[TFLM INIT] FAILED: rzalloc(%zu) OOM\n", sizeof(*cd));
		return -ENOMEM;
	}

	md->private = cd;
	cd->tfc.categories = TFLM_CATEGORY_COUNT;
	cd->drain_req_ms = TFLM_KPB_DRAIN_REQ_MS;
	cd->agc_gain_q23 = TFLM_AGC_GAIN_TARGET_Q23;
#if CONFIG_AMS
	cd->kpd_uuid_id = AMS_INVALID_MSG_TYPE;
#endif
	g_tflm_instance_count++;
	printk("[TFLM INIT] instance=%d dev=%p init complete\n",
	       g_tflm_instance_count, dev);
	return 0;
}

__cold static int tflm_free(struct processing_module *mod)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	tflm_log_summary_at_shutdown(mod);
	if (--g_tflm_instance_count <= 0) {
		g_tflm_instance_count = 0;
		g_tflm_initialized = false;
	}
	rfree(cd);
	return 0;
}

__cold static int tflm_set_config(struct processing_module *mod, uint32_t param_id,
	enum module_cfg_fragment_position pos, uint32_t data_offset_size,
	const uint8_t *fragment, size_t fragment_size, uint8_t *response,
	size_t response_size)
{
	return 0;
}

/*
 * This expects features from 16kHz mono 16 bit input stream.
 *
 * Features must be processed using the following flow
 * https://github.com/tensorflow/tflite-micro/blob/main/tensorflow/lite/micro/examples/micro_speech/images/audio_preprocessor_int8.png
 * 1. Preprocess the audio data using MFCC to generate the features
 * 2. Run the features through the model
 * 3. Print the model output predictions
 *
 * Each call TF_ProcessClassify() needs 1470ms of audio features or
 * TFLM_FEATURE_COUNT (49) features. We iterate over the feature count
 * and increment starting feature one by one (a 30ms stride) and re
 * call TF_ProcessClassify() until we have less than TFLM_FEATURE_COUNT
 * features in the input buffer.
 */

static int tflm_process(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);


	/* Guard: skip processing if TFLM interpreter was not prepared */
	if (!g_tflm_initialized) {
		printk("[TFLM PROCESS] WARNING: not initialized, skipping frame\n");
		/* Still drain the source so the pipeline doesn't stall */
		size_t avail = source_get_data_available(sources[0]);
		if (avail > 0) {
			const void *dp, *bs;
			size_t bsz;
			if (source_get_data(sources[0], avail, &dp, &bs, &bsz) == 0)
				source_release_data(sources[0], avail);
		}
		return 0;
	}

	size_t bytes_to_process = source_get_data_available(sources[0]);
	const void *data_ptr, *buf_start;
	size_t buf_size;
	int ret = 0;

#if CONFIG_COMP_TENSORFLOW_DEBUG_TRACE
	uint32_t sched_ccount_start = get_ccount();
	uint64_t sched_timer_start = sof_cycle_get_64();
#endif

	int8_t *feature_buf = cd->feature_buf;

	while (bytes_to_process >= MFCC_HOP_BYTES) {
		ret = source_get_data(sources[0], MFCC_HOP_BYTES,
				      &data_ptr, &buf_start, &buf_size);
		if (ret != 0 || !data_ptr)
			break;

		{
			/* Ring buffer may wrap inside a single hop; copy the
			 * 184-byte header+mel payload into a contiguous scratch
			 * so hdr / mel[] accesses (and the sink copy) can treat
			 * it as linear.
			 */
			uint8_t hop_scratch[MFCC_HOP_BYTES];
			const uint8_t *src_end = (const uint8_t *)buf_start + buf_size;
			size_t src_until_wrap = src_end - (const uint8_t *)data_ptr;

			if (src_until_wrap >= MFCC_HOP_BYTES) {
				memcpy(hop_scratch, data_ptr, MFCC_HOP_BYTES);
			} else {
				memcpy(hop_scratch, data_ptr, src_until_wrap);
				memcpy(hop_scratch + src_until_wrap, buf_start,
				       MFCC_HOP_BYTES - src_until_wrap);
			}

			/* Strip the mfcc_data_header and requantize the
			 * TFLM_FEATURE_SIZE int32 Q9.23 mel-log values into
			 * int8 features for this hop.
			 */
			const struct mfcc_data_header *hdr =
				(const struct mfcc_data_header *)hop_scratch;
			const int32_t *mel = (const int32_t *)
				(hop_scratch + sizeof(struct mfcc_data_header));
			int8_t hop_features[TFLM_FEATURE_SIZE];

			/* Update VAD history bitmask (tracks last TFLM_FEATURE_COUNT hops). */
			cd->vad_history = ((cd->vad_history << 1) | (hdr->vad_flag ? 1ULL : 0ULL)) &
					  ((1ULL << TFLM_FEATURE_COUNT) - 1);

			/* AGC: attack on this hop's peak (always track energy). */
			int32_t hop_peak_q23 = mel[0];
			for (int i = 1; i < TFLM_FEATURE_SIZE; i++)
				if (mel[i] > hop_peak_q23)
					hop_peak_q23 = mel[i];

			int32_t clip_headroom_q23 = MEL_CLIP_MAX_Q23 - hop_peak_q23;
			if (cd->agc_gain_q23 > clip_headroom_q23)
				cd->agc_gain_q23 = clip_headroom_q23;
			if (cd->agc_gain_q23 < TFLM_AGC_GAIN_FLOOR_Q23)
				cd->agc_gain_q23 = TFLM_AGC_GAIN_FLOOR_Q23;

			int32_t agc_gain_q23 = cd->agc_gain_q23;

			comp_info(mod->dev, "tflm agc: peak_q23=%d gain_q23=%d",
				  hop_peak_q23, agc_gain_q23);

			/* Requantize to int8: skip if no speech in the 49-hop window (fill with silence) */
			if (cd->vad_history) {
				for (int i = 0; i < TFLM_FEATURE_SIZE; i++) {
					int32_t mel_c = mel[i] + agc_gain_q23;

					/* Rescale asymmetrically with offset and gain. */
					mel_c = Q_MULTSR_32X32((int64_t)(mel_c + MEL_OFFSET_Q23),
							       MEL_SCALE_Q30, 23, 30, 7);
					if (mel_c > MEL_CLIP_MAX_Q7)
						mel_c = MEL_CLIP_MAX_Q7;
					else if (mel_c < MEL_CLIP_MIN_Q7)
						mel_c = MEL_CLIP_MIN_Q7;

					hop_features[i] = (int8_t)mel_c;
				}
			} else {
				memset(hop_features, MEL_CLIP_MIN_Q7, sizeof(hop_features));
			}

			/* Release: creep back toward 0 dB target. */
			if (cd->agc_gain_q23 < TFLM_AGC_GAIN_TARGET_Q23) {
				cd->agc_gain_q23 += TFLM_AGC_RELEASE_STEP_Q23;
				if (cd->agc_gain_q23 > TFLM_AGC_GAIN_TARGET_Q23)
					cd->agc_gain_q23 = TFLM_AGC_GAIN_TARGET_Q23;
			}

#if CONFIG_COMP_TENSORFLOW_DEBUG_TRACE
			{
				static int dbg_hop_count;
				int32_t mel_min = mel[0], mel_max = mel[0];
				int8_t f_min = hop_features[0], f_max = hop_features[0];
				char dbg_buf[192];

				dbg_hop_count++;
				for (int i = 1; i < TFLM_FEATURE_SIZE; i++) {
					if (mel[i] < mel_min) mel_min = mel[i];
					if (mel[i] > mel_max) mel_max = mel[i];
					if (hop_features[i] < f_min) f_min = hop_features[i];
					if (hop_features[i] > f_max) f_max = hop_features[i];
				}
				snprintk(dbg_buf, sizeof(dbg_buf),
					 "[DBG hop %d] vad=%d E=%d Ne=%d mel_min=%d mel_max=%d f_min=%d f_max=%d agc_q23=%d",
					 dbg_hop_count, (int)hdr->vad_flag,
					 (int)hdr->energy, (int)hdr->noise_energy,
					 mel_min, mel_max,
					 f_min, f_max,
					 (int)cd->agc_gain_q23);
				sof_ut_log(dbg_buf);
			}
#endif

			/* Shift 49-frame sliding window history by 1 frame (20ms hop) */
			memmove(&feature_buf[0], &feature_buf[TFLM_FEATURE_SIZE],
				TFLM_FEATURE_ELEM_COUNT - TFLM_FEATURE_SIZE);
			memcpy(&feature_buf[TFLM_FEATURE_ELEM_COUNT - TFLM_FEATURE_SIZE],
			       hop_features, TFLM_FEATURE_SIZE);

			/* Copy source data to sink first, before releasing source data */
			if (num_of_sinks > 0 && sinks[0]) {
				void *snk_ptr, *snk_buf_start;
				size_t snk_buf_size;
				int sret = sink_get_buffer(sinks[0], MFCC_HOP_BYTES,
							   &snk_ptr, &snk_buf_start, &snk_buf_size);
				if (sret == 0 && snk_ptr) {
					size_t size_to_wrap = (uint8_t *)snk_buf_start + snk_buf_size - (uint8_t *)snk_ptr;
					if (MFCC_HOP_BYTES <= size_to_wrap) {
						memcpy(snk_ptr, hop_scratch, MFCC_HOP_BYTES);
					} else {
						memcpy(snk_ptr, hop_scratch, size_to_wrap);
						memcpy(snk_buf_start, hop_scratch + size_to_wrap,
						       MFCC_HOP_BYTES - size_to_wrap);
					}
					sink_commit_buffer(sinks[0], MFCC_HOP_BYTES);
				} else {
					/* Rate-limited: first 5, then every 100th failure. */
					static uint32_t dbg_sink_fail_count;

					dbg_sink_fail_count++;
					if (dbg_sink_fail_count <= 5 ||
					    dbg_sink_fail_count % 100 == 0) {
						size_t free_sz = sink_get_free_size(sinks[0]);

						printk("[TFLM SINK FAIL #%u] sink_get_buffer(%u) ret=%d snk_ptr=%p free=%zu\n",
						       dbg_sink_fail_count,
						       (unsigned)MFCC_HOP_BYTES, sret,
						       snk_ptr, free_sz);
					}
				}
			}

			/* Release source data immediately after reading */
			source_release_data(sources[0], MFCC_HOP_BYTES);
			bytes_to_process -= MFCC_HOP_BYTES;

			cd->frame_counter++;

			/* Run inference every TFLM_INFERENCE_STRIDE_HOPS MFCC hops. */
			if (cd->frame_counter >= TFLM_INFERENCE_STRIDE_HOPS) {
				cd->frame_counter = 0;

				/* VAD gate: skip inference computation if no active speech in the 49-hop window */
				if (!cd->vad_history) {
#if CONFIG_COMP_TENSORFLOW_DEBUG_TRACE
					sof_ut_log("[DBG inference] skipped: VAD=0 in entire 49-hop window");
#endif
					continue;
				}

				cd->tfc.audio_features = feature_buf;
				cd->tfc.audio_data_size = TFLM_FEATURE_ELEM_COUNT;

#if CONFIG_COMP_TENSORFLOW_DEBUG_TRACE
				{
					int nonsat = 0;
					for (int fi = 0; fi < TFLM_FEATURE_ELEM_COUNT; fi++)
						if (feature_buf[fi] != -128)
							nonsat++;
					char nb[96];
					snprintk(nb, sizeof(nb),
						 "[DBG window] nonsat=%d/%d",
						 nonsat, TFLM_FEATURE_ELEM_COUNT);
					sof_ut_log(nb);
				}

				uint32_t stride_ms = TFLM_INFERENCE_STRIDE_HOPS * TFLM_MFCC_HOP_MS;
				uint32_t c_start = get_ccount();
				uint64_t t_start = sof_cycle_get_64();
#endif

				ret = TF_ProcessClassify(&cd->tfc);

#if CONFIG_COMP_TENSORFLOW_DEBUG_TRACE
				uint32_t c_end = get_ccount();
				uint64_t t_end = sof_cycle_get_64();
				uint32_t cycles = c_end - c_start;
				uint64_t timer_delta = t_end - t_start;
				uint32_t mcps_x100 = (timer_delta > 0) ? (uint32_t)(((uint64_t)cycles * 3840ULL) / timer_delta) : 0;
				static uint32_t inf_count = 0;
				inf_count++;

				char ut_buf[160];
				snprintk(ut_buf, sizeof(ut_buf), "[PERF] TFLM Inference #%u (stride=%ums): CCOUNT=%u cycles, 38M_Timer_delta=%llu, MCPS=%u.%02u",
					 inf_count, stride_ms,
					 cycles, (unsigned long long)timer_delta, mcps_x100 / 100, mcps_x100 % 100);
				sof_ut_log(ut_buf);
				printk("[PERF] TFLM Inference #%u: CCOUNT=%u cycles, timer_delta=%llu, MCPS=%u.%02u\n",
				       inf_count, cycles, (unsigned long long)timer_delta, mcps_x100 / 100, mcps_x100 % 100);

				snprintk(ut_buf, sizeof(ut_buf), "[TFLM MODEL GRAPH] Total Nodes = %d", cd->tfc.op_count);
				sof_ut_log(ut_buf);

				{
					char raw_buf[160];
					int off = snprintk(raw_buf, sizeof(raw_buf),
							   "[DBG raw_output] ret=%d", ret);
					for (int i = 0; i < TFLM_CATEGORY_COUNT &&
					     off < (int)sizeof(raw_buf); i++)
						off += snprintk(raw_buf + off,
								sizeof(raw_buf) - off,
								" %s=%d",
								prediction[i],
								cd->tfc.raw_output[i]);
					sof_ut_log(raw_buf);
				}

				for (int k = 0; k < cd->tfc.op_count && k < 10; k++) {
					snprintk(ut_buf, sizeof(ut_buf), "  Node %d (OpCode=%d): %u cycles",
						 k, cd->tfc.node_codes[k], cd->tfc.node_cycles[k]);
					sof_ut_log(ut_buf);
				}
#endif

				if (ret >= 0) {
					int max_idx = 0;
					float max_score = cd->tfc.predictions[0];

					for (int i = 0; i < cd->tfc.categories; i++) {
						if (cd->tfc.predictions[i] > max_score) {
							max_score = cd->tfc.predictions[i];
							max_idx = i;
						}
					}

					cd->total_inferences++;
					if (max_idx >= 0 && max_idx < TFLM_CATEGORY_COUNT)
						cd->category_totals[max_idx]++;

#if CONFIG_COMP_TENSORFLOW_DEBUG_TRACE
					{
						char result_buf[200];
						int max_pct_dbg = (int)(max_score * 100.0f);
						if (max_pct_dbg < 0) max_pct_dbg = 0;
						int off = snprintk(result_buf, sizeof(result_buf),
								   "TFLM top prediction: %s confidence=%d pct (inferences=%u):",
								   prediction[max_idx], max_pct_dbg,
								   cd->total_inferences);
						for (int i = 0; i < TFLM_CATEGORY_COUNT &&
						     off < (int)sizeof(result_buf); i++)
							off += snprintk(result_buf + off,
									sizeof(result_buf) - off,
									" %s=%u",
									prediction[i],
									cd->category_totals[i]);
						sof_ut_log(result_buf);
					}
#endif

					/* Only announce a keyword hit for real keyword classes
					 * (indices >= 2, i.e. not silence/unknown).
					 */
					if (max_idx >= 2 && max_score >= 0.50f) {
						char kw_buf[96];
						int max_pct = (int)(max_score * 100.0f);
						if (max_pct < 0) max_pct = 0;
						snprintk(kw_buf, sizeof(kw_buf),
							 "TFLM KEYWORD DETECTED: %s confidence=%d pct",
							 prediction[max_idx], max_pct);
						sof_ut_log(kw_buf);

						cd->kpb_trigger_events++;
						tflm_notify_kpb(mod);
					}
				}
			}
		}
	}

#if CONFIG_COMP_TENSORFLOW_DEBUG_TRACE
	{
		uint32_t sched_ccount_end = get_ccount();
		uint64_t sched_timer_end = sof_cycle_get_64();
		uint32_t sched_c_delta = sched_ccount_end - sched_ccount_start;
		uint64_t sched_t_delta = sched_timer_end - sched_timer_start;
		uint32_t sched_mcps_x100 = 0;

		if (sched_t_delta > 0)
			sched_mcps_x100 = (uint32_t)(((uint64_t)sched_c_delta * 3840ULL) / sched_t_delta);

		(void)sched_mcps_x100;
	}
#endif
	return ret;
}


static int tflm_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);

	printk("[TFLM PREPARE] tflm_prepare called, loading model...\n");

	if (g_tflm_initialized) {
		/* Shared TFLM engine already up. */
		cd->tfc.categories       = g_tflm_shared_categories;
		printk("[TFLM PREPARE] shared engine already initialized; attach instance\n");
		goto post_init;
	}

	int ret = TF_SetModel(&cd->tfc, NULL);
	if (ret < 0) {
		printk("[TFLM PREPARE] FAILED: TF_SetModel returned %d\n", ret);
		return ret;
	}

	ret = TF_InitOps(&cd->tfc);
	if (ret < 0) {
		printk("[TFLM PREPARE] FAILED: TF_InitOps returned %d (%s)\n",
		       ret, cd->tfc.error ? cd->tfc.error : "no detail");
		return ret;
	}

	g_tflm_shared_categories       = cd->tfc.categories;
	g_tflm_initialized = true;
	printk("[TFLM PREPARE] TFLM model & ops initialized successfully!\n");
	printk("[TFLM PREPARE] arena_used=%zu / capacity=%zu bytes\n",
	       TF_ArenaUsedBytes(), TF_ArenaCapacity());

post_init:
#if CONFIG_AMS
	if (cd->kpd_uuid_id == AMS_INVALID_MSG_TYPE) {
		int ams_ret = ams_helper_register_producer(mod->dev,
							   &cd->kpd_uuid_id,
							   tflm_ams_kpd_msg_uuid);
		if (ams_ret) {
			printk("[TFLM PREPARE] AMS producer register failed %d\n",
			       ams_ret);
			return ams_ret;
		}
	}
#endif
	return 0;
}


static int tflm_reset(struct processing_module *mod)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);

	tflm_log_summary_at_shutdown(mod);
	cd->vad_history = 0;
	cd->frame_counter = 0;
	memset(cd->feature_buf, 0, sizeof(cd->feature_buf));
#if CONFIG_AMS
	if (cd->kpd_uuid_id != AMS_INVALID_MSG_TYPE) {
		int ret = ams_helper_unregister_producer(mod->dev,
							 cd->kpd_uuid_id);
		if (ret)
			comp_err(mod->dev, "ams unregister failed %d", ret);
		cd->kpd_uuid_id = AMS_INVALID_MSG_TYPE;
	}
#else
	(void)cd;
#endif
	return 0;
}

static const struct module_interface tflmcly_interface = {
	.init = tflm_init,
	.prepare = tflm_prepare,
	.process = tflm_process,
	.set_configuration = tflm_set_config,
	.reset = tflm_reset,
	.free = tflm_free
};

DECLARE_TR_CTX(tflm_tr, SOF_UUID(tflmcly_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(tflmcly_interface, tflmcly_uuid, tflm_tr);
SOF_MODULE_INIT(tflmcly_interface, sys_comp_module_tflmcly_interface_init);

#if CONFIG_COMP_TENSORFLOW_MODULE
int llext_entry(void)
{
	printk("[TFLM LLEXT] llext_entry initialized cleanly.\n");
	return 0;
}
EXPORT_SYMBOL(llext_entry);
#endif









#if CONFIG_COMP_TENSORFLOW_MODULE

/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("TFLMCLY", &tflmcly_interface, 1, SOF_REG_UUID(tflmcly), 40);

SOF_LLEXT_BUILDINFO;

#endif
