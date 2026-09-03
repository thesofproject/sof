/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */
/*! \internal \file system_agent_interface.h */

#ifndef _ADSP_SYSTEM_AGENT_INTERFACE_H_
#define _ADSP_SYSTEM_AGENT_INTERFACE_H_

#include "system_service.h"
#include "system_error.h"

#include <stdint.h>
#include <stddef.h>

namespace intel_adsp
{
	struct ModulePlaceholder;
	class ProcessingModuleFactoryInterface;
	class ProcessingModuleInterface;
	class DetectorModuleInterface;
	class ModuleInitialSettings;
	struct ProcessingModulePrerequisites;
	struct IoPinsInfo;
	class ModuleHandle;
	struct LogHandle;

namespace internal
{
	class Endpoint;
} /* namespace internal */

	/*! \brief The SystemAgentInterface is a mediator to allow loadable module to interact
	 *  with the ADSP System.
	 *
	 * It allows loadable module and factory to register themselves
	 * and provide the list of the service functions exposed by the ADSP System.
	 * \note user-defined code should not directly interact with the SystemAgentInterface
	 * and rather should take leverage of the ProcessingModule and ProcessingModuleInterface
	 * base classes.
	 */
	class SystemAgentInterface
	{
	public:
		/*!
		 * \brief Scoped enumeration of error code value which can be reported by
		 * a SystemAgentInterface object
		 */
		struct ErrorCode : intel_adsp::ErrorCode
		{
			/*! \brief list of named error codes specific to the SystemAgentInterface */
			enum Enum {
				/*!< Reports that ProcessingModuleFactoryInterface::Create()
				 * has exited with error
				 */
				MODULE_CREATION_FAILURE = intel_adsp::ErrorCode::MaxValue + 1
			};
			/*! \brief Indicates the minimal value of the enumeration */
			static Enum const MinValue = MODULE_CREATION_FAILURE;
			/*! \brief Indicates the maximal value of the enumeration */
			static Enum const MaxValue = MODULE_CREATION_FAILURE;

			/*!
			 * \brief Initializes a new instance of ErrorCode given a value
			 */
			explicit ErrorCode(Type value)
				:   intel_adsp::ErrorCode(value)
			{}
		};

		/*! \brief Allows a ProcessingModuleInterface instance to be registered in
		 * the ADSP System
		 *
		 * internal purpose.
		 */
		virtual void CheckIn(ProcessingModuleInterface & processing_module,
				     /**< the instance to register for later use in processing
				      * pipeline
				      */
				     ModuleHandle & module_handle,
				     /**< the object that is required by the ADSP System to handle
				      * the module
				      */
				     LogHandle * &log_handle /**< module logging context */
				    ) = 0;

		/*! \brief Allows a ProcessingModuleFactoryInterface instance to be registered in
		 * the ADSP System
		 *
		 * internal purpose.
		 */
		virtual int CheckIn(ProcessingModuleFactoryInterface & module_factory,
				    /*!< the instance to register */
				    ModulePlaceholder * module_placeholder,
				    /*!< the place holder in memory for instantiation of
				     * a ProcessingModuleInterface instance
				     */
				    size_t,
				    uint32_t,
				    const void*,
				    void*,
				    void**) = 0;

		/*!
		 * \brief Gets the SystemService instance which contains all the service functions
		 */
		virtual SystemService const &GetSystemService(void) = 0;

		/*!
		 * \brief Gets the LogHandle required to send some log message
		 */
		virtual LogHandle const &GetLogHandle(void) = 0;
	};

	class SystemAgentInterface2 : public SystemAgentInterface
	{
	public:

		/*! \brief Allows a ProcessingModuleInterface instance to be registered in
		 * the ADSP System as detector module
		 *
		 * internal purpose.
		 */
		virtual void CheckInDetector(DetectorModuleInterface & processing_module,
					     /*!< the instance to register for later use in
					      * processing pipeline
					      */
					     ModuleHandle & module_handle,
					     /*!< the object that is required by the ADSP System
					      *  to handle the module
					      */
					     LogHandle * &log_handle
					     /*!< module logging context */
					    ) = 0;
	};

} /* namespace intel_adsp */

#endif /* _ADSP_SYSTEM_AGENT_H_ */
