// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2023 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Maruthi Machani <maruthi.machani@amd.com>

#include <sof/drivers/acp_dai_dma.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>

SOF_DEFINE_REG_UUID(swaudiodai);

DECLARE_TR_CTX(swaudiodai_tr, SOF_UUID(swaudiodai_uuid), LOG_LEVEL_INFO);

static inline int swaudiodai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
					const void *spec_config)
{
	const struct sof_ipc_dai_config *config = spec_config;
	struct acp_pdata *acpdata = dai_get_drvdata(dai);

	acpdata->config = *config;
	acpdata->sdw_params = config->acpsdw;

	return 0;
}

static int swaudiodai_trigger(struct dai *dai, int cmd, int direction)
{
	/* nothing to do on rembrandt for SW dai */
	return 0;
}

static int swaudiodai_probe(struct dai *dai)
{
	struct acp_pdata *acp;

	dai_info(dai, "#$AMD$# SW dai probe");
	/* allocate private data */
	acp = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*acp));
	if (!acp) {
		dai_err(dai, "SW dai probe alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, acp);

	return 0;
}

static int swaudiodai_remove(struct dai *dai)
{
	struct acp_pdata *acp = dai_get_drvdata(dai);

	dai_info(dai, "swaudiodai_remove");
	rfree(acp);
	dai_set_drvdata(dai, NULL);

	return 0;
}

static int swaudiodai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	switch (direction) {
	case DAI_DIR_PLAYBACK:
	case DAI_DIR_CAPTURE:
		return dai_fifo(dai, direction);
	default:
		dai_err(dai, "Invalid direction");
		return -EINVAL;
	}
}

static int swaudiodai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	int handshake = dai->plat_data.fifo[direction].handshake;

	interrupt_get_irq(handshake, "irqsteer1");

	return dai->plat_data.fifo[direction].handshake;
}

static int swaudiodai_get_hw_params(struct dai *dai,
				    struct sof_ipc_stream_params *params,
				    int dir)
{
	struct acp_pdata *acpdata = dai_get_drvdata(dai);

	/* DAI currently supports only these parameters */
	params->rate = acpdata->sdw_params.rate;
	params->channels = acpdata->sdw_params.channels;
	params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params->frame_fmt = SOF_IPC_FRAME_S16_LE;

	return 0;
}

const struct dai_driver acp_swaudiodai_driver = {
	.type = SOF_DAI_AMD_SW_AUDIO,
	.uid = SOF_UUID(swaudiodai_uuid),
	.tctx = &swaudiodai_tr,
	.dma_dev = DMA_DEV_SW,
	.dma_caps = DMA_CAP_SW,
	.ops = {
		.trigger		= swaudiodai_trigger,
		.set_config		= swaudiodai_set_config,
		.probe			= swaudiodai_probe,
		.remove			= swaudiodai_remove,
		.get_fifo		= swaudiodai_get_fifo,
		.get_handshake		= swaudiodai_get_handshake,
		.get_hw_params          = swaudiodai_get_hw_params,
	},
};

