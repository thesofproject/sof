// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc4/gateway.h>
#include <ipc4/header.h>
#include <ipc4/alh.h>
#include <ipc4/ssp.h>
#include <ipc4/copier.h>
#include <ipc/dai.h>

#include <sof/ipc/topology.h>
#include <sof/drivers/edma.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int dai_config_dma_channel(struct comp_dev *dev, void *spec_config)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	//struct CopierGatewayCfg *gtw_cfg = ipc_from_dai_config(spec_config);
	struct ipc_config_dai *dai = &dd->ipc_config;
	int channel;

	switch (dai->type) {
	case SOF_DAI_INTEL_SSP:
		COMPILER_FALLTHROUGH;
	case SOF_DAI_INTEL_DMIC:
		channel = 0;
		break;
	case SOF_DAI_INTEL_HDA:
		channel = dai->link_dma_ch;
		break;
	case SOF_DAI_INTEL_ALH:
		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		channel = 0;
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_err(dev, "dai_config_dma_channel(): Unknown dai type %d", dai->type);
		channel = DMA_CHAN_INVALID;
		break;
	}

	return channel;
}

int ipc_dai_data_config(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct ipc_config_dai *dai = &dd->ipc_config;

	if (!dai) {
		comp_err(dev, "dai_data_config(): no config set for dai %d type %d",
			 dai->dai_index, dai->type);
		return -EINVAL;
	}

	comp_info(dev, "dai_data_config() dai type = %d index = %d dd %p",
		  dai->type, dai->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_data_config(): Component is in active state.");
		return 0;
	}

	switch (dai->type) {
	case SOF_DAI_INTEL_SSP:
		/* set dma burst elems to slot number */
		dd->config.burst_elems = dai->channels;
		break;
	case SOF_DAI_INTEL_DMIC:
		/* We can use always the largest burst length. */
		dd->config.burst_elems = 8;
		break;
	case SOF_DAI_INTEL_HDA:
		break;
	case SOF_DAI_INTEL_ALH:
	default:
		/* other types of DAIs not handled for now */
		comp_warn(dev, "dai_data_config(): Unknown dai type %d", dai->type);
		return -EINVAL;
		break;
	}

	/* some DAIs may not need extra config */
	return 0;
}

/* dai config is not sent by ipc message */
int ipc_comp_dai_config(struct ipc *ipc, struct ipc_config_dai *common_config,
			void *spec_config)
{
	return 0;
}

int dai_config(struct comp_dev *dev, struct ipc_config_dai *common_config,
	       void *spec_config)
{
	struct ipc4_copier_gateway_cfg *gtw_cfg = spec_config;
	struct dai_data *dd = comp_get_drvdata(dev);
	int size;
	int ret = 0;

	/* ignore if message not for this DAI id/type */
	if (dd->ipc_config.dai_index != common_config->dai_index ||
	    dd->ipc_config.type != common_config->type)
		return 0;

	comp_info(dev, "dai_config() dai type = %d index = %d dd %p",
			common_config->type, common_config->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config(): Component is in active state. Ignore config");
		return 0;
	}

	if (dd->chan) {
		comp_info(dev, "dai_config(): Configured. dma channel index %d, ignore...",
			  dd->chan->index);
		return 0;
	}

	if (common_config->group_id) {
		ret = dai_assign_group(dev, common_config->group_id);

		if (ret)
			return ret;
	}

	/* do nothing for asking for channel free, for compatibility. */
	if (dai_config_dma_channel(dev, spec_config) == DMA_CHAN_INVALID)
		return 0;

	/* allocated dai_config if not yet */
	if (!dd->dai_spec_config) {
		size = sizeof(*gtw_cfg);
		dd->dai_spec_config = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);
		if (!dd->dai_spec_config) {
			comp_err(dev, "dai_config(): No memory for dai_config size %d", size);
			return -ENOMEM;
		}

		ret = memcpy_s(dd->dai_spec_config, size, gtw_cfg, size);
		if (ret < 0) {
			rfree(dd->dai_spec_config);
			dd->dai_spec_config = NULL;
		}
	}

	return dai_set_config(dd->dai, common_config, gtw_cfg->config_data);
}
