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

#ifndef __INCLUDE_DAI_H__
#define __INCLUDE_DAI_H__

#include <stdint.h>
#include <reef/audio/component.h>

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
#define DAI_FLAGS_IRQ_CB	(1 << 0)	/* irq used for copy() timer */


struct dai;

/* DAI operations - all optional */
struct dai_ops {
	int (*set_config)(struct dai *dai, struct dai_config *dai_config);
	int (*trigger)(struct dai *dai, int cmd, int direction);
	int (*pm_context_restore)(struct dai *dai);
	int (*pm_context_store)(struct dai *dai);
	int (*probe)(struct dai *dai);
	int (*set_loopback_mode)(struct dai *dai, uint32_t lbm);
};

/* DAI slot map to audio channel */
struct dai_slot_map {
	uint32_t channel;	/* channel ID - CHAN_ID_ */
	uint32_t slot;		/* physical slot index */
};

enum dai_type {
	DAI_TYPE_INTEL_SSP	= 0,
	DAI_TYPE_INTEL_HDA,
	DAI_TYPE_INTEL_DMIC,
};

/* DAI runtime hardware configuration */
struct dai_config {
	enum dai_type type;
	union {
		struct sof_ipc_dai_ssp_params *ssp;
		struct sof_ipc_dai_hda_params *hda;
		struct sof_ipc_dai_dmic_params *dmic;
	};

#if 0
	uint32_t format;
	uint32_t sample_size;	/* in BCLKs */
	struct dai_slot_map tx_slot_map[DAI_NUM_SLOT_MAPS];
	struct dai_slot_map rx_slot_map[DAI_NUM_SLOT_MAPS];
	uint32_t bclk;	/* BCLK frequency in Hz */
	uint32_t mclk;		/* mclk frequency in Hz */
	uint32_t clk_src;	/* DAI specific clk source */
	uint32_t lbm;	/* loopback mode */
#endif
};

struct dai_plat_fifo_data {
	uint32_t offset;
	uint32_t width;
	uint32_t depth;
	uint32_t watermark;
	uint32_t handshake;
};

/* DAI platform data */
struct dai_plat_data {
	uint32_t base;
	uint32_t irq;
	uint32_t flags;
	struct dai_plat_fifo_data fifo[2];
};

struct dai {
	uint32_t type;
	uint32_t index;
	struct dai_plat_data plat_data;
	struct dai_config config;
	const struct dai_ops *ops;
	void *private;
};

struct dai *dai_get(uint32_t type, uint32_t index);

#define dai_set_drvdata(dai, data) \
	dai->private = data
#define dai_get_drvdata(dai) \
	dai->private;
#define dai_base(dai) \
	dai->plat_data.base
#define dai_irq(dai) \
	dai->plat_data.irq
#define dai_fifo(dai, direction) \
	dai->plat_data.fifo[direction].offset

/* Digital Audio interface formatting */
static inline int dai_set_config(struct dai *dai, struct dai_config *dai_config)
{
	return dai->ops->set_config(dai, dai_config);
}

/* Digital Audio interface formatting */
static inline int dai_set_loopback_mode(struct dai *dai, uint32_t lbm)
{
	return dai->ops->set_loopback_mode(dai, lbm);
}

/* Digital Audio interface trigger */
static inline int dai_trigger(struct dai *dai, int cmd, int direction)
{
	return dai->ops->trigger(dai, cmd, direction);
}

/* Digital Audio interface PM context store */
static inline int dai_pm_context_store(struct dai *dai)
{
	return dai->ops->pm_context_store(dai);
}

/* Digital Audio interface PM context restore */
static inline int dai_pm_context_restore(struct dai *dai)
{
	return dai->ops->pm_context_restore(dai);
}

/* Digital Audio interface Probe */
static inline int dai_probe(struct dai *dai)
{
	return dai->ops->probe(dai);
}

#endif
