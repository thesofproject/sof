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

#include <sof/audio/module_adapter/module/generic.h>

LOG_MODULE_DECLARE(module_adapter, CONFIG_SOF_LOG_LEVEL);

/*****************************************************************************/
/* Local helper functions						     */
/*****************************************************************************/
static int validate_config(struct module_config *cfg);

int module_load_config(struct comp_dev *dev, void *cfg, size_t size)
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

	dst = &md->cfg;

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
	if (dst->data)
		rfree(dst->data);
	dst->data = NULL;
	return ret;
}

int module_init(struct processing_module *mod, struct module_interface *interface)
{
	int ret;
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "module_init() start");

	if (mod->priv.state == MODULE_INITIALIZED)
		return 0;
	if (mod->priv.state > MODULE_INITIALIZED)
		return -EPERM;

	if (!interface) {
		comp_err(dev, "module_init(): could not find module interface for comp id %d",
			 dev_comp_id(dev));
		return -EIO;
	}

	if (!interface->init || !interface->prepare || !interface->process ||
	    !interface->reset || !interface->free) {
		comp_err(dev, "module_init(): comp %d is missing mandatory interfaces",
			 dev_comp_id(dev));
		return -EIO;
	}

	/* Assign interface */
	md->ops = interface;
	/* Init memory list */
	list_init(&md->memory.mem_list);

	/* Now we can proceed with module specific initialization */
	ret = md->ops->init(mod);
	if (ret) {
		comp_err(dev, "module_init() error %d: module specific init failed, comp id %d",
			 ret, dev_comp_id(dev));
		return ret;
	}

	comp_info(dev, "module_init() done");
	md->state = MODULE_INITIALIZED;

	return ret;
}

void *module_allocate_memory(struct processing_module *mod, uint32_t size, uint32_t alignment)
{
	struct comp_dev *dev = mod->dev;
	struct module_memory *container;
	void *ptr;

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
		comp_err(dev, "module_allocate_memory: failed to allocate memory for comp %x.",
			 dev_comp_id(dev));
		return NULL;
	}
	/* Store reference to allocated memory */
	container->ptr = ptr;
	list_item_prepend(&container->mem_list, &mod->priv.memory.mem_list);

	return ptr;
}

int module_free_memory(struct processing_module *mod, void *ptr)
{
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

	comp_err(mod->dev, "module_free_memory: error: could not find memory pointed by %p",
		 ptr);

	return -EINVAL;
}

static int validate_config(struct module_config *cfg)
{
	/* TODO: validation of codec specific setup config */
	return 0;
}

int module_prepare(struct processing_module *mod)
{
	int ret;
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "module_prepare() start");

	if (mod->priv.state == MODULE_IDLE)
		return 0;
	if (mod->priv.state < MODULE_INITIALIZED)
		return -EPERM;

	ret = md->ops->prepare(mod);
	if (ret) {
		comp_err(dev, "module_prepare() error %d: module specific prepare failed, comp_id %d",
			 ret, dev_comp_id(dev));
		return ret;
	}

	/* After prepare is done we no longer need runtime configuration
	 * as it has been applied during the procedure - it is safe to
	 * free it.
	 */
	if (md->cfg.data)
		rfree(md->cfg.data);

	md->cfg.avail = false;
	md->cfg.data = NULL;

	md->state = MODULE_IDLE;
	comp_dbg(dev, "module_prepare() done");

	return ret;
}

int module_process(struct processing_module *mod, struct input_stream_buffer *input_buffers,
		   int num_input_buffers, struct output_stream_buffer *output_buffers,
		   int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	int ret;

	struct module_data *md = &mod->priv;

	comp_dbg(dev, "module_process() start");

	if (md->state != MODULE_IDLE) {
		comp_err(dev, "module_process(): wrong state of comp_id %x, state %d",
			 dev_comp_id(dev), md->state);
		return -EPERM;
	}

	/* set state to processing */
	md->state = MODULE_PROCESSING;

	ret = md->ops->process(mod, input_buffers, num_input_buffers, output_buffers,
			       num_output_buffers);
	if (ret && ret != -ENOSPC && ret != -ENODATA) {
		comp_err(dev, "module_process() error %d: for comp %d",
			 ret, dev_comp_id(dev));
		return ret;
	}

	comp_dbg(dev, "module_process() done");

	/* reset state to idle */
	md->state = MODULE_IDLE;
	return ret;
}

int module_reset(struct processing_module *mod)
{
	int ret;
	struct module_data *md = &mod->priv;

	/* if the module was never prepared, no need to reset */
	if (md->state < MODULE_IDLE)
		return 0;

	ret = md->ops->reset(mod);
	if (ret) {
		comp_err(mod->dev, "module_reset() error %d: module specific reset() failed for comp %d",
			 ret, dev_comp_id(mod->dev));
		return ret;
	}

	md->cfg.avail = false;
	md->cfg.size = 0;
	rfree(md->cfg.data);

	/*
	 * reset the state to allow the module's prepare callback to be invoked again for the
	 * subsequent triggers
	 */
	md->state = MODULE_INITIALIZED;

	return 0;
}

void module_free_all_memory(struct processing_module *mod)
{
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

int module_free(struct processing_module *mod)
{
	int ret;
	struct module_data *md = &mod->priv;

	ret = md->ops->free(mod);
	if (ret)
		comp_warn(mod->dev, "module_free(): error: %d for %d",
			  ret, dev_comp_id(mod->dev));

	/* Free all memory shared by module_adapter & module */
	md->cfg.avail = false;
	md->cfg.size = 0;
	rfree(md->cfg.data);
	if (md->runtime_params)
		rfree(md->runtime_params);

	md->state = MODULE_DISABLED;

	return ret;
}

/*
 * \brief Set module configuration - Common method to assemble large configuration message
 * \param[in] mod - struct processing_module pointer
 * \param[in] config_id - Configuration ID
 * \param[in] pos - position of the fragment in the large message
 * \param[in] data_offset_size: size of the whole configuration if it is the first fragment or the
 *				 only fragment. Otherwise, it is the offset of the fragment in the
 *				 whole configuration.
 * \param[in] fragment: configuration fragment buffer
 * \param[in] fragment_size: size of @fragment
 * \params[in] response: optional response buffer to fill
 * \params[in] response_size: size of @response
 *
 * \return: 0 upon success or error upon failure
 */
int module_set_configuration(struct processing_module *mod,
			     uint32_t config_id,
			     enum module_cfg_fragment_position pos, size_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	size_t offset = 0;
	uint8_t *dst;
	int ret;

	switch (pos) {
	case MODULE_CFG_FRAGMENT_FIRST:
	case MODULE_CFG_FRAGMENT_SINGLE:
		/*
		 * verify input params & allocate memory for the config blob when the first
		 * fragment arrives
		 */
		md->new_cfg_size = data_offset_size;

		/* Check that there is no previous request in progress */
		if (md->runtime_params) {
			comp_err(dev, "module_set_configuration(): error: busy with previous request");
			return -EBUSY;
		}

		if (!md->new_cfg_size)
			return 0;

		if (md->new_cfg_size > MAX_BLOB_SIZE) {
			comp_err(dev, "module_set_configuration(): error: blob size is too big cfg size %d, allowed %d",
				 md->new_cfg_size, MAX_BLOB_SIZE);
			return -EINVAL;
		}

		/* Allocate buffer for new params */
		md->runtime_params = rballoc(0, SOF_MEM_CAPS_RAM, md->new_cfg_size);
		if (!md->runtime_params) {
			comp_err(dev, "module_set_configuration(): space allocation for new params failed");
			return -ENOMEM;
		}

		memset(md->runtime_params, 0, md->new_cfg_size);
		break;
	default:
		if (!md->runtime_params) {
			comp_err(dev, "module_set_configuration(): error: no memory available for runtime params in consecutive load");
			return -EIO;
		}

		/* set offset for intermediate and last fragments */
		offset = data_offset_size;
		break;
	}

	dst = (uint8_t *)md->runtime_params + offset;

	ret = memcpy_s(dst, md->new_cfg_size - offset, fragment, fragment_size);
	if (ret < 0) {
		comp_err(dev, "module_set_configuration(): error: %d failed to copy fragment",
			 ret);
		return ret;
	}

	/* return as more fragments of config data expected */
	if (pos == MODULE_CFG_FRAGMENT_MIDDLE || pos == MODULE_CFG_FRAGMENT_FIRST)
		return 0;

	/* config fully copied, now load it */
	ret = module_load_config(dev, md->runtime_params, md->new_cfg_size);
	if (ret)
		comp_err(dev, "module_set_configuration(): error %d: config failed", ret);
	else
		comp_dbg(dev, "module_set_configuration(): config load successful");

	md->new_cfg_size = 0;

	if (md->runtime_params)
		rfree(md->runtime_params);
	md->runtime_params = NULL;

	return ret;
}
