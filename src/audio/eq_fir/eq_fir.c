// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/eq_fir/eq_fir.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/fir_config.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/eq.h>
#include <user/fir.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(eq_fir, CONFIG_SOF_LOG_LEVEL);

/* 43a90ce7-f3a5-41df-ac06-ba98651ae6a3 */
DECLARE_SOF_RT_UUID("eq-fir", eq_fir_uuid, 0x43a90ce7, 0xf3a5, 0x41df,
		 0xac, 0x06, 0xba, 0x98, 0x65, 0x1a, 0xe6, 0xa3);

DECLARE_TR_CTX(eq_fir_tr, SOF_UUID(eq_fir_uuid), LOG_LEVEL_INFO);

/* src component private data */
struct comp_data {
	struct fir_state_32x16 fir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct comp_data_blob_handler *model_handler;
	struct sof_eq_fir_config *config;
	int32_t *fir_delay;			/**< pointer to allocated RAM */
	size_t fir_delay_size;			/**< allocated size */
	void (*eq_fir_func)(struct fir_state_32x16 fir[],
			    struct input_stream_buffer *bsource,
			    struct output_stream_buffer *bsink,
			    int frames);
	int nch;
};

/*
 * The optimized FIR functions variants need to be updated into function
 * set_fir_func.
 */

#if FIR_HIFI3 || FIR_HIFIEP
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s32;
}
#endif /* CONFIG_FORMAT_S32LE */

#else
/* FIR_GENERIC */
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s32;
}
#endif /* CONFIG_FORMAT_S32LE */
#endif

#if CONFIG_IPC_MAJOR_3
static inline int set_fir_func(struct processing_module *mod, enum sof_ipc_frame fmt)
{
	struct comp_data *cd = module_get_private_data(mod);

	switch (fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		comp_dbg(mod->dev, "set_fir_func(), SOF_IPC_FRAME_S16_LE");
		set_s16_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		comp_dbg(mod->dev, "set_fir_func(), SOF_IPC_FRAME_S24_4LE");
		set_s24_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		comp_dbg(mod->dev, "set_fir_func(), SOF_IPC_FRAME_S32_LE");
		set_s32_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(mod->dev, "set_fir_func(), invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}
#endif /* CONFIG_IPC_MAJOR_3 */

#if CONFIG_IPC_MAJOR_4
static inline int set_fir_func(struct processing_module *mod, enum sof_ipc_frame fmt)
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

static int eq_fir_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct sof_ipc_stream_params comp_params;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i, ret;

	comp_dbg(dev, "eq_fir_params()");

	comp_params = *params;
	comp_params.channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	comp_params.rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;
	comp_params.buffer_fmt = mod->priv.cfg.base_cfg.audio_fmt.interleaving_style;

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	comp_params.frame_fmt = valid_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		comp_params.chmap[i] = (mod->priv.cfg.base_cfg.audio_fmt.ch_map >> i * 4) & 0xf;

	component_set_nearest_period_frames(dev, comp_params.rate);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sinkb);
	ret = buffer_set_params(sink_c, &comp_params, true);
	buffer_release(sink_c);
	return ret;
}
#endif /* CONFIG_IPC_MAJOR_4 */

/* Pass-through functions to replace FIR core while not configured for
 * response.
 */

static void eq_fir_passthrough(struct fir_state_32x16 fir[],
			       struct input_stream_buffer *bsource,
			       struct output_stream_buffer *bsink,
			       int frames)
{
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;

	audio_stream_copy(source, 0, sink, 0, frames * audio_stream_get_channels(source));
}

static void eq_fir_free_delaylines(struct comp_data *cd)
{
	struct fir_state_32x16 *fir = cd->fir;
	int i = 0;

	/* Free the common buffer for all EQs and point then
	 * each FIR channel delay line to NULL.
	 */
	rfree(cd->fir_delay);
	cd->fir_delay = NULL;
	cd->fir_delay_size = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir[i].delay = NULL;
}

static int eq_fir_init_coef(struct comp_dev *dev, struct sof_eq_fir_config *config,
			    struct fir_state_32x16 *fir, int nch)
{
	struct sof_fir_coef_data *lookup[SOF_EQ_FIR_MAX_RESPONSES];
	struct sof_fir_coef_data *eq;
	int16_t *assign_response;
	int16_t *coef_data;
	size_t size_sum = 0;
	int resp = 0;
	int i;
	int j;
	int s;

	/* If this function is called with fir==NULL, then it's supposed to validate the
	 * new configuration without actually altering it.
	 */
	if (!fir) {
		/* Called from validate(), we shall find nch and assign it accordingly,
		 * as the parameter is not valid
		 */
		struct processing_module *mod = comp_get_drvdata(dev);
		struct comp_data *cd = module_get_private_data(mod);

		nch = cd->nch;
	}

	comp_info(dev, "eq_fir_init_coef(): %u responses, %u channels, stream %d channels",
		  config->number_of_responses, config->channels_in_config, nch);

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		comp_err(dev, "eq_fir_init_coef(), invalid channels count");
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_FIR_MAX_RESPONSES) {
		comp_err(dev, "eq_fir_init_coef(), # of resp exceeds max");
		return -EINVAL;
	}

	/* Collect index of response start positions in all_coefficients[]  */
	j = 0;
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	coef_data = ASSUME_ALIGNED(&config->data[config->channels_in_config],
				   4);
	for (i = 0; i < SOF_EQ_FIR_MAX_RESPONSES; i++) {
		if (i < config->number_of_responses) {
			eq = (struct sof_fir_coef_data *)&coef_data[j];
			lookup[i] = eq;
			j += SOF_FIR_COEF_NHEADER + coef_data[j];
		} else {
			lookup[i] = NULL;
		}
	}

	/* Initialize 1st phase */
	for (i = 0; i < nch; i++) {
		/* Check for not reading past blob response to channel assign
		 * map. The previous channel response is assigned for any
		 * additional channels in the stream. It allows to use single
		 * channel configuration to setup multi channel equalization
		 * with the same response.
		 */
		if (i < config->channels_in_config)
			resp = assign_response[i];

		if (resp < 0) {
			/* Initialize EQ channel to bypass and continue with
			 * next channel response.
			 */
			if (fir) {
				comp_info(dev, "eq_fir_init_coef(), ch %d is set to bypass", i);
				fir_reset(&fir[i]);
			}
			continue;
		}

		if (resp >= config->number_of_responses) {
			comp_err(dev, "eq_fir_init_coef(), requested response %d exceeds what has been defined",
				 resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients. */
		eq = lookup[resp];
		s = fir_delay_size(eq);
		if (s > 0) {
			size_sum += s;
		} else {
			comp_info(dev, "eq_fir_init_coef(), FIR length %d is invalid", eq->length);
			return -EINVAL;
		}

#if defined FIR_MAX_LENGTH_BUILD_SPECIFIC
		if (eq->length * nch > FIR_MAX_LENGTH_BUILD_SPECIFIC) {
			comp_err(dev, "Filter length %d exceeds limitation for build.",
				 eq->length);
			return -EINVAL;
		}
#endif

		if (fir) {
			fir_init_coef(&fir[i], eq);
			comp_info(dev, "eq_fir_init_coef(), ch %d is set to response = %d",
				  i, resp);
		}
	}

	return size_sum;
}

static void eq_fir_init_delay(struct fir_state_32x16 *fir,
			      int32_t *delay_start, int nch)
{
	int32_t *fir_delay = delay_start;
	int i;

	/* Initialize 2nd phase to set EQ delay lines pointers */
	for (i = 0; i < nch; i++) {
		if (fir[i].length > 0)
			fir_init_delay(&fir[i], &fir_delay);
	}
}

static int eq_fir_setup(struct comp_dev *dev, struct comp_data *cd, int nch)
{
	int delay_size;

	/* Free existing FIR channels data if it was allocated */
	eq_fir_free_delaylines(cd);

	/* Update number of channels */
	cd->nch = nch;

	/* Set coefficients for each channel EQ from coefficient blob */
	delay_size = eq_fir_init_coef(dev, cd->config, cd->fir, nch);
	if (delay_size < 0)
		return delay_size; /* Contains error code */

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	/* Allocate all FIR channels data in a big chunk and clear it */
	cd->fir_delay = rballoc(0, SOF_MEM_CAPS_RAM, delay_size);
	if (!cd->fir_delay) {
		comp_err(dev, "eq_fir_setup(), delay allocation failed for size %d", delay_size);
		return -ENOMEM;
	}

	memset(cd->fir_delay, 0, delay_size);
	cd->fir_delay_size = delay_size;

	/* Assign delay line to each channel EQ */
	eq_fir_init_delay(cd->fir, cd->fir_delay, nch);
	return 0;
}

static int eq_fir_validator(struct comp_dev *dev, void *new_data, uint32_t new_data_size)
{
	return eq_fir_init_coef(dev, new_data, NULL, -1);
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static int eq_fir_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct comp_data *cd = NULL;
	size_t bs = cfg->size;
	int i;
	int ret;

	comp_info(dev, "eq_fir_init()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_EQ_FIR_MAX_SIZE) {
		comp_err(dev, "eq_fir_init(): coefficients blob size = %u > SOF_EQ_FIR_MAX_SIZE",
			 bs);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	cd->eq_fir_func = NULL;
	cd->fir_delay = NULL;
	cd->fir_delay_size = 0;
	cd->nch = -1;

	/* component model data handler */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "eq_fir_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err;
	}

	md->private = cd;

	/* Allocate and make a copy of the coefficients blob and reset FIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->init_data);
	if (ret < 0) {
		comp_err(dev, "eq_fir_init(): comp_init_data_blob() failed.");
		goto err_init;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	return 0;

err_init:
	comp_data_blob_handler_free(cd->model_handler);
err:
	rfree(cd);
	return ret;
}

static int eq_fir_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_fir_free()");

	eq_fir_free_delaylines(cd);
	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);

	return 0;
}

static int eq_fir_get_config(struct processing_module *mod,
			     uint32_t config_id, uint32_t *data_offset_size,
			     uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_fir_get_config()");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int eq_fir_set_config(struct processing_module *mod, uint32_t config_id,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_fir_set_config()");

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size,
				  fragment, fragment_size);
}

/* copy and process stream data from source to sink buffers */
static int eq_fir_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers,
			  int num_input_buffers,
			  struct output_stream_buffer *output_buffers,
			  int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = input_buffers[0].data;
	uint32_t frame_count = input_buffers[0].size;
	int ret;

	comp_dbg(mod->dev, "eq_fir_process()");

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = eq_fir_setup(mod->dev, cd, audio_stream_get_channels(source));
		if (ret < 0) {
			comp_err(mod->dev, "eq_fir_process(), failed FIR setup");
			return ret;
		} else if (cd->fir_delay_size) {
			comp_dbg(mod->dev, "eq_fir_process(), active");
			ret = set_fir_func(mod, audio_stream_get_frm_fmt(source));
			if (ret < 0)
				return ret;
		} else {
			cd->eq_fir_func = eq_fir_passthrough;
			comp_dbg(mod->dev, "eq_fir_process(), pass-through");
		}
	}

	/*
	 * Process only even number of frames with the FIR function. The
	 * optimized filter function loads the successive input samples from
	 * internal delay line with a 64 bit load operation. The other odd
	 * (or any) number of frames capable FIR version would permanently
	 * break the delay line alignment if called with odd number of frames
	 * so it can't be used here.
	 */

	frame_count &= ~0x1;
	if (frame_count) {
		cd->eq_fir_func(cd->fir, &input_buffers[0], &output_buffers[0], frame_count);
		module_update_buffer_position(&input_buffers[0], &output_buffers[0], frame_count);
	}

	return 0;
}

static void eq_fir_set_alignment(struct audio_stream __sparse_cache *source,
				 struct audio_stream __sparse_cache *sink)
{
	const uint32_t byte_align = 1;
	const uint32_t frame_align_req = 2; /* Process multiples of 2 frames */

	audio_stream_init_alignment_constants(byte_align, frame_align_req, source);
	audio_stream_init_alignment_constants(byte_align, frame_align_req, sink);
}

static int eq_fir_prepare(struct processing_module *mod,
			  struct sof_source __sparse_cache **sources, int num_of_sources,
			  struct sof_sink __sparse_cache **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct comp_dev *dev = mod->dev;
	int channels;
	enum sof_ipc_frame frame_fmt;
	int ret = 0;

	comp_dbg(dev, "eq_fir_prepare()");

#if CONFIG_IPC_MAJOR_4
	ret = eq_fir_params(mod);
	if (ret < 0) {
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return ret;
	}
#endif

	/* EQ component will only ever have 1 source and 1 sink buffer. */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	source_c = buffer_acquire(sourceb);
	sink_c = buffer_acquire(sinkb);
	eq_fir_set_alignment(&source_c->stream, &sink_c->stream);
	channels = audio_stream_get_channels(&sink_c->stream);
	frame_fmt = audio_stream_get_frm_fmt(&source_c->stream);
	buffer_release(sink_c);
	buffer_release(source_c);

	cd->eq_fir_func = eq_fir_passthrough;
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (cd->config) {
		ret = eq_fir_setup(dev, cd, channels);
		if (ret < 0)
			comp_err(dev, "eq_fir_prepare(): eq_fir_setup failed.");
		else if (cd->fir_delay_size)
			ret = set_fir_func(mod, frame_fmt);
		else
			comp_dbg(dev, "eq_fir_prepare(): pass-through");
	}

	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	/* Ensure concurrent changes don't mess with the playback */
	comp_data_blob_set_validator(cd->model_handler, eq_fir_validator);

	return ret;
}

static int eq_fir_reset(struct processing_module *mod)
{
	int i;
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_fir_reset()");

	comp_data_blob_set_validator(cd->model_handler, NULL);

	eq_fir_free_delaylines(cd);

	cd->eq_fir_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	return 0;
}

static struct module_interface eq_fir_interface = {
		.init = eq_fir_init,
		.free = eq_fir_free,
		.set_configuration = eq_fir_set_config,
		.get_configuration = eq_fir_get_config,
		.process_audio_stream = eq_fir_process,
		.prepare = eq_fir_prepare,
		.reset = eq_fir_reset,
};

DECLARE_MODULE_ADAPTER(eq_fir_interface, eq_fir_uuid, eq_fir_tr);
SOF_MODULE_INIT(eq_fir, sys_comp_module_eq_fir_interface_init);
