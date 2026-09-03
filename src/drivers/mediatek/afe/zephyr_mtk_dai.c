/*
 * Copyright 2026 MediaTek
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/dai.h>
#include <zephyr/logging/log.h>
#include <sof/drivers/afe-drv.h>
#include <ipc/stream.h>
#include <ipc/dai.h>
#include <ipc/dai-mediatek.h>

#define DT_DRV_COMPAT mediatek_afe

LOG_MODULE_REGISTER(dai_mtk_afe);

struct dai_mtk_afe_cfg {
	uint32_t dai_index;
	uint32_t memif_index;
	bool downlink;
};

struct dai_mtk_afe_data {
	struct dai_config cfg;
	struct dai_properties tx_props;
	struct dai_properties rx_props;
};

static int dai_mtk_afe_probe(const struct device *dev)
{
	LOG_DBG("%s: probe", dev->name);
	return 0;
}

static int dai_mtk_afe_remove(const struct device *dev)
{
	LOG_DBG("%s: remove", dev->name);
	return 0;
}

static int dai_mtk_afe_config_set(const struct device *dev,
				  const struct dai_config *cfg,
				  const void *bespoke_cfg, size_t size)
{
	const struct dai_mtk_afe_cfg *dev_cfg = dev->config;
	struct dai_mtk_afe_data *data = dev->data;
	const struct sof_ipc_dai_afe_params *afe_cfg = bespoke_cfg;
	struct mtk_base_afe *afe;
	int ret;

	if (cfg) {
		data->cfg.channels = cfg->channels;
		data->cfg.rate = cfg->rate;
		data->cfg.format = cfg->format;
		data->cfg.word_size = cfg->word_size;
	}

	if (afe_cfg) {
		int probe_ret;

		afe = afe_get();
		probe_ret = afe_probe(afe);
		if (probe_ret < 0) {
			LOG_ERR("%s: afe_probe failed: %d", dev->name, probe_ret);
			return probe_ret;
		}
		ret = afe_dai_set_config(afe, dev_cfg->dai_index,
					 afe_cfg->dai_channels,
					 afe_cfg->dai_rate,
					 afe_cfg->dai_format);
		if (ret < 0) {
			LOG_ERR("%s: afe_dai_set_config failed: %d",
				dev->name, ret);
			return ret;
		}

		LOG_DBG("%s: config_set dai=%u ch=%u rate=%u fmt=%u",
			dev->name, dev_cfg->dai_index,
			afe_cfg->dai_channels,
			afe_cfg->dai_rate,
			afe_cfg->dai_format);
	}

	return 0;
}

static int dai_mtk_afe_config_get(const struct device *dev,
				  struct dai_config *cfg, enum dai_dir dir)
{
	const struct dai_mtk_afe_cfg *dev_cfg = dev->config;
	struct dai_mtk_afe_data *data = dev->data;

	if (!cfg) {
		return -EINVAL;
	}

	*cfg = data->cfg;
	cfg->type = DAI_MEDIATEK_AFE;
	cfg->dai_index = dev_cfg->dai_index;

	return 0;
}

static const struct dai_properties *dai_mtk_afe_get_properties(
	const struct device *dev, enum dai_dir dir, int stream_id)
{
	const struct dai_mtk_afe_data *data = dev->data;

	if (dir == DAI_DIR_TX) {
		return &data->tx_props;
	}
	return &data->rx_props;
}

static int dai_mtk_afe_trigger(const struct device *dev,
			       enum dai_dir dir, enum dai_trigger_cmd cmd)
{
	LOG_DBG("%s: trigger dir=%d cmd=%d", dev->name, dir, cmd);
	return 0;
}

static DEVICE_API(dai, dai_mtk_afe_api) = {
	.probe = dai_mtk_afe_probe,
	.remove = dai_mtk_afe_remove,
	.config_set = dai_mtk_afe_config_set,
	.config_get = dai_mtk_afe_config_get,
	.get_properties = dai_mtk_afe_get_properties,
	.trigger = dai_mtk_afe_trigger,
};

#define DAI_MTK_AFE_HANDSHAKE(inst) \
	((inst) << 16) | DT_INST_PROP(inst, dai_id)

#define DAI_MTK_AFE_INIT(inst)						\
	static struct dai_mtk_afe_data dai_mtk_afe_data_##inst = {	\
		.cfg = {						\
			.type = DAI_MEDIATEK_AFE,			\
			.dai_index = DT_INST_PROP(inst, dai_id),	\
		},							\
		.tx_props = {						\
			.dma_hs_id = DAI_MTK_AFE_HANDSHAKE(inst),	\
		},							\
		.rx_props = {						\
			.dma_hs_id = DAI_MTK_AFE_HANDSHAKE(inst),	\
		},							\
	};								\
									\
	static const struct dai_mtk_afe_cfg dai_mtk_afe_cfg_##inst = {	\
		.dai_index = DT_INST_PROP(inst, dai_id),		\
		.memif_index = inst,					\
		.downlink = DT_INST_PROP(inst, downlink),		\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL,				\
			      &dai_mtk_afe_data_##inst,			\
			      &dai_mtk_afe_cfg_##inst,			\
			      POST_KERNEL, CONFIG_DAI_INIT_PRIORITY,	\
			      &dai_mtk_afe_api);

DT_INST_FOREACH_STATUS_OKAY(DAI_MTK_AFE_INIT)
