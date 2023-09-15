// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/component_ext.h>
#include <sof/audio/ipc-config.h>
#include <rtos/idc.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/lib/dai.h>
#include <sof/lib/notifier.h>
#include <sof/drivers/afe-dai.h>
#include <sof/drivers/edma.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* IPC3 headers */
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

void dai_set_link_hda_config(uint16_t *link_config,
			    struct ipc_config_dai *common_config,
			    const void *spec_config)
{ }

int dai_config_dma_channel(struct dai_data *dd, struct comp_dev *dev, const void *spec_config)
{
	const struct sof_ipc_dai_config *config = spec_config;
	struct ipc_config_dai *dai = &dd->ipc_config;
	int channel;
	int handshake;

	assert(config);

	switch (config->type) {
	case SOF_DAI_INTEL_SSP:
		COMPILER_FALLTHROUGH;
	case SOF_DAI_INTEL_DMIC:
		channel = 0;
		break;
	case SOF_DAI_INTEL_HDA:
		channel = config->hda.link_dma_ch;
		break;
	case SOF_DAI_INTEL_ALH:
		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		channel = config->alh.stream_id;
		break;
	case SOF_DAI_IMX_SAI:
		COMPILER_FALLTHROUGH;
	case SOF_DAI_IMX_ESAI:
		handshake = dai_get_handshake(dd->dai, dai->direction,
					      dd->stream_id);
		channel = EDMA_HS_GET_CHAN(handshake);
		break;
	case SOF_DAI_IMX_MICFIL:
		channel = dai_get_handshake(dd->dai, dai->direction,
					    dd->stream_id);
		break;
	case SOF_DAI_AMD_BT:
		channel = dai_get_handshake(dd->dai, dai->direction,
					    dd->stream_id);
		break;
	case SOF_DAI_AMD_SP:
	case SOF_DAI_AMD_SP_VIRTUAL:
		channel = dai_get_handshake(dd->dai, dai->direction,
					    dd->stream_id);
		break;
	case SOF_DAI_AMD_DMIC:
		channel = dai_get_handshake(dd->dai, dai->direction,
					    dd->stream_id);
		break;
	case SOF_DAI_AMD_HS:
	case SOF_DAI_AMD_HS_VIRTUAL:
		channel = dai_get_handshake(dd->dai, dai->direction,
					    dd->stream_id);
		break;
	case SOF_DAI_MEDIATEK_AFE:
		handshake = dai_get_handshake(dd->dai, dai->direction,
					      dd->stream_id);
		channel = AFE_HS_GET_CHAN(handshake);
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_err(dev, "dai_config_dma_channel(): Unknown dai type %d",
			 config->type);
		channel = DMA_CHAN_INVALID;
		break;
	}

	return channel;
}

int ipc_dai_data_config(struct dai_data *dd, struct comp_dev *dev)
{
	struct ipc_config_dai *dai = &dd->ipc_config;
	struct sof_ipc_dai_config *config = ipc_from_dai_config(dd->dai_spec_config);

	if (!config) {
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

	/* validate direction */
	if (dai->direction != SOF_IPC_STREAM_PLAYBACK &&
	    dai->direction != SOF_IPC_STREAM_CAPTURE) {
		comp_err(dev, "dai_data_config(): no direction set for dai %d type %d",
			 dai->dai_index, dai->type);
		return -EINVAL;
	}

	switch (config->type) {
	case SOF_DAI_INTEL_SSP:
		/* set dma burst elems to slot number */
		dd->config.burst_elems = config->ssp.tdm_slots;
		break;
	case SOF_DAI_INTEL_DMIC:
		/* Depth is passed by DMIC driver that retrieves it from blob */
		dd->config.burst_elems = dai_get_fifo_depth(dd->dai, dai->direction);
		comp_info(dev, "dai_data_config() burst_elems = %d", dd->config.burst_elems);
		break;
	case SOF_DAI_INTEL_HDA:
		break;
	case SOF_DAI_INTEL_ALH:
		/* SDW HW FIFO always requires 32bit MSB aligned sample data for
		 * all formats, such as 8/16/24/32 bits.
		 */
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S32_LE;
		if (dd->dma_buffer)
			audio_stream_set_frm_fmt(&dd->dma_buffer->stream,
						 dev->ipc_config.frame_fmt);

		dd->config.burst_elems = dai_get_fifo_depth(dd->dai, dai->direction);
		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		dd->stream_id = config->alh.stream_id;
		break;
	case SOF_DAI_IMX_MICFIL:
	case SOF_DAI_IMX_SAI:
	case SOF_DAI_IMX_ESAI:
		dd->config.burst_elems = dai_get_fifo_depth(dd->dai, dai->direction);
		break;
	case SOF_DAI_AMD_BT:
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S16_LE;
		break;
	case SOF_DAI_AMD_SP:
	case SOF_DAI_AMD_SP_VIRTUAL:
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S16_LE;
		break;
	case SOF_DAI_AMD_DMIC:
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S32_LE;
		if (dd->dma_buffer)
			audio_stream_set_frm_fmt(&dd->dma_buffer->stream,
						 dev->ipc_config.frame_fmt);
		break;
	case SOF_DAI_AMD_HS:
	case SOF_DAI_AMD_HS_VIRTUAL:
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S16_LE;
		break;
	case SOF_DAI_MEDIATEK_AFE:
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_warn(dev, "dai_data_config(): Unknown dai type %d",
			  config->type);
		break;
	}

	/* some DAIs may not need extra config */
	return 0;
}

int ipc_comp_dai_config(struct ipc *ipc, struct ipc_config_dai *common_config,
			void *spec_config)
{
	struct sof_ipc_dai_config *config = spec_config;
	bool comp_on_core[CONFIG_CORE_COUNT] = { false };
	struct sof_ipc_reply reply;
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	int ret = -ENODEV;
	int i;

	tr_info(&ipc_tr, "ipc_comp_dai_config() dai type = %d index = %d",
		config->type, config->dai_index);

	/* for each component */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		/* make sure we only config DAI comps */
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		if (!cpu_is_me(icd->core)) {
			comp_on_core[icd->core] = true;
			ret = 0;
			continue;
		}

		if (dev_comp_type(icd->cd) == SOF_COMP_DAI ||
		    dev_comp_type(icd->cd) == SOF_COMP_SG_DAI) {

			ret = comp_dai_config(icd->cd, common_config, spec_config);
			if (ret < 0)
				break;
		}
	}

	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_comp_dai_config(): comp_dai_config() failed");
		return ret;
	}

	/* message forwarded only by primary core */
	if (cpu_is_primary(cpu_get_id())) {
		for (i = 0; i < CONFIG_CORE_COUNT; ++i) {
			if (!comp_on_core[i])
				continue;

			/*
			 * TODO: can secondary cores execute dai_config() in
			 * parallel? Then we could just wait for the last of
			 * them. For now execute them sequentially.
			 */
			ret = ipc_process_on_core(i, true);
			if (ret < 0)
				break;

			/* check whether IPC failed on secondary core */
			mailbox_hostbox_read(&reply, sizeof(reply), 0,
					     sizeof(reply));
			if (reply.error < 0) {
				/* error reply already written */
				ret = 1;
				break;
			}
		}

		/* We have waited until all secondary cores configured their DAIs */
		ipc->core = PLATFORM_PRIMARY_CORE_ID;
	}

	return ret;
}

void dai_dma_release(struct dai_data *dd, struct comp_dev *dev)
{
	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config(): Component is in active state. Ignore resetting");
		return;
	}

	/* put the allocated DMA channel first */
	if (dd->chan) {
		/* remove callback */
		notifier_unregister(dev, dd->chan, NOTIFIER_ID_DMA_COPY);
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
		dma_release_channel(dd->chan->dma->z_dev, dd->chan->index);
#else
		dma_channel_put_legacy(dd->chan);
#endif
		dd->chan->dev_data = NULL;
		dd->chan = NULL;
	}
}

int dai_config(struct dai_data *dd, struct comp_dev *dev, struct ipc_config_dai *common_config,
	       const void *spec_config)
{
	const struct sof_ipc_dai_config *config = spec_config;
	int ret;

	/* ignore if message not for this DAI id/type */
	if (dd->ipc_config.dai_index != config->dai_index ||
	    dd->ipc_config.type != config->type)
		return 0;

	comp_info(dev, "dai_config() dai type = %d index = %d dd %p",
		  config->type, config->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config(): Component is in active state. Ignore config");
		return 0;
	}

	dd->dai_dev = dev;

	switch (config->flags & SOF_DAI_CONFIG_FLAGS_CMD_MASK) {
	case SOF_DAI_CONFIG_FLAGS_HW_PARAMS:
		/* set the delayed_dma_stop flag */
		if (SOF_DAI_QUIRK_IS_SET(config->flags, SOF_DAI_CONFIG_FLAGS_2_STEP_STOP))
			dd->delayed_dma_stop = true;

		if (dd->chan) {
			comp_info(dev, "dai_config(): Configured. dma channel index %d, ignore...",
				  dd->chan->index);
			return 0;
		}
		break;
	case SOF_DAI_CONFIG_FLAGS_HW_FREE:
		if (!dd->chan)
			return 0;

		/* stop DMA and reset config for two-step stop DMA */
		if (dd->delayed_dma_stop) {
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
			ret = dma_stop(dd->chan->dma->z_dev, dd->chan->index);
#else
			ret = dma_stop_delayed_legacy(dd->chan);
#endif
			if (ret < 0)
				return ret;

			dai_dma_release(dd, dev);
		}

		return 0;
	case SOF_DAI_CONFIG_FLAGS_PAUSE:
		if (!dd->chan)
			return 0;
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
		return dma_stop(dd->chan->dma->z_dev, dd->chan->index);
#else
		return dma_stop_delayed_legacy(dd->chan);
#endif
	default:
		break;
	}
#if CONFIG_COMP_DAI_GROUP
	if (config->group_id) {
		ret = dai_assign_group(dd, dev, config->group_id);

		if (ret)
			return ret;
	}
#endif
	/* do nothing for asking for channel free, for compatibility. */
	if (dai_config_dma_channel(dd, dev, spec_config) == DMA_CHAN_INVALID)
		return 0;

	/* allocated dai_config if not yet */
	if (!dd->dai_spec_config) {
		dd->dai_spec_config = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
					      sizeof(struct sof_ipc_dai_config));
		if (!dd->dai_spec_config) {
			comp_err(dev, "dai_config(): No memory for dai_config.");
			return -ENOMEM;
		}
	}

	ret = memcpy_s(dd->dai_spec_config, sizeof(struct sof_ipc_dai_config), config,
		       sizeof(struct sof_ipc_dai_config));
	if (ret < 0) {
		rfree(dd->dai_spec_config);
		dd->dai_spec_config = NULL;
	}

	return ret;
}

int dai_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/* TODO: improve accuracy by adding current DMA position */
	posn->dai_posn = dd->total_data_processed;

	/* set stream start wallclock */
	posn->wallclock = dd->wallclock;

	return 0;
}

void dai_dma_position_update(struct dai_data *dd, struct comp_dev *dev) { }

void dai_release_llp_slot(struct dai_data *dd) { }
