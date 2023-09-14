/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 *
 * \file common.h
 * \brief Extract from generic.h definitions that need to be included
 *        in intel module adapter code.
 * \author Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *
 */

#ifndef __SOF_MODULE_INTERFACE__
#define __SOF_MODULE_INTERFACE__

#include <sof/compiler_attributes.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>

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

struct comp_dev;
struct timestamp_data;
struct dai_ts_data;
/**
 * \struct module_endpoint_ops
 * \brief Ops relevant only for the endpoint devices such as the host copier or DAI copier.
 *	  Other modules should not implement these.
 */
struct module_endpoint_ops {
	/**
	 * Returns total data processed in number bytes.
	 * @param dev Component device
	 * @param stream_no Index of input/output stream
	 * @param input Selects between input (true) or output (false) stream direction
	 * @return total data processed if succeeded, 0 otherwise.
	 */
	uint64_t (*get_total_data_processed)(struct comp_dev *dev, uint32_t stream_no, bool input);
	/**
	 * Retrieves component rendering position.
	 * @param dev Component device.
	 * @param posn Receives reported position.
	 */
	int (*position)(struct comp_dev *dev, struct sof_ipc_stream_posn *posn);
	/**
	 * Configures timestamping in attached DAI.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_config)(struct comp_dev *dev);

	/**
	 * Starts timestamping.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_start)(struct comp_dev *dev);

	/**
	 * Stops timestamping.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_stop)(struct comp_dev *dev);

	/**
	 * Gets timestamp.
	 * @param dev Component device.
	 * @param tsd Receives timestamp data.
	 *
	 * Mandatory for components that allocate DAI.
	 */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	int (*dai_ts_get)(struct comp_dev *dev, struct dai_ts_data *tsd);
#else
	int (*dai_ts_get)(struct comp_dev *dev, struct timestamp_data *tsd);
#endif

	/**
	 * Fetches hardware stream parameters.
	 * @param dev Component device.
	 * @param params Receives copy of stream parameters retrieved from
	 *	DAI hw settings.
	 * @param dir Stream direction (see enum sof_ipc_stream_direction).
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_get_hw_params)(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params, int dir);

	/**
	 * Triggers device state.
	 * @param dev Component device.
	 * @param cmd Trigger command.
	 */
	int (*trigger)(struct comp_dev *dev, int cmd);
};

struct processing_module;
/**
 * \struct module_interface
 * \brief 3rd party processing module interface
 */
struct module_interface {
	/**
	 * Module specific initialization procedure, called as part of
	 * module_adapter component creation in .new()
	 */
	int (*init)(struct processing_module *mod);
	/**
	 * Module specific prepare procedure, called as part of module_adapter
	 * component preparation in .prepare()
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
	 * the module
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
	 */
	int (*process_raw_data)(struct processing_module *mod,
				struct input_stream_buffer *input_buffers,
				int num_input_buffers,
				struct output_stream_buffer *output_buffers,
				int num_output_buffers);

	/**
	 * Set module configuration for the given configuration ID
	 *
	 * If the complete configuration message is greater than MAX_BLOB_SIZE bytes, the
	 * transmission will be split into several smaller fragments.
	 * In this case the ADSP System will perform multiple calls to SetConfiguration() until
	 * completion of the configuration message sending.
	 * \note config_id indicates ID of the configuration message only on the first fragment
	 * sending, otherwise it is set to 0.
	 */
	int (*set_configuration)(struct processing_module *mod,
				 uint32_t config_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size);

	/**
	 * Get module runtime configuration for the given configuration ID
	 *
	 * If the complete configuration message is greater than MAX_BLOB_SIZE bytes, the
	 * transmission will be split into several smaller fragments.
	 * In this case the ADSP System will perform multiple calls to GetConfiguration() until
	 * completion of the configuration message retrieval.
	 * \note config_id indicates ID of the configuration message only on the first fragment
	 * retrieval, otherwise it is set to 0.
	 */
	int (*get_configuration)(struct processing_module *mod,
				 uint32_t config_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size);

	/**
	 * Set processing mode for the module
	 */
	int (*set_processing_mode)(struct processing_module *mod,
				   enum module_processing_mode mode);

	/**
	 * Get the current processing mode for the module
	 */
	enum module_processing_mode (*get_processing_mode)(struct processing_module *mod);

	/**
	 * Module specific reset procedure, called as part of module_adapter component
	 * reset in .reset(). This should reset all parameters to their initial stage
	 * and free all memory allocated during prepare().
	 */
	int (*reset)(struct processing_module *mod);
	/**
	 * Module specific free procedure, called as part of module_adapter component
	 * free in .free(). This should free all memory allocated during module initialization.
	 */
	int (*free)(struct processing_module *mod);
	/**
	 * Module specific bind procedure, called when modules are bound with each other
	 */
	int (*bind)(struct processing_module *mod, void *data);
	/**
	 * Module specific unbind procedure, called when modules are disconnected from one another
	 */
	int (*unbind)(struct processing_module *mod, void *data);

	const struct module_endpoint_ops *endpoint_ops;
};

/* Convert first_block/last_block indicator to fragment position */
static inline enum module_cfg_fragment_position
first_last_block_to_frag_pos(bool first_block, bool last_block)
{
	int frag_position = (last_block << 1) | first_block;

	return (enum module_cfg_fragment_position)frag_position;
}

#endif /* __SOF_MODULE_INTERFACE__ */
