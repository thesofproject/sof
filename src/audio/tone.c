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
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/trig.h>
#include <sof/platform.h>
#include <sof/string.h>
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

/* Convert float frequency in Hz to Q16.16 fractional format */
#define TONE_FREQ(f) Q_CONVERT_FLOAT(f, 16)

/* Convert float gain to Q1.31 fractional format */
#define TONE_GAIN(v) Q_CONVERT_FLOAT(v, 31)

/* Set default tone amplitude and frequency */
#define TONE_AMPLITUDE_DEFAULT TONE_GAIN(0.1)      /*  -20 dB  */
#define TONE_FREQUENCY_DEFAULT TONE_FREQ(997.0)
#define TONE_NUM_FS            13       /* Table size for 8-192 kHz range */

static const struct comp_driver comp_tone;

/* 04e3f894-2c5c-4f2e-8dc1-694eeaab53fa */
DECLARE_SOF_RT_UUID("tone", tone_uuid, 0x04e3f894, 0x2c5c, 0x4f2e,
		 0x8d, 0xc1, 0x69, 0x4e, 0xea, 0xab, 0x53, 0xfa);

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
	uint32_t period_bytes;
	uint32_t channels;
	uint32_t frame_bytes;
	uint32_t rate;
	struct tone_state sg[PLATFORM_MAX_CHANNELS];
	void (*tone_func)(struct comp_dev *dev, struct audio_stream *sink,
			  uint32_t frames);
};

static int32_t tonegen(struct tone_state *sg);
static void tonegen_control(struct tone_state *sg);
static void tonegen_update_f(struct tone_state *sg, int32_t f);

/*
 * Tone generator algorithm code
 */

static inline void tone_circ_inc_wrap(int32_t **ptr, int32_t *end, size_t size)
{
	if (*ptr >= end)
		*ptr = (int32_t *)((size_t)*ptr - size);
}

static void tone_s32_default(struct comp_dev *dev, struct audio_stream *sink,
			     uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *dest = (int32_t *)sink->w_ptr;
	int i;
	int n;
	int n_wrap_dest;
	int n_min;
	int nch = cd->channels;

	n = frames * nch;
	while (n > 0) {
		n_wrap_dest = (int32_t *)sink->end_addr - dest;
		n_min = (n < n_wrap_dest) ? n : n_wrap_dest;
		/* Process until wrap or completed n */
		while (n_min > 0) {
			n -= nch;
			n_min -= nch;
			for (i = 0; i < nch; i++) {
				tonegen_control(&cd->sg[i]);
				*dest = tonegen(&cd->sg[i]);
				dest++;
			}
		}
		tone_circ_inc_wrap(&dest, sink->end_addr, sink->size);
	}
}

static int32_t tonegen(struct tone_state *sg)
{
	int64_t sine;
	int64_t w;

	/* sg->w is angle in Q4.28 radians format, sin() returns Q1.31 */
	/* sg->a is amplitude as Q1.31 */
	sine =
		q_mults_32x32(sin_fixed(sg->w), sg->a,
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

/* Set sine amplitude */
static inline void tonegen_set_a(struct tone_state *sg, int32_t a)
{
	sg->a_target = a;
}

/* Repeated number of beeps */
static void tonegen_set_repeats(struct tone_state *sg, uint32_t r)
{
	sg->repeats = r;
}

/* The next functions support zero as shortcut for defaults to get
 * make a nicer API without need to remember the neutral steady
 * non-swept tone settings.
 */

/* Multiplication factor for frequency as Q2.30 for logarithmic change */
static void tonegen_set_freq_mult(struct tone_state *sg, int32_t fm)
{
	sg->freq_coef = (fm > 0) ? fm : ONE_Q2_30; /* Set freq mult to 1.0 */
}

/* Multiplication factor for amplitude as Q2.30 for logarithmic change */
static void tonegen_set_ampl_mult(struct tone_state *sg, int32_t am)
{
	sg->ampl_coef = (am > 0) ? am : ONE_Q2_30; /* Set ampl mult to 1.0 */
}

/* Tone length in samples, this is the active length of tone */
static void tonegen_set_length(struct tone_state *sg, uint32_t tl)
{
	sg->tone_length = (tl > 0) ? tl : INT32_MAX; /* Count rate 125 us */
}

/* Tone period in samples, this is the length including the pause after beep */
static void tonegen_set_period(struct tone_state *sg, uint32_t tp)
{
	sg->tone_period = (tp > 0) ? tp : INT32_MAX; /* Count rate 125 us */
}

/* Tone ramp parameters:
 * step - Value that is added or subtracted to amplitude. A zero or negative
 *        number disables the ramp and amplitude is immediately modified to
 *        final value.
 */

static inline void tonegen_set_linramp(struct tone_state *sg, int32_t step)
{
	sg->ramp_step = (step > 0) ? step : INT32_MAX;
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

static struct comp_dev *tone_new(const struct comp_driver *drv,
				 struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_tone *tone;
	struct sof_ipc_comp_tone *ipc_tone = (struct sof_ipc_comp_tone *)comp;
	struct comp_data *cd;
	int i;
	int ret;

	comp_cl_info(&comp_tone, "tone_new()");

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_tone));
	if (!dev)
		return NULL;

	tone = COMP_GET_IPC(dev, sof_ipc_comp_tone);
	ret = memcpy_s(tone, sizeof(*tone), ipc_tone,
		       sizeof(struct sof_ipc_comp_tone));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	cd->tone_func = tone_s32_default;

	cd->rate = ipc_tone->sample_rate;

	/* Reset tone generator and set channels volumes to default */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		tonegen_reset(&cd->sg[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void tone_free(struct comp_dev *dev)
{
	struct tone_data *td = comp_get_drvdata(dev);

	comp_info(dev, "tone_free()");

	rfree(td);
	rfree(dev);
}

/* set component audio stream parameters */
static int tone_params(struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t flags = 0;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	comp_info(dev, "tone_params(), config->frame_fmt = %u",
		  config->frame_fmt);

	/* Tone supports only S32_LE PCM format atm */
	if (config->frame_fmt != SOF_IPC_FRAME_S32_LE)
		return -EINVAL;

	buffer_lock(sourceb, &flags);
	buffer_lock(sinkb, &flags);

	sourceb->stream.frame_fmt = config->frame_fmt;
	sinkb->stream.frame_fmt = config->frame_fmt;

	/* calculate period size based on config */
	cd->period_bytes = dev->frames *
			   audio_stream_frame_bytes(&sourceb->stream);

	buffer_unlock(sinkb, flags);
	buffer_unlock(sourceb, flags);

	return 0;
}

static int tone_cmd_get_value(struct comp_dev *dev,
			      struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;

	comp_info(dev, "tone_cmd_get_value()");

	if (cdata->cmd == SOF_CTRL_CMD_SWITCH) {
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = !cd->sg[j].mute;
			comp_info(dev, "tone_cmd_get_value(), j = %u, cd->sg[j].mute = %u",
				  j, cd->sg[j].mute);
		}
	}
	return 0;
}

static int tone_cmd_set_value(struct comp_dev *dev,
			      struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;
	uint32_t ch;
	bool val;

	if (cdata->cmd == SOF_CTRL_CMD_SWITCH) {
		comp_info(dev, "tone_cmd_set_value(), SOF_CTRL_CMD_SWITCH");
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			comp_info(dev, "tone_cmd_set_value(), SOF_CTRL_CMD_SWITCH, ch = %u, val = %u",
				  ch, val);
			if (ch >= PLATFORM_MAX_CHANNELS) {
				comp_err(dev, "tone_cmd_set_value(): ch >= PLATFORM_MAX_CHANNELS");
				return -EINVAL;
			}

			if (val)
				tonegen_unmute(&cd->sg[ch]);
			else
				tonegen_mute(&cd->sg[ch]);
		}
	} else {
		comp_err(dev, "tone_cmd_set_value(): invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

static int tone_cmd_set_data(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_ctrl_value_comp *compv;
	int i;
	uint32_t ch;
	uint32_t val;

	comp_info(dev, "tone_cmd_set_data()");

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_info(dev, "tone_cmd_set_data(), SOF_CTRL_CMD_ENUM, cdata->index = %u",
			  cdata->index);
		compv = (struct sof_ipc_ctrl_value_comp *)ASSUME_ALIGNED(cdata->data->data, 4);

		for (i = 0; i < (int)cdata->num_elems; i++) {
			ch = compv[i].index;
			val = compv[i].svalue;
			comp_info(dev, "tone_cmd_set_data(), SOF_CTRL_CMD_ENUM, ch = %u, val = %u",
				  ch, val);
			switch (cdata->index) {
			case SOF_TONE_IDX_FREQUENCY:
				comp_info(dev, "tone_cmd_set_data(), SOF_TONE_IDX_FREQUENCY");
				tonegen_update_f(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_AMPLITUDE:
				comp_info(dev, "tone_cmd_set_data(), SOF_TONE_IDX_AMPLITUDE");
				tonegen_set_a(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_FREQ_MULT:
				comp_info(dev, "tone_cmd_set_data(), SOF_TONE_IDX_FREQ_MULT");
				tonegen_set_freq_mult(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_AMPL_MULT:
				comp_info(dev, "tone_cmd_set_data(), SOF_TONE_IDX_AMPL_MULT");
				tonegen_set_ampl_mult(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_LENGTH:
				comp_info(dev, "tone_cmd_set_data(), SOF_TONE_IDX_LENGTH");
				tonegen_set_length(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_PERIOD:
				comp_info(dev, "tone_cmd_set_data(), SOF_TONE_IDX_PERIOD");
				tonegen_set_period(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_REPEATS:
				comp_info(dev, "tone_cmd_set_data(), SOF_TONE_IDX_REPEATS");
				tonegen_set_repeats(&cd->sg[ch], val);
				break;
			case SOF_TONE_IDX_LIN_RAMP_STEP:
				comp_info(dev, "tone_cmd_set_data(), SOF_TONE_IDX_LIN_RAMP_STEP");
				tonegen_set_linramp(&cd->sg[ch], val);
				break;
			default:
				comp_err(dev, "tone_cmd_set_data(): invalid cdata->index");
				return -EINVAL;
			}
		}
		break;
	default:
		comp_err(dev, "tone_cmd_set_data(): invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int tone_cmd(struct comp_dev *dev, int cmd, void *data,
		    int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "tone_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = tone_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_SET_VALUE:
		ret = tone_cmd_set_value(dev, cdata);
		break;
	case COMP_CMD_GET_VALUE:
		ret = tone_cmd_get_value(dev, cdata, max_data_size);
		break;
	}

	return ret;
}

static int tone_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "tone_trigger()");

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int tone_copy(struct comp_dev *dev)
{
	struct comp_buffer *sink;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t free;
	uint32_t flags = 0;

	comp_dbg(dev, "tone_copy()");

	/* tone component sink buffer */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	buffer_lock(sink, &flags);
	free = audio_stream_get_free_bytes(&sink->stream);
	buffer_unlock(sink, flags);

	/* Test that sink has enough free frames. Then run once to maintain
	 * low latency and steady load for tones.
	 */
	if (free >= cd->period_bytes) {
		/* create tone */
		cd->tone_func(dev, &sink->stream, dev->frames);
		buffer_writeback(sink, cd->period_bytes);

		/* calc new free and available */
		comp_update_buffer_produce(sink, cd->period_bytes);

		return dev->frames;
	}

	return 0;
}

static int tone_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	int32_t f;
	int32_t a;
	int ret;
	int i;

	comp_info(dev, "tone_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	cd->channels = sourceb->stream.channels;
	comp_info(dev, "tone_prepare(), cd->channels = %u, cd->rate = %u",
		  cd->channels, cd->rate);

	for (i = 0; i < cd->channels; i++) {
		f = tonegen_get_f(&cd->sg[i]);
		a = tonegen_get_a(&cd->sg[i]);
		if (tonegen_init(&cd->sg[i], cd->rate, f, a) < 0) {
			comp_set_state(dev, COMP_TRIGGER_RESET);
			return -EINVAL;
		}
	}

	return 0;
}

static int tone_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i;

	comp_info(dev, "tone_reset()");

	/* Initialize with the defaults */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		tonegen_reset(&cd->sg[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static const struct comp_driver comp_tone = {
	.type = SOF_COMP_TONE,
	.uid = SOF_RT_UUID(tone_uuid),
	.tctx = &tone_tr,
	.ops = {
		.create = tone_new,
		.free = tone_free,
		.params = tone_params,
		.cmd = tone_cmd,
		.trigger = tone_trigger,
		.copy = tone_copy,
		.prepare = tone_prepare,
		.reset = tone_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_tone_info = {
	.drv = &comp_tone,
};

UT_STATIC void sys_comp_tone_init(void)
{
	comp_register(platform_shared_get(&comp_tone_info,
					  sizeof(comp_tone_info)));
}

DECLARE_MODULE(sys_comp_tone_init);
