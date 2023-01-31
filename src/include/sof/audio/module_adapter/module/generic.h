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

#include <sof/audio/component.h>
#include <sof/ut.h>
#include <sof/lib/memory.h>
#include "module_interface.h"

#if CONFIG_INTEL_MODULES
#include "iadk_modules.h"
#endif

#define module_get_private_data(mod) (mod->priv.private)
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

/** private, runtime module data */
struct module_data {
	enum module_state state;
	size_t new_cfg_size; /**< size of new module config data */
	void *private; /**< self object, memory tables etc here */
	void *runtime_params;
	struct module_config cfg; /**< module configuration data */
	struct module_interface *ops; /**< module specific operations */
	struct module_memory memory; /**< memory allocated by module */
	struct module_processing_data mpd; /**< shared data comp <-> module */
	void *module_adapter; /**<loadable module interface handle */
	uint32_t module_entry_point; /**<loadable module entry point address */
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

/* module_adapter private, runtime data */
struct processing_module {
	struct module_data priv; /**< module private data */
	struct sof_ipc_stream_params *stream_params;
	struct list_item sink_buffer_list; /* list of sink buffers to save produced output */

	/*
	 * This is a temporary change in order to support the trace messages in the modules. This
	 * will be removed once the trace API is updated.
	 */
	struct comp_dev *dev;
	uint32_t period_bytes; /** pipeline period bytes */
	uint32_t deep_buff_bytes; /**< copy start threshold */
	uint32_t output_buffer_size; /**< size of local buffer to save produced samples */
	struct input_stream_buffer *input_buffers;
	struct output_stream_buffer *output_buffers;
	uint32_t num_input_buffers; /**< number of input buffers */
	uint32_t num_output_buffers; /**< number of output buffers */
	/*
	 * flag set by a module that produces period_bytes every copy. It can be used by modules
	 * that support 1:1, 1:N, N:1 sources:sinks configuration.
	 */
	bool simple_copy;

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

	/* flag to insure that module is loadable */
	bool is_native_sof;

	/* pointer to system services for loadable modules */
	struct adsp_system_service *sys_service;

	/* table containing the list of connected sources */
	struct module_source_info *source_info;
};

/*****************************************************************************/
/* Module generic interfaces						     */
/*****************************************************************************/
int module_load_config(struct comp_dev *dev, const void *cfg, size_t size);
int module_init(struct processing_module *mod, struct module_interface *interface);
void *module_allocate_memory(struct processing_module *mod, uint32_t size, uint32_t alignment);
int module_free_memory(struct processing_module *mod, void *ptr);
void module_free_all_memory(struct processing_module *mod);
int module_prepare(struct processing_module *mod);
int module_process(struct processing_module *mod, struct input_stream_buffer *input_buffers,
		   int num_input_buffers, struct output_stream_buffer *output_buffers,
		   int num_output_buffers);
int module_reset(struct processing_module *mod);
int module_free(struct processing_module *mod);
int module_set_configuration(struct processing_module *mod,
			     uint32_t config_id,
			     enum module_cfg_fragment_position pos, size_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size);

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

static inline void module_update_buffer_position(struct input_stream_buffer *input_buffers,
						 struct output_stream_buffer *output_buffers,
						 uint32_t frames)
{
	struct audio_stream __sparse_cache *source = input_buffers->data;
	struct audio_stream __sparse_cache *sink = output_buffers->data;

	input_buffers->consumed += audio_stream_frame_bytes(source) * frames;
	output_buffers->size += audio_stream_frame_bytes(sink) * frames;
}

__must_check static inline
struct module_source_info __sparse_cache *module_source_info_acquire(struct module_source_info *msi)
{
	struct coherent __sparse_cache *c;

	c = coherent_acquire_thread(&msi->c, sizeof(*msi));

	return attr_container_of(c, struct module_source_info __sparse_cache, c, __sparse_cache);
}

static inline void module_source_info_release(struct module_source_info __sparse_cache *msi)
{
	coherent_release_thread(&msi->c, sizeof(*msi));
}

/* when source argument is NULL, this function returns the first unused entry */
static inline
int find_module_source_index(struct module_source_info __sparse_cache *msi,
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
