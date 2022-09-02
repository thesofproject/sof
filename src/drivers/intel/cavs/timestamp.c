// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/drivers/dmic.h>
#include <sof/drivers/ssp.h>
#include <sof/drivers/timestamp.h>
#include <rtos/clk.h>
#include <sof/lib/dai.h>
#include <sof/lib/io.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

#include <errno.h>
#include <stdint.h>

LOG_MODULE_REGISTER(dai_ts, CONFIG_SOF_LOG_LEVEL);

int timestamp_hda_config(struct dai *dai, struct timestamp_cfg *cfg)
{
	int i;

	if (cfg->type != SOF_DAI_INTEL_HDA) {
		dai_err(dai, "dmic_ts_config(): Illegal DAI type");
		return -EINVAL;
	}

	cfg->walclk_rate = 0;
	for (i = 0; i < NUM_SSP_FREQ; i++) {
		if (ssp_freq_sources[i] == SSP_CLOCK_XTAL_OSCILLATOR)
			cfg->walclk_rate = ssp_freq[i].freq;
	}

	return 0;
}

int timestamp_hda_start(struct dai *dai, struct timestamp_cfg *cfg)
{
	/* Set HDA timestamp registers */
	uint32_t addr = TIMESTAMP_BASE + TS_HDA_LOCAL_TSCTRL;
	uint32_t cdmas;

	/* Set HDA timestamp registers */

	/* Set CDMAS(4:0) to match DMA engine index and direction
	 * also clear NTK to be sure there is no old timestamp.
	 */
	cdmas = TS_LOCAL_TSCTRL_CDMAS(cfg->dma_chan_index |
		(cfg->direction == SOF_IPC_STREAM_PLAYBACK ? BIT(4) : 0));
	io_reg_write(addr, TS_LOCAL_TSCTRL_NTK_BIT | cdmas);

	/* Request on demand timestamp */
	io_reg_write(addr, TS_LOCAL_TSCTRL_ODTS_BIT | cdmas);

	return 0;
}

int timestamp_hda_stop(struct dai *dai, struct timestamp_cfg *cfg)
{
	/* Clear NTK and write zero to CDMAS */
	io_reg_write(TIMESTAMP_BASE + TS_HDA_LOCAL_TSCTRL,
		     TS_LOCAL_TSCTRL_NTK_BIT);
	return 0;
}

int timestamp_hda_get(struct dai *dai, struct timestamp_cfg *cfg,
		      struct timestamp_data *tsd)
{
	/* Read HDA timestamp registers */
	uint32_t tsctrl = TIMESTAMP_BASE + TS_HDA_LOCAL_TSCTRL;
	uint32_t ntk;

	ntk = io_reg_read(tsctrl) & TS_LOCAL_TSCTRL_NTK_BIT;
	if (!ntk)
		goto out;

	/* NTK was set, get wall clock */
	tsd->walclk = io_reg_read_64(TIMESTAMP_BASE + TS_HDA_LOCAL_WALCLK);

	/* Sample */
	tsd->sample = io_reg_read_64(TIMESTAMP_BASE + TS_HDA_LOCAL_SAMPLE);

	/* Clear NTK to enable successive timestamps */
	io_reg_write(tsctrl, TS_LOCAL_TSCTRL_NTK_BIT);

out:
	tsd->walclk_rate = cfg->walclk_rate;
	if (!ntk)
		return -ENODATA;

	return 0;
}

#if CONFIG_INTEL_DMIC

int timestamp_dmic_config(struct dai *dai, struct timestamp_cfg *cfg)
{
	if (cfg->type != SOF_DAI_INTEL_DMIC) {
		dai_err(dai, "dmic_ts_config(): Illegal DAI type");
		return -EINVAL;
	}

	cfg->walclk_rate = CONFIG_DMIC_HW_IOCLK;

	return 0;
}

int timestamp_dmic_start(struct dai *dai, struct timestamp_cfg *cfg)
{
	uint32_t addr = TIMESTAMP_BASE + TS_DMIC_LOCAL_TSCTRL;
	uint32_t cdmas;

	/* Set DMIC timestamp registers */

	/* First point CDMAS to GPDMA channel that is used by DMIC
	 * also clear NTK to be sure there is no old timestamp.
	 */
	cdmas = TS_LOCAL_TSCTRL_CDMAS(cfg->dma_chan_index +
		cfg->dma_chan_count * cfg->dma_id);
	io_reg_write(addr, TS_LOCAL_TSCTRL_NTK_BIT | cdmas);

	/* Request on demand timestamp */
	io_reg_write(addr, TS_LOCAL_TSCTRL_ODTS_BIT | cdmas);

	return 0;
}

int timestamp_dmic_stop(struct dai *dai, struct timestamp_cfg *cfg)
{
	/* Clear NTK and write zero to CDMAS */
	io_reg_write(TIMESTAMP_BASE + TS_DMIC_LOCAL_TSCTRL,
		     TS_LOCAL_TSCTRL_NTK_BIT);
	return 0;
}

int timestamp_dmic_get(struct dai *dai, struct timestamp_cfg *cfg,
		       struct timestamp_data *tsd)
{
	/* Read DMIC timestamp registers */
	uint32_t tsctrl = TIMESTAMP_BASE + TS_DMIC_LOCAL_TSCTRL;
	uint32_t ntk;

	/* Read SSP timestamp registers */
	ntk = io_reg_read(tsctrl) & TS_LOCAL_TSCTRL_NTK_BIT;
	if (!ntk)
		goto out;

	/* NTK was set, get wall clock */
	tsd->walclk = io_reg_read_64(TIMESTAMP_BASE + TS_DMIC_LOCAL_WALCLK);

	/* Sample */
	tsd->sample = io_reg_read_64(TIMESTAMP_BASE + TS_DMIC_LOCAL_SAMPLE);

	/* Clear NTK to enable successive timestamps */
	io_reg_write(tsctrl, TS_LOCAL_TSCTRL_NTK_BIT);

out:
	tsd->walclk_rate = cfg->walclk_rate;
	if (!ntk)
		return -ENODATA;

	return 0;
}

#endif /* CONFIG_INTEL_DMIC */

#if CONFIG_INTEL_SSP

static uint32_t ssp_ts_local_tsctrl_addr(int index)
{
#if CONFIG_APOLLOLAKE
	/* TSCTRL registers for SSP0, 1, 2, and 3 are in continuous
	 * registers space while SSP4 and more are handled with other
	 * macro.
	 */
	if (index < DAI_NUM_SSP_BASE)
		return TIMESTAMP_BASE + TS_I2S_LOCAL_TSCTRL(index);
	else
		return TIMESTAMP_BASE + TS_I2SE_LOCAL_TSCTRL(index);
#else
	return TIMESTAMP_BASE + TS_I2S_LOCAL_TSCTRL(index);
#endif
}

static uint32_t ssp_ts_local_sample_addr(int index)
{
#if CONFIG_APOLLOLAKE
	if (index < DAI_NUM_SSP_BASE)
		return TIMESTAMP_BASE + TS_I2S_LOCAL_SAMPLE(index);
	else
		return TIMESTAMP_BASE + TS_I2SE_LOCAL_SAMPLE(index);
#else
	return TIMESTAMP_BASE + TS_I2S_LOCAL_SAMPLE(index);
#endif
}

static uint32_t ssp_ts_local_walclk_addr(int index)
{
#if CONFIG_APOLLOLAKE
	if (index < DAI_NUM_SSP_BASE)
		return TIMESTAMP_BASE + TS_I2S_LOCAL_WALCLK(index);
	else
		return TIMESTAMP_BASE + TS_I2SE_LOCAL_WALCLK(index);
#else
	return TIMESTAMP_BASE + TS_I2S_LOCAL_WALCLK(index);
#endif
}

int timestamp_ssp_config(struct dai *dai, struct timestamp_cfg *cfg)
{
	int i;

	if (cfg->type != SOF_DAI_INTEL_SSP) {
		dai_err(dai, "ssp_ts_config(): Illegal DAI type");
		return -EINVAL;
	}

	if (cfg->index > DAI_NUM_SSP_BASE + DAI_NUM_SSP_EXT - 1) {
		dai_err(dai, "ssp_ts_config(): Illegal DAI index");
		return -EINVAL;
	}

	cfg->walclk_rate = 0;
	for (i = 0; i < NUM_SSP_FREQ; i++) {
		if (ssp_freq_sources[i] == SSP_CLOCK_XTAL_OSCILLATOR)
			cfg->walclk_rate = ssp_freq[i].freq;
	}

	if (!cfg->walclk_rate) {
		dai_err(dai, "ssp_ts_config(): No XTAL frequency defined");
		return -EINVAL;
	}

	return 0;
}

int timestamp_ssp_start(struct dai *dai, struct timestamp_cfg *cfg)
{
	uint32_t cdmas;
	uint32_t addr = ssp_ts_local_tsctrl_addr(cfg->index);

	/* Set SSP timestamp registers */

	/* First point CDMAS to GPDMA channel that is used by this SSP,
	 * also clear NTK to be sure there is no old timestamp.
	 */
	cdmas = TS_LOCAL_TSCTRL_CDMAS(cfg->dma_chan_index +
		cfg->dma_chan_count * cfg->dma_id);
	io_reg_write(addr, TS_LOCAL_TSCTRL_NTK_BIT | cdmas);

	/* Request on demand timestamp */
	io_reg_write(addr, TS_LOCAL_TSCTRL_ODTS_BIT | cdmas);

	return 0;
}

int timestamp_ssp_stop(struct dai *dai, struct timestamp_cfg *cfg)
{
	/* Clear NTK and write zero to CDMAS */
	io_reg_write(ssp_ts_local_tsctrl_addr(cfg->index),
		     TS_LOCAL_TSCTRL_NTK_BIT);
	return 0;
}

int timestamp_ssp_get(struct dai *dai, struct timestamp_cfg *cfg,
		      struct timestamp_data *tsd)
{
	uint32_t ntk;
	uint32_t tsctrl = ssp_ts_local_tsctrl_addr(cfg->index);

	/* Read SSP timestamp registers */
	ntk = io_reg_read(tsctrl) & TS_LOCAL_TSCTRL_NTK_BIT;
	if (!ntk)
		goto out;

	/* NTK was set, get wall clock */
	tsd->walclk = io_reg_read_64(ssp_ts_local_walclk_addr(cfg->index));

	/* Sample */
	tsd->sample = io_reg_read_64(ssp_ts_local_sample_addr(cfg->index));

	/* Clear NTK to enable successive timestamps */
	io_reg_write(tsctrl, TS_LOCAL_TSCTRL_NTK_BIT);

out:
	tsd->walclk_rate = cfg->walclk_rate;
	if (!ntk)
		return -ENODATA;

	return 0;
}

#endif /* CONFIG_INTEL_SSP */
