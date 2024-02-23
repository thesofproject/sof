/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 */

#ifndef __SOF_AUDIO_MODULES__
#define __SOF_AUDIO_MODULES__

#include <sof/audio/module_adapter/module/generic.h>
#include <iadk_module_adapter.h>

/* Intel module adapter is an extension to SOF module adapter component that allows to integrate
 * modules developed under IADK (Intel Audio Development Kit)
 * and LMDK (Loadable Modules Dev Kit) Framework. Modules uses uniform
 * set of interfaces and are linked into separate library. These modules are loaded in runtime
 * through library_manager and then after registration into SOF component infrastructure are
 * interfaced through module adapter API.
 * Since IADK modules uses ProcessingModuleInterface to control/data transfer and AdspSystemService
 * to use base FW services from internal module code, there is a communication shim layer defined
 * in intel directory.
 *
 * Since ProcessingModuleInterface consists of virtual functions, there are C++ -> C wrappers
 * defined to access the interface calls from SOF code.
 *
 * The main assumption here was to load IADK Modules without any code modifications. Therefore C++
 * function, structures and variables definition are here kept with original form from
 * IADK Framework. This provides binary compatibility for already developed 3rd party modules.
 *
 * There are three entities in intel module adapter package:
 *  - System Agent - A mediator to allow the custom module to interact with the base SOF FW.
 *                   It calls IADK module entry point and provides all necessary information to
 *                   connect both sides of ProcessingModuleInterface and System Service.
 *  - System Service - exposes of SOF base FW services to the module.
 *  - Processing Module Adapter - SOF base FW side of ProcessingModuleInterface API
 *
 * Using the same philosofy loadable modules are using module adapter to interact with SOF FW.
 * Module recognision is done by checking module API version defined in module/module/api_ver.h
 * with version read from elf file.
 */

/* Processing Module Adapter API */
extern const struct module_interface processing_module_adapter_interface;

static inline void declare_dynamic_module_adapter(struct comp_driver *drv,
						  enum sof_comp_type mtype,
						  const struct sof_uuid *uuid,
						  struct tr_ctx *tr)
{
	drv->type = mtype;
	drv->uid = uuid;
	drv->tctx = tr;
	drv->ops.create = module_adapter_new;
	drv->ops.prepare = module_adapter_prepare;
	drv->ops.params = module_adapter_params;
	drv->ops.copy = module_adapter_copy;
#if CONFIG_IPC_MAJOR_3
	drv->ops.cmd = module_adapter_cmd;
#endif
	drv->ops.trigger = module_adapter_trigger;
	drv->ops.reset = module_adapter_reset;
	drv->ops.free = module_adapter_free;
	drv->ops.set_large_config = module_set_large_config;
	drv->ops.get_large_config = module_get_large_config;
	drv->ops.get_attribute = module_adapter_get_attribute;
	drv->ops.bind = module_adapter_bind;
	drv->ops.unbind = module_adapter_unbind;
	drv->ops.get_total_data_processed = module_adapter_get_total_data_processed;
	drv->ops.dai_get_hw_params = module_adapter_get_hw_params;
	drv->ops.position = module_adapter_position;
	drv->ops.dai_ts_config = module_adapter_ts_config_op;
	drv->ops.dai_ts_start = module_adapter_ts_start_op;
	drv->ops.dai_ts_stop = module_adapter_ts_stop_op;
	drv->ops.dai_ts_get = module_adapter_ts_get_op;
	drv->adapter_ops = &processing_module_adapter_interface;
}

#endif /* __SOF_AUDIO_MODULES__ */
