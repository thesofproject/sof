/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2020 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_MODULE_INTERFACE__
#define __MODULE_MODULE_INTERFACE__

#include <stdint.h>
#include <stdbool.h>

/**
 * \enum module_cfg_fragment_position
 * \brief Fragment position in config
 * MODULE_CFG_FRAGMENT_FIRST: first fragment of the large configuration
 * MODULE_CFG_FRAGMENT_SINGLE: only fragment of the configuration
 * MODULE_CFG_FRAGMENT_LAST: last fragment of the configuration
 * MODULE_CFG_FRAGMENT_MIDDLE: intermediate fragment of the large configuration
 */
enum module_cfg_fragment_position {
	MODULE_CFG_FRAGMENT_MIDDLE = 0,
	MODULE_CFG_FRAGMENT_FIRST,
	MODULE_CFG_FRAGMENT_LAST,
	MODULE_CFG_FRAGMENT_SINGLE,
};

/**
 * \enum module_processing_mode
 * MODULE_PROCESSING_NORMAL: Indicates that module is expected to apply its custom processing on
 *			      the input signal
 * MODULE_PROCESSING_BYPASS: Indicates that module is expected to skip custom processing on
 *			      the input signal and act as a passthrough component
 */

enum module_processing_mode {
	MODULE_PROCESSING_NORMAL = 0,
	MODULE_PROCESSING_BYPASS,
};

/**
 * \struct input_stream_buffer
 * \brief Input stream buffer
 */
struct input_stream_buffer {
	void *data; /* data stream buffer */
	uint32_t size; /* size of data in the buffer */
	uint32_t consumed; /* number of bytes consumed by the module */

	/* Indicates end of stream condition has occurred on the input stream */
	bool end_of_stream;
};

/**
 * \struct output_stream_buffer
 * \brief Output stream buffer
 */
struct output_stream_buffer {
	void *data; /* data stream buffer */
	uint32_t size; /* size of data in the buffer */
};

struct processing_module;
struct sof_source;
struct sof_sink;
struct bind_info;
struct bind_info;

/*
 * This structure may be used by modules to carry short 16bit parameters.
 */
union config_param_id_data {
	uint32_t dw;
	struct {
		uint32_t data16	: 16;	/* Input/Output small config data */
		uint32_t id	: 14;	/* input parameter ID */
		uint32_t _rsvd	: 2;
	} f;
};

/**
 * \struct module_interface
 * \brief 3rd party processing module interface
 *
 * Module operations can be optimized for performance (default - no action) or
 * for memory and power efficiency (opt in using __cold). It is recommended that
 * module authors review their modules for non time sensitive code and mark it
 * using __cold based on the descriptions below. This will ensure modules
 * maintain peak performance and peak power/memory efficiency. Similarly cold
 * read-only data can be marked with __cold_rodata. In cases where a subset of
 * cold data has to be accessed from hot paths, it can be copied to fast memory,
 * using the \c fast_get() API and then released using \c fast_put().
 */
struct module_interface {
	/**
	 * Module specific initialization procedure, called as part of
	 * module_adapter component creation in .new(). Usually can be __cold
	 */
	int (*init)(struct processing_module *mod);

	/**
	 * (optional) Module specific prepare procedure, called as part of module_adapter
	 * component preparation in .prepare(). Usually can be __cold
	 */
	int (*prepare)(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks);

	/**
	 * (optional) return true if the module is ready to process
	 * This procedure should check if the module is ready for immediate
	 * processing.
	 *
	 * NOTE! the call MUST NOT perform any time consuming operations
	 *
	 * this procedure will always return true for LL module
	 *
	 * For DP there's a default implementation that will do a simple check if there's
	 * at least IBS bytes of data on first source and at least OBS free space on first sink
	 *
	 * In case more sophisticated check is needed the method should be implemented in
	 * the module. Usually shouldn't be __cold
	 */
	bool (*is_ready_to_process)(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks);

	/**
	 * Module specific processing procedure
	 * This procedure is responsible to consume
	 * samples provided by the module_adapter and produce/output the processed
	 * ones back to module_adapter.
	 *
	 * there are 3 versions of the procedure, the difference is the format of
	 * input/output data
	 *
	 * the module MUST implement one and ONLY one of them
	 *
	 * process_audio_stream and process_raw_data are depreciated and will be removed
	 * once pipeline learns to use module API directly (without module adapter)
	 * modules that need such processing should use proper wrappers
	 *
	 * process
	 *	- sources are handlers to source API struct source*[]
	 *	- sinks are handlers to sink API struct sink*[]
	 *
	 * Usually shouldn't be __cold
	 */
	int (*process)(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks);

	/**
	 * process_audio_stream (depreciated)
	 *	- sources are input_stream_buffer[]
	 *	    - sources[].data is a pointer to audio_stream structure
	 *	- sinks are output_stream_buffer[]
	 *	    - sinks[].data is a pointer to audio_stream structure
	 *
	 * It can be used by modules that support 1:1, 1:N, N:1 sources:sinks configuration.
	 *
	 * Usually shouldn't be __cold
	 */
	int (*process_audio_stream)(struct processing_module *mod,
				    struct input_stream_buffer *input_buffers,
				    int num_input_buffers,
				    struct output_stream_buffer *output_buffers,
				    int num_output_buffers);

	/**
	 * process_raw_data (depreciated)
	 *	- sources are input_stream_buffer[]
	 *	    - sources[].data is a pointer to raw audio data
	 *	- sinks are output_stream_buffer[]
	 *	    - sinks[].data is a pointer to raw audio data
	 *
	 * Usually shouldn't be __cold
	 */
	int (*process_raw_data)(struct processing_module *mod,
				struct input_stream_buffer *input_buffers,
				int num_input_buffers,
				struct output_stream_buffer *output_buffers,
				int num_output_buffers);

	/**
	 * (optional) Set module configuration parameter
	 *
	 * Using Module Config Set command, host driver may send a parameter
	 * that fits into the header (a very short one), packed along with parameter id.
	 *
	 * param_id_data specifies both ID of the parameter, defined by the module
	 * and value of the parameter.
	 * It is up to the module how to distribute bits to ID and value of the parameter.
	 */
	int (*set_config_param)(struct processing_module *mod, uint32_t param_id_data);

	/**
	 * (optional) Get module configuration parameter
	 *
	 * Using Module Config Get command, host driver may send a parameter
	 * that fits into the header (a very short one), packed along with parameter id.
	 *
	 * param_id_data specifies both ID of the parameter, defined by the module
	 * and value of the parameter.
	 * It is up to the module how to distribute bits to ID and value of the parameter.
	 */
	int (*get_config_param)(struct processing_module *mod, uint32_t *param_id_data);

	/**
	 * (optional) Set module configuration for the given configuration ID
	 *
	 * If the complete configuration message is greater than MAX_BLOB_SIZE bytes, the
	 * transmission will be split into several smaller fragments.
	 * In this case the ADSP System will perform multiple calls to SetConfiguration() until
	 * completion of the configuration message sending.
	 * \note config_id indicates ID of the configuration message only on the first fragment
	 * sending, otherwise it is set to 0. Usually can be __cold
	 */
	int (*set_configuration)(struct processing_module *mod,
				 uint32_t config_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size);

	/**
	 * (optional) Get module runtime configuration for the given configuration ID
	 *
	 * If the complete configuration message is greater than MAX_BLOB_SIZE bytes, the
	 * transmission will be split into several smaller fragments.
	 * In this case the ADSP System will perform multiple calls to GetConfiguration() until
	 * completion of the configuration message retrieval.
	 * \note config_id indicates ID of the configuration message only on the first fragment
	 * retrieval, otherwise it is set to 0. Usually can be __cold
	 */
	int (*get_configuration)(struct processing_module *mod,
				 uint32_t config_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size);

	/**
	 * (unused) Set processing mode for the module
	 */
	int (*set_processing_mode)(struct processing_module *mod,
				   enum module_processing_mode mode);

	/**
	 * (unused) Get the current processing mode for the module
	 */
	enum module_processing_mode (*get_processing_mode)(struct processing_module *mod);

	/**
	 * (optional) Module specific reset procedure, called as part of module_adapter component
	 * reset in .reset(). This should reset all parameters to their initial stage
	 * and free all memory allocated during prepare(). Usually shouldn't be __cold since it's
	 * called from pipeline_reset() from ipc4_pipeline_trigger()
	 */
	int (*reset)(struct processing_module *mod);

	/**
	 * (optional) Module specific free procedure, called as part of module_adapter component
	 * free in .free(). This should free all memory allocated during module initialization.
	 * Usually can be __cold
	 */
	int (*free)(struct processing_module *mod);

	/**
	 * (optional) Module specific bind procedure, called when modules are bound with each other.
	 * Usually can be __cold
	 */
	int (*bind)(struct processing_module *mod, struct bind_info *bind_data);

	/**
	 * (optional) Module specific unbind procedure, called when modules are disconnected from
	 * one another. Usually can be __cold
	 */
	int (*unbind)(struct processing_module *mod, struct bind_info *unbind_data);

	/**
	 * (optional) Module specific trigger procedure, called when modules are triggered. Usually
	 * shouldn't be __cold. If a module implements this method, even if it only handles
	 * commands, running in non-LL context, it will still be called from the high priority
	 * LL context, which will cause a short jump to DRAM to check for supported commands.
	 */
	int (*trigger)(struct processing_module *mod, int cmd);

	/*
	 * Ops relevant only for the endpoint devices such as the host copier or DAI copier.
	 * Other modules should not implement these.
	 *
	 * Below #ifdef is a temporary solution used until work on separating a common interface
	 * for loadable modules is completed.
	 */
#ifdef SOF_MODULE_API_PRIVATE
	const struct module_endpoint_ops *endpoint_ops;
#endif /* SOF_MODULE_PRIVATE */
};

#endif /* __MODULE_MODULE_INTERFACE__ */
