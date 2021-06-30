// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
/*! \file module_handle.h */


#ifndef _ADSP_MODULE_HANDLE_H_
#define _ADSP_MODULE_HANDLE_H_


/*! \def MODULE_PASS_BUFFER_SIZE
 * \internal
 * \brief This macro is defined by the build infrastructure and its value may depend on the API version.
 */
#ifndef MODULE_PASS_BUFFER_SIZE
 #error "MODULE_PASS_BUFFER_SIZE definition is missing"
#endif


#include "system_agent_interface.h"

#include <stddef.h>


/*!
 * \brief The AdspModuleHandle allows a ProcessingModuleInterface object to be handled by the ADSP System.
 *
 * It contains actually some buffer which the module shall provide to the ADSP System.
 *
 * \remarks The size of the buffer may depend on the API version exposed by the ADSP System. So a processing module package needs to be recompiled
 * when major or middle number of the System API version is changed (ADSP API version has format [major number].[middle number].[minor number])
 */
typedef struct AdspModuleHandle
{
    enum {kPassBufferLength = (MODULE_PASS_BUFFER_SIZE+sizeof(intptr_t)-1)/sizeof(intptr_t) /*!< actual size of the buffer reserved */ };
    intptr_t buffer[kPassBufferLength];    /*!< some buffer reserved for ADSP System usage */
} AdspModuleHandle;


#ifdef __cplusplus
namespace intel_adsp
{
// Note that doxygen weirdly fails to generate documentation if ModuleHandle type is defined as a "struct"
/*! \brief Alias for AdspModuleHandle type intended to be used in C++.
 */
class ModuleHandle : public AdspModuleHandle{};
}
#endif //__cplusplus


/*! \brief AdspLogHandle identifies the log message sender.
 *
 * AdspLogHandle instance is passed to the SystemService::LogMessage function.
 * This struct should not be used directly.
 */
typedef struct AdspLogHandle {} AdspLogHandle;


#ifdef __cplusplus
namespace intel_adsp
{
/*! \brief alias for AdspLogHandle type in C++
 */
struct LogHandle : public AdspLogHandle {};
}
#endif



#endif //_ADSP_MODULE_HANDLE_H_
