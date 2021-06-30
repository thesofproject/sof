// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
/*! \file processing_module_factory.h */


#ifndef _PROCESSING_MODULE_FACTORY_H_
#define _PROCESSING_MODULE_FACTORY_H_


#include "processing_module_factory_interface.h"
#include "processing_module.h"


namespace intel_adsp
{
    /*!
     * \brief The ProcessingModuleFactory provides default partial implementation for the ProcessingModuleFactoryInterface class
     * when PROCESSING_MODULE inherits from ProcessingModule.
     *
     * \remarks ProcessingModuleFactory provides static polymorphism by implementing the Curiously Recurring Template Pattern (CRTP).
     * \note ProcessingModuleFactory partially implements the ProcessingModuleFactoryInterface so is expected to be specialized (inherited) by a custom module factory.
     * \tparam DERIVED is the child class implementing the ProcessingModuleFactory::Create()
     * \tparam PROCESSING_MODULE is a custom module which is based on the ProcessingModule class that this ProcessingModuleFactory child will create
     */
    template<class DERIVED, class PROCESSING_MODULE>
    class ProcessingModuleFactory : public ProcessingModuleFactoryInterface
    {
    public:
        /*!
         * \brief this typedef helps to keep track of the PROCESSING_MODULE class passed as template parameter */
        typedef PROCESSING_MODULE Module;

    public:
        /*! \brief Initializes a new instance of ProcessingModuleFactory*/
        ProcessingModuleFactory(
            SystemAgentInterface& system_agent /*!< the system_agent provided by the ADSP System */
        )
            :
            system_service_(system_agent.GetSystemService()),
            log_handle_(system_agent.GetLogHandle())
        {}

        /*!
         * \brief This default implementation reports no special capability or requirement.
         *
         * It initializes the output ProcessingModulePrerequisites parameter with its default values.
         * \param [out] module_prerequisites reports module preprequisites that ADSP System needs to prepare the module creation.
         */
        virtual void GetPrerequisites(
                                ProcessingModulePrerequisites& module_prerequisites
                            ) /*override*/
        {
            module_prerequisites = ProcessingModulePrerequisites();
            module_prerequisites.input_pins_count = Module::kInputCount;
            module_prerequisites.output_pins_count = Module::kOutputCount;
        }

        /*! \brief This default implementation takes care of reporting the right pins_info to the ADSP System.
         */
        virtual ErrorCode::Type Create(
                                SystemAgentInterface& system_agent,
                                ModulePlaceholder* module_placeholder,
                                ModuleInitialSettings initial_settings,
                                IoPinsInfo& pins_info
                            ) /* override */
        {
            ErrorCode::Type ec = Create(system_agent, module_placeholder, initial_settings);

            reinterpret_cast<Module*&>(module_placeholder)->GetPinsInfo(pins_info);

            return ec;
        }
        /*!
         * \brief Creates a PROCESSING_MODULE instance.
         *
         * This method is provided for convenience and shall be implemented by the DERIVED class (static polymorphism).
         *
         * \param [in] system_agent the SystemAgentInterface object which can register the module instance which is being initialized
         * \param [in] module_placeholder the pointer to the memory location where the module instance can be initialized using the "new placement syntax".
         * Note that the size of the placeholder given by the System is worth
         * the size of the processing module class given as parameter of the \ref DECLARE_LOADABLE_MODULE macro
         * \param [in] initial_settings  initial settings for module startup.
         *
         * \see example::AmplifierModuleFactory::Create() for an example of implementation
         */
        ErrorCode::Type Create(
            SystemAgentInterface& system_agent,
            ModulePlaceholder* module_placeholder,
            ModuleInitialSettings initial_settings
                            )
        {
            return static_cast<DERIVED* const>(this)->Create(system_agent, module_placeholder, initial_settings);
        }

        /*!
         * \brief Gets the system service object.
         */
        SystemService const& GetSystemService() const
        { return system_service_; }

        /*!
         * \brief Gets the LogHandle required to send some log message
         */
        LogHandle const& GetLogHandle() const
        { return log_handle_; }

    private:
        SystemService const& system_service_;
        LogHandle const& log_handle_;
    }; //class ProcessingModuleFactory
} //namespace intel_adsp


#endif //_PROCESSING_MODULE_FACTORY_H_
