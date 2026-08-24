// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// VAD Gate — lightweight voice-activity detector placed between the DMIC
// copier and the downstream Mixin in a WOV pipeline.
//
// When the detected energy stays below the threshold for hangover_frames
// consecutive frames (silence), the gate drains its input but returns
// PPL_STATUS_PATH_STOP so the Mixin/KPB/WOV pipelines downstream do not
// run.  When voice is detected (onset_frames consecutive frames above the
// threshold) the gate passes audio through and the downstream chain wakes.
//
// Energy estimator: first-order IIR on the peak |sample| amplitude per
// processing period; same pattern as detect_test.c's activation tracker.

#include <sof/audio/buffer.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/vad_gate.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/base-config.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <adsp_clk.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(vad_gate, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(vad_gate);
DECLARE_TR_CTX(vad_gate_tr, SOF_UUID(vad_gate_uuid), LOG_LEVEL_INFO);

/* Private runtime data. */
struct vad_gate_data {
	struct ipc4_base_module_cfg base_cfg;
	struct ipc4_vad_gate_config config;

	/* IIR energy accumulator (same units as S32LE sample amplitude). */
	int32_t energy;

	/* Debounce counters. */
	uint16_t speech_cnt;
	uint16_t silence_cnt;

	bool vad_active;
};

/* -------------------------------------------------------------------------
 * Energy estimation and VAD state machine
 * ---------------------------------------------------------------------- */

/* Per-period energy update — handles both S16LE and S32LE mono capture.
 * sample_bytes is 2 for S16LE, 4 for S32LE.
 * data_ptr, buf_start, buf_size describe the circular source buffer so wrap
 * is handled without the legacy audio_stream API. */
static void vad_update_energy(struct comp_dev *dev,
			       const void *data_ptr, const void *buf_start,
			       size_t buf_size, uint32_t frames,
			       uint32_t sample_bytes)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);
	const uint8_t *ptr = data_ptr;
	const uint8_t *end = (const uint8_t *)buf_start + buf_size;
	bool above;
	uint32_t i;

	/* First-order IIR: energy += (|sample| - energy) >> shift.
	 * Higher energy_shift = slower attack and release. */
	for (i = 0; i < frames; i++) {
		if (ptr >= end)
			ptr = buf_start;
		int32_t sample = (sample_bytes == 2) ?
			(int32_t)(*(const int16_t *)ptr) :
			*(const int32_t *)ptr;
		int32_t diff = abs(sample) - abs(cd->energy);
		cd->energy += diff >> cd->config.energy_shift;
		ptr += sample_bytes;
	}


	/* Debounce: require onset_frames consecutive above-threshold periods
	 * to open the gate and hangover_frames below-threshold to close it. */
	above = (cd->energy >= cd->config.threshold);
	if (above) {
		cd->silence_cnt = 0;
		if (++cd->speech_cnt >= cd->config.onset_frames && !cd->vad_active) {
			cd->vad_active = true;
			adsp_clock_set_cpu_freq(ADSP_CPU_CLOCK_FREQ_HPRO);
			comp_info(dev, "SPEECH onset (energy=%d >= threshold=%d) -> HPRO",
				  cd->energy, cd->config.threshold);
		}
	} else {
		cd->speech_cnt = 0;
		if (++cd->silence_cnt >= cd->config.hangover_frames && cd->vad_active) {
			cd->vad_active = false;
			adsp_clock_set_cpu_freq(ADSP_CPU_CLOCK_FREQ_WOVCRO);
			comp_info(dev, "SILENCE hangover expired (energy=%d < threshold=%d) -> WOVCRO",
				  cd->energy, cd->config.threshold);
			notifier_event(dev, NOTIFIER_ID_VAD_SILENCE,
				       NOTIFIER_TARGET_CORE_ALL_MASK, NULL, 0);
		}
	}
}

/* -------------------------------------------------------------------------
 * Component lifecycle
 * ---------------------------------------------------------------------- */

static struct comp_dev *vad_gate_new(const struct comp_driver *drv,
				     const struct comp_ipc_config *config,
				     const void *spec)
{
	struct comp_dev *dev;
	struct vad_gate_data *cd;

	comp_cl_info(&drv->tctx, "create");

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

	/* Apply compile-time defaults; host can tune via IPC4 set_large_config. */
	cd->config.threshold      = VAD_DEFAULT_THRESHOLD;
	cd->config.onset_frames   = VAD_DEFAULT_ONSET_FRAMES;
	cd->config.hangover_frames = VAD_DEFAULT_HANGOVER;
	cd->config.energy_shift   = VAD_DEFAULT_ENERGY_SHIFT;

	comp_set_drvdata(dev, cd);
	/* VAD gate sits in a capture chain; pass-through when voice active. */
	dev->direction     = SOF_IPC_STREAM_CAPTURE;
	dev->direction_set = true;
	dev->state         = COMP_STATE_READY;

	return dev;
}

static void vad_gate_free(struct comp_dev *dev)
{
	comp_dbg(dev, "free");
	rfree(comp_get_drvdata(dev));
	comp_free_device(dev);
}

static int vad_gate_prepare(struct comp_dev *dev)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "threshold=%d onset=%u hangover=%u",
		  cd->config.threshold,
		  cd->config.onset_frames,
		  cd->config.hangover_frames);

	cd->energy      = 0;
	cd->speech_cnt  = 0;
	cd->silence_cnt = 0;
	cd->vad_active  = false;

	/* Start at WOVCRO; clock escalates to HPRO on first voice onset. */
	adsp_clock_set_cpu_freq(ADSP_CPU_CLOCK_FREQ_WOVCRO);

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static int vad_gate_reset(struct comp_dev *dev)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "reset");

	cd->energy      = 0;
	cd->speech_cnt  = 0;
	cd->silence_cnt = 0;
	cd->vad_active  = false;
	adsp_clock_set_cpu_freq(ADSP_CPU_CLOCK_FREQ_WOVCRO);

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int vad_gate_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "cmd=%d", cmd);
	return comp_set_state(dev, cmd);
}

static int vad_gate_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);

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
 * IPC4 large-config — runtime tuning of threshold, onset, hangover.
 * ---------------------------------------------------------------------- */

static int vad_gate_set_large_config(struct comp_dev *dev,
				      uint32_t param_id,
				      bool first_block,
				      bool last_block,
				      uint32_t data_offset,
				      const char *data)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);

	if (param_id != IPC4_VAD_GATE_SET_CONFIG) {
		/* Accept initial SET from topology for RO status kcontrol; ignore data. */
		if (param_id == IPC4_VAD_GATE_GET_STATUS)
			return 0;
		return -EINVAL;
	}

	if (data_offset < sizeof(struct ipc4_vad_gate_config))
		return -EINVAL;

	const struct ipc4_vad_gate_config *cfg =
		(const struct ipc4_vad_gate_config *)data;

	memcpy_s(&cd->config, sizeof(cd->config), cfg, sizeof(*cfg));

	comp_info(dev, "config updated threshold=%d onset=%u hangover=%u shift=%u",
		  cd->config.threshold,
		  cd->config.onset_frames,
		  cd->config.hangover_frames,
		  cd->config.energy_shift);

	return 0;
}

static int vad_gate_get_attribute(struct comp_dev *dev,
				   uint32_t type, void *value)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);

	if (type == COMP_ATTR_BASE_CONFIG) {
		*(struct ipc4_base_module_cfg *)value = cd->base_cfg;
		return 0;
	}
	return -EINVAL;
}

/* -------------------------------------------------------------------------
 * copy() — main audio processing
 *
 * Always drains the source buffer to prevent DMIC DMA back-pressure.
 * Only forwards data to the sink (and returns 0) when VAD is active.
 * Returns PPL_STATUS_PATH_STOP during silence so downstream components idle.
 *
 * Assumes single-channel S32LE (mono DMIC capture at 16 kHz).
 * ---------------------------------------------------------------------- */
static int vad_gate_copy(struct comp_dev *dev)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source_buf = comp_dev_get_first_data_producer(dev);
	struct sof_source *src = audio_buffer_get_source(&source_buf->audio_buffer);
	const void *data_ptr, *buf_start;
	size_t buf_size;
	uint32_t frame_bytes, frames, n_bytes;
	int ret;

	frame_bytes = source_get_frame_bytes(src);
	frames = source_get_data_available(src) / frame_bytes;
	if (!frames)
		return PPL_STATUS_PATH_STOP;
	n_bytes = frames * frame_bytes;

	ret = source_get_data(src, n_bytes, &data_ptr, &buf_start, &buf_size);
	if (ret)
		return ret;

	vad_update_energy(dev, data_ptr, buf_start, buf_size, frames, frame_bytes);

	if (!cd->vad_active) {
		/* Drain input to keep DMIC DMA running during silence. */
		source_release_data(src, n_bytes);
		return PPL_STATUS_PATH_STOP;
	}

	/* VAD active: limit frames to what sink can accept. */
	struct comp_buffer *sink_buf = comp_dev_get_first_data_consumer(dev);
	struct sof_sink *snk = audio_buffer_get_sink(&sink_buf->audio_buffer);
	void *snk_ptr, *snk_buf_start;
	size_t snk_buf_size;
	uint32_t snk_frames = sink_get_free_size(snk) / frame_bytes;

	if (snk_frames < frames) {
		frames = snk_frames;
		n_bytes = frames * frame_bytes;
	}
	if (!frames) {
		source_release_data(src, 0);
		return 0;
	}

	ret = sink_get_buffer(snk, n_bytes, &snk_ptr, &snk_buf_start, &snk_buf_size);
	if (ret) {
		source_release_data(src, 0);
		return ret;
	}

	/* Copy with circular-buffer wrap handling for both source and sink. */
	const uint8_t *sp = data_ptr;
	uint8_t *dp = snk_ptr;
	size_t src_left = (const uint8_t *)buf_start + buf_size - sp;
	size_t snk_left = (uint8_t *)snk_buf_start + snk_buf_size - dp;
	size_t todo = n_bytes;

	while (todo) {
		size_t chunk = MIN(MIN(src_left, snk_left), todo);

		memcpy_s(dp, chunk, sp, chunk);
		sp += chunk;
		dp += chunk;
		src_left -= chunk;
		snk_left -= chunk;
		todo -= chunk;
		if (!src_left) {
			sp = buf_start;
			src_left = buf_size;
		}
		if (!snk_left) {
			dp = snk_buf_start;
			snk_left = snk_buf_size;
		}
	}

	source_release_data(src, n_bytes);
	sink_commit_buffer(snk, n_bytes);
	return 0;
}

/* -------------------------------------------------------------------------
 * IPC4 large-config get: volatile RO status kcontrol.
 * param_id=2 returns ipc4_vad_gate_status (energy + vad_active).
 * ---------------------------------------------------------------------- */
static int vad_gate_get_large_config(struct comp_dev *dev,
				      uint32_t param_id,
				      bool first_block,
				      bool last_block,
				      uint32_t *data_offset,
				      char *data)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);
	struct ipc4_vad_gate_status *st;
	struct ipc4_vad_gate_config *cfg;

	if (param_id == IPC4_VAD_GATE_SET_CONFIG) {
		cfg = (struct ipc4_vad_gate_config *)data;
		*cfg = cd->config;
		*data_offset = sizeof(*cfg);
		return 0;
	}

	if (param_id != IPC4_VAD_GATE_GET_STATUS)
		return -EINVAL;

	st = (struct ipc4_vad_gate_status *)data;
	st->energy     = (uint32_t)abs(cd->energy);
	st->vad_active = cd->vad_active ? 1 : 0;
	memset(st->_pad, 0, sizeof(st->_pad));
	*data_offset = sizeof(*st);
	return 0;
}

/* -------------------------------------------------------------------------
 * Component driver registration
 * ---------------------------------------------------------------------- */

static const struct comp_driver vad_gate_drv = {
	.type  = SOF_COMP_NONE,
	.uid   = SOF_RT_UUID(vad_gate_uuid),
	.tctx  = &vad_gate_tr,
	.ops   = {
		.create           = vad_gate_new,
		.free             = vad_gate_free,
		.params           = vad_gate_params,
		.trigger          = vad_gate_trigger,
		.copy             = vad_gate_copy,
		.prepare          = vad_gate_prepare,
		.reset            = vad_gate_reset,
		.set_large_config = vad_gate_set_large_config,
		.get_large_config = vad_gate_get_large_config,
		.get_attribute    = vad_gate_get_attribute,
	},
};

static SHARED_DATA struct comp_driver_info vad_gate_info = {
	.drv = &vad_gate_drv,
};

UT_STATIC void sys_comp_vad_gate_init(void)
{
	comp_register(&vad_gate_info);
}

DECLARE_MODULE(sys_comp_vad_gate_init);
SOF_MODULE_INIT(vad_gate, sys_comp_vad_gate_init);
