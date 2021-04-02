// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2020 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>
//
// Passthrough codec implementation to demonstrate Codec Adapter API

#include <sof/audio/codec_adapter/codec/generic.h>

/* 376b5e44-9c82-4ec2-bc83-10ea101afa8f */
DECLARE_SOF_RT_UUID("passthrough_codec", passthrough_uuid, 0x376b5e44, 0x9c82, 0x4ec2,
		    0xbc, 0x83, 0x10, 0xea, 0x10, 0x1a, 0xf8, 0x8f);
DECLARE_TR_CTX(passthrough_tr, SOF_UUID(passthrough_uuid), LOG_LEVEL_INFO);

static int passthrough_codec_init(struct comp_dev *dev)
{
	comp_info(dev, "passthrough_codec_init() start");
	return 0;
}

static int passthrough_codec_prepare(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "passthrough_codec_process()");

	codec->cpd.in_buff = rballoc(0, SOF_MEM_CAPS_RAM, cd->period_bytes);
	if (!codec->cpd.in_buff) {
		comp_err(dev, "passthrough_codec_prepare(): Failed to alloc in_buff");
		return -ENOMEM;
	}
	codec->cpd.in_buff_size = cd->period_bytes;

	codec->cpd.out_buff = rballoc(0, SOF_MEM_CAPS_RAM, cd->period_bytes);
	if (!codec->cpd.out_buff) {
		comp_err(dev, "passthrough_codec_prepare(): Failed to alloc out_buff");
		rfree(codec->cpd.in_buff);
		return -ENOMEM;
	}
	codec->cpd.out_buff_size = cd->period_bytes;

	return 0;
}

static int passthrough_codec_init_process(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);

	comp_dbg(dev, "passthrough_codec_init_process()");

	codec->cpd.produced = 0;
	codec->cpd.consumed = 0;
	codec->cpd.init_done = 1;

	return 0;
}

static int passthrough_codec_process(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "passthrough_codec_process()");

	memcpy_s(codec->cpd.out_buff, codec->cpd.out_buff_size,
		 codec->cpd.in_buff, codec->cpd.in_buff_size);
	codec->cpd.produced = cd->period_bytes;
	codec->cpd.consumed = cd->period_bytes;

	return 0;
}

static int passthrough_codec_apply_config(struct comp_dev *dev)
{
	comp_info(dev, "passthrough_codec_apply_config()");

	/* nothing to do */
	return 0;
}

static int passthrough_codec_reset(struct comp_dev *dev)
{
	comp_info(dev, "passthrough_codec_reset()");

	/* nothing to do */
	return 0;
}

static int passthrough_codec_free(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);

	comp_info(dev, "passthrough_codec_free()");

	rfree(codec->cpd.in_buff);
	rfree(codec->cpd.out_buff);

	return 0;
}

static struct codec_interface passthrough_interface = {
	.init  = passthrough_codec_init,
	.prepare = passthrough_codec_prepare,
	.init_process = passthrough_codec_init_process,
	.process = passthrough_codec_process,
	.apply_config = passthrough_codec_apply_config,
	.reset = passthrough_codec_reset,
	.free = passthrough_codec_free
};

DECLARE_CODEC_ADAPTER(passthrough_interface, passthrough_uuid, passthrough_tr);
