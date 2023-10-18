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
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/dp_queue.h>
#include "module_interface.h"

#if CONFIG_INTEL_MODULES
#include "modules.h"
#endif

/*
 * helpers to determine processing type
 * Needed till all the modules use PROCESSING_MODE_SINK_SOURCE
 */
#define IS_PROCESSING_MODE_AUDIO_STREAM(mod) \
		(!!((struct module_data *)&(mod)->priv)->ops->process_audio_stream)

#define IS_PROCESSING_MODE_RAW_DATA(mod) \
		(!!((struct module_data *)&(mod)->priv)->ops->process_raw_data)

#define IS_PROCESSING_MODE_SINK_SOURCE(mod) \
		(!!((struct module_data *)&(mod)->priv)->ops->process)

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
	const struct module_interface *ops; /**< module specific operations */
	struct module_memory memory; /**< memory allocated by module */
	struct module_processing_data mpd; /**< shared data comp <-> module */
	void *module_adapter; /**<loadable module interface handle */
	uint32_t module_entry_point; /**<loadable module entry point address */
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
};

/*****************************************************************************/
/* Module generic interfaces						     */
/*****************************************************************************/
int module_load_config(struct comp_dev *dev, const void *cfg, size_t size);
int module_init(struct processing_module *mod, const struct module_interface *interface);
void *module_allocate_memory(struct processing_module *mod, uint32_t size, uint32_t alignment);
int module_free_memory(struct processing_module *mod, void *ptr);
void module_free_all_memory(struct processing_module *mod);
int module_prepare(struct processing_module *mod,
		   struct sof_source **sources, int num_of_sources,
		   struct sof_sink **sinks, int num_of_sinks);

static inline
bool module_is_ready_to_process(struct processing_module *mod,
				struct sof_source **sources,
				int num_of_sources,
				struct sof_sink **sinks,
				int num_of_sinks)
{
	struct module_data *md = &mod->priv;

	/* LL module has to be always ready for processing */
	if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL)
		return true;

	if (md->ops->is_ready_to_process)
		return md->ops->is_ready_to_process(mod, sources, num_of_sources,
						    sinks, num_of_sinks);
	/* default action - the module is ready if there's enough data for processing and enough
	 * space to store result. IBS/OBS as declared in init_instance
	 */
	return (source_get_data_available(sources[0]) >= source_get_min_available(sources[0]) &&
		sink_get_free_size(sinks[0]) >= sink_get_min_free_space(sinks[0]));
}

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
				    const struct module_interface *interface, const void *spec);
int module_adapter_prepare(struct comp_dev *dev);
int module_adapter_params(struct comp_dev *dev, struct sof_ipc_stream_params *params);
int module_adapter_copy(struct comp_dev *dev);
int module_adapter_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size);
int module_adapter_trigger(struct comp_dev *dev, int cmd);
void module_adapter_free(struct comp_dev *dev);
int module_adapter_reset(struct comp_dev *dev);

#if CONFIG_IPC_MAJOR_3
static inline
int module_adapter_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	return -EINVAL;
}

static inline
int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset, const char *data)
{
	return 0;
}

static inline
int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset, char *data)
{
	return 0;
}

static inline
int module_adapter_bind(struct comp_dev *dev, void *data)
{
	return 0;
}

static inline
int module_adapter_unbind(struct comp_dev *dev, void *data)
{
	return 0;
}

static inline
uint64_t module_adapter_get_total_data_processed(struct comp_dev *dev,
						 uint32_t stream_no, bool input)
{
	return 0;
}

static inline int module_process_endpoint(struct processing_module *mod,
					  struct input_stream_buffer *input_buffers,
					  int num_input_buffers,
					  struct output_stream_buffer *output_buffers,
					  int num_output_buffers)
{
	return module_process_legacy(mod, input_buffers, num_input_buffers,
				     output_buffers, num_output_buffers);
}

#else
int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset, const char *data);
int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset, char *data);
int module_adapter_get_attribute(struct comp_dev *dev, uint32_t type, void *value);
int module_adapter_bind(struct comp_dev *dev, void *data);
int module_adapter_unbind(struct comp_dev *dev, void *data);
uint64_t module_adapter_get_total_data_processed(struct comp_dev *dev,
						 uint32_t stream_no, bool input);

static inline int module_process_endpoint(struct processing_module *mod,
					  struct input_stream_buffer *input_buffers,
					  int num_input_buffers,
					  struct output_stream_buffer *output_buffers,
					  int num_output_buffers)
{
	struct module_data *md = &mod->priv;

	return md->ops->process_audio_stream(mod, input_buffers, num_input_buffers,
					     output_buffers, num_output_buffers);
}

#endif

int module_adapter_get_hw_params(struct comp_dev *dev, struct sof_ipc_stream_params *params,
				 int dir);
int module_adapter_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn);
int module_adapter_ts_config_op(struct comp_dev *dev);
int module_adapter_ts_start_op(struct comp_dev *dev);
int module_adapter_ts_stop_op(struct comp_dev *dev);
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
int module_adapter_ts_get_op(struct comp_dev *dev, struct dai_ts_data *tsd);
#else
int module_adapter_ts_get_op(struct comp_dev *dev, struct timestamp_data *tsd);
#endif

void module_update_buffer_position(struct input_stream_buffer *input_buffers,
				   struct output_stream_buffer *output_buffers,
				   uint32_t frames);

int module_adapter_init_data(struct comp_dev *dev,
			     struct module_config *dst,
			     const struct comp_ipc_config *config,
			     const void *spec);
void module_adapter_reset_data(struct module_config *dst);
void module_adapter_check_data(struct processing_module *mod, struct comp_dev *dev,
			       struct comp_buffer *sink);
void module_adapter_set_params(struct processing_module *mod, struct sof_ipc_stream_params *params);
int module_adapter_set_state(struct processing_module *mod, struct comp_dev *dev,
			     int cmd);
#endif /* __SOF_AUDIO_MODULE_GENERIC__ */
