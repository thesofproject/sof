// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2025 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <rtos/panic.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/msg.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <module/module/interface.h>
#include <zephyr/logging/log.h>

#include <sof/audio/nxp/eap/eap_lib_defines.h>
#include <nxp/eap/EAP_Includes/EAP16.h>
#include <sof/audio/nxp/eap/EAP_Parameter_presets.h>

LOG_MODULE_REGISTER(nxp_eap, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(nxp_eap);
DECLARE_TR_CTX(nxp_eap_tr, SOF_UUID(nxp_eap_uuid), LOG_LEVEL_INFO);

#define NXP_EAP_DEFAULT_MAX_BLOCK_SIZE 480

struct nxp_eap_data  {
	LVM_Handle_t instance;
	LVM_MemTab_t mem_tab;
	LVM_InstParams_t inst_params;
	LVM_ControlParams_t ctrl_params;
	int sample_rate;
	int channels;
	int frame_bytes;
	int audio_time_ms;
	uint32_t buffer_bytes;
};

struct nxp_eap_preset_params {
	char *name;
	LVM_ControlParams_t *params;
};

struct nxp_eap_preset_params nxp_eap_effect_presets[] = {
	{
		.name	= "AllEffectsOff",
		.params = (LVM_ControlParams_t *)&ControlParamSet_allEffectOff,
	},
	{
		.name	= "AutoVolumeLeveler",
		.params	= (LVM_ControlParams_t *)&ControlParamSet_autoVolumeLeveler,
	},
	{
		.name	= "ConcertSound",
		.params	= (LVM_ControlParams_t *)&ControlParamSet_concertSound,
	},
	{
		.name	= "LoudnessMaximiser",
		.params	= (LVM_ControlParams_t *)&ControlParamSet_loudnessMaximiser,
	},
	{
		.name	= "MusicEnhancer",
		.params	= (LVM_ControlParams_t *)&ControlParamSet_musicEnhancerRmsLimiter,
	},
	{
		.name = "VoiceEnhancer",
		.params = (LVM_ControlParams_t *)&ControlParamSet_voiceEnhancer,
	}
};

static int nxp_eap_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct nxp_eap_data *eap;
	LVM_VersionInfo_st info;
	LVM_ReturnStatus_en lvm_ret;
	int ret = 0;

	LVM_GetVersionInfo(&info);

	tr_info(mod->dev, "NXP EAP library, platform: %s version:%s",
		info.pPlatform, info.pVersionNumber);

	eap = rballoc(0, SOF_MEM_CAPS_RAM, sizeof(*eap));
	if (!eap) {
		comp_err(dev, "nxp_eap_init() failed to allocate module private data");
		return -ENOMEM;
	}

	module_set_private_data(mod, eap);

	memcpy(&eap->inst_params, &InstParams_allEffectOff, sizeof(eap->inst_params));

	lvm_ret = LVM_GetMemoryTable(LVM_NULL, &eap->mem_tab, &eap->inst_params);
	if (lvm_ret != LVM_SUCCESS) {
		comp_err(dev, "nxp_eap_init() failed to get memory table %d", lvm_ret);
		rfree(eap);
		return -EINVAL;
	}

	/* mark all pBaseAddress with NULL so that would be easier to implement cleanup */
	for (int i = 0; i < LVM_NR_MEMORY_REGIONS; i++)
		eap->mem_tab.Region[i].pBaseAddress = NULL;

	for (int i = 0; i < LVM_NR_MEMORY_REGIONS; i++) {
		eap->mem_tab.Region[i].pBaseAddress = rballoc(0, SOF_MEM_CAPS_RAM,
							      eap->mem_tab.Region[i].Size);
		if (!eap->mem_tab.Region[i].pBaseAddress) {
			comp_err(dev, "nxp_eap_init() failed to allocate memory for region %d", i);
			ret = -ENOMEM;
			goto free_mem;
		}
	}

	lvm_ret = LVM_GetInstanceHandle(&eap->instance, &eap->mem_tab, &eap->inst_params);
	if (lvm_ret != LVM_SUCCESS) {
		comp_err(dev, "nxp_eap_init() failed to get instance handle err: %d", lvm_ret);
		ret = -EINVAL;
		goto free_mem;
	}

	/* default parameters, no effects */
	memcpy(&eap->ctrl_params, &ControlParamSet_allEffectOff, sizeof(eap->ctrl_params));

	return 0;

free_mem:
	for (int i = 0; i < LVM_NR_MEMORY_REGIONS; i++) {
		if (eap->mem_tab.Region[i].pBaseAddress) {
			rfree(eap->mem_tab.Region[i].pBaseAddress);
			eap->mem_tab.Region[i].pBaseAddress = NULL;
		}
	}
	rfree(eap);
	return ret;
}

static int nxp_eap_free(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct nxp_eap_data *eap = module_get_private_data(mod);

	comp_dbg(dev, "nxp_eap_free()");

	for (int i = 0; i < LVM_NR_MEMORY_REGIONS; i++) {
		if (eap->mem_tab.Region[i].pBaseAddress) {
			rfree(eap->mem_tab.Region[i].pBaseAddress);
			eap->mem_tab.Region[i].pBaseAddress = NULL;
		}
	}

	rfree(eap);

	return 0;
}

static int nxp_eap_prepare(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *md = &mod->priv;
	struct nxp_eap_data *eap = module_get_private_data(mod);
	struct comp_buffer *source = comp_dev_get_first_data_producer(dev);
	const struct audio_stream *stream;

	comp_dbg(dev, "nxp_eap_prepare()");

	stream = &source->stream;
	eap->sample_rate = audio_stream_get_rate(stream);
	eap->channels = audio_stream_get_channels(stream);
	eap->frame_bytes = audio_stream_frame_bytes(stream);
	eap->audio_time_ms = 0;

	/* total bytes needed to be in the input buffer to be processed
	 * by the EAP library
	 */
	eap->buffer_bytes = NXP_EAP_DEFAULT_MAX_BLOCK_SIZE;

	md->mpd.in_buff = rballoc_align(0, SOF_MEM_CAPS_RAM, eap->buffer_bytes, 32);
	if (!md->mpd.in_buff)
		return -ENOMEM;

	md->mpd.out_buff = rballoc_align(0, SOF_MEM_CAPS_RAM, eap->buffer_bytes, 32);
	if (!md->mpd.out_buff) {
		rfree(md->mpd.in_buff);
		return -ENOMEM;
	}

	md->mpd.in_buff_size = eap->buffer_bytes;
	md->mpd.out_buff_size = eap->buffer_bytes;

	return 0;
}

static int nxp_eap_reset(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *md = &mod->priv;

	comp_dbg(dev, "nxp_eap_reset");

	if (md->mpd.in_buff) {
		rfree(md->mpd.in_buff);
		md->mpd.in_buff = NULL;
		md->mpd.in_buff_size = 0;
	}

	if (md->mpd.out_buff) {
		rfree(md->mpd.out_buff);
		md->mpd.out_buff = NULL;
		md->mpd.out_buff_size = 0;
	}

	return 0;
}

static int nxp_eap_process(struct processing_module *mod,
			   struct input_stream_buffer *input_buffers, int num_input_buffers,
			   struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *eap = &mod->priv;
	struct nxp_eap_data *eap_data = module_get_private_data(mod);
	LVM_INT16 *buffer_table[2];
	LVM_ReturnStatus_en ret;

	comp_dbg(dev, "nxp_eap_process()");

	/* we need to input buffer to be completely full to be able to process it */
	if (input_buffers[0].size < eap->mpd.in_buff_size)
		return -ENODATA;

	memcpy_s(eap->mpd.in_buff, eap->mpd.in_buff_size,
		 (uint8_t *)input_buffers[0].data + input_buffers[0].consumed,
		 eap->mpd.in_buff_size);
	eap->mpd.avail = eap->mpd.in_buff_size;

	buffer_table[0] = eap->mpd.out_buff;
	buffer_table[1] = LVM_NULL;

	eap_data->audio_time_ms += eap->mpd.avail / (eap_data->sample_rate / 1000);

	ret = LVM_Process(eap_data->instance, (LVM_INT16 *)eap->mpd.in_buff,
			  (LVM_INT16 **)buffer_table, eap->mpd.avail / eap_data->frame_bytes,
			  eap_data->audio_time_ms);
	if (ret != LVM_SUCCESS) {
		comp_err(dev, "nxp_eap_process() failed with error %d", ret);
		return -EIO;
	}

	eap->mpd.produced = eap->mpd.in_buff_size;
	eap->mpd.consumed = eap->mpd.in_buff_size;

	input_buffers[0].consumed = eap->mpd.consumed;

	/* copy produced samples to output buffer */
	memcpy_s(output_buffers[0].data, eap->mpd.produced, eap->mpd.out_buff, eap->mpd.produced);
	output_buffers[0].size = eap->mpd.produced;

	return 0;
}

static int nxp_eap_cmd_set_value(struct processing_module *mod, struct sof_ipc_ctrl_data *cdata)
{
	int index;
	LVM_ReturnStatus_en ret;
	struct comp_dev *dev = mod->dev;
	struct nxp_eap_data *eap = module_get_private_data(mod);

	index = cdata->chanv[0].value;

	if (index >= ARRAY_SIZE(nxp_eap_effect_presets)) {
		comp_info(dev, "nxp_eap_cmd_set_value() invalid index (%d), config not changed",
			  index);
	} else {
		memcpy(&eap->ctrl_params, nxp_eap_effect_presets[index].params,
		       sizeof(eap->ctrl_params));
		comp_info(dev, "New config set to %s", nxp_eap_effect_presets[index].name);
	}

	ret = LVM_SetControlParameters(eap->instance, &eap->ctrl_params);
	if (ret != LVM_SUCCESS) {
		comp_err(dev, "LVM_SetControlParameters failed with error %d", ret);
		return -EIO;
	}
	return 0;
}

static int nxp_eap_set_config(struct processing_module *mod, uint32_t param_id,
			      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			      const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			      size_t response_size)
{
	struct comp_dev *dev = mod->dev;
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	comp_dbg(dev, "nxp_eap_set_config()");

	if (cdata->cmd != SOF_CTRL_CMD_BINARY)
		return nxp_eap_cmd_set_value(mod, cdata);

	comp_err(dev, "nxp_set_config() binary config not supported");
	return -EINVAL;
}

static int nxp_eap_get_config(struct processing_module *mod,
			      uint32_t param_id, uint32_t *data_offset_size,
			      uint8_t *fragment, size_t fragment_size)
{
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "nxp_eap_get_config()");

	return 0;
}

static const struct module_interface nxp_eap_interface = {
	.init = nxp_eap_init,
	.prepare = nxp_eap_prepare,
	.process_raw_data = nxp_eap_process,
	.set_configuration = nxp_eap_set_config,
	.get_configuration = nxp_eap_get_config,
	.reset = nxp_eap_reset,
	.free = nxp_eap_free,
};

DECLARE_MODULE_ADAPTER(nxp_eap_interface, nxp_eap_uuid, nxp_eap_tr);
SOF_MODULE_INIT(nxp_eap, sys_comp_module_nxp_eap_interface_init);
