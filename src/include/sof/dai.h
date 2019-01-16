/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

 /**
  * \file include/sof/dai.h
  * \brief DAI Drivers definition
  * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
  * \author Keyon Jie <yang.jie@linux.intel.com>
  */

#ifndef __INCLUDE_DAI_H__
#define __INCLUDE_DAI_H__

#include <stdint.h>
#include <sof/audio/component.h>

/** \addtogroup sof_dai_drivers DAI Drivers
 *  DAI Drivers API specification.
 *  @{
 */

#define DAI_CLOCK_IN		0
#define DAI_CLOCK_OUT		1

#define DAI_DIR_PLAYBACK	0
#define DAI_DIR_CAPTURE		1

#define DAI_TRIGGER_START	COMP_CMD_START
#define DAI_TRIGGER_STOP	COMP_CMD_STOP
#define DAI_TRIGGER_PAUSE_PUSH	COMP_CMD_PAUSE
#define DAI_TRIGGER_PAUSE_RELEASE	COMP_CMD_RELEASE
#define DAI_TRIGGER_SUSPEND	COMP_CMD_SUSPEND
#define DAI_TRIGGER_RESUME	COMP_CMD_RESUME

#define DAI_NUM_SLOT_MAPS	8

/* DAI flags */

/** \brief IRQ used for copy() timer */
#define DAI_FLAGS_IRQ_CB	BIT(0)

/* DAI get() flags */

/** \brief If the device does not exist it will be created */
#define DAI_CREAT		BIT(0)

struct dai;

/**
 * \brief DAI operations - all optional
 */
struct dai_ops {
	int (*set_config)(struct dai *dai, struct sof_ipc_dai_config *config);
	int (*trigger)(struct dai *dai, int cmd, int direction);
	int (*pm_context_restore)(struct dai *dai);
	int (*pm_context_store)(struct dai *dai);
	int (*probe)(struct dai *dai);
	int (*remove)(struct dai *dai);
	int (*set_loopback_mode)(struct dai *dai, uint32_t lbm);
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
	uint32_t irq;
	uint32_t flags;
	struct dai_plat_fifo_data fifo[2];
};

struct dai {
	uint32_t type;		/**< type, one of SOF_DAI_... */
	uint32_t index;		/**< index */
	spinlock_t lock;
	int sref;		/**< simple ref counter, guarded by lock */
	struct dai_plat_data plat_data;
	const struct dai_ops *ops;
	void *private;
};

/**
 * \brief Array of DAIs grouped by type.
 */
struct dai_type_info {
	uint32_t type;		/**< Type */
	struct dai *dai_array;	/**< Array of DAIs */
	size_t num_dais;	/**< Number of elements in dai_array */
};

/**
 * \brief Plugs platform specific DAI array once initialized into the lib.
 *
 * Lib serves the DAIs to other FW elements with dai_get()
 *
 * \param[in] dai_type_array Array of DAI arrays grouped by type.
 * \param[in] num_dai_types Number of elements in the dai_type_array.
 */
void dai_install(struct dai_type_info *dai_type_array, size_t num_dai_types);

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
	dai->private = data
#define dai_get_drvdata(dai) \
	dai->private
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
	return dai->ops->set_config(dai, config);
}

/**
 * \brief Digital Audio interface formatting
 */
static inline int dai_set_loopback_mode(struct dai *dai, uint32_t lbm)
{
	return dai->ops->set_loopback_mode(dai, lbm);
}

/**
 * \brief Digital Audio interface trigger
 */
static inline int dai_trigger(struct dai *dai, int cmd, int direction)
{
	return dai->ops->trigger(dai, cmd, direction);
}

/**
 * \brief Digital Audio interface PM context store
 */
static inline int dai_pm_context_store(struct dai *dai)
{
	return dai->ops->pm_context_store(dai);
}

/**
 * \brief Digital Audio interface PM context restore
 */
static inline int dai_pm_context_restore(struct dai *dai)
{
	return dai->ops->pm_context_restore(dai);
}

/**
 * \brief Digital Audio interface Probe
 */
static inline int dai_probe(struct dai *dai)
{
	return dai->ops->probe(dai);
}

/**
 * \brief Digital Audio interface Remove
 */
static inline int dai_remove(struct dai *dai)
{
	return dai->ops->remove(dai);
}

/** @}*/

#endif
