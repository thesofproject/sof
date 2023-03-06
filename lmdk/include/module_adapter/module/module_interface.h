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
	int (*prepare)(struct processing_module *mod);
	/**
	 * Module specific processing procedure, called as part of module_adapter
	 * component copy in .copy(). This procedure is responsible to consume
	 * samples provided by the module_adapter and produce/output the processed
	 * ones back to module_adapter.
	 */
	int (*process)(struct processing_module *mod, struct input_stream_buffer *input_buffers,
		       int num_input_buffers, struct output_stream_buffer *output_buffers,
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
};

/* Convert first_block/last_block indicator to fragment position */
static inline enum module_cfg_fragment_position
first_last_block_to_frag_pos(bool first_block, bool last_block)
{
	int frag_position = (last_block << 1) | first_block;

	return (enum module_cfg_fragment_position)frag_position;
}

#endif /* __SOF_MODULE_INTERFACE__ */
