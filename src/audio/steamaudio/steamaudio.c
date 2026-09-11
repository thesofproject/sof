// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Steam Audio Spatial Offload Component for SOF

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <rtos/init.h>
#include "steamaudio.h"

SOF_DEFINE_REG_UUID(steamaudio);

LOG_MODULE_REGISTER(steamaudio, CONFIG_SOF_LOG_LEVEL);

__cold static int steamaudio_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct steamaudio_comp_data *cd;

	comp_info(dev, "steamaudio: init entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	steamaudio_dsp_init(cd, 48000);
	md->private = cd;
	return 0;
}

static int steamaudio_process(struct processing_module *mod,
			      struct sof_source **sources,
			      int num_of_sources,
			      struct sof_sink **sinks,
			      int num_of_sinks)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	int frames = source_get_data_frames_available(source);
	int sink_frames = sink_get_free_frames(sink);

	frames = MIN(frames, sink_frames);
	if (frames <= 0)
		return 0;

	/* Cap to scratch buffer size */
	if (frames > 256)
		frames = 256;

	if (cd->enable && cd->proc_func)
		return cd->proc_func(mod, source, sink, frames);

	/* Passthrough if disabled */
	source_to_sink_copy(source, sink, true, frames * cd->frame_bytes);
	return 0;
}

static int steamaudio_prepare(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame source_format;
	uint32_t rate;

	comp_dbg(dev, "steamaudio: prepare entry");

	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	cd->frame_bytes = source_get_frame_bytes(sources[0]);
	cd->channels = source_get_channels(sources[0]);
	source_format = source_get_frm_fmt(sources[0]);
	rate = source_get_rate(sources[0]);

	if (rate != cd->sample_rate && rate > 0)
		steamaudio_dsp_init(cd, rate);

	cd->proc_func = steamaudio_find_proc_func(source_format);
	if (!cd->proc_func) {
		comp_err(dev, "steamaudio: no proc func for format %d", source_format);
		return -EINVAL;
	}

	comp_info(dev, "steamaudio: prepared fmt=%d, ch=%d, rate=%u",
		  source_format, cd->channels, rate);
	return 0;
}

static int steamaudio_reset(struct processing_module *mod)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "steamaudio: reset entry");
	if (cd)
		steamaudio_dsp_init(cd, cd->sample_rate);

	return 0;
}

__cold static int steamaudio_free(struct processing_module *mod)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "steamaudio: free entry");

	if (cd)
		mod_free(mod, cd);

	return 0;
}

static const struct module_interface steamaudio_interface = {
	.init = steamaudio_init,
	.prepare = steamaudio_prepare,
	.process = steamaudio_process,
	.set_configuration = steamaudio_set_config,
	.get_configuration = steamaudio_get_config,
	.reset = steamaudio_reset,
	.free = steamaudio_free
};

#if CONFIG_COMP_STEAMAUDIO_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("STEAMAUD", &steamaudio_interface, 1,
				  SOF_REG_UUID(steamaudio), 10);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(steamaudio_tr, SOF_UUID(steamaudio_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(steamaudio_interface, steamaudio_uuid, steamaudio_tr);
SOF_MODULE_INIT(steamaudio, sys_comp_module_steamaudio_interface_init);

#endif
