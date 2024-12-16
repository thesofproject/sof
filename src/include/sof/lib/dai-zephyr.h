/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
  * \file include/sof/lib/dai-zephyr.h
  * \brief DAI Drivers definition
  * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
  * \author Keyon Jie <yang.jie@linux.intel.com>
  */

#ifndef __SOF_LIB_DAI_ZEPHYR_H__
#define __SOF_LIB_DAI_ZEPHYR_H__

#include <rtos/bit.h>
#include <sof/list.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/dma.h>
#include <sof/list.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <sof/ipc/topology.h>
#include <sof/audio/pcm_converter.h>
#include <sof/audio/ipc-config.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/dai.h>

#include <sof/debug/telemetry/performance_monitor.h>

/** \addtogroup sof_dai_drivers DAI Drivers
 *  DAI Drivers API specification.
 *  @{
 */

/** \brief If the device does not exist it will be created */
#define DAI_CREAT		BIT(0)

struct dai {
	uint32_t index;
	uint32_t type;
	uint32_t dma_caps;
	uint32_t dma_dev;
	const struct device *dev;
	const struct dai_data *dd;
	struct k_spinlock lock;		/* protect properties */
};

union hdalink_cfg {
	uint16_t full;
	struct {
		uint16_t lchan	:4;
		uint16_t hchan	:4;
		uint16_t stream	:6;
		uint16_t rsvd	:1;
		uint16_t dir	:1;
	} part;
};
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
 * \brief llp slot info for memory window
 */
struct llp_slot_info {
	uint32_t node_id;
	uint32_t reg_offset;
};

typedef int (*channel_copy_func)(const struct audio_stream *src, unsigned int src_channel,
				 struct audio_stream *dst, unsigned int dst_channel,
				 unsigned int frames);

/**
 * \brief DAI runtime data
 */
struct dai_data {
	/* local DMA config */
	struct dma_chan_data *chan;
	uint32_t stream_id;
	struct dma_sg_config config;
	struct dma_config *z_config;
	struct comp_dev *dai_dev;
	struct comp_buffer *dma_buffer;
	struct comp_buffer *local_buffer;
	struct dai_ts_cfg ts_config;
	struct dai *dai;
	struct sof_dma *dma;
	struct dai_group *group;		/* NULL if no group assigned */
	int xrun;				/* true if we are doing xrun recovery */

	pcm_converter_func process;		/* processing function */
	uint32_t chmap;

	channel_copy_func channel_copy;		/* channel copy func used by multi-endpoint
						 * gateway to mux/demux stream from/to multiple
						 * DMA buffers
						 */

	uint32_t period_bytes;			/* number of DMA bytes per one period */
	uint64_t total_data_processed;

	struct ipc_config_dai ipc_config;	/* generic common config */
	void *dai_spec_config;			/* dai specific config from the host */

	uint64_t wallclock;			/* wall clock at stream start */

	/*
	 * flag indicating two-step stop/pause for DAI comp and DAI DMA.
	 * DAI stop occurs during STREAM_TRIG_STOP IPC and DMA stop during DAI_CONFIG IPC with
	 * the SOF_DAI_CONFIG_FLAGS_HW_FREE flag.
	 * DAI pause occurs during STREAM_TRIG_PAUSE IPC and DMA pause during DAI_CONFIG IPC with
	 * the SOF_DAI_CONFIG_FLAGS_PAUSE flag.
	 */
	bool delayed_dma_stop;

	/* llp slot info in memory windows */
	struct llp_slot_info slot_info;
	/* fast mode, use one byte memory to save repreated cycles */
	bool fast_mode;
#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
	bool xrun_notification_sent;
#endif
#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS
	/* io performance measurement */
	struct io_perf_data_item *io_perf_bytes_count;
#endif
	/* Copier gain params */
	struct copier_gain_params *gain_data;
};

/* these 3 are here to satisfy clk.c and ssp.h interconnection, will be removed leter */
static inline void dai_write(struct dai *dai, uint32_t reg, uint32_t value)
{
}

static inline uint32_t dai_read(struct dai *dai, uint32_t reg)
{
	return 0;
}

static inline void dai_update_bits(struct dai *dai, uint32_t reg,
				   uint32_t mask, uint32_t value)
{
}

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
 * \brief API to initialize a platform DAI.
 *
 * \param[in] sof Pointer to firmware main context.
 */
static inline int dai_init(struct sof *sof)
{
	return 0;
}

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

/**
 * \brief Digital Audio interface formatting
 */
int dai_set_config(struct dai *dai, struct ipc_config_dai *config, const void *spec_config);

/**
 * \brief Get Digital Audio interface DMA Handshake
 */
int dai_get_handshake(struct dai *dai, int direction, int stream_id);

/**
 * \brief Get Digital Audio interface fifo depth
 */
int dai_get_fifo_depth(struct dai *dai, int direction);

/**
 * \brief Get DAI initial delay in milliseconds
 */
uint32_t dai_get_init_delay_ms(struct dai *dai);

/**
 * \brief Get DAI stream id
 */
int dai_get_stream_id(struct dai *dai, int direction);

/**
 * \brief Configure DMA channel for DAI
 */
int dai_config_dma_channel(struct dai_data *dd, struct comp_dev *dev, const void *config);

/**
 * \brief Configure HD Audio DMA params for DAI
 */
void dai_set_link_hda_config(uint16_t *link_config,
			     struct ipc_config_dai *common_config,
			     const void *spec_config);
/**
 * \brief Reset DAI DMA config
 */
void dai_dma_release(struct dai_data *dd, struct comp_dev *dev);

/**
 * \brief Configure DAI physical interface.
 */
int dai_config(struct dai_data *dd, struct comp_dev *dev,
	       struct ipc_config_dai *common_cfg, const void *spec_cfg);

/**
 * \brief Assign DAI to a group for simultaneous triggering.
 */
int dai_assign_group(struct dai_data *dd, struct comp_dev *dev, uint32_t group_id);

/**
 * \brief dai position for host driver.
 */
int dai_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn);

/**
 * \brief update dai dma position for host driver.
 */
void dai_dma_position_update(struct dai_data *dd, struct comp_dev *dev);

/**
 * \brief release llp slot
 */
void dai_release_llp_slot(struct dai_data *dd);

/**
 * \brief Retrieve a pointer to the Zephyr device structure for a DAI of a given type and index.
 */
const struct device *dai_get_device(uint32_t type, uint32_t index);
/** @}*/

#endif /* __SOF_LIB_DAI_ZEPHYR_H__ */
