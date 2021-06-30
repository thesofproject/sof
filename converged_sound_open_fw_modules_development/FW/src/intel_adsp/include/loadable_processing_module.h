// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
/*! \file loadable_processing_module.h */


#ifndef _LOADABLE_PROCESSING_MODULE_H_
#define _LOADABLE_PROCESSING_MODULE_H_


#include "processing_module_factory.h"


#ifdef __XTENSA__
/*!
 * \def SECTION_ATTRIBUTE(sect_name)
 */
  #define SECTION_ATTRIBUTE(sect_name) __attribute__((section(sect_name)))
#else
/*!
 * \def SECTION_ATTRIBUTE(sect_name)
 * Applies section attribute when compiling with XTENSA toolchain
 * and does nothing otherwise.
 */
  #define SECTION_ATTRIBUTE(sect_name)
#endif

#define MODULE_ENTRYPOINT_SECTION SECTION_ATTRIBUTE(".cmi.text")
#define FIRST_MODULE_SECTION SECTION_ATTRIBUTE(".first")
#define BUILD_INFO_SECTION SECTION_ATTRIBUTE(".buildinfo")

/*!
 * \def STR(s)
 * intermediate macro for Stringification
 * \note internal purpose only. Consider to use XSTR(s) for Stringification.
 */
#define STR(s) #s
/*!
 * \def XSTR(s)
 * Stringification macro
 */
#define XSTR(s) STR(s)

namespace intel_adsp
{
/*! \cond INTERNAL */
namespace internal
{
    extern
    int LoadableModuleMain(
        ProcessingModuleFactoryInterface&,
        void* module_placeholder,
        size_t,
        uint32_t,
        const void*,
        void*,
        void**);
}
/*! \endcond INTERNAL */
}





/*!
 * \brief Type definition of the package entry point.
 * \note internal purpose.
 */
typedef int (ModulePackageEntryPoint)(uint32_t, uint32_t, uint32_t, const void*, void*, void**);


/*! \def MODULE_PACKAGE_ENTRY_POINT_NAME(MODULE)
 * Defines function name for the entry point of a given custom module class.
 * \note internal purpose.
 */
#define MODULE_PACKAGE_ENTRY_POINT_NAME(MODULE) \
    MODULE ## PackageEntryPoint

/*! \def MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE)
 * Defines variable name for the build info of a given custom module class.
 * \note internal purpose.
 */
#define MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE) \
    MODULE ## BuildInfo

/*! \def MODULE_PLACEHOLDER_NAME(MODULE)
 * Defines variable name for the first instance of module placeholder.
 * \note internal purpose.
 */
#define MODULE_PLACEHOLDER_NAME(MODULE) \
    MODULE ## Placeholder

/*! \def MODULE_PLACEHOLDER_LENGTH_NAME(MODULE)
 * Defines variable name for module placeholder length
 * \note internal purpose.
 */
#define MODULE_PLACEHOLDER_LENGTH_NAME(MODULE) \
    MODULE ## PlaceholderLength

/*! \def DECLARE_MODULE_PACKAGE_ENTRY_POINT(MODULE)
 * package entry point declaration.
 * \note internal purpose.
 */
#define DECLARE_MODULE_PACKAGE_ENTRY_POINT(MODULE) \
    extern "C" ModulePackageEntryPoint MODULE_PACKAGE_ENTRY_POINT_NAME(MODULE) MODULE_ENTRYPOINT_SECTION;

/*! \def DECLARE_MODULE_PLACEHOLDER_NAME(MODULE)
 * package entry point declaration.
 * \note internal purpose.
 */
#define DECLARE_MODULE_PLACEHOLDER_NAME(MODULE) \
    enum{ MODULE_PLACEHOLDER_LENGTH_NAME(MODULE) = ((sizeof(MODULE)+MODULE_INSTANCE_ALIGNMENT-1) & (~(MODULE_INSTANCE_ALIGNMENT-1))) }; \
    extern "C" \
	{ intptr_t MODULE_PLACEHOLDER_NAME(MODULE)[(MODULE_PLACEHOLDER_LENGTH_NAME(MODULE)+sizeof(intptr_t)-1)/sizeof(intptr_t)] FIRST_MODULE_SECTION; }

/*! \def DEFINE_MODULE_PACKAGE_ENTRY_POINT(MODULE, MODULE_FACTORY)
 * package entry point definition.
 * \note internal purpose.
 */
#define DEFINE_MODULE_PACKAGE_ENTRY_POINT(MODULE, MODULE_FACTORY) \
    volatile AdspBuildInfo MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE) BUILD_INFO_SECTION = \
    { \
        ADSP_BUILD_INFO_FORMAT, \
        {(   ((0x3FF&MAJOR_IADSP_API_VERSION)<<20) | \
            ((0x3FF&MIDDLE_IADSP_API_VERSION)<<10) | \
            ((0x3FF&MINOR_IADSP_API_VERSION)<<0) )} \
    }; \
    int MODULE_PACKAGE_ENTRY_POINT_NAME(MODULE)(uint32_t, uint32_t instance_id, uint32_t c, const void* d, void* e, void** f) \
    { \
        (void) MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE).FORMAT; \
        (void) MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE).API_VERSION_NUMBER.full; \
        MODULE_FACTORY factory(*reinterpret_cast<intel_adsp::SystemAgentInterface*>(*f)); \
        return intel_adsp::internal::LoadableModuleMain( \
            factory, \
            &MODULE_PLACEHOLDER_NAME(MODULE)[(MODULE_PLACEHOLDER_LENGTH_NAME(MODULE)+sizeof(intptr_t)-1)/sizeof(intptr_t)*instance_id], \
            sizeof(MODULE), \
            c, d, e, f); \
    }


/*!
 * \def DECLARE_LOADABLE_MODULE(MODULE, MODULE_FACTORY)
 * \brief Declare a processing module package for the ADSP System.
 *
 * \a MODULE is the custom class implementing ProcessingModuleInterface
 * and \a MODULE_FACTORY is the custom class implementing the ProcessingModuleInterfaceFactory
 *
 * It shall be applied in a C++ source file part of the processing module package
 *
 * \remarks nested classes and namespace-qualified classes are not allowed for \a MODULE and \a MODULE_FACTORY.
 * Please consider working with alias if any.
 * e.g.:
 * \code
 * typedef my_namespace::CustomModule::Factory CustomModuleFactory;
 * namespace my_namespace
 * {
 *     DECLARE_LOADABLE_MODULE(CustomModule, CustomModuleFactory);
 * }
 * \endcode
 *
 * Usage example
 * -------------
 * c.f. amplifier_module.cc
 */
#define DECLARE_LOADABLE_MODULE(MODULE, MODULE_FACTORY) \
    DECLARE_MODULE_PLACEHOLDER_NAME(MODULE) \
    DECLARE_MODULE_PACKAGE_ENTRY_POINT(MODULE) \
    DEFINE_MODULE_PACKAGE_ENTRY_POINT(MODULE, MODULE_FACTORY)


#endif //_LOADABLE_PROCESSING_MODULE_H_

/*!

 \mainpage
 \tableofcontents
 \version $(INTEL_ADSP_API_VERSION)


The Audio DSP (ADSP) embedded in INTEL SoC is dedicated to real-time processing of audio and voice streams.
While the INTEL ADSP system provides some built-in processing modules for usual use-cases, its feature set can also be extended with user-defined module packages.
Such module packages are indifferently named "loadable processing module" or shortly "loadable module" package as they can be dynamically loaded into the ADSP memory based on need. Modules can be grouped and chained one after another into Pipelines.
Modules expose an interface to control:
- Parameters, which really are inner algorithm parameters
- Status, defining the enabled / disabled (pass-through) state of the Module

The picture hereafter shows a dataflow model of an example Pipeline having 2 inputs and 1 output. It is made up of 3 audio processing Modules executed in a row.

\dot
digraph G {
	graph [fontsize=10 fontname="Verdana" compound=true];
		rankdir="LR";
		subgraph cluster0 {
		node [style=solid,color=black,fontsize=10,fontname="Verdana"];
		style=solid;
		color=black;
		AEC -> BF -> NR;
		label = "Pipeline Example";
	}
	"Mic\nStream" -> AEC;
	"Reference\nStream" -> AEC;
	NR -> "Processed Mic\nStream";
	"Mic\nStream" [shape=none,fontsize=10,fontname="Verdana"];
	"Reference\nStream" [shape=none,fontsize=10,fontname="Verdana"];
	AEC [shape=box,fontsize=10,fontname="Verdana"];
	BF [shape=box,fontsize=10,fontname="Verdana"];
	NR [shape=box,fontsize=10,fontname="Verdana"];
	"Processed Mic\nStream" [shape=none,fontsize=10,fontname="Verdana"];
}
\enddot


This following documentation is intended to present the API which will help in developing a loadable module package.

 In order to be handled by the ADSP System a custom processing module package shall meet 4 requirements:
- As a processing module class, it shall implement the ProcessingModuleInterface class. Moreover custom module class shall provide some ModuleHandle object reserved for ADSP System usage.
The intel_adsp::ProcessingModule template class provides some default implementation which help to implement such a custom module.<br>
- As a module factory class, it shall implement the ProcessingModuleFactoryInterface class.
The intel_adsp::ProcessingModuleFactory template class provides some default implementation suitable for factory dedicated to creation of ProcessingModule child class.<br>
- the user-defined ProcessingModuleInterface and ProcessingModuleFactoryInterface class shall be registered will help of the intel_adsp::SystemAgentInterface::Checkin methods.<br>
- At last it shall declare itself as "loadable" with help of the macro \ref DECLARE_LOADABLE_MODULE.<br>



 \section ProcessingModuleInterface_sec The ProcessingModuleInterface class

At heart of the development of a loadable module package is the intel_adsp::ProcessingModuleInterface class which any custom modules shall implement.
The ProcessingModuleInterface defines methods which ADSP System will call to configure and fire the stream processing. <br>

However the ADSP System cannot directly manipulate a ProcessingModuleInterface instance.
Indeed some ModuleHandle and IoPinsInfo objects have to be provided also along with each ProcessingModuleInterface instance.
It has to moreover be registered by calling intel_adsp::SystemAgentInterface::CheckIn()<br>

For convenience the intel_adsp::ProcessingModule class is provided as a partial default implementation of the ProcessingModuleInterface.
In peculiar, intel_adsp::ProcessingModule handles the extra items and operations mentioned just above
which are required for a module to be integrated in the ADSP System.
It should be suitable for usual needs of most modules development when the count of input and output module pins is well known at compile time.<br>
@startuml
interface ProcessingModuleInterface
abstract ProcessingModule<INPUT_COUNT, OUTPUT_COUNT>

ProcessingModule ..|> ProcessingModuleInterface : "partially implements"
@enduml

A custom module which would relied on the ProcessingModule class could simply be set up by at least :
- implementing the intel_adsp::ProcessingModule::Process() operation to apply the custom processing algorithm
- implementing the intel_adsp::ProcessingModule::SetConfiguration() operation to capture configuration message emitted by the host (attached to the ADSP)
- implementing the intel_adsp::ProcessingModule::SetProcessingMode() operation to handle some bypass or normal processing modes.

Optionally following methods can be overrided in ProcessingModule:
- the intel_adsp::ProcessingModule::GetConfiguration() operation to inform the host about the current configuration value.
- the intel_adsp::ProcessingModule::Reset() operation to allow ADSP system to reset the module in a well-known initial state.

\note Calls to the SetConfiguration() and GetConfiguration() methods are performed by the ADSP System in same execution context as Process().
So there is no need for mutual exclusion on class members used inside those methods. <br>

At last, along with the custom ProcessingModuleInterface implementation a module package is expected to provide some input and ouput pins which allows the ADSP System to stream the signal through the module

Those both expectations are actually met by the ProcessingModule class template and reported to system
with help of the intel_adsp::ProcessingModuleFactory. In return the ProcessingModule class template request some
intel_adsp::ProcessingModuleFactory::Create() method to be implemented in the child class
to fully complete the ProcessingModuleInterface implementation.<br>



 \section ProcessingModuleFactoryInterface_sec The ProcessingModuleFactoryInterface class

As part of a custom module package, a process module factory shall be provided to handle creation of the custom processing module components.
Requirements for the factory class is defined by the intel_adsp::ProcessingModuleFactoryInterface class.<br>
At call to its Create() method and upon some parameters given by the ADSP System the factory is intended to provide:
- an initialized user-defined module instance
- the input and output pins associated to the module instance

The intel_adsp::ProcessingModuleFactory class template provides a partial implementation suitable with those requirements
when the custom module class inherits from the \b intel_adsp::ProcessingModule class template.<br>



 \section DECLARE_LOADABLE_MODULE_sec The DECLARE_LOADABLE_MODULE macro

Ultimately a custom module package shall declared itself by applying once the \ref DECLARE_LOADABLE_MODULE macro
in some of its c++ source files (not in header file).
Calling this macro will make the package to comply with the required ABI for its runtime loading by the ADSP System.

 \warning The custom module package is only allowed to allocate \b constant static or global variables.
The only memory areas available for non-constant allocations are the C-stack for local variables
and the placeholder provided through the intel_adsp::ProcessingModuleFactoryInterface::Create() for members of the processing module.


 \section namespace_sec Namespace organization

INTEL ADSP source code is organized into 3 different namespaces:
- intel_adsp, \copybrief intel_adsp \copydetails intel_adsp
- intel_adsp::internal, \copybrief intel_adsp::internal \copydetails intel_adsp::internal
- intel_adsp::example, \copybrief intel_adsp::example \copydetails intel_adsp::example


 \section systemservice_sec System service

The ADSP System exposes some system service in runtime which can be used by the custom module package.
See the page \ref systemservice_page for a descriptive list of service functions available.

 \section ProcessingModule_examples_sec Examples of custom module packages
Some examples of implementation of custom module packages are available below directory intel_adsp/example:
 - intel_adsp::example::AmplifierModule


 \section Sequence_diagram_sec Typical sequence diagram for module components usage
 @startuml
 actor "ADSP System"
 == Initialization ==
 "ADSP System" -> "custom ModuleFactory"  : GetPrerequisites()
 "custom ModuleFactory" --> "ADSP System"
 "ADSP System" -> ProcessingModuleFactory : Create()
 ProcessingModuleFactory -> "custom ModuleFactory" : Create(module_placeholder)
 "custom ModuleFactory" -> "custom ProcessingModule" : constructor()
 "custom ProcessingModule" --> "custom ModuleFactory"
 "custom ModuleFactory" --> "ADSP System"
 == Processing ==
 loop until release of the processing pipeline
   opt Upon function driver request
     "ADSP System" -> "custom ProcessingModule" : SetConfiguration()
     "custom ProcessingModule" --> "ADSP System"
   end
   opt Upon function driver request
     "ADSP System" -> "custom ProcessingModule" : GetConfiguration()
     "custom ProcessingModule" --> "ADSP System"
   end
   opt Upon ADSP System request
     "ADSP System" -> "custom ProcessingModule" : Reset()
     "custom ProcessingModule" --> "ADSP System"
   end
   opt Upon ADSP System request
     "ADSP System" -> "custom ProcessingModule" : SetProcessingMode()
     "custom ProcessingModule" --> "ADSP System"
   end
   "ADSP System" -> "custom ProcessingModule" : Process()
   "custom ProcessingModule" --> "ADSP System"
 end
@enduml
*/

/*!
 \namespace intel_adsp
 \brief Within this namespace is gathered anything which is expected to be of interest in custom module development e.g. the class interfaces to implement, the system service facades.
*/

/*!
 \namespace intel_adsp::example
 \brief This namespace contains some examples of custom module package implementation.
Source code presented is intended to demonstrate the usage of the ADSP System API. Implementation is often intentionally trivial
to keep things as simple (to understand) as possible. The example modules within this namespace may not be suitable for production software
as they miss performance optimization.
*/

/*!
 \namespace intel_adsp::internal
 \brief Anything within this namespace is not intended to be directly used (no call to any class methods, no instance creation) for custom module development.
Elements in this namespace performs the bind between the public ADSP API and the internal system.
*/
