/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * FFmpeg stereo widener (stereowiden) module for SOF.
 */

#include "stereowiden.h"
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>

LOG_MODULE_REGISTER(stereowiden, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(stereowiden);

#if CONFIG_COMP_STEREOWIDEN_STUB
static const struct stereowiden_backend *stereowiden_active_backend = &stereowiden_stub_backend;
#else
static const struct stereowiden_backend *stereowiden_active_backend = &stereowiden_real_backend;
#endif

__cold static int stereowiden_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct stereowiden_comp_data *cd;
	int ret;

	comp_info(dev, "stereowiden: init");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = stereowiden_active_backend;
	cd->enabled = true;
	cd->delay_ms = CONFIG_STEREOWIDEN_DELAY_MS;
	/* Q30 defaults: drymix = 0.8, crossfeed = 0.3, feedback = 0.3 */
	cd->drymix = (int32_t)((int64_t)858993459LL);
	cd->crossfeed = (int32_t)((int64_t)322122547LL);
	cd->feedback = (int32_t)((int64_t)322122547LL);

	comp_info(dev, "stereowiden: backend '%s' delay=%d ms",
		  cd->backend->name, cd->delay_ms);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "stereowiden: backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

__cold static int stereowiden_prepare(struct processing_module *mod,
				      struct sof_source **sources, int num_of_sources,
				      struct sof_sink **sinks, int num_of_sinks)
{
	struct stereowiden_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "stereowiden: need exactly 1 source and 1 sink");
		return -EINVAL;
	}

	cd->rate = source_get_rate(sources[0]);
	cd->channels = source_get_channels(sources[0]);

	if (cd->channels > STEREOWIDEN_CHANNELS_MAX) {
		comp_err(dev, "stereowiden: channels %d exceeds max %d",
			 cd->channels, STEREOWIDEN_CHANNELS_MAX);
		return -EINVAL;
	}

	comp_info(dev, "stereowiden: prepare rate=%d ch=%d", cd->rate, cd->channels);

	if (cd->backend->prepare) {
		ret = cd->backend->prepare(mod);
		if (ret) {
			comp_err(dev, "stereowiden: backend prepare failed %d", ret);
			return ret;
		}
	}

	return 0;
}

static int stereowiden_process(struct processing_module *mod,
			       struct sof_source **sources, int num_of_sources,
			       struct sof_sink **sinks, int num_of_sinks)
{
	struct stereowiden_comp_data *cd = module_get_private_data(mod);
	struct sof_source *src = sources[0];
	struct sof_sink *snk = sinks[0];
	enum sof_ipc_frame fmt = source_get_frm_fmt(src);
	int avail = (int)source_get_data_frames_available(src);
	int free_frames = (int)sink_get_free_frames(snk);
	int n = MIN(avail, free_frames);
	size_t nbytes, buf_size;
	void const *rd_ptr, *buf_start;
	void *wr_ptr, *wr_buf_start;
	int ret;

	if (n <= 0)
		return 0;

	nbytes = (size_t)n * source_get_frame_bytes(src);

	ret = source_get_data(src, nbytes, &rd_ptr, &buf_start, &buf_size);
	if (ret)
		return ret;

	ret = sink_get_buffer(snk, nbytes, &wr_ptr, &wr_buf_start, &buf_size);
	if (ret) {
		source_release_data(src, 0);
		return ret;
	}

	if (cd->enabled && cd->backend->process) {
		ret = cd->backend->process(mod, rd_ptr, wr_ptr, n, fmt);
		if (ret)
			memcpy(wr_ptr, rd_ptr, nbytes);
	} else {
		memcpy(wr_ptr, rd_ptr, nbytes);
	}

	source_release_data(src, nbytes);
	sink_commit_buffer(snk, nbytes);
	return 0;
}

__cold static int stereowiden_reset(struct processing_module *mod)
{
	struct stereowiden_comp_data *cd = module_get_private_data(mod);

	if (cd && cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

__cold static int stereowiden_free(struct processing_module *mod)
{
	struct stereowiden_comp_data *cd = module_get_private_data(mod);

	if (cd) {
		if (cd->backend->free)
			cd->backend->free(mod);
		mod_free(mod, cd);
	}

	return 0;
}

static const struct module_interface stereowiden_interface = {
	.init    = stereowiden_init,
	.prepare = stereowiden_prepare,
	.process = stereowiden_process,
	.reset   = stereowiden_reset,
	.free    = stereowiden_free,
};

#if CONFIG_COMP_STEREOWIDEN_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("STWIDEN", &stereowiden_interface, 1,
				  SOF_REG_UUID(stereowiden), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(stereowiden_tr, SOF_UUID(stereowiden_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(stereowiden_interface, stereowiden_uuid, stereowiden_tr);
SOF_MODULE_INIT(stereowiden, sys_comp_module_stereowiden_interface_init);

#endif
