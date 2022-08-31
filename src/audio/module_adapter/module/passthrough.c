// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2020 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>
//
// Passthrough codec implementation to demonstrate Codec Adapter API

#include <sof/audio/module_adapter/module/generic.h>

/* 376b5e44-9c82-4ec2-bc83-10ea101afa8f */
DECLARE_SOF_RT_UUID("passthrough_codec", passthrough_uuid, 0x376b5e44, 0x9c82, 0x4ec2,
		    0xbc, 0x83, 0x10, 0xea, 0x10, 0x1a, 0xf8, 0x8f);
DECLARE_TR_CTX(passthrough_tr, SOF_UUID(passthrough_uuid), LOG_LEVEL_INFO);

static int passthrough_codec_init(struct processing_module *mod)
{
	comp_info(mod->dev, "passthrough_codec_init() start");
	return 0;
}

static int passthrough_codec_prepare(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;

	comp_info(dev, "passthrough_codec_prepare()");

	codec->mpd.in_buff = rballoc(0, SOF_MEM_CAPS_RAM, mod->period_bytes);
	if (!codec->mpd.in_buff) {
		comp_err(dev, "passthrough_codec_prepare(): Failed to alloc in_buff");
		return -ENOMEM;
	}
	codec->mpd.in_buff_size = mod->period_bytes;

	codec->mpd.out_buff = rballoc(0, SOF_MEM_CAPS_RAM, mod->period_bytes);
	if (!codec->mpd.out_buff) {
		comp_err(dev, "passthrough_codec_prepare(): Failed to alloc out_buff");
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

	comp_dbg(dev, "passthrough_codec_init_process()");

	codec->mpd.produced = 0;
	codec->mpd.consumed = 0;
	codec->mpd.init_done = 1;

	return 0;
}

static int
passthrough_codec_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (input_buffers[0].size < codec->mpd.in_buff_size) {
		comp_dbg(dev, "passthrough_codec_process(): not enough data to process");
		return -ENODATA;
	}

	if (!codec->mpd.init_done)
		passthrough_codec_init_process(mod);

	memcpy_s(codec->mpd.in_buff, codec->mpd.in_buff_size,
		 input_buffers[0].data, codec->mpd.in_buff_size);

	comp_dbg(dev, "passthrough_codec_process()");

	memcpy_s(codec->mpd.out_buff, codec->mpd.out_buff_size,
		 codec->mpd.in_buff, codec->mpd.in_buff_size);
	codec->mpd.produced = mod->period_bytes;
	codec->mpd.consumed = mod->period_bytes;
	input_buffers[0].consumed = codec->mpd.consumed;

	/* copy the produced samples into the output buffer */
	memcpy_s(output_buffers[0].data, codec->mpd.produced, codec->mpd.out_buff,
		 codec->mpd.produced);
	output_buffers[0].size = codec->mpd.produced;

	return 0;
}

static int passthrough_codec_reset(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;

	comp_info(mod->dev, "passthrough_codec_reset()");

	rfree(codec->mpd.in_buff);
	rfree(codec->mpd.out_buff);
	return 0;
}

static int passthrough_codec_free(struct processing_module *mod)
{
	comp_info(dev, "passthrough_codec_free()");

	/* Nothing to do */
	return 0;
}

static struct module_interface passthrough_interface = {
	.init  = passthrough_codec_init,
	.prepare = passthrough_codec_prepare,
	.process = passthrough_codec_process,
	.reset = passthrough_codec_reset,
	.free = passthrough_codec_free
};

DECLARE_MODULE_ADAPTER(passthrough_interface, passthrough_uuid, passthrough_tr);
