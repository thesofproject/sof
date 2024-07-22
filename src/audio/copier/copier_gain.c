// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.
//
// Author: Ievgen Ganakov <ievgen.ganakov@intel.com>

#include <sof/trace/trace.h>
#include <ipc4/base-config.h>
#include <sof/audio/component_ext.h>
#include <module/module/base.h>
#include "copier.h"
#include "copier_gain.h"

LOG_MODULE_DECLARE(copier, CONFIG_SOF_LOG_LEVEL);

int copier_gain_set_params(struct comp_dev *dev, struct dai_data *dd)
{
	struct processing_module *mod = comp_mod(dev);
	struct copier_data *cd = module_get_private_data(mod);
	struct ipc4_base_module_cfg *ipc4_cfg = &cd->config.base;
	uint32_t sampling_freq = ipc4_cfg->audio_fmt.sampling_frequency;
	uint32_t frames = sampling_freq / dev->pipeline->period;
	uint32_t fade_period = GAIN_DEFAULT_FADE_PERIOD;
	int ret;

	/* Set basic gain parameters */
	copier_gain_set_basic_params(dev, dd, ipc4_cfg);

	/* Set fade parameters */
	ret = copier_gain_set_fade_params(dev, dd, ipc4_cfg, fade_period, frames);
	if (ret)
		comp_err(dev, "Failed to set fade params");

	return ret;
}
