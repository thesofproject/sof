/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 *
 * \file generic.h
 * \brief Generic Codec API header file
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#ifndef __SOF_AUDIO_CODEC_GENERIC__
#define __SOF_AUDIO_CODEC_GENERIC__

#include <sof/audio/component.h>
#include <sof/ut.h>
#include <sof/lib/memory.h>

#define comp_get_codec(d) (&(((struct comp_data *)((d)->priv_data))->codec))
#define CODEC_GET_INTERFACE_ID(id) ((id) >> 0x8)
#define CODEC_GET_API_ID(id) ((id) & 0xFF)
#define API_CALL(cd, cmd, sub_cmd, value, ret) \
	do { \
		ret = (cd)->api((cd)->self, \
				(cmd), \
				(sub_cmd), \
				(value)); \
	} while (0)

#define DECLARE_CODEC_ADAPTER(adapter, uuid, tr) \
static struct comp_dev *adapter_shim_new(const struct comp_driver *drv, \
					 struct comp_ipc_config *config, \
					 void *spec) \
{ \
	return codec_adapter_new(drv, config, &(adapter), spec);\
} \
\
static const struct comp_driver comp_codec_adapter = { \
	.type = SOF_COMP_CODEC_ADAPTOR, \
	.uid = SOF_RT_UUID(uuid), \
	.tctx = &(tr), \
	.ops = { \
		.create = adapter_shim_new, \
		.prepare = codec_adapter_prepare, \
		.params = codec_adapter_params, \
		.copy = codec_adapter_copy, \
		.cmd = codec_adapter_cmd, \
		.trigger = codec_adapter_trigger, \
		.reset = codec_adapter_reset, \
		.free = codec_adapter_free, \
	}, \
}; \
\
static SHARED_DATA struct comp_driver_info comp_codec_adapter_info = { \
	.drv = &comp_codec_adapter, \
}; \
\
UT_STATIC void sys_comp_codec_##adapter_init(void) \
{ \
	comp_register(platform_shared_get(&comp_codec_adapter_info, \
					  sizeof(comp_codec_adapter_info))); \
} \
\
DECLARE_MODULE(sys_comp_codec_##adapter_init)

/*
 * NOTES:
 *
 * Architecture - split into high level modules (aka codec_apapter) and
 * low level component (aka component.h)
 *
 * 1) high level - rename this to module.h, and align to module API, does
 *                 all processing. Simple API for integrators.
 *
 * 2) low level - interfaces to pipeline, does all cache mgmt, routing, and
 *                state mgmt.
 *
 * Need https://github.com/thesofproject/sof/pull/4192 for memory API.
 */

/*! MODULE API USAGE INFO - TODO: TO BE UPDATED FOR C VERSION

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

/*
 * TODO: C API life cycle flow should be
 *
 * 1) init() - mandatory
 * 2) reset() - mandatory (although not really needed after init - free all runtime resources)
 * 3) set_configuartion() - mandatory
 * 4) get_configuration() - optional
 * 5) prepare() - mandatory (allocate any resources and prepare to process)
 * 6) process() - mandatory
 * 7) goto 6 or goto 2 (depends on host use case)
 * 8) reset()
 * 9) delete()
 *
 */

/* START OF MODULE API IMPORTS */
/*
 * TODO:
 * 1) Code style updates
 * 2) Use namespace prefixes
 * 3) refactor for C API but keep in mind wrapping for C++ API.
 */

/*! \brief enumeration values of processing mode */
enum ProcessingModeEnum
{
	NORMAL = 0, /*!< Indicates that module is expected to apply its custom processing on signal. */
	BYPASS /*!< Indicates that module is expected to not apply its custom processing on signal.
	   * The module is expected to forward as far as possible the input signals unmodified
	   * with repect of the signal continuity at the mode transition.
	   */
};

/*! \brief Scoped enumeration which defines processing mode values
 * \see ProcessingModuleInterface::SetProcessingMode()
 */
struct ProcessingMode
{
	enum ProcessingModeEnum value_;
};

/*! \brief defines the bitfield structure of flags associated to an InputStreamBuffer
    */
struct InputStreamFlags
{
	bool end_of_stream : 1; /*!< indicates that End Of Stream condition has occured on the input stream */
};

/*! \brief Descriptor of the data stream buffer extracted from a input module pin
 * \see ProcessingModuleInterface::Process()
 */
struct InputStreamBuffer
{
	uint8_t *const data; /*!< data stream buffer */
	/*!
	* \brief size indicator about the data in the stream buffer
	*
	* - When read, it indicates the size of available data in the data stream buffer
	* - When written, it reports the size of data which has actually be considered during the buffer processing
	*   (can be less than the given available data size)
	*/
	size_t size;
	const struct InputStreamFlags flags; /*!< readonly status flags about the input stream */
};

/*! \brief Descriptor of the data stream buffer to inject into an output module pin
 * \see ProcessingModuleInterface::Process()
 */
struct OutputStreamBuffer
{
	uint8_t *const data; /*!< data stream buffer */
	/*!
	* \brief size indicator about the data in the stream buffer
	*
	* - When read, it indicates the size of available room in the stream buffer
	* - When written, it reports the size of data which has actually be produced into the buffer during the buffer processing
	*   (can be less than the given available room size)
	*/
	size_t size;
};

/*! \brief enumeration values of fragment position tag */
enum ConfigurationFragmentPositionEnum
{
	MIDDLE = 0, /*!< Indicates that the associacted fragment is in the middle of message transmission (nor first neither last one) */
	FIRST = 1, /*!< Indicates that the associacted fragment is the first one of a multi-fragment message transmission */
	LAST = 2, /*!< Indicates that the associacted fragment is the last one of a multi-fragment message transmission */
	SINGLE = 3 /*!< Indicates that the associacted fragment is the single one of the message transmission */
};

/*! \brief Scoped enumeration which defines location of a configuration message fragment in the whole message
*
* \see
* - ProcessingModuleInterface::SetConfiguration()
* - ProcessingModuleInterface::GetConfiguration()
*/
struct ConfigurationFragmentPosition
{
	enum ConfigurationFragmentPositionEnum value_;
};


/*! \brief list of named error codes specific to the ProcessingModuleInterface */
enum ErrorCode
{
	INVALID_CONFIGURATION = -1 /*!< Reports that the message content given for configuration is invalid */,
	NO_CONFIGURATION /*!< Reports that the module does not support retrieval of its current configuration information */
};


/*
 * New and not in CAVS - object to be passed in all module API C calls instead
 * of comp_dev.
 * This should be any data that should be passed to modules.
 */
struct processing_module {
	// TODO:
};

/* END OF MODULE API IMPORTS */


/*****************************************************************************/
/* Codec generic data types						     */
/*****************************************************************************/
/**
 * \struct codec_interface
 * \brief Codec specific interfaces
 */
struct codec_interface {
	/**
	 * The unique ID for a codec, used for initialization as well as
	 * parameters loading.
	 */
	uint32_t id;
	/**
	 * Codec specific initialization procedure, called as part of
	 * codec_adapter component creation in .new()
	 */
	int (*init)(struct comp_dev *dev);
	 /*!
	  * \brief CAVS: Additional method called after module initialization
	  */
	//int (*Init)(void);
	//int (*init)(struct processing_module *module); /* preferred API */
	/**
	 * Codec specific prepare procedure, called as part of codec_adapter
	 * component preparation in .prepare()
	 */
	int (*prepare)(struct comp_dev *dev);
	//int (*prepare)(struct processing_module *module); /* preferred API */
	/**
	 * Codec specific. Returns the number of PCM output
	 * samples after decoding one input compressed frame.
	 * Codecs will return 0 for don't care.
	 */
	int (*get_samples)(struct comp_dev *dev); /* TODO: how done with CAVS ??? */
	/**
	 * Codec specific init processing procedure, called as a part of
	 * codec_adapter component copy in .copy(). Typically in this
	 * phase a processing algorithm searches for the valid header,
	 * does header decoding to get the parameters and initializes
	 * state and configuration structures.
	 */
	int (*init_process)(struct comp_dev *dev); /* TODO: how done with CAVS ???, maybe not done as codecs not used ?? */
	/**
	 * Codec specific processing procedure, called as part of codec_adapter
	 * component copy in .copy(). This procedure is responsible to consume
	 * samples provided by the codec_adapter and produce/output the processed
	 * ones back to codec_adapter.
	 */
	int (*process)(struct comp_dev *dev);
	/*!
	 * \brief CAVS: Processes the stream buffers extracted from the input pins and produces the resulting signal in stream buffer of the output pins.
	 *
	 * The user-defined implementation of Process() is generally expected to consume all the samples avalaible in the input stream buffers
	 * and should produce the samples for all free room available in the output stream buffers.
	 * Note that in normal condition all connected input pins will receive "ibs" (i.e. "Input Buffer Size") data bytes in their input stream buffers
	 * and output pins should produce "obs" (i.e. "Output Buffer Size") data bytes in their output stream buffers.
	 * ("ibs" and "obs" values are given to module at construction time within the \ref ModuleInitialSettings parameter).
	 * However in "end of stream" condition input stream buffers may be filled with less data count than "ibs".
	 * Therefore less data count than "obs" can be put in the output buffers.
	 *
	 * \remarks Length of input_stream_buffers and output_stream_buffers C-arrays don't need to be part of the Process() prototype
	 * as those lengths are well-known by the user-defined implementation of the ProcessingModuleInterface.
	 * \return Custom implementation can return an user-defined error code value. This user-defined error code will be transmitted to
	 * host driver if the value is different from 0 (0 is considered as a "no-error value")
	 */
	uint32_t (*Process)(
		struct InputStreamBuffer* input_stream_buffers /*!< [in,out] C-array of input buffers to process. "data" field value can be NULL if the associated pin is not connected */,
		struct OutputStreamBuffer* output_stream_buffers /*!< [in,out] C-array of output buffers to produce. "data" field value can be NULL if the associated pin is not connected
						       *
						       * \note "size" field value is set with the total room available in the output buffers at Process() method call.
						       * It shall be updated within the method to report to the ADSP System the actual data size put in the output buffers.
						       */
	);
	/* preferred API */
//	uint32_t (*process)(struct processing_module *module,
//		struct InputStreamBuffer* input_stream_buffers /*!< [in,out] C-array of input buffers to process. "data" field value can be NULL if the associated pin is not connected */,
//		struct OutputStreamBuffer* output_stream_buffers /*!< [in,out] C-array of output buffers to produce. "data" field value can be NULL if the associated pin is not connected
//						       *
//						       * \note "size" field value is set with the total room available in the output buffers at Process() method call.
//						       * It shall be updated within the method to report to the ADSP System the actual data size put in the output buffers.
//						       */
//	);
	/**
	 * Codec specific apply config procedure, called by codec_adapter every time
	 * a new RUNTIME configuration has been sent if the adapter has been
	 * prepared. This will not be called for SETUP cfg.
	 */
	int (*apply_config)(struct comp_dev *dev); // Needed ??? should do in prepare.
	/*!
	 * \brief CAVS Retrieves the configuration message for the given configuration ID.
	 *
	 * If the complete configuration message is greater than 4096 bytes, the transmission will be splitted into several fragments (lesser or equal to 4096 bytes).
	 * In this case the ADSP System will perform multiple call to GetConfiguration() until completion of the configuration message retrieval.
	 * \note config_id indicates ID of the configuration message only on first fragment retrieval otherwise it is set to 0.
	 */
	int (*GetConfiguration)(
		uint32_t config_id, /*!< [in] indicates ID of the configuration message that is requested to be returned*/
		struct ConfigurationFragmentPosition fragment_position, /*!< [in] indicates position of the fragment in the whole message transmission */
		uint32_t *data_offset_size, /*!< [in,out] Meaning of parameter depends on the fragment_position value.
		    * - if fragment_position is worth ConfigurationFragmentPosition::FIRST or ConfigurationFragmentPosition::SINGLE
		    *   data_offset_size shall report the data size of the full message
		    * - if fragment_position is worth ConfigurationFragmentPosition::MIDDLE or ConfigurationFragmentPosition::LAST
		    *   data_offset_size indicates the position offset of the received fragment in the full message */
		uint8_t *fragment_buffer, /*!< [out] the fragment buffer to fill */
		size_t *fragment_size /*!< [in,out] the fragment buffer size.
				       * The actual size of data written into the fragment buffer shall be reported to the ADSP System. */
	);
	/*!
	 * \brief CAVS Applies the upcoming configuration message for the given configuration ID.
	 *
	 * If the complete configuration message is greater than 4096 bytes, the transmission will be splitted into several fragments (lesser or equal to 4096 bytes).
	 * In this case the ADSP System will perform multiple calls to SetConfiguration() until completion of the configuration message sending.
	 * \note config_id indicates ID of the configuration message only on the first fragment sending otherwise it is set to 0.
	 */
	int (*SetConfiguration)(
		uint32_t config_id, /*!< [in] indicates ID of the configuration message that is provided */
		struct ConfigurationFragmentPosition fragment_position, /*!< [in] indicates position of the fragment in the whole message transmission */
		uint32_t data_offset_size, /*!< [in] Meaning of parameter depends on the fragment_position value:
		    * - if fragment_position is worth ConfigurationFragmentPosition::FIRST or ConfigurationFragmentPosition::SINGLE
		    *   data_offset_size indicates the data size of the full message
		    * - if fragment_position is worth ConfigurationFragmentPosition::MIDDLE or ConfigurationFragmentPosition::LAST
		    *   data_offset_size indicates the position offset of the received fragment in the full message. */
		const uint8_t *fragment_buffer, /*!< [in] the configuration fragment buffer */
		size_t fragment_size,  /*!< [in] the fragment buffer size.
		    * As per ADSP System design the fragment_size value will not exceed 4096 bytes. */
		uint8_t *response, /*!< [out] the response message buffer to optionaly fill */
		size_t *response_size  /*!< [in,out] the response message size.
		    * As per ADSP System design the response_size value shall not exceed 2048 bytes.
		    * Implementation of SetConfiguration shall set response_size value to the actual size (in bytes) of the response message */
	);

	/**
	 * Codec specific reset procedure, called as part of codec_adapter component
	 * reset in .reset(). This should reset all parameters to their initial stage
	 * but leave allocated memory intact.
	 */
	int (*reset)(struct comp_dev *dev);
	/*!
	 * \brief CAVS: Upon call to this method the ADSP system requires the module to reset its internal state into a well-known initial value.
	 *
	 * Parameters which may have been set through SetConfiguration() are supposed to be left unchanged.
	 * \remarks E.g. a configurable FIR filter module will reset its internal samples history buffer but not the taps values
	 * (which may have been configured through SetConfiguration())
	 */
	//void (*Reset);
	//void (*reset)(struct processing_module *module); /* preferred API */
	/**
	 * Codec specific free procedure, called as part of codec_adapter component
	 * free in .free(). This should free all memory allocated by codec.
	 */
	int (*free)(struct comp_dev *dev);
	/*!
	 * \brief CAVS: Destructor that may contain logic executed on module destruction
	 */
	int (*Delete)(void);
	//int (*free)(struct processing_module *module); /* preferred API */


        /*!
         * \brief CAVS Sets the processing mode for the module.
         *
         * Upon the transition from one processing mode to another, the module is required to handle enabling/disabling of its custom processing
         * as smoothly as possible (no glitch, no signal discontinuity).
         *
         * \note This method is actually only relevant for modules which only manipulate PCM signal streams.
         * Thus, the ADSP System will only fire the SetProcessingMode() method for those kind of modules.
         * (e.g. not for signal decoders, encoders etc.)
         * Moreover, disabling the processing of modules which convert the trait of the signal samples (bit depth, sampling rate, etc.)
         * would make the resulting stream(s) unsuitable for the downstream modules.
         * Therefore ADSP System will not fire this method for such modules too.
         */
        void (*SetProcessingMode)(struct ProcessingMode mode);
        //void (*set_processing_mode)(struct processing_module *module, struct processing_mode *mode);
        /*!
         * \brief CAVS Gets the processing mode for the module.
         */
        struct ProcessingMode *(*GetProcessingMode)(void);
        //struct processing_mode *(*get_processing_mode)(struct processing_module *module);
};

/**
 * \enum codec_cfg_type
 * \brief Specific configuration types which can be either:
 */
enum codec_cfg_type {
	CODEC_CFG_SETUP, /**< Used to pass setup parameters */
	CODEC_CFG_RUNTIME /**< Used every time runtime parameters has been loaded. */
};

/**
 * \enum codec_state
 * \brief Codec specific states
 */
enum codec_state {
	CODEC_DISABLED, /**< Codec isn't initialized yet or has been freed.*/
	CODEC_INITIALIZED, /**< Codec initialized or reset. */
	CODEC_IDLE, /**< Codec is idle now. */
	CODEC_PROCESSING, /**< Codec is processing samples now. */
};

/** codec adapter setup config parameters */
struct ca_config {
	uint32_t codec_id;
	uint32_t reserved;
	uint32_t sample_rate;
	uint32_t sample_width;
	uint32_t channels;
};

/**
 * \struct codec_config
 * \brief Codec TLV parameters container - used for both config types.
 * For example if one want to set the sample_rate to 16 [kHz] and this
 * parameter was assigned to id 0x01, its max size is four bytes then the
 * configuration filed should look like this (note little-endian format):
 * 0x01 0x00 0x00 0x00, 0x0C 0x00 0x00 0x00, 0x10 0x00 0x00 0x00.
 */
struct codec_param {
	/**
	 * Specifies the unique id of a parameter. For example the parameter
	 * sample_rate may have an id of 0x01.
	 */
	uint32_t id;
	uint32_t size; /**< The size of whole parameter - id + size + data */
	int32_t data[]; /**< A pointer to memory where config is stored.*/
};

/**
 * \struct codec_config
 * \brief Codec config container, used for both config types.
 */
struct codec_config {
	size_t size; /**< Specifies the size of whole config */
	bool avail; /**< Marks config as available to use.*/
	void *data; /**< tlv config, a pointer to memory where config is stored. */
};

/**
 * \struct codec_memory
 * \brief codec memory block - used for every memory allocated by codec
 */
struct codec_memory {
	void *ptr; /**< A pointr to particular memory block */
	struct list_item mem_list; /**< list of memory allocated by codec */
};

/**
 * \struct codec_processing_data
 * \brief Processing data shared between particular codec & codec_adapter
 */
struct codec_processing_data {
	uint32_t in_buff_size; /**< Specifies the size of codec input buffer. */
	uint32_t out_buff_size; /**< Specifies the size of codec output buffer.*/
	uint32_t avail; /**< Specifies how much data is available for codec to process.*/
	uint32_t produced; /**< Specifies how much data the codec produced in its last task.*/
	uint32_t consumed; /**< Specified how much data the codec consumed in its last task */
	uint32_t init_done; /**< Specifies if the codec initialization is finished */
	void *in_buff; /**< A pointer to codec input buffer. */
	void *out_buff; /**< A pointer to codec output buffer. */
};

/** private, runtime codec data */
struct codec_data {
	uint32_t id;
	enum codec_state state;
	void *private; /**< self object, memory tables etc here */
	void *runtime_params;
	struct codec_config s_cfg; /**< setup config */
	struct codec_config r_cfg; /**< runtime config */
	struct codec_interface *ops; /**< codec specific operations */
	struct codec_memory memory; /**< memory allocated by codec */
	struct codec_processing_data cpd; /**< shared data comp <-> codec */
};

/* codec_adapter private, runtime data */
struct comp_data {
	struct ca_config ca_config;
	struct codec_data codec; /**< codec private data */
	struct comp_buffer *ca_sink;
	struct comp_buffer *ca_source;
	struct comp_buffer *local_buff;
	struct sof_ipc_stream_params stream_params;
	uint32_t period_bytes; /** pipeline period bytes */
	uint32_t deep_buff_bytes; /**< copy start threshold */
};

/*****************************************************************************/
/* Codec generic interfaces						     */
/*****************************************************************************/
int codec_load_config(struct comp_dev *dev, void *cfg, size_t size,
		      enum codec_cfg_type type);
int codec_init(struct comp_dev *dev, struct codec_interface *interface);
void *codec_allocate_memory(struct comp_dev *dev, uint32_t size,
			    uint32_t alignment);
int codec_free_memory(struct comp_dev *dev, void *ptr);
void codec_free_all_memory(struct comp_dev *dev);
int codec_prepare(struct comp_dev *dev);
int codec_get_samples(struct comp_dev *dev);
int codec_init_process(struct comp_dev *dev);
int codec_process(struct comp_dev *dev);
int codec_apply_runtime_config(struct comp_dev *dev);
int codec_reset(struct comp_dev *dev);
int codec_free(struct comp_dev *dev);

struct comp_dev *codec_adapter_new(const struct comp_driver *drv,
				   struct comp_ipc_config *config,
				   struct codec_interface *interface,
				   void *spec);
int codec_adapter_prepare(struct comp_dev *dev);
int codec_adapter_params(struct comp_dev *dev, struct sof_ipc_stream_params *params);
int codec_adapter_copy(struct comp_dev *dev);
int codec_adapter_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size);
int codec_adapter_trigger(struct comp_dev *dev, int cmd);
void codec_adapter_free(struct comp_dev *dev);
int codec_adapter_reset(struct comp_dev *dev);

#endif /* __SOF_AUDIO_CODEC_GENERIC__ */
