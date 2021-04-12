// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/drivers/mn.h>
#include <sof/drivers/ssp.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>

#include <cavs/version.h>

#include <stdbool.h>
#include <stdint.h>

/* tracing */

/* fa3b3763-759c-4c64-82b6-3dd239c89f58 */
DECLARE_SOF_UUID("mn", mn_uuid, 0xfa3b3763, 0x759c, 0x4c64,
		 0x82, 0xb6, 0x3d, 0xd2, 0x39, 0xc8, 0x9f, 0x58);

DECLARE_TR_CTX(mn_tr, SOF_UUID(mn_uuid), LOG_LEVEL_INFO);

#if CONFIG_INTEL_MN
/** \brief BCLKs can be driven by multiple sources - M/N or XTAL directly.
 *	   Even in the case of M/N, the actual clock source can be XTAL,
 *	   Audio cardinal clock (24.576) or 96 MHz PLL.
 *	   The MN block is not really the source of clocks, but rather
 *	   an intermediate component.
 *	   Input for source is shared by all outputs coming from that source
 *	   and once it's in use, it can be adjusted only with dividers.
 *	   In order to change input, the source should not be in use, that's why
 *	   it's necessary to keep track of BCLKs sources to know when it's safe
 *	   to change shared input clock.
 */
enum bclk_source {
	MN_BCLK_SOURCE_NONE = 0, /**< port is not using any clock */
	MN_BCLK_SOURCE_MN, /**< port is using clock driven by M/N */
	MN_BCLK_SOURCE_XTAL, /**< port is using XTAL directly */
};
#endif

struct mn {
	/**< keep track of which MCLKs are in use to know when it's safe to
	 * change shared clock
	 */
	int mclk_sources_ref[DAI_NUM_SSP_MCLK];
	int mclk_rate[DAI_NUM_SSP_MCLK];
	int mclk_source_clock;

#if CONFIG_INTEL_MN
	enum bclk_source bclk_sources[(DAI_NUM_SSP_BASE + DAI_NUM_SSP_EXT)];
	int bclk_source_mn_clock;
#endif

	spinlock_t lock; /**< lock mechanism */
};

static SHARED_DATA struct mn mn;

void mn_init(struct sof *sof)
{
	int i;

	sof->mn = platform_shared_get(&mn, sizeof(mn));

	sof->mn->mclk_source_clock = 0;

	/* initialize the ref counts for mclk */
	for (i = 0; i < DAI_NUM_SSP_MCLK; i++) {
		sof->mn->mclk_sources_ref[i] = 0;
		sof->mn->mclk_rate[i] = 0;
	}

#if CONFIG_INTEL_MN
	for (i = 0; i < ARRAY_SIZE(sof->mn->bclk_sources); i++)
		sof->mn->bclk_sources[i] = MN_BCLK_SOURCE_NONE;
#endif

	spinlock_init(&sof->mn->lock);

}

/**
 * \brief Checks if given clock is used as source for any MCLK.
 * \param[in] clk_src MCLK source.
 * \return true if any port use given clock source, false otherwise.
 */
static inline bool is_mclk_source_in_use(void)
{
	struct mn *mn = mn_get();
	bool ret = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(mn->mclk_sources_ref); i++) {
		if (mn->mclk_sources_ref[i] > 0) {
			ret = true;
			break;
		}
	}

	return ret;
}

/**
 * \brief Configures source clock for MCLK.
 *	  All MCLKs share the same source, so it should be changed
 *	  only if there are no other ports using it already.
 * \param[in] mclk_rate main clock frequency.
 * \return 0 on success, error code otherwise.
 */
static inline int setup_initial_mclk_source(uint32_t mclk_id,
					    uint32_t mclk_rate)
{
	struct mn *mn = mn_get();
	int i;
	int clk_index = -1;
	uint32_t mdivc;
	int ret = 0;

	if (mclk_id >= DAI_NUM_SSP_MCLK) {
		tr_err(&mn_tr, "can't configure MCLK %d, only %d mclk[s] existed!",
		       mclk_id, DAI_NUM_SSP_MCLK);
		ret = -EINVAL;
		goto out;
	}

	/* searching the smallest possible mclk source */
	for (i = 0; i <= MAX_SSP_FREQ_INDEX; i++) {
		if (ssp_freq[i].freq % mclk_rate == 0) {
			clk_index = i;
			break;
		}
	}

	if (clk_index < 0) {
		tr_err(&mn_tr, "MCLK %d, no valid source", mclk_rate);
		ret = -EINVAL;
		goto out;
	}

	mn->mclk_source_clock = clk_index;

	mdivc = mn_reg_read(MN_MDIVCTRL, mclk_id);

	/* enable MCLK divider */
	mdivc |= MN_MDIVCTRL_M_DIV_ENABLE(mclk_id);

	/* clear source mclk clock - bits 17-16 */
	mdivc &= ~MCDSS(MN_SOURCE_CLKS_MASK);

	/* select source clock */
	mdivc |= MCDSS(ssp_freq_sources[clk_index]);

	mn_reg_write(MN_MDIVCTRL, mclk_id, mdivc);

	mn->mclk_sources_ref[mclk_id]++;
out:

	return ret;
}

/**
 * \brief Checks if requested MCLK can be achieved with current source.
 * \param[in] mclk_rate main clock frequency.
 * \return 0 on success, error code otherwise.
 */
static inline int check_current_mclk_source(uint16_t mclk_id, uint32_t mclk_rate)
{
	struct mn *mn = mn_get();
	uint32_t mdivc;
	int ret = 0;

	tr_info(&mn_tr, "MCLK %d, source = %d",	mclk_rate, mn->mclk_source_clock);

	if (ssp_freq[mn->mclk_source_clock].freq % mclk_rate != 0) {
		tr_err(&mn_tr, "MCLK %d, no valid configuration for already selected source = %d",
		       mclk_rate, mn->mclk_source_clock);
		ret = -EINVAL;
	}

	/* if the mclk is already used, can't change its divider, just increase ref count */
	if (mn->mclk_sources_ref[mclk_id] > 0) {
		if (mn->mclk_rate[mclk_id] != mclk_rate) {
			tr_err(&mn_tr, "Can't set MCLK %d to %d, it is already configured to %d",
			       mclk_id, mclk_rate, mn->mclk_rate[mclk_id]);
			return -EINVAL;
		}

		mn->mclk_sources_ref[mclk_id]++;
	} else {
		mdivc = mn_reg_read(MN_MDIVCTRL, mclk_id);

		/* enable MCLK divider */
		mdivc |= MN_MDIVCTRL_M_DIV_ENABLE(mclk_id);
		mn_reg_write(MN_MDIVCTRL, mclk_id, mdivc);

		mn->mclk_sources_ref[mclk_id]++;
	}

	return ret;
}

/**
 * \brief Sets MCLK divider to given value.
 * \param[in] mclk_id ID of MCLK.
 * \param[in] mdivr_val divider value.
 * \return 0 on success, error code otherwise.
 */
static inline int set_mclk_divider(uint16_t mclk_id, uint32_t mdivr_val)
{
	uint32_t mdivr;

	tr_info(&mn_tr, "mclk_id %d mdivr_val %d", mclk_id, mdivr_val);
	switch (mdivr_val) {
	case 1:
		mdivr = 0x00000fff; /* bypass divider for MCLK */
		break;
	case 2 ... 8:
		mdivr = mdivr_val - 2; /* 1/n */
		break;
	default:
		tr_err(&mn_tr, "invalid mdivr_val %d", mdivr_val);
		return -EINVAL;
	}

	mn_reg_write(MN_MDIVR(mclk_id), mclk_id, mdivr);
	return 0;
}

int mn_set_mclk(uint16_t mclk_id, uint32_t mclk_rate)
{
	struct mn *mn = mn_get();
	int ret = 0;

	if (mclk_id >= DAI_NUM_SSP_MCLK) {
		tr_err(&mn_tr, "mclk ID (%d) >= %d", mclk_id, DAI_NUM_SSP_MCLK);
		return -EINVAL;
	}

	spin_lock(&mn->lock);

	if (is_mclk_source_in_use())
		ret = check_current_mclk_source(mclk_id, mclk_rate);
	else
		ret = setup_initial_mclk_source(mclk_id, mclk_rate);

	if (ret < 0)
		goto out;

	tr_info(&mn_tr, "mclk_rate %d, mclk_source_clock %d",
		mclk_rate, mn->mclk_source_clock);

	ret = set_mclk_divider(mclk_id,
			       ssp_freq[mn->mclk_source_clock].freq /
			       mclk_rate);
	if (!ret)
		mn->mclk_rate[mclk_id] = mclk_rate;

out:

	spin_unlock(&mn->lock);

	return ret;
}

void mn_release_mclk(uint32_t mclk_id)
{
	struct mn *mn = mn_get();
	uint32_t mdivc;

	spin_lock(&mn->lock);

	mn->mclk_sources_ref[mclk_id]--;

	/* disable MCLK divider if nobody use it */
	if (!mn->mclk_sources_ref[mclk_id]) {
		mdivc = mn_reg_read(MN_MDIVCTRL, mclk_id);

		mdivc |= ~MN_MDIVCTRL_M_DIV_ENABLE(mclk_id);
		mn_reg_write(MN_MDIVCTRL, mclk_id, mdivc);
	}

	/* release the clock source if all mclks are released */
	if (!is_mclk_source_in_use()) {
		mdivc = mn_reg_read(MN_MDIVCTRL, mclk_id);

		/* clear source mclk clock - bits 17-16 */
		mdivc &= ~MCDSS(MN_SOURCE_CLKS_MASK);

		mn_reg_write(MN_MDIVCTRL, mclk_id, mdivc);

		mn->mclk_source_clock = 0;
	}
	spin_unlock(&mn->lock);
}

#if CONFIG_INTEL_MN
/**
 * \brief Finds valid M/(N * SCR) values for given frequencies.
 * \param[in] freq SSP clock frequency.
 * \param[in] bclk Bit clock frequency.
 * \param[out] out_scr_div SCR divisor.
 * \param[out] out_m M value of M/N divider.
 * \param[out] out_n N value of M/N divider.
 * \return true if found suitable values, false otherwise.
 */
static bool find_mn(uint32_t freq, uint32_t bclk,
		    uint32_t *out_scr_div, uint32_t *out_m, uint32_t *out_n)
{
	uint32_t m, n, mn_div;
	uint32_t scr_div = freq / bclk;

	tr_info(&mn_tr, "find_mn for freq %d bclk %d", freq, bclk);
	/* check if just SCR is enough */
	if (freq % bclk == 0 && scr_div < (SSCR0_SCR_MASK >> 8) + 1) {
		*out_scr_div = scr_div;
		*out_m = 1;
		*out_n = 1;

		return true;
	}

	/* M/(N * scr_div) has to be less than 1/2 */
	if ((bclk * 2) >= freq)
		return false;

	/* odd SCR gives lower duty cycle */
	if (scr_div > 1 && scr_div % 2 != 0)
		--scr_div;

	/* clamp to valid SCR range */
	scr_div = MIN(scr_div, (SSCR0_SCR_MASK >> 8) + 1);

	/* find highest even divisor */
	while (scr_div > 1 && freq % scr_div != 0)
		scr_div -= 2;

	/* compute M/N with smallest dividend and divisor */
	mn_div = gcd(bclk, freq / scr_div);

	m = bclk / mn_div;
	n = freq / scr_div / mn_div;

	/* M/N values can be up to 24 bits */
	if (n & (~0xffffff))
		return false;

	*out_scr_div = scr_div;
	*out_m = m;
	*out_n = n;

	tr_info(&mn_tr, "find_mn m %d n %d", m, n);
	return true;
}

/**
 * \brief Finds index of clock valid for given BCLK rate.
 *	  Clock that can use just SCR is preferred.
 *	  M/N other than 1/1 is used only if there are no other possibilities.
 * \param[in] bclk Bit clock frequency.
 * \param[out] scr_div SCR divisor.
 * \param[out] m M value of M/N divider.
 * \param[out] n N value of M/N divider.
 * \return index of suitable clock if could find it, -EINVAL otherwise.
 */
static int find_bclk_source(uint32_t bclk,
			    uint32_t *scr_div, uint32_t *m, uint32_t *n)
{
	struct mn *mn = mn_get();
	int i;

	/* check if we can use MCLK source clock */
	if (is_mclk_source_in_use()) {
		if (find_mn(ssp_freq[mn->mclk_source_clock].freq, bclk, scr_div, m, n))
			return mn->mclk_source_clock;

		tr_warn(&mn_tr, "BCLK %d warning: cannot use MCLK source %d",
			bclk, ssp_freq[mn->mclk_source_clock].freq);
	}

	/* searching the smallest possible bclk source */
	for (i = 0; i <= MAX_SSP_FREQ_INDEX; i++)
		if (ssp_freq[i].freq % bclk == 0) {
			*scr_div = ssp_freq[i].freq / bclk;
			return i;
		}

	/* check if we can get target BCLK with M/N */
	for (i = 0; i <= MAX_SSP_FREQ_INDEX; i++)
		if (find_mn(ssp_freq[i].freq, bclk,
			    scr_div, m, n))
			return i;

	return -EINVAL;
}

/**
 * \brief Finds index of SSP clock with the given clock source encoded index.
 * \return the index in ssp_freq if could find it, -EINVAL otherwise.
 */
static int find_clk_ssp_index(uint32_t src_enc)
{
	int i;

	/* searching for the encode value matched bclk source */
	for (i = 0; i <= MAX_SSP_FREQ_INDEX; i++)
		if (ssp_freq_sources[i] == src_enc)
			return i;

	return -EINVAL;
}

/**
 * \brief Checks if given clock is used as source for any BCLK.
 * \param[in] clk_src Bit clock source.
 * \return true if any port use given clock source, false otherwise.
 */
static inline bool is_bclk_source_in_use(enum bclk_source clk_src)
{
	struct mn *mn = mn_get();
	bool ret = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(mn->bclk_sources); i++) {
		if (mn->bclk_sources[i] == clk_src) {
			ret = true;
			break;
		}
	}

	return ret;
}

/**
 * \brief Configures M/N source clock for BCLK.
 *	  All ports that use M/N share the same source, so it should be changed
 *	  only if there are no other ports using M/N already.
 * \param[in] bclk Bit clock frequency.
 * \param[out] scr_div SCR divisor.
 * \param[out] m M value of M/N divider.
 * \param[out] n N value of M/N divider.
 * \return 0 on success, error code otherwise.
 */
static inline int setup_initial_bclk_mn_source(uint32_t bclk, uint32_t *scr_div,
					       uint32_t *m, uint32_t *n)
{
	struct mn *mn = mn_get();
	uint32_t mdivc;
	int clk_index = find_bclk_source(bclk, scr_div, m, n);

	if (clk_index < 0) {
		tr_err(&mn_tr, "BCLK %d, no valid source", bclk);
		return -EINVAL;
	}

	mn->bclk_source_mn_clock = clk_index;

	mdivc = mn_reg_read(MN_MDIVCTRL, 0);

	/* clear source bclk clock - 21-20 bits */
	mdivc &= ~MNDSS(MN_SOURCE_CLKS_MASK);

	/* select source clock */
	mdivc |= MNDSS(ssp_freq_sources[clk_index]);

	mn_reg_write(MN_MDIVCTRL, 0, mdivc);

	return 0;
}

/**
 * \brief Reset M/N source clock for BCLK.
 *	  If no port is using bclk, reset to use SSP_CLOCK_XTAL_OSCILLATOR
 *	  as the default clock source.
 */
static inline void reset_bclk_mn_source(void)
{
	struct mn *mn = mn_get();
	uint32_t mdivc;
	int clk_index = find_clk_ssp_index(SSP_CLOCK_XTAL_OSCILLATOR);

	if (clk_index < 0) {
		tr_err(&mn_tr, "BCLK reset failed, no SSP_CLOCK_XTAL_OSCILLATOR source!");
		return;
	}

	mdivc = mn_reg_read(MN_MDIVCTRL, 0);

	/* reset to use XTAL Oscillator */
	mdivc &= ~MNDSS(MN_SOURCE_CLKS_MASK);
	mdivc |= MNDSS(SSP_CLOCK_XTAL_OSCILLATOR);

	mn_reg_write(MN_MDIVCTRL, 0, mdivc);

	mn->bclk_source_mn_clock = clk_index;

}

/**
 * \brief Finds valid M/(N * SCR) values for source clock that is already locked
 *	  because other ports use it.
 * \param[in] bclk Bit clock frequency.
 * \param[out] scr_div SCR divisor.
 * \param[out] m M value of M/N divider.
 * \param[out] n N value of M/N divider.
 * \return 0 on success, error code otherwise.
 */
static inline int setup_current_bclk_mn_source(uint32_t bclk, uint32_t *scr_div,
					       uint32_t *m, uint32_t *n)
{
	struct mn *mn = mn_get();
	int ret = 0;

	/* source for M/N is already set, no need to do it */
	if (find_mn(ssp_freq[mn->bclk_source_mn_clock].freq, bclk, scr_div, m,
		    n))
		goto out;

	tr_err(&mn_tr, "BCLK %d, no valid configuration for already selected source = %d",
	       bclk, mn->bclk_source_mn_clock);
	ret = -EINVAL;

out:

	return ret;
}

#if CAVS_VERSION >= CAVS_VERSION_2_0
static inline bool check_bclk_xtal_source(uint32_t bclk, bool mn_in_use,
					  uint32_t *scr_div)
{
	/* since cAVS 2.0 bypassing XTAL (ECS=0) is not supported */
	return false;
}
#else
/**
 * \brief Checks if XTAL source for BCLK should be used.
 *	  Before cAVS 2.0 BCLK could use XTAL directly (without M/N).
 *	  BCLK that use M/N = 1/1 or bypass XTAL is preferred.
 * \param[in] bclk Bit clock frequency.
 * \param[in] mn_in_use True if M/N source is already locked by another port.
 * \param[out] scr_div SCR divisor.
 * \return true if XTAL source should be used, false otherwise.
 */
static inline bool check_bclk_xtal_source(uint32_t bclk, bool mn_in_use,
					  uint32_t *scr_div)
{
	struct mn *mn = mn_get();
	bool ret = false;
	int i;

	for (i = 0; i <= MAX_SSP_FREQ_INDEX; i++) {
		if (ssp_freq[i].freq % bclk != 0)
			continue;

		if (ssp_freq_sources[i] == SSP_CLOCK_XTAL_OSCILLATOR) {
			/* XTAL turned out to be lowest source that can work
			 * just with SCR, so use it
			 */
			*scr_div = ssp_freq[i].freq / bclk;
			ret = true;
			break;
		}

		/* if M/N is already set up for desired clock,
		 * we can quit and let M/N logic handle it
		 */
		if (!mn_in_use || mn->bclk_source_mn_clock == i)
			break;
	}

	return ret;
}
#endif

int mn_set_bclk(uint32_t dai_index, uint32_t bclk_rate,
		uint32_t *out_scr_div, bool *out_need_ecs)
{
	struct mn *mn = mn_get();
	uint32_t m = 1;
	uint32_t n = 1;
	int ret = 0;
	bool mn_in_use;

	spin_lock(&mn->lock);

	mn->bclk_sources[dai_index] = MN_BCLK_SOURCE_NONE;

	mn_in_use = is_bclk_source_in_use(MN_BCLK_SOURCE_MN);

	if (check_bclk_xtal_source(bclk_rate, mn_in_use, out_scr_div)) {
		mn->bclk_sources[dai_index] = MN_BCLK_SOURCE_XTAL;
		*out_need_ecs = false;
		goto out;
	}

	*out_need_ecs = true;

	if (mn_in_use)
		ret = setup_current_bclk_mn_source(bclk_rate,
						   out_scr_div, &m, &n);
	else
		ret = setup_initial_bclk_mn_source(bclk_rate,
						   out_scr_div, &m, &n);

	if (ret >= 0) {
		mn->bclk_sources[dai_index] = MN_BCLK_SOURCE_MN;

		tr_info(&mn_tr, "bclk_rate %d, *out_scr_div %d, m %d, n %d",
			bclk_rate, *out_scr_div, m, n);

		mn_reg_write(MN_MDIV_M_VAL(dai_index), dai_index, m);
		mn_reg_write(MN_MDIV_N_VAL(dai_index), dai_index, n);
	}

out:

	spin_unlock(&mn->lock);

	return ret;
}

void mn_release_bclk(uint32_t dai_index)
{
	struct mn *mn = mn_get();
	bool mn_in_use;

	spin_lock(&mn->lock);
	mn->bclk_sources[dai_index] = MN_BCLK_SOURCE_NONE;

	mn_in_use = is_bclk_source_in_use(MN_BCLK_SOURCE_MN);
	/* release the M/N clock source if not used */
	if (!mn_in_use)
		reset_bclk_mn_source();
	spin_unlock(&mn->lock);
}

void mn_reset_bclk_divider(uint32_t dai_index)
{
	struct mn *mn = mn_get();

	spin_lock(&mn->lock);
	mn_reg_write(MN_MDIV_M_VAL(dai_index), dai_index, 1);
	mn_reg_write(MN_MDIV_N_VAL(dai_index), dai_index, 1);
	spin_unlock(&mn->lock);
}

#endif
