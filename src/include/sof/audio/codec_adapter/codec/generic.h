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
	 * Codec specific processing procedure, called as part of codec_adapter
	 * component copy in .copy(). This procedure is responsible to consume
	 * samples provided by the codec_adapter and produce/output the processed
	 * ones back to codec_adapter.
	 */
	int (*process)(struct comp_dev *dev);
	/**
	 * Codec specific apply config procedure, called by codec_adapter every time
	 * new configuration has been loaded.
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
 * \enum ca_state
 * \brief States of codec_adapter
 */
enum ca_state {
	PP_STATE_DISABLED = 0, /**< Codec adapter isn't initialized yet or has just been freed. */
	PP_STATE_CREATED, /**< Codec adapter created or reset.*/
	PP_STATE_PREPARED, /**< Codec adapter prepared. */
	PP_STATE_RUN, /**< Codec adapter is running now. */
};

/**
 * \enum codec_state
 * \brief Codec specific states
 */
enum codec_state {
	CODEC_DISABLED, /**< Codec isn't initialized yet or has been freed.*/
	CODEC_INITIALIZED, /**< Codec initialized or reset. */
	CODEC_PREPARED, /**< Codec prepared. */
	CODEC_RUNNING, /**< Codec is running now. */
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
	uint32_t produced; /**< Specifies how much data the codec processed in its last task.*/
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
	enum ca_state state; /**< current state of codec_adapter */
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
int codec_init(struct comp_dev *dev);
void *codec_allocate_memory(struct comp_dev *dev, uint32_t size,
			    uint32_t alignment);
int codec_free_memory(struct comp_dev *dev, void *ptr);
void codec_free_all_memory(struct comp_dev *dev);
int codec_prepare(struct comp_dev *dev);
int codec_process(struct comp_dev *dev);
int codec_apply_runtime_config(struct comp_dev *dev);
int codec_reset(struct comp_dev *dev);
int codec_free(struct comp_dev *dev);

#endif /* __SOF_AUDIO_CODEC_GENERIC__ */
