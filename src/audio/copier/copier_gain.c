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

int copier_gain_input(struct comp_dev *dev, struct comp_buffer *buff,
		      struct copier_gain_params *gain_params,
		      enum copier_gain_envelope_dir dir, uint32_t stream_bytes)
{
	enum sof_ipc_frame frame_fmt = audio_stream_get_frm_fmt(&buff->stream);
	uint32_t frames = stream_bytes / audio_stream_frame_bytes(&buff->stream);
	enum copier_gain_state state;

	if (!gain_params)
		return -EINVAL;

	state = copier_gain_eval_state(gain_params);

	comp_dbg(dev, "copier selected gain state %d", state);

	switch (frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		return copier_gain_input16(buff, state, dir, gain_params, frames);
	case SOF_IPC_FRAME_S32_LE:
		return copier_gain_input32(buff, state, dir, gain_params, frames);
	default:
		comp_err(dev, "unsupported frame format %d for copier gain", frame_fmt);
		return -EINVAL;
	}
}

enum copier_gain_state copier_gain_eval_state(struct copier_gain_params *gain_params)
{
	enum copier_gain_state state = STATIC_GAIN;

	if (gain_params->silence_sg_count < gain_params->silence_sg_length)
		state = MUTE;
	else if ((gain_params->fade_in_sg_count < gain_params->fade_sg_length) &&
		 (gain_params->fade_sg_length != 0))
		state = TRANS_GAIN;

	return state;
}
