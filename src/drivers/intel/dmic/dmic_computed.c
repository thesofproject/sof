// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017-2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/drivers/dmic.h>
#include <sof/math/numbers.h>
#include <ipc/dai.h>
#include <ipc/dai-intel.h>
#include <stdint.h>

/* Decimation filter struct */
#include <sof/audio/coefficients/pdm_decim/pdm_decim_fir.h>

/* Decimation filters */
#include <sof/audio/coefficients/pdm_decim/pdm_decim_table.h>

LOG_MODULE_DECLARE(dmic_dai, CONFIG_SOF_LOG_LEVEL);

/* Base addresses (in PDM scope) of 2ch PDM controllers and coefficient RAM. */
static const uint32_t base[4] = {PDM0, PDM1, PDM2, PDM3};
static const uint32_t coef_base_a[4] = {PDM0_COEFFICIENT_A, PDM1_COEFFICIENT_A,
					PDM2_COEFFICIENT_A, PDM3_COEFFICIENT_A};
static const uint32_t coef_base_b[4] = {PDM0_COEFFICIENT_B, PDM1_COEFFICIENT_B,
					PDM2_COEFFICIENT_B, PDM3_COEFFICIENT_B};

/* This function returns a raw list of potential microphone clock and decimation
 * modes for achieving requested sample rates. The search is constrained by
 * decimation HW capabililies and setup parameters. The parameters such as
 * microphone clock min/max and duty cycle requirements need be checked from
 * used microphone component datasheet.
 */
static void find_modes(struct dai *dai,
		       struct decim_modes *modes, uint32_t fs, int di)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
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
	if (dmic->global->prm[di].pdmclk_max < DMIC_HW_PDM_CLK_MIN ||
	    dmic->global->prm[di].pdmclk_max > CONFIG_DMIC_HW_IOCLK / 2) {
		dai_err(dai, "find_modes():  pdm clock max not in range");
		return;
	}
	if (dmic->global->prm[di].pdmclk_min < DMIC_HW_PDM_CLK_MIN ||
	    dmic->global->prm[di].pdmclk_min > dmic->global->prm[di].pdmclk_max) {
		dai_err(dai, "find_modes():  pdm clock min not in range");
		return;
	}

	/* Check for sane duty cycle */
	if (dmic->global->prm[di].duty_min > dmic->global->prm[di].duty_max) {
		dai_err(dai, "find_modes(): duty cycle min > max");
		return;
	}
	if (dmic->global->prm[di].duty_min < DMIC_HW_DUTY_MIN ||
	    dmic->global->prm[di].duty_min > DMIC_HW_DUTY_MAX) {
		dai_err(dai, "find_modes():  pdm clock min not in range");
		return;
	}
	if (dmic->global->prm[di].duty_max < DMIC_HW_DUTY_MIN ||
	    dmic->global->prm[di].duty_max > DMIC_HW_DUTY_MAX) {
		dai_err(dai, "find_modes(): pdm clock max not in range");
		return;
	}

	/* Min and max clock dividers */
	clkdiv_min = ceil_divide(CONFIG_DMIC_HW_IOCLK, dmic->global->prm[di].pdmclk_max);
	clkdiv_min = MAX(clkdiv_min, DMIC_HW_CIC_DECIM_MIN);
	clkdiv_max = CONFIG_DMIC_HW_IOCLK / dmic->global->prm[di].pdmclk_min;

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
		pdmclk = CONFIG_DMIC_HW_IOCLK / clkdiv;
		osr = pdmclk / fs;

		/* Check that OSR constraints is met and clock duty cycle does
		 * not exceed microphone specification. If exceed proceed to
		 * next clkdiv.
		 */
		if (osr < osr_min || du_min < dmic->global->prm[di].duty_min ||
		    du_max > dmic->global->prm[di].duty_max)
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
			if (j != 0 && fir_list[j - 1]->decim_factor == mfir)
				continue;

			mcic = osr / mfir;
			ioclk_test = fs * mfir * mcic * clkdiv;

			if (ioclk_test == CONFIG_DMIC_HW_IOCLK &&
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

	cic_fs = CONFIG_DMIC_HW_IOCLK / cfg->clkdiv / cfg->mcic;
	fs = cic_fs / mfir;
	/* FIR max. length depends on available cycles and coef RAM
	 * length. Exceeding this length sets HW overrun status and
	 * overwrite of other register.
	 */
	fir_max_length = MIN(DMIC_HW_FIR_LENGTH_MAX,
			     CONFIG_DMIC_HW_IOCLK / fs / 2 -
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
#ifdef DMIC_IPM_VER1

static void ipm_helper1(struct dmic_pdata *dmic, int *ipm, int di)
{
	int pdm[DMIC_HW_CONTROLLERS];
	int i;

	/* Loop number of PDM controllers in the configuration. If mic A
	 * or B is enabled then a pdm controller is marked as active for
	 * this DAI.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (dmic->global->prm[di].pdm[i].enable_mic_a ||
		    dmic->global->prm[di].pdm[i].enable_mic_b)
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

#endif

#ifdef DMIC_IPM_VER2

static inline void ipm_helper2(struct dmic_pdata *dmic, int source[], int *ipm, int di)
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
		if (dmic->global->prm[di].pdm[i].enable_mic_a ||
		    dmic->global->prm[di].pdm[i].enable_mic_b) {
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
static int stereo_helper(struct dmic_pdata *dmic, int stereo[], int swap[])
{
	int cnt;
	int i;
	int swap_check;
	int ret = 0;

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		cnt = 0;
		if (dmic->global->prm[0].pdm[i].enable_mic_a ||
		    dmic->global->prm[1].pdm[i].enable_mic_a)
			cnt++;

		if (dmic->global->prm[0].pdm[i].enable_mic_b ||
		    dmic->global->prm[1].pdm[i].enable_mic_b)
			cnt++;

		/* Set stereo mode if both mic A anc B are enabled. */
		cnt >>= 1;
		stereo[i] = cnt;

		/* Swap channels if only mic B is used for mono processing. */
		swap[i] = (dmic->global->prm[0].pdm[i].enable_mic_b ||
			dmic->global->prm[1].pdm[i].enable_mic_b) && !cnt;

		/* Check that swap does not conflict with other DAI request */
		swap_check = dmic->global->prm[1].pdm[i].enable_mic_a ||
			dmic->global->prm[0].pdm[i].enable_mic_a;

		if (swap_check && swap[i])
			ret = -EINVAL;
	}
	return ret;
}

static int configure_registers(struct dai *dai,
			       struct dmic_configuration *cfg)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int stereo[DMIC_HW_CONTROLLERS];
	int swap[DMIC_HW_CONTROLLERS];
	uint32_t val;
	uint32_t ref;
	int32_t ci;
	uint32_t cu;
#if defined(DMIC_IPM_VER1) || defined(DMIC_IPM_VER2)
	int ipm;
#endif
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
	int pol_a;
	int pol_b;
	int di = dai->index;
	int dccomp = 1;
	int array_a = 0;
	int array_b = 0;
	int bfth = 3; /* Should be 3 for 8 entries, 1 is 2 entries */
	int th = 0; /* Used with TIE=1 */

	/* Normal start sequence */
	soft_reset = 1;
	cic_mute = 1;
	fir_mute = 1;

#ifdef DMIC_IPM_VER2
	int source[OUTCONTROLX_IPM_NUMSOURCES];
#endif

	/* pdata is set by dmic_probe(), error if it has not been set */
	if (!dmic) {
		dai_err(dai, "configure_registers(): pdata not set");
		return -EINVAL;
	}

	/* Pass 2^BFTH to plat_data fifo depth. It will be used later in DMA
	 * configuration
	 */
	dai->plat_data.fifo->depth = 1 << bfth;

	dai_info(dai, "configuring registers");

	/* OUTCONTROL0 and OUTCONTROL1 */
	of0 = (dmic->global->prm[0].fifo_bits == 32) ? 2 : 0;

#if DMIC_HW_FIFOS > 1
	of1 = (dmic->global->prm[1].fifo_bits == 32) ? 2 : 0;
#else
	of1 = 0;
#endif

#ifdef DMIC_IPM_VER1
	if (di == 0) {
		ipm_helper1(dmic, &ipm, 0);
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
		ipm_helper1(dmic, &ipm, 1);
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

#ifdef DMIC_IPM_VER2
	if (di == 0) {
		ipm_helper2(dmic, source, &ipm, 0);
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
		ipm_helper2(dmic, source, &ipm, 1);
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
		dmic->enable[i] = (dmic->global->prm[di].pdm[i].enable_mic_b << 1) |
			dmic->global->prm[di].pdm[i].enable_mic_a;
	}


	ret = stereo_helper(dmic, stereo, swap);
	if (ret < 0) {
		dai_err(dai, "configure_registers(): enable conflict");
		return ret;
	}

	/* Note about accessing dmic_active_fifos_mask: the dai spinlock has been set in
	 * the calling function dmic_set_config().
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (dmic->global->active_fifos_mask == 0) {
			/* CIC */
			pol_a = dmic->global->prm[di].pdm[i].polarity_mic_a;
			pol_b = dmic->global->prm[di].pdm[i].polarity_mic_b;
			val = CIC_CONTROL_SOFT_RESET(soft_reset) |
				CIC_CONTROL_CIC_START_B(0) |
				CIC_CONTROL_CIC_START_A(0) |
				CIC_CONTROL_MIC_B_POLARITY(pol_b) |
				CIC_CONTROL_MIC_A_POLARITY(pol_a) |
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
			edge = dmic->global->prm[di].pdm[i].clk_edge;
			if (swap[i])
				edge = !edge;

			val = MIC_CONTROL_PDM_CLKDIV(cfg->clkdiv - 2) |
				MIC_CONTROL_PDM_SKEW(dmic->global->prm[di].pdm[i].skew) |
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
int dmic_get_hw_params_computed(struct dai *dai, struct sof_ipc_stream_params *params, int dir)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int di = dai->index;

	if (!dmic) {
		dai_err(dai, "dmic_get_hw_params(): dai %d not configured!", di);
		return -EINVAL;
	}
	params->rate = dmic->global->prm[di].fifo_fs;
	params->buffer_fmt = 0;

	switch (dmic->global->prm[di].num_pdm_active) {
	case 1:
		params->channels = 2;
		break;
	case 2:
		params->channels = 4;
		break;
	default:
		dai_info(dai, "dmic_get_hw_params(): not supported PDM active count %d",
			 dmic->global->prm[di].num_pdm_active);
		return -EINVAL;
	}

	switch (dmic->global->prm[di].fifo_bits) {
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

int dmic_set_config_computed(struct dai *dai)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	struct matched_modes modes_ab;
	struct dmic_configuration cfg;
	struct decim_modes modes_a;
	struct decim_modes modes_b;
	int ret;
	int di = dai->index;

	dai_info(dai, "dmic_set_config(), prm config->dmic.num_pdm_active = %u",
		 dmic->global->prm[di].num_pdm_active);
	dai_info(dai, "dmic_set_config(), prm pdmclk_min = %u, pdmclk_max = %u",
		 dmic->global->prm[di].pdmclk_min, dmic->global->prm[di].pdmclk_max);
	dai_info(dai, "dmic_set_config(), prm duty_min = %u, duty_max = %u",
		 dmic->global->prm[di].duty_min, dmic->global->prm[di].duty_max);
	dai_info(dai, "dmic_set_config(), prm fifo_fs = %u, fifo_bits = %u",
		 dmic->global->prm[di].fifo_fs, dmic->global->prm[di].fifo_bits);

	switch (dmic->global->prm[di].fifo_bits) {
	case 0:
	case 16:
	case 32:
		break;
	default:
		dai_err(dai, "dmic_set_config_computed(): invalid fifo_bits");
		return -EINVAL;
	}

	/* Match and select optimal decimators configuration for FIFOs A and B
	 * paths. This setup phase is still abstract. Successful completion
	 * points struct cfg to FIR coefficients and contains the scale value
	 * to use for FIR coefficient RAM write as well as the CIC and FIR
	 * shift values.
	 */
	find_modes(dai, &modes_a, dmic->global->prm[0].fifo_fs, di);
	if (modes_a.num_of_modes == 0 && dmic->global->prm[0].fifo_fs > 0) {
		dai_err(dai, "dmic_set_config(): No modes found found for FIFO A");
		return -EINVAL;
	}

	find_modes(dai, &modes_b, dmic->global->prm[1].fifo_fs, di);
	if (modes_b.num_of_modes == 0 && dmic->global->prm[1].fifo_fs > 0) {
		dai_err(dai, "dmic_set_config(): No modes found for FIFO B");
		return -EINVAL;
	}

	match_modes(&modes_ab, &modes_a, &modes_b);
	ret = select_mode(dai, &cfg, &modes_ab);
	if (ret < 0) {
		dai_err(dai, "dmic_set_config(): select_mode() failed");
		return -EINVAL;
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

	/* Determine register bits configuration from decimator configuration and
	 * the requested parameters.
	 */
	ret = configure_registers(dai, &cfg);
	if (ret < 0) {
		dai_err(dai, "dmic_set_config(): cannot configure registers");
		ret = -EINVAL;
	}

	return ret;
}
