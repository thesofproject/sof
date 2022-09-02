/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_IPC_TOPOLOGY_H__
#define __SOF_IPC_TOPOLOGY_H__

#include <sof/audio/buffer.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/pipeline.h>
#include <rtos/alloc.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <sof/ipc/common.h>
#include <stdbool.h>
#include <stdint.h>

/* generic IPC pipeline regardless of ABI MAJOR type that is always 4 byte aligned */
typedef uint32_t ipc_pipe_new;
typedef uint32_t ipc_pipe_comp_connect;
typedef uint32_t ipc_comp;

/*
 * Topology IPC logic uses standard types for abstract IPC features. This means all ABI MAJOR
 * abstraction is done in the IPC layer only and not in the surrounding infrastructure.
 */
#if CONFIG_IPC_MAJOR_3
#include <ipc/topology.h>
#define ipc_from_pipe_new(x) ((struct sof_ipc_pipe_new *)x)
#define ipc_from_pipe_connect(x) ((struct sof_ipc_pipe_comp_connect *)x)
#define ipc_from_comp_new(x) ((struct sof_ipc_comp *)x)
#define ipc_from_dai_config(x) ((struct sof_ipc_dai_config *)x)
#elif CONFIG_IPC_MAJOR_4
#include <ipc4/pipeline.h>
#include <ipc4/module.h>
#include <ipc4/gateway.h>
#define ipc_from_pipe_new(x) ((struct ipc4_pipeline_create *)x)
#define ipc_from_pipe_connect(x) ((struct ipc4_module_bind_unbind *)x)

const struct comp_driver *ipc4_get_comp_drv(int module_id);
struct comp_dev *ipc4_get_comp_dev(uint32_t comp_id);
int ipc4_add_comp_dev(struct comp_dev *dev);
const struct comp_driver *ipc4_get_drv(uint8_t *uuid);
int ipc4_create_chain_dma(struct ipc *ipc, struct ipc4_chain_dma *cdma);
int ipc4_trigger_chain_dma(struct ipc *ipc, struct ipc4_chain_dma *cdma);
#else
#error "No or invalid IPC MAJOR version selected."
#endif

#define ipc_to_pipe_new(x)	((ipc_pipe_new *)x)
#define ipc_to_pipe_connect(x)	((ipc_pipe_comp_connect *)x)
#define ipc_to_comp_new(x)	((ipc_comp *)x)

struct ipc_msg;

#define COMP_TYPE_COMPONENT	1
#define COMP_TYPE_BUFFER	2
#define COMP_TYPE_PIPELINE	3

/* IPC generic component device */
struct ipc_comp_dev {
	uint16_t type;	/* COMP_TYPE_ */
	uint16_t core;
	uint32_t id;

	/* component type data */
	union {
		struct comp_dev *cd;
		struct comp_buffer *cb;
		struct pipeline *pipeline;
	};

	/* lists */
	struct list_item list;		/* list in components */
};

/**
 * \brief Create a new IPC component.
 * @param ipc The global IPC context.
 * @param new New IPC component descriptor.
 * @return 0 on success or negative error.
 */
int ipc_comp_new(struct ipc *ipc, ipc_comp *new);

/**
 * \brief Free an IPC component.
 * @param ipc The global IPC context.
 * @param comp_id Component ID to free.
 * @return 0 on success or negative error.
 */
int ipc_comp_free(struct ipc *ipc, uint32_t comp_id);

/**
 * \brief Create a new IPC buffer.
 * @param ipc The global IPC context.
 * @param buffer New IPC buffer descriptor.
 * @return 0 on success or negative error.
 */
int ipc_buffer_new(struct ipc *ipc, const struct sof_ipc_buffer *buffer);

/**
 * \brief Free an IPC buffer.
 * @param ipc The global IPC context.
 * @param buffer_id buffer ID to free.
 * @return 0 on success or negative error.
 */
int ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id);

/**
 * \brief Create a new IPC pipeline.
 * @param ipc The global IPC context.
 * @param pipeline New IPC pipeline descriptor.
 * @return 0 on success or negative error.
 */
int ipc_pipeline_new(struct ipc *ipc, ipc_pipe_new *pipeline);

/**
 * \brief Free an IPC pipeline.
 * @param ipc The global IPC context.
 * @param comp_id Pipeline ID to free.
 * @return 0 on success or negative error.
 */
int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id);

/**
 * \brief Complete an IPC pipeline.
 * @param ipc The global IPC context.
 * @param comp_id Pipeline ID to complete.
 * @return 0 on success or negative error.
 */
int ipc_pipeline_complete(struct ipc *ipc, uint32_t comp_id);

/**
 * \brief Connect components together on a pipeline.
 * @param ipc The global IPC context.
 * @param connect Components to connect together..
 * @return 0 on success or negative error.
 */
int ipc_comp_connect(struct ipc *ipc, ipc_pipe_comp_connect *connect);

/**
 * \brief Disconnect components in a pipeline.
 * @param ipc The global IPC context.
 * @param connect Components.
 * @return 0 on success or negative error.
 */
int ipc_comp_disconnect(struct ipc *ipc, ipc_pipe_comp_connect *connect);

/**
 * \brief Get component device from component ID.
 * @param ipc The global IPC context.
 * @param id The component ID.
 * @return Component device or NULL.
 */
struct ipc_comp_dev *ipc_get_comp_by_id(struct ipc *ipc, uint32_t id);

/**
 * \brief Get component device from pipeline ID and type.
 * @param ipc The global IPC context.
 * @param type The component type.
 * @param ppl_id The pipeline ID.
 * @return component device or NULL.
 */
struct ipc_comp_dev *ipc_get_comp_by_ppl_id(struct ipc *ipc, uint16_t type,
					    uint32_t ppl_id);
/**
 * \brief Get buffer device from pipeline ID.
 * @param ipc The global IPC context.
 * @param pipeline_id The pipeline ID.
 * @param dir Pipeline stream direction.
 * @return Pipeline device or NULL.
 */
struct ipc_comp_dev *ipc_get_ppl_comp(struct ipc *ipc,
				      uint32_t pipeline_id, int dir);

/**
 * \brief Get pipeline ID from component.
 * @param icd The component device.
 * @return Pipeline ID or negative error.
 */
int32_t ipc_comp_pipe_id(const struct ipc_comp_dev *icd);

/**
 * \brief Configure all DAI components attached to DAI.
 * @param ipc Global IPC context.
 * @param common_config Common DAI configuration.
 * @param spec_config Specific DAI configuration.
 * @return 0 on success or negative error.
 */
int ipc_comp_dai_config(struct ipc *ipc, struct ipc_config_dai *common_config,
			void *spec_config);

/*
 * Warning: duplicate declaration in component.h
 */
int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params);

/**
 * \brief connect component and buffer
 * @param comp Component dev
 * @param comp_core comp core id
 * @param buffer Component buffer
 * @param dir connection direction
 * @return connection status
 */
int comp_buffer_connect(struct comp_dev *comp, uint32_t comp_core,
			struct comp_buffer *buffer, uint32_t dir);
#endif
