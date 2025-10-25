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

/* Convert float frequency in Hz to Q16.16 fractional format */
#define TONE_FREQ(f) Q_CONVERT_FLOAT(f, 16)

/* Convert float gain to Q1.31 fractional format */
#define TONE_GAIN(v) Q_CONVERT_FLOAT(v, 31)

/* Set default tone amplitude and frequency */
#define TONE_AMPLITUDE_DEFAULT TONE_GAIN(0.1)      /*  -20 dB  */
#define TONE_FREQUENCY_DEFAULT TONE_FREQ(997.0)
#define TONE_NUM_FS            13       /* Table size for 8-192 kHz range */

LOG_MODULE_REGISTER(tone, CONFIG_SOF_LOG_LEVEL);

/* tone uuid : 04e3f894-2c5c-4f2e-8dc1694eeaab53fa */
SOF_DEFINE_REG_UUID(tone);

DECLARE_TR_CTX(tone_tr, SOF_UUID(tone_uuid), LOG_LEVEL_INFO);

/* 2*pi/Fs lookup tables in Q1.31 for each Fs */
static const int32_t tone_fs_list[TONE_NUM_FS] = {
	8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000,
	64000, 88200, 96000, 176400, 192000
};

static const int32_t tone_pi2_div_fs[TONE_NUM_FS] = {
	1686630, 1223858, 843315, 611929, 562210, 421657, 305965,
	281105, 210829, 152982, 140552, 76491, 70276
};

/* tone component private data */

struct tone_state {
	int mute;
	int32_t a; /* Current amplitude Q1.31 */
	int32_t a_target; /* Target amplitude Q1.31 */
	int32_t ampl_coef; /* Amplitude multiplier Q2.30 */
	int32_t c; /* Coefficient 2*pi/Fs Q1.31 */
	int32_t f; /* Frequency Q16.16 */
	int32_t freq_coef; /* Frequency multiplier Q2.30 */
	int32_t fs; /* Sample rate in Hertz Q32.0 */
	int32_t ramp_step; /* Amplitude ramp step Q1.31 */
	int32_t w; /* Angle radians Q4.28 */
	int32_t w_step; /* Angle step Q4.28 */
	uint32_t block_count;
	uint32_t repeat_count;
	uint32_t repeats; /* Number of repeats for tone (sweep steps) */
	uint32_t sample_count;
	uint32_t samples_in_block; /* Samples in 125 us block */
	uint32_t tone_length; /* Active length in 125 us blocks */
	uint32_t tone_period; /* Active + idle time in 125 us blocks */
};

struct comp_data {
	uint32_t channels;
	uint32_t rate;
	struct tone_state sg[PLATFORM_MAX_CHANNELS];
	int (*tone_func)(struct processing_module *mod, struct sof_sink *sink,
			 uint32_t frames, int32_t *output_pos, int32_t *output_start,
			 int32_t output_cirbuf_size);
};

static int32_t tonegen(struct tone_state *sg);
static void tonegen_control(struct tone_state *sg);
static void tonegen_update_f(struct tone_state *sg, int32_t f);

/*
 * Tone generator algorithm code
 */
static int tone_s32_default(struct processing_module *mod, struct sof_sink *sink,
			    uint32_t frames, int32_t *output_pos, int32_t *output_start,
			    int32_t output_cirbuf_size)
{
	struct comp_data *cd = module_get_private_data(mod);
	int32_t *output_end;
	int nch = cd->channels;
	int i;
	int n;
	int n_wrap_dest;
	int n_min;

	output_end = output_start + output_cirbuf_size;

	n = frames * nch;
	while (n > 0) {
		n_wrap_dest = output_end - output_pos;

		/* Process until wrap or completed n */
		n_min = (n < n_wrap_dest) ? n : n_wrap_dest;
		while (n_min > 0) {
			n -= nch;
			n_min -= nch;
			for (i = 0; i < nch; i++) {
				tonegen_control(&cd->sg[i]);
				*output_pos = tonegen(&cd->sg[i]);
				output_pos++;
			}
		}

		/* Wrap destination buffer */
		output_pos = output_start;
	}

	return 0;
}

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
	if ((sg->block_count > sg->tone_period) &&
	    (sg->repeat_count + 1 < sg->repeats)) {
		sg->block_count = 0;
		if (sg->ampl_coef > 0) {
			sg->a_target =
				sat_int32(q_multsr_32x32(sg->a_target,
				sg->ampl_coef, Q_SHIFT_BITS_64(31, 30, 31)));
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

static inline int32_t tonegen_get_f(struct tone_state *sg)
{
	return sg->f;
}

static inline int32_t tonegen_get_a(struct tone_state *sg)
{
	return sg->a_target;
}

static void tonegen_update_f(struct tone_state *sg, int32_t f)
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

static void tonegen_reset(struct tone_state *sg)
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

static int tonegen_init(struct tone_state *sg, int32_t fs, int32_t f, int32_t a)
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
		(int32_t) q_multsr_32x32(fs, 268435, Q_SHIFT_BITS_64(0, 31, 0));

	return 0;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static int tone_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd;
	int i;

	comp_info(dev, "tone_new()");

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	mod_data->private = cd;

	cd->tone_func = tone_s32_default;

	/* Reset tone generator and set channels volumes to default */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		tonegen_reset(&cd->sg[i]);

	return 0;
}

static int tone_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "tone_free()");

	rfree(cd);
	return 0;
}

/* set component audio stream parameters */
static int tone_params(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	enum sof_ipc_frame frame_fmt, valid_fmt;

	sinkb = comp_dev_get_first_data_consumer(dev);
	if (!sinkb) {
		comp_err(dev, "sink buffer");
		return -ENOTCONN;
	}

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);
	cd->rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;

	comp_info(dev, "tone_params(), frame_fmt = %u", frame_fmt);

	/* Tone supports only S32_LE PCM format atm */
	if (frame_fmt != SOF_IPC_FRAME_S32_LE) {
		comp_err(dev, "tone_params(), unsupported frame_fmt = %u", frame_fmt);
		return -EINVAL;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int tone_process(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_sink *sink = sinks[0];
	size_t output_frame_bytes, output_frames;
	int32_t *output_pos, *output_start, output_cirbuf_size;
	int ret;

	/* tone generator only ever has 1 sink */
	output_frames = sink_get_free_frames(sinks[0]);
	output_frame_bytes = sink_get_frame_bytes(sinks[0]);

	ret = sink_get_buffer_s32(sinks[0], output_frames * output_frame_bytes,
				  &output_pos, &output_start, &output_cirbuf_size);
	if (ret)
		return -ENODATA;


	/* Test that sink has enough free frames. Then run once to maintain
	 * low latency and steady load for tones.
	 */
	if (output_frames * output_frame_bytes >= mod->period_bytes) {
		uint32_t frames = mod->period_bytes / output_frame_bytes;

		/* create tone */
		ret = cd->tone_func(mod, sinks[0], frames, output_pos, output_start,
				    output_cirbuf_size);

		/* calc new free */
		sink_commit_buffer(sink, mod->period_bytes);
	}

	return ret;
}

static int tone_prepare(struct processing_module *mod, struct sof_source **sources,
			int num_of_sources, struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int32_t f;
	int32_t a;
	int ret;
	int i;

	comp_info(dev, "tone_prepare()");

	ret = tone_params(mod);
	if (ret < 0)
		return ret;

	cd->channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	comp_info(dev, "tone_prepare(), cd->channels = %u, cd->rate = %u",
		  cd->channels, cd->rate);

	for (i = 0; i < cd->channels; i++) {
		f = tonegen_get_f(&cd->sg[i]);
		a = tonegen_get_a(&cd->sg[i]);
		if (tonegen_init(&cd->sg[i], cd->rate, f, a) < 0)
			return -EINVAL;
	}

	return 0;
}

static int tone_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	int i;

	comp_info(mod->dev, "tone_reset()");

	/* Initialize with the defaults */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		tonegen_reset(&cd->sg[i]);

	return 0;
}

static const struct module_interface tone_interface = {
	.init = tone_init,
	.prepare = tone_prepare,
	.process = tone_process,
	.reset = tone_reset,
	.free = tone_free,
};

#if CONFIG_COMP_TONE_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest[] __section(".module") __used =
{
	SOF_LLEXT_MODULE_MANIFEST("TONE", &tone_interface, 1, SOF_REG_UUID(tone), 30),
};

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(tone_interface, tone_uuid, tone_tr);
SOF_MODULE_INIT(tone, sys_comp_module_tone_interface_init);

#endif
