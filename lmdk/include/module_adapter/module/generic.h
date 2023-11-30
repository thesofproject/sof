/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 *
 * \file generic.h
 * \brief Generic Module API header file
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#ifndef __SOF_AUDIO_MODULE_GENERIC__
#define __SOF_AUDIO_MODULE_GENERIC__

#include <../../include/component.h>
#include "module_interface.h"
#if CONFIG_INTEL_MODULES
#include "iadk_modules.h"
#endif

#define MAX_BLOB_SIZE 8192
#define MODULE_MAX_SOURCES 8

#define API_CALL(cd, cmd, sub_cmd, value, ret) \
	do { \
		ret = (cd)->api((cd)->self, \
				(cmd), \
				(sub_cmd), \
				(value)); \
	} while (0)

#define DECLARE_MODULE_ADAPTER(adapter, uuid, tr) \
static struct comp_dev *module_##adapter##_shim_new(const struct comp_driver *drv, \
					 const struct comp_ipc_config *config, \
					 const void *spec) \
{ \
	return module_adapter_new(drv, config, &(adapter), spec);\
} \
\
static const struct comp_driver comp_##adapter##_module = { \
	.type = SOF_COMP_MODULE_ADAPTER, \
	.uid = SOF_RT_UUID(uuid), \
	.tctx = &(tr), \
	.ops = { \
		.create = module_##adapter##_shim_new, \
		.prepare = module_adapter_prepare, \
		.params = module_adapter_params, \
		.copy = module_adapter_copy, \
		.cmd = module_adapter_cmd, \
		.trigger = module_adapter_trigger, \
		.reset = module_adapter_reset, \
		.free = module_adapter_free, \
		.set_large_config = module_set_large_config,\
		.get_large_config = module_get_large_config,\
		.get_attribute = module_adapter_get_attribute,\
		.bind = module_adapter_bind,\
		.unbind = module_adapter_unbind,\
		.get_total_data_processed = module_adapter_get_total_data_processed,\
		.dai_get_hw_params = module_adapter_get_hw_params,\
		.position = module_adapter_position,\
		.dai_ts_config = module_adapter_ts_config_op,\
		.dai_ts_start = module_adapter_ts_start_op,\
		.dai_ts_stop = module_adapter_ts_stop_op,\
		.dai_ts_get = module_adapter_ts_get_op,\
	}, \
}; \
\
static SHARED_DATA struct comp_driver_info comp_module_##adapter##_info = { \
	.drv = &comp_##adapter##_module, \
}; \
\
UT_STATIC void sys_comp_module_##adapter##_init(void) \
{ \
	comp_register(platform_shared_get(&comp_module_##adapter##_info, \
					  sizeof(comp_module_##adapter##_info))); \
} \
\
DECLARE_MODULE(sys_comp_module_##adapter##_init)

/**
 * \enum module_state
 * \brief Module-specific states
 */
enum module_state {
	MODULE_DISABLED, /**< Module isn't initialized yet or has been freed.*/
	MODULE_INITIALIZED, /**< Module initialized or reset. */
	MODULE_IDLE, /**< Module is idle now. */
	MODULE_PROCESSING, /**< Module is processing samples now. */
};

/**
 * \struct module_param
 * \brief Module TLV parameters container - used for both config types.
 * For example if one want to set the sample_rate to 16 [kHz] and this
 * parameter was assigned to id 0x01, its max size is four bytes then the
 * configuration filed should look like this (note little-endian format):
 * 0x01 0x00 0x00 0x00, 0x0C 0x00 0x00 0x00, 0x10 0x00 0x00 0x00.
 */
struct module_param {
	/**
	 * Specifies the unique id of a parameter. For example the parameter
	 * sample_rate may have an id of 0x01.
	 */
	uint32_t id;
	uint32_t size; /**< The size of whole parameter - id + size + data */
	int32_t data[]; /**< A pointer to memory where config is stored.*/
};

/**
 * \struct module_memory
 * \brief module memory block - used for every memory allocated by module
 */
struct module_memory {
	void *ptr; /**< A pointr to particular memory block */
	struct list_item mem_list; /**< list of memory allocated by module */
};

/**
 * \struct module_processing_data
 * \brief Processing data shared between particular module & module_adapter
 */
struct module_processing_data {
	uint32_t in_buff_size; /**< Specifies the size of module input buffer. */
	uint32_t out_buff_size; /**< Specifies the size of module output buffer.*/
	uint32_t avail; /**< Specifies how much data is available for module to process.*/
	uint32_t produced; /**< Specifies how much data the module produced in its last task.*/
	uint32_t consumed; /**< Specified how much data the module consumed in its last task */
	uint32_t init_done; /**< Specifies if the module initialization is finished */
	void *in_buff; /**< A pointer to module input buffer. */
	void *out_buff; /**< A pointer to module output buffer. */
};

/*
 * Used by the module to keep track of the number of sources bound to it and can be accessed
 * from different cores
 */
struct module_source_info {
	struct coherent c;
	struct comp_dev *sources[MODULE_MAX_SOURCES];
	void *private; /**< additional module-specific private info */
};

/*
 * Definition used to extend structure definitions to include fields for exclusive use by SOF.
 * This is a temporary solution used until work on separating a common interface for loadable
 * modules is completed.
 */
#define SOF_MODULE_API_PRIVATE

#include <module/module/base.h>

/*****************************************************************************/
/* Module generic interfaces						     */
/*****************************************************************************/
int module_load_config(struct comp_dev *dev, const void *cfg, size_t size);
int module_init(struct processing_module *mod, struct module_interface *interface);
void *module_allocate_memory(struct processing_module *mod, uint32_t size, uint32_t alignment);
int module_free_memory(struct processing_module *mod, void *ptr);
void module_free_all_memory(struct processing_module *mod);
int module_prepare(struct processing_module *mod,
		   struct sof_source **sources, int num_of_sources,
		   struct sof_sink **sinks, int num_of_sinks);
int module_process_sink_src(struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks);
int module_process_legacy(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers,
			  int num_output_buffers);
int module_reset(struct processing_module *mod);
int module_free(struct processing_module *mod);
int module_set_configuration(struct processing_module *mod,
			     uint32_t config_id,
			     enum module_cfg_fragment_position pos, size_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size);
int module_bind(struct processing_module *mod, void *data);
int module_unbind(struct processing_module *mod, void *data);

struct comp_dev *module_adapter_new(const struct comp_driver *drv,
				    const struct comp_ipc_config *config,
				    struct module_interface *interface, const void *spec);
int module_adapter_prepare(struct comp_dev *dev);
int module_adapter_params(struct comp_dev *dev, struct sof_ipc_stream_params *params);
int module_adapter_copy(struct comp_dev *dev);
int module_adapter_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size);
int module_adapter_trigger(struct comp_dev *dev, int cmd);
void module_adapter_free(struct comp_dev *dev);
int module_adapter_reset(struct comp_dev *dev);
int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset, const char *data);
int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset, char *data);
int module_adapter_get_attribute(struct comp_dev *dev, uint32_t type, void *value);
int module_adapter_bind(struct comp_dev *dev, void *data);
int module_adapter_unbind(struct comp_dev *dev, void *data);
uint64_t module_adapter_get_total_data_processed(struct comp_dev *dev,
						 uint32_t stream_no, bool input);
int module_adapter_get_hw_params(struct comp_dev *dev, struct sof_ipc_stream_params *params,
				 int dir);
int module_adapter_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn);
int module_adapter_ts_config_op(struct comp_dev *dev);
int module_adapter_ts_start_op(struct comp_dev *dev);
int module_adapter_ts_stop_op(struct comp_dev *dev);
int module_adapter_ts_get_op(struct comp_dev *dev, struct timestamp_data *tsd);

static inline void module_update_buffer_position(struct input_stream_buffer *input_buffers,
						 struct output_stream_buffer *output_buffers,
						 uint32_t frames)
{
	struct audio_stream  *source = input_buffers->data;
	struct audio_stream  *sink = output_buffers->data;

	input_buffers->consumed += audio_stream_frame_bytes(source) * frames;
	output_buffers->size += audio_stream_frame_bytes(sink) * frames;
}

/* when source argument is NULL, this function returns the first unused entry */
static inline
int find_module_source_index(struct module_source_info  *msi,
			     const struct comp_dev *source)
{
	int i;

	for (i = 0; i < MODULE_MAX_SOURCES; i++) {
		if (msi->sources[i] == source)
			return i;
	}

	return -EINVAL;
}

#endif /* __SOF_AUDIO_MODULE_GENERIC__ */
