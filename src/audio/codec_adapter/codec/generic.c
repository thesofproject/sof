/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 *
 * \file generic.c
 * \brief Generic Codec API
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#include <sof/audio/codec_adapter/codec/generic.h>
#include <sof/audio/codec_adapter/codec/interfaces.h>

static int validate_config(struct codec_config *cfg)
{
	//TODO:
	return 0;
}

int
codec_load_config(struct comp_dev *dev, void *cfg, size_t size,
		  enum codec_cfg_type type)
{
	int ret;
	struct codec_config *dst;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_data *codec = &cd->codec;

	/* wyciagnij cd z dev i skopiuj config do pola "data", na tym etapie nie
	trzeba wiedziec jaki to jest codec id
	*/
	comp_dbg(dev, "codec_load_config() start");

	if (!dev || !cfg || !size) {
		comp_err(dev, "codec_load_config() error: wrong input params! dev %x, cfg %x size %d",
			 (uint32_t)dev, (uint32_t)cfg, size);
		return -EINVAL;
	}

	dst = (type == CODEC_CFG_SETUP) ? &codec->s_cfg :
					  &codec->r_cfg;

	dst->data = rballoc(0, SOF_MEM_CAPS_RAM, size);

	if (!dst->data) {
		comp_err(dev, "codec_load_config() error: failed to allocate space for setup config.");
		ret = -ENOMEM;
		goto err;
	}

	ret = memcpy_s(dst->data, size, cfg, size);
	assert(!ret);

	ret = validate_config(dst->data);
	if (ret) {
		comp_err(dev, "codec_load_config() error: validation of config failed!");
		ret = -EINVAL;
		goto err;
	}

	/* Config loaded, mark it as valid */
	dst->size = size;
	dst->avail = true;

	return ret;
err:
	if (dst->data)
		rfree(dst->data);
	dst->data = NULL;
	return ret;
}

int codec_init(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t codec_id = cd->ca_config.codec_id;
	struct codec_data *codec = &cd->codec;
	struct codec_interface *interface = NULL;
	uint32_t i;
	uint32_t no_of_interfaces = sizeof(interfaces) /
				    sizeof(struct codec_interface);

	comp_info(dev, "codec_init() start");

	/* Find proper interface */
	for (i = 0; i < no_of_interfaces; i++) {
		if (interfaces[i].id == codec_id) {
			interface = &interfaces[i];
			break;
		}
	}
	if (!interface) {
		comp_err(dev, "codec_init() error: could not find codec interface for codec id %x",
			 codec_id);
		ret = -EIO;
		goto out;
	} else if (!interface->init) {//TODO: verify other interfaces
		comp_err(dev, "codec_init() error: codec %x is missing required interfaces",
			 codec_id);
		ret = -EIO;
		goto out;
	}
	/* Assign interface */
	codec->call = interface;
	/* Now we can proceed with codec specific initialization */
	ret = codec->call->init(dev);
	if (ret) {
		comp_err(dev, "codec_init() error %d: codec specific init failed, codec_id %x",
			 ret, codec_id);
		goto out;
	}

	comp_info(dev, "codec_init() done");
	codec->state = CODEC_INITIALIZED;
out:
	return ret;
}

int codec_prepare(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t codec_id = cd->ca_config.codec_id;
	struct codec_data *codec = &cd->codec;

	comp_dbg(dev, "codec_prepare() start");

	if (cd->codec.state != CODEC_INITIALIZED && 0) {
		comp_err(dev, "codec_prepare() error: wrong state of codec %d",
			 cd->codec.state);
		return -EPERM;
	}

	ret = codec->call->prepare(dev);
	if (ret) {
		comp_err(dev, "codec_prepare() error %d: codec specific prepare failed, codec_id %x",
			 ret, codec_id);
		goto out;
	}

	comp_dbg(dev, "codec_prepare() done");
	codec->state = CODEC_PREPARED;
out:
	return ret;
}

int codec_process(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t codec_id = cd->ca_config.codec_id;
	struct codec_data *codec = &cd->codec;

	comp_dbg(dev, "codec_process() start");

	if (cd->codec.state < CODEC_PREPARED) {
		comp_err(dev, "codec_prepare() error: wrong state of codec %x, state %d",
			 cd->ca_config.codec_id, codec->state);
		return -EPERM;
	}

	ret = codec->call->process(dev);
	if (ret) {
		comp_err(dev, "codec_prepare() error %d: codec process failed for codec_id %x",
			 ret, codec_id);
		goto out;
	}

	comp_dbg(dev, "codec_process() end");
out:
	return ret;
}

int codec_apply_runtime_config(struct comp_dev *dev) {
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t codec_id = cd->ca_config.codec_id;
	struct codec_data *codec = &cd->codec;

	comp_dbg(dev, "codec_apply_config() start");

	if (cd->codec.state < CODEC_PREPARED) {
		comp_err(dev, "codec_prepare() error: wrong state of codec %x, state %d",
			 cd->ca_config.codec_id, codec->state);
		return -EPERM;
	}

	ret = codec->call->apply_config(dev);
	if (ret) {
		comp_err(dev, "codec_apply_config() error %d: codec process failed for codec_id %x",
			 ret, codec_id);
		goto out;
	}

	comp_dbg(dev, "codec_apply_config() end");
out:
	return ret;
}
