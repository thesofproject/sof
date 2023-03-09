/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 */

#ifndef __SOF_AUDIO_IADK_MODULES__
#define __SOF_AUDIO_IADK_MODULES__

#include <iadk_module_adapter.h>

/* Intel module adapter is an extension to SOF module adapter component that allows to integrate
 * modules developed under IADK (Intel Audio Development Kit)
 * and LMDK (Loadable Modules Dev Kit) Framework. Modules uses uniform
 * set of interfaces and are linked into separate library. These modules are loaded in runtime
 * through library_manager and then after registration into SOF component infrastructure are
 * interfaced through module adapter API.
 *
 * Since IADK modules uses ProcessingModuleInterface to control/data transfer
 * and adsp_system_service to use base FW services from internal module code,
 * there is a communication shim layer defined in intel directory.
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
 * Module recognision is done by checking module API version defined in module_api_ver.h
 * with version read from elf file.
 */


struct comp_dev *iadk_modules_shim_new(const struct comp_driver *drv,
				       const struct comp_ipc_config *config,
				       const void *spec);

#define DECLARE_DYNAMIC_MODULE_ADAPTER(comp_dynamic_module, mtype, uuid, tr) \
do { \
	(comp_dynamic_module)->type = mtype; \
	(comp_dynamic_module)->uid = SOF_RT_UUID(uuid); \
	(comp_dynamic_module)->tctx = &(tr); \
	(comp_dynamic_module)->ops.create = iadk_modules_shim_new; \
	(comp_dynamic_module)->ops.prepare = module_adapter_prepare; \
	(comp_dynamic_module)->ops.params = module_adapter_params; \
	(comp_dynamic_module)->ops.copy = module_adapter_copy; \
	(comp_dynamic_module)->ops.cmd = module_adapter_cmd; \
	(comp_dynamic_module)->ops.trigger = module_adapter_trigger; \
	(comp_dynamic_module)->ops.reset = module_adapter_reset; \
	(comp_dynamic_module)->ops.free = module_adapter_free; \
	(comp_dynamic_module)->ops.set_large_config = module_set_large_config;\
	(comp_dynamic_module)->ops.get_large_config = module_get_large_config;\
	(comp_dynamic_module)->ops.get_attribute = module_adapter_get_attribute; \
} while (0)

#endif /* __SOF_AUDIO_IADK_MODULES__ */
