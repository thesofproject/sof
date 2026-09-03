/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */
/*! \file processing_module_factory_interface.h */

#ifndef _PROCESSING_MODULE_FACTORY_INTERFACE_H_
#define _PROCESSING_MODULE_FACTORY_INTERFACE_H_

#include "processing_module_interface.h"
#include "system_agent_interface.h"
#include "processing_module_prerequisites.h"
#include "module_initial_settings.h"

namespace intel_adsp
{
	/*! \brief defines type of the pin endpoint.
	 *
	 * A custom module is required to provide to the ADSP System some PinEndpoint value arrays.
	 * Arrays length shall be as long as it has input and output pins.
	 * (ref \ref ProcessingModuleFactoryInterface::Create()).
	 */
	typedef void *PinEndpoint;
	typedef struct { void *prt[2]; } FwdEvent;

	/*! \brief holds information about pins of a module.
	 *
	 * For each custom module,
	 * input pins of a module are associated to some "sources" PinEndPoint and
	 * output pins are associated to some sinks "PinEndPoint" objects.
	 * Those sinks and sources objects shall be instantiated by the custom module and delivered
	 * to the ADSP System with this IoPinsInfo structure
	 * (through \ref ProcessingModuleFactoryInterface::Create()).
	 *
	 * \note The "pin" of a module is purely conceptual and has no programmatic correspondence.
	 * A module has as many input/output pins as input/output streams which can be
	 * driven through it.
	 */
	struct IoPinsInfo {
		/*!
		 * \brief pointer on a PinEndpoint array with "sources_count" elements
		 *
		 * A module is required to provide some PinEndpoint arrays to allow
		 * the ADSP System to drive streams into the module.
		 */
		PinEndpoint *sources;
		/*!
		 * \brief pointer on a PinEndpoint array with "sinks_count" elements
		 *
		 * A module is required to provide some PinEndpoint arrays to allow
		 * the ADSP System to drive stream out of the module.
		 */
		PinEndpoint *sinks;
		/*!
		 * \brief pointer on a FwdEvents array with "events_count" elements
		 *
		 * A module is required to provide some FwdEvents arrays to allow
		 * the ADSP System to handle key phrase detection.
		 */
		FwdEvent *events;

		/*! \brief description of buffer reserved for DP queue objects and buffers
		 * used for all additional input and output pins (e.g. reference pin)
		 */
		uint8_t *pins_mem_pool;
		size_t pins_mem_pool_size;
	};

	/*!
	 * \brief The ProcessingModuleFactoryInterface class defines requirements for creating
	 * a processing module controllable by the ADSP System.
	 */
	class ProcessingModuleFactoryInterface
	{
	public:
		/*!
		 * \brief Scoped enumeration of error code value which can be reported by
		 * a ProcessingModuleFactoryInterface object
		 */
		struct ErrorCode : intel_adsp::ErrorCode {
			/*! \brief list of named error codes specific to
			 * the ProcessingModuleFactoryInterface
			 */
			enum Enum {
				/*!< Reports that the given value of Input Buffer Size is invalid */
				INVALID_IBS = intel_adsp::ErrorCode::MaxValue + 1,
				/*!< Reports that the given value of Output Buffer Size is invalid*/
				INVALID_OBS,
				/*!< Reports that the given value of Cycles Per Chunk processing
				 * is invalid
				 */
				INVALID_CPC,
				/*!< Reports that the settings provided for module creation
				 * are invalid
				 */
				INVALID_SETTINGS
			};
			/*! \brief Indicates the minimal value of the enumeration */
			static Enum const MinValue = INVALID_IBS;
			/*! \brief Indicates the maximal value of the enumeration */
			static Enum const MaxValue = INVALID_SETTINGS;

			/*!
			 * \brief Initializes a new instance of ErrorCode given a value
			 */
			explicit ErrorCode(Type value)
				:   intel_adsp::ErrorCode(value)
			{}
		};

		/*!
		 * \brief Indicates the prerequisites for module instance creation.
		 *
		 * ADSP System calls this method before each module instance creation.
		 * \param [out] module_prereqs reports module prerequisites
		 *              that ADSP System needs to prepare the module creation.
		 */
		virtual void GetPrerequisites(ProcessingModulePrerequisites & module_prereqs
				) = 0;
		/*!
		 * \brief Creates a ProcessingModuleInstance instance in the given placeholder.
		 *
		 * The custom implementation of the Create method is expected to handle
		 * initialization of the custom module instances.
		 * \note The ADSP System will provide a dedicated memory \e placeholder for every
		 *       module instance to create.
		 *
		 * \param [in] system_agent the SystemAgentInterface object which can register the
		 *             module instance which is being initialized
		 * \param [in] module_placeholder the pointer to the memory location where the
		 *             module instance can be initialized using the "new placement syntax".
		 * Note that the size of the placeholder given by the System is worth
		 * the size of the processing module class given as parameter of the
		 * DECLARE_LOADABLE_MODULE macro
		 * \param [in] initial_settings  initial settings for module startup.
		 * \param [out] pins_info will report the IoPinsInfo data that ADSP System
		 *              requires to bind the input and output streams to the module.
		 * \return some ErrorCode value upon creation status.
		 */
		virtual ErrorCode::Type Create(SystemAgentInterface & system_agent,
				ModulePlaceholder * module_placeholder,
				ModuleInitialSettings initial_settings,
				IoPinsInfo & pins_info
			) = 0;
	}; /* class ProcessingModuleFactoryInterface */

} /* namespace intel_adsp */

#endif /* #ifndef _PROCESSING_MODULE_FACTORY_INTERFACE_H_ */
