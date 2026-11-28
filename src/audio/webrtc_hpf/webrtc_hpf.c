// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// WebRTC High-Pass Filter module for SOF.

#include "webrtc_hpf.h"
#include <ipc4/header.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/audio_stream.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>

LOG_MODULE_REGISTER(webrtc_hpf, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(webrtc_hpf);

#if CONFIG_COMP_WEBRTC_HPF_STUB
static const struct webrtc_hpf_backend *webrtc_hpf_active_backend = &webrtc_hpf_stub_backend;
#else
static const struct webrtc_hpf_backend *webrtc_hpf_active_backend = &webrtc_hpf_real_backend;
#endif

__cold static int webrtc_hpf_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct webrtc_hpf_comp_data *cd;
	int ret;

	comp_info(dev, "webrtc_hpf: init");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = webrtc_hpf_active_backend;
	cd->enabled = true;

	comp_info(dev, "webrtc_hpf: backend '%s'", cd->backend->name);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "webrtc_hpf: backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

static void webrtc_hpf_cir_passthrough(struct cir_buf_source *source_buf,
				       struct cir_buf_sink *sink_buf,
				       size_t frames, size_t channels,
				       enum sof_ipc_frame in_fmt,
				       enum sof_ipc_frame out_fmt)
{
	size_t in_sample_bytes = (in_fmt == SOF_IPC_FRAME_S16_LE) ?
				 sizeof(int16_t) : sizeof(int32_t);
	size_t out_sample_bytes = (out_fmt == SOF_IPC_FRAME_S16_LE) ?
				  sizeof(int16_t) : sizeof(int32_t);
	size_t in_frame_bytes = in_sample_bytes * channels;
	size_t out_frame_bytes = out_sample_bytes * channels;

	if (in_fmt == out_fmt) {
		cir_buf_copy(source_buf->ptr, source_buf->buf_start, source_buf->buf_end,
			     sink_buf->ptr, sink_buf->buf_start, sink_buf->buf_end,
			     frames * in_frame_bytes);
		return;
	}

	while (frames) {
		size_t fmax_src = circ_buf_frames_without_wrap(source_buf->ptr, source_buf->buf_end,
							      in_sample_bytes, channels);
		size_t fmax_snk = circ_buf_frames_without_wrap(sink_buf->ptr, sink_buf->buf_end,
							      out_sample_bytes, channels);
		size_t f = MIN(frames, MIN(fmax_src, fmax_snk));
		size_t samples = f * channels;
		size_t i;

		if (in_fmt == SOF_IPC_FRAME_S32_LE) {
			const int32_t *s = source_buf->ptr;
			int16_t *d = sink_buf->ptr;

			for (i = 0; i < samples; i++)
				d[i] = (int16_t)(s[i] >> 16);
		} else {
			const int16_t *s = source_buf->ptr;
			int32_t *d = sink_buf->ptr;

			for (i = 0; i < samples; i++)
				d[i] = ((int32_t)s[i]) << 16;
		}

		frames -= f;
		source_buf->ptr = source_cir_buf_wrap((const char *)source_buf->ptr + f * in_frame_bytes,
						      source_buf->buf_start, source_buf->buf_end);
		sink_buf->ptr = cir_buf_wrap((char *)sink_buf->ptr + f * out_frame_bytes,
					     sink_buf->buf_start, sink_buf->buf_end);
	}
}

__cold static int webrtc_hpf_prepare(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame in_fmt, out_fmt;
	int ret;

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "webrtc_hpf: need exactly 1 source and 1 sink");
		return -EINVAL;
	}

	cd->rate = source_get_rate(sources[0]);
	cd->channels = source_get_channels(sources[0]);
	in_fmt = source_get_frm_fmt(sources[0]);
	out_fmt = sink_get_frm_fmt(sinks[0]);

	if (cd->channels != sink_get_channels(sinks[0]) ||
	    cd->rate != sink_get_rate(sinks[0])) {
		comp_err(dev, "webrtc_hpf: source/sink mismatch");
		return -EINVAL;
	}

	if ((in_fmt != SOF_IPC_FRAME_S16_LE && in_fmt != SOF_IPC_FRAME_S32_LE) ||
	    (out_fmt != SOF_IPC_FRAME_S16_LE && out_fmt != SOF_IPC_FRAME_S32_LE)) {
		comp_err(dev, "webrtc_hpf: unsupported format in=%d out=%d", in_fmt, out_fmt);
		return -EINVAL;
	}

	if (cd->channels > WEBRTC_HPF_CHANNELS_MAX) {
		comp_err(dev, "webrtc_hpf: channels %d exceeds max %d",
			 cd->channels, WEBRTC_HPF_CHANNELS_MAX);
		return -EINVAL;
	}

	cd->in_fmt = in_fmt;
	cd->out_fmt = out_fmt;

	comp_info(dev, "webrtc_hpf: prepare rate=%d ch=%d in_fmt=%d out_fmt=%d",
		  cd->rate, cd->channels, in_fmt, out_fmt);

	if (cd->backend->prepare) {
		ret = cd->backend->prepare(mod);
		if (ret) {
			comp_err(dev, "webrtc_hpf: backend prepare failed %d", ret);
			return ret;
		}
	}

	return 0;
}

static int webrtc_hpf_process(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);
	struct sof_source *src = sources[0];
	struct sof_sink *snk = sinks[0];
	enum sof_ipc_frame in_fmt = source_get_frm_fmt(src);
	enum sof_ipc_frame out_fmt = sink_get_frm_fmt(snk);
	size_t in_sample_bytes = (in_fmt == SOF_IPC_FRAME_S16_LE) ? sizeof(int16_t) : sizeof(int32_t);
	size_t out_sample_bytes = (out_fmt == SOF_IPC_FRAME_S16_LE) ? sizeof(int16_t) : sizeof(int32_t);
	size_t in_frame_bytes = source_get_frame_bytes(src);
	size_t out_frame_bytes = sink_get_frame_bytes(snk);
	int avail = (int)source_get_data_frames_available(src);
	int free_frames = (int)sink_get_free_frames(snk);
	int n = MIN(avail, free_frames);
	size_t in_nbytes, out_nbytes, src_bytes, snk_bytes;
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;
	int ret;

	if (n <= 0)
		return 0;

	in_nbytes = (size_t)n * in_frame_bytes;
	out_nbytes = (size_t)n * out_frame_bytes;

	ret = source_get_data(src, in_nbytes, &source_buf.ptr, &source_buf.buf_start, &src_bytes);
	if (ret)
		return ret;
	source_buf.buf_end = (const char *)source_buf.buf_start + src_bytes;

	ret = sink_get_buffer(snk, out_nbytes, &sink_buf.ptr, &sink_buf.buf_start, &snk_bytes);
	if (ret) {
		source_release_data(src, 0);
		return ret;
	}
	sink_buf.buf_end = (char *)sink_buf.buf_start + snk_bytes;

	if (cd->enabled && cd->backend->process) {
		size_t remaining_frames = n;

		while (remaining_frames) {
			size_t fmax_src = circ_buf_frames_without_wrap(source_buf.ptr, source_buf.buf_end,
								      in_sample_bytes, cd->channels);
			size_t fmax_snk = circ_buf_frames_without_wrap(sink_buf.ptr, sink_buf.buf_end,
								      out_sample_bytes, cd->channels);
			size_t f = MIN(remaining_frames, MIN(fmax_src, fmax_snk));

			ret = cd->backend->process(mod, source_buf.ptr, sink_buf.ptr, f,
						   in_fmt, out_fmt);
			if (ret) {
				webrtc_hpf_cir_passthrough(&source_buf, &sink_buf, remaining_frames,
							   cd->channels, in_fmt, out_fmt);
				break;
			}

			remaining_frames -= f;
			source_buf.ptr = source_cir_buf_wrap((const char *)source_buf.ptr + f * in_frame_bytes,
							     source_buf.buf_start, source_buf.buf_end);
			sink_buf.ptr = cir_buf_wrap((char *)sink_buf.ptr + f * out_frame_bytes,
						    sink_buf.buf_start, sink_buf.buf_end);
		}
	} else {
		webrtc_hpf_cir_passthrough(&source_buf, &sink_buf, n, cd->channels, in_fmt, out_fmt);
	}

	source_release_data(src, in_nbytes);
	sink_commit_buffer(snk, out_nbytes);
	return 0;
}

static int webrtc_hpf_reset(struct processing_module *mod)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "webrtc_hpf: reset");

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

__cold static int webrtc_hpf_free(struct processing_module *mod)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "webrtc_hpf: free");

	if (cd->backend->free)
		cd->backend->free(mod);

	mod_free(mod, cd);
	return 0;
}

static int webrtc_hpf_set_config(struct processing_module *mod, uint32_t param_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (param_id == SOF_IPC4_SWITCH_CONTROL_PARAM_ID) {
		const struct sof_ipc4_control_msg_payload *ctl =
			(const struct sof_ipc4_control_msg_payload *)fragment;

		if (pos != MODULE_CFG_FRAGMENT_SINGLE ||
		    fragment_size < sizeof(struct sof_ipc4_control_msg_payload) +
		    sizeof(struct sof_ipc4_ctrl_value_chan)) {
			comp_err(dev, "webrtc_hpf: invalid payload size %zu", fragment_size);
			return -EINVAL;
		}

		if (ctl->num_elems != 1) {
			comp_err(dev, "webrtc_hpf: invalid num_elems %d", ctl->num_elems);
			return -EINVAL;
		}

		if (ctl->id == 0) {
			cd->enabled = (ctl->chanv[0].value != 0);
			comp_info(dev, "webrtc_hpf: switch enable = %d", cd->enabled);
			return 0;
		}

		comp_err(dev, "webrtc_hpf: unknown control id %d", ctl->id);
		return -EINVAL;
	}

	comp_err(dev, "webrtc_hpf: unsupported param_id 0x%x", param_id);
	return -EINVAL;
}

static int webrtc_hpf_get_config(struct processing_module *mod, uint32_t config_id,
				 uint32_t *data_offset_size, uint8_t *fragment,
				 size_t fragment_size)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);

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

static const struct module_interface webrtc_hpf_interface = {
	.init              = webrtc_hpf_init,
	.prepare           = webrtc_hpf_prepare,
	.process           = webrtc_hpf_process,
	.set_configuration = webrtc_hpf_set_config,
	.get_configuration = webrtc_hpf_get_config,
	.reset             = webrtc_hpf_reset,
	.free              = webrtc_hpf_free,
};

#if CONFIG_COMP_WEBRTC_HPF_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("WRTCHPF", &webrtc_hpf_interface, 1,
				  SOF_REG_UUID(webrtc_hpf), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(webrtc_hpf_tr, SOF_UUID(webrtc_hpf_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(webrtc_hpf_interface, webrtc_hpf_uuid, webrtc_hpf_tr);
SOF_MODULE_INIT(webrtc_hpf, sys_comp_module_webrtc_hpf_interface_init);

#endif
