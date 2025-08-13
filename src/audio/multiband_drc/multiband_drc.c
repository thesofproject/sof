// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/msg.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <module/crossover/crossover_common.h>
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

#include "../drc/drc_algorithm.h"
#include "multiband_drc.h"

LOG_MODULE_REGISTER(multiband_drc, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(multiband_drc);

DECLARE_TR_CTX(multiband_drc_tr, SOF_UUID(multiband_drc_uuid), LOG_LEVEL_INFO);

/* Called from multiband_drc_setup() from multiband_drc_process(), so cannot be __cold */
static void multiband_drc_reset_state(struct processing_module *mod,
				      struct multiband_drc_state *state)
{
	int i;

	/* Reset emphasis eq-iir state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		multiband_drc_iir_reset_state_ch(mod, &state->emphasis[i]);

	/* Reset crossover state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		crossover_reset_state_ch(mod, &state->crossover[i]);

	/* Reset drc kernel state */
	for (i = 0; i < SOF_MULTIBAND_DRC_MAX_BANDS; i++)
		drc_reset_state(&state->drc[i]);

	/* Reset deemphasis eq-iir state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		multiband_drc_iir_reset_state_ch(mod, &state->deemphasis[i]);
}

static int multiband_drc_eq_init_coef_ch(struct processing_module *mod,
					 struct sof_eq_iir_biquad *coef,
					 struct iir_state_df1 *eq)
{
	int ret;

	/* Ensure the LR4 can be processed with the simplified 4th order IIR */
	if (SOF_EMP_DEEMP_BIQUADS != SOF_IIR_DF1_4TH_NUM_BIQUADS)
		return -EINVAL;

	eq->coef = mod_zalloc(mod, sizeof(struct sof_eq_iir_biquad) * SOF_EMP_DEEMP_BIQUADS);
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
	eq->delay = mod_zalloc(mod, sizeof(uint64_t) * CROSSOVER_NUM_DELAYS_LR4);
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

	/* Crossover: determine the split function */
	cd->crossover_split = crossover_find_split_func(config->num_bands);
	if (!cd->crossover_split) {
		comp_err(dev, "multiband_drc_init_coef(), No crossover_split for band count(%i)",
			 config->num_bands);
		return -EINVAL;
	}

	/* Crossover: collect the coef array and assign it to every channel */
	crossover = config->crossover_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = crossover_init_coef_ch(mod, crossover, &state->crossover[ch],
					     config->num_bands);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_err(dev,
				 "multiband_drc_init_coef(), could not assign coeffs to ch %d", ch);
			return ret;
		}
	}

	comp_info(dev, "multiband_drc_init_coef(), initializing emphasis_eq");

	/* Emphasis: collect the coef array and assign it to every channel */
	emphasis = config->emp_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = multiband_drc_eq_init_coef_ch(mod, emphasis, &state->emphasis[ch]);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_err(dev, "multiband_drc_init_coef(), could not assign coeffs to ch %d",
				 ch);
			return ret;
		}
	}

	comp_info(dev, "multiband_drc_init_coef(), initializing deemphasis_eq");

	/* Deemphasis: collect the coef array and assign it to every channel */
	deemphasis = config->deemp_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = multiband_drc_eq_init_coef_ch(mod, deemphasis, &state->deemphasis[ch]);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_err(dev, "multiband_drc_init_coef(), could not assign coeffs to ch %d",
				 ch);
			return ret;
		}
	}

	/* Allocate all DRC pre-delay buffers and set delay time with band number */
	for (i = 0; i < num_bands; i++) {
		comp_info(dev, "multiband_drc_init_coef(), initializing drc band %d", i);

		ret = drc_init_pre_delay_buffers(&state->drc[i], (size_t)sample_bytes, (int)nch);
		if (ret < 0) {
			comp_err(dev,
				 "multiband_drc_init_coef(), could not init pre delay buffers");
			return ret;
		}

		ret = drc_set_pre_delay_time(&state->drc[i],
					     cd->config->drc_coef[i].pre_delay_time, rate);
		if (ret < 0) {
			comp_err(dev, "multiband_drc_init_coef(), could not set pre delay time");
			return ret;
		}
	}

	return 0;
}

/* Called from multiband_drc_process(), so cannot be __cold */
static int multiband_drc_setup(struct processing_module *mod, int16_t channels,
			       uint32_t rate)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);

	/* Reset any previous state */
	multiband_drc_reset_state(mod, &cd->state);

	/* Setup Crossover, Emphasis EQ, Deemphasis EQ, and DRC */
	return multiband_drc_init_coef(mod, channels, rate);
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

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->multiband_drc_func = NULL;
	cd->crossover_split = NULL;
	/* Initialize to enabled is a workaround for IPC4 kernel version 6.6 and
	 * before where the processing is never enabled via switch control. New
	 * kernel sends the IPC4 switch control and sets this to desired state
	 * before prepare.
	 */
	multiband_drc_process_enable(&cd->process_enabled);

	/* Handler for configuration data */
	cd->model_handler = mod_data_blob_handler_new(mod);
	if (!cd->model_handler) {
		comp_err(dev, "comp_data_blob_handler_new() failed.");
		return -ENOMEM;
	}

	/* Get configuration data and reset DRC state */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "comp_init_data_blob() failed.");
		return ret;
	}
	multiband_drc_reset_state(mod, &cd->state);

	return 0;
}

__cold static int multiband_drc_free(struct processing_module *mod)
{
	assert_can_be_cold();

	comp_info(mod->dev, "multiband_drc_free()");

	return 0;
}

__cold static int multiband_drc_set_config(struct processing_module *mod, uint32_t param_id,
					   enum module_cfg_fragment_position pos,
					   uint32_t data_offset_size, const uint8_t *fragment,
					   size_t fragment_size, uint8_t *response,
					   size_t response_size)
{
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	comp_dbg(dev, "multiband_drc_set_config()");

	return multiband_drc_set_ipc_config(mod, param_id,
					    fragment, pos, data_offset_size, fragment_size);
}

__cold static int multiband_drc_get_config(struct processing_module *mod,
					   uint32_t config_id, uint32_t *data_offset_size,
					   uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	assert_can_be_cold();

	comp_dbg(mod->dev, "multiband_drc_get_config()");

	return multiband_drc_get_ipc_config(mod, cdata, fragment_size);
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

	if (cd->process_enabled)
		cd->multiband_drc_func(mod, source, sink, frames);
	else
		multiband_drc_default_pass(mod, source, sink, frames);

	/* calc new free and available */
	module_update_buffer_position(&input_buffers[0], &output_buffers[0], frames);
	return 0;
}

static int multiband_drc_prepare(struct processing_module *mod,
				 struct sof_source **sources, int num_of_sources,
				 struct sof_sink **sinks, int num_of_sinks)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb;
	int channels;
	int rate;
	int ret = 0;

	comp_info(dev, "multiband_drc_prepare()");

	ret = multiband_drc_params(mod);
	if (ret < 0)
		return ret;

	/* DRC component will only ever have 1 source and 1 sink buffer */
	sourceb = comp_dev_get_first_data_producer(dev);
	if (!sourceb) {
		comp_err(dev, "no source buffer");
		return -ENOTCONN;
	}

	/* get source data format */
	cd->source_format = audio_stream_get_frm_fmt(&sourceb->stream);
	channels = audio_stream_get_channels(&sourceb->stream);
	rate = audio_stream_get_rate(&sourceb->stream);

	/* Initialize DRC */
	comp_dbg(dev, "multiband_drc_prepare(), source_format=%d, sink_format=%d",
		 cd->source_format, cd->source_format);
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (cd->config) {
		ret = multiband_drc_setup(mod, channels, rate);
		if (ret < 0) {
			comp_err(dev, "multiband_drc_prepare() error: multiband_drc_setup failed.");
			return ret;
		}
	}

	cd->multiband_drc_func = multiband_drc_find_proc_func(cd->source_format);
	if (!cd->multiband_drc_func) {
		comp_err(dev, "multiband_drc_prepare(), No proc func");
		return -EINVAL;
	}

	return ret;
}

static int multiband_drc_reset(struct processing_module *mod)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "multiband_drc_reset()");

	multiband_drc_reset_state(mod, &cd->state);

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

#if CONFIG_COMP_MULTIBAND_DRC_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(multiband_drc, &multiband_drc_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("MB_DRC", multiband_drc_llext_entry, 1,
				  SOF_REG_UUID(multiband_drc), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(multiband_drc_interface, multiband_drc_uuid, multiband_drc_tr);
SOF_MODULE_INIT(multiband_drc, sys_comp_module_multiband_drc_interface_init);

#endif
