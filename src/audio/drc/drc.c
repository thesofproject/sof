// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
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
#include <module/module/llext.h>
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

#include "drc.h"
#include "drc_algorithm.h"

LOG_MODULE_DECLARE(drc, CONFIG_SOF_LOG_LEVEL);

extern const struct sof_uuid drc_uuid;
extern struct tr_ctx drc_tr;

/* Called from drc_setup() from drc_process(), so cannot be __cold */
void drc_reset_state(struct drc_state *state)
{
	int i;

	rfree(state->pre_delay_buffers[0]);
	for (i = 0; i < PLATFORM_MAX_CHANNELS; ++i) {
		state->pre_delay_buffers[i] = NULL;
	}

	state->detector_average = 0;
	state->compressor_gain = Q_CONVERT_FLOAT(1.0f, 30);

	state->last_pre_delay_frames = DRC_DEFAULT_PRE_DELAY_FRAMES;
	state->pre_delay_read_index = 0;
	state->pre_delay_write_index = DRC_DEFAULT_PRE_DELAY_FRAMES;

	state->envelope_rate = 0;
	state->scaled_desired_gain = 0;

	state->processed = 0;

	state->max_attack_compression_diff_db = INT32_MIN;
}

int drc_init_pre_delay_buffers(struct drc_state *state,
			       size_t sample_bytes,
			       int channels)
{
	size_t bytes_per_channel = sample_bytes * CONFIG_DRC_MAX_PRE_DELAY_FRAMES;
	size_t bytes_total = bytes_per_channel * channels;
	int i;

	/* Allocate pre-delay (lookahead) buffers */
	state->pre_delay_buffers[0] = rballoc(0, SOF_MEM_CAPS_RAM, bytes_total);
	if (!state->pre_delay_buffers[0])
		return -ENOMEM;

	memset(state->pre_delay_buffers[0], 0, bytes_total);

	for (i = 1; i < channels; ++i) {
		state->pre_delay_buffers[i] =
			state->pre_delay_buffers[i - 1] + bytes_per_channel;
	}

	return 0;
}

int drc_set_pre_delay_time(struct drc_state *state,
			   int32_t pre_delay_time,
			   int32_t rate)
{
	int32_t pre_delay_frames;

	/* Re-configure look-ahead section pre-delay if delay time has changed. */
	pre_delay_frames = Q_MULTSR_32X32((int64_t)pre_delay_time, rate, 30, 0, 0);
	if (pre_delay_frames < 0)
		return -EINVAL;
	pre_delay_frames = MIN(pre_delay_frames, CONFIG_DRC_MAX_PRE_DELAY_FRAMES - 1);

	/* Make pre_delay_frames multiplies of DIVISION_FRAMES. This way we
	 * won't split a division of samples into two blocks of memory, so it is
	 * easier to process. This may make the actual delay time slightly less
	 * than the specified value, but the difference is less than 1ms. */
	pre_delay_frames &= ~DRC_DIVISION_FRAMES_MASK;

	/* We need at least one division buffer, so the incoming data won't
	 * overwrite the output data */
	pre_delay_frames = MAX(pre_delay_frames, DRC_DIVISION_FRAMES);

	if (state->last_pre_delay_frames != pre_delay_frames) {
		state->last_pre_delay_frames = pre_delay_frames;
		state->pre_delay_read_index = 0;
		state->pre_delay_write_index = pre_delay_frames;
	}
	return 0;
}

/* Called from drc_process(), so cannot be __cold */
static int drc_setup(struct drc_comp_data *cd, uint16_t channels, uint32_t rate)
{
	uint32_t sample_bytes = get_sample_bytes(cd->source_format);
	int ret;

	/* Reset any previous state */
	drc_reset_state(&cd->state);

	/* Allocate pre-delay buffers */
	ret = drc_init_pre_delay_buffers(&cd->state, (size_t)sample_bytes, (int)channels);
	if (ret < 0)
		return ret;

	/* Set pre-dely time */
	return drc_set_pre_delay_time(&cd->state, cd->config->params.pre_delay_time, rate);
}

/*
 * End of DRC setup code. Next the standard component methods.
 */

__cold static int drc_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct drc_comp_data *cd;
	size_t bs = cfg->size;
	int ret;

	assert_can_be_cold();

	comp_info(dev, "drc_init()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_DRC_MAX_SIZE) {
		comp_err(dev, "drc_init(), error: configuration blob size = %u > %d",
			 bs, SOF_DRC_MAX_SIZE);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "drc_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto cd_fail;
	}

	/* Get configuration data and reset DRC state */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "drc_init(): comp_init_data_blob() failed.");
		goto cd_fail;
	}

	drc_reset_state(&cd->state);

	/* Initialize DRC to enabled. If defined by topology, a control may set
	 * enabled to false before prepare() or during streaming with the switch
	 * control from user space.
	 */
	cd->enabled = true;
	cd->enable_switch = true;
	return 0;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	return ret;
}

__cold static int drc_free(struct processing_module *mod)
{
	struct drc_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	return 0;
}

__cold static int drc_set_config(struct processing_module *mod, uint32_t param_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size)
{
	struct drc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	comp_dbg(dev, "drc_set_config()");

#if CONFIG_IPC_MAJOR_4
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		if (ctl->id == SOF_DRC_CTRL_INDEX_ENABLE_SWITCH &&
		    ctl->num_elems == SOF_DRC_NUM_ELEMS_ENABLE_SWITCH) {
			cd->enable_switch = ctl->chanv[0].value;
			comp_info(dev, "drc_set_config(), enable_switch = %d", cd->enable_switch);
		} else {
			comp_err(dev, "Illegal switch control id = %d, num_elems = %d",
				 ctl->id, ctl->num_elems);
			return -EINVAL;
		}

		return 0;

	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(dev, "drc_set_config(), illegal control.");
		return -EINVAL;
	}
#endif

	comp_info(dev, "drc_set_config(), bytes control");
	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

__cold static int drc_get_config(struct processing_module *mod,
				 uint32_t config_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct drc_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	comp_info(mod->dev, "drc_get_config()");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int drc_process(struct processing_module *mod,
		       struct input_stream_buffer *input_buffers,
		       int num_input_buffers,
		       struct output_stream_buffer *output_buffers,
		       int num_output_buffers)
{
	struct drc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	int frames = input_buffers[0].size;
	int ret;

	comp_dbg(dev, "drc_process()");

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = drc_setup(cd, audio_stream_get_channels(source),
				audio_stream_get_rate(source));
		if (ret < 0) {
			comp_err(dev, "drc_copy(), failed DRC setup");
			return ret;
		}

		/* If new configuration blob is received in pass-through mode, and it
		 * has params.enabled true, then find the DRC processing function.
		 */
		if (cd->drc_func == drc_default_pass && cd->config->params.enabled)
			cd->drc_func = drc_find_proc_func(cd->source_format);

		/* If new configuration blob has params.enabled false, then it is safe
		 * to switch to pass-through mode.
		 */
		if (!cd->config->params.enabled)
			cd->drc_func = drc_default_pass;
	}

	/* Control pass-though in processing function with switch control */
	cd->enabled = cd->config && cd->config->params.enabled && cd->enable_switch;

	cd->drc_func(mod, source, sink, frames);

	/* calc new free and available */
	module_update_buffer_position(&input_buffers[0], &output_buffers[0], frames);
	return 0;
}

#if CONFIG_IPC_MAJOR_4
static void drc_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "drc_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);

	sinkb = comp_dev_get_first_data_consumer(dev);
	ipc4_update_buffer_format(sinkb, &mod->priv.cfg.base_cfg.audio_fmt);

	sourceb = comp_dev_get_first_data_producer(dev);
	ipc4_update_buffer_format(sourceb, &mod->priv.cfg.base_cfg.audio_fmt);
}
#endif /* CONFIG_IPC_MAJOR_4 */

static int drc_prepare(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	struct drc_comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_dev *dev = mod->dev;
	int channels;
	int rate;
	int ret;

	comp_info(dev, "drc_prepare()");

#if CONFIG_IPC_MAJOR_4
	drc_params(mod);
#endif

	/* DRC component will only ever have 1 source and 1 sink buffer */
	sourceb = comp_dev_get_first_data_producer(dev);
	sinkb = comp_dev_get_first_data_consumer(dev);

	/* get source data format */
	cd->source_format = audio_stream_get_frm_fmt(&sourceb->stream);
	channels = audio_stream_get_channels(&sinkb->stream);
	rate = audio_stream_get_rate(&sinkb->stream);

	/* Initialize DRC */
	comp_info(dev, "drc_prepare(), source_format=%d", cd->source_format);
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (cd->config) {
		ret = drc_setup(cd, channels, rate);
		if (ret < 0) {
			comp_err(dev, "drc_prepare() error: drc_setup failed.");
			return ret;
		}

		cd->drc_func = drc_find_proc_func(cd->source_format);
		if (!cd->drc_func) {
			comp_err(dev, "drc_prepare(), No proc func");
			return -EINVAL;
		}

		/* Params.enabled in the configuration blob is the master switch of DRC.
		 * The enable switch control does not have impact if this is not set to
		 * true. When false it is safe to use fast pass-through copy. A
		 * non-enabled blob can be used when same pipeline is used for both
		 * headphone and speaker where DRC should be off for headphone mode.
		 */
		if (!cd->config->params.enabled)
			cd->drc_func = drc_default_pass;
	} else {
		/* Generic function for all formats */
		cd->drc_func = drc_default_pass;
	}

	comp_info(dev, "drc_prepare(), DRC is configured.");
	return 0;
}

static int drc_reset(struct processing_module *mod)
{
	struct drc_comp_data *cd = module_get_private_data(mod);

	drc_reset_state(&cd->state);

	return 0;
}

static const struct module_interface drc_interface = {
	.init = drc_init,
	.prepare = drc_prepare,
	.process_audio_stream = drc_process,
	.set_configuration = drc_set_config,
	.get_configuration = drc_get_config,
	.reset = drc_reset,
	.free = drc_free
};

#if CONFIG_COMP_DRC_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(drc, &drc_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("DRC", drc_llext_entry, 1, SOF_REG_UUID(drc), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(drc_interface, drc_uuid, drc_tr);
SOF_MODULE_INIT(drc, sys_comp_module_drc_interface_init);

#endif
