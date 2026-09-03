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

#define DAI_INDEX_INVALID	0xFFFF

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
/* TODO: remove this when transition to native drivers is complete on all NXP platforms */
#ifndef CONFIG_ZEPHYR_NATIVE_DRIVERS
		channel = EDMA_HS_GET_CHAN(handshake);
#else
		channel = handshake & GENMASK(7, 0);
#endif /* CONFIG_ZEPHYR_NATIVE_DRIVERS */
		break;
	case SOF_DAI_IMX_MICFIL:
		channel = dai_get_handshake(dd->dai, dai->direction,
					    dd->stream_id);
/* TODO: remove ifdef when transitioning to native drivers is complete on all NXP platforms */
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
		channel = channel & GENMASK(7, 0);
#endif
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
	case SOF_DAI_AMD_TDM:
	case SOF_DAI_AMD_SDW: {
		struct dai_config *params = (struct dai_config *)dd->dai->dev->config;

		params->dai_index = dd->dai->index;
		channel = dai_get_handshake(dd->dai, dai->direction,
					    dd->stream_id);
#if defined(CONFIG_SOC_ACP_7_0)
		if (channel >= 64 && channel < 128) {
		channel = channel - 64;
		}
#endif
		break;
	}
	case SOF_DAI_MEDIATEK_AFE:
		handshake = dai_get_handshake(dd->dai, dai->direction,
					      dd->stream_id);
		channel = AFE_HS_GET_CHAN(handshake);
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_err(dev, "Unknown dai type %d",
			 config->type);
		channel = SOF_DMA_CHAN_INVALID;
		break;
	}

	return channel;
}

int ipc_dai_data_config(struct dai_data *dd, struct comp_dev *dev)
{
	struct ipc_config_dai *dai = &dd->ipc_config;
	struct sof_ipc_dai_config *config = ipc_from_dai_config(dd->dai_spec_config);

	if (!config) {
		comp_err(dev, "no config set for dai %d type %d",
			 dai->dai_index, dai->type);
		return -EINVAL;
	}

	comp_info(dev, "dai_data_config() dai type = %d index = %d dd %p",
		  dai->type, dai->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "Component is in active state.");
		return 0;
	}

	/* validate direction */
	if (dai->direction != SOF_IPC_STREAM_PLAYBACK &&
	    dai->direction != SOF_IPC_STREAM_CAPTURE) {
		comp_err(dev, "no direction set for dai %d type %d",
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
	case SOF_DAI_AMD_DMIC:
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S32_LE;
		if (dd->dma_buffer)
			audio_stream_set_frm_fmt(&dd->dma_buffer->stream,
						 dev->ipc_config.frame_fmt);
		break;
	case SOF_DAI_AMD_BT:
	case SOF_DAI_AMD_SP:
	case SOF_DAI_AMD_SP_VIRTUAL:
	case SOF_DAI_AMD_HS:
	case SOF_DAI_AMD_HS_VIRTUAL:
	case SOF_DAI_AMD_TDM:
#if defined(CONFIG_AMD) && !defined(CONFIG_SOC_ACP_6_0)
	{
		struct acp_dma_dev_data *tdm_data = dd->dma->z_dev->data;
		struct tdm_context *tdm_ctx;

		/* Allocate coherent memory for TDM context shared between
		 * IPC and DMA contexts.
		 */
		if (!tdm_data->dai_index_ptr) {
			tdm_ctx = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
					  sizeof(*tdm_ctx));
			if (!tdm_ctx)
				return -ENOMEM;
			tdm_data->dai_index_ptr = tdm_ctx;
		} else {
			tdm_ctx = (struct tdm_context *)tdm_data->dai_index_ptr;
		}
		tdm_ctx->index = dd->dai->index;
		tdm_ctx->frame_fmt = dev->ipc_config.frame_fmt;
		/* AMD HW needs 24-bit data MSB-aligned in 32-bit word */
		if (dev->ipc_config.frame_fmt == SOF_IPC_FRAME_S24_4LE) {
			dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S24_4LE_MSB;
			if (dd->dma_buffer) {
				audio_stream_set_frm_fmt(&dd->dma_buffer->stream,
							 dev->ipc_config.frame_fmt);
			}
		}
	}
#endif
		break;
	case SOF_DAI_AMD_SDW:
#if defined(CONFIG_AMD) && !defined(CONFIG_SOC_ACP_6_0)
	{
		/* AMD SDW/HS HW needs 24-bit data MSB-aligned in 32-bit word */
		if (dev->ipc_config.frame_fmt == SOF_IPC_FRAME_S24_4LE)
			dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S24_4LE_MSB;
		if (dd->dma_buffer)
			audio_stream_set_frm_fmt(&dd->dma_buffer->stream,
						 dev->ipc_config.frame_fmt);

		struct acp_dma_dev_data *dev_data = dd->dma->z_dev->data;
		struct sdw_pin_data *pin_data;

		/* Allocate memory only if not already allocated */
		if (!dev_data->dai_index_ptr) {
			pin_data = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
					   sizeof(*pin_data));
			dev_data->dai_index_ptr = pin_data;
		} else {
			pin_data = dev_data->dai_index_ptr;
		}
		pin_data->pin_num = dd->dai->index;
		pin_data->pin_dir = dai->direction;
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
		pin_data->dma_channel = dd->chan_index >= 0 ? dd->chan_index : DAI_INDEX_INVALID;
#else
		pin_data->dma_channel = dd->chan ? dd->chan->index : DAI_INDEX_INVALID;
#endif
#if defined(CONFIG_SOC_ACP_7_X)
		for (int i = 0; i < SDW_INSTANCES; i++) {
			pin_data->index[i] = DAI_INDEX_INVALID;
		}
#else
			pin_data->index = DAI_INDEX_INVALID;
#endif
		pin_data->instance = DAI_INDEX_INVALID;
		dev_data->dai_index_ptr = pin_data;
	}
#endif
		break;
	case SOF_DAI_MEDIATEK_AFE:
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_warn(dev, "Unknown dai type %d",
			  config->type);
		break;
	}

	/* some DAIs may not need extra config */
	return 0;
}

int ipc_comp_dai_config(struct ipc *ipc, struct ipc_config_dai *common_config,
			void *spec_config)
{
	struct sof_ipc_dai_config __maybe_unused *config = spec_config;
	bool comp_on_core[CONFIG_CORE_COUNT] = { false };
	struct sof_ipc_reply reply;
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	int ret = -ENODEV;
	int i;

	tr_info(&ipc_tr, "dai type = %d index = %d",
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
		tr_err(&ipc_tr, "comp_dai_config() failed");
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
		comp_info(dev, "Component is in active state. Ignore resetting");
		return;
	}

#if defined(CONFIG_AMD)
	/* Free DAI-specific data allocated in ipc_dai_data_config() */
	if (dd->dma && dd->dma->z_dev && dd->dma->z_dev->data) {
		struct acp_dma_dev_data *dev_data = dd->dma->z_dev->data;

		if (dev_data->dai_index_ptr) {
			rfree(dev_data->dai_index_ptr);
			dev_data->dai_index_ptr = NULL;
		}
	}
#endif

	/* put the allocated DMA channel first */
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
	if (dd->chan_index >= 0) {
		/* remove callback */
		notifier_unregister(dev, &dd->dma->chan[dd->chan_index],
				    NOTIFIER_ID_DMA_COPY);
		dma_release_channel(dd->dma->z_dev, dd->chan_index);
		dd->chan_index = -EINVAL;
	}
#else
	if (dd->chan) {
		/* remove callback */
		notifier_unregister(dev, dd->chan, NOTIFIER_ID_DMA_COPY);
		dma_channel_put_legacy(dd->chan);
		dd->chan->dev_data = NULL;
		dd->chan = NULL;
	}
#endif
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

	comp_info(dev, "dai type = %d index = %d dd %p",
		  config->type, config->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "Component is in active state. Ignore config");
		return 0;
	}

	dd->dai_dev = dev;

	switch (config->flags & SOF_DAI_CONFIG_FLAGS_CMD_MASK) {
	case SOF_DAI_CONFIG_FLAGS_HW_PARAMS:
		/* set the delayed_dma_stop flag */
		if (SOF_DAI_QUIRK_IS_SET(config->flags, SOF_DAI_CONFIG_FLAGS_2_STEP_STOP))
			dd->delayed_dma_stop = true;

#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
		if (dd->chan_index >= 0) {
			comp_info(dev, "Configured. dma channel index %d, ignore...",
				  dd->chan_index);
			return 0;
		}
#else
		if (dd->chan) {
			comp_info(dev, "Configured. dma channel index %d, ignore...",
				  dd->chan->index);
			return 0;
		}
#endif
		break;
	case SOF_DAI_CONFIG_FLAGS_HW_FREE:
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
		if (dd->chan_index < 0)
			return 0;
#else
		if (!dd->chan)
			return 0;
#endif

		/* stop DMA and reset config for two-step stop DMA */
		if (dd->delayed_dma_stop) {
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
			ret = dma_stop(dd->dma->z_dev, dd->chan_index);
#else
			ret = dma_stop_delayed_legacy(dd->chan);
#endif
			if (ret < 0)
				return ret;

			dai_dma_release(dd, dev);
		}

		return 0;
	case SOF_DAI_CONFIG_FLAGS_PAUSE:
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
		if (dd->chan_index < 0)
			return 0;
		return dma_stop(dd->dma->z_dev, dd->chan_index);
#else
		if (!dd->chan)
			return 0;
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
	if (dai_config_dma_channel(dd, dev, spec_config) == SOF_DMA_CHAN_INVALID)
		return 0;

	/* allocated dai_config if not yet */
	if (!dd->dai_spec_config) {
		dd->dai_spec_config = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
					      sizeof(struct sof_ipc_dai_config));
		if (!dd->dai_spec_config) {
			comp_err(dev, "No memory");
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
