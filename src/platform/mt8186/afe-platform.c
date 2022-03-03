// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek
//
// Author: Chunxu Li <chunxu.li@mediatek.com>

#include <sof/common.h>
#include <errno.h>
#include <sof/drivers/afe-drv.h>
#include <mt8186-afe-regs.h>
#include <mt8186-afe-common.h>

/*
 * AFE: Audio Front-End
 *
 * frontend (memif):
 *   memory interface
 *   UL (uplink for capture)
 *   DL (downlink for playback)
 * backend:
 *   TDM In
 *   TMD Out
 *   DMIC
 *   GASRC
 *   I2S Out
 *   I2S In
 *   etc.
 * interconn:
 *   inter-connection,
 *   connect frontends and backends as DSP path
 */

static const struct mtk_base_memif_data memif_data[MT8186_MEMIF_NUM] = {
	[MT8186_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT8186_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_base_msb = AFE_DL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_END_MSB,
		.fs_reg = AFE_DL1_CON0,
		.fs_shift = DL1_MODE_SFT,
		.fs_maskbit = DL1_MODE_MASK,
		.mono_reg = AFE_DL1_CON0,
		.mono_shift = DL1_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL1_ON_SFT,
		.hd_reg = AFE_DL1_CON0,
		.hd_shift = DL1_HD_MODE_SFT,
		.hd_align_reg = AFE_DL1_CON0,
		.hd_align_mshift = DL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.ch_num_reg = -1,
		.ch_num_shift = -1,
		.ch_num_maskbit = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.msb2_reg = -1,
		.msb2_shift = -1,
		.pbuf_reg = AFE_DL1_CON0,
		.pbuf_mask = DL1_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL1_CON0,
		.minlen_mask = DL1_MINLEN_MASK,
		.minlen_shift = DL1_MINLEN_SFT,
	},
	[MT8186_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT8186_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_base_msb = AFE_DL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL2_END_MSB,
		.fs_reg = AFE_DL2_CON0,
		.fs_shift = DL2_MODE_SFT,
		.fs_maskbit = DL2_MODE_MASK,
		.mono_reg = AFE_DL2_CON0,
		.mono_shift = DL2_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL2_ON_SFT,
		.hd_reg = AFE_DL2_CON0,
		.hd_shift = DL2_HD_MODE_SFT,
		.hd_align_reg = AFE_DL2_CON0,
		.hd_align_mshift = DL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.ch_num_reg = -1,
		.ch_num_shift = -1,
		.ch_num_maskbit = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.msb2_reg = -1,
		.msb2_shift = -1,
		.pbuf_reg = AFE_DL2_CON0,
		.pbuf_mask = DL2_PBUF_SIZE_MASK,
		.pbuf_shift = DL2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL2_CON0,
		.minlen_mask = DL2_MINLEN_MASK,
		.minlen_shift = DL2_MINLEN_SFT,
	},
	[MT8186_MEMIF_UL1] = {
		.name = "UL1",
		.id = MT8186_MEMIF_UL1,
		.reg_ofs_base = AFE_VUL12_BASE,
		.reg_ofs_cur = AFE_VUL12_CUR,
		.reg_ofs_end = AFE_VUL12_END,
		.reg_ofs_base_msb = AFE_VUL12_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL12_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL12_END_MSB,
		.fs_reg = AFE_VUL12_CON0,
		.fs_shift = VUL12_MODE_SFT,
		.fs_maskbit = VUL12_MODE_MASK,
		.mono_reg = AFE_VUL12_CON0,
		.mono_shift = VUL12_MONO_SFT,
		.quad_ch_reg = AFE_VUL12_CON0,
		.quad_ch_mask = VUL12_4CH_EN_MASK,
		.quad_ch_shift = VUL12_4CH_EN_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL12_ON_SFT,
		.hd_reg = AFE_VUL12_CON0,
		.hd_shift = VUL12_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL12_CON0,
		.hd_align_mshift = VUL12_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.ch_num_reg = -1,
		.ch_num_shift = -1,
		.ch_num_maskbit = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.msb2_reg = -1,
		.msb2_shift = -1,
	},
	[MT8186_MEMIF_UL2] = {
		.name = "UL2",
		.id = MT8186_MEMIF_UL2,
		.reg_ofs_base = AFE_AWB_BASE,
		.reg_ofs_cur = AFE_AWB_CUR,
		.reg_ofs_end = AFE_AWB_END,
		.reg_ofs_base_msb = AFE_AWB_BASE_MSB,
		.reg_ofs_cur_msb = AFE_AWB_CUR_MSB,
		.reg_ofs_end_msb = AFE_AWB_END_MSB,
		.fs_reg = AFE_AWB_CON0,
		.fs_shift = AWB_MODE_SFT,
		.fs_maskbit = AWB_MODE_MASK,
		.mono_reg = AFE_AWB_CON0,
		.mono_shift = AWB_MONO_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = AWB_ON_SFT,
		.hd_reg = AFE_AWB_CON0,
		.hd_shift = AWB_HD_MODE_SFT,
		.hd_align_reg = AFE_AWB_CON0,
		.hd_align_mshift = AWB_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.ch_num_reg = -1,
		.ch_num_shift = -1,
		.ch_num_maskbit = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.msb2_reg = -1,
		.msb2_shift = -1,
	}
};

struct mt8186_afe_rate {
	unsigned int rate;
	unsigned int reg_value;
};

enum {
	MTK_AFE_RATE_8K = 0,
	MTK_AFE_RATE_11K = 1,
	MTK_AFE_RATE_12K = 2,
	MTK_AFE_RATE_384K = 3,
	MTK_AFE_RATE_16K = 4,
	MTK_AFE_RATE_22K = 5,
	MTK_AFE_RATE_24K = 6,
	MTK_AFE_RATE_352K = 7,
	MTK_AFE_RATE_32K = 8,
	MTK_AFE_RATE_44K = 9,
	MTK_AFE_RATE_48K = 10,
	MTK_AFE_RATE_88K = 11,
	MTK_AFE_RATE_96K = 12,
	MTK_AFE_RATE_176K = 13,
	MTK_AFE_RATE_192K = 14,
	MTK_AFE_RATE_260K = 15,
};

static const struct mt8186_afe_rate mt8186_afe_rates[] = {
	{
		.rate = 8000,
		.reg_value = MTK_AFE_RATE_8K,
	},
	{
		.rate = 12000,
		.reg_value = MTK_AFE_RATE_12K,
	},
	{
		.rate = 16000,
		.reg_value = MTK_AFE_RATE_16K,
	},
	{
		.rate = 24000,
		.reg_value = MTK_AFE_RATE_24K,
	},
	{
		.rate = 32000,
		.reg_value = MTK_AFE_RATE_32K,
	},
	{
		.rate = 48000,
		.reg_value = MTK_AFE_RATE_48K,
	},
	{
		.rate = 96000,
		.reg_value = MTK_AFE_RATE_96K,
	},
	{
		.rate = 192000,
		.reg_value = MTK_AFE_RATE_192K,
	},
	{
		.rate = 384000,
		.reg_value = MTK_AFE_RATE_384K,
	},
	{
		.rate = 11025,
		.reg_value = MTK_AFE_RATE_11K,
	},
	{
		.rate = 22050,
		.reg_value = MTK_AFE_RATE_22K,
	},
	{
		.rate = 44100,
		.reg_value = MTK_AFE_RATE_44K,
	},
	{
		.rate = 88200,
		.reg_value = MTK_AFE_RATE_88K,
	},
	{
		.rate = 176400,
		.reg_value = MTK_AFE_RATE_176K,
	},
	{
		.rate = 352800,
		.reg_value = MTK_AFE_RATE_352K,
	},
};

static unsigned int mt8186_afe_fs_timing(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8186_afe_rates); i++)
		if (mt8186_afe_rates[i].rate == rate)
			return mt8186_afe_rates[i].reg_value;

	return -EINVAL;
}

static unsigned int mt8186_afe_fs(unsigned int rate, int aud_blk)
{
	return mt8186_afe_fs_timing(rate);
}

static unsigned int mt8186_afe2adsp_addr(unsigned int addr)
{
	/*TODO : Need apply the address remap */
	return addr;
}

static unsigned int mt8186_adsp2afe_addr(unsigned int addr)
{
	/* TODO : Need apply the address remap */
	return addr;
}

struct mtk_base_afe_platform mtk_afe_platform = {
	.base_addr = AFE_BASE_ADDR,
	.memif_datas = memif_data,
	.memif_size = MT8186_MEMIF_NUM,
	.memif_dl_num = MT8186_MEMIF_DL_NUM,
	.memif_32bit_supported = 0,
	.irq_datas = NULL,
	.irqs_size = 0,
	.dais_size = MT8186_DAI_NUM,
	.afe2adsp_addr = mt8186_afe2adsp_addr,
	.adsp2afe_addr = mt8186_adsp2afe_addr,
	.afe_fs = mt8186_afe_fs,
	.irq_fs = mt8186_afe_fs_timing,
};
