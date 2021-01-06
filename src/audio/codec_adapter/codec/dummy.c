// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2020 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>
//
// Dummy codec implementation to demonstrate Codec Adapter API

#include <sof/audio/codec_adapter/codec/generic.h>
#include <sof/audio/codec_adapter/codec/dummy.h>

int dummy_codec_init(struct comp_dev *dev)
{
	comp_info(dev, "dummy_codec_init() start");
	return 0;
}

int dummy_codec_prepare(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "dummy_codec_process()");

	codec->cpd.in_buff = rballoc(0, SOF_MEM_CAPS_RAM, cd->period_bytes);
	if (!codec->cpd.in_buff) {
		comp_err(dev, "dummy_codec_prepare(): Failed to alloc in_buff");
		return -ENOMEM;
	}
	codec->cpd.in_buff_size = cd->period_bytes;

	codec->cpd.out_buff = rballoc(0, SOF_MEM_CAPS_RAM, cd->period_bytes);
	if (!codec->cpd.out_buff) {
		comp_err(dev, "dummy_codec_prepare(): Failed to alloc out_buff");
		rfree(codec->cpd.in_buff);
		return -ENOMEM;
	}
	codec->cpd.out_buff_size = cd->period_bytes;

	return 0;
}

int dummy_codec_process(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "dummy_codec_process()");

	memcpy_s(codec->cpd.out_buff, codec->cpd.out_buff_size,
		 codec->cpd.in_buff, codec->cpd.in_buff_size);
	codec->cpd.produced = cd->period_bytes;

	return 0;
}

int dummy_codec_apply_config(struct comp_dev *dev)
{
	comp_info(dev, "dummy_codec_apply_config()");

	/* nothing to do */
	return 0;
}

int dummy_codec_reset(struct comp_dev *dev)
{
	comp_info(dev, "dummy_codec_reset()");

	/* nothing to do */
	return 0;
}

int dummy_codec_free(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);

	comp_info(dev, "dummy_codec_free()");

	rfree(codec->cpd.in_buff);
	rfree(codec->cpd.out_buff);

	return 0;
}
