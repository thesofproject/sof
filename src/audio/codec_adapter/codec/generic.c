// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * \file generic.c
 * \brief Generic Codec API
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#include <sof/audio/codec_adapter/codec/generic.h>
#include <sof/audio/codec_adapter/interfaces.h>

/*****************************************************************************/
/* Local helper functions						     */
/*****************************************************************************/
static int validate_config(struct codec_config *cfg);

int
codec_load_config(struct comp_dev *dev, void *cfg, size_t size,
		  enum codec_cfg_type type)
{
	int ret;
	struct codec_config *dst;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_data *codec = &cd->codec;

	comp_dbg(dev, "codec_load_config() start");

	if (!dev || !cfg || !size) {
		comp_err(dev, "codec_load_config(): wrong input params! dev %x, cfg %x size %d",
			 (uint32_t)dev, (uint32_t)cfg, size);
		return -EINVAL;
	}

	dst = (type == CODEC_CFG_SETUP) ? &codec->s_cfg :
					  &codec->r_cfg;

	if (!dst->data) {
		/* No space for config available yet, allocate now */
		dst->data = rballoc(0, SOF_MEM_CAPS_RAM, size);
	} else if (dst->size != size) {
		/* The size allocated for previous config doesn't match the new one.
		 * Free old container and allocate new one.
		 */
		rfree(dst->data);
		dst->data = rballoc(0, SOF_MEM_CAPS_RAM, size);
	}
	if (!dst->data) {
		comp_err(dev, "codec_load_config(): failed to allocate space for setup config.");
		ret = -ENOMEM;
		goto err;
	}

	ret = memcpy_s(dst->data, size, cfg, size);
	assert(!ret);
	ret = validate_config(dst->data);
	if (ret) {
		comp_err(dev, "codec_load_config(): validation of config failed!");
		ret = -EINVAL;
		goto err;
	}

	/* Config loaded, mark it as valid */
	dst->size = size;
	dst->avail = true;

	comp_dbg(dev, "codec_load_config() done");
	return ret;
err:
	if (dst->data && type == CODEC_CFG_RUNTIME)
		rfree(dst->data);
	dst->data = NULL;
	return ret;
}

int codec_init(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t codec_id = cd->ca_config.codec_id;
	uint32_t interface_id = CODEC_GET_INTERFACE_ID(codec_id);
	struct codec_data *codec = &cd->codec;
	struct codec_interface *interface = NULL;
	uint32_t i;
	uint32_t no_of_interfaces = ARRAY_SIZE(interfaces);

	comp_info(dev, "codec_init() start");

	if (cd->codec.state == CODEC_INITIALIZED)
		return 0;
	if (cd->codec.state > CODEC_INITIALIZED)
		return -EPERM;

	codec->id = codec_id;

	/* Find proper interface */
	for (i = 0; i < no_of_interfaces; i++) {
		if (interfaces[i].id == interface_id) {
			interface = &interfaces[i];
			break;
		}
	}
	if (!interface) {
		comp_err(dev, "codec_init(): could not find codec interface for codec id %x",
			 codec_id);
		ret = -EIO;
		goto out;
	} else if (!interface->init || !interface->prepare ||
		   !interface->process || !interface->apply_config ||
		   !interface->reset || !interface->free) {
		comp_err(dev, "codec_init(): codec %x is missing mandatory interfaces",
			 codec_id);
		ret = -EIO;
		goto out;
	}
	/* Assign interface */
	codec->ops = interface;
	/* Init memory list */
	list_init(&codec->memory.mem_list);

	/* Now we can proceed with codec specific initialization */
	ret = codec->ops->init(dev);
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

void *codec_allocate_memory(struct comp_dev *dev, uint32_t size,
			    uint32_t alignment)
{
	struct codec_memory *container;
	void *ptr;
	struct comp_data *cd = comp_get_drvdata(dev);

	if (!size) {
		comp_err(dev, "codec_allocate_memory: requested allocation of 0 bytes.");
		return NULL;
	}

	/* Allocate memory container */
	container = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct codec_memory));
	if (!container) {
		comp_err(dev, "codec_allocate_memory: failed to allocate memory container.");
		return NULL;
	}

	/* Allocate memory for codec */
	if (alignment)
		ptr = rballoc_align(0, SOF_MEM_CAPS_RAM, size, alignment);
	else
		ptr = rballoc(0, SOF_MEM_CAPS_RAM, size);

	if (!ptr) {
		comp_err(dev, "codec_allocate_memory: failed to allocate memory for codec %x.",
			 cd->ca_config.codec_id);
		return NULL;
	}
	/* Store reference to allocated memory */
	container->ptr = ptr;
	list_item_prepend(&container->mem_list, &cd->codec.memory.mem_list);

	return ptr;
}

int codec_free_memory(struct comp_dev *dev, void *ptr)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_memory *mem;
	struct list_item *mem_list;
	struct list_item *_mem_list;

	if (!ptr) {
		comp_err(dev, "codec_free_memory: error: NULL pointer passed.");
		return -EINVAL;
	}
	/* Find which container keeps this memory */
	list_for_item_safe(mem_list, _mem_list, &cd->codec.memory.mem_list) {
		mem = container_of(mem_list, struct codec_memory, mem_list);
		if (mem->ptr == ptr) {
			rfree(mem->ptr);
			list_item_del(&mem->mem_list);
			rfree(mem);
			return 0;
		}
	}

	comp_err(dev, "codec_free_memory: error: could not find memory pointed by %p",
		 (uint32_t)ptr);

	return -EINVAL;
}

static int validate_config(struct codec_config *cfg)
{
	/* TODO: validation of codec specific setup config */
	return 0;
}

int codec_prepare(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t codec_id = cd->ca_config.codec_id;
	struct codec_data *codec = &cd->codec;

	comp_dbg(dev, "codec_prepare() start");

	if (cd->codec.state == CODEC_PREPARED)
		return 0;
	if (cd->codec.state < CODEC_INITIALIZED)
		return -EPERM;

	ret = codec->ops->prepare(dev);
	if (ret) {
		comp_err(dev, "codec_prepare() error %d: codec specific prepare failed, codec_id 0x%x",
			 ret, codec_id);
		goto end;
	}

	codec->s_cfg.avail = false;
	codec->r_cfg.avail = false;
	codec->r_cfg.data = NULL;

	/* After prepare is done we no longer need runtime configuration
	 * as it has been applied during the procedure - it is safe to
	 * free it.
	 */
	if (codec->r_cfg.data)
		rfree(codec->r_cfg.data);

	codec->state = CODEC_PREPARED;
	comp_dbg(dev, "codec_prepare() done");
end:
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
		comp_err(dev, "codec_prepare(): wrong state of codec %x, state %d",
			 cd->ca_config.codec_id, codec->state);
		return -EPERM;
	}

	ret = codec->ops->process(dev);
	if (ret) {
		comp_err(dev, "codec_prepare() error %d: codec process failed for codec_id %x",
			 ret, codec_id);
		goto out;
	}

	comp_dbg(dev, "codec_process() done");
out:
	return ret;
}

int codec_apply_runtime_config(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t codec_id = cd->ca_config.codec_id;
	struct codec_data *codec = &cd->codec;

	comp_dbg(dev, "codec_apply_config() start");

	if (cd->codec.state < CODEC_PREPARED) {
		comp_err(dev, "codec_prepare() wrong state of codec %x, state %d",
			 cd->ca_config.codec_id, codec->state);
		return -EPERM;
	}

	ret = codec->ops->apply_config(dev);
	if (ret) {
		comp_err(dev, "codec_apply_config() error %d: codec process failed for codec_id %x",
			 ret, codec_id);
		goto out;
	}

	cd->codec.r_cfg.avail = false;
	/* Configuration had been applied, we can free it now. */
	rfree(codec->r_cfg.data);
	codec->r_cfg.data = NULL;

	comp_dbg(dev, "codec_apply_config() end");
out:
	return ret;
}

int codec_reset(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_data *codec = &cd->codec;

	ret = codec->ops->reset(dev);
	if (ret) {
		comp_err(dev, "codec_apply_config() error %d: codec specific .reset() failed for codec_id %x",
			 ret, cd->ca_config.codec_id);
		return ret;
	}

	codec->r_cfg.avail = false;
	codec->r_cfg.size = 0;
	rfree(codec->r_cfg.data);

	/* Codec reset itself to the initial condition after prepare()
	 * so let's change its state to reflect that.
	 */
	codec->state = CODEC_INITIALIZED;

	return 0;
}

void codec_free_all_memory(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_memory *mem;
	struct list_item *mem_list;
	struct list_item *_mem_list;

	/* Find which container keeps this memory */
	list_for_item_safe(mem_list, _mem_list, &cd->codec.memory.mem_list) {
		mem = container_of(mem_list, struct codec_memory, mem_list);
		rfree(mem->ptr);
		list_item_del(&mem->mem_list);
		rfree(mem);
	}
}

int codec_free(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct codec_data *codec = &cd->codec;

	ret = codec->ops->free(dev);
	if (ret) {
		comp_warn(dev, "codec_apply_config() error %d: codec specific .free() failed for codec_id %x",
			  ret, cd->ca_config.codec_id);
	}
	/* Free all memory requested by codec */
	codec_free_all_memory(dev);
	/* Free all memory shared by codec_adapter & codec */
	codec->s_cfg.avail = false;
	codec->s_cfg.size = 0;
	codec->r_cfg.avail = false;
	codec->r_cfg.size = 0;
	rfree(codec->r_cfg.data);
	rfree(codec->s_cfg.data);
	if (codec->runtime_params)
		rfree(codec->runtime_params);

	codec->state = CODEC_DISABLED;

	return ret;
}
