// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
 /*! file loadable_module_entrypoint.cc
  * The source code of the loadable module entry point is provided for building convenience.
  * However it is not expected to be modified as its content is tightly tied to the ADSP System.
  * */

#include "processing_module_factory_interface.h"
#include "system_agent_interface.h"
#include "core/kernel/logger/log.h"

namespace intel_adsp
{
namespace internal
{

/*! \internal */
int LoadableModuleMain(ProcessingModuleFactoryInterface& factory,
                       void* module_placeholder,
                       size_t ms,
                       uint32_t ci,
                       const void* sb,
                       void* ppl,
                       void** system_agent_p)
{
    SystemAgentInterface& system_agent = *reinterpret_cast<SystemAgentInterface*>(*system_agent_p);
    ModulePlaceholder *placeholder = reinterpret_cast<ModulePlaceholder*>(module_placeholder);

    return system_agent.CheckIn(factory, placeholder, ms, ci, sb, ppl, system_agent_p);
}


} //namespace internal
} //namespace intel_adsp

extern "C" void __cxa_pure_virtual()  __attribute__((weak));

void __cxa_pure_virtual()
{
    while(1);
}
