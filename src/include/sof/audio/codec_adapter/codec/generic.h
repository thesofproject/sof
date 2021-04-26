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
					 struct sof_ipc_comp *comp)\
{ \
	return codec_adapter_new(drv, comp, &(adapter));\
} \
\
static const struct comp_driver comp_codec_adapter = { \
	.type = SOF_COMP_NONE, \
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
	/**
	 * Codec specific prepare procedure, called as part of codec_adapter
	 * component preparation in .prepare()
	 */
	int (*prepare)(struct comp_dev *dev);
	/**
	 * Codec specific. Returns the number of PCM output
	 * samples after decoding one input compressed frame.
	 * Codecs will return 0 for don't care.
	 */
	int (*get_samples)(struct comp_dev *dev);
	/**
	 * Codec specific init processing procedure, called as a part of
	 * codec_adapter component copy in .copy(). Typically in this
	 * phase a processing algorithm searches for the valid header,
	 * does header decoding to get the parameters and initializes
	 * state and configuration structures.
	 */
	int (*init_process)(struct comp_dev *dev);
	/**
	 * Codec specific processing procedure, called as part of codec_adapter
	 * component copy in .copy(). This procedure is responsible to consume
	 * samples provided by the codec_adapter and produce/output the processed
	 * ones back to codec_adapter.
	 */
	int (*process)(struct comp_dev *dev);
	/**
	 * Codec specific apply config procedure, called by codec_adapter every time
	 * a new RUNTIME configuration has been sent if the adapter has been
	 * prepared. This will not be called for SETUP cfg.
	 */
	int (*apply_config)(struct comp_dev *dev);
	/**
	 * Codec specific reset procedure, called as part of codec_adapter component
	 * reset in .reset(). This should reset all parameters to their initial stage
	 * but leave allocated memory intact.
	 */
	int (*reset)(struct comp_dev *dev);
	/**
	 * Codec specific free procedure, called as part of codec_adapter component
	 * free in .free(). This should free all memory allocated by codec.
	 */
	int (*free)(struct comp_dev *dev);
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
				   struct sof_ipc_comp *comp,
				   struct codec_interface *interface);
int codec_adapter_prepare(struct comp_dev *dev);
int codec_adapter_params(struct comp_dev *dev, struct sof_ipc_stream_params *params);
int codec_adapter_copy(struct comp_dev *dev);
int codec_adapter_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size);
int codec_adapter_trigger(struct comp_dev *dev, int cmd);
void codec_adapter_free(struct comp_dev *dev);
int codec_adapter_reset(struct comp_dev *dev);

#endif /* __SOF_AUDIO_CODEC_GENERIC__ */
