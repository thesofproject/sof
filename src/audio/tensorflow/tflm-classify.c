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
#include <rtos/string.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <user/eq.h>
#include <user/trace.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

extern void sof_ut_log(const char *msg);
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <ipc4/base-config.h>
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/notification.h>
#include <sof/audio/kpb.h>
#include <sof/lib/notifier.h>

#include <rtos/timer.h>
#include <platform/lib/clk.h>
#include <sof/lib/cpu-clk-manager.h>
#include "speech.h"

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
EXPORT_SYMBOL(tflmcly_uuid);
EXPORT_SYMBOL(log_const_tflmcly);

static const char * const prediction[] = TFLM_CATEGORY_DATA;

struct tflm_comp_data {
	struct comp_data_blob_handler *model_handler;
	struct tf_classify tfc;
	struct ipc_msg *msg;
	struct kpb_event_data event_data;
	struct kpb_client client_data;
} __attribute__((aligned(8)));

static void tflm_notify_kpb(struct processing_module *mod)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "TFLM keyword trigger -> notifying KPB to begin draining");

	cd->client_data.r_ptr = NULL;
	cd->client_data.sink = NULL;
	cd->client_data.id = 0;
	cd->event_data.event_id = KPB_EVENT_BEGIN_DRAINING;
	cd->event_data.client_data = &cd->client_data;

	notifier_event(dev, NOTIFIER_ID_KPB_CLIENT_EVT,
		       NOTIFIER_TARGET_CORE_ALL_MASK, &cd->event_data,
		       sizeof(cd->event_data));
}

static int tflm_ipc_notification_init(struct processing_module *mod)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct ipc_msg msg_proto;
	struct comp_dev *dev = mod->dev;
	struct comp_ipc_config *ipc_config = &dev->ipc_config;
	union ipc4_notification_header *primary =
		(union ipc4_notification_header *)&msg_proto.header;
	struct sof_ipc4_notify_module_data *msg_module_data;
	struct sof_ipc4_control_msg_payload *msg_payload;

	return 0;
}

static void tflm_send_keyword_notification(struct processing_module *mod, uint32_t category_idx)
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


static bool g_tflm_initialized = false;
static struct tflm_comp_data *g_tflm_cd = NULL;
static struct comp_dev *g_tflm_dev = NULL;
static uint64_t last_inference_timer = 0;
static uint32_t g_category_totals[TFLM_CATEGORY_COUNT] = {0, 0, 0, 0};
static uint32_t g_total_inferences = 0;
static uint32_t g_kpb_trigger_events = 0;

__cold static void tflm_log_summary_at_shutdown(struct processing_module *mod)
{
	struct comp_dev *dev = mod ? mod->dev : NULL;
	char summary_buf[256];
	snprintk(summary_buf, sizeof(summary_buf),
		 "[TFLM STREAM SHUTDOWN SUMMARY] Total Inferences=%u | Keyword Events: Silence=%u, Unknown=%u, Yes=%u, No=%u | Total KPB Triggers=%u",
		 g_total_inferences, g_category_totals[0], g_category_totals[1],
		 g_category_totals[2], g_category_totals[3], g_kpb_trigger_events);
	sof_ut_log(summary_buf);
	if (dev)
		comp_info(dev, "%s", summary_buf);
	printk("%s\n", summary_buf);
}

void tflm_yield(void)
{
	k_yield();
}

static struct tflm_comp_data g_tflm_comp_data_static;

__cold static int tflm_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct tflm_comp_data *cd = &g_tflm_comp_data_static;

	assert_can_be_cold();

	printk("[TFLM INIT] entered tflm_init using static BSS struct\n");
	memset(cd, 0, sizeof(*cd));
	md->private = cd;
	cd->tfc.categories = TFLM_CATEGORY_COUNT;
	g_tflm_cd = cd;
	g_tflm_dev = dev;
	printk("[TFLM INIT] init complete successfully!\n");
	return 0;
}

__cold static int tflm_free(struct processing_module *mod)
{
	assert_can_be_cold();

	tflm_log_summary_at_shutdown(mod);
	g_tflm_initialized = false;
	return 0;
}

__cold static int tflm_set_config(struct processing_module *mod, uint32_t param_id,
	enum module_cfg_fragment_position pos, uint32_t data_offset_size,
	const uint8_t *fragment, size_t fragment_size, uint8_t *response,
	size_t response_size)
{
	return 0;
}

/* The first feature for no and yes used in tflm_speech example */

static const int8_t expected_feature_no[TFLM_FEATURE_SIZE] = {
	126, 103, 124, 102, 124, 102, 123, 100, 118, 97, 118, 100, 118, 98,
	121, 100, 121, 98,  117, 91,  96,  74,  54,  87, 100, 87,  109, 92,
	91,  80,  64,  55,  83,  74,  74,  78,  114, 95, 101, 81,
};

static const int8_t expected_feature_yes[TFLM_FEATURE_SIZE] = {
	124, 105, 126, 103, 125, 101, 123, 100, 116, 98,  115, 97,  113, 90,
	91,  82,  104, 96,  117, 97,  121, 103, 126, 101, 125, 104, 126, 104,
	125, 101, 116, 90,  81,  74,  80,  71,  83,  76,  82,  71,
};

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
	struct comp_dev *dev = mod->dev;
	g_tflm_dev = dev;

	if (mod->priv.state != MODULE_PROCESSING)
		return 0;

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

	uint32_t sched_ccount_start = get_ccount();
	uint64_t sched_timer_start = sof_cycle_get_64();

	static int8_t feature_buf[TFLM_FEATURE_ELEM_COUNT];
	static int frame_counter = 0;

	if (bytes_to_process > 0) {
		ret = source_get_data(sources[0], bytes_to_process,
				      &data_ptr, &buf_start, &buf_size);
		if (ret == 0 && data_ptr) {
			/* Shift 49-frame sliding window history by 1 frame (40 bytes = 10ms) */
			size_t copy_feat_size = MIN(bytes_to_process, TFLM_FEATURE_SIZE);
			memmove(&feature_buf[0], &feature_buf[copy_feat_size],
				TFLM_FEATURE_ELEM_COUNT - copy_feat_size);
			memcpy(&feature_buf[TFLM_FEATURE_ELEM_COUNT - copy_feat_size],
			       data_ptr, copy_feat_size);

			/* Copy source data to sink first, before releasing source data */
			if (num_of_sinks > 0 && sinks[0]) {
				void *snk_ptr, *snk_buf_start;
				size_t snk_buf_size;
				int sret = sink_get_buffer(sinks[0], bytes_to_process,
							   &snk_ptr, &snk_buf_start, &snk_buf_size);
				if (sret == 0 && snk_ptr) {
					size_t size_to_wrap = (uint8_t *)snk_buf_start + snk_buf_size - (uint8_t *)snk_ptr;
					if (bytes_to_process <= size_to_wrap) {
						memcpy(snk_ptr, data_ptr, bytes_to_process);
					} else {
						memcpy(snk_ptr, data_ptr, size_to_wrap);
						memcpy(snk_buf_start, (const uint8_t *)data_ptr + size_to_wrap,
						       bytes_to_process - size_to_wrap);
					}
					sink_commit_buffer(sinks[0], bytes_to_process);
				}
			}

			/* Release source data immediately after reading */
			source_release_data(sources[0], bytes_to_process);

			frame_counter++;

			uint64_t current_timer = sof_cycle_get_64();
			uint64_t stride_delta = current_timer - last_inference_timer;
			uint32_t stride_ms_x100 = (uint32_t)((stride_delta * 100ULL) / 38400ULL);

			/* Run inference every 500ms */
			if (last_inference_timer > 0 && stride_delta >= (38400ULL * 500ULL)) {
				last_inference_timer = current_timer;
				frame_counter = 0;

				cd->tfc.audio_features = feature_buf;
				cd->tfc.audio_data_size = TFLM_FEATURE_ELEM_COUNT;

				uint32_t c_start = get_ccount();
				uint64_t t_start = sof_cycle_get_64();

				ret = TF_ProcessClassify(&cd->tfc);

				uint32_t c_end = get_ccount();
				uint64_t t_end = sof_cycle_get_64();

				uint32_t cycles = c_end - c_start;
				uint64_t timer_delta = t_end - t_start;
				uint32_t mcps_x100 = (timer_delta > 0) ? (uint32_t)(((uint64_t)cycles * 3840ULL) / timer_delta) : 0;
				static uint32_t inf_count = 0;
				inf_count++;

				char ut_buf[160];
				snprintk(ut_buf, sizeof(ut_buf), "[PERF] TFLM Inference #%u (stride=%u.%02ums): CCOUNT=%u cycles, 38M_Timer_delta=%llu, MCPS=%u.%02u",
					 inf_count, stride_ms_x100 / 100, stride_ms_x100 % 100,
					 cycles, (unsigned long long)timer_delta, mcps_x100 / 100, mcps_x100 % 100);
				sof_ut_log(ut_buf);
				printk("[PERF] TFLM Inference #%u: CCOUNT=%u cycles, timer_delta=%llu, MCPS=%u.%02u\n",
				       inf_count, cycles, (unsigned long long)timer_delta, mcps_x100 / 100, mcps_x100 % 100);

				snprintk(ut_buf, sizeof(ut_buf), "[TFLM MODEL GRAPH] Total Nodes = %d", cd->tfc.op_count);
				sof_ut_log(ut_buf);

				for (int k = 0; k < cd->tfc.op_count && k < 10; k++) {
					snprintk(ut_buf, sizeof(ut_buf), "  Node %d (OpCode=%d): %u cycles",
						 k, cd->tfc.node_codes[k], cd->tfc.node_cycles[k]);
					sof_ut_log(ut_buf);
				}

				if (ret >= 0) {
					int max_idx = 0;
					float max_score = cd->tfc.predictions[0];

					for (int i = 0; i < cd->tfc.categories; i++) {
						if (cd->tfc.predictions[i] > max_score) {
							max_score = cd->tfc.predictions[i];
							max_idx = i;
						}
					}

					g_total_inferences++;
					if (max_idx >= 0 && max_idx < TFLM_CATEGORY_COUNT)
						g_category_totals[max_idx]++;

					char result_buf[160];
					int max_pct = (int)(max_score * 100.0f);
					if (max_pct < 0) max_pct = 0;
					snprintk(result_buf, sizeof(result_buf), "TFLM top prediction: %s confidence=%d pct (inferences=%u): silence=%u, unknown=%u, yes=%u, no=%u",
						 prediction[max_idx], max_pct, g_total_inferences,
						 g_category_totals[0], g_category_totals[1], g_category_totals[2], g_category_totals[3]);
					sof_ut_log(result_buf);
					printk("%s\n", result_buf);

					if (max_score >= 0.50f) {
						char ut_buf[160];
						int max_pct = (int)(max_score * 100.0f);
						snprintk(ut_buf, sizeof(ut_buf), "TFLM KEYWORD DETECTED: %s confidence=%d pct",
							 prediction[max_idx], max_pct);
						sof_ut_log(ut_buf);

						if (max_idx >= 2) {
							g_kpb_trigger_events++;
							tflm_notify_kpb(mod);
						}
					}
				}
			}
		}
	}

	uint32_t sched_ccount_end = get_ccount();
	uint64_t sched_timer_end = sof_cycle_get_64();
	uint32_t sched_c_delta = sched_ccount_end - sched_ccount_start;
	uint64_t sched_t_delta = sched_timer_end - sched_timer_start;
	uint32_t sched_mcps_x100 = 0;

	if (sched_t_delta > 0)
		sched_mcps_x100 = (uint32_t)(((uint64_t)sched_c_delta * 3840ULL) / sched_t_delta);

	(void)sched_mcps_x100;
	return ret;
}


static int tflm_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);

	printk("[TFLM PREPARE] tflm_prepare called, loading model...\n");

	if (g_tflm_initialized) {
		printk("[TFLM PREPARE] already initialized, skipping\n");
		return 0;
	}

	int ret = TF_SetModel(&cd->tfc, NULL);
	if (ret < 0) {
		printk("[TFLM PREPARE] FAILED: TF_SetModel returned %d\n", ret);
		return ret;
	}

	ret = TF_InitOps(&cd->tfc);
	if (ret < 0) {
		printk("[TFLM PREPARE] FAILED: TF_InitOps returned %d\n", ret);
		return ret;
	}

	g_tflm_initialized = true;
	last_inference_timer = sof_cycle_get_64();
	printk("[TFLM PREPARE] TFLM model & ops initialized successfully!\n");
	return 0;
}


static int tflm_reset(struct processing_module *mod)
{
	tflm_log_summary_at_shutdown(mod);
	g_tflm_initialized = false;
	return 0;
}

static const struct module_interface tflmcly_interface = {
	.init = tflm_init,
	.prepare = tflm_prepare,
	.process = tflm_process,
	.set_configuration = tflm_set_config,
//	.get_configuration = tflm_get_config,
	.reset = tflm_reset,
	.free = tflm_free
};

DECLARE_TR_CTX(tflm_tr, SOF_UUID(tflmcly_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(tflmcly_interface, tflmcly_uuid, tflm_tr);
SOF_MODULE_INIT(tflmcly_interface, sys_comp_module_tflmcly_interface_init);

int llext_entry(void)
{
	printk("[TFLM LLEXT] llext_entry initialized cleanly.\n");
	return 0;
}
EXPORT_SYMBOL(llext_entry);









#if CONFIG_COMP_TENSORFLOW_MODULE

/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("TFLMCLY", &tflmcly_interface, 1, SOF_REG_UUID(tflmcly), 40);

SOF_LLEXT_BUILDINFO;

#endif
