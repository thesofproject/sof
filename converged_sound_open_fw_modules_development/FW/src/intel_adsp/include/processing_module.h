// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
/*! \file processing_module.h */


#ifndef _ADSP_PROCESSING_MODULE_H_
#define _ADSP_PROCESSING_MODULE_H_


#include "processing_module_interface.h"
#include "processing_module_factory_interface.h"
#include "module_handle.h"
#include "module_initial_settings.h"
#include "adsp_stddef.h"

#include <limits.h>


namespace intel_adsp
{
    template<class DERIVED, class PROCESSING_MODULE> class ProcessingModuleFactory;


    /*!
     * \brief The ProcessingModule class provides a partial default implementation of the ProcessingModuleInterface which can serve as base
     * for most of custom modules as long as their counts of input and output pins is well-known at compile time.
     *  \see ProcessingModuleFactory is the associated factory for ProcessingModule instance creation.
     *
     * \note ProcessingModule partially implements the ProcessingModuleInterface so is expected to be specialized (inherited) by a custom module.
     * \tparam INPUT_COUNT      count of input pins for the custom module
     * \tparam OUTPUT_COUNT     count of output pins for the custom module
     * \tparam ADDITIONAL_IO_SIZE total amount of memory reserved for the custom module additional pins
     */
    template<int INPUT_COUNT, int OUTPUT_COUNT, int REF_QUEUES_POOL_SIZE>
    class ProcessingModule: public ProcessingModuleInterface
    {
        template<class, class> friend class ProcessingModuleFactory;

    public:
        /*! \brief this anonymous enum helps to keep track of the actual template parameters.
         */
        enum
        {
            kInputCount = INPUT_COUNT,
            kOutputCount = OUTPUT_COUNT
        };
        /*! \brief Initializes a new instance of ProcessingModule.
         */
        ProcessingModule(
                            SystemAgentInterface& system_agent /*!< [in] the SystemAgentInterface object which can register the instance which is being initialized
                                                                          * \remarks the system_agent is a temporary object and cannot be stored.
                                                                          */
                        )   :
                                system_service_(system_agent.GetSystemService())
        { system_agent.CheckIn(*this, module_handle_, log_handle_); }

        /*!
         * \brief Gets the system service object.
         */
        SystemService const& GetSystemService() const
        { return system_service_; }

        /*!
         * \brief Gets the LogHandle required to send some log message
         */
        LogHandle const& GetLogHandle() const
        { return *log_handle_; }

    protected:
        /*!
         * \brief Gets the IoPinsInfo data which the ADSP System requires to drive streams through the module.
         */
        void GetPinsInfo(IoPinsInfo& pins_info)
        {
            pins_info.sources = sources_;
            pins_info.sinks = sinks_;
            pins_info.pins_mem_pool = pins_mem_pool_;
            pins_info.pins_mem_pool_size = sizeof(pins_mem_pool_);
        }

    private:
        DCACHE_ALIGNED uint8_t pins_mem_pool_
        [
            OUTPUT_COUNT == 0 ? REF_QUEUES_POOL_SIZE + HUNGRY_RT_SINK_SIZE : REF_QUEUES_POOL_SIZE
        ];
        PinEndpoint sources_[INPUT_COUNT];
        PinEndpoint sinks_[OUTPUT_COUNT == 0 ? 1 : OUTPUT_COUNT];
        SystemService const& system_service_;
        ModuleHandle module_handle_;
        LogHandle* log_handle_;

    }; //class ProcessingModule
    /*! \class ProcessingModule
     * Examples of custom modules implementation based on ProcessingModule
     * -------------------------------------------------------------------
     * - A simple one-input-one-output module which multiplies the input stream by a constant gain value : example::AmplifierModule in files \ref amplifier_module.h and \ref amplifier_module.cc
     */
}


#endif //_ADSP_PROCESSING_MODULE_H_
