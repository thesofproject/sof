// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek
//
// Author: Bo Pan <bo.pan@mediatek.com>
//         Chunxu Li <chunxu.li@mediatek.com>

#include <sof/common.h>
#include <sof/lib/io.h>
#include <rtos/alloc.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <sof/lib/uuid.h>
#include <errno.h>
#include <sof/drivers/afe-drv.h>
#include <sof/drivers/afe-memif.h>

/*#define AFE_DRV_LOG*/
static struct mtk_base_afe mtk_afe;

#ifndef AFE_DRV_LOG
#undef printf
#define printf(format, ...)
#endif

/* 21448a3f-c054-41b3-8d9e-b7619a93c27b */
DECLARE_SOF_UUID("afedrv", afedrv_uuid, 0x21448a3f, 0xc054, 0x41b3,
		 0x8d, 0x9e, 0xb7, 0x61, 0x9a, 0x93, 0xc2, 0x7b);

DECLARE_TR_CTX(afedrv_tr, SOF_UUID(afedrv_uuid), LOG_LEVEL_INFO);

static inline void afe_reg_read(struct mtk_base_afe *afe, uint32_t reg, uint32_t *value)
{
	*value = io_reg_read((uint32_t)((char *)afe->base + reg));
	tr_dbg(&afedrv_tr, "r_reg:0x%x, value:0x%x\n", reg, *value);
}

static inline void afe_reg_write(struct mtk_base_afe *afe, uint32_t reg, uint32_t value)
{
	io_reg_write((uint32_t)((char *)afe->base + reg), value);
	tr_dbg(&afedrv_tr, "w_reg:0x%x, value:0x%x\n", reg, value);
}

static inline void afe_reg_update_bits(struct mtk_base_afe *afe, uint32_t reg, uint32_t mask,
				       uint32_t value)
{
	io_reg_update_bits((uint32_t)((char *)afe->base + reg), mask, value);
	tr_dbg(&afedrv_tr, "u_reg:0x%x, value:0x%x\n", reg, value);
}

static int afe_memif_set_channel(struct mtk_base_afe *afe, int id, unsigned int channel)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	unsigned int mono;

	if (memif->data->mono_shift < 0)
		return 0;

	if (memif->data->ch_num_reg >= 0) {
		afe_reg_update_bits(afe, memif->data->ch_num_reg,
				    memif->data->ch_num_maskbit << memif->data->ch_num_shift,
				    channel << memif->data->ch_num_shift);
	}

	if (memif->data->quad_ch_mask) {
		unsigned int quad_ch = 0;

		if (channel == 4)
			quad_ch = 1;

		afe_reg_update_bits(afe, memif->data->quad_ch_reg,
				    memif->data->quad_ch_mask << memif->data->quad_ch_shift,
				    quad_ch << memif->data->quad_ch_shift);
	}

	mono = (bool)memif->data->mono_invert ^ (channel == 1);
	afe_reg_update_bits(afe, memif->data->mono_reg, 1 << memif->data->mono_shift,
			    mono << memif->data->mono_shift);
	return 0;
}

static int afe_memif_set_rate(struct mtk_base_afe *afe, int id, unsigned int rate)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int fs;

	fs = afe->afe_fs(rate, memif->data->id);
	if (fs < 0) {
		tr_err(&afedrv_tr, "invalid fs:%d\n", fs);
		return -EINVAL;
	}

	afe_reg_update_bits(afe, memif->data->fs_reg,
			    memif->data->fs_maskbit << memif->data->fs_shift,
			    fs << memif->data->fs_shift);

	return 0;
}

static int afe_memif_set_format(struct mtk_base_afe *afe, int id, unsigned int format)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int hd_audio;
	int memif_32bit_supported = afe->memif_32bit_supported;

	/* set hd mode */
	switch (format) {
	case SOF_IPC_FRAME_S16_LE:
		hd_audio = 0;
		break;
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_S24_4LE:
		if (memif_32bit_supported)
			hd_audio = 2;
		else
			hd_audio = 1;
		break;
	default:
		tr_err(&afedrv_tr, "not support format:%u\n", format);
		return -EINVAL;
	}

	afe_reg_update_bits(afe, memif->data->hd_reg, 0x3 << memif->data->hd_shift,
			    hd_audio << memif->data->hd_shift);
	return 0;
}

int afe_memif_set_params(struct mtk_base_afe *afe, int id, unsigned int channel, unsigned int rate,
			 unsigned int format)
{
	int ret;

	ret = afe_memif_set_channel(afe, id, channel);
	if (ret < 0)
		return ret;
	ret = afe_memif_set_rate(afe, id, rate);
	if (ret < 0)
		return ret;
	ret = afe_memif_set_format(afe, id, format);
	if (ret < 0)
		return ret;
	/* TODO IRQ direction, irq format setting ? */

	return ret;
}

int afe_memif_set_addr(struct mtk_base_afe *afe, int id, unsigned int dma_addr,
		       unsigned int dma_bytes)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int msb_at_bit33 = 0; /* for dsp side only support 32bit address */
	unsigned int phys_buf_addr;
	unsigned int phys_buf_addr_upper_32 = 0; /* for dsp side only support 32bit address */

	memif->dma_addr = dma_addr;

	/* convert adsp address to afe address */
	if (afe->adsp2afe_addr)
		dma_addr = afe->adsp2afe_addr(dma_addr);

	phys_buf_addr = dma_addr;

	memif->afe_addr = phys_buf_addr;
	memif->buffer_size = dma_bytes;
	tr_dbg(&afedrv_tr, "dma_addr:%u, size:%u\n", dma_addr, dma_bytes);
	/* start */
	afe_reg_write(afe, memif->data->reg_ofs_base, phys_buf_addr);
	/* end */
	if (memif->data->reg_ofs_end)
		afe_reg_write(afe, memif->data->reg_ofs_end, phys_buf_addr + dma_bytes - 1);
	else
		afe_reg_write(afe, memif->data->reg_ofs_base + afe->base_end_offset,
			      phys_buf_addr + dma_bytes - 1);

	/* set start, end, upper 32 bits */
	if (memif->data->reg_ofs_base_msb) {
		afe_reg_write(afe, memif->data->reg_ofs_base_msb, phys_buf_addr_upper_32);
		afe_reg_write(afe, memif->data->reg_ofs_end_msb, phys_buf_addr_upper_32);
	}

	/* set MSB to 33-bit */
	if (memif->data->msb_reg >= 0)
		afe_reg_update_bits(afe, memif->data->msb_reg, 1 << memif->data->msb_shift,
				    msb_at_bit33 << memif->data->msb_shift);

	/* set MSB to 33-bit, for memif end address */
	if (memif->data->msb2_reg >= 0)
		afe_reg_update_bits(afe, memif->data->msb2_reg, 1 << memif->data->msb2_shift,
				    msb_at_bit33 << memif->data->msb2_shift);

	return 0;
}

int afe_memif_set_enable(struct mtk_base_afe *afe, int id, int enable)
{
	const struct mtk_base_memif_data *memif_data = afe->memif[id].data;

	if (memif_data->enable_shift < 0)
		return 0;

	/* enable agent */
	/* TODO: enable/disable should in different sequence? */
	if (memif_data->agent_disable_reg > 0) {
		afe_reg_update_bits(afe, memif_data->agent_disable_reg,
				    1 << memif_data->agent_disable_shift,
				    (!enable) << memif_data->agent_disable_shift);
	}

	afe_reg_update_bits(afe, memif_data->enable_reg, 1 << memif_data->enable_shift,
			    enable << memif_data->enable_shift);

	return 0;
}

int afe_memif_get_direction(struct mtk_base_afe *afe, int id)
{
	const struct mtk_base_memif_data *memif_data = afe->memif[id].data;

	if (memif_data->id >= 0 && memif_data->id < afe->memif_dl_num)
		return MEM_DIR_PLAYBACK;
	return MEM_DIR_CAPTURE;
}

unsigned int afe_memif_get_cur_position(struct mtk_base_afe *afe, int id)
{
	const struct mtk_base_memif_data *memif_data = afe->memif[id].data;
	unsigned int hw_ptr;

	if (memif_data->reg_ofs_cur < 0)
		return 0;
	afe_reg_read(afe, memif_data->reg_ofs_cur, &hw_ptr);

	/* convert afe address to adsp address */
	if (afe->afe2adsp_addr)
		hw_ptr = afe->afe2adsp_addr(hw_ptr);
	return hw_ptr;
}

int afe_dai_set_config(struct mtk_base_afe *afe, int id, unsigned int channel, unsigned int rate,
		       unsigned int format)
{
	struct mtk_base_afe_dai *dai;

	/* TODO 1. if need use dai->id to search target dai */
	/* TODO 1. if need a status to control the dai status */

	if (id >= afe->dais_size)
		return -EINVAL;

	tr_info(&afedrv_tr, "afe_dai_set_config, id:%d\n", id);

	dai = &afe->dais[id];
	dai->channel = channel;
	dai->format = format;
	dai->rate = rate;

	tr_info(&afedrv_tr, "dai:%d set: format:%d, rate:%d, channel:%d\n", id, format, rate,
		channel);

	return 0;
}

int afe_dai_get_config(struct mtk_base_afe *afe, int id, unsigned int *channel, unsigned int *rate,
		       unsigned int *format)
{
	struct mtk_base_afe_dai *dai;

	/* TODO 1. if need use dai->id to search target dai */
	/* TODO 1. if need a status to control the dai status */
	tr_info(&afedrv_tr, "afe_dai_get_config, id:%d\n", id);

	if (id >= afe->dais_size || id < 0) {
		tr_err(&afedrv_tr, "afe_dai_get_config , invalid id:%d\n", id);
		return -EINVAL;
	}
	dai = &afe->dais[id];

	*channel = dai->channel;
	*rate = dai->rate;
	*format = dai->format;

	tr_info(&afedrv_tr, "dai:%d get: format:%d, rate:%d, channel:%d\n", id, *format, *rate,
		*channel);

	return 0;
}

/* TODO, IRQ common register name need use config? */
int afe_irq_get_status(struct mtk_base_afe *afe, int id)
{
	return 0;
}

int afe_irq_clear(struct mtk_base_afe *afe, int id)
{
	return 0;
}

int afe_irq_config(struct mtk_base_afe *afe, int id, unsigned int rate, unsigned int period)
{
	struct mtk_base_afe_irq *irq = &afe->irqs[id];
	int fs;

	afe_reg_update_bits(afe, irq->irq_data->irq_cnt_reg,
			    irq->irq_data->irq_cnt_maskbit << irq->irq_data->irq_cnt_shift,
			    period << irq->irq_data->irq_cnt_shift);

	/* set irq fs */
	fs = afe->irq_fs(rate);
	if (fs < 0)
		return -EINVAL;

	afe_reg_update_bits(afe, irq->irq_data->irq_fs_reg,
			    irq->irq_data->irq_fs_maskbit << irq->irq_data->irq_fs_shift,
			    fs << irq->irq_data->irq_fs_shift);
	return 0;
}

/* TODO, for dma based scheduler*/
int afe_irq_enable(struct mtk_base_afe *afe, int id)
{
	return 0;
}

int afe_irq_disable(struct mtk_base_afe *afe, int id)
{
	return 0;
}

struct mtk_base_afe *afe_get(void)
{
	return &mtk_afe;
}

int afe_probe(struct mtk_base_afe *afe)
{
	int i;
	struct mtk_base_afe_platform *platform = &mtk_afe_platform;

	/* mtk afe already init done */
	if (afe->ref_count > 0) {
		afe->ref_count++;
		return 0;
	}

	afe->platform_priv = platform;
	afe->base = platform->base_addr;
	afe->memif_32bit_supported = platform->memif_32bit_supported;
	afe->memif_dl_num = platform->memif_dl_num;

	afe->base_end_offset = platform->base_end_offset;
	afe->adsp2afe_addr = platform->adsp2afe_addr;
	afe->afe2adsp_addr = platform->afe2adsp_addr;
	afe->afe_fs = platform->afe_fs; /* must be */
	afe->irq_fs = platform->irq_fs;
	if (!afe->afe_fs)
		return -EINVAL;
	tr_dbg(&afedrv_tr, "afe_base:0x%x\n", afe->base);
	/* TODO how to get the memif number, how to sync with dmac lib */
	afe->memifs_size = platform->memif_size;
	afe->memif = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			     sizeof(struct mtk_base_afe_memif) * afe->memifs_size);
	if (!afe->memif)
		return -ENOMEM;

	for (i = 0; i < afe->memifs_size; i++)
		afe->memif[i].data = &platform->memif_datas[i];

	/* TODO how to get the dai number, how to sync with dai lib*/
	afe->dais_size = platform->dais_size;
	afe->dais = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct mtk_base_afe_dai) * afe->dais_size);
	if (!afe->dais)
		goto err_alloc_memif;

	/* TODO how to get the irq number */
	afe->irqs_size = platform->irqs_size;
	afe->irqs = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct mtk_base_afe_irq) * afe->irqs_size);
	if (!afe->irqs)
		goto err_alloc_dais;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &platform->irq_datas[i];

	afe->ref_count++;

	return 0;
err_alloc_dais:
	rfree(afe->dais);
err_alloc_memif:
	rfree(afe->memif);

	return -ENOMEM;
}

void afe_remove(struct mtk_base_afe *afe)
{
	afe->ref_count--;

	if (afe->ref_count > 0)
		return;

	if (afe->ref_count < 0) {
		afe->ref_count = 0;
		tr_dbg(&afedrv_tr, "afe ref_count < 0, :%d\n", afe->ref_count);
		return;
	}

	rfree(afe->memif);
	afe->memif = NULL;

	rfree(afe->dais);
	afe->dais = NULL;

	rfree(afe->irqs);
	afe->irqs = NULL;
}

