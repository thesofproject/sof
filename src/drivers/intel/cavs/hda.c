// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <sof/drivers/hda.h>
#include <sof/drivers/ssp.h>
#include <sof/drivers/timestamp.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/uuid.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

/* bc9ebe20-4577-41bb-9eed-d0cb236328da */
DECLARE_SOF_UUID("hda-dai", hda_uuid, 0xbc9ebe20, 0x4577, 0x41bb,
		 0x9e, 0xed, 0xd0, 0xcb, 0x23, 0x63, 0x28, 0xda);

static int hda_trigger(struct dai *dai, int cmd, int direction)
{
	return 0;
}

static int hda_set_config(struct dai *dai,
			  struct sof_ipc_dai_config *config)
{
	return 0;
}

/* get HDA hw params */
static int hda_get_hw_params(struct dai *dai,
			     struct sof_ipc_stream_params  *params, int dir)
{
	/* 0 means variable */
	params->rate = 0;
	params->channels = 0;
	params->buffer_fmt = 0;
	params->frame_fmt = 0;

	return 0;
}

static int hda_dummy(struct dai *dai)
{
	return 0;
}

static int hda_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return 0;
}

static int hda_get_fifo(struct dai *dai, int direction, int stream_id)
{
	return 0;
}

/* Functions for HW timestamp */

static inline uint32_t hda_ts_local_tsctrl_addr(void)
{
	return TIMESTAMP_BASE + TS_HDA_LOCAL_TSCTRL;
}

static inline uint32_t hda_ts_local_offs_addr(void)
{
	return TIMESTAMP_BASE + TS_HDA_LOCAL_OFFS;
}

static inline uint32_t hda_ts_local_sample_addr(void)
{
	return TIMESTAMP_BASE + TS_HDA_LOCAL_SAMPLE;
}

static inline uint32_t hda_ts_local_walclk_addr(void)
{
	return TIMESTAMP_BASE + TS_HDA_LOCAL_WALCLK;
}

static inline uint32_t hda_ts_tscc_addr(void)
{
	return TIMESTAMP_BASE + TS_HDA_TSCC;
}

static int hda_ts_config(struct dai *dai, struct timestamp_cfg *cfg)
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

static int hda_ts_start(struct dai *dai, struct timestamp_cfg *cfg)
{
	/* Set HDA timestamp registers */
	uint32_t cdmas;
	uint32_t addr = hda_ts_local_tsctrl_addr();

	/* Set DMIC timestamp registers */
	spin_lock(&dai->lock);

	/* Set CDMAS(4:0) to match DMA engine index and direction
	 * also clear NTK to be sure there is no old timestamp.
	 */
	cdmas = TS_LOCAL_TSCTRL_CDMAS(cfg->dma_chan_index |
		(cfg->direction == SOF_IPC_STREAM_PLAYBACK ? BIT(4) : 0));
	io_reg_write(addr, TS_LOCAL_TSCTRL_NTK_BIT | cdmas);

	/* Request on demand timestamp */
	io_reg_write(addr, TS_LOCAL_TSCTRL_ODTS_BIT | cdmas);

	spin_unlock(&dai->lock);
	return 0;
}

static int hda_ts_stop(struct dai *dai, struct timestamp_cfg *cfg)
{
	/* Clear NTK and write zero to CDMAS */
	io_reg_write(hda_ts_local_tsctrl_addr(), TS_LOCAL_TSCTRL_NTK_BIT);
	return 0;
}

static int hda_ts_get(struct dai *dai, struct timestamp_cfg *cfg,
		      struct timestamp_data *tsd)
{
	/* Read HDA timestamp registers */
	uint32_t ntk;
	uint32_t tsctrl = hda_ts_local_tsctrl_addr();

	/* Read SSP timestamp registers */
	spin_lock(&dai->lock);
	ntk = io_reg_read(tsctrl) & TS_LOCAL_TSCTRL_NTK_BIT;
	if (!ntk)
		goto out;

	/* NTK was set, get wall clock */
	tsd->walclk = io_reg_read_64(hda_ts_local_walclk_addr());

	/* Sample */
	tsd->sample = io_reg_read_64(hda_ts_local_sample_addr());

	/* Clear NTK to enable successive timestamps */
	io_reg_write(tsctrl, TS_LOCAL_TSCTRL_NTK_BIT);

out:
	spin_unlock(&dai->lock);
	tsd->walclk_rate = cfg->walclk_rate;
	if (!ntk)
		return -ENODATA;

	return 0;
}

const struct dai_driver hda_driver = {
	.type = SOF_DAI_INTEL_HDA,
	.uid = SOF_UUID(hda_uuid),
	.dma_caps = DMA_CAP_HDA,
	.dma_dev = DMA_DEV_HDA,
	.ops = {
		.trigger		= hda_trigger,
		.set_config		= hda_set_config,
		.pm_context_store	= hda_dummy,
		.pm_context_restore	= hda_dummy,
		.get_hw_params		= hda_get_hw_params,
		.get_handshake		= hda_get_handshake,
		.get_fifo		= hda_get_fifo,
		.probe			= hda_dummy,
		.remove			= hda_dummy,
	},
	.ts_ops = {
		.ts_config		= hda_ts_config,
		.ts_start		= hda_ts_start,
		.ts_get			= hda_ts_get,
		.ts_stop		= hda_ts_stop,
	},
};
