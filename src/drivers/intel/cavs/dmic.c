/*
 * Copyright (c) 2017, Intel Corporation
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
 * Author:	Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <sof/stream.h>
#include <sof/dmic.h>
#include <sof/interrupt.h>
#include <sof/math/numbers.h>
#include <sof/audio/format.h>

#if defined DMIC_HW_VERSION

#include <sof/audio/coefficients/pdm_decim/pdm_decim_table.h>

#if defined MODULE_TEST
#include <stdio.h>
#endif

#define DMIC_MAX_MODES 50

/* HW FIR pipeline needs 5 additional cycles per channel for internal
 * operations. This is used in MAX filter length check.
 */
#define DMIC_FIR_PIPELINE_OVERHEAD 5

struct decim_modes {
	int16_t clkdiv[DMIC_MAX_MODES];
	int16_t mcic[DMIC_MAX_MODES];
	int16_t mfir[DMIC_MAX_MODES];
	int num_of_modes;
};

struct matched_modes {
	int16_t clkdiv[DMIC_MAX_MODES];
	int16_t mcic[DMIC_MAX_MODES];
	int16_t mfir_a[DMIC_MAX_MODES];
	int16_t mfir_b[DMIC_MAX_MODES];
	int num_of_modes;
};

struct dmic_configuration {
	struct pdm_decim *fir_a;
	struct pdm_decim *fir_b;
	int clkdiv;
	int mcic;
	int mfir_a;
	int mfir_b;
	int cic_shift;
	int fir_a_shift;
	int fir_b_shift;
	int fir_a_length;
	int fir_b_length;
	int32_t fir_a_scale;
	int32_t fir_b_scale;
};

struct pdm_controllers_configuration {
	uint32_t cic_control;
	uint32_t cic_config;
	uint32_t mic_control;
	uint32_t fir_control_a;
	uint32_t fir_config_a;
	uint32_t dc_offset_left_a;
	uint32_t dc_offset_right_a;
	uint32_t out_gain_left_a;
	uint32_t out_gain_right_a;
	uint32_t fir_control_b;
	uint32_t fir_config_b;
	uint32_t dc_offset_left_b;
	uint32_t dc_offset_right_b;
	uint32_t out_gain_left_b;
	uint32_t out_gain_right_b;
};

/* Configuration ABI version, increment if not compatible with previous
 * version.
 */
#define DMIC_IPC_VERSION 1

/* Minimum OSR is always applied for 48 kHz and less sample rates */
#define DMIC_MIN_OSR  50

/* These are used as guideline for configuring > 48 kHz sample rates. The
 * minimum OSR can be relaxed down to 40 (use 3.84 MHz clock for 96 kHz).
 */
#define DMIC_HIGH_RATE_MIN_FS	64000
#define DMIC_HIGH_RATE_OSR_MIN	40

/* Used for scaling FIR coeffcients for HW */
#define DMIC_HW_FIR_COEF_MAX ((1 << (DMIC_HW_BITS_FIR_COEF - 1)) - 1)
#define DMIC_HW_FIR_COEF_Q (DMIC_HW_BITS_FIR_COEF - 1)

/* Internal precision in gains computation, e.g. Q4.28 in int32_t */
#define DMIC_FIR_SCALE_Q 28

/* Used in unmute ramp values calculation */
#define DMIC_HW_FIR_GAIN_MAX ((1 << (DMIC_HW_BITS_FIR_GAIN - 1)) - 1)

/* Hardwired log ramp parameters. The first value is the initial logarithmic
 * gain and the second value is the multiplier for gain to achieve the linear
 * decibels change over time. Currently the coefficient GM needs to be
 * calculated manually. The 300 ms ramp should ensure clean sounding start with
 * any microphone. However it is likely unnecessarily long for machine hearing.
 * TODO: Add ramp characteristic passing via topology.
 */
#define LOGRAMP_GI 33954 /* -90 dB, Q2.30*/
#define LOGRAMP_GM 16959 /* Gives 300 ms ramp for -90..0 dB, Q2.14 */

/* tracing */
#define trace_dmic(__e) trace_event(TRACE_CLASS_DMIC, __e)
#define trace_dmic_error(__e) trace_error(TRACE_CLASS_DMIC, __e)
#define tracev_dmic(__e) tracev_event(TRACE_CLASS_DMIC, __e)

/* Base addresses (in PDM scope) of 2ch PDM controllers and coefficient RAM. */
static const uint32_t base[4] = {PDM0, PDM1, PDM2, PDM3};
static const uint32_t coef_base_a[4] = {PDM0_COEFFICIENT_A, PDM1_COEFFICIENT_A,
	PDM2_COEFFICIENT_A, PDM3_COEFFICIENT_A};
static const uint32_t coef_base_b[4] = {PDM0_COEFFICIENT_B, PDM1_COEFFICIENT_B,
	PDM2_COEFFICIENT_B, PDM3_COEFFICIENT_B};

#if defined MODULE_TEST
#define IO_BYTES_GLOBAL  (PDM0 - OUTCONTROL0)
#define IO_BYTES_MIDDLE  (PDM1 - PDM0)
#define IO_BYTES_LAST    (PDM0_COEFFICIENT_B + PDM_COEF_RAM_B_LENGTH - PDM0)
#define IO_BYTES  (((DMIC_HW_CONTROLLERS) - 1) * (IO_BYTES_MIDDLE) \
				+ (IO_BYTES_LAST) + (IO_BYTES_GLOBAL))
#define IO_LENGTH ((IO_BYTES) >> 2)

static uint32_t dmic_io[IO_LENGTH];

static void dmic_write(struct dai *dai, uint32_t reg, uint32_t value)
{
	printf("W %04x %08x\n", reg, value);
	dmic_io[reg >> 2] = value;
}

static uint32_t dmic_read(struct dai *dai, uint32_t reg)
{
	uint32_t value = dmic_io[reg >> 2];

	printf("R %04x %08x\n", reg, value);
	return value;
}

static void dmic_update_bits(struct dai *dai, uint32_t reg, uint32_t mask,
	uint32_t value)
{
	uint32_t new_value;
	uint32_t old_value = dmic_io[reg >> 2];

	new_value = (old_value & (~mask)) | value;
	dmic_io[reg >> 2] = new_value;
	printf("W %04x %08x\n", reg, new_value);
}
#else

static void dmic_write(struct dai *dai, uint32_t reg, uint32_t value)
{
	io_reg_write(dai_base(dai) + reg, value);
}

static uint32_t dmic_read(struct dai *dai, uint32_t reg)
{
	return io_reg_read(dai_base(dai) + reg);
}

static void dmic_update_bits(struct dai *dai, uint32_t reg, uint32_t mask,
	uint32_t value)
{
	io_reg_update_bits(dai_base(dai) + reg, mask, value);
}
#endif

/* this ramps volume changes over time */
static uint64_t dmic_work(void *data, uint64_t delay)
{
	struct dai *dai = (struct dai *)data;
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int32_t gval;
	uint32_t val;
	int i;

	tracev_dmic("wrk");
	spin_lock(&dmic->lock);

	/* Increment gain with logaritmic step.
	 * Gain is Q2.30 and gain modifier is Q2.14.
	 */
	dmic->startcount++;
	dmic->gain = Q_MULTSR_32X32((int64_t)dmic->gain,
				    LOGRAMP_GM, 30, 14, 30);

	/* Gain is stored as Q2.30, while HW register is Q1.19 so shift
	 * the value right by 11.
	 */
	gval = dmic->gain >> 11;

	/* Note that DMIC gain value zero has a special purpose. Value zero
	 * sets gain bypass mode in HW. Zero value will be applied after ramp
	 * is complete. It is because exact 1.0 gain is not possible with Q1.19.
	 */
	if (gval > DMIC_HW_FIR_GAIN_MAX)
		gval = 0;

	/* Write gain to registers */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (!dmic->enable[i])
			continue;

		if (dmic->startcount == DMIC_UNMUTE_CIC)
			dmic_update_bits(dai, base[i] + CIC_CONTROL,
					 CIC_CONTROL_MIC_MUTE_BIT, 0);

		if (dmic->startcount == DMIC_UNMUTE_FIR) {
			if (dmic->fifo_a)
				dmic_update_bits(dai, base[i] + FIR_CONTROL_A,
						 FIR_CONTROL_A_MUTE_BIT, 0);

			if (dmic->fifo_b)
				dmic_update_bits(dai, base[i] + FIR_CONTROL_B,
						 FIR_CONTROL_B_MUTE_BIT, 0);
		}
		if (dmic->fifo_a) {
			val = OUT_GAIN_LEFT_A_GAIN(gval);
			dmic_write(dai, base[i] + OUT_GAIN_LEFT_A, val);
			dmic_write(dai, base[i] + OUT_GAIN_RIGHT_A, val);
		}
		if (dmic->fifo_b) {
			val = OUT_GAIN_LEFT_B_GAIN(gval);
			dmic_write(dai, base[i] + OUT_GAIN_LEFT_B, val);
			dmic_write(dai, base[i] + OUT_GAIN_RIGHT_B, val);
		}
	}
	spin_unlock(&dmic->lock);

	if (gval)
		return DMIC_UNMUTE_RAMP_US;
	else
		return 0;
}

/* This function returns a raw list of potential microphone clock and decimation
 * modes for achieving requested sample rates. The search is constrained by
 * decimation HW capabililies and setup parameters. The parameters such as
 * microphone clock min/max and duty cycle requirements need be checked from
 * used microphone component datasheet.
 */
static void find_modes(struct decim_modes *modes,
	struct sof_ipc_dai_dmic_params *prm, uint32_t fs)
{
	int clkdiv_min;
	int clkdiv_max;
	int clkdiv;
	int c1;
	int du_min;
	int du_max;
	int pdmclk;
	int osr;
	int mfir;
	int mcic;
	int ioclk_test;
	int osr_min = DMIC_MIN_OSR;
	int i = 0;

	/* Defaults, empty result */
	modes->num_of_modes = 0;

	/* The FIFO is not requested if sample rate is set to zero. Just
	 * return in such case with num_of_modes as zero.
	 */
	if (fs == 0)
		return;

	/* Override DMIC_MIN_OSR for very high sample rates, use as minimum
	 * the nominal clock for the high rates.
	 */
	if (fs >= DMIC_HIGH_RATE_MIN_FS)
		osr_min = DMIC_HIGH_RATE_OSR_MIN;

	/* Check for sane pdm clock, min 100 kHz, max ioclk/2 */
	if (prm->pdmclk_max < DMIC_HW_PDM_CLK_MIN ||
		prm->pdmclk_max > DMIC_HW_IOCLK / 2) {
		trace_dmic_error("pmx");
		return;
	}
	if (prm->pdmclk_min < DMIC_HW_PDM_CLK_MIN ||
		prm->pdmclk_min > prm->pdmclk_max) {
		trace_dmic_error("pmn");
		return;
	}

	/* Check for sane duty cycle */
	if (prm->duty_min > prm->duty_max) {
		trace_dmic_error("pdu");
		return;
	}
	if (prm->duty_min < DMIC_HW_DUTY_MIN ||
		prm->duty_min > DMIC_HW_DUTY_MAX) {
		trace_dmic_error("pdn");
		return;
	}
	if (prm->duty_max < DMIC_HW_DUTY_MIN ||
		prm->duty_max > DMIC_HW_DUTY_MAX) {
		trace_dmic_error("pdx");
		return;
	}

	/* Min and max clock dividers */
	clkdiv_min = ceil_divide(DMIC_HW_IOCLK, prm->pdmclk_max);
	clkdiv_min = MAX(clkdiv_min, DMIC_HW_CIC_DECIM_MIN);
	clkdiv_max = DMIC_HW_IOCLK / prm->pdmclk_min;

	/* Loop possible clock dividers and check based on resulting
	 * oversampling ratio that CIC and FIR decimation ratios are
	 * feasible. The ratios need to be integers. Also the mic clock
	 * duty cycle need to be within limits.
	 */
	for (clkdiv = clkdiv_min; clkdiv <= clkdiv_max; clkdiv++) {
		/* Calculate duty cycle for this clock divider. Note that
		 * odd dividers cause non-50% duty cycle.
		 */
		c1 = clkdiv >> 1;
		du_min = 100 * c1 / clkdiv;
		du_max = 100 - du_min;

		/* Calculate PDM clock rate and oversampling ratio. */
		pdmclk = DMIC_HW_IOCLK / clkdiv;
		osr = pdmclk / fs;

		/* Check that OSR constraints is met and clock duty cycle does
		 * not exceed microphone specification. If exceed proceed to
		 * next clkdiv.
		 */
		if (osr < osr_min || du_min < prm->duty_min ||
			du_max > prm->duty_max)
			continue;

		/* Loop FIR decimation factors candidates. If the
		 * integer divided decimation factors and clock dividers
		 * as multiplied with sample rate match the IO clock
		 * rate the division was exact and such decimation mode
		 * is possible. Then check that CIC decimation constraints
		 * are met. The passed decimation modes are added to array.
		 */
		for (mfir = DMIC_HW_FIR_DECIM_MIN;
			mfir <= DMIC_HW_FIR_DECIM_MAX; mfir++) {
			mcic = osr / mfir;
			ioclk_test = fs * mfir * mcic * clkdiv;

			if (ioclk_test == DMIC_HW_IOCLK &&
				mcic >= DMIC_HW_CIC_DECIM_MIN &&
				mcic <= DMIC_HW_CIC_DECIM_MAX &&
				i < DMIC_MAX_MODES) {
				modes->clkdiv[i] = clkdiv;
				modes->mcic[i] = mcic;
				modes->mfir[i] = mfir;
				i++;
				modes->num_of_modes = i;
			}
		}
	}
#if defined MODULE_TEST
	printf("# Found %d modes\n", i);
#endif
}

/* The previous raw modes list contains sane configuration possibilities. When
 * there is request for both FIFOs A and B operation this function returns
 * list of compatible settings.
 */
static void match_modes(struct matched_modes *c, struct decim_modes *a,
	struct decim_modes *b)
{
	int16_t idx[DMIC_MAX_MODES];
	int idx_length;
	int i;
	int n;
	int m;

	/* Check if previous search got results. */
	c->num_of_modes = 0;
	if (a->num_of_modes == 0 && b->num_of_modes == 0) {
		/* Nothing to do */
		return;
	}

	/* Ensure that num_of_modes is sane. */
	if (a->num_of_modes > DMIC_MAX_MODES ||
		b->num_of_modes > DMIC_MAX_MODES)
		return;

	/* Check for request only for FIFO A or B. In such case pass list for
	 * A or B as such.
	 */
	if (b->num_of_modes == 0) {
		c->num_of_modes = a->num_of_modes;
		for (i = 0; i < a->num_of_modes; i++) {
			c->clkdiv[i] = a->clkdiv[i];
			c->mcic[i] = a->mcic[i];
			c->mfir_a[i] = a->mfir[i];
			c->mfir_b[i] = 0; /* Mark FIR B as non-used */
		}
		return;
	}

	if (a->num_of_modes == 0) {
		c->num_of_modes = b->num_of_modes;
		for (i = 0; i < b->num_of_modes; i++) {
			c->clkdiv[i] = b->clkdiv[i];
			c->mcic[i] = b->mcic[i];
			c->mfir_b[i] = b->mfir[i];
			c->mfir_a[i] = 0; /* Mark FIR A as non-used */
		}
		return;
	}

	/* Merge a list of compatible modes */
	i = 0;
	for (n = 0; n < a->num_of_modes; n++) {
		/* Find all indices of values a->clkdiv[n] in b->clkdiv[] */
		idx_length = find_equal_int16(idx, b->clkdiv, a->clkdiv[n],
			b->num_of_modes, 0);
		for (m = 0; m < idx_length; m++) {
			if (b->mcic[idx[m]] == a->mcic[n]) {
				c->clkdiv[i] = a->clkdiv[n];
				c->mcic[i] = a->mcic[n];
				c->mfir_a[i] = a->mfir[n];
				c->mfir_b[i] = b->mfir[idx[m]];
				i++;
			}
		}
		c->num_of_modes = i;
	}
}

/* Finds a suitable FIR decimation filter from the included set */
static struct pdm_decim *get_fir(struct dmic_configuration *cfg, int mfir)
{
	int i;
	int fs;
	int cic_fs;
	int fir_max_length;
	struct pdm_decim *fir = NULL;

	if (mfir <= 0)
		return fir;

	cic_fs = DMIC_HW_IOCLK / cfg->clkdiv / cfg->mcic;
	fs = cic_fs / mfir;
	/* FIR max. length depends on available cycles and coef RAM
	 * length. Exceeding this length sets HW overrun status and
	 * overwrite of other register.
	 */
	fir_max_length = MIN(DMIC_HW_FIR_LENGTH_MAX,
		DMIC_HW_IOCLK / fs / 2 - DMIC_FIR_PIPELINE_OVERHEAD);

	for (i = 0; i < DMIC_FIR_LIST_LENGTH; i++) {
		if (fir_list[i]->decim_factor == mfir &&
			fir_list[i]->length <= fir_max_length) {
			/* Store pointer, break from loop to avoid a
			 * Possible other mode with lower FIR length.
			 */
			fir = fir_list[i];
			break;
		}
	}

	return fir;
}

/* Calculate scale and shift to use for FIR coefficients. Scale is applied
 * before write to HW coef RAM. Shift will be programmed to HW register.
 */
static int fir_coef_scale(int32_t *fir_scale, int *fir_shift, int add_shift,
	const int32_t coef[], int coef_length, int32_t gain)
{
	int32_t amax;
	int32_t new_amax;
	int32_t fir_gain;
	int shift;

	/* Multiply gain passed from CIC with output full scale. */
	fir_gain = Q_MULTSR_32X32((int64_t)gain, DMIC_HW_SENS_Q28,
		DMIC_FIR_SCALE_Q, 28, DMIC_FIR_SCALE_Q);

	/* Find the largest FIR coefficient value. */
	amax = find_max_abs_int32((int32_t *)coef, coef_length);

	/* Scale max. tap value with FIR gain. */
	new_amax = Q_MULTSR_32X32((int64_t)amax, fir_gain, 31,
		DMIC_FIR_SCALE_Q, DMIC_FIR_SCALE_Q);
	if (new_amax <= 0)
		return -EINVAL;

	/* Get left shifts count to normalize the fractional value as 32 bit.
	 * We need right shifts count for scaling so need to invert. The
	 * difference of Q31 vs. used Q format is added to get the correct
	 * normalization right shift value.
	 */
	shift = 31 - DMIC_FIR_SCALE_Q - norm_int32(new_amax);

	/* Add to shift for coef raw Q31 format shift and store to
	 * configuration. Ensure range (fail should not happen with OK
	 * coefficient set).
	 */
	*fir_shift = -shift + add_shift;
	if (*fir_shift < DMIC_HW_FIR_SHIFT_MIN ||
		*fir_shift > DMIC_HW_FIR_SHIFT_MAX)
		return -EINVAL;

	/* Compensate shift into FIR coef scaler and store as Q4.20. */
	if (shift < 0)
		*fir_scale = (fir_gain << -shift);
	else
		*fir_scale = (fir_gain >> shift);

#if defined MODULE_TEST
	printf("# FIR gain need Q28 = %d (%f)\n", fir_gain,
		Q_CONVERT_QTOF(fir_gain, 28));
	printf("# FIR max coef no gain Q31 = %d (%f)\n", amax,
		Q_CONVERT_QTOF(amax, 31));
	printf("# FIR max coef with gain Q28 = %d (%f)\n", new_amax,
		Q_CONVERT_QTOF(new_amax, 28));
	printf("# FIR coef norm rshift = %d\n", shift);
	printf("# FIR coef old rshift = %d\n", add_shift);
	printf("# FIR coef new rshift = %d\n", *fir_shift);
	printf("# FIR coef scaler Q28 = %d (%f)\n", *fir_scale,
		Q_CONVERT_QTOF(*fir_scale, 28));
#endif

	return 0;
}

/* This function selects with a simple criteria one mode to set up the
 * decimator. For the settings chosen for FIFOs A and B output a lookup
 * is done for FIR coefficients from the included coefficients tables.
 * For some decimation factors there may be several length coefficient sets.
 * It is due to possible restruction of decimation engine cycles per given
 * sample rate. If the coefficients length is exceeded the lookup continues.
 * Therefore the list of coefficient set must present the filters for a
 * decimation factor in decreasing length order.
 *
 * Note: If there is no filter available an error is returned. The parameters
 * should be reviewed for such case. If still a filter is missing it should be
 * added into the included set. FIR decimation with a high factor usually
 * needs compromizes into specifications and is not desirable.
 */
static int select_mode(struct dmic_configuration *cfg,
	struct matched_modes *modes)
{
	int32_t g_cic;
	int32_t fir_in_max;
	int32_t cic_out_max;
	int32_t gain_to_fir;
	int16_t idx[DMIC_MAX_MODES];
	int16_t *mfir;
	int n = 1;
	int mmin;
	int count;
	int mcic;
	int bits_cic;
	int ret;

	/* If there are more than one possibilities select a mode with lowest
	 * FIR decimation factor. If there are several select mode with highest
	 * ioclk divider to minimize microphone power consumption. The highest
	 * clock divisors are in the end of list so select the last of list.
	 * The minimum OSR criteria used in previous ensures that quality in
	 * the candidates should be sufficient.
	 */
	if (modes->num_of_modes == 0) {
		trace_dmic_error("nom");
		return -EINVAL;
	}

	/* Valid modes presence is indicated with non-zero decimation
	 * factor in 1st element. If FIR A is not used get decimation factors
	 * from FIR B instead.
	 */
	if (modes->mfir_a[0] > 0)
		mfir = modes->mfir_a;
	else
		mfir = modes->mfir_b;

	mmin = find_min_int16(mfir, modes->num_of_modes);
	count = find_equal_int16(idx, mfir, mmin, modes->num_of_modes, 0);
	n = idx[count - 1];

	/* Get microphone clock and decimation parameters for used mode from
	 * the list.
	 */
	cfg->clkdiv = modes->clkdiv[n];
	cfg->mfir_a = modes->mfir_a[n];
	cfg->mfir_b = modes->mfir_b[n];
	cfg->mcic = modes->mcic[n];
	cfg->fir_a = NULL;
	cfg->fir_b = NULL;

	/* Find raw FIR coefficients to match the decimation factors of FIR
	 * A and B.
	 */
	if (cfg->mfir_a > 0) {
		cfg->fir_a = get_fir(cfg, cfg->mfir_a);
		if (!cfg->fir_a) {
			trace_dmic_error("fam");
			trace_value(cfg->mfir_a);
			return -EINVAL;
		}
	}

	if (cfg->mfir_b > 0) {
		cfg->fir_b = get_fir(cfg, cfg->mfir_b);
		if (!cfg->fir_b) {
			trace_dmic_error("fbm");
			trace_value(cfg->mfir_b);
			return -EINVAL;
		}
	}

	/* Calculate CIC shift from the decimation factor specific gain. The
	 * gain of HW decimator equals decimation factor to power of 5.
	 */
	mcic = cfg->mcic;
	g_cic = mcic * mcic * mcic * mcic * mcic;
	if (g_cic < 0) {
		/* Erroneous decimation factor and CIC gain */
		trace_dmic_error("gci");
		return -EINVAL;
	}

	bits_cic = 32 - norm_int32(g_cic);
	cfg->cic_shift = bits_cic - DMIC_HW_BITS_FIR_INPUT;

	/* Calculate remaining gain to FIR in Q format used for gain
	 * values.
	 */
	fir_in_max = (1 << (DMIC_HW_BITS_FIR_INPUT - 1));
	if (cfg->cic_shift >= 0)
		cic_out_max = g_cic >> cfg->cic_shift;
	else
		cic_out_max = g_cic << -cfg->cic_shift;

	gain_to_fir = (int32_t)((((int64_t)fir_in_max) << DMIC_FIR_SCALE_Q) /
		cic_out_max);

	/* Calculate FIR scale and shift */
	if (cfg->mfir_a > 0) {
		cfg->fir_a_length = cfg->fir_a->length;
		ret = fir_coef_scale(&cfg->fir_a_scale, &cfg->fir_a_shift,
			cfg->fir_a->shift, cfg->fir_a->coef, cfg->fir_a->length,
			gain_to_fir);
		if (ret < 0) {
			/* Invalid coefficient set found, should not happen. */
			trace_dmic_error("ina");
			return -EINVAL;
		}
	} else {
		cfg->fir_a_scale = 0;
		cfg->fir_a_shift = 0;
		cfg->fir_a_length = 0;
	}

	if (cfg->mfir_b > 0) {
		cfg->fir_b_length = cfg->fir_b->length;
		ret = fir_coef_scale(&cfg->fir_b_scale, &cfg->fir_b_shift,
			cfg->fir_b->shift, cfg->fir_b->coef, cfg->fir_b->length,
			gain_to_fir);
		if (ret < 0) {
			/* Invalid coefficient set found, should not happen. */
			trace_dmic_error("inb");
			return -EINVAL;
		}
	} else {
		cfg->fir_b_scale = 0;
		cfg->fir_b_shift = 0;
		cfg->fir_b_length = 0;
	}

	return 0;
}

/* The FIFO input packer mode (IPM) settings are somewhat different in
 * HW versions. This helper function returns a suitable IPM bit field
 * value to use.
 */

static inline void ipm_helper1(int *ipm, int stereo[], int swap[],
	struct sof_ipc_dai_dmic_params *dmic)
{
	int pdm[DMIC_HW_CONTROLLERS] = {0};
	int cnt;
	int i;

	/* Loop number of PDM controllers in the configuration. If mic A
	 * or B is enabled then a pdm controller is marked as active. Also it
	 * is checked whether the controller should operate as stereo or mono
	 * left (A) or mono right (B) mode. Mono right mode is setup as channel
	 * swapped mono left.
	 */
	for (i = 0; i < dmic->num_pdm_active; i++) {
		cnt = 0;
		if (dmic->pdm[i].enable_mic_a > 0)
			cnt++;

		if (dmic->pdm[i].enable_mic_b > 0)
			cnt++;

		/* A PDM controller is used if at least one mic was enabled. */
		pdm[i] = !!cnt;

		/* Set stereo mode if both mic A anc B are enabled. */
		cnt >>= 1;
		stereo[i] = cnt;

		/* Swap channels if only mic B is used for mono processing. */
		swap[i] = dmic->pdm[i].enable_mic_b & !cnt;
	}

	/* IPM indicates active pdm controllers. */
	*ipm = 0;

	if (pdm[0] == 0 && pdm[1] > 0)
		*ipm = 1;

	if (pdm[0] > 0 && pdm[1] > 0)
		*ipm = 2;
}

#if DMIC_HW_VERSION == 2

static inline void ipm_helper2(int source[], int *ipm, int stereo[],
	int swap[], struct sof_ipc_dai_dmic_params *dmic)
{
	int pdm[DMIC_HW_CONTROLLERS];
	int i;
	int n = 0;

	/* Loop number of PDM controllers in the configuration. If mic A
	 * or B is enabled then a pdm controller is marked as active. Also it
	 * is checked whether the controller should operate as stereo or mono
	 * left (A) or mono right (B) mode. Mono right mode is setup as channel
	 * swapped mono left. The function returns also in array source[] the
	 * indice of enabled pdm controllers to be used for IPM configuration.
	 */
	for (i = 0; i < dmic->num_pdm_active; i++) {
		if (dmic->pdm[i].enable_mic_a > 0 ||
			dmic->pdm[i].enable_mic_b > 0) {
			pdm[i] = 1;
			source[n] = i;
			n++;
		} else {
			pdm[i] = 0;
			swap[i] = 0;
		}

		if (dmic->pdm[i].enable_mic_a > 0 &&
			dmic->pdm[i].enable_mic_b > 0) {
			stereo[i] = 1;
			swap[i] = 0;
		} else {
			stereo[i] = 0;
			if (dmic->pdm[i].enable_mic_a == 0)
				swap[i] = 1;
			else
				swap[i] = 0;
		}
	}

	/* IPM bit field is set to count of active pdm controllers. */
	*ipm = pdm[0];
	for (i = 1; i < dmic->num_pdm_active; i++)
		*ipm += pdm[i];
}
#endif

static int configure_registers(struct dai *dai, struct dmic_configuration *cfg,
	struct sof_ipc_dai_dmic_params *dmic)
{
	int stereo[DMIC_HW_CONTROLLERS];
	int swap[DMIC_HW_CONTROLLERS];
	uint32_t val;
	int32_t ci;
	uint32_t cu;
	int ipm;
	int of0;
	int of1;
	int fir_decim;
	int fir_length;
	int length;
	int edge;
	int dccomp;
	int cic_start_a;
	int cic_start_b;
	int fir_start_a;
	int fir_start_b;
	int soft_reset;
	int i;
	int j;

	struct dmic_pdata *pdata = dai_get_drvdata(dai);
	int array_a = 0;
	int array_b = 0;
	int cic_mute = 1;
	int fir_mute = 1;
	int bfth = 3; /* Should be 3 for 8 entries, 1 is 2 entries */
	int th = 0; /* Used with TIE=1 */

	/* Normal start sequence */
	dccomp = 1;
	soft_reset = 1;
	cic_start_a = 0;
	cic_start_b = 0;
	fir_start_a = 0;
	fir_start_b = 0;

#if DMIC_HW_VERSION == 2
	int source[4] = {0, 0, 0, 0};
#endif

#if defined MODULE_TEST
		int32_t fir_a_max = 0;
		int32_t fir_a_min = 0;
		int32_t fir_b_max = 0;
		int32_t fir_b_min = 0;
#endif

	/* pdata is set by dmic_probe(), error if it has not been set */
	if (!pdata) {
		trace_dmic_error("cfr");
		return -EINVAL;
	}

	/* Sanity checks */
	if (dmic->num_pdm_active > DMIC_HW_CONTROLLERS) {
		trace_dmic_error("num");
		return -EINVAL;
	}

	/* OUTCONTROL0 and OUTCONTROL1 */
	trace_dmic("reg");
	of0 = (dmic->fifo_bits_a == 32) ? 2 : 0;
	of1 = (dmic->fifo_bits_b == 32) ? 2 : 0;

#if DMIC_HW_VERSION == 1
	ipm_helper1(&ipm, stereo, swap, dmic);
	val = OUTCONTROL0_TIE(0) |
		OUTCONTROL0_SIP(0) |
		OUTCONTROL0_FINIT(1) |
		OUTCONTROL0_FCI(0) |
		OUTCONTROL0_BFTH(bfth) |
		OUTCONTROL0_OF(of0) |
		OUTCONTROL0_IPM(ipm) |
		OUTCONTROL0_TH(th);
	dmic_write(dai, OUTCONTROL0, val);
	trace_value(val);

	val = OUTCONTROL1_TIE(0) |
		OUTCONTROL1_SIP(0) |
		OUTCONTROL1_FINIT(1) |
		OUTCONTROL1_FCI(0) |
		OUTCONTROL1_BFTH(bfth) |
		OUTCONTROL1_OF(of1) |
		OUTCONTROL1_IPM(ipm) |
		OUTCONTROL1_TH(th);
	dmic_write(dai, OUTCONTROL1, val);
	trace_value(val);
#endif

#if DMIC_HW_VERSION == 2
#if DMIC_HW_CONTROLLERS > 2
	ipm_helper2(source, &ipm, stereo, swap, dmic);
#else
	ipm_helper1(&ipm, stereo, swap, dmic);
#endif
	val = OUTCONTROL0_TIE(0) |
		OUTCONTROL0_SIP(0) |
		OUTCONTROL0_FINIT(1) |
		OUTCONTROL0_FCI(0) |
		OUTCONTROL0_BFTH(bfth) |
		OUTCONTROL0_OF(of0) |
		OUTCONTROL0_IPM(ipm) |
		OUTCONTROL0_IPM_SOURCE_1(source[0]) |
		OUTCONTROL0_IPM_SOURCE_2(source[1]) |
		OUTCONTROL0_IPM_SOURCE_3(source[2]) |
		OUTCONTROL0_IPM_SOURCE_4(source[3]) |
		OUTCONTROL0_TH(th);
	dmic_write(dai, OUTCONTROL0, val);
	trace_value(val);

	val = OUTCONTROL1_TIE(0) |
		OUTCONTROL1_SIP(0) |
		OUTCONTROL1_FINIT(1) |
		OUTCONTROL1_FCI(0) |
		OUTCONTROL1_BFTH(bfth) |
		OUTCONTROL1_OF(of1) |
		OUTCONTROL1_IPM(ipm) |
		OUTCONTROL1_IPM_SOURCE_1(source[0]) |
		OUTCONTROL1_IPM_SOURCE_2(source[1]) |
		OUTCONTROL1_IPM_SOURCE_3(source[2]) |
		OUTCONTROL1_IPM_SOURCE_4(source[3]) |
		OUTCONTROL1_TH(th);
	dmic_write(dai, OUTCONTROL1, val);
	trace_value(val);
#endif

	/* Mark enabled microphones into private data to be later used
	 * for starting correct parts of the HW.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		pdata->enable[i] = (dmic->pdm[i].enable_mic_b << 1) |
			dmic->pdm[i].enable_mic_a;
	}

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		/* CIC */
		val = CIC_CONTROL_SOFT_RESET(soft_reset) |
			CIC_CONTROL_CIC_START_B(cic_start_b) |
			CIC_CONTROL_CIC_START_A(cic_start_a) |
		CIC_CONTROL_MIC_B_POLARITY(dmic->pdm[i].polarity_mic_a) |
		CIC_CONTROL_MIC_A_POLARITY(dmic->pdm[i].polarity_mic_b) |
			CIC_CONTROL_MIC_MUTE(cic_mute) |
			CIC_CONTROL_STEREO_MODE(stereo[i]);
		dmic_write(dai, base[i] + CIC_CONTROL, val);
		trace_value(val);

		val = CIC_CONFIG_CIC_SHIFT(cfg->cic_shift + 8) |
			CIC_CONFIG_COMB_COUNT(cfg->mcic - 1);
		dmic_write(dai, base[i] + CIC_CONFIG, val);
		trace_value(val);

		/* Mono right channel mic usage requires swap of PDM channels
		 * since the mono decimation is done with only left channel
		 * processing active.
		 */
		edge = dmic->pdm[i].clk_edge;
		if (swap[i])
			edge = !edge;

		val = MIC_CONTROL_PDM_CLKDIV(cfg->clkdiv - 2) |
			MIC_CONTROL_PDM_SKEW(dmic->pdm[i].skew) |
			MIC_CONTROL_CLK_EDGE(edge) |
			MIC_CONTROL_PDM_EN_B(cic_start_b) |
			MIC_CONTROL_PDM_EN_A(cic_start_a);
		dmic_write(dai, base[i] + MIC_CONTROL, val);
		trace_value(val);

		/* FIR A */
		fir_decim = MAX(cfg->mfir_a - 1, 0);
		fir_length = MAX(cfg->fir_a_length - 1, 0);
		val = FIR_CONTROL_A_START(fir_start_a) |
			FIR_CONTROL_A_ARRAY_START_EN(array_a) |
			FIR_CONTROL_A_DCCOMP(dccomp) |
			FIR_CONTROL_A_MUTE(fir_mute) |
			FIR_CONTROL_A_STEREO(stereo[i]);
		dmic_write(dai, base[i] + FIR_CONTROL_A, val);
		trace_value(val);

		val = FIR_CONFIG_A_FIR_DECIMATION(fir_decim) |
			FIR_CONFIG_A_FIR_SHIFT(cfg->fir_a_shift) |
			FIR_CONFIG_A_FIR_LENGTH(fir_length);
		dmic_write(dai, base[i] + FIR_CONFIG_A, val);
		trace_value(val);

		val = DC_OFFSET_LEFT_A_DC_OFFS(DCCOMP_TC0);
		dmic_write(dai, base[i] + DC_OFFSET_LEFT_A, val);
		trace_value(val);

		val = DC_OFFSET_RIGHT_A_DC_OFFS(DCCOMP_TC0);
		dmic_write(dai, base[i] + DC_OFFSET_RIGHT_A, val);
		trace_value(val);

		val = OUT_GAIN_LEFT_A_GAIN(0);
		dmic_write(dai, base[i] + OUT_GAIN_LEFT_A, val);
		trace_value(val);

		val = OUT_GAIN_RIGHT_A_GAIN(0);
		dmic_write(dai, base[i] + OUT_GAIN_RIGHT_A, val);
		trace_value(val);

		/* FIR B */
		fir_decim = MAX(cfg->mfir_b - 1, 0);
		fir_length = MAX(cfg->fir_b_length - 1, 0);
		val = FIR_CONTROL_B_START(fir_start_b) |
			FIR_CONTROL_B_ARRAY_START_EN(array_b) |
			FIR_CONTROL_B_DCCOMP(dccomp) |
			FIR_CONTROL_B_MUTE(fir_mute) |
			FIR_CONTROL_B_STEREO(stereo[i]);
		dmic_write(dai, base[i] + FIR_CONTROL_B, val);
		trace_value(val);

		val = FIR_CONFIG_B_FIR_DECIMATION(fir_decim) |
			FIR_CONFIG_B_FIR_SHIFT(cfg->fir_b_shift) |
			FIR_CONFIG_B_FIR_LENGTH(fir_length);
		dmic_write(dai, base[i] + FIR_CONFIG_B, val);
		trace_value(val);

		val = DC_OFFSET_LEFT_B_DC_OFFS(DCCOMP_TC0);
		dmic_write(dai, base[i] + DC_OFFSET_LEFT_B, val);
		trace_value(val);
		trace_value(val);

		val = DC_OFFSET_RIGHT_B_DC_OFFS(DCCOMP_TC0);
		dmic_write(dai, base[i] + DC_OFFSET_RIGHT_B, val);
		trace_value(val);

		val = OUT_GAIN_LEFT_B_GAIN(0);
		dmic_write(dai, base[i] + OUT_GAIN_LEFT_B, val);
		trace_value(val);

		val = OUT_GAIN_RIGHT_B_GAIN(0);
		dmic_write(dai, base[i] + OUT_GAIN_RIGHT_B, val);
		trace_value(val);

		/* Write coef RAM A with scaled coefficient in reverse order */
		length = cfg->fir_a_length;
		for (j = 0; j < length; j++) {
			ci = (int32_t)Q_MULTSR_32X32(
				(int64_t)cfg->fir_a->coef[j], cfg->fir_a_scale,
				31, DMIC_FIR_SCALE_Q, DMIC_HW_FIR_COEF_Q);
			cu = FIR_COEF_A(ci);
			dmic_write(dai, coef_base_a[i]
				+ ((length - j - 1) << 2), cu);
#if defined MODULE_TEST
			fir_a_max = MAX(fir_a_max, ci);
			fir_a_min = MIN(fir_a_min, ci);
#endif
		}

		/* Write coef RAM B with scaled coefficient in reverse order */
		length = cfg->fir_b_length;
		for (j = 0; j < length; j++) {
			ci = (int32_t)Q_MULTSR_32X32(
				(int64_t)cfg->fir_b->coef[j], cfg->fir_b_scale,
				31, DMIC_FIR_SCALE_Q, DMIC_HW_FIR_COEF_Q);
			cu = FIR_COEF_B(ci);
			dmic_write(dai, coef_base_b[i]
				+ ((length - j - 1) << 2), cu);
#if defined MODULE_TEST
			fir_b_max = MAX(fir_b_max, ci);
			fir_b_min = MIN(fir_b_min, ci);
#endif
		}
	}

#if defined MODULE_TEST
	printf("# FIR A max Q19 = %d (%f)\n", fir_a_max,
		Q_CONVERT_QTOF(fir_a_max, 19));
	printf("# FIR A min Q19 = %d (%f)\n", fir_a_min,
		Q_CONVERT_QTOF(fir_a_min, 19));
	printf("# FIR B max Q19 = %d (%f)\n", fir_b_max,
		Q_CONVERT_QTOF(fir_b_max, 19));
	printf("# FIR B min Q19 = %d (%f)\n", fir_b_min,
		Q_CONVERT_QTOF(fir_b_min, 19));
#endif

	/* Function dmic_start() uses these to start the used FIFOs */
	if (cfg->mfir_a > 0)
		pdata->fifo_a = 1;
	else
		pdata->fifo_a = 0;

	if (cfg->mfir_b > 0)
		pdata->fifo_b = 1;
	else
		pdata->fifo_b = 0;

	return 0;
}

static int dmic_set_config(struct dai *dai, struct sof_ipc_dai_config *config)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	struct sof_ipc_dai_dmic_params *prm;
	struct matched_modes modes_ab;
	struct dmic_configuration cfg;
	struct decim_modes modes_a;
	struct decim_modes modes_b;
	int num_pdm_active = config->dmic.num_pdm_active;
	size_t size;
	int i, j, ret = 0;

	trace_dmic("dsc");

	/* Initialize start sequence handler */
	work_init(&dmic->dmicwork, dmic_work, dai, WORK_ASYNC);

	/*
	 * "config" might contain pdm controller params for only
	 * the active controllers
	 * "prm" is initialized with default params for all HW controllers
	 */
	size = sizeof(*prm) + DMIC_HW_CONTROLLERS
		* sizeof(struct sof_ipc_dai_dmic_pdm_ctrl);
	prm = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, size);

	/* copy the DMIC params */
	memcpy(prm, &config->dmic, sizeof(struct sof_ipc_dai_dmic_params));

	/* copy the pdm controller params from ipc */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		prm->pdm[i].id = i;
		for (j = 0; j < num_pdm_active; j++) {
			/* copy the pdm controller params id the id's match */
			if (prm->pdm[i].id == config->dmic.pdm[j].id)
				memcpy(&prm->pdm[i], &config->dmic.pdm[j],
				       sizeof(prm->pdm[i]));
		}
	}

	trace_value(prm->driver_ipc_version);
	trace_value(prm->pdmclk_min);
	trace_value(prm->pdmclk_max);
	trace_value(prm->fifo_fs_a);
	trace_value(prm->fifo_fs_b);
	trace_value(prm->fifo_bits_a);
	trace_value(prm->fifo_bits_b);
	trace_value(prm->duty_min);
	trace_value(prm->duty_max);
	trace_value(prm->num_pdm_active);

	if (prm->driver_ipc_version != DMIC_IPC_VERSION) {
		trace_dmic_error("ver");
		ret = -EINVAL;
		goto finish;
	}

	/* Match and select optimal decimators configuration for FIFOs A and B
	 * paths. This setup phase is still abstract. Successful completion
	 * points struct cfg to FIR coefficients and contains the scale value
	 * to use for FIR coefficient RAM write as well as the CIC and FIR
	 * shift values.
	 */
	find_modes(&modes_a, prm, prm->fifo_fs_a);
	if (modes_a.num_of_modes == 0 && prm->fifo_fs_a > 0) {
		trace_dmic_error("amo");
		ret = -EINVAL;
		goto finish;
	}

	find_modes(&modes_b, prm, prm->fifo_fs_b);
	if (modes_b.num_of_modes == 0 && prm->fifo_fs_b > 0) {
		trace_dmic_error("bmo");
		ret = -EINVAL;
		goto finish;
	}

	match_modes(&modes_ab, &modes_a, &modes_b);
	ret = select_mode(&cfg, &modes_ab);
	if (ret < 0) {
		trace_dmic_error("smo");
		ret = -EINVAL;
		goto finish;
	}

	trace_dmic("cfg");
	trace_value(cfg.clkdiv);
	trace_value(cfg.mcic);
	trace_value(cfg.mfir_a);
	trace_value(cfg.mfir_b);
	trace_value(cfg.fir_a_length);
	trace_value(cfg.fir_b_length);
	trace_value(cfg.cic_shift);
	trace_value(cfg.fir_a_shift);
	trace_value(cfg.fir_b_shift);

	/* Struct reg contains a mirror of actual HW registers. Determine
	 * register bits configuration from decimator configuration and the
	 * requested parameters.
	 */
	ret = configure_registers(dai, &cfg, prm);
	if (ret < 0) {
		trace_dmic_error("cor");
		ret = -EINVAL;
		goto finish;
	}

	dmic->state = COMP_STATE_PREPARE;

finish:
	/* free config params */
	rfree(prm);

	return ret;
}

/* start the DMIC for capture */
static void dmic_start(struct dai *dai)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int i;
	int mic_a;
	int mic_b;
	int fir_a;
	int fir_b;

	/* enable port */
	spin_lock(&dmic->lock);
	trace_dmic("sta");
	dmic->state = COMP_STATE_ACTIVE;
	dmic->startcount = 0;
	dmic->gain = LOGRAMP_GI; /* Initial gain value */

	if (dmic->fifo_a) {
		trace_dmic("ffa");
		/*  Clear FIFO A initialize, Enable interrupts to DSP,
		 *  Start FIFO A packer.
		 */
		dmic_update_bits(dai, OUTCONTROL0,
			OUTCONTROL0_FINIT_BIT | OUTCONTROL0_SIP_BIT,
			OUTCONTROL0_SIP_BIT);
	}
	if (dmic->fifo_b) {
		trace_dmic("ffb");
		/*  Clear FIFO B initialize, Enable interrupts to DSP,
		 *  Start FIFO B packer.
		 */
		dmic_update_bits(dai, OUTCONTROL1,
			OUTCONTROL1_FINIT_BIT | OUTCONTROL1_SIP_BIT,
			OUTCONTROL1_SIP_BIT);
	}

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		mic_a = dmic->enable[i] & 1;
		mic_b = (dmic->enable[i] & 2) >> 1;
		if (dmic->fifo_a)
			fir_a = (dmic->enable[i] > 0) ? 1 : 0;
		else
			fir_a = 0;
		if (dmic->fifo_b)
			fir_b = (dmic->enable[i] > 0) ? 1 : 0;
		else
			fir_b = 0;

		trace_dmic("mfn");
		trace_value(mic_a);
		trace_value(mic_b);
		trace_value(fir_a);
		trace_value(fir_b);

		dmic_update_bits(dai, base[i] + CIC_CONTROL,
			CIC_CONTROL_CIC_START_A_BIT |
			CIC_CONTROL_CIC_START_B_BIT,
			CIC_CONTROL_CIC_START_A(mic_a) |
			CIC_CONTROL_CIC_START_B(mic_b));
		dmic_update_bits(dai, base[i] + MIC_CONTROL,
			MIC_CONTROL_PDM_EN_A_BIT |
			MIC_CONTROL_PDM_EN_B_BIT,
			MIC_CONTROL_PDM_EN_A(mic_a) |
			MIC_CONTROL_PDM_EN_B(mic_b));

		dmic_update_bits(dai, base[i] + FIR_CONTROL_A,
			FIR_CONTROL_A_START_BIT, FIR_CONTROL_A_START(fir_a));
		dmic_update_bits(dai, base[i] + FIR_CONTROL_B,
			FIR_CONTROL_B_START_BIT, FIR_CONTROL_B_START(fir_b));
	}

	/* Clear soft reset for all/used PDM controllers. This should
	 * start capture in sync.
	 */
	trace_dmic("unr");
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		dmic_update_bits(dai, base[i] + CIC_CONTROL,
			CIC_CONTROL_SOFT_RESET_BIT, 0);
	}

	spin_unlock(&dmic->lock);

	/* Currently there's no DMIC HW internal mutings and wait times
	 * applied into this start sequence. It can be implemented here if
	 * start of audio capture would contain clicks and/or noise and it
	 * is not suppressed by gain ramp somewhere in the capture pipe.
	 */

	work_schedule_default(&dmic->dmicwork, DMIC_UNMUTE_RAMP_US);

	trace_dmic("run");
}
/* stop the DMIC for capture */
static void dmic_stop(struct dai *dai)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int i;

	trace_dmic("sto")
	spin_lock(&dmic->lock);
	dmic->state = COMP_STATE_PREPARE;

	/* Stop FIFO packers and set FIFO initialize bits */
	dmic_update_bits(dai, OUTCONTROL0,
		OUTCONTROL0_SIP_BIT | OUTCONTROL0_FINIT_BIT,
		OUTCONTROL0_FINIT_BIT);
	dmic_update_bits(dai, OUTCONTROL1,
		OUTCONTROL1_SIP_BIT | OUTCONTROL1_FINIT_BIT,
		OUTCONTROL1_FINIT_BIT);

	/* Set soft reset and mute on for all PDM controllers.
	 */
	trace_dmic("sre");
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		dmic_update_bits(dai, base[i] + CIC_CONTROL,
				 CIC_CONTROL_SOFT_RESET_BIT |
				 CIC_CONTROL_MIC_MUTE_BIT,
				 CIC_CONTROL_SOFT_RESET_BIT |
				 CIC_CONTROL_MIC_MUTE_BIT);
		dmic_update_bits(dai, base[i] + FIR_CONTROL_A,
				 FIR_CONTROL_A_MUTE_BIT,
				 FIR_CONTROL_A_MUTE_BIT);
		dmic_update_bits(dai, base[i] + FIR_CONTROL_B,
				 FIR_CONTROL_B_MUTE_BIT,
				 FIR_CONTROL_B_MUTE_BIT);
	}

	spin_unlock(&dmic->lock);
}

/* save DMIC context prior to entering D3 */
static int dmic_context_store(struct dai *dai)
{
	/* TODO: Nothing stored at the moment. Could read the registers into
	 * persisten memory if needed but the large coef RAM is not desirable
	 * to copy. It would be better to store selected mode parametesr from
	 * previous configuration request and re-program registers from
	 * scratch.
	 */
	return 0;
}

/* restore DMIC context after leaving D3 */
static int dmic_context_restore(struct dai *dai)
{
	/* Nothing restored at the moment. */
	return 0;
}

static int dmic_trigger(struct dai *dai, int cmd, int direction)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);

	trace_dmic("tri");

	/* dai private is set in dmic_probe(), error if not set */
	if (!dmic) {
		trace_dmic_error("trn");
		return -EINVAL;
	}

	if (direction != DAI_DIR_CAPTURE) {
		trace_dmic_error("cap");
		return -EINVAL;
	}

	switch (cmd) {
	case COMP_TRIGGER_RELEASE:
	case COMP_TRIGGER_START:
		if (dmic->state == COMP_STATE_PREPARE ||
			dmic->state == COMP_STATE_PAUSED) {
			dmic_start(dai);
		} else {
			trace_dmic_error("cst");
			trace_value(dmic->state);
		}
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_PAUSE:
		dmic_stop(dai);
		break;
	case COMP_TRIGGER_RESUME:
		dmic_context_restore(dai);
		break;
	case COMP_TRIGGER_SUSPEND:
		dmic_context_store(dai);
		break;
	default:
		break;
	}

	return 0;
}

/* On DMIC IRQ event trace the status register that contains the status and
 * error bit fields.
 */
static void dmic_irq_handler(void *data)
{
	struct dai *dai = data;
	uint32_t val;

	/* Trace OUTSTAT0 register */
	val = dmic_read(dai, OUTSTAT0);
	trace_dmic("irq");

	if (val & OUTSTAT0_ROR_BIT)
		trace_dmic_error("eor"); /* Full fifo or PDM overrrun */

	trace_value(val);

	/* clear IRQ */
	platform_interrupt_clear(dmic_irq(dai), 1);
}

static int dmic_probe(struct dai *dai)
{
	struct dmic_pdata *dmic;

	trace_dmic("pro");

	/* allocate private data */
	dmic = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*dmic));
	dai_set_drvdata(dai, dmic);

	spinlock_init(&dmic->lock);

	/* Set state, note there is no playback direction support */
	dmic->state = COMP_STATE_READY;

	/* register our IRQ handler */
	interrupt_register(dmic_irq(dai), dmic_irq_handler, dai);

	platform_interrupt_unmask(dmic_irq(dai), 1);
	interrupt_enable(dmic_irq(dai));

	return 0;
}

/* DMIC has no loopback support */
static inline int dmic_set_loopback_mode(struct dai *dai, uint32_t lbm)
{
	return -EINVAL;
}

const struct dai_ops dmic_ops = {
	.trigger = dmic_trigger,
	.set_config = dmic_set_config,
	.pm_context_store = dmic_context_store,
	.pm_context_restore = dmic_context_restore,
	.probe = dmic_probe,
	.set_loopback_mode = dmic_set_loopback_mode,
};

#endif
