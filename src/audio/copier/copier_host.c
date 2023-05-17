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

	ret = host_zephyr_new(hd, parent_dev, &ipc_host, config->id);
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
	host_zephyr_free(hd);
e_data:
	rfree(hd);

	return ret;
}

void copier_host_free(struct copier_data *cd)
{
	host_zephyr_free(cd->hd);
	rfree(cd->hd);
}
