/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
  * \file include/sof/lib/dai.h
  * \brief DAI Drivers definition
  * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
  * \author Keyon Jie <yang.jie@linux.intel.com>
  */

#ifndef __SOF_LIB_DAI_H__
#define __SOF_LIB_DAI_H__

#include <platform/lib/dai.h>
#include <sof/bit.h>
#include <sof/list.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

struct dai;
struct sof_ipc_dai_config;
struct sof_ipc_stream_params;

/** \addtogroup sof_dai_drivers DAI Drivers
 *  DAI Drivers API specification.
 *  @{
 */

#define DAI_CLOCK_IN		0
#define DAI_CLOCK_OUT		1

#define DAI_DIR_PLAYBACK	0
#define DAI_DIR_CAPTURE		1

#define DAI_NUM_SLOT_MAPS	8

#define DAI_INFO_TYPE		0
#define DAI_INFO_DMA_CAPS	1
#define DAI_INFO_DMA_DEV	2

/* DAI flags */

/** \brief IRQ used for copy() timer */
#define DAI_FLAGS_IRQ_CB	BIT(0)

/* DAI get() flags */

/** \brief If the device does not exist it will be created */
#define DAI_CREAT		BIT(0)

/**
 * \brief DAI operations - all optional
 *
 * DAI drivers may allocate private data,
 * which can be set with 'dai_set_drvdata' and retrieved with 'dai_get_drvdata'.
 * If a single DAI instance can have multiple DMA links and/or there is
 * some other possibility of the same instance being used in multiple
 * contexts at the same time, the private data should be allocated in the
 * SOF_MEM_ZONE_SHARED.
 */
struct dai_ops {
	int (*set_config)(struct dai *dai, struct sof_ipc_dai_config *config);
	int (*trigger)(struct dai *dai, int cmd, int direction);
	int (*pm_context_restore)(struct dai *dai);
	int (*pm_context_store)(struct dai *dai);
	int (*get_hw_params)(struct dai *dai,
			     struct sof_ipc_stream_params *params, int dir);
	int (*hw_params)(struct dai *dai, struct sof_ipc_stream_params *params);
	int (*get_handshake)(struct dai *dai, int direction, int stream_id);
	int (*get_fifo)(struct dai *dai, int direction, int stream_id);
	int (*probe)(struct dai *dai);
	int (*remove)(struct dai *dai);
};

struct timestamp_cfg {
	uint32_t walclk_rate; /* Rate in Hz, e.g. 19200000 */
	int type; /* SSP, DMIC, HDA, etc. */
	int direction; /* Playback, capture */
	int index; /* For SSPx to select correct timestamp register */
	int dma_id; /* GPDMA id*/
	int dma_chan_index; /* Used GPDMA channel */
	int dma_chan_count; /* Channels in single GPDMA */
};

struct timestamp_data {
	uint64_t walclk; /* Wall clock */
	uint64_t sample; /* Sample count */
	uint32_t walclk_rate; /* Rate in Hz, e.g. 19200000 */
};

struct timestamp_ops {
	int (*ts_config)(struct dai *dai, struct timestamp_cfg *cfg);
	int (*ts_start)(struct dai *dai, struct timestamp_cfg *cfg);
	int (*ts_stop)(struct dai *dai, struct timestamp_cfg *cfg);
	int (*ts_get)(struct dai *dai, struct timestamp_cfg *cfg,
		      struct timestamp_data *tsd);
};

struct dai_driver {
	uint32_t type;	/**< type, one of SOF_DAI_... */
	const struct sof_uuid_entry *uid;
	struct tr_ctx *tctx;
	uint32_t dma_caps;
	uint32_t dma_dev;
	struct dai_ops ops;
	struct timestamp_ops ts_ops;
};

/**
 * \brief DAI slot map to audio channel
 */
struct dai_slot_map {
	uint32_t channel;	/**< channel ID - CHAN_ID_ */
	uint32_t slot;		/**< physical slot index */
};

struct dai_plat_fifo_data {
	uint32_t offset;
	uint32_t width;
	uint32_t depth;
	uint32_t watermark;
	uint32_t handshake;
};

/**
 * \brief DAI platform data
 */
struct dai_plat_data {
	uint32_t base;
	int irq;
	const char *irq_name;
	uint32_t flags;
	struct dai_plat_fifo_data fifo[2];
};

struct dai {
	uint32_t index;		/**< index */
	spinlock_t lock;	/**< locking mechanism */
	int sref;		/**< simple ref counter, guarded by lock */
	struct dai_plat_data plat_data;
	const struct dai_driver *drv;
	void *priv_data;
};

/**
 * \brief Array of DAIs grouped by type.
 */
struct dai_type_info {
	uint32_t type;		/**< Type */
	struct dai *dai_array;	/**< Array of DAIs */
	size_t num_dais;	/**< Number of elements in dai_array */
};

/* dai tracing */
#define trace_dai_drv_get_tr_ctx(drv_p) ((drv_p)->tctx)
#define trace_dai_drv_get_id(drv_p) (-1)
#define trace_dai_drv_get_subid(drv_p) (-1)

#define trace_dai_get_tr_ctx(dai_p) ((dai_p)->drv->tctx)
#define trace_dai_get_id(dai_p) ((dai_p)->drv->type)
#define trace_dai_get_subid(dai_p) ((dai_p)->index)

/* class (driver) level (no device object) tracing */

#define dai_cl_err(drv_p, __e, ...)		\
	trace_dev_err(trace_dai_dvr_get_tr_ctx,	\
		      trace_dai_drv_get_id,	\
		      trace_dai_drv_get_subid,	\
		      drv_p, __e, ##__VA_ARGS__)

#define dai_cl_warn(drv_p, __e, ...)		\
	trace_dev_warn(trace_dai_drv_get_tr_ctx,\
		       trace_dai_drv_get_id,	\
		       trace_dai_drv_get_subid,	\
		       drv_p, __e, ##__VA_ARGS__)

#define dai_cl_info(drv_p, __e, ...)		\
	trace_dev_info(trace_dai_drv_get_tr_ctx,\
		       trace_dai_drv_get_id,	\
		       trace_dai_drv_get_subid,	\
		       drv_p, __e, ##__VA_ARGS__)

#define dai_cl_dbg(drv_p, __e, ...)		\
	trace_dev_dbg(trace_dai_drv_get_tr_ctx,	\
		      trace_dai_drv_get_id,	\
		      trace_dai_drv_get_subid,	\
		      drv_p, __e, ##__VA_ARGS__)

/* device tracing */

#define dai_err(dai_p, __e, ...)					\
	trace_dev_err(trace_dai_get_tr_ctx,				\
		      trace_dai_get_id,					\
		      trace_dai_get_subid, dai_p, __e, ##__VA_ARGS__)

#define dai_warn(dai_p, __e, ...)					\
	trace_dev_warn(trace_dai_get_tr_ctx,				\
		       trace_dai_get_id,				\
		       trace_dai_get_subid, dai_p, __e, ##__VA_ARGS__)

#define dai_info(dai_p, __e, ...)					\
	trace_dev_info(trace_dai_get_tr_ctx,				\
		       trace_dai_get_id,				\
		       trace_dai_get_subid, dai_p, __e, ##__VA_ARGS__)

#define dai_dbg(dai_p, __e, ...)					\
	trace_dev_dbg(trace_dai_get_tr_ctx,				\
		      trace_dai_get_id,					\
		      trace_dai_get_subid, dai_p, __e, ##__VA_ARGS__)

/**
 * \brief API to request DAI group
 *
 * Returns a DAI group for the given ID and
 * increments the counter of DAIs in the group.
 *
 * If a group for the given ID doesn't exist,
 * it will either return NULL or allocate a new group structure
 * if the CREATE flag is supplied.
 *
 * \param[in] group_id Group ID
 * \param[in] flags Flags (CREATE)
 */
struct dai_group *dai_group_get(uint32_t group_id, uint32_t flags);

/**
 * \brief API to release DAI group
 *
 * Decrements the DAI counter inside the group.
 *
 * \param[in] group Group
 */
void dai_group_put(struct dai_group *group);

/**
 * \brief DAI group information
 */
struct dai_group {
	/**
	 * Group ID
	 */
	uint32_t group_id;

	/**
	 * Number of DAIs in this group
	 */
	uint32_t num_dais;

	/**
	 * Number of DAIs to receive a trigger before processing begins
	 */
	uint32_t trigger_counter;

	/**
	 * Trigger command to propagate
	 */
	int trigger_cmd;

	/**
	 * Last trigger error
	 */
	int trigger_ret;

	/**
	 * Group list
	 */
	struct list_item list;
};

/**
 * \brief Holds information about array of DAIs grouped by type.
 */
struct dai_info {
	const struct dai_type_info *dai_type_array;
	size_t num_dai_types;
};

/**
 * \brief API to initialize a platform DAI.
 *
 * \param[in] sof Pointer to firmware main context.
 */
int dai_init(struct sof *sof);

/**
 * \brief API to request a platform DAI.
 *
 * \param[in] type Type of requested DAI.
 * \param[in] index Index of requested DAI.
 * \param[in] flags Flags (CREATE)
 */
struct dai *dai_get(uint32_t type, uint32_t index, uint32_t flags);

/**
 * \brief API to release a platform DAI.
 *
 * @param[in] dai DAI to relese.
 */
void dai_put(struct dai *dai);

#define dai_set_drvdata(dai, data) \
	(dai->priv_data = data)
#define dai_get_drvdata(dai) \
	dai->priv_data
#define dai_base(dai) \
	dai->plat_data.base
#define dai_irq(dai) \
	dai->plat_data.irq
#define dai_fifo(dai, direction) \
	dai->plat_data.fifo[direction].offset

/**
 * \brief Digital Audio interface formatting
 */
static inline int dai_set_config(struct dai *dai,
				 struct sof_ipc_dai_config *config)
{
	int ret = dai->drv->ops.set_config(dai, config);


	return ret;
}

/**
 * \brief Digital Audio interface trigger
 */
static inline int dai_trigger(struct dai *dai, int cmd, int direction)
{
	int ret = dai->drv->ops.trigger(dai, cmd, direction);


	return ret;
}

/**
 * \brief Digital Audio interface PM context store
 */
static inline int dai_pm_context_store(struct dai *dai)
{
	int ret = dai->drv->ops.pm_context_store(dai);


	return ret;
}

/**
 * \brief Digital Audio interface PM context restore
 */
static inline int dai_pm_context_restore(struct dai *dai)
{
	int ret = dai->drv->ops.pm_context_restore(dai);


	return ret;
}

/**
 * \brief Get Digital Audio interface stream parameters
 */
static inline int dai_get_hw_params(struct dai *dai,
				    struct sof_ipc_stream_params *params,
				    int dir)
{
	int ret = dai->drv->ops.get_hw_params(dai, params, dir);


	return ret;
}

/**
 * \brief Configure Digital Audio interface stream parameters
 */
static inline int dai_hw_params(struct dai *dai,
				struct sof_ipc_stream_params *params)
{
	int ret = 0;

	if (dai->drv->ops.hw_params)
		ret = dai->drv->ops.hw_params(dai, params);

	return ret;
}

/**
 * \brief Get Digital Audio interface DMA Handshake
 */
static inline int dai_get_handshake(struct dai *dai, int direction,
				    int stream_id)
{
	int ret = dai->drv->ops.get_handshake(dai, direction, stream_id);


	return ret;
}

/**
 * \brief Get Digital Audio interface FIFO address
 */
static inline int dai_get_fifo(struct dai *dai, int direction,
			       int stream_id)
{
	int ret = dai->drv->ops.get_fifo(dai, direction, stream_id);


	return ret;
}

/**
 * \brief Digital Audio interface Probe
 */
static inline int dai_probe(struct dai *dai)
{
	int ret = dai->drv->ops.probe(dai);


	return ret;
}

/**
 * \brief Digital Audio interface Remove
 */
static inline int dai_remove(struct dai *dai)
{
	int ret = dai->drv->ops.remove(dai);


	return ret;
}

/**
 * \brief Get driver specific DAI information
 */
static inline int dai_get_info(struct dai *dai, int info)
{
	int ret;

	switch (info) {
	case DAI_INFO_TYPE:
		ret = dai->drv->type;
		break;
	case DAI_INFO_DMA_CAPS:
		ret = dai->drv->dma_caps;
		break;
	case DAI_INFO_DMA_DEV:
		ret = dai->drv->dma_dev;
		break;
	default:
		ret = -EINVAL;
		break;
	}


	return ret;
}

static inline void dai_write(struct dai *dai, uint32_t reg, uint32_t value)
{
	io_reg_write(dai_base(dai) + reg, value);

}

static inline uint32_t dai_read(struct dai *dai, uint32_t reg)
{
	uint32_t val = io_reg_read(dai_base(dai) + reg);


	return val;
}

static inline void dai_update_bits(struct dai *dai, uint32_t reg,
				   uint32_t mask, uint32_t value)
{
	io_reg_update_bits(dai_base(dai) + reg, mask, value);

}

static inline const struct dai_info *dai_info_get(void)
{
	return sof_get()->dai_info;
}

/** @}*/

#endif /* __SOF_LIB_DAI_H__ */
