// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: YC Hung <yc.hung@mediatek.com>

#include <sof/common.h>
#include <errno.h>
#include <sof/drivers/afe-drv.h>
#include <mt8195-afe-regs.h>
#include <mt8195-afe-common.h>

/*
 * AFE: Audio Front-End
 *
 * frontend (memif):
 *   memory interface
 *   UL (uplink for capture)
 *   DL (downlink for playback)
 * backend:
 *   TDM In
 *   TMD out
 *   DMIC
 *   GASRC
 *   etc.
 * interconn:
 *   inter-connection,
 *   connect frontends and backends as DSP path
 */

static const struct mtk_base_memif_data memif_data[MT8195_MEMIF_NUM] = {
	[MT8195_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT8195_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON0,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 18,
		.hd_reg = AFE_DL2_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 18,
		.ch_num_reg = AFE_DL2_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x1f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 18,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 18,
	},
	[MT8195_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT8195_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON0,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 19,
		.hd_reg = AFE_DL3_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 19,
		.ch_num_reg = AFE_DL3_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x1f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 19,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 19,
	},
	[MT8195_MEMIF_UL4] = {
		.name = "UL4",
		.id = MT8195_MEMIF_UL4,
		.reg_ofs_base = AFE_UL4_BASE,
		.reg_ofs_cur = AFE_UL4_CUR,
		.reg_ofs_end = AFE_UL4_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL4_CON0,
		.mono_shift = 1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 4,
		.hd_reg = AFE_UL4_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 3,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 3,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 3,
	},
	[MT8195_MEMIF_UL5] = {
		.name = "UL5",
		.id = MT8195_MEMIF_UL5,
		.reg_ofs_base = AFE_UL5_BASE,
		.reg_ofs_cur = AFE_UL5_CUR,
		.reg_ofs_end = AFE_UL5_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 20,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL5_CON0,
		.mono_shift = 1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 5,
		.hd_reg = AFE_UL5_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 4,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 4,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 4,
	}
};

struct mt8195_afe_rate {
	unsigned int rate;
	unsigned int reg_value;
};

static const struct mt8195_afe_rate mt8195_afe_rates[] = {
	{
		.rate = 8000,
		.reg_value = 0,
	},
	{
		.rate = 12000,
		.reg_value = 1,
	},
	{
		.rate = 16000,
		.reg_value = 2,
	},
	{
		.rate = 24000,
		.reg_value = 3,
	},
	{
		.rate = 32000,
		.reg_value = 4,
	},
	{
		.rate = 48000,
		.reg_value = 5,
	},
	{
		.rate = 96000,
		.reg_value = 6,
	},
	{
		.rate = 192000,
		.reg_value = 7,
	},
	{
		.rate = 384000,
		.reg_value = 8,
	},
	{
		.rate = 7350,
		.reg_value = 16,
	},
	{
		.rate = 11025,
		.reg_value = 17,
	},
	{
		.rate = 14700,
		.reg_value = 18,
	},
	{
		.rate = 22050,
		.reg_value = 19,
	},
	{
		.rate = 29400,
		.reg_value = 20,
	},
	{
		.rate = 44100,
		.reg_value = 21,
	},
	{
		.rate = 88200,
		.reg_value = 22,
	},
	{
		.rate = 176400,
		.reg_value = 23,
	},
	{
		.rate = 352800,
		.reg_value = 24,
	},
};

static unsigned int mt8195_afe_fs_timing(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8195_afe_rates); i++)
		if (mt8195_afe_rates[i].rate == rate)
			return mt8195_afe_rates[i].reg_value;

	return -EINVAL;
}

static unsigned int mt8195_afe_fs(unsigned int rate, int aud_blk)
{
	return mt8195_afe_fs_timing(rate);
}

static unsigned int mt8195_afe2adsp_addr(unsigned int addr)
{
	/*TODO : Need apply the address remap */
	return addr;
}

static unsigned int mt8195_adsp2afe_addr(unsigned int addr)
{
	/* TODO : Need apply the address remap */
	return addr;
}

struct mtk_base_afe_platform mtk_afe_platform = {
	.base_addr = AFE_BASE_ADDR,
	.memif_datas = memif_data,
	.memif_size = MT8195_MEMIF_NUM,
	.memif_dl_num = MT8195_MEMIF_DL_NUM,
	.memif_32bit_supported = 0,
	.irq_datas = NULL,
	.irqs_size = 0,
	.dais_size = MT8195_DAI_NUM,
	.afe2adsp_addr = mt8195_afe2adsp_addr,
	.adsp2afe_addr = mt8195_adsp2afe_addr,
	.afe_fs = mt8195_afe_fs,
	.irq_fs = mt8195_afe_fs_timing,
};
