/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * FFmpeg lookahead peak limiter (alimiter) module for SOF.
 */

#include "alimiter.h"
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>

LOG_MODULE_REGISTER(alimiter, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(alimiter);

#if CONFIG_COMP_ALIMITER_STUB
static const struct alimiter_backend *alimiter_active_backend = &alimiter_stub_backend;
#else
static const struct alimiter_backend *alimiter_active_backend = &alimiter_real_backend;
#endif

__cold static int alimiter_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct alimiter_comp_data *cd;
	int ret;

	comp_info(dev, "alimiter: init");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = alimiter_active_backend;
	cd->enabled = true;
	cd->attack_ms = CONFIG_ALIMITER_ATTACK_MS;
	cd->release_ms = CONFIG_ALIMITER_RELEASE_MS;

	comp_info(dev, "alimiter: backend '%s' attack=%d ms release=%d ms",
		  cd->backend->name, cd->attack_ms, cd->release_ms);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "alimiter: backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

__cold static int alimiter_prepare(struct processing_module *mod,
				  struct sof_source **sources, int num_of_sources,
				  struct sof_sink **sinks, int num_of_sinks)
{
	struct alimiter_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "alimiter: need exactly 1 source and 1 sink");
		return -EINVAL;
	}

	cd->rate = source_get_rate(sources[0]);
	cd->channels = source_get_channels(sources[0]);

	if (cd->channels > ALIMITER_CHANNELS_MAX) {
		comp_err(dev, "alimiter: channels %d exceeds max %d",
			 cd->channels, ALIMITER_CHANNELS_MAX);
		return -EINVAL;
	}

	comp_info(dev, "alimiter: prepare rate=%d ch=%d", cd->rate, cd->channels);

	if (cd->backend->prepare) {
		ret = cd->backend->prepare(mod);
		if (ret) {
			comp_err(dev, "alimiter: backend prepare failed %d", ret);
			return ret;
		}
	}

	return 0;
}

static int alimiter_process(struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks)
{
	struct alimiter_comp_data *cd = module_get_private_data(mod);
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

static int alimiter_reset(struct processing_module *mod)
{
	struct alimiter_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "alimiter: reset");

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

__cold static int alimiter_free(struct processing_module *mod)
{
	struct alimiter_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "alimiter: free");

	if (cd->backend->free)
		cd->backend->free(mod);

	mod_free(mod, cd);
	return 0;
}

static const struct module_interface alimiter_interface = {
	.init    = alimiter_init,
	.prepare = alimiter_prepare,
	.process = alimiter_process,
	.reset   = alimiter_reset,
	.free    = alimiter_free,
};

#if CONFIG_COMP_ALIMITER_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("ALIMITER", &alimiter_interface, 1,
				  SOF_REG_UUID(alimiter), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(alimiter_tr, SOF_UUID(alimiter_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(alimiter_interface, alimiter_uuid, alimiter_tr);
SOF_MODULE_INIT(alimiter, sys_comp_module_alimiter_interface_init);

#endif
