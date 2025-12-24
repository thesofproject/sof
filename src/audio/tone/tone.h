/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
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

/* Convert float frequency in Hz to Q16.16 fractional format */
#define TONE_FREQ(f) Q_CONVERT_FLOAT(f, 16)

/* Convert float gain to Q1.31 fractional format */
#define TONE_GAIN(v) Q_CONVERT_FLOAT(v, 31)

/* Set default tone amplitude and frequency */
#define TONE_AMPLITUDE_DEFAULT TONE_GAIN(0.1)      /*  -20 dB  */
#define TONE_FREQUENCY_DEFAULT TONE_FREQ(997.0)
#define TONE_NUM_FS            13       /* Table size for 8-192 kHz range */

#define TONE_MODE_TONEGEN	0
#define TONE_MODE_PASSTHROUGH	1
#define TONE_MODE_SILENCE	2

extern struct tr_ctx tone_tr;
extern const struct sof_uuid tone;

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
			 struct sof_source *source);
	int mode;
};

void tonegen_reset(struct tone_state *sg);
int tonegen_init(struct tone_state *sg, int32_t fs, int32_t f, int32_t a);
void tonegen_update_f(struct tone_state *sg, int32_t f);
int tone_s32_default(struct processing_module *mod, struct sof_sink *sink,
		     struct sof_source *source);

/* Set sine amplitude */
static inline void tonegen_set_a(struct tone_state *sg, int32_t a)
{
	sg->a_target = a;
}

/* Repeated number of beeps */
static inline void tonegen_set_repeats(struct tone_state *sg, uint32_t r)
{
	sg->repeats = r;
}

/* The next functions support zero as shortcut for defaults to get
 * make a nicer API without need to remember the neutral steady
 * non-swept tone settings.
 */

/* Multiplication factor for frequency as Q2.30 for logarithmic change */
static inline void tonegen_set_freq_mult(struct tone_state *sg, int32_t fm)
{
	sg->freq_coef = (fm > 0) ? fm : ONE_Q2_30; /* Set freq mult to 1.0 */
}

/* Multiplication factor for amplitude as Q2.30 for logarithmic change */
static inline void tonegen_set_ampl_mult(struct tone_state *sg, int32_t am)
{
	sg->ampl_coef = (am > 0) ? am : ONE_Q2_30; /* Set ampl mult to 1.0 */
}

/* Tone length in samples, this is the active length of tone */
static inline void tonegen_set_length(struct tone_state *sg, uint32_t tl)
{
	sg->tone_length = (tl > 0) ? tl : INT32_MAX; /* Count rate 125 us */
}

/* Tone period in samples, this is the length including the pause after beep */
static inline void tonegen_set_period(struct tone_state *sg, uint32_t tp)
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
