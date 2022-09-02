// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek
//
// Author: Bo Pan <bo.pan@mediatek.com>
//         Chunxu Li <chunxu.li@mediatek.com>

#include <sof/common.h>
#include <sof/audio/component.h>
#include <sof/drivers/afe-drv.h>
#include <sof/drivers/afe-dai.h>
#include <sof/drivers/afe-memif.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* 76f4e24c-cd46-4564-8d1d-2e93ddbf14f0 */
DECLARE_SOF_UUID("memif", memif_uuid, 0x76f4e24c, 0xcd46, 0x4564,
		 0x8d, 0x1d, 0x2e, 0x93, 0xdd, 0xbf, 0x14, 0xf0);

DECLARE_TR_CTX(memif_tr, SOF_UUID(memif_uuid), LOG_LEVEL_INFO);

/*
 * Note: TEST_SGEN for test only
 * Define this TEST_SGEN to enable sine tone generator
 * then output data to audio memory interface(memif),
 * you can set TEST_SGEN_ID to choose output to which memif.
 * e.g. set TEST_SGEN as '1' and TEST_SGEN_ID as "MT8186_MEMIF_DL2",
 * the data source of DL2 will from sine generator.
 */

#if CONFIG_TEST_SGEN
#include <mt8186-afe-regs.h>
#include <mt8186-afe-common.h>
#define TEST_SGEN_ID MT8186_MEMIF_UL1
#define AUDIO_TML_PD_MASK 1
#define AUDIO_TML_PD_SHIFT 27

#define AFE_SGEN_FREQ_DIV_CH1_MASK 0x1f
#define AFE_SGEN_FREQ_DIV_CH1_SHIFT 0
#define AFE_SGEN_FREQ_DIV_CH2_MASK 0x1f
#define AFE_SGEN_FREQ_DIV_CH2_SHIFT 12
#define AFE_SGEN_AMP_DIV_CH1_MASK 0x7
#define AFE_SGEN_AMP_DIV_CH1_SHIFT 5
#define AFE_SGEN_AMP_DIV_CH2_MASK 0x7
#define AFE_SGEN_AMP_DIV_CH2_SHIFT 17
#define AFE_SGEN_MUTE_CH1_MASK 0x1
#define AFE_SGEN_MUTE_CH1_SHIFT 24
#define AFE_SGEN_MUTE_CH2_MASK 0x1
#define AFE_SGEN_MUTE_CH2_SHIFT 25
#define AFE_SGEN_ENABLE_MASK 0x1
#define AFE_SGEN_ENABLE_SHIFT 26

#define AFE_SINEGEN_CON1_TIMING_CH1_MASK 0xf
#define AFE_SINEGEN_CON1_TIMING_CH1_SHIFT 8
#define AFE_SINEGEN_CON1_TIMING_CH2_MASK 0xf
#define AFE_SINEGEN_CON1_TIMING_CH2_SHIFT 20

#define AFE_SINEGEN_LB_MODE_MSK 0xff
#define AFE_SINEGEN_LB_MODE_SHIFT 0

enum {
	MT8186_SGEN_UL1 = 0x96,
	MT8186_SGEN_UL2 = 0x86,
	MT8186_SGEN_DL1 = 0x6,
	MT8186_SGEN_DL2 = 0x8,
};

/*sgen freq div*/
enum {
	SGEN_FREQ_64D1 = 1,
	SGEN_FREQ_64D2 = 2,
	SGEN_FREQ_64D3 = 3,
	SGEN_FREQ_64D4 = 4,
	SGEN_FREQ_64D5 = 5,
	SGEN_FREQ_64D6 = 6,
	SGEN_FREQ_64D7 = 7,
	SGEN_FREQ_64D8 = 8,
};

/*sgen amp div*/
enum {
	SGEN_AMP_D128 = 0,
	SGEN_AMP_D64 = 1,
	SGEN_AMP_D32 = 2,
	SGEN_AMP_D16 = 3,
	SGEN_AMP_D8 = 4,
	SGEN_AMP_D4 = 5,
	SGEN_AMP_D2 = 6,
	SGEN_AMP_D1 = 7,
};

enum {
	SGEN_CH_TIMING_8K = 0,
	SGEN_CH_TIMING_11P025K = 1,
	SGEN_CH_TIMING_12K = 2,
	SGEN_CH_TIMING_384K = 3,
	SGEN_CH_TIMING_16K = 4,
	SGEN_CH_TIMING_22P05K = 5,
	SGEN_CH_TIMING_24K = 6,
	SGEN_CH_TIMING_352P8K = 7,
	SGEN_CH_TIMING_32K = 8,
	SGEN_CH_TIMING_44P1K = 9,
	SGEN_CH_TIMING_48K = 10,
	SGEN_CH_TIMING_88P2K = 11,
	SGEN_CH_TIMING_96K = 12,
	SGEN_CH_TIMING_176P4K = 13,
	SGEN_CH_TIMING_192K = 14,
};
#endif

struct afe_memif_dma {
	int direction; /* 1 downlink, 0 uplink */

	int memif_id;
	int dai_id;
	int irq_id;
	struct mtk_base_afe *afe;

	uint32_t dma_base;
	uint32_t dma_size;
	uint32_t rptr;
	uint32_t wptr;

	uint32_t period_size;

	unsigned int channel;
	unsigned int rate;
	unsigned int format;
};

/* acquire the specific DMA channel */
static struct dma_chan_data *memif_channel_get(struct dma *dma, unsigned int req_chan)
{
	k_spinlock_key_t key;
	struct dma_chan_data *channel;

	tr_dbg(&memif_tr, "MEMIF: channel_get(%d)", req_chan);

	key = k_spin_lock(&dma->lock);
	if (req_chan >= dma->plat_data.channels) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&memif_tr, "MEMIF: Channel %d out of range", req_chan);
		return NULL;
	}

	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&memif_tr, "MEMIF: Cannot reuse channel %d", req_chan);
		return NULL;
	}

	atomic_add(&dma->num_channels_busy, 1);
	channel->status = COMP_STATE_READY;
	k_spin_unlock(&dma->lock, key);

	return channel;
}

/* channel must not be running when this is called */
static void memif_channel_put(struct dma_chan_data *channel)
{
	k_spinlock_key_t key;

	/* Assuming channel is stopped, we thus don't need hardware to
	 * do anything right now
	 */
	tr_info(&memif_tr, "MEMIF: channel_put(%d)", channel->index);

	notifier_unregister_all(NULL, channel);

	key = k_spin_lock(&channel->dma->lock);
	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
	k_spin_unlock(&channel->dma->lock, key);
}

#if CONFIG_TEST_SGEN
static uint32_t mt8186_sinegen_timing(uint32_t rate)
{
	uint32_t sinegen_timing;

	switch (rate) {
	case 8000:
		sinegen_timing = SGEN_CH_TIMING_8K;
		break;
	case 12000:
		sinegen_timing = SGEN_CH_TIMING_12K;
		break;
	case 16000:
		sinegen_timing = SGEN_CH_TIMING_16K;
		break;
	case 24000:
		sinegen_timing = SGEN_CH_TIMING_24K;
		break;
	case 32000:
		sinegen_timing = SGEN_CH_TIMING_32K;
		break;
	case 48000:
		sinegen_timing = SGEN_CH_TIMING_48K;
		break;
	case 96000:
		sinegen_timing = SGEN_CH_TIMING_96K;
		break;
	case 192000:
		sinegen_timing = SGEN_CH_TIMING_192K;
		break;
	case 384000:
		sinegen_timing = SGEN_CH_TIMING_384K;
		break;
	case 11025:
		sinegen_timing = SGEN_CH_TIMING_11P025K;
		break;
	case 22050:
		sinegen_timing = SGEN_CH_TIMING_22P05K;
		break;
	case 44100:
		sinegen_timing = SGEN_CH_TIMING_44P1K;
		break;
	case 88200:
		sinegen_timing = SGEN_CH_TIMING_88P2K;
		break;
	case 176400:
		sinegen_timing = SGEN_CH_TIMING_176P4K;
		break;
	case 352800:
		sinegen_timing = SGEN_CH_TIMING_352P8K;
		break;
	default:
		sinegen_timing = SGEN_CH_TIMING_48K;
		tr_err(&memif_tr, "invalid rate %d, set default 48k ", rate);
	}
	tr_dbg(&memif_tr, "rate %d, sinegen_timing %d ", rate, sinegen_timing);
	return sinegen_timing;
}

static void mtk_afe_reg_update_bits(uint32_t addr_offset, uint32_t mask, uint32_t val, int shift)
{
	io_reg_update_bits(AFE_BASE_ADDR + addr_offset, mask << shift, val << shift);
}

static uint32_t mtk_afe_reg_read(uint32_t addr_offset)
{
	return io_reg_read(AFE_BASE_ADDR + addr_offset);
}

static void mt8186_afe_sinegen_enable(uint32_t sgen_id, uint32_t rate, int enable)
{
	uint32_t loopback_mode, reg_1, reg_2, sinegen_timing;

	tr_dbg(&memif_tr, "sgen_id %d, enable %d", sgen_id, enable);

	sinegen_timing = mt8186_sinegen_timing(rate);

	if (enable == 1) {
		/* set loopback mode */
		switch (sgen_id) {
		case MT8186_MEMIF_UL1:
			loopback_mode = MT8186_SGEN_UL1;
			break;
		case MT8186_MEMIF_UL2:
			loopback_mode = MT8186_SGEN_UL2;
			break;
		case MT8186_MEMIF_DL1:
			loopback_mode = MT8186_SGEN_DL1;
			break;
		case MT8186_MEMIF_DL2:
			loopback_mode = MT8186_SGEN_DL2;
			break;
		default:
			tr_err(&memif_tr, "invalid sgen_id %d", sgen_id);
			return;
		}
		/* enable sinegen clock*/
		mtk_afe_reg_update_bits(AUDIO_TOP_CON0, AUDIO_TML_PD_MASK, 0, AUDIO_TML_PD_SHIFT);

		/*loopback source*/
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON2, AFE_SINEGEN_LB_MODE_MSK, loopback_mode,
					AFE_SINEGEN_LB_MODE_SHIFT);

		/* sine gen timing*/
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON0, AFE_SINEGEN_CON1_TIMING_CH1_MASK,
					sinegen_timing, AFE_SINEGEN_CON1_TIMING_CH1_SHIFT);
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON0, AFE_SINEGEN_CON1_TIMING_CH2_MASK,
					sinegen_timing, AFE_SINEGEN_CON1_TIMING_CH2_SHIFT);

		/*freq div*/
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON0, AFE_SGEN_FREQ_DIV_CH1_MASK,
					SGEN_FREQ_64D1, AFE_SGEN_FREQ_DIV_CH1_SHIFT);
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON0, AFE_SGEN_FREQ_DIV_CH2_MASK,
					SGEN_FREQ_64D2, AFE_SGEN_FREQ_DIV_CH2_SHIFT);

		/*amp div*/
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON0, AFE_SGEN_AMP_DIV_CH1_MASK, SGEN_AMP_D2,
					AFE_SGEN_AMP_DIV_CH1_SHIFT);
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON0, AFE_SGEN_AMP_DIV_CH2_MASK, SGEN_AMP_D2,
					AFE_SGEN_AMP_DIV_CH2_SHIFT);
		/* enable sgen*/
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON0, AFE_SGEN_ENABLE_MASK, 1,
					AFE_SGEN_ENABLE_SHIFT);
	} else {
		/* disable sgen*/
		mtk_afe_reg_update_bits(AFE_SINEGEN_CON0, AFE_SGEN_ENABLE_MASK, 0,
					AFE_SGEN_ENABLE_SHIFT);

		/* disable sgen clock */
		mtk_afe_reg_update_bits(AUDIO_TOP_CON0, AUDIO_TML_PD_MASK, 1, AUDIO_TML_PD_SHIFT);
	}

	reg_1 = mtk_afe_reg_read(AFE_SINEGEN_CON0);
	reg_2 = mtk_afe_reg_read(AFE_SINEGEN_CON2);
	tr_dbg(&memif_tr, "AFE_SINEGEN_CON0 0x%x, AFE_SINEGEN_CON2 0x%x", reg_1, reg_2);
}
#endif

static int memif_start(struct dma_chan_data *channel)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	tr_info(&memif_tr, "MEMIF:%d start(%d), channel_status:%d", memif->memif_id, channel->index,
		channel->status);

	if (channel->status != COMP_STATE_PREPARE && channel->status != COMP_STATE_SUSPEND)
		return -EINVAL;

	channel->status = COMP_STATE_ACTIVE;
#if CONFIG_TEST_SGEN
	mt8186_afe_sinegen_enable(TEST_SGEN_ID, 48000, 1);
#endif
	/* Do the HW start of the DMA */
	return afe_memif_set_enable(memif->afe, memif->memif_id, 1);
}

static int memif_release(struct dma_chan_data *channel)
{
	int ret;
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	/* TODO actually handle pause/release properly? */
	tr_info(&memif_tr, "MEMIF: release(%d)", channel->index);

	if (channel->status != COMP_STATE_PAUSED)
		return -EINVAL;

	channel->status = COMP_STATE_ACTIVE;
	ret = afe_memif_set_enable(memif->afe, memif->memif_id, 0);
	if (ret < 0)
		return ret;
#if CONFIG_TEST_SGEN
	mt8186_afe_sinegen_enable(TEST_SGEN_ID, 48000, 0);
#endif

	return 0;
}

static int memif_pause(struct dma_chan_data *channel)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	/* TODO actually handle pause/release properly? */
	tr_info(&memif_tr, "MEMIF: pause(%d)", channel->index);

	if (channel->status != COMP_STATE_ACTIVE)
		return -EINVAL;

	channel->status = COMP_STATE_PAUSED;

	/* Disable HW requests */
	return afe_memif_set_enable(memif->afe, memif->memif_id, 0);
}

static int memif_stop(struct dma_chan_data *channel)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	tr_info(&memif_tr, "MEMIF: stop(%d)", channel->index);
	/* Validate state */
	/* TODO: Should we? */
	switch (channel->status) {
	case COMP_STATE_READY:
	case COMP_STATE_PREPARE:
		return 0; /* do not try to stop multiple times */
	case COMP_STATE_PAUSED:
	case COMP_STATE_ACTIVE:
		break;
	default:
		return -EINVAL;
	}
	channel->status = COMP_STATE_READY;
	/* Disable channel */
	return afe_memif_set_enable(memif->afe, memif->memif_id, 0);
}

static int memif_copy(struct dma_chan_data *channel, int bytes, uint32_t flags)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);
	struct dma_cb_data next = {
		.channel = channel,
		.elem.size = bytes,
	};

	/* TODO XRUN check, update hw ptr */
	/* TODO TBD Playback first data check */

	/* update user hwptr */
	if (memif->direction)
		memif->wptr = (memif->wptr + bytes) % memif->dma_size;
	else
		memif->rptr = (memif->rptr + bytes) % memif->dma_size;
	tr_dbg(&memif_tr, "memif_copy: wptr:%u, rptr:%u", memif->wptr, memif->rptr);

	notifier_event(channel, NOTIFIER_ID_DMA_COPY, NOTIFIER_TARGET_CORE_LOCAL, &next,
		       sizeof(next));

	return 0;
}

static int memif_status(struct dma_chan_data *channel, struct dma_chan_status *status,
			uint8_t direction)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);
	unsigned int hw_ptr;

	status->state = channel->status;
	status->flags = 0;

	/* update current hw point */
	hw_ptr = afe_memif_get_cur_position(memif->afe, memif->memif_id);
	if (!hw_ptr) {
		status->r_pos = 0;
		status->w_pos = 0;
		status->timestamp = timer_get_system(timer_get());
		return -EINVAL;
	}
	hw_ptr -= memif->dma_base;
	if (memif->direction)
		memif->rptr = hw_ptr;
	else
		memif->wptr = hw_ptr;

	status->r_pos = memif->rptr + memif->dma_base;
	status->w_pos = memif->wptr + memif->dma_base;
	status->timestamp = timer_get_system(timer_get());
	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int memif_set_config(struct dma_chan_data *channel, struct dma_sg_config *config)
{
	unsigned int dma_addr;
	int i, dai_id, irq_id, direction, ret;
	int dma_size = 0;
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	channel->is_scheduling_source = config->is_scheduling_source;
	channel->direction = config->direction;

	direction = afe_memif_get_direction(memif->afe, memif->memif_id);
	tr_info(&memif_tr, "memif_set_config, direction:%d, afe_dir:%d", config->direction,
		direction);

	switch (config->direction) {
	case DMA_DIR_MEM_TO_DEV:
		if (direction != MEM_DIR_PLAYBACK)
			return -EINVAL;

		dai_id = (int)AFE_HS_GET_DAI(config->dest_dev);
		irq_id = (int)AFE_HS_GET_IRQ(config->dest_dev);
		dma_addr = (int)config->elem_array.elems[0].src;
		break;
	case DMA_DIR_DEV_TO_MEM:
		if (direction != MEM_DIR_CAPTURE)
			return -EINVAL;

		dai_id = (int)AFE_HS_GET_DAI(config->src_dev);
		irq_id = (int)AFE_HS_GET_IRQ(config->src_dev);
		dma_addr = (int)config->elem_array.elems[0].dest;
		tr_dbg(&memif_tr, "capture: dai_id:%d, dma_addr:%u\n", dai_id, dma_addr);
		break;
	default:
		tr_err(&memif_tr, "afe_memif_set_config() unsupported config direction");
		return -EINVAL;
	}

	for (i = 0; i < config->elem_array.count; i++)
		dma_size += (int)config->elem_array.elems[i].size;

	if (!config->cyclic) {
		tr_err(&memif_tr, "afe-memif: Only cyclic configurations are supported!");
		return -ENOTSUP;
	}
	if (config->scatter) {
		tr_err(&memif_tr, "afe-memif: scatter enabled, that is not supported for now!");
		return -ENOTSUP;
	}

	memif->dai_id = dai_id;
	memif->irq_id = irq_id;
	memif->dma_base = dma_addr;
	memif->dma_size = dma_size;
	memif->direction = direction;
	/* TODO risk, it may has sync problems with DAI comp */
	memif->rptr = 0;
	memif->wptr = 0;
	memif->period_size = config->elem_array.elems[0].size;

	/* get dai's config setting from afe driver */
	ret = afe_dai_get_config(memif->afe, dai_id, &memif->channel, &memif->rate, &memif->format);
	if (ret < 0)
		return ret;
	/* set the afe memif parameters */
	ret = afe_memif_set_params(memif->afe, memif->memif_id, memif->channel, memif->rate,
				   memif->format);
	if (ret < 0)
		return ret;
	ret = afe_memif_set_addr(memif->afe, memif->memif_id, memif->dma_base, memif->dma_size);
	if (ret < 0)
		return ret;
	channel->status = COMP_STATE_PREPARE;

	return 0;
}

static int memif_remove(struct dma *dma)
{
	int channel;
	struct mtk_base_afe *afe = afe_get();

	if (dma->chan) {
		for (channel = 0; channel < dma->plat_data.channels; channel++) {
			/* TODO Disable HW requests for this channel */
			rfree(dma_chan_get_data(&dma->chan[channel]));
		}
	}
	rfree(dma->chan);
	dma->chan = NULL;

	afe_remove(afe);
	return 0;
}

static int memif_probe(struct dma *dma)
{
	int channel;
	int ret;
	struct mtk_base_afe *afe = afe_get();
	struct afe_memif_dma *memif;

	if (!dma || dma->chan) {
		tr_err(&memif_tr, "MEMIF: Repeated probe");
		return -EEXIST;
	}

	/* do afe driver probe */
	ret = afe_probe(afe);
	if (ret < 0) {
		tr_err(&memif_tr, "MEMIF: afe_probe fail:%d", ret);
		return ret;
	}

	dma->chan = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    dma->plat_data.channels * sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&memif_tr, "MEMIF: Probe failure, unable to allocate channel descriptors");
		return -ENOMEM;
	}

	for (channel = 0; channel < dma->plat_data.channels; channel++) {
		dma->chan[channel].dma = dma;
		/* TODO need divide to UL and DL for different index */
		dma->chan[channel].index = channel;

		memif = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				sizeof(struct afe_memif_dma));
		if (!memif) {
			tr_err(&memif_tr, "afe-memif: %d channel %d private data alloc failed",
			       dma->plat_data.id, channel);
			goto out;
		}

		memif->afe = afe;
		memif->memif_id = channel;
		dma_chan_set_data(&dma->chan[channel], memif);
	}
	return 0;

out:
	memif_remove(dma);

	return -ENOMEM;
}

static int memif_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	int ret;
	unsigned int period;
	unsigned int sample_size;
	struct afe_memif_dma *memif;
	struct mtk_base_afe *afe = afe_get();

	if (channel->status == COMP_STATE_INIT)
		return 0;

	memif = dma_chan_get_data(channel);

	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		ret = afe_irq_get_status(afe, memif->irq_id);
		break;
	case DMA_IRQ_CLEAR:
		ret = afe_irq_clear(afe, memif->irq_id);
		break;
	case DMA_IRQ_MASK:
		ret = afe_irq_disable(afe, memif->irq_id);
		break;
	case DMA_IRQ_UNMASK:
		sample_size = ((memif->format == SOF_IPC_FRAME_S16_LE) ? 2 : 4) * memif->channel;
		period = memif->period_size / sample_size;
		ret = afe_irq_config(afe, memif->irq_id, memif->rate, period);
		if (ret < 0)
			return ret;
		ret = afe_irq_enable(afe, memif->irq_id);
		if (ret < 0)
			return ret;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

/* TODO need convert number to platform MACRO */
static int memif_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = 4;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = 16;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = 4;
		break;
	default:
		return -ENOENT;
	}
	return 0;
}

static int memif_get_data_size(struct dma_chan_data *channel, uint32_t *avail, uint32_t *free)
{
	uint32_t hw_ptr;
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	/* update hw pointer from afe memif */
	hw_ptr = afe_memif_get_cur_position(memif->afe, memif->memif_id);
	tr_dbg(&memif_tr, "get_pos:0x%x, base:0x%x, dir:%d", hw_ptr, memif->dma_base,
	       memif->direction);
	tr_dbg(&memif_tr, "dma_size:%u, period_size:%d", memif->dma_size, memif->period_size);
	if (!hw_ptr)
		return -EINVAL;

	hw_ptr -= memif->dma_base;

	if (memif->direction)
		memif->rptr = hw_ptr;
	else
		memif->wptr = hw_ptr;

	*avail = (memif->wptr + memif->dma_size - memif->rptr) % memif->dma_size;
	/* TODO, check if need alignment the available and free size to 1 period */
	if (memif->direction)
		*avail = DIV_ROUND_UP(*avail, memif->period_size) * memif->period_size;
	else
		*avail = *avail / memif->period_size * memif->period_size;

	*free = memif->dma_size - *avail;
	tr_dbg(&memif_tr, "r:0x%x, w:0x%x, avail:%u, free:%u ", memif->wptr, *avail, *free);

	return 0;
}

const struct dma_ops memif_ops = {
	.channel_get = memif_channel_get,
	.channel_put = memif_channel_put,
	.start = memif_start,
	.stop = memif_stop,
	.pause = memif_pause,
	.release = memif_release,
	.copy = memif_copy,
	.status = memif_status,
	.set_config = memif_set_config,
	.probe = memif_probe,
	.remove = memif_remove,
	.interrupt = memif_interrupt,
	.get_attribute = memif_get_attribute,
	.get_data_size = memif_get_data_size,
};
