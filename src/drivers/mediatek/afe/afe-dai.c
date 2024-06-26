// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 MediaTek. All rights reserved.
 *
 * Author: Bo Pan <bo.pan@mediatek.com>
 *	   YC Hung <yc.hung@mediatek.com>
 *         Chunxu Li <chunxu.li@mediatek.com>
 *	   Trevor Wu <trevor.wu@mediatek.com>
 */

#include <sof/audio/component.h>
#include <sof/drivers/afe-drv.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <ipc/dai.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

SOF_DEFINE_REG_UUID(afe_dai);

DECLARE_TR_CTX(afe_dai_tr, SOF_UUID(afe_dai_uuid), LOG_LEVEL_INFO);

static int afe_dai_drv_trigger(struct dai *dai, int cmd, int direction)
{
	return 0;
}

static int afe_dai_drv_set_config(struct dai *dai, struct ipc_config_dai *common_config,
				  const void *spec_config)
{
	const struct sof_ipc_dai_config *config = spec_config;
	struct mtk_base_afe *afe = dai_get_drvdata(dai);

	return afe_dai_set_config(afe,
				  dai->index,
				  config->afe.dai_channels,
				  config->afe.dai_rate,
				  config->afe.dai_format);
}

/* get AFE hw params */
static int afe_dai_drv_get_hw_params(struct dai *dai, struct sof_ipc_stream_params *params, int dir)
{
	int ret;
	struct mtk_base_afe *afe = dai_get_drvdata(dai);
	unsigned int channel, rate, format;

	ret = afe_dai_get_config(afe, dai->index, &channel, &rate, &format);
	if (ret < 0)
		return ret;

	params->rate = rate;
	params->channels = channel;
	params->buffer_fmt = format;
	params->frame_fmt = format;

	return 0;
}

static int afe_dai_drv_probe(struct dai *dai)
{
	struct mtk_base_afe *afe = afe_get();

	dai_info(dai, "afe_dai_probe()");

	if (dai_get_drvdata(dai))
		return -EEXIST;

	dai_set_drvdata(dai, afe);

	return 0;
}

static int afe_dai_drv_remove(struct dai *dai)
{
	dai_info(dai, "afe_dai_remove()");

	return 0;
}

static int afe_dai_drv_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return (int)dai->plat_data.fifo[0].handshake;
}

static int afe_dai_drv_get_fifo(struct dai *dai, int direction, int stream_id)
{
	return 0;
}

const struct dai_driver afe_dai_driver = {
	.type = SOF_DAI_MEDIATEK_AFE,
	.uid = SOF_UUID(afe_dai_uuid),
	.tctx = &afe_dai_tr,
	.dma_dev = DMA_DEV_AFE_MEMIF,
	.ops = {
		.trigger		= afe_dai_drv_trigger,
		.set_config		= afe_dai_drv_set_config,
		.get_hw_params		= afe_dai_drv_get_hw_params,
		.get_handshake		= afe_dai_drv_get_handshake,
		.get_fifo		= afe_dai_drv_get_fifo,
		.probe			= afe_dai_drv_probe,
		.remove			= afe_dai_drv_remove,
	},
};
