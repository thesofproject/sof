/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef _SYSTEM_AGENT_H
#define _SYSTEM_AGENT_H

#ifdef __cplusplus

#include <processing_module_factory_interface.h>
#include <system_service.h>

namespace intel_adsp
{
namespace system
{
	/*! \brief The SystemAgent is a mediator to allow the custom module to interact
	 *  with the base FW
	 *
	 * A SystemAgent can only be delivered by the ADSP System.
	 * Once registered, a ModuleHandle instance can be handled by the ADSP System.
	 */
	class SystemAgent : public intel_adsp::SystemAgentInterface
	{
	public:
		SystemAgent(uint32_t module_id,
			    uint32_t instance_id,
			    uint32_t core_id,
			    uint32_t log_handle);

		/*! \brief Initializes a new instance of ModuleAdapter in the ModuleHandle buffer*/
		virtual void CheckIn(intel_adsp::ProcessingModuleInterface & processing_module,
				     intel_adsp::ModuleHandle & module_handle,
				     intel_adsp::LogHandle * &log_handle) /*override*/;

		/*! \return a value part of error code list defined within the adsp_error.h*/
		virtual int CheckIn(intel_adsp::ProcessingModuleFactoryInterface & module_factory,
				    intel_adsp::ModulePlaceholder * module_placeholder,
				    size_t processing_module_size,
				    uint32_t core_id,
				    const void *obfuscated_mod_cfg,
				    void *obfuscated_parent_ppl,
				    void **obfuscated_modinst_p) /*override*/;

		virtual intel_adsp::SystemService const &GetSystemService() /*override*/
		{
			return
			    reinterpret_cast<intel_adsp::SystemService const &>(system_service_);
		}

		virtual intel_adsp::LogHandle const &GetLogHandle() /*override*/
		{
			return *reinterpret_cast<intel_adsp::LogHandle const *>(&log_handle_);
		}

	private:
		static const AdspSystemService system_service_;
		uint32_t log_handle_;
		uint32_t const core_id_;
		uint32_t module_id_;
		uint32_t instance_id_;
		uint32_t module_size_;
		intel_adsp::ModuleHandle * module_handle_;

	}; /* class SystemAgent */
} /* namespace system */
} /* namespace intel_adsp */

#endif /* #ifdef __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif
/*
 * The process of loading a IADK module is quite complicated. The function call stack is as follows:
 *
 *   1. IADK module adapter initialization function (modules_init) pass module entry point to the
 *	system_agent_start function.
 *
 *   2. system_agent_start: This function creates an instance of the SystemAgent class on the stack
 *	and then calls the module entry point function (ModuleEntryPoint) passing a pointer to the
 *	SystemAgent object as a parameter.
 *
 *   3. ModuleEntryPoint(system_agent): Creates a ModuleFactory object of the module on the stack
 *	(inheriting from ProcessingModuleFactoryInterface) and then calls the
 *	LoadableModuleMain(system_agent, module_factory, placeholder) function, which is the default
 *	entry point for all IADK modules. Placeholder is part of the .bss section intended for
 *	a given module instance, with its address is determined based on the instance ID.
 *
 *   4. LoadableModuleMain(system_agent, module_factory, placeholder): Calls the CheckIn method
 *	(variant with 7 parameters) of the SystemAgent class.
 *
 *   5. SystemAgent.CheckIn(7): Calls the Create method from the ModuleFactory object.
 *
 *   6. ModuleFactory.Create(system_agent, placeholder): Creates an instance of the module class
 *	(inheriting from ProcessingModule) at the address pointed to by placeholder.
 *	The ProcessingModule class contains a module_handle field, which reserves an area in memory
 *	of size IADK_MODULE_PASS_BUFFER_SIZE bytes. The constructor of this class calls the CheckIn
 *	method (the variant with 3 parameters) from the SystemAgent class, passing a pointer to self
 *	and a pointer to module_handle.
 *
 *   7. SystemAgent.CheckIn(3): Stores the address of module_handle in a private variable and
 *	creates an instance of the IadkModuleAdapter class in this location.
 *
 * The saved address of the module_handle (the adapter's IADK instance) is returned in the CheckIn
 * method (the variant with 7 parameters) via a parameter that initially contained the address to
 * the agent system. The system_agent_start function returns it in the variable adapter.
 */
int system_agent_start(uintptr_t entry_point, uint32_t module_id, uint32_t instance_id,
		       uint32_t core_id, uint32_t log_handle, void *mod_cfg, void **adapter);
#ifdef __cplusplus
}
#endif

#endif /* _SYSTEM_AGENT_H */
