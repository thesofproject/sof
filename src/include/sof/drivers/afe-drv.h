// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: Bo Pan <bo.pan@mediatek.com>
//         YC Hung <yc.hung@mediatek.com>

#ifndef __SOF_DRIVERS_AFE_DRV_H__
#define __SOF_DRIVERS_AFE_DRV_H__

struct mtk_base_memif_data {
	int id;
	const char *name;
	int reg_ofs_base;
	int reg_ofs_cur;
	int reg_ofs_end;
	int reg_ofs_base_msb;
	int reg_ofs_cur_msb;
	int reg_ofs_end_msb;
	int fs_reg;
	int fs_shift;
	int fs_maskbit;
	int mono_reg;
	int mono_shift;
	int mono_invert;
	int quad_ch_reg;
	int quad_ch_mask;
	int quad_ch_shift;
	int enable_reg;
	int enable_shift;
	int hd_reg;
	int hd_shift;
	int hd_align_reg;
	int hd_align_mshift;
	int msb_reg;
	int msb_shift;
	int msb2_reg;
	int msb2_shift;
	int agent_disable_reg;
	int agent_disable_shift;
	int ch_num_reg;
	int ch_num_shift;
	int ch_num_maskbit;
	/* playback memif only */
	int pbuf_reg;
	int pbuf_mask;
	int pbuf_shift;
	int minlen_reg;
	int minlen_mask;
	int minlen_shift;
};

struct mtk_base_irq_data {
	int id;
	int irq_cnt_reg;
	int irq_cnt_shift;
	int irq_cnt_maskbit;
	int irq_fs_reg;
	int irq_fs_shift;
	int irq_fs_maskbit;
	int irq_en_reg;
	int irq_en_shift;
	int irq_clr_reg;
	int irq_clr_shift;
	int irq_ap_en_reg;
	int irq_ap_en_shift;
	int irq_scp_en_reg;
	int irq_scp_en_shift;
};

struct mtk_base_afe_memif {
	unsigned int dma_addr;
	unsigned int afe_addr;
	unsigned int buffer_size;

	const struct mtk_base_memif_data *data;
	int irq_usage;
};

struct mtk_base_afe_dai {
	int id;
	unsigned int channel;
	unsigned int rate;
	unsigned int format;
	/* other? */
};

struct mtk_base_afe_irq {
	const struct mtk_base_irq_data *irq_data;
	int mask;
	int irq_occupyed;
};

struct mtk_base_afe {
	int ref_count;
	unsigned int base;

	struct mtk_base_afe_memif *memif;
	int memifs_size;
	int memif_32bit_supported;
	int memif_dl_num;

	struct mtk_base_afe_irq *irqs;
	int irqs_size;

	struct mtk_base_afe_dai *dais;
	int dais_size;

	unsigned int (*afe2adsp_addr)(unsigned int addr);
	unsigned int (*adsp2afe_addr)(unsigned int addr);
	unsigned int (*afe_fs)(unsigned int rate, int aud_blk);
	unsigned int (*irq_fs)(unsigned int rate);

	int base_end_offset;

	void *platform_priv;
};

/* platform information */
struct mtk_base_afe_platform {
	unsigned int base_addr;
	const struct mtk_base_memif_data *memif_datas;
	int memif_size;
	int memif_32bit_supported;
	int memif_dl_num;

	struct mtk_base_irq_data *irq_datas;
	int irqs_size;
	int dais_size;

	int base_end_offset;

	/* misc */
	unsigned int (*afe2adsp_addr)(unsigned int addr);
	unsigned int (*adsp2afe_addr)(unsigned int addr);
	unsigned int (*afe_fs)(unsigned int rate, int aud_blk);
	unsigned int (*irq_fs)(unsigned int rate);
};

extern struct mtk_base_afe_platform mtk_afe_platform;

struct mtk_base_afe *afe_get(void);
int afe_probe(struct mtk_base_afe *afe);
int afe_remove(struct mtk_base_afe *afe);

/* dai operation */
int afe_dai_get_config(struct mtk_base_afe *afe, int id, unsigned int *channel, unsigned int *rate,
		       unsigned int *format);
int afe_dai_set_config(struct mtk_base_afe *afe, int id, unsigned int channel, unsigned int rate,
		       unsigned int format);

/* memif operation */
int afe_memif_set_params(struct mtk_base_afe *afe, int id, unsigned int channel, unsigned int rate,
			 unsigned int format);
int afe_memif_set_addr(struct mtk_base_afe *afe, int id, unsigned int dma_addr,
		       unsigned int dma_bytes);
int afe_memif_set_enable(struct mtk_base_afe *afe, int id, int enable);
unsigned int afe_memif_get_cur_position(struct mtk_base_afe *afe, int id);
int afe_memif_get_direction(struct mtk_base_afe *afe, int id);

/* irq opeartion */
int afe_irq_get_status(struct mtk_base_afe *afe, int id);
int afe_irq_clear(struct mtk_base_afe *afe, int id);
int afe_irq_config(struct mtk_base_afe *afe, int id, unsigned int rate, unsigned int period);
int afe_irq_enable(struct mtk_base_afe *afe, int id);
int afe_irq_disable(struct mtk_base_afe *afe, int id);

#endif /* __SOF_DRIVERS_AFE_DRV_H__ */

