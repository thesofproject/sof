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
		static adsp_system_service system_service_;
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
void *system_agent_start(uint32_t entry_point, uint32_t module_id, uint32_t instance_id,
			 uint32_t core_id, uint32_t log_handle, void *mod_cfg);
#ifdef __cplusplus
}
#endif

#endif /* _SYSTEM_AGENT_H */
