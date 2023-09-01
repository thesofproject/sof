/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Baofeng Tian <baofeng.tian@intel.com>
 */

/**
 * \file audio/host_copier.h
 * \brief host copier shared header file
 * \authors Baofeng Tian <baofeng.tian@intel.com>
 */

#ifndef __SOF_HOST_COPIER_H__
#define __SOF_HOST_COPIER_H__

#include <sof/audio/component_ext.h>
#include <sof/audio/pcm_converter.h>
#include <sof/lib/dma.h>
#include <sof/audio/ipc-config.h>
#include <ipc/stream.h>
#include <sof/lib/notifier.h>
#include <ipc4/copier.h>

typedef void (*copy_callback_t)(struct comp_dev *dev, size_t bytes);

struct host_data;
/** \brief Host copy function interface. */
typedef int (*host_copy_func)(struct host_data *hd, struct comp_dev *dev, copy_callback_t cb);

/**
 * \brief Host buffer info.
 */
struct hc_buf {
	struct dma_sg_elem_array elem_array; /**< array of SG elements */
	uint32_t current;		/**< index of current element */
	uint32_t current_end;
};

/**
 * \brief Host component data.
 *
 * Host reports local position in the host buffer every params.host_period_bytes
 * if the latter is != 0. report_pos is used to track progress since the last
 * multiple of host_period_bytes.
 *
 * host_size is the host buffer size (in bytes) specified in the IPC parameters.
 */
struct host_data {
	/* local DMA config */
	struct dma *dma;
	struct dma_chan_data *chan;
	struct dma_sg_config config;
#ifdef __ZEPHYR__
	struct dma_config z_config;
#endif
	struct comp_dev *cb_dev;

	struct comp_buffer *dma_buffer;
	struct comp_buffer *local_buffer;

	/* host position reporting related */
	uint32_t host_size;	/**< Host buffer size (in bytes) */
	uint32_t report_pos;	/**< Position in current report period */
	uint32_t local_pos;	/**< Local position in host buffer */
	uint32_t host_period_bytes;
	uint16_t stream_tag;
	uint16_t no_stream_position; /**< 1 means don't send stream position */
	uint64_t total_data_processed;
	uint8_t cont_update_posn; /**< 1 means continuous update stream position */

	/* host component attributes */
	enum comp_copy_type copy_type;	/**< Current host copy type */

	/* local and host DMA buffer info */
	struct hc_buf host;
	struct hc_buf local;

	size_t partial_size;	/**< add up DMA updates for deep buffer */

	/* pointers set during params to host or local above */
	struct hc_buf *source;
	struct hc_buf *sink;

	uint32_t dma_copy_align; /**< Minimal chunk of data possible to be
				   *  copied by dma connected to host
				   */
	uint32_t period_bytes;	/**< number of bytes per one period */

	host_copy_func copy;	/**< host copy function */
	pcm_converter_func process;	/**< processing function */

	/* IPC host init info */
	struct ipc_config_host ipc_host;

	/* stream info */
	struct sof_ipc_stream_posn posn; /* TODO: update this */
	struct ipc_msg *msg;	/**< host notification */
	uint32_t dma_buffer_size;	/* dma buffer size */
#if CONFIG_HOST_DMA_STREAM_SYNCHRONIZATION
	bool is_grouped;
	uint8_t group_id;
	uint64_t next_sync;
	uint64_t period_in_cycles;
#endif
};

int host_common_new(struct host_data *hd, struct comp_dev *dev,
		    const struct ipc_config_host *ipc_host, uint32_t config_id);
void host_common_free(struct host_data *hd);
int host_common_prepare(struct host_data *hd);
void host_common_reset(struct host_data *hd, uint16_t state);
int host_common_trigger(struct host_data *hd, struct comp_dev *dev, int cmd);
int host_common_params(struct host_data *hd, struct comp_dev *dev,
		       struct sof_ipc_stream_params *params, notifier_callback_t cb);
/* copy and process stream data from source to sink buffers */
static inline int host_common_copy(struct host_data *hd, struct comp_dev *dev, copy_callback_t cb)
{
	return hd->copy(hd, dev, cb);
}
void host_common_update(struct host_data *hd, struct comp_dev *dev, uint32_t bytes);
void host_common_one_shot(struct host_data *hd, uint32_t bytes);
int copier_host_create(struct comp_dev *dev, struct copier_data *cd,
		       const struct ipc4_copier_module_cfg *copier_cfg,
		       struct pipeline *pipeline);
void copier_host_free(struct copier_data *cd);
int copier_host_params(struct copier_data *cd, struct comp_dev *dev,
		       struct sof_ipc_stream_params *params);
void copier_host_dma_cb(struct comp_dev *dev, size_t bytes);

#endif /* __SOF_HOST_COPIER_H__ */
