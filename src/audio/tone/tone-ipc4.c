// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h> /* for SHARED_DATA */
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/trig.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/tone.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ipc4/base-config.h>
#include <sof/audio/sink_api.h>
#include "tone.h"

SOF_DEFINE_REG_UUID(tone);
LOG_MODULE_DECLARE(tone, CONFIG_SOF_LOG_LEVEL);

static int tone_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct module_config *mod_config = &mod->priv.cfg;
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd;
	int i;

	cd = mod_alloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	mod_data->private = cd;

	/* Tnoe only supports 32-bit format */
	cd->tone_func = tone_s32_default;

	/*
	 * set direction for the comp. In the case of the tone generator being used for
	 * echo reference, the number of input pins will be non-zero
	 */
	if (mod_config->nb_input_pins > 0) {
		dev->direction = SOF_IPC_STREAM_CAPTURE;
		cd->mode = TONE_MODE_SILENCE;

	} else {
		dev->direction = SOF_IPC_STREAM_PLAYBACK;
		cd->mode = TONE_MODE_TONEGEN;
	}

	dev->direction_set = true;

	/* Reset tone generator and set channels volumes to default */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		tonegen_reset(&cd->sg[i]);

	return 0;
}

static int tone_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	mod_free(mod, cd);
	return 0;
}

/* set component audio stream parameters */
static int tone_params(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	enum sof_ipc_frame frame_fmt, valid_fmt;

	sinkb = comp_dev_get_first_data_consumer(dev);
	if (!sinkb) {
		comp_err(dev, "no sink buffer found for tone component");
		return -ENOTCONN;
	}

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);
	cd->rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;

	/* Tone supports only S32_LE PCM format atm */
	if (frame_fmt != SOF_IPC_FRAME_S32_LE) {
		comp_err(dev, "unsupported frame_fmt = %u", frame_fmt);
		return -EINVAL;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int tone_process(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);

	if (num_of_sources > 0)
		return cd->tone_func(mod, sinks[0], sources[0]);

	return cd->tone_func(mod, sinks[0], NULL);
}

static int tone_prepare(struct processing_module *mod, struct sof_source **sources,
			int num_of_sources, struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	int32_t f;
	int32_t a;
	int ret;
	int i;

	ret = tone_params(mod);
	if (ret < 0)
		return ret;

	cd->channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;

	for (i = 0; i < cd->channels; i++) {
		f = tonegen_get_f(&cd->sg[i]);
		a = tonegen_get_a(&cd->sg[i]);
		if (tonegen_init(&cd->sg[i], cd->rate, f, a) < 0)
			return -EINVAL;
	}

	return 0;
}

static int tone_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	int i;

	/* Initialize with the defaults */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		tonegen_reset(&cd->sg[i]);

	return 0;
}

static int tone_bind(struct processing_module *mod, struct bind_info *bind_data)
{
	struct comp_data *cd = module_get_private_data(mod);

	/* nothing to do when tone is not the sink */
	if (bind_data->bind_type != COMP_BIND_TYPE_SOURCE)
		return 0;

	/* set passthrough mode when tone generator is bound as a sink */
	cd->mode = TONE_MODE_PASSTHROUGH;

	return 0;
}

static int tone_unbind(struct processing_module *mod, struct bind_info *unbind_data)
{
	struct comp_data *cd = module_get_private_data(mod);

	/* nothing to do when tone is not the sink */
	if (unbind_data->bind_type != COMP_BIND_TYPE_SOURCE)
		return 0;

	/* set silence mode when tone generator is unbound from a source module */
	cd->mode = TONE_MODE_SILENCE;

	return 0;
}

static const struct module_interface tone_interface = {
	.init = tone_init,
	.prepare = tone_prepare,
	.process = tone_process,
	.reset = tone_reset,
	.free = tone_free,
	.bind = tone_bind,
	.unbind = tone_unbind,
};

#if CONFIG_COMP_TONE_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest[] __section(".module") __used = {
	SOF_LLEXT_MODULE_MANIFEST("TONE", &tone_interface, 1, SOF_REG_UUID(tone), 30),
};

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(tone_tr, SOF_UUID(tone_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(tone_interface, tone_uuid, tone_tr);
SOF_MODULE_INIT(tone, sys_comp_module_tone_interface_init);

#endif
