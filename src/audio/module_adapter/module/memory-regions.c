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

#include <rtos/symbol.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/lib/fast-get.h>

LOG_MODULE_DECLARE(module_adapter, CONFIG_SOF_LOG_LEVEL);

/**
 * Allocates aligned buffer memory block for module.
 * @param mod		Pointer to the module this memory block is allocated for.
 * @param bytes		Size in bytes.
 * @param alignment	Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * The allocated memory is automatically freed when the module is
 * unloaded. The back-end, rballoc(), always aligns the memory to
 * PLATFORM_DCACHE_ALIGN at the minimum.
 */
void *mod_balloc_align(struct processing_module *mod, size_t size, size_t alignment)
{
	void *ptr;

	if (!size) {
		comp_err(mod->dev, "requested allocation of 0 bytes.");
		return NULL;
	}

	/* do we need to use the dynamic heap or the static heap? */
	struct vregion *vregion = module_get_vregion(mod);
	if (mod->priv.state != MODULE_INITIALIZED) {
		/* lifetime allocator */
		ptr = vregion_alloc_align(vregion, VREGION_MEM_TYPE_LIFETIME, size, alignment);
	} else {
		/* interim allocator */
		ptr = vregion_alloc_align(vregion, VREGION_MEM_TYPE_INTERIM, size, alignment);
	}

	if (!ptr) {
		comp_err(mod->dev, "Failed to alloc %zu bytes %zu alignment for comp %#x.",
			 size, alignment, dev_comp_id(mod->dev));
		return NULL;
	}

	return ptr;
}
EXPORT_SYMBOL(mod_balloc_align);

/**
 * Allocates aligned memory block with flags for module.
 * @param mod		Pointer to the module this memory block is allocated for.
 * @param flags		Allocator flags.
 * @param bytes		Size in bytes.
 * @param alignment	Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * The allocated memory is automatically freed when the module is unloaded.
 */
void *mod_alloc_ext(struct processing_module *mod, uint32_t flags, size_t size, size_t alignment)
{
	void *ptr;

	if (!size) {
		comp_err(mod->dev, "requested allocation of 0 bytes.");
		return NULL;
	}

	/* do we need to use the dynamic heap or the static heap? */
	struct vregion *vregion = module_get_vregion(mod);
	if (mod->priv.state != MODULE_INITIALIZED) {
		/* static allocator */
		ptr = vregion_alloc_align(vregion, VREGION_MEM_TYPE_LIFETIME, size, alignment);
	} else {
		/* dynamic allocator */
		ptr = vregion_alloc_align(vregion, VREGION_MEM_TYPE_INTERIM, size, alignment);
	}

	if (!ptr) {
		comp_err(mod->dev, "Failed to alloc %zu bytes %zu alignment for comp %#x.",
			 size, alignment, dev_comp_id(mod->dev));
		return NULL;
	}

	return ptr;
}
EXPORT_SYMBOL(mod_alloc_ext);


/**
 * Creates a blob handler and releases it when the module is unloaded
 * @param mod	Pointer to module this memory block is allocated for.
 * @return Pointer to the created data blob handler
 *
 * Like comp_data_blob_handler_new() but the handler is automatically freed.
 */
#if CONFIG_COMP_BLOB
struct comp_data_blob_handler *
mod_data_blob_handler_new(struct processing_module *mod)
{
	struct comp_data_blob_handler *bhp;

	bhp = comp_data_blob_handler_new_ext(mod->dev, false, NULL, NULL);
	if (!bhp) {
		return NULL;
	}

	return bhp;
}
EXPORT_SYMBOL(mod_data_blob_handler_new);
#endif

/**
 * Make a module associated shared SRAM copy of DRAM read-only data.
 * @param mod	Pointer to module this copy is allocated for.
 * @return Pointer to the SRAM copy.
 *
 * Like fast_get() but the handler is automatically freed.
 */
#if CONFIG_FAST_GET
const void *mod_fast_get(struct processing_module *mod, const void * const dram_ptr, size_t size)
{
	const void *ptr;

	ptr = fast_get(mod, dram_ptr, size);
	if (!ptr) {
		return NULL;
	}

	return ptr;
}
EXPORT_SYMBOL(mod_fast_get);
#endif

/**
 * Frees the memory block removes it from module's book keeping.
 * @param mod	Pointer to module this memory block was allocated for.
 * @param ptr	Pointer to the memory block.
 */
int mod_free(struct processing_module *mod, const void *ptr)
{
	vregion_free(module_get_vregion(mod), (__sparse_force void *)ptr);
	return 0;
}
EXPORT_SYMBOL(mod_free);

/**
 * Frees all the resources registered for this module
 * @param mod	Pointer to module that should have its resource freed.
 *
 * This function is called automatically when the module is unloaded.
 */
void mod_free_all(struct processing_module *mod)
{
	// TODO free the vregion for the module ?
}
EXPORT_SYMBOL(mod_free_all);

struct processing_module *module_adapter_mem_alloc(const struct comp_driver *drv,
						   const struct comp_ipc_config *config,
						   struct pipeline *pipeline)
{
	struct comp_dev *dev = comp_alloc(drv, sizeof(*dev));
	struct processing_module *mod;
	struct vregion *vregion;

	if (!dev) {
		comp_cl_err(drv, "failed to allocate memory for comp_dev");
		return NULL;
	}

	/* check for pipeline */
	if (!pipeline || !pipeline->vregion) {
		comp_cl_err(drv, "failed to get pipeline");
		return NULL;
	}

	/* TODO: determine if our user domain is different from the LL pipeline domain */
	if (config->proc_domain == COMP_PROCESSING_DOMAIN_DP) {
		// TODO: get the text, heap, stack and shared sizes from topology too
		/* create a vregion region for all resources */
		size_t interim_size = 0x4000; /* 16kB scratch */
		size_t lifetime_size = 0x20000;  /* 128kB batch */
		size_t shared_size = 0x4000; /* 16kB shared */
		size_t text_size = 0x4000; /* 16kB text */

		vregion = vregion_create(lifetime_size, interim_size, shared_size,
					 0, text_size);
		if (!vregion) {
			//comp_err(dev, "failed to create vregion for DP module");
			goto err;
		}
	} else {
		/* LL modules use pipeline vregion */
		vregion = pipeline->vregion;
	}

	/* allocate module in correct vregion*/
	//TODO: add coherent flag for cross core DP modules
	mod = vregion_alloc(vregion, VREGION_MEM_TYPE_LIFETIME, sizeof(*mod));
	if (!mod) {
		comp_err(dev, "failed to allocate memory for module");
		goto err;
	}

	if (!mod) {
		comp_err(dev, "failed to allocate memory for module");
		goto err;
	}

	dev->ipc_config = *config;
	mod->dev = dev;
	dev->mod = mod;

	/* set virtual region for DP module only otherwise we use pipeline vregion */
	if (config->proc_domain == COMP_PROCESSING_DOMAIN_DP)
		mod->vregion = vregion;

	return mod;

err:
	//module_driver_heap_free(drv->user_heap, dev); // TODO

	return NULL;
}

void module_adapter_mem_free(struct processing_module *mod)
{
	struct vregion *vregion = module_get_vregion(mod);

#if CONFIG_IPC_MAJOR_4
	mod_free(mod, mod->priv.cfg.input_pins);
#endif

	mod_free(mod, mod->priv.cfg.input_pins);
	//mod_free(mod, mod->dev);
	vregion_free(vregion, mod);

	/* free the vregion if its a separate instance from the pipeline */
	if (mod->dev->pipeline->vregion != vregion)
		vregion_destroy(vregion);
}
