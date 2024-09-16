// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/stream.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/audio_stream.h>
#include <sof/list.h>
#include <sof/trace/trace.h>
#include <errno.h>

#include "eq_fir.h"

LOG_MODULE_DECLARE(eq_fir, CONFIG_SOF_LOG_LEVEL);

int set_fir_func(struct processing_module *mod, enum sof_ipc_frame fmt)
{
	struct comp_data *cd = module_get_private_data(mod);
	unsigned int valid_bit_depth = mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth;

	comp_dbg(mod->dev, "set_fir_func(): valid_bit_depth %d", valid_bit_depth);
	switch (valid_bit_depth) {
#if CONFIG_FORMAT_S16LE
	case IPC4_DEPTH_16BIT:
		set_s16_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case IPC4_DEPTH_24BIT:
		set_s24_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case IPC4_DEPTH_32BIT:
		set_s32_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(mod->dev, "set_fir_func(), invalid valid_bith_depth");
		return -EINVAL;
	}
	return 0;
}

int eq_fir_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "eq_fir_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);

	sourceb = comp_dev_get_first_data_producer(dev);
	ipc4_update_buffer_format(sourceb, &mod->priv.cfg.base_cfg.audio_fmt);

	sinkb = comp_dev_get_first_data_consumer(dev);
	ipc4_update_buffer_format(sinkb, &mod->priv.cfg.base_cfg.audio_fmt);

	return 0;
}

