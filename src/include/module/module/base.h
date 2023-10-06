/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2020 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_MODULE_GENERIC__
#define __MODULE_MODULE_GENERIC__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "interface.h"
#include "../ipc4/base-config.h"

#define module_get_private_data(mod) ((mod)->priv.private)
#define module_set_private_data(mod, data) ((mod)->priv.private = data)

/**
 * \struct module_config
 * \brief Module config container, used for both config types.
 */
struct module_config {
	size_t size; /**< Specifies the size of whole config */
	bool avail; /**< Marks config as available to use.*/
	void *data; /**< tlv config, a pointer to memory where config is stored. */
	const void *init_data; /**< Initial IPC configuration. */
#if CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
#endif
};

/*
 * A structure containing a module's private data, intended for its exclusive use.
 *
 * This structure should contain only fields that are used be a module.
 * All other fields, used exclusively by SOF must be moved to another structure!
 */
struct module_data {
	void *private; /**< self object, memory tables etc here */
	struct module_config cfg; /**< module configuration data */

	/*
	 * Fields below can only be accessed by the SOF and must be moved to a new structure.
	 * Below #ifdef is a temporary solution used until work on separating a common interface
	 * for loadable modules is completed.
	 */
#ifdef SOF_MODULE_API_PRIVATE
	enum module_state state;
	size_t new_cfg_size; /**< size of new module config data */
	void *runtime_params;
	const struct module_interface *ops; /**< module specific operations */
	struct module_memory memory; /**< memory allocated by module */
	struct module_processing_data mpd; /**< shared data comp <-> module */
	void *module_adapter; /**<loadable module interface handle */
	uint32_t module_entry_point; /**<loadable module entry point address */
#endif /* SOF_MODULE_PRIVATE */
};

/*
 * A pointer to this structure is passed to module API functions (from struct module_interface).
 * This structure should contain only fields that should be available to a module.
 * All other fields, used exclusively by SOF must be moved to another structure!
 */
struct processing_module {
	struct module_data priv; /**< module private data */
	uint32_t period_bytes; /** pipeline period bytes */

	/*
	 * Fields below can only be accessed by the SOF and must be moved to a new structure.
	 * Below #ifdef is a temporary solution used until work on separating a common interface
	 * for loadable modules is completed.
	 */
#ifdef SOF_MODULE_API_PRIVATE
	struct sof_ipc_stream_params *stream_params;
	struct list_item sink_buffer_list; /* list of sink buffers to save produced output */

	/*
	 * This is a temporary change in order to support the trace messages in the modules. This
	 * will be removed once the trace API is updated.
	 */
	struct comp_dev *dev;
	uint32_t deep_buff_bytes; /**< copy start threshold */
	uint32_t output_buffer_size; /**< size of local buffer to save produced samples */

	/* number of sinks / sources and (when in use) input_buffers / input_buffers */
	uint32_t num_of_sources;
	uint32_t num_of_sinks;

	/* sink and source handlers for the module */
	struct sof_sink *sinks[MODULE_MAX_SOURCES];
	struct sof_source *sources[MODULE_MAX_SOURCES];

	union {
		struct {
			/* this is used in case of raw data or audio_stream mode
			 * number of buffers described by fields:
			 * input_buffers  - num_of_sources
			 * output_buffers - num_of_sinks
			 */
			struct input_stream_buffer *input_buffers;
			struct output_stream_buffer *output_buffers;
		};
		struct {
			/* this is used in case of DP processing
			 * dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP
			 */
			struct list_item dp_queue_ll_to_dp_list;
			struct list_item dp_queue_dp_to_ll_list;
		};
	};
	struct comp_buffer *source_comp_buffer; /**< single source component buffer */
	struct comp_buffer *sink_comp_buffer; /**< single sink compoonent buffer */

	/* module-specific flags for comp_verify_params() */
	uint32_t verify_params_flags;

	/* flag to indicate module does not pause */
	bool no_pause;

	/*
	 * flag to indicate that the sink buffer writeback should be skipped. It will be handled
	 * in the module's process callback
	 */
	bool skip_sink_buffer_writeback;

	/*
	 * flag to indicate that the source buffer invalidate should be skipped. It will be handled
	 * in the module's process callback
	 */
	bool skip_src_buffer_invalidate;

	/*
	 * True for module with one source component buffer and one sink component buffer
	 * to enable reduction of module processing overhead. False if component uses
	 * multiple buffers.
	 */
	bool stream_copy_single_to_single;

	/* flag to insure that module is loadable */
	bool is_native_sof;

	/* pointer to system services for loadable modules */
	uint32_t *sys_service;

	/* total processed data after stream started */
	uint64_t total_data_consumed;
	uint64_t total_data_produced;

	/* max source/sinks supported by the module */
	uint32_t max_sources;
	uint32_t max_sinks;
#endif /* SOF_MODULE_PRIVATE */
};

#endif /* __MODULE_MODULE_GENERIC__ */
