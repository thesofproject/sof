/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2020 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_MODULE_BASE__
#define __MODULE_MODULE_BASE__

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
	uint8_t nb_input_pins;
	uint8_t nb_output_pins;
	struct ipc4_input_pin_format *input_pins;
	struct ipc4_output_pin_format *output_pins;
#endif
};

struct llext;

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
	struct module_memory memory; /**< memory allocated by module */
	struct module_processing_data mpd; /**< shared data comp <-> module */
	struct llext *llext; /**< Zephyr loadable extension context */
#endif /* SOF_MODULE_PRIVATE */
};

enum module_processing_type {
	MODULE_PROCESS_TYPE_SOURCE_SINK,
	MODULE_PROCESS_TYPE_STREAM,
	MODULE_PROCESS_TYPE_RAW,
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
	/* list of sink buffers to save produced output, to be used in Raw data
	 * processing mode
	 */
	struct list_item raw_data_buffers_list;

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

	/* this is used in case of raw data or audio_stream mode
	 * number of buffers described by fields:
	 * input_buffers  - num_of_sources
	 * output_buffers - num_of_sinks
	 */
	struct input_stream_buffer *input_buffers;
	struct output_stream_buffer *output_buffers;
	struct comp_buffer *source_comp_buffer; /**< single source component buffer */
	struct comp_buffer *sink_comp_buffer; /**< single sink compoonent buffer */

	/* module-specific flags for comp_verify_params() */
	uint32_t verify_params_flags;

	/* indicates that this DP module did not yet reach its first deadline and
	 * no data should be passed yet to next LL module
	 *
	 * why: lets assume DP with 10ms period (a.k.a a deadline). It starts and finishes
	 * Earlier, i.e. in 2ms providing 10ms of data. LL starts consuming data in 1ms chunks and
	 * will drain 10ms buffer in 10ms, expecting a new portion of data on 11th ms
	 * BUT - the DP module deadline is still 10ms, regardless if it had finished earlier
	 * and it is completely fine that processing in next cycle takes full 10ms - as long as it
	 * fits into the deadline.
	 * It may lead to underruns:
	 *
	 * LL1 (1ms) ---> DP (10ms) -->LL2 (1ms)
	 *
	 * ticks 0..9 -> LL1 is producing 1ms data portions, DP is waiting, LL2 is waiting
	 * tick 10 - DP has enough data to run, it starts processing
	 * tick 12 - DP finishes earlier, LL2 starts consuming, LL1 is producing data
	 * ticks 13-19 LL1 is producing data, LL2 is consuming data (both in 1ms chunks)
	 * tick 20  - DP starts processing a new portion of 10ms data, having 10ms to finish
	 *	      !!!! but LL2 has already consumed 8ms !!!!
	 * tick 22 - LL2 is consuming the last 1ms data chunk
	 * tick 23 - DP is still processing, LL2 has no data to process
	 *			!!! UNDERRUN !!!!
	 * tick 19 - DP finishes properly in a deadline time
	 *
	 * Solution: even if DP finishes before its deadline, the data must be held till
	 * deadline time, so LL2 may start processing no earlier than tick 20
	 */
	bool dp_startup_delay;

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

	/* total processed data after stream started */
	uint64_t total_data_consumed;
	uint64_t total_data_produced;

	/* max source/sinks supported by the module */
	uint32_t max_sources;
	uint32_t max_sinks;

	enum module_processing_type proc_type;
#endif /* SOF_MODULE_PRIVATE */
};

#endif /* __MODULE_MODULE_BASE__ */
