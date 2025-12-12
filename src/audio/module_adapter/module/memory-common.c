// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * \file memory-common.c
 * \brief Generic Codec Memory API common functions
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#include <rtos/symbol.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/lib/fast-get.h>

LOG_MODULE_DECLARE(module_adapter, CONFIG_SOF_LOG_LEVEL);

int module_init(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	const struct module_interface *const interface = dev->drv->adapter_ops;

	comp_dbg(dev, "entry");

#if CONFIG_IPC_MAJOR_3
	if (mod->priv.state == MODULE_INITIALIZED)
		return 0;
	if (mod->priv.state > MODULE_INITIALIZED)
		return -EPERM;
#endif
	if (!interface) {
		comp_err(dev, "module interface not defined");
		return -EIO;
	}

	/* check interface, there must be one and only one of processing procedure */
	if (!interface->init ||
	    (!!interface->process + !!interface->process_audio_stream +
	     !!interface->process_raw_data < 1)) {
		comp_err(dev, "comp is missing mandatory interfaces");
		return -EIO;
	}

	mod_resource_init(mod);
#if CONFIG_MODULE_MEMORY_API_DEBUG && defined(__ZEPHYR__)
	mod->priv.resources.rsrc_mngr = k_current_get();
#endif
	/* Now we can proceed with module specific initialization */
	ret = interface->init(mod);
	if (ret) {
		comp_err(dev, "error %d: module specific init failed", ret);
		mod_free_all(mod);
		return ret;
	}

	comp_dbg(dev, "done");
#if CONFIG_IPC_MAJOR_3
	mod->priv.state = MODULE_INITIALIZED;
#endif

	return 0;
}

#if CONFIG_COMP_BLOB
void mod_data_blob_handler_free(struct processing_module *mod, struct comp_data_blob_handler *dbh)
{
	mod_free(mod, (void *)dbh);
}
EXPORT_SYMBOL(mod_data_blob_handler_free);
#endif

#if CONFIG_FAST_GET
void mod_fast_put(struct processing_module *mod, const void *sram_ptr)
{
	mod_free(mod, sram_ptr);
}
EXPORT_SYMBOL(mod_fast_put);
#endif

