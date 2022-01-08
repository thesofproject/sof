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

/*****************************************************************************/
/* Local helper functions						     */
/*****************************************************************************/
static int validate_config(struct module_config *cfg);

int module_load_config(struct comp_dev *dev, void *cfg, size_t size, enum module_cfg_type type)
{
	int ret;
	struct module_config *dst;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;

	comp_dbg(dev, "module_load_config() start");

	if (!cfg || !size) {
		comp_err(dev, "module_load_config(): wrong input params! dev %x, cfg %x size %d",
			 (uint32_t)dev, (uint32_t)cfg, size);
		return -EINVAL;
	}

	dst = (type == MODULE_CFG_SETUP) ? &md->s_cfg :
					  &md->r_cfg;

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
		comp_err(dev, "module_load_config(): failed to allocate space for setup config.");
		ret = -ENOMEM;
		goto err;
	}

	ret = memcpy_s(dst->data, size, cfg, size);
	assert(!ret);
	ret = validate_config(dst->data);
	if (ret) {
		comp_err(dev, "module_load_config(): validation of config failed!");
		ret = -EINVAL;
		goto err;
	}

	/* Config loaded, mark it as valid */
	dst->size = size;
	dst->avail = true;

	comp_dbg(dev, "module_load_config() done");
	return ret;
err:
	if (dst->data && type == MODULE_CFG_RUNTIME)
		rfree(dst->data);
	dst->data = NULL;
	return ret;
}

int module_init(struct comp_dev *dev, struct module_interface *interface)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);
	uint32_t codec_id = mod->ca_config.codec_id;
	struct module_data *md = &mod->priv;

	comp_info(dev, "module_init() start");

	if (mod->priv.state == MODULE_INITIALIZED)
		return 0;
	if (mod->priv.state > MODULE_INITIALIZED)
		return -EPERM;

	md->id = codec_id;

	if (!interface) {
		comp_err(dev, "module_init(): could not find module interface for codec id %x",
			 codec_id);
		ret = -EIO;
		goto out;
	} else if (!interface->init || !interface->prepare ||
		   !interface->process || !interface->apply_config ||
		   !interface->reset || !interface->free) {
		comp_err(dev, "module_init(): module %x is missing mandatory interfaces",
			 codec_id);
		ret = -EIO;
		goto out;
	}
	/* Assign interface */
	md->ops = interface;
	/* Init memory list */
	list_init(&md->memory.mem_list);

	/* Now we can proceed with module specific initialization */
	ret = md->ops->init(dev);
	if (ret) {
		comp_err(dev, "module_init() error %d: module specific init failed, codec_id %x",
			 ret, codec_id);
		goto out;
	}

	comp_info(dev, "module_init() done");
	md->state = MODULE_INITIALIZED;
out:
	return ret;
}

void *module_allocate_memory(struct comp_dev *dev, uint32_t size, uint32_t alignment)
{
	struct module_memory *container;
	void *ptr;
	struct processing_module *mod = comp_get_drvdata(dev);

	if (!size) {
		comp_err(dev, "module_allocate_memory: requested allocation of 0 bytes.");
		return NULL;
	}

	/* Allocate memory container */
	container = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct module_memory));
	if (!container) {
		comp_err(dev, "module_allocate_memory: failed to allocate memory container.");
		return NULL;
	}

	/* Allocate memory for module */
	if (alignment)
		ptr = rballoc_align(0, SOF_MEM_CAPS_RAM, size, alignment);
	else
		ptr = rballoc(0, SOF_MEM_CAPS_RAM, size);

	if (!ptr) {
		comp_err(dev, "module_allocate_memory: failed to allocate memory for module %x.",
			 mod->ca_config.codec_id);
		return NULL;
	}
	/* Store reference to allocated memory */
	container->ptr = ptr;
	list_item_prepend(&container->mem_list, &mod->priv.memory.mem_list);

	return ptr;
}

int module_free_memory(struct comp_dev *dev, void *ptr)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_memory *mem;
	struct list_item *mem_list;
	struct list_item *_mem_list;

	if (!ptr)
		return 0;

	/* Find which container keeps this memory */
	list_for_item_safe(mem_list, _mem_list, &mod->priv.memory.mem_list) {
		mem = container_of(mem_list, struct module_memory, mem_list);
		if (mem->ptr == ptr) {
			rfree(mem->ptr);
			list_item_del(&mem->mem_list);
			rfree(mem);
			return 0;
		}
	}

	comp_err(dev, "module_free_memory: error: could not find memory pointed by %p",
		 (uint32_t)ptr);

	return -EINVAL;
}

static int validate_config(struct module_config *cfg)
{
	/* TODO: validation of codec specific setup config */
	return 0;
}

int module_prepare(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);
	uint32_t codec_id = mod->ca_config.codec_id;
	struct module_data *md = &mod->priv;

	comp_dbg(dev, "module_prepare() start");

	if (mod->priv.state == MODULE_IDLE)
		return 0;
	if (mod->priv.state < MODULE_INITIALIZED)
		return -EPERM;

	ret = md->ops->prepare(dev);
	if (ret) {
		comp_err(dev, "module_prepare() error %d: module specific prepare failed, codec_id 0x%x",
			 ret, codec_id);
		goto end;
	}

	/* After prepare is done we no longer need runtime configuration
	 * as it has been applied during the procedure - it is safe to
	 * free it.
	 */
	if (md->r_cfg.data)
		rfree(md->r_cfg.data);

	md->s_cfg.avail = false;
	md->r_cfg.avail = false;
	md->r_cfg.data = NULL;

	md->state = MODULE_IDLE;
	comp_dbg(dev, "module_prepare() done");
end:
	return ret;
}

int module_process(struct comp_dev *dev)
{
	int ret;

	struct processing_module *mod = comp_get_drvdata(dev);
	uint32_t codec_id = mod->ca_config.codec_id;
	struct module_data *md = &mod->priv;

	comp_dbg(dev, "module_process() start");

	if (mod->priv.state != MODULE_IDLE) {
		comp_err(dev, "module_process(): wrong state of module %x, state %d",
			 mod->ca_config.codec_id, md->state);
		return -EPERM;
	}

	ret = md->ops->process(dev);
	if (ret) {
		comp_err(dev, "module_process() error %d: for codec_id %x",
			 ret, codec_id);
		goto out;
	}

	comp_dbg(dev, "module_process() done");
out:
	md->state = MODULE_IDLE;
	return ret;
}

int module_apply_runtime_config(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);
	uint32_t codec_id = mod->ca_config.codec_id;
	struct module_data *md = &mod->priv;

	comp_dbg(dev, "module_apply_config() start");

	ret = md->ops->apply_config(dev);
	if (ret) {
		comp_err(dev, "module_apply_config() error %d: for codec_id %x",
			 ret, codec_id);
		goto out;
	}

	mod->priv.r_cfg.avail = false;
	/* Configuration had been applied, we can free it now. */
	rfree(md->r_cfg.data);
	md->r_cfg.data = NULL;

	comp_dbg(dev, "module_apply_config() end");
out:
	return ret;
}

int module_reset(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;

	ret = md->ops->reset(dev);
	if (ret) {
		comp_err(dev, "module_reset() error %d: module specific reset() failed for codec_id %x",
			 ret, mod->ca_config.codec_id);
		return ret;
	}

	md->r_cfg.avail = false;
	md->r_cfg.size = 0;
	rfree(md->r_cfg.data);

	/* module resets itself to the initial condition after prepare()
	 * so let's change its state to reflect that.
	 */
	md->state = MODULE_IDLE;

	return 0;
}

void codec_free_all_memory(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_memory *mem;
	struct list_item *mem_list;
	struct list_item *_mem_list;

	/* Find which container keeps this memory */
	list_for_item_safe(mem_list, _mem_list, &mod->priv.memory.mem_list) {
		mem = container_of(mem_list, struct module_memory, mem_list);
		rfree(mem->ptr);
		list_item_del(&mem->mem_list);
		rfree(mem);
	}
}

int module_free(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;

	ret = md->ops->free(dev);
	if (ret)
		comp_warn(dev, "module_free(): error: %d for codec_id %x",
			  ret, mod->ca_config.codec_id);

	/* Free all memory requested by module */
	codec_free_all_memory(dev);
	/* Free all memory shared by codec_adapter & module */
	md->s_cfg.avail = false;
	md->s_cfg.size = 0;
	md->r_cfg.avail = false;
	md->r_cfg.size = 0;
	rfree(md->r_cfg.data);
	rfree(md->s_cfg.data);
	if (md->runtime_params)
		rfree(md->runtime_params);

	md->state = MODULE_DISABLED;

	return ret;
}
