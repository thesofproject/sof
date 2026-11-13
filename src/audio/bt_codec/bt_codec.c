// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// SOF Bluetooth Transparent Mode (0x03) DSP Codec Offload Components:
//   - bt_lc3  (Zephyr liblc3, 16 kHz WB / 8 kHz NB)
//   - bt_msbc (Bluetooth HFP v1.8 / A2DP v1.3 H2-framed mSBC)
//   - bt_g722 (ITU-T Rec. G.722 SB-ADPCM)
//   - bt_sb4  (Experimental 4-band 24-tap tree-QMF subband ADPCM)

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/ipc-config.h>
#include <module/ipc4/base-config.h>
#include <sof/common.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/toolchain.h>

#include "lc3_math_shim.h"
#include "bt_codec_algos.h"

#undef fabsf
#undef fminf
#undef fmaxf
#undef truncf
#undef floorf
#undef ceilf
#undef roundf
#undef ldexpf
#undef frexpf
#undef sqrtf
#undef log10f

__weak float fabsf(float x) { return sof_lc3_fabsf(x); }
__weak float fminf(float a, float b) { return sof_lc3_fminf(a, b); }
__weak float fmaxf(float a, float b) { return sof_lc3_fmaxf(a, b); }
__weak float truncf(float x) { return sof_lc3_truncf(x); }
__weak float floorf(float x) { return sof_lc3_floorf(x); }
__weak float ceilf(float x) { return sof_lc3_ceilf(x); }
__weak float roundf(float x) { return sof_lc3_roundf(x); }
__weak float ldexpf(float x, int exp) { return sof_lc3_ldexpf(x, exp); }
__weak float frexpf(float x, int *exp) { return sof_lc3_frexpf(x, exp); }
__weak float sqrtf(float x) { return sof_lc3_sqrtf(x); }
__weak float log10f(float x) { return sof_lc3_log10f(x); }

LOG_MODULE_REGISTER(bt_codec, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(bt_lc3);
SOF_DEFINE_REG_UUID(bt_msbc);
SOF_DEFINE_REG_UUID(bt_g722);
SOF_DEFINE_REG_UUID(bt_sb4);

struct bt_codec_comp_data {
	enum bt_codec_type codec_type;
	enum sof_ipc_frame frame_format;
	uint32_t pcm_rate;
	uint32_t wire_rate;
	uint32_t pcm_step;
	uint32_t frame_bytes;
	struct bt_codec_state state;
};

static int bt_codec_init_common(struct processing_module *mod, enum bt_codec_type type)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct bt_codec_comp_data *cd;

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	cd->codec_type = type;
	cd->pcm_rate = 8000;
	cd->wire_rate = 8000;
	cd->pcm_step = 1;
	md->private = cd;
	comp_info(dev, "bt_codec init type=%d", (int)type);
	return 0;
}

static int bt_lc3_init(struct processing_module *mod)
{
	return bt_codec_init_common(mod, BT_CODEC_LC3);
}

static int bt_msbc_init(struct processing_module *mod)
{
	return bt_codec_init_common(mod, BT_CODEC_MSBC);
}

static int bt_g722_init(struct processing_module *mod)
{
	return bt_codec_init_common(mod, BT_CODEC_G722);
}

static int bt_sb4_init(struct processing_module *mod)
{
	return bt_codec_init_common(mod, BT_CODEC_SB4);
}

static int bt_codec_free(struct processing_module *mod)
{
	struct bt_codec_comp_data *cd = module_get_private_data(mod);

	mod_free(mod, cd);
	return 0;
}

static void bt_codec_params(struct processing_module *mod,
			    struct sof_source *source, struct sof_sink *sink)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	if (mod->priv.cfg.nb_input_pins == 1 && mod->priv.cfg.nb_output_pins == 1) {
		ipc4_update_source_format(source, &mod->priv.cfg.input_pins[0].audio_fmt);
		ipc4_update_sink_format(sink, &mod->priv.cfg.output_pins[0].audio_fmt);
	} else {
		ipc4_update_source_format(source, &mod->priv.cfg.base_cfg.audio_fmt);
		ipc4_update_sink_format(sink, &mod->priv.cfg.base_cfg.audio_fmt);
	}

	component_set_nearest_period_frames(dev, sink_get_rate(sink));
}

static int bt_codec_prepare(struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks)
{
	struct bt_codec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame in_fmt, out_fmt;
	uint32_t in_ch, out_ch, in_rate, out_rate;
	bool is_enc;

	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	bt_codec_params(mod, sources[0], sinks[0]);

	in_fmt = source_get_frm_fmt(sources[0]);
	out_fmt = sink_get_frm_fmt(sinks[0]);
	in_ch = source_get_channels(sources[0]);
	out_ch = sink_get_channels(sinks[0]);
	in_rate = source_get_rate(sources[0]);
	out_rate = sink_get_rate(sinks[0]);

	if (in_ch != 1 || out_ch != 1) {
		comp_err(dev, "bt_codec invalid channels in=%u out=%u (expected mono)",
			 in_ch, out_ch);
		return -EINVAL;
	}

	if (in_fmt != out_fmt ||
	    (in_fmt != SOF_IPC_FRAME_S16_LE && in_fmt != SOF_IPC_FRAME_S32_LE)) {
		comp_err(dev, "bt_codec unsupported frame fmt in=%d out=%d",
			 (int)in_fmt, (int)out_fmt);
		return -EINVAL;
	}

	is_enc = (dev->direction == SOF_IPC_STREAM_PLAYBACK);
	cd->pcm_rate = is_enc ? in_rate : out_rate;
	cd->wire_rate = is_enc ? out_rate : in_rate;

	if (cd->wire_rate != 8000 ||
	    (cd->pcm_rate != 8000 && cd->pcm_rate != 16000)) {
		comp_err(dev, "bt_codec invalid rates in=%u out=%u (pcm=%u wire=%u)",
			 in_rate, out_rate, cd->pcm_rate, cd->wire_rate);
		return -EINVAL;
	}

	cd->frame_format = in_fmt;
	cd->pcm_step = (cd->pcm_rate == 16000) ? 2u : 1u;
	cd->frame_bytes = source_get_frame_bytes(sources[0]);

	bt_codec_state_init(&cd->state, cd->codec_type, is_enc, cd->pcm_rate);

	comp_info(dev, "bt_codec prepare: type=%d enc=%d pcm_rate=%u wire_rate=%u fmt=%d",
		  (int)cd->codec_type, (int)is_enc, cd->pcm_rate, cd->wire_rate,
		  (int)cd->frame_format);
	return 0;
}

static int bt_codec_reset(struct processing_module *mod)
{
	struct bt_codec_comp_data *cd = module_get_private_data(mod);
	bool is_enc = (mod->dev->direction == SOF_IPC_STREAM_PLAYBACK);

	bt_codec_state_init(&cd->state, cd->codec_type, is_enc, cd->pcm_rate);
	return 0;
}

static int bt_codec_process_s16(struct bt_codec_comp_data *cd,
				struct sof_source *source, struct sof_sink *sink,
				uint32_t wire_frames, size_t src_bytes, size_t sink_bytes)
{
	const int16_t *x, *x_start, *x_end;
	int16_t *y, *y_start, *y_end;
	size_t x_size, y_size;
	uint32_t w, s;
	int ret;

	ret = source_get_data_s16(source, src_bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s16(sink, sink_bytes, &y, &y_start, &y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	x_end = x_start + x_size;
	y_end = y_start + y_size;

	if (cd->state.is_encoder) {
		for (w = 0; w < wire_frames; w++) {
			int16_t pcm[2];

			for (s = 0; s < cd->pcm_step; s++) {
				pcm[s] = *x++;
				if (x >= x_end)
					x = x_start;
			}
			*y++ = bt_codec_process_enc_step(&cd->state, pcm);
			if (y >= y_end)
				y = y_start;
		}
	} else {
		for (w = 0; w < wire_frames; w++) {
			int16_t cw16 = *x++;
			int16_t pcm[2];

			if (x >= x_end)
				x = x_start;

			bt_codec_process_dec_step(&cd->state, cw16, pcm);
			for (s = 0; s < cd->pcm_step; s++) {
				*y++ = pcm[s];
				if (y >= y_end)
					y = y_start;
			}
		}
	}

	source_release_data(source, src_bytes);
	sink_commit_buffer(sink, sink_bytes);
	return 0;
}

static int bt_codec_process_s32(struct bt_codec_comp_data *cd,
				struct sof_source *source, struct sof_sink *sink,
				uint32_t wire_frames, size_t src_bytes, size_t sink_bytes)
{
	const int32_t *x, *x_start, *x_end;
	int32_t *y, *y_start, *y_end;
	size_t x_size, y_size;
	uint32_t w, s;
	int ret;

	ret = source_get_data_s32(source, src_bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, sink_bytes, &y, &y_start, &y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	x_end = x_start + x_size;
	y_end = y_start + y_size;

	if (cd->state.is_encoder) {
		for (w = 0; w < wire_frames; w++) {
			int16_t pcm[2];
			int16_t cw16;

			for (s = 0; s < cd->pcm_step; s++) {
				pcm[s] = sat_int16(Q_SHIFT_RND(*x++, 31, 15));
				if (x >= x_end)
					x = x_start;
			}
			cw16 = bt_codec_process_enc_step(&cd->state, pcm);
			*y++ = (int32_t)cw16 << 16;
			if (y >= y_end)
				y = y_start;
		}
	} else {
		for (w = 0; w < wire_frames; w++) {
			int16_t cw16 = (int16_t)((uint32_t)*x++ >> 16);
			int16_t pcm[2];

			if (x >= x_end)
				x = x_start;

			bt_codec_process_dec_step(&cd->state, cw16, pcm);
			for (s = 0; s < cd->pcm_step; s++) {
				*y++ = (int32_t)pcm[s] << 16;
				if (y >= y_end)
					y = y_start;
			}
		}
	}

	source_release_data(source, src_bytes);
	sink_commit_buffer(sink, sink_bytes);
	return 0;
}

static int bt_codec_process(struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks)
{
	struct bt_codec_comp_data *cd = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	uint32_t src_frames = source_get_data_frames_available(source);
	uint32_t sink_frames = sink_get_free_frames(sink);
	uint32_t wire_frames;
	size_t src_bytes, sink_bytes;

	(void)num_of_sources;
	(void)num_of_sinks;

	if (cd->state.is_encoder) {
		wire_frames = MIN(src_frames / cd->pcm_step, sink_frames);
		src_bytes = wire_frames * cd->pcm_step * cd->frame_bytes;
		sink_bytes = wire_frames * cd->frame_bytes;
	} else {
		wire_frames = MIN(src_frames, sink_frames / cd->pcm_step);
		src_bytes = wire_frames * cd->frame_bytes;
		sink_bytes = wire_frames * cd->pcm_step * cd->frame_bytes;
	}

	if (!wire_frames)
		return 0;

	if (cd->frame_format == SOF_IPC_FRAME_S16_LE)
		return bt_codec_process_s16(cd, source, sink, wire_frames,
					    src_bytes, sink_bytes);

	if (cd->frame_format == SOF_IPC_FRAME_S32_LE)
		return bt_codec_process_s32(cd, source, sink, wire_frames,
					    src_bytes, sink_bytes);

	return -EINVAL;
}

static const struct module_interface bt_lc3_interface = {
	.init = bt_lc3_init,
	.prepare = bt_codec_prepare,
	.process = bt_codec_process,
	.reset = bt_codec_reset,
	.free = bt_codec_free,
};

static const struct module_interface bt_msbc_interface = {
	.init = bt_msbc_init,
	.prepare = bt_codec_prepare,
	.process = bt_codec_process,
	.reset = bt_codec_reset,
	.free = bt_codec_free,
};

static const struct module_interface bt_g722_interface = {
	.init = bt_g722_init,
	.prepare = bt_codec_prepare,
	.process = bt_codec_process,
	.reset = bt_codec_reset,
	.free = bt_codec_free,
};

static const struct module_interface bt_sb4_interface = {
	.init = bt_sb4_init,
	.prepare = bt_codec_prepare,
	.process = bt_codec_process,
	.reset = bt_codec_reset,
	.free = bt_codec_free,
};

DECLARE_TR_CTX(bt_lc3_tr, SOF_UUID(bt_lc3_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(bt_lc3_interface, bt_lc3_uuid, bt_lc3_tr);
SOF_MODULE_INIT(bt_lc3, sys_comp_module_bt_lc3_interface_init);

DECLARE_TR_CTX(bt_msbc_tr, SOF_UUID(bt_msbc_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(bt_msbc_interface, bt_msbc_uuid, bt_msbc_tr);
SOF_MODULE_INIT(bt_msbc, sys_comp_module_bt_msbc_interface_init);

DECLARE_TR_CTX(bt_g722_tr, SOF_UUID(bt_g722_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(bt_g722_interface, bt_g722_uuid, bt_g722_tr);
SOF_MODULE_INIT(bt_g722, sys_comp_module_bt_g722_interface_init);

DECLARE_TR_CTX(bt_sb4_tr, SOF_UUID(bt_sb4_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(bt_sb4_interface, bt_sb4_uuid, bt_sb4_tr);
SOF_MODULE_INIT(bt_sb4, sys_comp_module_bt_sb4_interface_init);
