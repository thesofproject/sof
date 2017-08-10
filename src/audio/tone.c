/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/work.h>
#include <reef/clock.h>
#include <reef/audio/component.h>
#include <reef/audio/format.h>
#include <reef/audio/pipeline.h>
#include <reef/math/trig.h>
#include <uapi/ipc.h>
#include "tone.h"

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#define trace_tone(__e) trace_event(TRACE_CLASS_TONE, __e)
#define tracev_tone(__e) tracev_event(TRACE_CLASS_TONE, __e)
#define trace_tone_error(__e) trace_error(TRACE_CLASS_TONE, __e)

#define TONE_NUM_FS            13       /* Table size for 8-192 kHz range */
#define TONE_AMPLITUDE_DEFAULT MINUS_60DB_Q1_31  /* -60 dB */
#define TONE_FREQUENCY_DEFAULT TONE_FREQ(997.0)    /* 997 Hz */

static int32_t tonegen(struct tone_state *sg);
static void tonegen_control(struct tone_state *sg);
static void tonegen_update_f(struct tone_state *sg, int32_t f);


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

/* TODO: Remove *source when internal endpoint is possible */
struct comp_data {
	uint32_t period_bytes;
	uint32_t channels;
	uint32_t frame_bytes;
	uint32_t rate;
	struct tone_state sg;
	void (*tone_func)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source, uint32_t frames);
};

/*
 * Tone generator algorithm code
 */

static void tone_s32_default(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t sine_sample;
	int32_t *dest = (int32_t*) sink->w_ptr;
	int i, n, n_wrap_dest;
	int nch = cd->channels;

	n = frames * nch;
	while (n > 0) {
		n_wrap_dest = (int32_t *) sink->end_addr - dest;
		if (n < n_wrap_dest) {
			/* No circular wrap need */
			while (n > 0) {
				/* Update period count for sweeps, etc. */
				tonegen_control(&cd->sg);
				/* Calculate mono sine wave sample and then
				 * duplicate to channels.
				 */
				sine_sample = tonegen(&cd->sg);
				n -= nch;
				for (i = 0; i < nch; i++) {
					*dest = sine_sample;
					dest++;
				}
			}
		} else {
			/* Process until wrap */
			while (n_wrap_dest > 0) {
				tonegen_control(&cd->sg);
				sine_sample = tonegen(&cd->sg);
				n -= nch;
				n_wrap_dest -= nch;
				for (i = 0; i < nch; i++) {
					*dest = sine_sample;
					dest++;
				}
			}
			/* No need to check if past end_addr,
			 * it is so just subtract buffer size.
			 */
			dest = (int32_t *) ((size_t) dest
				- sink->ipc_buffer.size);
		}
	}
	sink->w_ptr = dest;
}

static int32_t tonegen(struct tone_state *sg)
{
	int64_t sine, w;

	/* sg->w is angle in Q4.28 radians format, sin() returns Q1.31 */
	/* sg->a is amplitude as Q1.31 */
	sine = q_mults_32x32(sin_fixed(sg->w), sg->a, 31, 31, 31);

	/* Next point */
	w = (int64_t) sg->w + sg->w_step;
	sg->w = (w > PI_MUL2_Q4_28)
		? (int32_t) (w - PI_MUL2_Q4_28) : (int32_t) w;

	if (sg->mute)
		return 0;
	else
		return(int32_t) sine; /* Q1.31 no saturation need */
}

static void tonegen_control(struct tone_state *sg)
{
	int64_t a, p;

	/* Count samples, 125 us blocks */
	sg->sample_count++;
	if (sg->sample_count < sg->samples_in_block)
		return;

	sg->sample_count = 0;
	if (sg->block_count < INT32_MAXVALUE)
		sg->block_count++;

	/* Fadein ramp during tone */
	if (sg->block_count < sg->tone_length) {
		if (sg->a == 0)
			sg->w = 0; /* Reset phase to have less clicky ramp */

		if (sg->a > sg->a_target) {
			a = (int64_t) sg->a - sg->ramp_step;
			if (a < sg->a_target)
				a = sg->a_target;

		} else {
			a = (int64_t) sg->a + sg->ramp_step;
			if (a > sg->a_target)
				a = sg->a_target;
		}
		sg->a = (int32_t) a;
	}

	/* Fadeout ramp after tone*/
	if (sg->block_count > sg->tone_length) {
		a = (int64_t) sg->a - sg->ramp_step;
		if (a < 0)
			a = 0;

		sg->a = (int32_t) a;
	}

	/* New repeated tone, update for frequency or amplitude sweep */
	if ((sg->block_count > sg->tone_period)
		&& (sg->repeat_count + 1 < sg->repeats)) {
		sg->block_count = 0;
		if (sg->ampl_coef > 0) {
			sg->a_target = sat_int32(q_multsr_32x32(
				sg->a_target,
				sg->ampl_coef, 31, 30, 31));
			sg->a = (sg->ramp_step > sg->a_target)
				? sg->a_target : sg->ramp_step;
		}
		if (sg->freq_coef > 0) {
			/* f is Q16.16, freq_coef is Q2.30 */
			p = q_multsr_32x32(sg->f, sg->freq_coef, 16, 30, 16);
			tonegen_update_f(sg, (int32_t) p); /* No saturation */
		}
		sg->repeat_count++;
	}
}

static inline void tonegen_set_a(struct tone_state *sg, int32_t a)
{
	sg->a_target = a;
}

static inline void tonegen_set_f(struct tone_state *sg, int32_t f)
{
	sg->f = f;
}

/* Tone sweep parameters description:
 * fc - Multiplication factor for frequency as Q2.30 for logarithmic change
 * ac - Multiplication factor for amplitude as Q2.30 for logarithmic change
 * l - Tone length in samples, this is the active length of tone
 * p - Tone period in samples, this is the length including the pause after beep
 * r - Repeated number of beeps
 */

static void tonegen_set_sweep(struct tone_state *sg, int32_t fc, int32_t ac,
	uint32_t l, uint32_t p, uint32_t r)
{
	sg->repeats = r;

	/* Zeros as defaults make a nicer API without need to remember
	 * the neutral settings for sweep and repeat parameters.
	 */
	sg->freq_coef = (fc > 0) ? fc : ONE_Q2_30; /* Set freq mult to 1.0 */
	sg->ampl_coef = (ac > 0) ? ac : ONE_Q2_30; /* Set ampl mult to 1.0 */
	sg->tone_length = (l > 0) ? l : INT32_MAXVALUE; /* Count rate 125 us */
	sg->tone_period = (p > 0) ? p : INT32_MAXVALUE; /* Count rate 125 us */
}

/* Tone ramp parameters:
 * step - Value that is added or subtracted to amplitude. A zero or negative
 *        number disables the ramp and amplitude is immediately modified to
 *        final value.
 */

static inline void tonegen_set_linramp(struct tone_state *sg, int32_t step)
{
	sg->ramp_step = (step > 0) ? step : INT32_MAXVALUE;
}

static inline int32_t tonegen_get_f(struct tone_state *sg)
{
	return sg->f;
}

static inline int32_t tonegen_get_a(struct tone_state *sg)
{
	return sg->a_target;
}

static inline void tonegen_mute(struct tone_state *sg)
{
	sg->mute = 1;
}

static inline void tonegen_unmute(struct tone_state *sg)
{
	sg->mute = 0;
}

static void tonegen_update_f(struct tone_state *sg, int32_t f)
{
	int64_t w_tmp;
	int64_t f_max;

	/* Calculate Fs/2, fs is Q32.0, f is Q16.16 */
	f_max = Q_SHIFT_LEFT((int64_t) sg->fs, 0, 16 - 1);
	f_max = (f_max > INT32_MAXVALUE) ? INT32_MAXVALUE : f_max;
	sg->f = (f > f_max) ? f_max : f;
	w_tmp = q_multsr_32x32(sg->f, sg->c, 16, 31, 28); /* Q16 x Q31 -> Q28 */
	w_tmp = (w_tmp > PI_Q4_28) ? PI_Q4_28 : w_tmp; /* Limit to pi Q4.28 */
	sg->w_step = (int32_t) w_tmp;

#ifdef MODULE_TEST
	printf("Fs=%d, f_max=%d, f_new=%.3f\n",
		sg->fs, (int32_t) (f_max >> 16), sg->f / 65536.0);
#endif
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
	sg->tone_length = INT32_MAXVALUE;
	sg->tone_period = INT32_MAXVALUE;
	sg->ramp_step = ONE_Q1_31; /* Set lin ramp modification to max */
}

static int tonegen_init(struct tone_state *sg, int32_t fs, int32_t f, int32_t a)
{
	int idx, i;

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
	sg->samples_in_block = (int32_t) q_multsr_32x32(fs, 268435, 0, 31, 0);

	return 0;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static struct comp_dev *tone_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;

	trace_tone("new");
	dev = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	//memcpy(&dev->comp, comp, sizeof(struct sof_ipc_comp_tone));


	cd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*cd));
	if (cd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	cd->tone_func = tone_s32_default;

	/* Reset tone generator and set channels volumes to default */
	tonegen_reset(&cd->sg);

	return dev;
}

static void tone_free(struct comp_dev *dev)
{
	struct tone_data *td = comp_get_drvdata(dev);

	trace_tone("fre");

	rfree(td);
	rfree(dev);
}

/* set component audio stream parameters */
static int tone_params(struct comp_dev *dev, struct stream_params *host_params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_stream_params *params = &dev->params;
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);

	trace_tone("par");

	comp_install_params(dev, host_params);

	/* calculate period size based on config */
	cd->period_bytes = config->frames * config->frame_size;

	/* EQ supports only S32_LE PCM format */
	if (params->frame_fmt != SOF_IPC_FRAME_S32_LE)
		return -EINVAL;

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int tone_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_tone *ct;

	trace_tone("tri");

	switch (cmd) {
	case COMP_CMD_TONE:
		trace_tone("Tto");
		ct = (struct sof_ipc_comp_tone *) data;
		/* Ignore channels while tone implementation is mono */
		tonegen_set_f(&cd->sg, ct->frequency);
		tonegen_set_a(&cd->sg, ct->amplitude);
		tonegen_set_sweep(&cd->sg, ct->freq_mult, ct->ampl_mult,
			ct->length, ct->period, ct->repeats);
		tonegen_set_linramp(&cd->sg, ct->ramp_step);
		break;
	case COMP_CMD_MUTE:
		trace_tone("TMu");
		tonegen_mute(&cd->sg);
		break;
	case COMP_CMD_UNMUTE:
		trace_tone("TUm");
		tonegen_unmute(&cd->sg);
		break;
	case COMP_CMD_START:
		trace_tone("TSt");
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_STOP:
		trace_tone("TSp");
		if (dev->state == COMP_STATE_RUNNING ||
			dev->state == COMP_STATE_DRAINING ||
			dev->state == COMP_STATE_PAUSED) {
			comp_buffer_reset(dev);
			dev->state = COMP_STATE_SETUP;
		}
		break;
	case COMP_CMD_PAUSE:
		trace_tone("TPe");
		/* only support pausing for running */
		if (dev->state == COMP_STATE_RUNNING)
			dev->state = COMP_STATE_PAUSED;

		break;
	case COMP_CMD_RELEASE:
		trace_tone("TRl");
		dev->state = COMP_STATE_RUNNING;
		break;
	default:
		trace_tone("TDf");

		break;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int tone_copy(struct comp_dev *dev)
{
	struct comp_buffer *sink;
	struct comp_buffer *source = NULL;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);

	trace_comp("cpy");

	/* tone component sink buffer */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);

	/* Test that sink has enough free frames. Then run once to maintain
	 * low latency and steady load for tones.
	 */
	if (sink->free >= cd->period_bytes) {
		/* create tone */
		cd->tone_func(dev, sink, source, config->frames);
	}

	comp_update_buffer_produce(sink, cd->period_bytes);

	return config->frames;
}

static int tone_prepare(struct comp_dev *dev)
{
	int32_t f, a;
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_tone("TPp");
	tracev_value(cd->channels);
	tracev_value(cd->rate);

	f = tonegen_get_f(&cd->sg);
	a = tonegen_get_a(&cd->sg);
	if (tonegen_init(&cd->sg, cd->rate, f, a) < 0)
		return -EINVAL;

	//dev->preload = PLAT_INT_PERIODS;
	dev->state = COMP_STATE_PREPARE;

	return 0;
}

static int tone_preload(struct comp_dev *dev)
{
	//int i;
	trace_tone("TPl");

	//for (i = 0; i < dev->preload; i++)
	//	tone_copy(dev);

	return 0;
}

static int tone_reset(struct comp_dev *dev)
{

	struct comp_data *cd = comp_get_drvdata(dev);

	trace_tone("TRe");

	/* Initialize with the defaults */
	tonegen_reset(&cd->sg);

	dev->state = COMP_STATE_INIT;

	return 0;
}

struct comp_driver comp_tone = {
	.type = SOF_COMP_TONE,
	.ops =
	{
		.new = tone_new,
		.free = tone_free,
		.params = tone_params,
		.cmd = tone_cmd,
		.copy = tone_copy,
		.prepare = tone_prepare,
		.reset = tone_reset,
		.preload = tone_preload,
	},
};

void sys_comp_tone_init(void)
{
	comp_register(&comp_tone);
}
