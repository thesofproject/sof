// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.
//
// Author: Ievgen Ganakov <ievgen.ganakov@intel.com>

#include <sof/trace/trace.h>
#include <ipc4/base-config.h>
#include <sof/audio/component_ext.h>
#include <module/module/base.h>
#include <sof/tlv.h>
#include <ipc4/dmic.h>
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

	switch (dd->dai->type) {
	case SOF_DAI_INTEL_DMIC:
		{
			struct dmic_config_data *dmic_cfg = cd->gtw_cfg;

			if (!dmic_cfg) {
				comp_err(dev, "No dmic config found");
				return -EINVAL;
			}

			union dmic_global_cfg *dmic_glb_cfg = &dmic_cfg->dmic_blob.global_cfg;

			/* Get fade period from DMIC blob */
			fade_period = dmic_glb_cfg->ext_global_cfg.fade_in_period;
			/* Convert and assign silence and fade length values */
			dd->gain_data->silence_sg_length =
				frames * dmic_glb_cfg->ext_global_cfg.silence_period;
			dd->gain_data->fade_sg_length = frames * fade_period;
		}
		break;
	default:
		comp_info(dev, "Apply default fade period for dai type %d", dd->dai->type);
		break;
	}

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

int copier_gain_dma_control(union ipc4_connector_node_id node, const char *config_data,
			    size_t config_size, enum sof_ipc_dai_type dai_type)
{
	struct sof_tlv *tlv = (struct sof_tlv *)config_data;
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	struct comp_dev *dev;
	struct list_item *clist;
	int ret;

	list_for_item(clist, &ipc->comp_list) {
		struct gain_dma_control_data *gain_data = NULL;
		void *tlv_val = NULL;

		icd = container_of(clist, struct ipc_comp_dev, list);

		if (!icd || icd->type != COMP_TYPE_COMPONENT)
			continue;

		dev = icd->cd;

		if (!dev || dev->ipc_config.type != SOF_COMP_DAI)
			continue;

		struct processing_module *mod = comp_mod(dev);
		struct copier_data *cd = module_get_private_data(mod);

		switch (dai_type) {
		case SOF_DAI_INTEL_DMIC:
			if (cd->dd[0]->dai->index != node.f.v_index)
				continue;

			if (!config_size) {
				comp_err(dev, "Config length for DMIC couldn't be zero");
				return -EINVAL;
			}

			/* Gain coefficients for DMIC */
			tlv_val = tlv_value_ptr_get(tlv, DMIC_SET_GAIN_COEFFICIENTS);
			if (!tlv_val) {
				comp_err(dev, "No gain coefficients in DMA_CONTROL ipc");
				return -EINVAL;
			}
			gain_data = tlv_val;
			break;
		default:
			comp_warn(dev, "Gain DMA control: no dai type=%d found", dai_type);
			break;
		}

		ret = copier_set_gain(dev, cd->dd[0], gain_data);
		if (ret)
			comp_err(dev, "Gain DMA control: failed to set gain");
		return ret;
	}

	return -ENODEV;
}

int copier_set_gain(struct comp_dev *dev, struct dai_data *dd,
		    struct gain_dma_control_data *gain_data)
{
	struct copier_gain_params *gain_params = dd->gain_data;
	struct ipc4_copier_module_cfg *copier_cfg = dd->dai_spec_config;
	const int channels = copier_cfg->base.audio_fmt.channels_count;
	uint16_t static_gain[MAX_GAIN_COEFFS_CNT];
	int ret;

	if (!gain_data) {
		comp_err(dev, "Gain data is NULL");
		return -EINVAL;
	}

	/* Set gain coefficients */
	comp_info(dev, "Update gain coefficients from DMA_CONTROL ipc");

	size_t gain_coef_size = channels * sizeof(uint16_t);

	ret = memcpy_s(static_gain, gain_coef_size, gain_data->gain_coeffs,
		       gain_coef_size);
	if (ret) {
		comp_err(dev, "memcpy_s failed with error %d", ret);
		return ret;
	}

	for (int i = channels; i < MAX_GAIN_COEFFS_CNT; i++)
		static_gain[i] = static_gain[i % channels];

	ret = memcpy_s(gain_params->gain_coeffs, sizeof(static_gain),
		       static_gain, sizeof(static_gain));
	if (ret) {
		comp_err(dev, "memcpy_s failed with error %d", ret);
		return ret;
	}

	gain_params->unity_gain = copier_is_unity_gain(gain_params);

	return 0;
}
