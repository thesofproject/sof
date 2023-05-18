// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

#include <sof/audio/component_ext.h>
#include <sof/audio/host_copier.h>
#include <sof/trace/trace.h>
#include <ipc4/copier.h>

LOG_MODULE_DECLARE(copier, CONFIG_SOF_LOG_LEVEL);

/* if copier is linked to host gateway, it will manage host dma.
 * Sof host component can support this case so copier reuses host
 * component to support host gateway.
 */
int copier_host_create(struct comp_dev *parent_dev, struct copier_data *cd,
		       struct comp_ipc_config *config,
		       const struct ipc4_copier_module_cfg *copier_cfg,
		       int dir)
{
	struct ipc_config_host ipc_host;
	struct host_data *hd;
	int ret;
	enum sof_ipc_frame in_frame_fmt, out_frame_fmt;
	enum sof_ipc_frame in_valid_fmt, out_valid_fmt;

	config->type = SOF_COMP_HOST;

	audio_stream_fmt_conversion(copier_cfg->base.audio_fmt.depth,
				    copier_cfg->base.audio_fmt.valid_bit_depth,
				    &in_frame_fmt, &in_valid_fmt,
				    copier_cfg->base.audio_fmt.s_type);

	audio_stream_fmt_conversion(copier_cfg->out_fmt.depth,
				    copier_cfg->out_fmt.valid_bit_depth,
				    &out_frame_fmt, &out_valid_fmt,
				    copier_cfg->out_fmt.s_type);

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK)
		config->frame_fmt = in_frame_fmt;
	else
		config->frame_fmt = out_frame_fmt;

	parent_dev->ipc_config.frame_fmt = config->frame_fmt;

	memset(&ipc_host, 0, sizeof(ipc_host));
	ipc_host.direction = dir;
	ipc_host.dma_buffer_size = copier_cfg->gtw_cfg.dma_buffer_size;
	ipc_host.feature_mask = copier_cfg->copier_feature_mask;

	hd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*hd));
	if (!hd)
		return -ENOMEM;

	ret = host_common_new(hd, parent_dev, &ipc_host, config->id);
	if (ret < 0) {
		comp_err(parent_dev, "copier: host new failed with exit");
		goto e_data;
	}

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
		get_converter_func(&copier_cfg->base.audio_fmt,
				   &copier_cfg->out_fmt,
				   ipc4_gtw_host, IPC4_DIRECTION(dir));
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(parent_dev, "failed to get converter for host, dir %d", dir);
		ret = -EINVAL;
		goto e_conv;
	}

	cd->endpoint_num++;
	cd->hd = hd;

	return 0;

e_conv:
	host_common_free(hd);
e_data:
	rfree(hd);

	return ret;
}

void copier_host_free(struct copier_data *cd)
{
	host_common_free(cd->hd);
	rfree(cd->hd);
}

/* This is called by DMA driver every time when DMA completes its current
 * transfer between host and DSP.
 */
void copier_host_dma_cb(struct comp_dev *dev, size_t bytes)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *sink;
	int ret, frames;

	comp_dbg(dev, "copier_host_dma_cb() %p", dev);

	/* update position */
	host_common_update(cd->hd, dev, bytes);

	/* callback for one shot copy */
	if (cd->hd->copy_type == COMP_COPY_ONE_SHOT)
		host_common_one_shot(cd->hd, bytes);

	/* apply attenuation since copier copy missed this with host device remove */
	if (cd->attenuation) {
		if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
			sink = buffer_acquire(cd->hd->local_buffer);
		else
			sink = buffer_acquire(cd->hd->dma_buffer);

		frames = bytes / get_sample_bytes(audio_stream_get_frm_fmt(&sink->stream));
		frames = frames / audio_stream_get_channels(&sink->stream);

		ret = apply_attenuation(dev, cd, sink, frames);
		if (ret < 0)
			comp_dbg(dev, "copier_host_dma_cb() apply attenuation failed! %d", ret);

		buffer_stream_writeback(sink, bytes);
		buffer_release(sink);
	}
}

static void copier_notifier_cb(void *arg, enum notify_id type, void *data)
{
	struct dma_cb_data *next = data;
	uint32_t bytes = next->elem.size;

	copier_host_dma_cb(arg, bytes);
}

int copier_host_params(struct copier_data *cd, struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	int ret;

	component_set_nearest_period_frames(dev, params->rate);
	ret = host_common_params(cd->hd, dev, params,
				 copier_notifier_cb);

	cd->hd->process = cd->converter[IPC4_COPIER_GATEWAY_PIN];

	return ret;
}
