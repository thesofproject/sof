// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/drivers/dmic.h>

#if defined DMIC_HW_VERSION

#include <sof/audio/coefficients/pdm_decim/pdm_decim_fir.h>
#include <sof/audio/coefficients/pdm_decim/pdm_decim_table.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timestamp.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/math/decibels.h>
#include <sof/math/numbers.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <ipc/dai.h>
#include <ipc/dai-intel.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* aafc26fe-3b8d-498d-8bd6-248fc72efa31 */
DECLARE_SOF_UUID("dmic-dai", dmic_uuid, 0xaafc26fe, 0x3b8d, 0x498d,
		 0x8b, 0xd6, 0x24, 0x8f, 0xc7, 0x2e, 0xfa, 0x31);

DECLARE_TR_CTX(dmic_tr, SOF_UUID(dmic_uuid), LOG_LEVEL_INFO);

/* 59c87728-d8f9-42f6-b89d-5870a87b0e1e */
DECLARE_SOF_UUID("dmic-work", dmic_work_task_uuid, 0x59c87728, 0xd8f9, 0x42f6,
		 0xb8, 0x9d, 0x58, 0x70, 0xa8, 0x7b, 0x0e, 0x1e);

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

/* Used for scaling FIR coefficients for HW */
#define DMIC_HW_FIR_COEF_MAX ((1 << (DMIC_HW_BITS_FIR_COEF - 1)) - 1)
#define DMIC_HW_FIR_COEF_Q (DMIC_HW_BITS_FIR_COEF - 1)

/* Internal precision in gains computation, e.g. Q4.28 in int32_t */
#define DMIC_FIR_SCALE_Q 28

/* Used in unmute ramp values calculation */
#define DMIC_HW_FIR_GAIN_MAX ((1 << (DMIC_HW_BITS_FIR_GAIN - 1)) - 1)

/* Hardwired log ramp parameters. The first value is the initial gain in
 * decibels. The second value is the default ramp time.
 */
#define LOGRAMP_START_DB Q_CONVERT_FLOAT(-90, DB2LIN_FIXED_INPUT_QY)
#define LOGRAMP_TIME_MS 400 /* Default ramp time in milliseconds */

/* Limits for ramp time from topology */
#define LOGRAMP_TIME_MIN_MS 10 /* Min. 10 ms */
#define LOGRAMP_TIME_MAX_MS 1000 /* Max. 1s */

/* Simplify log ramp step calculation equation with this constant term */
#define LOGRAMP_CONST_TERM ((int32_t) \
	((int64_t)-LOGRAMP_START_DB * DMIC_UNMUTE_RAMP_US / 1000))

/* Fractional shift for gain update. Gain format is Q2.30. */
#define Q_SHIFT_GAIN_X_GAIN_COEF \
	(Q_SHIFT_BITS_32(30, DB2LIN_FIXED_OUTPUT_QY, 30))

/* Base addresses (in PDM scope) of 2ch PDM controllers and coefficient RAM. */
static const uint32_t base[4] = {PDM0, PDM1, PDM2, PDM3};
static const uint32_t coef_base_a[4] = {PDM0_COEFFICIENT_A, PDM1_COEFFICIENT_A,
	PDM2_COEFFICIENT_A, PDM3_COEFFICIENT_A};
static const uint32_t coef_base_b[4] = {PDM0_COEFFICIENT_B, PDM1_COEFFICIENT_B,
	PDM2_COEFFICIENT_B, PDM3_COEFFICIENT_B};

/* Global configuration request for DMIC */
static struct sof_ipc_dai_dmic_params *dmic_prm[DMIC_HW_FIFOS];
static int dmic_active_fifos;

/* this ramps volume changes over time */
static enum task_state dmic_work(void *data)
{
	struct dai *dai = (struct dai *)data;
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int32_t gval;
	uint32_t val;
	int i;
	int ret;

	dai_dbg(dai, "dmic_work()");

	ret = spin_try_lock(&dai->lock);
	if (!ret) {
		dai_dbg(dai, "dmic_work(): spin_try_lock(dai->lock, ret) failed: RESCHEDULE");
		return SOF_TASK_STATE_RESCHEDULE;
	}

	/* Increment gain with logarithmic step.
	 * Gain is Q2.30 and gain modifier is Q12.20.
	 */
	dmic->startcount++;
	dmic->gain = q_multsr_sat_32x32(dmic->gain, dmic->gain_coef,
					Q_SHIFT_GAIN_X_GAIN_COEF);

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
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_MIC_MUTE_BIT, 0);

		if (dmic->startcount == DMIC_UNMUTE_FIR) {
			switch (dai->index) {
			case 0:
				dai_update_bits(dai, base[i] + FIR_CONTROL_A,
						FIR_CONTROL_A_MUTE_BIT, 0);
				break;
			case 1:
				dai_update_bits(dai, base[i] + FIR_CONTROL_B,
						FIR_CONTROL_B_MUTE_BIT, 0);
				break;
			}
		}
		switch (dai->index) {
		case 0:
			val = OUT_GAIN_LEFT_A_GAIN(gval);
			dai_write(dai, base[i] + OUT_GAIN_LEFT_A, val);
			dai_write(dai, base[i] + OUT_GAIN_RIGHT_A, val);
			break;
		case 1:
			val = OUT_GAIN_LEFT_B_GAIN(gval);
			dai_write(dai, base[i] + OUT_GAIN_LEFT_B, val);
			dai_write(dai, base[i] + OUT_GAIN_RIGHT_B, val);
			break;
		}
	}


	spin_unlock(&dai->lock);

	return gval ? SOF_TASK_STATE_RESCHEDULE : SOF_TASK_STATE_COMPLETED;
}

/* This function returns a raw list of potential microphone clock and decimation
 * modes for achieving requested sample rates. The search is constrained by
 * decimation HW capabililies and setup parameters. The parameters such as
 * microphone clock min/max and duty cycle requirements need be checked from
 * used microphone component datasheet.
 */
static void find_modes(struct dai *dai,
		       struct decim_modes *modes, uint32_t fs, int di)
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
	int j;
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
	if (dmic_prm[di]->pdmclk_max < DMIC_HW_PDM_CLK_MIN ||
	    dmic_prm[di]->pdmclk_max > DMIC_HW_IOCLK / 2) {
		dai_err(dai, "find_modes():  pdm clock max not in range");
		return;
	}
	if (dmic_prm[di]->pdmclk_min < DMIC_HW_PDM_CLK_MIN ||
	    dmic_prm[di]->pdmclk_min > dmic_prm[di]->pdmclk_max) {
		dai_err(dai, "find_modes():  pdm clock min not in range");
		return;
	}

	/* Check for sane duty cycle */
	if (dmic_prm[di]->duty_min > dmic_prm[di]->duty_max) {
		dai_err(dai, "find_modes(): duty cycle min > max");
		return;
	}
	if (dmic_prm[di]->duty_min < DMIC_HW_DUTY_MIN ||
	    dmic_prm[di]->duty_min > DMIC_HW_DUTY_MAX) {
		dai_err(dai, "find_modes():  pdm clock min not in range");
		return;
	}
	if (dmic_prm[di]->duty_max < DMIC_HW_DUTY_MIN ||
	    dmic_prm[di]->duty_max > DMIC_HW_DUTY_MAX) {
		dai_err(dai, "find_modes(): pdm clock max not in range");
		return;
	}

	/* Min and max clock dividers */
	clkdiv_min = ceil_divide(DMIC_HW_IOCLK, dmic_prm[di]->pdmclk_max);
	clkdiv_min = MAX(clkdiv_min, DMIC_HW_CIC_DECIM_MIN);
	clkdiv_max = DMIC_HW_IOCLK / dmic_prm[di]->pdmclk_min;

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
		if (osr < osr_min || du_min < dmic_prm[di]->duty_min ||
		    du_max > dmic_prm[di]->duty_max)
			continue;

		/* Loop FIR decimation factors candidates. If the
		 * integer divided decimation factors and clock dividers
		 * as multiplied with sample rate match the IO clock
		 * rate the division was exact and such decimation mode
		 * is possible. Then check that CIC decimation constraints
		 * are met. The passed decimation modes are added to array.
		 */
		for (j = 0; fir_list[j]; j++) {
			mfir = fir_list[j]->decim_factor;

			/* Skip if previous decimation factor was the same */
			if (j > 1 && fir_list[j - 1]->decim_factor == mfir)
				continue;

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
			}
		}
	}

	modes->num_of_modes = i;
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
static struct pdm_decim *get_fir(struct dai *dai,
				 struct dmic_configuration *cfg, int mfir)
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
			     DMIC_HW_IOCLK / fs / 2 -
			     DMIC_FIR_PIPELINE_OVERHEAD);

	i = 0;
	/* Loop until NULL */
	while (fir_list[i]) {
		if (fir_list[i]->decim_factor == mfir) {
			if (fir_list[i]->length <= fir_max_length) {
				/* Store pointer, break from loop to avoid a
				 * Possible other mode with lower FIR length.
				 */
				fir = fir_list[i];
				break;
			}
			dai_info(dai, "get_fir(), Note length=%d exceeds max=%d",
				 fir_list[i]->length, fir_max_length);
		}
		i++;
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
		*fir_scale = fir_gain << -shift;
	else
		*fir_scale = fir_gain >> shift;

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
static int select_mode(struct dai *dai,
		       struct dmic_configuration *cfg,
		       struct matched_modes *modes)
{
	int32_t g_cic;
	int32_t fir_in_max;
	int32_t cic_out_max;
	int32_t gain_to_fir;
	int16_t idx[DMIC_MAX_MODES];
	int16_t *mfir;
	int mcic;
	int bits_cic;
	int ret;
	int n;
	int found = 0;

	/* If there are more than one possibilities select a mode with a preferred
	 * FIR decimation factor. If there are several select mode with highest
	 * ioclk divider to minimize microphone power consumption. The highest
	 * clock divisors are in the end of list so select the last of list.
	 * The minimum OSR criteria used in previous ensures that quality in
	 * the candidates should be sufficient.
	 */
	if (modes->num_of_modes == 0) {
		dai_err(dai, "select_mode(): no modes available");
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

	/* Search fir_list[] decimation factors from start towards end. The found
	 * last configuration entry with searched decimation factor will be used.
	 */
	for (n = 0; fir_list[n]; n++) {
		found = find_equal_int16(idx, mfir, fir_list[n]->decim_factor,
					 modes->num_of_modes, 0);
		if (found)
			break;
	}

	if (!found) {
		dai_err(dai, "select_mode(): No filter for decimation found");
		return -EINVAL;
	}
	n = idx[found - 1]; /* Option with highest clock divisor and lowest mic clock rate */

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
		cfg->fir_a = get_fir(dai, cfg, cfg->mfir_a);
		if (!cfg->fir_a) {
			dai_err(dai, "select_mode(): cannot find FIR coefficients, mfir_a = %u",
				cfg->mfir_a);
			return -EINVAL;
		}
	}

	if (cfg->mfir_b > 0) {
		cfg->fir_b = get_fir(dai, cfg, cfg->mfir_b);
		if (!cfg->fir_b) {
			dai_err(dai, "select_mode(): cannot find FIR coefficients, mfir_b = %u",
				cfg->mfir_b);
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
		dai_err(dai, "select_mode(): erroneous decimation factor and CIC gain");
		return -EINVAL;
	}

	bits_cic = 32 - norm_int32(g_cic);
	cfg->cic_shift = bits_cic - DMIC_HW_BITS_FIR_INPUT;

	/* Calculate remaining gain to FIR in Q format used for gain
	 * values.
	 */
	fir_in_max = INT_MAX(DMIC_HW_BITS_FIR_INPUT);
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
				     cfg->fir_a->shift, cfg->fir_a->coef,
				     cfg->fir_a->length, gain_to_fir);
		if (ret < 0) {
			/* Invalid coefficient set found, should not happen. */
			dai_err(dai, "select_mode(): invalid coefficient set found");
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
				     cfg->fir_b->shift, cfg->fir_b->coef,
				     cfg->fir_b->length, gain_to_fir);
		if (ret < 0) {
			/* Invalid coefficient set found, should not happen. */
			dai_err(dai, "select_mode(): invalid coefficient set found");
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

static inline void ipm_helper1(int *ipm, int di)
{
	int pdm[DMIC_HW_CONTROLLERS];
	int i;

	/* Loop number of PDM controllers in the configuration. If mic A
	 * or B is enabled then a pdm controller is marked as active for
	 * this DAI.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (dmic_prm[di]->pdm[i].enable_mic_a ||
		    dmic_prm[di]->pdm[i].enable_mic_b)
			pdm[i] = 1;
		else
			pdm[i] = 0;
	}

	/* Set IPM to match active pdm controllers. */
	*ipm = 0;

	if (pdm[0] == 0 && pdm[1] > 0)
		*ipm = 1;

	if (pdm[0] > 0 && pdm[1] > 0)
		*ipm = 2;
}

#if DMIC_HW_VERSION >= 2

static inline void ipm_helper2(int source[], int *ipm, int di)
{
	int pdm[DMIC_HW_CONTROLLERS];
	int i;
	int n = 0;

	for (i = 0; i < OUTCONTROLX_IPM_NUMSOURCES; i++)
		source[i] = 0;

	/* Loop number of PDM controllers in the configuration. If mic A
	 * or B is enabled then a pdm controller is marked as active.
	 * The function returns in array source[] the indice of enabled
	 * pdm controllers to be used for IPM configuration.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (dmic_prm[di]->pdm[i].enable_mic_a ||
		    dmic_prm[di]->pdm[i].enable_mic_b) {
			pdm[i] = 1;
			source[n] = i;
			n++;
		} else {
			pdm[i] = 0;
		}
	}

	/* IPM bit field is set to count of active pdm controllers. */
	*ipm = pdm[0];
	for (i = 1; i < DMIC_HW_CONTROLLERS; i++)
		*ipm += pdm[i];
}
#endif

/* Loop number of PDM controllers in the configuration. The function
 * checks if the controller should operate as stereo or mono left (A)
 * or mono right (B) mode. Mono right mode is setup as channel
 * swapped mono left.
 */
static int stereo_helper(int stereo[], int swap[])
{
	int cnt;
	int i;
	int swap_check;
	int ret = 0;

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		cnt = 0;
		if (dmic_prm[0]->pdm[i].enable_mic_a ||
		    dmic_prm[1]->pdm[i].enable_mic_a)
			cnt++;

		if (dmic_prm[0]->pdm[i].enable_mic_b ||
		    dmic_prm[1]->pdm[i].enable_mic_b)
			cnt++;

		/* Set stereo mode if both mic A anc B are enabled. */
		cnt >>= 1;
		stereo[i] = cnt;

		/* Swap channels if only mic B is used for mono processing. */
		swap[i] = (dmic_prm[0]->pdm[i].enable_mic_b ||
			dmic_prm[1]->pdm[i].enable_mic_b) && !cnt;

		/* Check that swap does not conflict with other DAI request */
		swap_check = dmic_prm[1]->pdm[i].enable_mic_a ||
			dmic_prm[0]->pdm[i].enable_mic_a;

		if (swap_check && swap[i])
			ret = -EINVAL;
	}
	return ret;
}

static int configure_registers(struct dai *dai,
			       struct dmic_configuration *cfg)
{
	int stereo[DMIC_HW_CONTROLLERS];
	int swap[DMIC_HW_CONTROLLERS];
	uint32_t val;
	uint32_t ref;
	int32_t ci;
	uint32_t cu;
	int ipm;
	int of0;
	int of1;
	int fir_decim;
	int fir_length;
	int length;
	int edge;
	int soft_reset;
	int cic_mute;
	int fir_mute;
	int i;
	int j;
	int ret;
	int di = dai->index;
	struct dmic_pdata *pdata = dai_get_drvdata(dai);
	int dccomp = 1;
	int array_a = 0;
	int array_b = 0;
	int bfth = 3; /* Should be 3 for 8 entries, 1 is 2 entries */
	int th = 0; /* Used with TIE=1 */

	/* Normal start sequence */
	soft_reset = 1;
	cic_mute = 1;
	fir_mute = 1;

#if (DMIC_HW_VERSION == 2 && DMIC_HW_CONTROLLERS > 2) || DMIC_HW_VERSION == 3
	int source[OUTCONTROLX_IPM_NUMSOURCES];
#endif

	/* pdata is set by dmic_probe(), error if it has not been set */
	if (!pdata) {
		dai_err(dai, "configure_registers(): pdata not set");
		return -EINVAL;
	}

	dai_info(dai, "configuring registers");

	/* OUTCONTROL0 and OUTCONTROL1 */
	of0 = (dmic_prm[0]->fifo_bits == 32) ? 2 : 0;

#if DMIC_HW_FIFOS > 1
	of1 = (dmic_prm[1]->fifo_bits == 32) ? 2 : 0;
#else
	of1 = 0;
#endif

#if DMIC_HW_VERSION == 1 || (DMIC_HW_VERSION == 2 && DMIC_HW_CONTROLLERS <= 2)
	if (di == 0) {
		ipm_helper1(&ipm, 0);
		val = OUTCONTROL0_TIE(0) |
			OUTCONTROL0_SIP(0) |
			OUTCONTROL0_FINIT(1) |
			OUTCONTROL0_FCI(0) |
			OUTCONTROL0_BFTH(bfth) |
			OUTCONTROL0_OF(of0) |
			OUTCONTROL0_IPM(ipm) |
			OUTCONTROL0_TH(th);
		dai_write(dai, OUTCONTROL0, val);
		dai_dbg(dai, "configure_registers(), OUTCONTROL0 = %08x", val);
	} else {
		ipm_helper1(&ipm, 1);
		val = OUTCONTROL1_TIE(0) |
			OUTCONTROL1_SIP(0) |
			OUTCONTROL1_FINIT(1) |
			OUTCONTROL1_FCI(0) |
			OUTCONTROL1_BFTH(bfth) |
			OUTCONTROL1_OF(of1) |
			OUTCONTROL1_IPM(ipm) |
			OUTCONTROL1_TH(th);
		dai_write(dai, OUTCONTROL1, val);
		dai_dbg(dai, "configure_registers(), OUTCONTROL1 = %08x", val);
	}
#endif

#if DMIC_HW_VERSION == 3 || (DMIC_HW_VERSION == 2 && DMIC_HW_CONTROLLERS > 2)
	if (di == 0) {
		ipm_helper2(source, &ipm, 0);
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
		dai_write(dai, OUTCONTROL0, val);
		dai_dbg(dai, "configure_registers(), OUTCONTROL0 = %08x", val);
	} else {
		ipm_helper2(source, &ipm, 1);
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
		dai_write(dai, OUTCONTROL1, val);
		dai_dbg(dai, "configure_registers(), OUTCONTROL1 = %08x", val);
	}
#endif

	/* Mark enabled microphones into private data to be later used
	 * for starting correct parts of the HW.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		pdata->enable[i] = (dmic_prm[di]->pdm[i].enable_mic_b << 1) |
			dmic_prm[di]->pdm[i].enable_mic_a;
	}


	ret = stereo_helper(stereo, swap);
	if (ret < 0) {
		dai_err(dai, "configure_registers(): enable conflict");
		return ret;
	}

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (dmic_active_fifos == 0) {
			/* CIC */
			val = CIC_CONTROL_SOFT_RESET(soft_reset) |
				CIC_CONTROL_CIC_START_B(0) |
				CIC_CONTROL_CIC_START_A(0) |
				CIC_CONTROL_MIC_B_POLARITY(dmic_prm[di]->pdm[i].polarity_mic_a) |
				CIC_CONTROL_MIC_A_POLARITY(dmic_prm[di]->pdm[i].polarity_mic_b) |
				CIC_CONTROL_MIC_MUTE(cic_mute) |
				CIC_CONTROL_STEREO_MODE(stereo[i]);
			dai_write(dai, base[i] + CIC_CONTROL, val);
			dai_dbg(dai, "configure_registers(), CIC_CONTROL = %08x", val);

			val = CIC_CONFIG_CIC_SHIFT(cfg->cic_shift + 8) |
				CIC_CONFIG_COMB_COUNT(cfg->mcic - 1);
			dai_write(dai, base[i] + CIC_CONFIG, val);
			dai_dbg(dai, "configure_registers(), CIC_CONFIG = %08x", val);

			/* Mono right channel mic usage requires swap of PDM channels
			 * since the mono decimation is done with only left channel
			 * processing active.
			 */
			edge = dmic_prm[di]->pdm[i].clk_edge;
			if (swap[i])
				edge = !edge;

			val = MIC_CONTROL_PDM_CLKDIV(cfg->clkdiv - 2) |
				MIC_CONTROL_PDM_SKEW(dmic_prm[di]->pdm[i].skew) |
				MIC_CONTROL_CLK_EDGE(edge) |
				MIC_CONTROL_PDM_EN_B(0) |
				MIC_CONTROL_PDM_EN_A(0);
			dai_write(dai, base[i] + MIC_CONTROL, val);
			dai_dbg(dai, "configure_registers(), MIC_CONTROL = %08x", val);
		} else {
			/* Check that request is compatible with running configuration:
			 * CIC decimation factor and shift value check
			 */
			val = dai_read(dai, base[i] + CIC_CONFIG);
			ref = CIC_CONFIG_CIC_SHIFT(cfg->cic_shift + 8) |
				CIC_CONFIG_COMB_COUNT(cfg->mcic - 1);
			if ((val &
			     (CIC_CONFIG_CIC_SHIFT_MASK | CIC_CONFIG_COMB_COUNT_MASK)) != ref) {
				dai_err(dai, "configure_registers(): CIC_CONFIG %08x block", val);
				return -EINVAL;
			}

			/* Clock divider check */
			val = dai_read(dai, base[i] + MIC_CONTROL);
			ref = MIC_CONTROL_PDM_CLKDIV(cfg->clkdiv - 2);
			if ((val & MIC_CONTROL_PDM_CLKDIV_MASK) != ref) {
				dai_err(dai, "configure_registers(): MIC_CONTROL %08x block", val);
				return -EINVAL;
			}
		}

		if (di == 0) {
			/* FIR A */
			fir_decim = MAX(cfg->mfir_a - 1, 0);
			fir_length = MAX(cfg->fir_a_length - 1, 0);
			val = FIR_CONTROL_A_START(0) |
				FIR_CONTROL_A_ARRAY_START_EN(array_a) |
				FIR_CONTROL_A_DCCOMP(dccomp) |
				FIR_CONTROL_A_MUTE(fir_mute) |
				FIR_CONTROL_A_STEREO(stereo[i]);
			dai_write(dai, base[i] + FIR_CONTROL_A, val);
			dai_dbg(dai, "configure_registers(), FIR_CONTROL_A = %08x", val);

			val = FIR_CONFIG_A_FIR_DECIMATION(fir_decim) |
				FIR_CONFIG_A_FIR_SHIFT(cfg->fir_a_shift) |
				FIR_CONFIG_A_FIR_LENGTH(fir_length);
			dai_write(dai, base[i] + FIR_CONFIG_A, val);
			dai_dbg(dai, "configure_registers(), FIR_CONFIG_A = %08x", val);

			val = DC_OFFSET_LEFT_A_DC_OFFS(DCCOMP_TC0);
			dai_write(dai, base[i] + DC_OFFSET_LEFT_A, val);
			dai_dbg(dai, "configure_registers(), DC_OFFSET_LEFT_A = %08x", val);

			val = DC_OFFSET_RIGHT_A_DC_OFFS(DCCOMP_TC0);
			dai_write(dai, base[i] + DC_OFFSET_RIGHT_A, val);
			dai_dbg(dai, "configure_registers(), DC_OFFSET_RIGHT_A = %08x", val);

			val = OUT_GAIN_LEFT_A_GAIN(0);
			dai_write(dai, base[i] + OUT_GAIN_LEFT_A, val);
			dai_dbg(dai, "configure_registers(), OUT_GAIN_LEFT_A = %08x", val);

			val = OUT_GAIN_RIGHT_A_GAIN(0);
			dai_write(dai, base[i] + OUT_GAIN_RIGHT_A, val);
			dai_dbg(dai, "configure_registers(), OUT_GAIN_RIGHT_A = %08x", val);

			/* Write coef RAM A with scaled coefficient in reverse order */
			length = cfg->fir_a_length;
			for (j = 0; j < length; j++) {
				ci = (int32_t)Q_MULTSR_32X32((int64_t)cfg->fir_a->coef[j],
							     cfg->fir_a_scale, 31,
							     DMIC_FIR_SCALE_Q, DMIC_HW_FIR_COEF_Q);
				cu = FIR_COEF_A(ci);
				dai_write(dai, coef_base_a[i] + ((length - j - 1) << 2), cu);
			}
		}

		if (di == 1) {
			/* FIR B */
			fir_decim = MAX(cfg->mfir_b - 1, 0);
			fir_length = MAX(cfg->fir_b_length - 1, 0);
			val = FIR_CONTROL_B_START(0) |
				FIR_CONTROL_B_ARRAY_START_EN(array_b) |
				FIR_CONTROL_B_DCCOMP(dccomp) |
				FIR_CONTROL_B_MUTE(fir_mute) |
				FIR_CONTROL_B_STEREO(stereo[i]);
			dai_write(dai, base[i] + FIR_CONTROL_B, val);
			dai_dbg(dai, "configure_registers(), FIR_CONTROL_B = %08x", val);

			val = FIR_CONFIG_B_FIR_DECIMATION(fir_decim) |
				FIR_CONFIG_B_FIR_SHIFT(cfg->fir_b_shift) |
				FIR_CONFIG_B_FIR_LENGTH(fir_length);
			dai_write(dai, base[i] + FIR_CONFIG_B, val);
			dai_dbg(dai, "configure_registers(), FIR_CONFIG_B = %08x", val);

			val = DC_OFFSET_LEFT_B_DC_OFFS(DCCOMP_TC0);
			dai_write(dai, base[i] + DC_OFFSET_LEFT_B, val);
			dai_dbg(dai, "configure_registers(), DC_OFFSET_LEFT_B = %08x", val);

			val = DC_OFFSET_RIGHT_B_DC_OFFS(DCCOMP_TC0);
			dai_write(dai, base[i] + DC_OFFSET_RIGHT_B, val);
			dai_dbg(dai, "configure_registers(), DC_OFFSET_RIGHT_B = %08x", val);

			val = OUT_GAIN_LEFT_B_GAIN(0);
			dai_write(dai, base[i] + OUT_GAIN_LEFT_B, val);
			dai_dbg(dai, "configure_registers(), OUT_GAIN_LEFT_B = %08x", val);

			val = OUT_GAIN_RIGHT_B_GAIN(0);
			dai_write(dai, base[i] + OUT_GAIN_RIGHT_B, val);
			dai_dbg(dai, "configure_registers(), OUT_GAIN_RIGHT_B = %08x", val);

			/* Write coef RAM B with scaled coefficient in reverse order */
			length = cfg->fir_b_length;
			for (j = 0; j < length; j++) {
				ci = (int32_t)Q_MULTSR_32X32((int64_t)cfg->fir_b->coef[j],
							     cfg->fir_b_scale, 31,
							     DMIC_FIR_SCALE_Q, DMIC_HW_FIR_COEF_Q);
				cu = FIR_COEF_B(ci);
				dai_write(dai, coef_base_b[i] + ((length - j - 1) << 2), cu);
			}
		}
	}

	return 0;
}

/* get DMIC hw params */
static int dmic_get_hw_params(struct dai *dai,
			      struct sof_ipc_stream_params  *params, int dir)
{
	int di = dai->index;

	params->rate = dmic_prm[di]->fifo_fs;
	params->buffer_fmt = 0;

	switch (dmic_prm[di]->num_pdm_active) {
	case 1:
		params->channels = 2;
		break;
	case 2:
		params->channels = 4;
		break;
	default:
		dai_info(dai, "dmic_get_hw_params(): not supported channels amount");
		return -EINVAL;
	}

	switch (dmic_prm[di]->fifo_bits) {
	case 16:
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
		break;
	case 32:
		params->frame_fmt = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		dai_err(dai, "dmic_get_hw_params(): not supported format");
		return -EINVAL;
	}

	return 0;
}

static int dmic_set_config(struct dai *dai, struct sof_ipc_dai_config *config)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	struct matched_modes modes_ab;
	struct dmic_configuration cfg;
	struct decim_modes modes_a;
	struct decim_modes modes_b;
	int32_t unmute_ramp_time_ms;
	int32_t step_db;
	size_t size;
	int i, j, ret = 0;
	int di = dai->index;

	dai_info(dai, "dmic_set_config()");

	if (config->dmic.driver_ipc_version != DMIC_IPC_VERSION) {
		dai_err(dai, "dmic_set_config(): wrong ipc version");
		return -EINVAL;
	}

	spin_lock(&dai->lock);

	/* Compute unmute ramp gain update coefficient. Use the value from
	 * topology if it is non-zero, otherwise use default length.
	 */
	if (config->dmic.unmute_ramp_time)
		unmute_ramp_time_ms = config->dmic.unmute_ramp_time;
	else
		unmute_ramp_time_ms = LOGRAMP_TIME_MS;

	if (unmute_ramp_time_ms < LOGRAMP_TIME_MIN_MS ||
	    unmute_ramp_time_ms > LOGRAMP_TIME_MAX_MS) {
		dai_err(dai, "dmic_set_config(): Illegal ramp time = %d",
			unmute_ramp_time_ms);
		ret = -EINVAL;
		goto out;
	}

	if (di >= DMIC_HW_FIFOS) {
		dai_err(dai, "dmic_set_config(): dai->index exceeds number of FIFOs");
		ret = -EINVAL;
		goto out;
	}

	if (config->dmic.num_pdm_active > DMIC_HW_CONTROLLERS) {
		dai_err(dai, "dmic_set_config(): the requested PDM controllers count exceeds platform capability");
		ret = -EINVAL;
		goto out;
	}

	step_db = LOGRAMP_CONST_TERM / unmute_ramp_time_ms;
	dmic->gain_coef = db2lin_fixed(step_db);
	dai_info(dai, "dmic_set_config(): unmute_ramp_time_ms = %d",
		 unmute_ramp_time_ms);


	/*
	 * "config" might contain pdm controller params for only
	 * the active controllers
	 * "prm" is initialized with default params for all HW controllers
	 */
	if (!dmic_prm[0]) {
		size = sizeof(struct sof_ipc_dai_dmic_params);
		dmic_prm[0] = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0,
				      SOF_MEM_CAPS_RAM,
				      DMIC_HW_FIFOS * size);
		if (!dmic_prm[0]) {
			dai_err(dai, "dmic_set_config(): prm not initialized");
			ret = -ENOMEM;
			goto out;
		}
		for (i = 1; i < DMIC_HW_FIFOS; i++)
			dmic_prm[i] = (struct sof_ipc_dai_dmic_params *)
				((uint8_t *)dmic_prm[i - 1] + size);
	}

	/* Copy the new DMIC params header (all but not pdm[]) to persistent.
	 * The last arrived request determines the parameters.
	 */
	ret = memcpy_s(dmic_prm[di], sizeof(*dmic_prm[di]), &config->dmic,
		       offsetof(struct sof_ipc_dai_dmic_params, pdm));
	assert(!ret);

	/* copy the pdm controller params from ipc */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		dmic_prm[di]->pdm[i].id = i;
		for (j = 0; j < config->dmic.num_pdm_active; j++) {
			/* copy the pdm controller params id the id's match */
			if (dmic_prm[di]->pdm[i].id == config->dmic.pdm[j].id) {
				ret = memcpy_s(&dmic_prm[di]->pdm[i],
					       sizeof(dmic_prm[di]->pdm[i]),
					       &config->dmic.pdm[j],
					       sizeof(
					       struct sof_ipc_dai_dmic_pdm_ctrl));
				assert(!ret);
			}
		}
	}

	dai_info(dai, "dmic_set_config(), prm config->dmic.num_pdm_active = %u",
		 config->dmic.num_pdm_active);
	dai_info(dai, "dmic_set_config(), prm pdmclk_min = %u, pdmclk_max = %u",
		 dmic_prm[di]->pdmclk_min, dmic_prm[di]->pdmclk_max);
	dai_info(dai, "dmic_set_config(), prm duty_min = %u, duty_max = %u",
		 dmic_prm[di]->duty_min, dmic_prm[di]->duty_max);
	dai_info(dai, "dmic_set_config(), prm fifo_fs = %u, fifo_bits = %u",
		 dmic_prm[di]->fifo_fs, dmic_prm[di]->fifo_bits);

	switch (dmic_prm[di]->fifo_bits) {
	case 0:
	case 16:
	case 32:
		break;
	default:
		dai_err(dai, "dmic_set_config(): fifo_bits EINVAL");
		ret = -EINVAL;
		goto out;
	}

	/* Match and select optimal decimators configuration for FIFOs A and B
	 * paths. This setup phase is still abstract. Successful completion
	 * points struct cfg to FIR coefficients and contains the scale value
	 * to use for FIR coefficient RAM write as well as the CIC and FIR
	 * shift values.
	 */
	find_modes(dai, &modes_a, dmic_prm[0]->fifo_fs, di);
	if (modes_a.num_of_modes == 0 && dmic_prm[0]->fifo_fs > 0) {
		dai_err(dai, "dmic_set_config(): No modes found found for FIFO A");
		ret = -EINVAL;
		goto out;
	}

	find_modes(dai, &modes_b, dmic_prm[1]->fifo_fs, di);
	if (modes_b.num_of_modes == 0 && dmic_prm[1]->fifo_fs > 0) {
		dai_err(dai, "dmic_set_config(): No modes found for FIFO B");
		ret = -EINVAL;
		goto out;
	}

	match_modes(&modes_ab, &modes_a, &modes_b);
	ret = select_mode(dai, &cfg, &modes_ab);
	if (ret < 0) {
		dai_err(dai, "dmic_set_config(): select_mode() failed");
		ret = -EINVAL;
		goto out;
	}

	dai_info(dai, "dmic_set_config(), cfg clkdiv = %u, mcic = %u",
		 cfg.clkdiv, cfg.mcic);
	dai_info(dai, "dmic_set_config(), cfg mfir_a = %u, mfir_b = %u",
		 cfg.mfir_a, cfg.mfir_b);
	dai_info(dai, "dmic_set_config(), cfg cic_shift = %u",
		 cfg.cic_shift);
	dai_info(dai, "dmic_set_config(), cfg fir_a_shift = %u, cfg.fir_b_shift = %u",
		 cfg.fir_a_shift, cfg.fir_b_shift);
	dai_info(dai, "dmic_set_config(), cfg fir_a_length = %u, fir_b_length = %u",
		 cfg.fir_a_length, cfg.fir_b_length);

	/* Struct reg contains a mirror of actual HW registers. Determine
	 * register bits configuration from decimator configuration and the
	 * requested parameters.
	 */
	ret = configure_registers(dai, &cfg);
	if (ret < 0) {
		dai_err(dai, "dmic_set_config(): cannot configure registers");
		ret = -EINVAL;
		goto out;
	}

	dmic->state = COMP_STATE_PREPARE;

out:
	spin_unlock(&dai->lock);

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
	spin_lock(&dai->lock);
	dai_dbg(dai, "dmic_start()");
	dmic->startcount = 0;

	/* Initial gain value, convert Q12.20 to Q2.30 */
	dmic->gain = Q_SHIFT_LEFT(db2lin_fixed(LOGRAMP_START_DB), 20, 30);

	switch (dai->index) {
	case 0:
		dai_info(dai, "dmic_start(), dmic->fifo_a");
		/*  Clear FIFO A initialize, Enable interrupts to DSP,
		 *  Start FIFO A packer.
		 */
		dai_update_bits(dai, OUTCONTROL0,
				OUTCONTROL0_FINIT_BIT | OUTCONTROL0_SIP_BIT,
				OUTCONTROL0_SIP_BIT);
		break;
	case 1:
		dai_info(dai, "dmic_start(), dmic->fifo_b");
		/*  Clear FIFO B initialize, Enable interrupts to DSP,
		 *  Start FIFO B packer.
		 */
		dai_update_bits(dai, OUTCONTROL1,
				OUTCONTROL1_FINIT_BIT | OUTCONTROL1_SIP_BIT,
				OUTCONTROL1_SIP_BIT);
	}

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		mic_a = dmic->enable[i] & 1;
		mic_b = (dmic->enable[i] & 2) >> 1;
		if (dmic_prm[0]->fifo_fs > 0)
			fir_a = (dmic->enable[i] > 0) ? 1 : 0;
		else
			fir_a = 0;

#if DMIC_HW_FIFOS > 1
		if (dmic_prm[1]->fifo_fs > 0)
			fir_b = (dmic->enable[i] > 0) ? 1 : 0;
		else
			fir_b = 0;
#else
		fir_b = 0;
#endif
		dai_info(dai, "dmic_start(), mic_a = %u, mic_b = %u, fir_a = %u, fir_b = %u",
			 mic_a, mic_b, fir_a, fir_b);

		/* If both microphones are needed start them simultaneously
		 * to start them in sync. The reset may be cleared for another
		 * FIFO already. If only one mic, start them independently.
		 * This makes sure we do not clear start/en for another DAI.
		 */
		if (mic_a && mic_b) {
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_CIC_START_A_BIT |
					CIC_CONTROL_CIC_START_B_BIT,
					CIC_CONTROL_CIC_START_A(1) |
					CIC_CONTROL_CIC_START_B(1));
			dai_update_bits(dai, base[i] + MIC_CONTROL,
					MIC_CONTROL_PDM_EN_A_BIT |
					MIC_CONTROL_PDM_EN_B_BIT,
					MIC_CONTROL_PDM_EN_A(1) |
					MIC_CONTROL_PDM_EN_B(1));
		} else if (mic_a) {
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_CIC_START_A_BIT,
					CIC_CONTROL_CIC_START_A(1));
			dai_update_bits(dai, base[i] + MIC_CONTROL,
					MIC_CONTROL_PDM_EN_A_BIT,
					MIC_CONTROL_PDM_EN_A(1));
		} else if (mic_b) {
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_CIC_START_B_BIT,
					CIC_CONTROL_CIC_START_B(1));
			dai_update_bits(dai, base[i] + MIC_CONTROL,
					MIC_CONTROL_PDM_EN_B_BIT,
					MIC_CONTROL_PDM_EN_B(1));
		}

		switch (dai->index) {
		case 0:
			dai_update_bits(dai, base[i] + FIR_CONTROL_A,
					FIR_CONTROL_A_START_BIT,
					FIR_CONTROL_A_START(fir_a));
			break;
		case 1:
			dai_update_bits(dai, base[i] + FIR_CONTROL_B,
					FIR_CONTROL_B_START_BIT,
					FIR_CONTROL_B_START(fir_b));
			break;
		}
	}

	/* Clear soft reset for all/used PDM controllers. This should
	 * start capture in sync.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		dai_update_bits(dai, base[i] + CIC_CONTROL,
				CIC_CONTROL_SOFT_RESET_BIT, 0);
	}

	if (dmic->state == COMP_STATE_PREPARE)
		dmic_active_fifos++;

	dmic->state = COMP_STATE_ACTIVE;

	spin_unlock(&dai->lock);

	/* Currently there's no DMIC HW internal mutings and wait times
	 * applied into this start sequence. It can be implemented here if
	 * start of audio capture would contain clicks and/or noise and it
	 * is not suppressed by gain ramp somewhere in the capture pipe.
	 */

	schedule_task(&dmic->dmicwork, DMIC_UNMUTE_RAMP_US,
		      DMIC_UNMUTE_RAMP_US);


	dai_info(dai, "dmic_start(), done active_fifos = %d",
		 dmic_active_fifos);
}

/* stop the DMIC for capture */
static void dmic_stop(struct dai *dai)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int i;

	dai_dbg(dai, "dmic_stop()");
	spin_lock(&dai->lock);

	/* Stop FIFO packers and set FIFO initialize bits */
	switch (dai->index) {
	case 0:
		dai_update_bits(dai, OUTCONTROL0,
				OUTCONTROL0_SIP_BIT | OUTCONTROL0_FINIT_BIT,
				OUTCONTROL0_FINIT_BIT);
		break;
	case 1:
		dai_update_bits(dai, OUTCONTROL1,
				OUTCONTROL1_SIP_BIT | OUTCONTROL1_FINIT_BIT,
				OUTCONTROL1_FINIT_BIT);
		break;
	}

	/* Set soft reset and mute on for all PDM controllers.
	 */
	dai_info(dai, "dmic_stop(), dmic_active_fifos = %d",
		 dmic_active_fifos);

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		/* Don't stop CIC yet if both FIFOs were active */
		if (dmic_active_fifos == 1) {
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_SOFT_RESET_BIT |
					CIC_CONTROL_MIC_MUTE_BIT,
					CIC_CONTROL_SOFT_RESET_BIT |
					CIC_CONTROL_MIC_MUTE_BIT);
		}
		switch (dai->index) {
		case 0:
			dai_update_bits(dai, base[i] + FIR_CONTROL_A,
					FIR_CONTROL_A_MUTE_BIT,
					FIR_CONTROL_A_MUTE_BIT);
			break;
		case 1:
			dai_update_bits(dai, base[i] + FIR_CONTROL_B,
					FIR_CONTROL_B_MUTE_BIT,
					FIR_CONTROL_B_MUTE_BIT);
			break;
		}
	}

	if (dmic->state == COMP_STATE_PREPARE)
		dmic_active_fifos--;

	schedule_task_cancel(&dmic->dmicwork);
	spin_unlock(&dai->lock);
}

/* save DMIC context prior to entering D3 */
static int dmic_context_store(struct dai *dai)
{
	/* Nothing stored at the moment. */
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

	dai_dbg(dai, "dmic_trigger()");

	/* dai private is set in dmic_probe(), error if not set */
	if (!dmic) {
		dai_err(dai, "dmic_trigger(): dai not set");
		return -EINVAL;
	}

	if (direction != DAI_DIR_CAPTURE) {
		dai_err(dai, "dmic_trigger(): direction != DAI_DIR_CAPTURE");
		return -EINVAL;
	}

	switch (cmd) {
	case COMP_TRIGGER_RELEASE:
	case COMP_TRIGGER_START:
		if (dmic->state == COMP_STATE_PREPARE ||
		    dmic->state == COMP_STATE_PAUSED) {
			dmic_start(dai);
		} else {
			dai_err(dai, "dmic_trigger(): state is not prepare or paused, dmic->state = %u",
				dmic->state);
		}
		break;
	case COMP_TRIGGER_STOP:
		dmic->state = COMP_STATE_PREPARE;
		dmic_stop(dai);
		break;
	case COMP_TRIGGER_PAUSE:
		dmic->state = COMP_STATE_PAUSED;
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
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	uint32_t val0;
	uint32_t val1;

	/* Trace OUTSTAT0 register */
	val0 = dai_read(dai, OUTSTAT0);
	val1 = dai_read(dai, OUTSTAT1);
	dai_info(dai, "dmic_irq_handler(), OUTSTAT0 = 0x%x, OUTSTAT1 = 0x%x",
		 val0, val1);

	if (val0 & OUTSTAT0_ROR_BIT) {
		dai_err(dai, "dmic_irq_handler(): full fifo A or PDM overrun");
		dai_write(dai, OUTSTAT0, val0);
		dmic->state = COMP_STATE_PREPARE;
		dmic_stop(dai);
	}

	if (val1 & OUTSTAT1_ROR_BIT) {
		dai_err(dai, "dmic_irq_handler(): full fifo B or PDM overrun");
		dai_write(dai, OUTSTAT1, val1);
		dmic->state = COMP_STATE_PREPARE;
		dmic_stop(dai);
	}
}

static int dmic_probe(struct dai *dai)
{
	int irq = dmic_irq(dai);
	struct dmic_pdata *dmic;
	int ret;

	dai_info(dai, "dmic_probe()");

	if (dai_get_drvdata(dai))
		return -EEXIST; /* already created */

	/* allocate private data */
	dmic = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dmic));
	if (!dmic) {
		dai_err(dai, "dmic_probe(): alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, dmic);

	/* Set state, note there is no playback direction support */
	dmic->state = COMP_STATE_READY;

	/* register our IRQ handler */
	dmic->irq = interrupt_get_irq(irq, dmic_irq_name(dai));
	if (dmic->irq < 0) {
		ret = dmic->irq;
		rfree(dmic);
		return ret;
	}

	ret = interrupt_register(dmic->irq, dmic_irq_handler, dai);
	if (ret < 0) {
		dai_err(dai, "dmic failed to allocate IRQ");
		rfree(dmic);
		return ret;
	}

	/* Initialize start sequence handler */
	schedule_task_init_ll(&dmic->dmicwork, SOF_UUID(dmic_work_task_uuid),
			      SOF_SCHEDULE_LL_TIMER,
			      SOF_TASK_PRI_MED, dmic_work, dai, 0, 0);

	/* Enable DMIC power */
	pm_runtime_get_sync(DMIC_POW, dai->index);
	/* Disable dynamic clock gating for dmic before touching any reg */
	pm_runtime_get_sync(DMIC_CLK, dai->index);

	interrupt_enable(dmic->irq, dai);


	return 0;
}

static int dmic_remove(struct dai *dai)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int i;

	dai_info(dai, "dmic_remove()");

	/* remove scheduling */
	schedule_task_free(&dmic->dmicwork);

	rfree(dai_get_drvdata(dai));
	dai_set_drvdata(dai, NULL);

	interrupt_disable(dmic->irq, dai);
	interrupt_unregister(dmic->irq, dai);

	/* The next end tasks must be passed if another DAI FIFO still runs */
	if (dmic_active_fifos)
		return 0;

	pm_runtime_put_sync(DMIC_CLK, dai->index);
	/* Disable DMIC power */
	pm_runtime_put_sync(DMIC_POW, dai->index);

	rfree(dmic_prm[0]);
	for (i = 0; i < DMIC_HW_FIFOS; i++)
		dmic_prm[i] = NULL;

	return 0;
}

static int dmic_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[SOF_IPC_STREAM_CAPTURE].handshake;
}

static int dmic_get_fifo(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[SOF_IPC_STREAM_CAPTURE].offset;
}

const struct dai_driver dmic_driver = {
	.type = SOF_DAI_INTEL_DMIC,
	.uid = SOF_UUID(dmic_uuid),
	.tctx = &dmic_tr,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_DMIC,
	.ops = {
		.trigger		= dmic_trigger,
		.set_config		= dmic_set_config,
		.get_hw_params		= dmic_get_hw_params,
		.pm_context_store	= dmic_context_store,
		.pm_context_restore	= dmic_context_restore,
		.get_handshake		= dmic_get_handshake,
		.get_fifo		= dmic_get_fifo,
		.probe			= dmic_probe,
		.remove			= dmic_remove,
	},
	.ts_ops = {
		.ts_config		= timestamp_dmic_config,
		.ts_start		= timestamp_dmic_start,
		.ts_get			= timestamp_dmic_get,
		.ts_stop		= timestamp_dmic_stop,
	},
};

#endif
