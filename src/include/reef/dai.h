/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_DAI__
#define __INCLUDE_DAI__

#include <stdint.h>
#include <reef/audio/pipeline.h>

#define DAI_FMT_I2S		1 /* I2S mode */
#define DAI_FMT_RIGHT_J		2 /* Right Justified mode */
#define DAI_FMT_LEFT_J		3 /* Left Justified mode */
#define DAI_FMT_DSP_A		4 /* L data MSB after FRM LRC */
#define DAI_FMT_DSP_B		5 /* L data MSB during FRM LRC */
#define DAI_FMT_PDM		6 /* Pulse density modulation */

#define DAI_FMT_CONT		(1 << 4) /* continuous clock */
#define DAI_FMT_GATED		(0 << 4) /* clock is gated */

#define DAI_FMT_NB_NF		(0 << 8) /* normal bit clock + frame */
#define DAI_FMT_NB_IF		(2 << 8) /* normal BCLK + inv FRM */
#define DAI_FMT_IB_NF		(3 << 8) /* invert BCLK + nor FRM */
#define DAI_FMT_IB_IF		(4 << 8) /* invert BCLK + FRM */

#define DAI_FMT_CBM_CFM		(1 << 12) /* codec clk & FRM master */
#define DAI_FMT_CBS_CFM		(2 << 12) /* codec clk slave & FRM master */
#define DAI_FMT_CBM_CFS		(3 << 12) /* codec clk master & frame slave */
#define DAI_FMT_CBS_CFS		(4 << 12) /* codec clk & FRM slave */

#define DAI_FMT_FORMAT_MASK	0x000f
#define DAI_FMT_CLOCK_MASK	0x00f0
#define DAI_FMT_INV_MASK	0x0f00
#define DAI_FMT_MASTER_MASK	0xf000

#define DAI_CLOCK_IN		0
#define DAI_CLOCK_OUT		1

#define DAI_DIR_PLAYBACK	0
#define DAI_DIR_CAPTURE		1

#define DAI_TRIGGER_START	PIPELINE_CMD_START
#define DAI_TRIGGER_STOP	PIPELINE_CMD_STOP
#define DAI_TRIGGER_PAUSE_PUSH	PIPELINE_CMD_PAUSE
#define DAI_TRIGGER_PAUSE_RELEASE	PIPELINE_CMD_RELEASE
#define DAI_TRIGGER_SUSPEND	PIPELINE_CMD_SUSPEND
#define DAI_TRIGGER_RESUME	PIPELINE_CMD_RESUME

#define DAI_NUM_SLOT_MAPS	8

/* UUIDs for known DAIs */
#define DAI_UUID_SSP0		0
#define DAI_UUID_SSP1		1
#define DAI_UUID_SSP2		2
#define DAI_UUID_SSP3		3
#define DAI_UUID_SSP4		4
#define DAI_UUID_SSP5		5

struct dai;

/* DAI operations - all optional */ 
struct dai_ops {
	int (*set_fmt)(struct dai *dai);
	int (*trigger)(struct dai *dai, int cmd, int direction);
	int (*pm_context_restore)(struct dai *dai);
	int (*pm_context_store)(struct dai *dai);
	int (*probe)(struct dai *dai);
};

/* DAI slot map to audio channel */
struct dai_slot_map {
	uint8_t channel;	/* channel ID - CHAN_ID_ */
	uint8_t slot;		/* physical slot index */
};

/* DAI runtime hardware configuration */
struct dai_config {
	uint16_t format;
	uint16_t frame_size;	/* in BCLKs */
	struct dai_slot_map tx_slot_map[DAI_NUM_SLOT_MAPS];
	struct dai_slot_map rx_slot_map[DAI_NUM_SLOT_MAPS];
	uint16_t bclk_fs;	/* ratio between frame size and BCLK */
	uint16_t mclk_fs;	/* ratio between frame size and MCLK */
	uint32_t mclk;		/* mclk frequency in Hz */
	uint16_t clk_src;	/* DAI specific clk source */
};

/* DAI platform data */
struct dai_plat_data {
	uint32_t base;
	uint16_t irq;
	uint16_t fifo_width;
	uint16_t fifo_depth;
	uint16_t fifo_tx_watermark;
	uint16_t fifo_rx_watermark;
	uint16_t flags;
};

struct dai {
	uint32_t uuid;
	struct dai_plat_data plat_data;
	struct dai_config config;
	const struct dai_ops *ops;
	void *private;
};

struct dai *dai_get(uint32_t uuid);

#define dai_set_drvdata(dai, data) \
	dai->private = data
#define dai_get_drvdata(dai) \
	dai->private;
#define dai_base(dai) \
	dai->plat_data.base
#define dai_irq(dai) \
	dai->plat_data.irq

/* Digital Audio interface formatting */
static inline int dai_set_fmt(struct dai *dai)
{
	return dai->ops->set_fmt(dai);
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
