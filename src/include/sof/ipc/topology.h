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
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <sof/ipc/common.h>
#include <stdbool.h>
#include <stdint.h>

struct dai_config;
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
 * \brief Get pipeline ID from component.
 * @param icd The component device.
 * @return Pipeline ID or negative error.
 */
static inline int32_t ipc_comp_pipe_id(const struct ipc_comp_dev *icd)
{
	switch (icd->type) {
	case COMP_TYPE_COMPONENT:
		return dev_comp_pipe_id(icd->cd);
	case COMP_TYPE_BUFFER:
		return icd->cb->pipeline_id;
	case COMP_TYPE_PIPELINE:
		return icd->pipeline->pipeline_id;
	default:
		tr_err(&ipc_tr, "Unknown ipc component type %u", icd->type);
		return -EINVAL;
	};
}

/**
 * \brief Create a new IPC component.
 * @param ipc The global IPC context.
 * @param new New IPC component descriptor.
 * @return 0 on success or negative error.
 */
int ipc_comp_new(struct ipc *ipc, struct sof_ipc_comp *new);

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
int ipc_buffer_new(struct ipc *ipc, struct sof_ipc_buffer *buffer);

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
int ipc_pipeline_new(struct ipc *ipc, struct sof_ipc_pipe_new *pipeline);

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
int ipc_comp_connect(struct ipc *ipc,
		     struct sof_ipc_pipe_comp_connect *connect);

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
 * \brief Configure all DAI components attached to DAI.
 * @param ipc Global IPC context.
 * @param config DAI configuration.
 * @return 0 on success or negative error.
 */
int ipc_comp_dai_config(struct ipc *ipc, struct sof_ipc_dai_config *config);

#endif
