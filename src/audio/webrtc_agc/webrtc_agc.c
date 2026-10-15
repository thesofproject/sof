/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * WebRTC Automatic Gain Control module for SOF.
 */

#include "webrtc_agc.h"
#include <ipc4/header.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/audio_stream.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>

LOG_MODULE_REGISTER(webrtc_agc, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(webrtc_agc);

#if CONFIG_COMP_WEBRTC_AGC_STUB
static const struct webrtc_agc_backend *webrtc_agc_active_backend = &webrtc_agc_stub_backend;
#else
static const struct webrtc_agc_backend *webrtc_agc_active_backend = &webrtc_agc_real_backend;
#endif

__cold static int webrtc_agc_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct webrtc_agc_comp_data *cd;
	int ret;

	comp_info(dev, "webrtc_agc: init");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = webrtc_agc_active_backend;
	cd->enabled = true;
	cd->target_dbfs = CONFIG_WEBRTC_AGC_TARGET_DBFS;
	cd->compression_gain_db = CONFIG_WEBRTC_AGC_COMPRESSION_GAIN_DB;
	cd->limiter_enable = IS_ENABLED(CONFIG_WEBRTC_AGC_LIMITER) ? 1 : 0;
	cd->mode = 2; /* kAgcModeAdaptiveDigital */

	comp_info(dev, "webrtc_agc: backend '%s' target=%d dBFS comp=%d dB limiter=%d",
		  cd->backend->name, cd->target_dbfs, cd->compression_gain_db, cd->limiter_enable);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "webrtc_agc: backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

__cold static int webrtc_agc_prepare(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "webrtc_agc: need exactly 1 source and 1 sink");
		return -EINVAL;
	}

	cd->rate = source_get_rate(sources[0]);
	cd->channels = source_get_channels(sources[0]);

	if (cd->rate != 8000 && cd->rate != 16000) {
		comp_err(dev, "webrtc_agc: unsupported rate %d (only 8000 and 16000 Hz supported)",
			 cd->rate);
		return -EINVAL;
	}

	if (cd->channels > WEBRTC_AGC_CHANNELS_MAX) {
		comp_err(dev, "webrtc_agc: channels %d exceeds max %d",
			 cd->channels, WEBRTC_AGC_CHANNELS_MAX);
		return -EINVAL;
	}

	comp_info(dev, "webrtc_agc: prepare rate=%d ch=%d", cd->rate, cd->channels);

	if (cd->backend->prepare) {
		ret = cd->backend->prepare(mod);
		if (ret) {
			comp_err(dev, "webrtc_agc: backend prepare failed %d", ret);
			return ret;
		}
	}

	return 0;
}

static int webrtc_agc_process(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);
	struct sof_source *src = sources[0];
	struct sof_sink *snk = sinks[0];
	enum sof_ipc_frame fmt = source_get_frm_fmt(src);
	size_t sample_bytes = (fmt == SOF_IPC_FRAME_S16_LE) ? sizeof(int16_t) : sizeof(int32_t);
	size_t frame_bytes = source_get_frame_bytes(src);
	int avail = (int)source_get_data_frames_available(src);
	int free_frames = (int)sink_get_free_frames(snk);
	int n = MIN(avail, free_frames);
	size_t nbytes, src_bytes, snk_bytes;
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;
	int ret;

	if (n <= 0)
		return 0;

	nbytes = (size_t)n * frame_bytes;

	ret = source_get_data(src, nbytes, &source_buf.ptr, &source_buf.buf_start, &src_bytes);
	if (ret)
		return ret;
	source_buf.buf_end = (const char *)source_buf.buf_start + src_bytes;

	ret = sink_get_buffer(snk, nbytes, &sink_buf.ptr, &sink_buf.buf_start, &snk_bytes);
	if (ret) {
		source_release_data(src, 0);
		return ret;
	}
	sink_buf.buf_end = (char *)sink_buf.buf_start + snk_bytes;

	if (cd->enabled && cd->backend->process) {
		size_t remaining_frames = n;

		while (remaining_frames) {
			size_t fmax_src = circ_buf_frames_without_wrap(source_buf.ptr, source_buf.buf_end,
								      sample_bytes, cd->channels);
			size_t fmax_snk = circ_buf_frames_without_wrap(sink_buf.ptr, sink_buf.buf_end,
								      sample_bytes, cd->channels);
			size_t f = MIN(remaining_frames, MIN(fmax_src, fmax_snk));

			ret = cd->backend->process(mod, source_buf.ptr, sink_buf.ptr, f, fmt);
			if (ret) {
				cir_buf_copy(source_buf.ptr, source_buf.buf_start, source_buf.buf_end,
					     sink_buf.ptr, sink_buf.buf_start, sink_buf.buf_end,
					     remaining_frames * frame_bytes);
				break;
			}

			remaining_frames -= f;
			source_buf.ptr = source_cir_buf_wrap((const char *)source_buf.ptr + f * frame_bytes,
							     source_buf.buf_start, source_buf.buf_end);
			sink_buf.ptr = cir_buf_wrap((char *)sink_buf.ptr + f * frame_bytes,
						    sink_buf.buf_start, sink_buf.buf_end);
		}
	} else {
		cir_buf_copy(source_buf.ptr, source_buf.buf_start, source_buf.buf_end,
			     sink_buf.ptr, sink_buf.buf_start, sink_buf.buf_end,
			     nbytes);
	}

	source_release_data(src, nbytes);
	sink_commit_buffer(snk, nbytes);
	return 0;
}

static int webrtc_agc_reset(struct processing_module *mod)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "webrtc_agc: reset");

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

__cold static int webrtc_agc_free(struct processing_module *mod)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "webrtc_agc: free");

	if (cd->backend->free)
		cd->backend->free(mod);

	mod_free(mod, cd);
	return 0;
}

static int webrtc_agc_set_config(struct processing_module *mod, uint32_t param_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (param_id == SOF_IPC4_SWITCH_CONTROL_PARAM_ID) {
		const struct sof_ipc4_control_msg_payload *ctl =
			(const struct sof_ipc4_control_msg_payload *)fragment;

		if (pos != MODULE_CFG_FRAGMENT_SINGLE ||
		    fragment_size < sizeof(struct sof_ipc4_control_msg_payload) +
		    sizeof(struct sof_ipc4_ctrl_value_chan)) {
			comp_err(dev, "webrtc_agc: invalid payload size %zu", fragment_size);
			return -EINVAL;
		}

		if (ctl->num_elems != 1) {
			comp_err(dev, "webrtc_agc: invalid num_elems %d", ctl->num_elems);
			return -EINVAL;
		}

		if (ctl->id == 0) {
			cd->enabled = (ctl->chanv[0].value != 0);
			comp_info(dev, "webrtc_agc: switch enable = %d", cd->enabled);
			return 0;
		}

		comp_err(dev, "webrtc_agc: unknown control id %d", ctl->id);
		return -EINVAL;
	}

	comp_err(dev, "webrtc_agc: unsupported param_id 0x%x", param_id);
	return -EINVAL;
}

static int webrtc_agc_get_config(struct processing_module *mod, uint32_t config_id,
				 uint32_t *data_offset_size, uint8_t *fragment,
				 size_t fragment_size)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);

	if (config_id == SOF_IPC4_SWITCH_CONTROL_PARAM_ID) {
		struct sof_ipc4_control_msg_payload *ctl =
			(struct sof_ipc4_control_msg_payload *)fragment;
		ctl->id = 0;
		ctl->num_elems = 1;
		ctl->chanv[0].channel = 0;
		ctl->chanv[0].value = cd->enabled ? 1 : 0;
		*data_offset_size = sizeof(struct sof_ipc4_control_msg_payload) +
				    sizeof(struct sof_ipc4_ctrl_value_chan);
		return 0;
	}

	return -EINVAL;
}

static const struct module_interface webrtc_agc_interface = {
	.init              = webrtc_agc_init,
	.prepare           = webrtc_agc_prepare,
	.process           = webrtc_agc_process,
	.set_configuration = webrtc_agc_set_config,
	.get_configuration = webrtc_agc_get_config,
	.reset             = webrtc_agc_reset,
	.free              = webrtc_agc_free,
};

#if CONFIG_COMP_WEBRTC_AGC_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("WRTCAGC", &webrtc_agc_interface, 1,
				  SOF_REG_UUID(webrtc_agc), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(webrtc_agc_tr, SOF_UUID(webrtc_agc_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(webrtc_agc_interface, webrtc_agc_uuid, webrtc_agc_tr);
SOF_MODULE_INIT(webrtc_agc, sys_comp_module_webrtc_agc_interface_init);

#endif
