// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
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
#if CONFIG_IPC_MAJOR_4
#include <ipc4/base-config.h>
#endif
#include <sof/lib/memory.h>
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
#if CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
#endif
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

/* Per-period energy update; assumes single-channel S32LE (mono DMIC capture). */
static void vad_update_energy(struct comp_dev *dev,
			       struct audio_stream *s, uint32_t frames)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);
	uint32_t i;
	bool above;

	for (i = 0; i < frames; i++) {
		const int32_t *src = audio_stream_read_frag_s32(s, i);
		int32_t diff = abs(*src) - abs(cd->energy);
		cd->energy += diff >> cd->config.energy_shift;
	}

	static uint32_t log_cnt = 0;
	if (++log_cnt % 100 == 0) {
		comp_info(dev, "vad_gate: energy=%d threshold=%d active=%d speech_cnt=%u",
			  cd->energy, cd->config.threshold, cd->vad_active, cd->speech_cnt);
	}

	above = (cd->energy >= cd->config.threshold);
	if (above) {
		cd->silence_cnt = 0;
		if (++cd->speech_cnt >= cd->config.onset_frames && !cd->vad_active) {
			cd->vad_active = true;
			comp_info(dev, "vad_gate: SPEECH onset (energy=%d >= threshold=%d)",
				  cd->energy, cd->config.threshold);
		}
	} else {
		cd->speech_cnt = 0;
		if (++cd->silence_cnt >= cd->config.hangover_frames && cd->vad_active) {
			cd->vad_active = false;
			comp_info(dev, "vad_gate: SILENCE hangover expired (energy=%d < threshold=%d)",
				  cd->energy, cd->config.threshold);
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

	comp_cl_info(&drv->tctx, "vad_gate_new");

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

	cd->config.threshold      = VAD_DEFAULT_THRESHOLD;
	cd->config.onset_frames   = VAD_DEFAULT_ONSET_FRAMES;
	cd->config.hangover_frames = VAD_DEFAULT_HANGOVER;
	cd->config.energy_shift   = VAD_DEFAULT_ENERGY_SHIFT;

	comp_set_drvdata(dev, cd);
	dev->direction     = SOF_IPC_STREAM_CAPTURE;
	dev->direction_set = true;
	dev->state         = COMP_STATE_READY;

	return dev;
}

static void vad_gate_free(struct comp_dev *dev)
{
	comp_info(dev, "vad_gate_free");
	rfree(comp_get_drvdata(dev));
	comp_free_device(dev);
}

static int vad_gate_prepare(struct comp_dev *dev)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "vad_gate_prepare threshold=%d onset=%u hangover=%u",
		  cd->config.threshold,
		  cd->config.onset_frames,
		  cd->config.hangover_frames);

	cd->energy      = 0;
	cd->speech_cnt  = 0;
	cd->silence_cnt = 0;
	cd->vad_active  = false;

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static int vad_gate_reset(struct comp_dev *dev)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "vad_gate_reset");

	cd->energy      = 0;
	cd->speech_cnt  = 0;
	cd->silence_cnt = 0;
	cd->vad_active  = false;

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int vad_gate_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "vad_gate_trigger cmd %d", cmd);
	return comp_set_state(dev, cmd);
}

static int vad_gate_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
#if CONFIG_IPC_MAJOR_4
	struct vad_gate_data *cd = comp_get_drvdata(dev);

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
 * IPC4 large-config — runtime tuning of threshold, onset, hangover.
 * ---------------------------------------------------------------------- */

#if CONFIG_IPC_MAJOR_4
static int vad_gate_set_large_config(struct comp_dev *dev,
				      uint32_t param_id,
				      bool first_block,
				      bool last_block,
				      uint32_t data_offset,
				      const char *data)
{
	struct vad_gate_data *cd = comp_get_drvdata(dev);

	if (param_id != IPC4_VAD_GATE_SET_CONFIG)
		return -EINVAL;

	if (data_offset < sizeof(struct ipc4_vad_gate_config))
		return -EINVAL;

	const struct ipc4_vad_gate_config *cfg =
		(const struct ipc4_vad_gate_config *)data;

	memcpy_s(&cd->config, sizeof(cd->config), cfg, sizeof(*cfg));

	comp_info(dev, "vad_gate: config updated threshold=%d onset=%u hangover=%u shift=%u",
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
#endif /* CONFIG_IPC_MAJOR_4 */

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
	struct comp_buffer *source, *sink;
	uint32_t frame_bytes, frames, src_bytes;

	source = comp_dev_get_first_data_producer(dev);
	sink   = comp_dev_get_first_data_consumer(dev);

	frame_bytes = audio_stream_frame_bytes(&source->stream);
	frames = audio_stream_get_avail_bytes(&source->stream) / frame_bytes;
	if (!frames)
		return PPL_STATUS_PATH_STOP;
	src_bytes = frames * frame_bytes;

	buffer_stream_invalidate(source, src_bytes);
	vad_update_energy(dev, &source->stream, frames);

	if (!cd->vad_active) {
		/* Drain input to keep DMIC DMA running during silence. */
		comp_update_buffer_consume(source, src_bytes);
		return PPL_STATUS_PATH_STOP;
	}

	/* VAD active: limit frames to what sink can accept. */
	uint32_t sink_frame_bytes = audio_stream_frame_bytes(&sink->stream);
	uint32_t sink_frames = audio_stream_get_free_bytes(&sink->stream) / sink_frame_bytes;

	if (sink_frames < frames)
		frames = sink_frames;
	if (!frames)
		return 0;

	src_bytes = frames * frame_bytes;
	uint32_t sink_bytes = frames * sink_frame_bytes;

	audio_stream_copy(&source->stream, 0, &sink->stream, 0,
			  frames * audio_stream_get_channels(&source->stream));
	buffer_stream_writeback(sink, sink_bytes);
	comp_update_buffer_produce(sink, sink_bytes);
	comp_update_buffer_consume(source, src_bytes);

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
#if CONFIG_IPC_MAJOR_4
		.set_large_config = vad_gate_set_large_config,
		.get_attribute    = vad_gate_get_attribute,
#endif
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
