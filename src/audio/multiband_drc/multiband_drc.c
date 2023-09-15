// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/crossover/crossover_algorithm.h>
#include <sof/audio/drc/drc_algorithm.h>
#include <sof/audio/multiband_drc/multiband_drc.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/msg.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <rtos/panic.h>
#include <rtos/string.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(multiband_drc, CONFIG_SOF_LOG_LEVEL);

/* 0d9f2256-8e4f-47b3-8448-239a334f1191 */
DECLARE_SOF_RT_UUID("multiband_drc", multiband_drc_uuid, 0x0d9f2256, 0x8e4f, 0x47b3,
		    0x84, 0x48, 0x23, 0x9a, 0x33, 0x4f, 0x11, 0x91);

DECLARE_TR_CTX(multiband_drc_tr, SOF_UUID(multiband_drc_uuid), LOG_LEVEL_INFO);

static inline void multiband_drc_iir_reset_state_ch(struct iir_state_df2t *iir)
{
	rfree(iir->coef);
	rfree(iir->delay);

	iir->coef = NULL;
	iir->delay = NULL;
}

static inline void multiband_drc_reset_state(struct multiband_drc_state *state)
{
	int i;

	/* Reset emphasis eq-iir state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		multiband_drc_iir_reset_state_ch(&state->emphasis[i]);

	/* Reset crossover state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		crossover_reset_state_ch(&state->crossover[i]);

	/* Reset drc kernel state */
	for (i = 0; i < SOF_MULTIBAND_DRC_MAX_BANDS; i++)
		drc_reset_state(&state->drc[i]);

	/* Reset deemphasis eq-iir state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		multiband_drc_iir_reset_state_ch(&state->deemphasis[i]);
}

static int multiband_drc_eq_init_coef_ch(struct sof_eq_iir_biquad *coef,
					 struct iir_state_df2t *eq)
{
	int ret;

	eq->coef = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			   sizeof(struct sof_eq_iir_biquad) * SOF_EMP_DEEMP_BIQUADS);
	if (!eq->coef)
		return -ENOMEM;

	/* Coefficients of the first biquad and second biquad */
	ret = memcpy_s(eq->coef, sizeof(struct sof_eq_iir_biquad) * SOF_EMP_DEEMP_BIQUADS,
		       coef, sizeof(struct sof_eq_iir_biquad) * SOF_EMP_DEEMP_BIQUADS);
	assert(!ret);

	/* EQ filters are two 2nd order filters, so only need 4 delay slots
	 * delay[0..1] -> state for first biquad
	 * delay[2..3] -> state for second biquad
	 */
	eq->delay = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    sizeof(uint64_t) * CROSSOVER_NUM_DELAYS_LR4);
	if (!eq->delay)
		return -ENOMEM;

	eq->biquads = SOF_EMP_DEEMP_BIQUADS;
	eq->biquads_in_series = SOF_EMP_DEEMP_BIQUADS;

	return 0;
}

static int multiband_drc_init_coef(struct processing_module *mod, int16_t nch, uint32_t rate)
{
	struct comp_dev *dev = mod->dev;
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	struct sof_eq_iir_biquad *crossover;
	struct sof_eq_iir_biquad *emphasis;
	struct sof_eq_iir_biquad *deemphasis;
	struct sof_multiband_drc_config *config = cd->config;
	struct multiband_drc_state *state = &cd->state;
	uint32_t sample_bytes = get_sample_bytes(cd->source_format);
	int i, ch, ret, num_bands;

	if (!config) {
		comp_err(dev, "multiband_drc_init_coef(), no config is set");
		return -EINVAL;
	}

	num_bands = config->num_bands;

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS) {
		comp_err(dev,
			 "multiband_drc_init_coef(), invalid channels count(%i)", nch);
		return -EINVAL;
	}
	if (config->num_bands > SOF_MULTIBAND_DRC_MAX_BANDS) {
		comp_err(dev, "multiband_drc_init_coef(), invalid bands count(%i)",
			 config->num_bands);
		return -EINVAL;
	}

	comp_info(dev, "multiband_drc_init_coef(), initializing %i-way crossover",
		  config->num_bands);

	/* Crossover: collect the coef array and assign it to every channel */
	crossover = config->crossover_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = crossover_init_coef_ch(crossover, &state->crossover[ch],
					     config->num_bands);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_err(dev,
				 "multiband_drc_init_coef(), could not assign coeffs to ch %d", ch);
			goto err;
		}
	}

	comp_info(dev, "multiband_drc_init_coef(), initializing emphasis_eq");

	/* Emphasis: collect the coef array and assign it to every channel */
	emphasis = config->emp_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = multiband_drc_eq_init_coef_ch(emphasis, &state->emphasis[ch]);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_err(dev, "multiband_drc_init_coef(), could not assign coeffs to ch %d",
				 ch);
			goto err;
		}
	}

	comp_info(dev, "multiband_drc_init_coef(), initializing deemphasis_eq");

	/* Deemphasis: collect the coef array and assign it to every channel */
	deemphasis = config->deemp_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = multiband_drc_eq_init_coef_ch(deemphasis, &state->deemphasis[ch]);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_err(dev, "multiband_drc_init_coef(), could not assign coeffs to ch %d",
				 ch);
			goto err;
		}
	}

	/* Allocate all DRC pre-delay buffers and set delay time with band number */
	for (i = 0; i < num_bands; i++) {
		comp_info(dev, "multiband_drc_init_coef(), initializing drc band %d", i);

		ret = drc_init_pre_delay_buffers(&state->drc[i], (size_t)sample_bytes, (int)nch);
		if (ret < 0) {
			comp_err(dev,
				 "multiband_drc_init_coef(), could not init pre delay buffers");
			goto err;
		}

		ret = drc_set_pre_delay_time(&state->drc[i],
					     cd->config->drc_coef[i].pre_delay_time, rate);
		if (ret < 0) {
			comp_err(dev, "multiband_drc_init_coef(), could not set pre delay time");
			goto err;
		}
	}

	return 0;

err:
	multiband_drc_reset_state(state);
	return ret;
}

static int multiband_drc_setup(struct processing_module *mod, int16_t channels, uint32_t rate)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	int ret;

	/* Reset any previous state */
	multiband_drc_reset_state(&cd->state);

	/* Setup Crossover, Emphasis EQ, Deemphasis EQ, and DRC */
	ret = multiband_drc_init_coef(mod, channels, rate);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * End of Multiband DRC setup code. Next the standard component methods.
 */

static int multiband_drc_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct multiband_drc_comp_data *cd;
	size_t bs = cfg->size;
	int ret;

	comp_info(dev, "multiband_drc_init()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_MULTIBAND_DRC_MAX_BLOB_SIZE) {
		comp_err(dev, "multiband_drc_init(), error: configuration blob size = %u > %d",
			 bs, SOF_MULTIBAND_DRC_MAX_BLOB_SIZE);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->multiband_drc_func = NULL;
	cd->crossover_split = NULL;
#if CONFIG_IPC_MAJOR_4
	/* Initialize to enabled is a workaround for IPC4 kernel version 6.6 and
	 * before where the processing is never enabled via switch control. New
	 * kernel sends the IPC4 switch control and sets this to desired state
	 * before prepare.
	 */
	cd->process_enabled = true;
#else
	cd->process_enabled = false;
#endif

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "multiband_drc_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto cd_fail;
	}

	/* Get configuration data and reset DRC state */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "multiband_drc_init(): comp_init_data_blob() failed.");
		goto cd_fail;
	}
	multiband_drc_reset_state(&cd->state);

	return 0;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	return ret;
}

static int multiband_drc_free(struct processing_module *mod)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "multiband_drc_free()");

	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);
	return 0;
}

#if CONFIG_IPC_MAJOR_3
static int multiband_drc_cmd_set_value(struct processing_module *mod,
				       struct sof_ipc_ctrl_data *cdata)
{
	struct comp_dev *dev = mod->dev;
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "multiband_drc_multiband_drc_cmd_set_value(), SOF_CTRL_CMD_SWITCH");
		if (cdata->num_elems == 1) {
			cd->process_enabled = cdata->chanv[0].value;
			comp_info(dev, "multiband_drc_cmd_set_value(), process_enabled = %d",
				  cd->process_enabled);
			return 0;
		}
	}

	comp_err(mod->dev, "cmd_set_value() error: invalid cdata->cmd");
	return -EINVAL;
}
#endif

static int multiband_drc_set_config(struct processing_module *mod, uint32_t param_id,
				    enum module_cfg_fragment_position pos,
				    uint32_t data_offset_size, const uint8_t *fragment,
				    size_t fragment_size, uint8_t *response,
				    size_t response_size)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "multiband_drc_set_config()");

#if CONFIG_IPC_MAJOR_4
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		comp_dbg(dev, "SOF_IPC4_SWITCH_CONTROL_PARAM_ID id = %d, num_elems = %d",
			 ctl->id, ctl->num_elems);

		if (ctl->id == 0 && ctl->num_elems == 1) {
			cd->process_enabled = ctl->chanv[0].value;
			comp_info(dev, "process_enabled = %d", cd->process_enabled);
		} else {
			comp_err(dev, "Illegal control id = %d, num_elems = %d",
				 ctl->id, ctl->num_elems);
			return -EINVAL;
		}

		return 0;

	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(dev, "multiband_drc_set_config(), illegal control.");
		return -EINVAL;
	}

#elif CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	if (cdata->cmd != SOF_CTRL_CMD_BINARY)
		return multiband_drc_cmd_set_value(mod, cdata);
#endif

	comp_dbg(mod->dev, "multiband_drc_set_config(), SOF_CTRL_CMD_BINARY");
	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

#if CONFIG_IPC_MAJOR_3
static int multiband_drc_cmd_get_value(struct processing_module *mod,
				       struct sof_ipc_ctrl_data *cdata)
{
	struct comp_dev *dev = mod->dev;
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	int j;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "multiband_drc_cmd_get_value(), SOF_CTRL_CMD_SWITCH");
		for (j = 0; j < cdata->num_elems; j++)
			cdata->chanv[j].value = cd->process_enabled;
		if (cdata->num_elems == 1)
			return 0;

		comp_warn(dev, "multiband_drc_cmd_get_value() warn: num_elems should be 1, got %d",
			  cdata->num_elems);
		return 0;
	}

	comp_err(dev, "tdfb_cmd_get_value() error: invalid cdata->cmd");
	return -EINVAL;
}
#endif

static int multiband_drc_get_config(struct processing_module *mod,
				    uint32_t config_id, uint32_t *data_offset_size,
				    uint8_t *fragment, size_t fragment_size)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	comp_dbg(mod->dev, "multiband_drc_get_config()");

#if CONFIG_IPC_MAJOR_3
	if (cdata->cmd != SOF_CTRL_CMD_BINARY)
		return multiband_drc_cmd_get_value(mod, cdata);
#endif

	comp_dbg(mod->dev, "multiband_drc_get_config(), SOF_CTRL_CMD_BINARY");
	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static void multiband_drc_set_alignment(struct audio_stream *source,
					struct audio_stream *sink)
{
	/* Currently no optimizations those would use wider loads and stores */
	audio_stream_init_alignment_constants(1, 1, source);
	audio_stream_init_alignment_constants(1, 1, sink);
}

static int multiband_drc_process(struct processing_module *mod,
				 struct input_stream_buffer *input_buffers, int num_input_buffers,
				 struct output_stream_buffer *output_buffers,
				 int num_output_buffers)
{
	struct multiband_drc_comp_data *cd =  module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	int frames = input_buffers[0].size;
	int ret;

	comp_dbg(dev, "multiband_drc_process()");

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = multiband_drc_setup(mod, (int16_t)audio_stream_get_channels(sink),
					  audio_stream_get_rate(sink));
		if (ret < 0) {
			comp_err(dev, "multiband_drc_process(), failed DRC setup");
			return ret;
		}
	}

	cd->multiband_drc_func(mod, source, sink, frames);

	/* calc new free and available */
	module_update_buffer_position(&input_buffers[0], &output_buffers[0], frames);
	return 0;
}

#if CONFIG_IPC_MAJOR_4
static int multiband_drc_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct sof_ipc_stream_params comp_params;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i, ret;

	comp_dbg(dev, "multiband_drc_params()");

	comp_params = *params;
	comp_params.channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	comp_params.rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;
	comp_params.buffer_fmt = mod->priv.cfg.base_cfg.audio_fmt.interleaving_style;

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	comp_params.frame_fmt = frame_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		comp_params.chmap[i] = (mod->priv.cfg.base_cfg.audio_fmt.ch_map >> i * 4) & 0xf;

	component_set_nearest_period_frames(dev, comp_params.rate);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	ret = buffer_set_params(sinkb, &comp_params, true);

	return ret;
}
#endif /* CONFIG_IPC_MAJOR_4 */

static int multiband_drc_prepare(struct processing_module *mod,
				 struct sof_source **sources, int num_of_sources,
				 struct sof_sink **sinks, int num_of_sinks)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb, *sinkb;
	int channels;
	int rate;
	int ret = 0;

	comp_info(dev, "multiband_drc_prepare()");

#if CONFIG_IPC_MAJOR_4
	ret = multiband_drc_params(mod);
	if (ret < 0)
		return ret;
#endif

	/* DRC component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	multiband_drc_set_alignment(&sourceb->stream, &sinkb->stream);

	/* get source data format */
	cd->source_format = audio_stream_get_frm_fmt(&sourceb->stream);
	channels = audio_stream_get_channels(&sourceb->stream);
	rate = audio_stream_get_rate(&sourceb->stream);

	/* Initialize DRC */
	comp_dbg(dev, "multiband_drc_prepare(), source_format=%d, sink_format=%d",
		 cd->source_format, cd->source_format);
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (cd->config && cd->process_enabled) {
		ret = multiband_drc_setup(mod, channels, rate);
		if (ret < 0) {
			comp_err(dev, "multiband_drc_prepare() error: multiband_drc_setup failed.");
			return ret;
		}

		cd->multiband_drc_func = multiband_drc_find_proc_func(cd->source_format);
		if (!cd->multiband_drc_func) {
			comp_err(dev, "multiband_drc_prepare(), No proc func");
			return -EINVAL;
		}

		cd->crossover_split = crossover_find_split_func(cd->config->num_bands);
		if (!cd->crossover_split) {
			comp_err(dev, "multiband_drc_prepare(), No crossover_split for band num %i",
				 cd->config->num_bands);
			return -EINVAL;
		}
	} else {
		comp_info(dev, "multiband_drc_prepare(), DRC is in passthrough mode");
		cd->multiband_drc_func = multiband_drc_find_proc_func_pass(cd->source_format);
		if (!cd->multiband_drc_func) {
			comp_err(dev, "multiband_drc_prepare(), No proc func passthrough");
			return -EINVAL;
		}
	}

	return ret;
}

static int multiband_drc_reset(struct processing_module *mod)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "multiband_drc_reset()");

	multiband_drc_reset_state(&cd->state);

	cd->source_format = 0;
	cd->multiband_drc_func = NULL;
	cd->crossover_split = NULL;

	return 0;
}

static const struct module_interface multiband_drc_interface = {
	.init = multiband_drc_init,
	.prepare = multiband_drc_prepare,
	.process_audio_stream = multiband_drc_process,
	.set_configuration = multiband_drc_set_config,
	.get_configuration = multiband_drc_get_config,
	.reset = multiband_drc_reset,
	.free = multiband_drc_free
};

DECLARE_MODULE_ADAPTER(multiband_drc_interface, multiband_drc_uuid, multiband_drc_tr);
SOF_MODULE_INIT(multiband_drc, sys_comp_module_multiband_drc_interface_init);
