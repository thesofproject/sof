// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h> /* for SHARED_DATA */
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/trig.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/tone.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ipc4/base-config.h>
#include <sof/audio/sink_api.h>
#include "tone.h"

LOG_MODULE_REGISTER(tone, CONFIG_SOF_LOG_LEVEL);

static int32_t tonegen(struct tone_state *sg)
{
	int64_t sine;
	int64_t w;
	/* sg->w is angle in Q4.28 radians format, sin() returns Q1.31 */
	/* sg->a is amplitude as Q1.31 */
	sine =
		q_mults_32x32(sin_fixed_32b(sg->w), sg->a,
			      Q_SHIFT_BITS_64(31, 31, 31));

	/* Next point */
	w = (int64_t)sg->w + sg->w_step;
	sg->w = (w > PI_MUL2_Q4_28)
		? (int32_t)(w - PI_MUL2_Q4_28) : (int32_t)w;

	if (sg->mute)
		return 0;
	else
		return (int32_t)sine; /* Q1.31 no saturation need */
}

static void tonegen_control(struct tone_state *sg)
{
	int64_t a;
	int64_t p;

	/* Count samples, 125 us blocks */
	sg->sample_count++;
	if (sg->sample_count < sg->samples_in_block)
		return;

	sg->sample_count = 0;
	if (sg->block_count < INT32_MAX)
		sg->block_count++;

	/* Fade-in ramp during tone */
	if (sg->block_count < sg->tone_length) {
		if (sg->a == 0)
			sg->w = 0; /* Reset phase to have less clicky ramp */

		if (sg->a > sg->a_target) {
			a = (int64_t)sg->a - sg->ramp_step;
			if (a < sg->a_target)
				a = sg->a_target;

		} else {
			a = (int64_t)sg->a + sg->ramp_step;
			if (a > sg->a_target)
				a = sg->a_target;
		}
		sg->a = (int32_t)a;
	}

	/* Fade-out ramp after tone*/
	if (sg->block_count > sg->tone_length) {
		a = (int64_t)sg->a - sg->ramp_step;
		if (a < 0)
			a = 0;

		sg->a = (int32_t)a;
	}

	/* New repeated tone, update for frequency or amplitude sweep */
	if (sg->block_count > sg->tone_period &&
	    (sg->repeat_count + 1 < sg->repeats)) {
		sg->block_count = 0;
		if (sg->ampl_coef > 0) {
			sg->a_target =
				sat_int32(q_multsr_32x32(sg->a_target,
							 sg->ampl_coef,
							 Q_SHIFT_BITS_64(31, 30, 31)));
			sg->a = (sg->ramp_step > sg->a_target)
				? sg->a_target : sg->ramp_step;
		}
		if (sg->freq_coef > 0) {
			/* f is Q16.16, freq_coef is Q2.30 */
			p = q_multsr_32x32(sg->f, sg->freq_coef,
					   Q_SHIFT_BITS_64(16, 30, 16));
			tonegen_update_f(sg, (int32_t)p); /* No saturation */
		}
		sg->repeat_count++;
	}
}

static int tone_s32_passthrough(struct processing_module *mod, struct sof_sink *sink,
				struct sof_source *source)
{
	struct comp_data *cd = module_get_private_data(mod);
	size_t output_frame_bytes, output_frames;
	size_t input_frame_bytes, input_frames;
	int32_t *output_pos, *output_start, output_cirbuf_size;
	int32_t const *input_pos, *input_start, *input_end;
	int32_t *output_end, input_cirbuf_size;
	uint32_t frames, bytes;
	int nch = cd->channels;
	int n;
	int ret;

	/* tone generator only ever has 1 sink */
	output_frames = sink_get_free_frames(sink);
	output_frame_bytes = sink_get_frame_bytes(sink);
	output_frames = mod->period_bytes / output_frame_bytes;

	ret = sink_get_buffer_s32(sink, output_frames * output_frame_bytes,
				  &output_pos, &output_start, &output_cirbuf_size);
	if (ret) {
		comp_err(mod->dev, "sink_get_buffer_s32() failed");
		return -ENODATA;
	}

	input_frames = source_get_data_frames_available(source);
	input_frame_bytes = source_get_frame_bytes(source);

	ret = source_get_data_s32(source, input_frames * input_frame_bytes,
				  &input_pos, &input_start, &input_cirbuf_size);
	if (ret) {
		comp_err(mod->dev, "source_get_data_s32() failed");
		return -ENODATA;
	}
	input_end = input_start + input_cirbuf_size;

	frames = MIN(output_frames, input_frames);

	if (frames * output_frame_bytes >= mod->period_bytes)
		frames = mod->period_bytes / output_frame_bytes;
	bytes = frames * output_frame_bytes;

	output_end = output_start + output_cirbuf_size;

	n = frames * nch;

	while (n > 0) {
		int n_wrap_source, n_wrap_dest, n_min;
		int i;

		n_wrap_dest = output_end - output_pos;
		n_wrap_source = input_end - input_pos;

		/* Process until source/dest wrap or completed n */
		n_min = (n < n_wrap_dest) ? n : n_wrap_dest;
		n_min = (n_min < n_wrap_source) ? n_min : n_wrap_source;
		while (n_min > 0) {
			n -= nch;
			n_min -= nch;
			for (i = 0; i < nch; i++) {
				*output_pos = *input_pos;
				output_pos++;
				input_pos++;
			}
		}

		/* Wrap destination/source buffer */
		if (output_pos >= output_end)
			output_pos = output_start;
		if (input_pos >= input_end)
			input_pos = input_start;
	}

	ret = sink_commit_buffer(sink, bytes);
	if (ret)
		return ret;

	return source_release_data(source, bytes);
}

/*
 * Tone generator algorithm code
 */
int tone_s32_default(struct processing_module *mod, struct sof_sink *sink,
		     struct sof_source *source)
{
	struct comp_data *cd = module_get_private_data(mod);
	size_t output_frame_bytes, output_frames;
	int32_t *output_pos, *output_start, output_cirbuf_size;
	int32_t *output_end;
	uint32_t frames, bytes;
	int nch = cd->channels;
	int i;
	int n;
	int n_wrap_dest;
	int n_min;
	int ret;

	if (cd->mode == TONE_MODE_PASSTHROUGH)
		return tone_s32_passthrough(mod, sink, source);

	/* tone generator only ever has 1 sink */
	output_frames = sink_get_free_frames(sink);
	output_frame_bytes = sink_get_frame_bytes(sink);
	output_frames = mod->period_bytes / output_frame_bytes;

	ret = sink_get_buffer_s32(sink, output_frames * output_frame_bytes,
				  &output_pos, &output_start, &output_cirbuf_size);
	if (ret)
		return -ENODATA;

	frames = output_frames;

	if (frames * output_frame_bytes >= mod->period_bytes)
		frames = mod->period_bytes / output_frame_bytes;
	bytes = frames * output_frame_bytes;

	output_end = output_start + output_cirbuf_size;

	n = frames * nch;
	if (!source) {
		while (n > 0) {
			n_wrap_dest = output_end - output_pos;

			/* Process until wrap or completed n */
			n_min = (n < n_wrap_dest) ? n : n_wrap_dest;
			while (n_min > 0) {
				n -= nch;
				n_min -= nch;
				for (i = 0; i < nch; i++) {
					switch (cd->mode) {
					case TONE_MODE_TONEGEN:
						tonegen_control(&cd->sg[i]);
						*output_pos = tonegen(&cd->sg[i]);
						break;
					case TONE_MODE_SILENCE:
						*output_pos = 0;
						break;
					default:
						break;
					}
					output_pos++;
				}
			}

			/* Wrap destination buffer */
			output_pos = output_start;
		}
	}

	return sink_commit_buffer(sink, bytes);
}

void tonegen_update_f(struct tone_state *sg, int32_t f)
{
	int64_t w_tmp;
	int64_t f_max;

	/* Calculate Fs/2, fs is Q32.0, f is Q16.16 */
	f_max = Q_SHIFT_LEFT((int64_t)sg->fs, 0, 16 - 1);
	f_max = (f_max > INT32_MAX) ? INT32_MAX : f_max;
	sg->f = (f > f_max) ? f_max : f;
	/* Q16 x Q31 -> Q28 */
	w_tmp = q_multsr_32x32(sg->f, sg->c, Q_SHIFT_BITS_64(16, 31, 28));
	w_tmp = (w_tmp > PI_Q4_28) ? PI_Q4_28 : w_tmp; /* Limit to pi Q4.28 */
	sg->w_step = (int32_t)w_tmp;
}

void tonegen_reset(struct tone_state *sg)
{
	sg->mute = 1;
	sg->a = 0;
	sg->a_target = TONE_AMPLITUDE_DEFAULT;
	sg->c = 0;
	sg->f = TONE_FREQUENCY_DEFAULT;
	sg->w = 0;
	sg->w_step = 0;

	sg->block_count = 0;
	sg->repeat_count = 0;
	sg->repeats = 0;
	sg->sample_count = 0;
	sg->samples_in_block = 0;

	/* Continuous tone */
	sg->freq_coef = ONE_Q2_30; /* Set freq multiplier to 1.0 */
	sg->ampl_coef = ONE_Q2_30; /* Set ampl multiplier to 1.0 */
	sg->tone_length = INT32_MAX;
	sg->tone_period = INT32_MAX;
	sg->ramp_step = ONE_Q1_31; /* Set lin ramp modification to max */
}

int tonegen_init(struct tone_state *sg, int32_t fs, int32_t f, int32_t a)
{
	int idx;
	int i;

	sg->a_target = a;
	sg->a = (sg->ramp_step > sg->a_target) ? sg->a_target : sg->ramp_step;

	idx = -1;
	sg->mute = 1;
	sg->fs = 0;

	/* Find index of current sample rate and then get from lookup table the
	 * corresponding 2*pi/Fs value.
	 */
	for (i = 0; i < TONE_NUM_FS; i++) {
		if (fs == tone_fs_list[i])
			idx = i;
	}

	if (idx < 0) {
		sg->w_step = 0;
		return -EINVAL;
	}

	sg->fs = fs;
	sg->c = tone_pi2_div_fs[idx]; /* Store 2*pi/Fs */
	sg->mute = 0;
	tonegen_update_f(sg, f);

	/* 125us as Q1.31 is 268435, calculate fs * 125e-6 in Q31.0  */
	sg->samples_in_block =
		(int32_t)q_multsr_32x32(fs, 268435, Q_SHIFT_BITS_64(0, 31, 0));

	return 0;
}
