// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2020 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>
//
// Passthrough codec implementation to demonstrate Codec Adapter API

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/init.h>

LOG_MODULE_REGISTER(passthrough, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(passthrough);

static int passthrough_codec_init(struct processing_module *mod)
{
	comp_info(mod->dev, "entry");
	return 0;
}

static int passthrough_codec_prepare(struct processing_module *mod,
				     struct sof_source **sources, int num_of_sources,
				     struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct comp_buffer *source = comp_dev_get_first_data_producer(dev);

	comp_info(dev, "entry");

	mod->period_bytes =  audio_stream_period_bytes(&source->stream, dev->frames);

	codec->mpd.in_buff = rballoc(SOF_MEM_FLAG_USER, mod->period_bytes);
	if (!codec->mpd.in_buff) {
		comp_err(dev, "Failed to alloc in_buff");
		return -ENOMEM;
	}
	codec->mpd.in_buff_size = mod->period_bytes;

	codec->mpd.out_buff = rballoc(SOF_MEM_FLAG_USER, mod->period_bytes);
	if (!codec->mpd.out_buff) {
		comp_err(dev, "Failed to alloc out_buff");
		rfree(codec->mpd.in_buff);
		return -ENOMEM;
	}
	codec->mpd.out_buff_size = mod->period_bytes;

	return 0;
}

static int passthrough_codec_init_process(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "entry");

	codec->mpd.produced = 0;
	codec->mpd.consumed = 0;
	codec->mpd.init_done = 1;

	return 0;
}

static int passthrough_codec_process(struct processing_module *mod,
				     struct sof_source **sources, int num_of_sources,
				     struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	const void *src_ptr, *src_buf_start;
	void *snk_ptr, *snk_buf_start;
	size_t src_buf_size, snk_buf_size;
	size_t n_bytes = codec->mpd.in_buff_size;
	size_t size_to_wrap;
	int err;

	comp_dbg(dev, "entry");

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (source_get_data_available(sources[0]) < n_bytes) {
		comp_dbg(dev, "not enough data to process");
		return -ENODATA;
	}

	if (!codec->mpd.init_done)
		passthrough_codec_init_process(mod);

	err = source_get_data(sources[0], n_bytes, &src_ptr, &src_buf_start, &src_buf_size);
	if (err)
		return err;

	/* src_buf_size is the total ring buffer size; handle wrap when copying to in_buff */
	size_to_wrap = (const uint8_t *)src_buf_start + src_buf_size - (const uint8_t *)src_ptr;
	if (n_bytes <= size_to_wrap) {
		memcpy_s(codec->mpd.in_buff, n_bytes, src_ptr, n_bytes);
	} else {
		memcpy_s(codec->mpd.in_buff, n_bytes, src_ptr, size_to_wrap);
		memcpy_s((uint8_t *)codec->mpd.in_buff + size_to_wrap, n_bytes - size_to_wrap,
			 src_buf_start, n_bytes - size_to_wrap);
	}
	source_release_data(sources[0], n_bytes);

	memcpy_s(codec->mpd.out_buff, codec->mpd.out_buff_size, codec->mpd.in_buff, n_bytes);

	err = sink_get_buffer(sinks[0], n_bytes, &snk_ptr, &snk_buf_start, &snk_buf_size);
	if (err)
		return err;

	/* snk_buf_size is the total ring buffer size; handle wrap when copying from out_buff */
	size_to_wrap = (uint8_t *)snk_buf_start + snk_buf_size - (uint8_t *)snk_ptr;
	if (n_bytes <= size_to_wrap) {
		memcpy_s(snk_ptr, n_bytes, codec->mpd.out_buff, n_bytes);
	} else {
		memcpy_s(snk_ptr, n_bytes, codec->mpd.out_buff, size_to_wrap);
		memcpy_s(snk_buf_start, n_bytes - size_to_wrap,
			 (const uint8_t *)codec->mpd.out_buff + size_to_wrap,
			 n_bytes - size_to_wrap);
	}
	sink_commit_buffer(sinks[0], n_bytes);

	return 0;
}

static int passthrough_codec_reset(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;

	comp_info(mod->dev, "entry");

	rfree(codec->mpd.in_buff);
	rfree(codec->mpd.out_buff);
	return 0;
}

static int passthrough_codec_free(struct processing_module *mod)
{
	comp_info(mod->dev, "entry");

	/* Nothing to do */
	return 0;
}

static const struct module_interface passthrough_interface = {
	.init = passthrough_codec_init,
	.prepare = passthrough_codec_prepare,
	.process = passthrough_codec_process,
	.reset = passthrough_codec_reset,
	.free = passthrough_codec_free
};

DECLARE_TR_CTX(passthrough_tr, SOF_UUID(passthrough_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(passthrough_interface, passthrough_uuid, passthrough_tr);
SOF_MODULE_INIT(passthrough, sys_comp_module_passthrough_interface_init);
